/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/mod_devicetable.h>
#include <linux/memblock.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/sipc.h>
#ifdef CONFIG_X86
#include <asm/cacheflush.h>
#endif
#include <uapi/linux/sched/types.h>

#define MAX_CHAR_NUM               128

#define IQ_BUF_INIT                0x5A5A5A5A
#define IQ_BUF_WRITE_FINISHED      0x5A5A8181
#define IQ_BUF_READ_FINISHED       0x81815A5A
#define IQ_BUF_READING             0x81005a00

#define IQ_BUF_OPEN                0x424F504E
#define IQ_BUF_LOCK                0x424C434B
#define DATA_AP_MOVE               0x4441504D
#define DATA_AP_MOVING             0x504D4441
#define DATA_CP_INJECT             0x44435049
#define DATA_AP_MOVE_FINISH        DATA_CP_INJECT
#define DATA_RESET                 0x44525354
#define MAX_PB_HEADER_SIZE		   0x100

#define IQ_TRANSFER_SIZE (500*1024)

#define SPRD_IQ_CLASS_NAME		"sprd_iq"
#define IQ_TAG					"sprd_iq "

extern ssize_t vser_iq_write(char *buf, size_t count);
extern void kernel_vser_register_callback(void *function);

enum {
	CMD_GET_IQ_BUF_INFO = 0x0,
	CMD_GET_IQ_PB_INFO,
	CMD_SET_IQ_CH_TYPE = 0x80,
	CMD_SET_IQ_WR_FINISHED,
	CMD_SET_IQ_RD_FINISHED,
	CMD_SET_IQ_MOVE_FINISHED,
};

enum {
	IQ_USB_MODE = 0,
	IQ_SLOG_MODE,
	PLAY_BACK_MODE,
};

struct iq_buf_info {
	u32 base_offs;
	u32 data_len;
};

struct iq_header {
	volatile u32  WR_RD_FLAG;
	u32  data_addr;
	u32  data_len;
	u32  reserved;
};

struct iq_pb_data_header {
	u32 data_status;
	u32 iqdata_offset;
	u32 iqdata_length;
	char iqdata_filename[MAX_CHAR_NUM];
};

struct iq_header_info {
	struct iq_header *head_1;
	struct iq_header *head_2;
	struct iq_pb_data_header *ipd_head;
};

struct sprd_iq_mgr {
	uint mode;
	uint ch;
	phys_addr_t base;
	phys_addr_t size;
	u32 mapping_offs;
	void *vbase;
	struct reserved_mem *rmem;
	struct iq_header_info *header_info;
	struct task_struct *iq_thread;
	wait_queue_head_t wait;
};

static struct sprd_iq_mgr iq;

static uint iq_base;
static uint iq_size;

module_param_named(iq_base, iq_base, uint, 0444);
module_param_named(iq_size, iq_size, uint, 0444);

static int __init early_mode(char *str)
{
	pr_info(IQ_TAG "early_mode \n");
	if (!memcmp(str, "iq", 2))
		iq.mode = 1;

	return 0;
}

int in_iqmode(void)
{
	return (iq.mode == 1);
}

early_param("androidboot.mode", early_mode);

static ssize_t sprd_iq_write(u32 paddr, u32 length)
{
#ifdef CONFIG_USB_F_VSERIAL
	void *vaddr;
	u32 len;
	u32 send_num = 0;
	ssize_t ret;

	pr_info(IQ_TAG "sprd_iq_write 0x%x, 0x%x \n", paddr, length);
	while (length - send_num > 0) {
		vaddr = NULL;

		len = (length - send_num > IQ_TRANSFER_SIZE) ?
			IQ_TRANSFER_SIZE : (length - send_num);
		vaddr = __va(paddr + send_num);
		if (!vaddr) {
			pr_err(IQ_TAG "sprd iq no memory\n");
			msleep(10);
			continue;
		}

		ret = vser_iq_write(vaddr, len);
		if (ret < 0)
			msleep(200);
		else
			send_num += ret;
	}
	return send_num;
#else
	pr_err(IQ_TAG "the usb mode is not support\n");
	return 0;
#endif
}

static int sprd_iq_thread(void *data)
{
	struct sprd_iq_mgr *t_iq = (struct sprd_iq_mgr *)data;
	struct iq_header_info *p_iq = t_iq->header_info;
	struct sched_param param = {.sched_priority = 80};

	if (NULL == p_iq->head_1 || NULL == p_iq->head_2)
		return -EPERM;

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		if (PLAY_BACK_MODE == iq.ch) {
			if (DATA_AP_MOVE == p_iq->ipd_head->data_status) {
				p_iq->ipd_head->data_status = DATA_AP_MOVING;
				wake_up_interruptible(&t_iq->wait);

			} else if (DATA_AP_MOVE == p_iq->ipd_head->data_status) {
				if (t_iq->vbase)
					memset(t_iq->vbase, 0, t_iq->size);
				p_iq->head_1->WR_RD_FLAG = IQ_BUF_OPEN;
			}
		}

		if (p_iq->head_1->WR_RD_FLAG == IQ_BUF_WRITE_FINISHED) {
			p_iq->head_1->WR_RD_FLAG = IQ_BUF_READING;
			if (IQ_USB_MODE == iq.ch)
				sprd_iq_write(p_iq->head_1->data_addr -
					      iq.mapping_offs,
					      p_iq->head_1->data_len);

			else if (IQ_SLOG_MODE == iq.ch)
				wake_up_interruptible(&t_iq->wait);

		} else if (p_iq->head_2->WR_RD_FLAG == IQ_BUF_WRITE_FINISHED) {
			p_iq->head_2->WR_RD_FLAG = IQ_BUF_READING;
			if (IQ_USB_MODE == iq.ch)
				sprd_iq_write(p_iq->head_2->data_addr -
					      iq.mapping_offs,
					      p_iq->head_2->data_len);

			else if (IQ_SLOG_MODE == iq.ch)
				wake_up_interruptible(&t_iq->wait);

		} else {
			pr_info(IQ_TAG "ch: %d, hand1: 0x%x, flag2: 0x%x\n",
				iq.ch,
				p_iq->head_1->WR_RD_FLAG,
				p_iq->head_2->WR_RD_FLAG);
			msleep(1500);
		}
	}
	return 0;
}

#ifdef CONFIG_USB_F_VSERIAL
static void sprd_iq_complete(char *buf,  int length)
{
	char *vaddr;

	if (IQ_USB_MODE != iq.ch)
		return;
	vaddr = buf;
	pr_info(IQ_TAG "sprd_iq_complete 0x%p, 0x%x \n", vaddr, length);
	if (vaddr + length == (char *)__va(iq.header_info->head_1->data_addr -
					   iq.mapping_offs +
			iq.header_info->head_1->data_len))
		iq.header_info->head_1->WR_RD_FLAG = IQ_BUF_READ_FINISHED;

	if (vaddr + length == (char *)__va(iq.header_info->head_2->data_addr -
					   iq.mapping_offs +
			iq.header_info->head_2->data_len))
		iq.header_info->head_2->WR_RD_FLAG = IQ_BUF_READ_FINISHED;
}
#endif

static long iq_mem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct iq_buf_info b_info;

	pr_info(IQ_TAG "enter iq_mem_ioctl cmd = 0x%x\n", cmd);

	if (NULL == iq.header_info->head_1 || NULL == iq.header_info->head_2)
		return -EPERM;

	switch (cmd) {
	case CMD_GET_IQ_BUF_INFO:
		if (IQ_BUF_READING ==
				iq.header_info->head_1->WR_RD_FLAG) {
			b_info.base_offs =
				iq.header_info->head_1->data_addr - iq.base - iq.mapping_offs;
			b_info.data_len = iq.header_info->head_1->data_len;

		} else if (IQ_BUF_READING ==
				iq.header_info->head_2->WR_RD_FLAG) {
			b_info.base_offs =
				iq.header_info->head_2->data_addr - iq.base - iq.mapping_offs;
			b_info.data_len = iq.header_info->head_2->data_len;
		}

		if (copy_to_user((void *)arg,
				(void *)&b_info, sizeof(struct iq_buf_info))) {
			pr_err(IQ_TAG "copy iq buf info to user space failed.\n");
			return -EFAULT;
		}

		break;

	case CMD_GET_IQ_PB_INFO:
		if (PLAY_BACK_MODE != iq.ch) {
			pr_err(IQ_TAG "current mode is not playback mode\n");
			return -EINVAL;
		}

		if (copy_to_user((void *)arg,
				 (void *)(iq.header_info->ipd_head),
				 sizeof(struct iq_pb_data_header))) {
			pr_err(IQ_TAG "copy iq playback data info to user space failed.\n");
			return -EFAULT;
		}

		break;

	case CMD_SET_IQ_CH_TYPE:
		if (IQ_USB_MODE <= arg && PLAY_BACK_MODE >= arg) {
			iq.ch = arg;
		} else {
			pr_err(IQ_TAG "iq ch type invalid\n");
			return -EINVAL;
		}

		if (IQ_SLOG_MODE == arg) {
			if (IQ_BUF_READING ==
					iq.header_info->head_1->WR_RD_FLAG ||
				IQ_BUF_READING ==
					iq.header_info->head_2->WR_RD_FLAG) {
				wake_up_interruptible(&iq.wait);
			}

		} else if (PLAY_BACK_MODE == arg) {
			iq.header_info->ipd_head =
				(struct iq_pb_data_header *)(iq.vbase +
					sizeof(struct iq_header));
			iq.header_info->head_1->data_addr =
				iq.base + MAX_PB_HEADER_SIZE;
			iq.header_info->head_1->WR_RD_FLAG =
				IQ_BUF_OPEN;
		}
		break;

	case CMD_SET_IQ_RD_FINISHED:
		if (IQ_BUF_READING ==
			iq.header_info->head_1->WR_RD_FLAG) {
			iq.header_info->head_1->WR_RD_FLAG =
				IQ_BUF_READ_FINISHED;
		} else if (IQ_BUF_READING ==
			iq.header_info->head_2->WR_RD_FLAG) {
			iq.header_info->head_2->WR_RD_FLAG =
				IQ_BUF_READ_FINISHED;
		}

		break;

	case CMD_SET_IQ_MOVE_FINISHED:
		if (PLAY_BACK_MODE != iq.ch) {
			pr_err(IQ_TAG "current mode is not playback mode\n");
			return -EINVAL;
		}

		if (copy_from_user((void *)iq.header_info->ipd_head,
				   (void *)arg,
				   sizeof(struct iq_pb_data_header))) {
			pr_err(IQ_TAG "copy iq playback data info from user space failed.\n");
			return -EFAULT;
		}

		iq.header_info->head_1->data_len =
			iq.header_info->ipd_head->iqdata_length;

		if (DATA_AP_MOVE_FINISH ==
			iq.header_info->ipd_head->data_status) {
			iq.header_info->head_1->WR_RD_FLAG = IQ_BUF_LOCK;
		}

		break;

	default:
		break;

	}

	return 0;
}

static unsigned int iq_mem_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	unsigned mask = 0;

	if (NULL == iq.header_info->head_1 || NULL == iq.header_info->head_2)
		return POLLERR;

	poll_wait(filp, &iq.wait, wait);

	if (IQ_SLOG_MODE == iq.ch) {
		if (IQ_BUF_READING == iq.header_info->head_1->WR_RD_FLAG ||
		    IQ_BUF_READING == iq.header_info->head_2->WR_RD_FLAG) {
			mask |= (POLLIN | POLLRDNORM);
		}
	} else if (PLAY_BACK_MODE == iq.ch) {
		if (DATA_AP_MOVING == iq.header_info->ipd_head->data_status) {
			pr_err(IQ_TAG "I/Q playback status DATA_AP_MOVING\n");
			mask |= (POLLIN | POLLRDNORM);
		}
	}

	return mask;
}

static int iq_mem_nocache_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;

	pr_info(IQ_TAG "iq_mem_nocache_mmap\n");

	if (iq.base == 0x0) {
		pr_err(IQ_TAG "invalid iq_base(0x%llx).\n", (long long)iq.base);
		return -EAGAIN;
	}

	if (size <= iq.size) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		if (remap_pfn_range(vma,
			vma->vm_start,
			iq.base>>PAGE_SHIFT,
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot)) {
			pr_err(IQ_TAG "remap_pfn_range failed\n");
			return -EAGAIN;
		}

	} else {
		pr_err(IQ_TAG "map size to big, exceed maxsize.\n");
		return -EAGAIN;
	}

	pr_info(IQ_TAG "iq mmap %x,%x,%x\n", (unsigned int)PAGE_SHIFT,
	       (unsigned int)vma->vm_start,
	       (unsigned int)(vma->vm_end - vma->vm_start));
	return 0;
}

static int iq_mem_open(struct inode *inode, struct file *filp)
{
	pr_info(IQ_TAG "iq_mem_open called\n");
	return 0;
}

static int iq_mem_release(struct inode *inode, struct file *filp)
{
	pr_info(IQ_TAG "iq_mem_release\n");
	return 0;
}

static const struct file_operations iq_mem_fops = {
	.owner = THIS_MODULE,
	.poll = iq_mem_poll,
	.unlocked_ioctl = iq_mem_ioctl,
	.mmap  = iq_mem_nocache_mmap,
	.open = iq_mem_open,
	.release = iq_mem_release,
};

static struct miscdevice iq_mem_dev = {
	.minor   = MISC_DYNAMIC_MINOR,
	.name   = "iq_mem",
	.fops   = &iq_mem_fops,
};

static int sprd_iq_parse_dt(struct platform_device *pdev,
			    struct sprd_iq_mgr *iq)
{
	int ret;
	u32 val;
	struct device_node *np;
	struct device_node *np_memory;
	struct resource res;

	pr_info(IQ_TAG "%s\n", __func__);
	if (!pdev || !pdev->dev.of_node || !iq) {
		pr_err(IQ_TAG "param error\n");
		return -EINVAL;
	}

	np = pdev->dev.of_node;

	if (iq->base == 0) {
		np_memory = of_parse_phandle(np, "sprd,region", 0);
		if (!np_memory)
			return -ENOMEM;

		ret = of_address_to_resource(np_memory, 0, &res);
		if (ret) {
			pr_err(IQ_TAG "get iq mem info failed\n");
			return -EINVAL;
		}
		iq->base = res.start;
		iq->size = res.end - res.start + 1;
		pr_info(IQ_TAG "iq base: 0x%lx, iq size: 0x%lx\n",
			(unsigned long)iq->base, (unsigned long)iq->size);
	}

	/* get the cp and ap address mapping on ddr, such as iwhale2,
	the address 0x00000000 in AP is 0x80000000 in CP, the default
	mapping_offs is 0
	*/
	iq->mapping_offs = 0;

	if (of_property_read_u32(np, "sprd,mapping-offs", &val) == 0)
		iq->mapping_offs = val;

	pr_info(IQ_TAG "iq mapping_offs: 0x%x\n", iq->mapping_offs);

	return 0;
}

static int sprd_iq_probe(struct platform_device *pdev)
{
	int ret;
	pr_info(IQ_TAG "%s\n", __func__);

	/* if iq.base == 0, indicate that the  reserved_mem_iq_setup function
	is not be called, the iq base must be gotten by sprd_iq_parse_dt
	*/
	if (iq.base == 0) {
		ret = sprd_iq_parse_dt(pdev, &iq);
		if (ret)
			goto err0;
		iq_base = iq.base + iq.mapping_offs;
		iq_size = iq.size;
	}

	iq.header_info = kzalloc(sizeof(struct iq_header_info), GFP_KERNEL);
	if (!iq.header_info) {
		pr_err(IQ_TAG "kzalloc struct sprd_iq_mgr failed\n");
		ret = -ENOMEM;
		goto err0;
	}

	iq.ch = IQ_USB_MODE;

	ret = misc_register(&iq_mem_dev);
	if (ret) {
		pr_err(IQ_TAG "cannot register iq_mmap_dev ret = (%d)\n", ret);
		goto err1;
	}

	init_waitqueue_head(&iq.wait);
	iq.vbase = shmem_ram_vmap_nocache(iq.base, iq.size);
	if (NULL == iq.vbase) {
		pr_err(IQ_TAG "iq.vbase is null\n");
		ret = -ENOMEM;
		goto err2;
	}

#ifdef CONFIG_X86
	if (set_memory_uc((unsigned long)page_address(
					pfn_to_page(PFN_DOWN(iq.base))),
			  iq.size / PAGE_SIZE)) {
		pr_err(IQ_TAG "change memory type to uncache failed\n");
		goto err3;
	}
#endif
	iq.header_info->head_1 = (struct iq_header *)(iq.vbase);
	iq.header_info->head_2 = (struct iq_header *)(iq.vbase + iq.size
					      - sizeof(struct iq_header));

	iq.header_info->head_1->WR_RD_FLAG = IQ_BUF_INIT;
	iq.header_info->head_2->WR_RD_FLAG = IQ_BUF_INIT;

#ifdef CONFIG_USB_F_VSERIAL
	kernel_vser_register_callback((void *)sprd_iq_complete);
#endif

	iq.iq_thread = kthread_create(sprd_iq_thread, (void *)&iq, "iq_thread");
	if (IS_ERR(iq.iq_thread)) {
		pr_err(IQ_TAG "create iq_thread error!\n");
		ret = PTR_ERR(iq.iq_thread);
		goto err3;
	}
	platform_set_drvdata(pdev, &iq);

	wake_up_process(iq.iq_thread);

	return 0;

err3:
	shmem_ram_unmap(iq.vbase);
err2:
	misc_deregister(&iq_mem_dev);
err1:
	kfree(iq.header_info);
err0:
	return ret;
}

static int sprd_iq_remove(struct platform_device *pdev)
{
	if (NULL == iq.vbase)
		shmem_ram_unmap(iq.vbase);

	kfree(iq.header_info);

	misc_deregister(&iq_mem_dev);
	return 0;
}

static const struct of_device_id sprd_iq_ids[] = {
	{ .compatible = "sprd,iq" },
	{},
};

static struct platform_driver iq_driver = {
	.probe = sprd_iq_probe,
	.remove = sprd_iq_remove,
	.driver = {
		.name = "iq",
		.of_match_table = of_match_ptr(sprd_iq_ids),
	}
};

static int __init sprd_iq_init(void)
{
	if (!in_iqmode()) {
		pr_info(IQ_TAG "no in iq mode\n");
		return -ENODEV;
	}
	pr_info(IQ_TAG "in iq mode and register iq driver\n");
	return platform_driver_register(&iq_driver);
}

static void __exit sprd_iq_exit(void)
{
	platform_driver_unregister(&iq_driver);
}

/*
 * Save the WCDMA I/Q memory address and size.
 *
 * This function is not called on i-series SoCs because CONFIG_OF_RESERVED_MEM
 * is not turned on.
 * iq_base/iq_size is initialized in sprd_iq_probe() on i-series SoCs.
 */
static int __init reserved_mem_iq_setup(struct reserved_mem *rmem)
{
	pr_info(IQ_TAG "enter the rmem_iq_setup\n");
	if (rmem == NULL)
		return -EINVAL;

	iq.base = rmem->base;
	iq.size = rmem->size;
	iq.rmem = rmem;
	iq_base = iq.base;
	iq_size = iq.size;

	pr_info(IQ_TAG "created iq memory at 0x%llx, size is 0x%llx\n",
		(unsigned long long)iq.base, (unsigned long long)iq.size);

	return 0;
}

RESERVEDMEM_OF_DECLARE(iq, "sprd,iq-mem", reserved_mem_iq_setup);

module_init(sprd_iq_init);
module_exit(sprd_iq_exit);

MODULE_AUTHOR("zhongping tan");
MODULE_DESCRIPTION("SPRD IQ driver");
MODULE_LICENSE("GPL");
