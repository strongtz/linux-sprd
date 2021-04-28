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

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/sipc.h>
#include <linux/debugfs.h>
#include <linux/sprd_cp.h>

#include "../include/sprd_cproc.h"

#ifdef CONFIG_DEBUG_FS
static const struct file_operations sprd_cproc_debug_fops;
static struct dentry *cproc_root;

static void sprd_cproc_init_debugfs(void);
#endif

#ifndef CONFIG_SPRD_SIPC_V2
#define SOC_MODEM	0
#define modem_ram_vmap_nocache(type, start, size) \
	shmem_ram_vmap_nocache((start), (size))
#define modem_ram_unmap(type, mem)	shmem_ram_unmap((mem))
#endif

#define CPROC_WDT_TRUE   true
#define CPROC_WDT_FLASE  false
/*used for ioremap to limit vmalloc size, shi yunlong*/
#define CPROC_VMALLOC_SIZE_LIMIT 4096
#define MAX_CPROC_ENTRY_NUM		0x10

enum {
	CP_NORMAL_STATUS = 0,
	CP_STOP_STATUS,
	CP_WDTIRQ_STATUS,
	CP_MAX_STATUS,
};

enum {
	BE_SEGMFG   = BIT(4),
	BE_RDONLY   = BIT(5),
	BE_WRONLY   = BIT(6),
	BE_CPDUMP   = BIT(7),
	BE_MNDUMP   = BIT(8),
	BE_RDWDT    = BIT(9),
	BE_RDWDTS   = BIT(10),
	BE_RDLDIF   = BIT(11),
	BE_LD	    = BIT(12),
	BE_CTRL_ON  = BIT(13),
	BE_CTRL_OFF	= BIT(14),
	BE_IOCTL    = BIT(15),
};

enum {
	ARM_CTRL = 0x0,	/* arm control */
	AGDSP_CTRL,		/* agdsp control */
	CTRL_NR,
};

static int cp_boot_mode;

const char *cp_status_info[] = {
	"started\n",
	"stopped\n",
	"wdtirq\n",
};

const char *cproc_dts_args[CPROC_CTRL_NR] = {
	"shutdown",
	"deepsleep",
	"corereset",
	"sysreset",
	"getstatus",
	"dspreset",
	"fshutdown",
};

struct cproc_proc_fs;

struct cproc_proc_entry {
	char				*name;
	struct proc_dir_entry	*entry;
	struct cproc_device		*cproc;
	unsigned int flag; /*if set bit4 => bit0~bit3 for segs index */
};

struct cproc_proc_fs {
	struct proc_dir_entry		*procdir;
	struct cproc_proc_entry		entrys[MAX_CPROC_ENTRY_NUM];
};

struct cproc_device {
	struct miscdevice miscdev;
	struct cproc_init_data *initdata;
	void *vbase;
	int wdtirq;
	int	wdtcnt;
	wait_queue_head_t wdtwait;
	char *name;
	int status;
	int	type;
	struct cproc_proc_fs procfs;
};

struct cproc_dump_info {
	char parent_name[20];
	char name[20];
	u32 start_addr;
	u32 size;
};

static void sprd_cproc_regmap_update_bit(struct cproc_ctrl *ctrl,
					 u32 index,
					 u32 mask,
					 u32 val);
static void sprd_cproc_regmap_read(struct cproc_ctrl *ctrl,
				u32 index,
				unsigned int *val);

static int list_each_dump_info(struct cproc_dump_info *base,
			       struct cproc_dump_info **info)
{
	struct cproc_dump_info *next;
	int ret = 1;

	if (!info)
		return 0;

	next = *info;
	if (!next)
		next = base;
	else
		next++;

	if (next->parent_name[0] != '\0') {
		*info = next;
	} else {
		*info = NULL;
		ret = 0;
	}

	return ret;
}

static ssize_t sprd_cproc_seg_dump(u32 base,
				   u32 maxsz,
				   char __user *buf,
				   size_t count,
				   loff_t offset)
{
	void *vmem;
	u32 loop = 0;
	u32 start_addr;
	u32 total;

	if (offset >= maxsz)
		return 0;

	if ((offset + count) > maxsz)
		count = maxsz - offset;

	start_addr = base + offset;
	total = count;

	do {
		u32 copy_size = CPROC_VMALLOC_SIZE_LIMIT;

		vmem = modem_ram_vmap_nocache(SOC_MODEM,
				start_addr + CPROC_VMALLOC_SIZE_LIMIT * loop,
				CPROC_VMALLOC_SIZE_LIMIT);
		if (!vmem) {
			pr_err("unable to map cproc base: 0x%08x\n",
			       start_addr + CPROC_VMALLOC_SIZE_LIMIT * loop);
			if (loop > 0)
				return CPROC_VMALLOC_SIZE_LIMIT * loop;
			else
				return -ENOMEM;
		}

		if (count < CPROC_VMALLOC_SIZE_LIMIT)
			copy_size = count;

		if (unalign_copy_to_user(buf, vmem, copy_size)) {
			pr_err("cproc_proc_read copy data to user error !\n");
			modem_ram_unmap(SOC_MODEM, vmem);
			return -EFAULT;
		}

		modem_ram_unmap(SOC_MODEM, vmem);

		count -= copy_size;
		loop++;
		buf += copy_size;
	} while (count);

	return total;
}

static int __init early_mode(char *str)
{
	if (!memcmp("shutdown", str, 8))
		cp_boot_mode = 1;

	return 0;
}

early_param("modem", early_mode);

static int sprd_cproc_open(struct inode *inode, struct file *filp)
{
	struct cproc_device *cproc = container_of(filp->private_data,
			struct cproc_device, miscdev);

	filp->private_data = cproc;

	pr_debug("%s: %s!\n", __func__, cproc->initdata->devname);

	return 0;
}

static int sprd_cproc_release(struct inode *inode, struct file *filp)
{
	struct cproc_device *cproc = filp->private_data;

	pr_debug("%s: %s !\n", __func__, cproc->initdata->devname);

	return 0;
}

static long sprd_cproc_ioctl(struct file *filp,
			     unsigned int cmd,
			     unsigned long arg)
{
	return 0;
}

static int sprd_cproc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

static const struct file_operations sprd_cproc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_cproc_open,
	.release = sprd_cproc_release,
	.unlocked_ioctl = sprd_cproc_ioctl,
	.mmap = sprd_cproc_mmap,
};

static int cproc_proc_open(struct inode *inode, struct file *filp)
{
	struct cproc_proc_entry
	*entry = (struct cproc_proc_entry *)PDE_DATA(inode);

	pr_debug("%s: %s\n!", __func__, entry->name);

	filp->private_data = entry;

	return 0;
}

static int cproc_proc_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t cproc_proc_read(struct file *filp,
			       char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct cproc_proc_entry
		*entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	unsigned int len;
	unsigned int flag;
	void *vmem;
	int rval;
	int i;
	unsigned long r;
	u32 base, size;

	flag = entry->flag;
	pr_debug("%s: type = %s, flag = 0x%x\n", __func__, type, flag);

	if ((flag & BE_RDONLY) == 0)
		return -EPERM;

	if ((flag & (BE_CPDUMP | BE_SEGMFG)) != 0) {
		if (flag & BE_CPDUMP) {
			base = cproc->initdata->base;
			size = cproc->initdata->maxsz;
		} else {
			for (i = 0; i < cproc->initdata->segnr; i++) {
				if (strcmp(cproc->initdata->segs[i].name,
					   entry->name) == 0)
					break;
			}

			if (i == cproc->initdata->segnr)
				return -EINVAL;

			base = cproc->initdata->segs[i].base;
			size = cproc->initdata->segs[i].maxsz;
		}

		if (*ppos >= size)
			return 0;

		if ((*ppos + count) > size)
			count = size - *ppos;

		r = count, i = 0;
		do {
			u32 copy_size = CPROC_VMALLOC_SIZE_LIMIT;

			vmem = modem_ram_vmap_nocache(SOC_MODEM,
					base +
					*ppos + CPROC_VMALLOC_SIZE_LIMIT * i,
					CPROC_VMALLOC_SIZE_LIMIT);
			if (!vmem) {
				unsigned long addr = base +
					*ppos + CPROC_VMALLOC_SIZE_LIMIT * i;
				pr_err("Unable to map cproc base: 0x%lx\n",
				       addr);
				if (i > 0) {
					*ppos += CPROC_VMALLOC_SIZE_LIMIT * i;
					return CPROC_VMALLOC_SIZE_LIMIT * i;
				} else {
					return -ENOMEM;
				}
			}
			if (r < CPROC_VMALLOC_SIZE_LIMIT)
				copy_size = r;
			if (unalign_copy_to_user(buf, vmem, copy_size)) {
				pr_err("cproc read copy data to user err!\n");
				modem_ram_unmap(SOC_MODEM, vmem);
				return -EFAULT;
			}
			modem_ram_unmap(SOC_MODEM, vmem);
			r -= copy_size;
			buf += copy_size;
			i++;
		} while (r > 0);
		/*remap and unmap in each read operation, shi yunlong, end*/
	} else if ((flag & BE_RDWDTS) != 0) {
		if (cproc->status >= CP_MAX_STATUS)
			return -EINVAL;

		len = strlen(cp_status_info[cproc->status]);
		if (*ppos >= len)
			return 0;

		count = (len > count) ? count : len;
		if (unalign_copy_to_user(buf,
					 cp_status_info[cproc->status],
					 count)) {
			pr_err("cproc: read, copy data to user error!\n");
			return -EFAULT;
		}
	} else if ((flag & BE_RDWDT) != 0) {
		/* wait forever */
		rval = wait_event_interruptible(cproc->wdtwait, cproc->wdtcnt
						!= CPROC_WDT_FLASE);
		if (rval < 0)
			pr_err("cproc: read, wait interrupted error!\n");

		len = strlen(cp_status_info[CP_WDTIRQ_STATUS]);
		if (*ppos >= len)
			return 0;
		count = (len > count) ? count : len;
		if (unalign_copy_to_user(buf, cp_status_info[CP_WDTIRQ_STATUS],
					 count)) {
			pr_err("cproc: read, copy data to user error!\n");
			return -EFAULT;
		}
	} else if ((flag & BE_RDLDIF) != 0) {
		struct cproc_init_data *data = cproc->initdata;
		struct cproc_segments *segm = NULL;
		struct load_node lnode;
		char *p = buf;

		/* lseek i to *ppos, equivalent to i = *ppos /
		 * sizeof(struct load_node)
		 */
		for (i = 0; i * sizeof(struct load_node) < *ppos; i++)
			;

		for (len = 0, segm = &data->segs[i];
			i < data->segnr;
			i++, segm++) {
			if (segm->maxsz == 0)
				break;

			memset(&lnode, 0, sizeof(lnode));
			pr_debug("%s: segm[%d] name=%s base=0x%x,size=0x%0x\n",
				__func__,
				i, segm->name,
				segm->base,
				segm->maxsz);
			memcpy(lnode.name, segm->name, sizeof(lnode.name));
			lnode.base = segm->base;
			lnode.size = segm->maxsz;
			len += sizeof(lnode);

			if (count < len) {
				len -= sizeof(lnode);
				break;
			}

			if (unalign_copy_to_user(p, &lnode, sizeof(lnode))) {
				pr_err("%s: copy ldif to user err!\n",
				       __func__);
				return -EFAULT;
			}

			p += sizeof(lnode);
		}

		count = len;

	} else if ((flag & BE_MNDUMP) != 0) {
		static struct cproc_dump_info *s_cur_info;
		u8 head[sizeof(struct cproc_dump_info) + 32];
		int len, total = 0, offset = 0;
		ssize_t written = 0;

		if (!s_cur_info && *ppos)
			return 0;

		if (!s_cur_info)
			list_each_dump_info(cproc->initdata->mini_mem,
					    &s_cur_info);

		while (s_cur_info) {
			if (!count)
				break;
			len = sprintf(head,
				      "%s_%s_0x%8x_0x%x.bin",
				      s_cur_info->parent_name,
				      s_cur_info->name,
				      s_cur_info->start_addr,
				      s_cur_info->size);

			if (*ppos > len) {
				offset = *ppos - len;
			} else {
				if (*ppos + count > len)
					written = len - *ppos;
				else
					written = count;

				if (unalign_copy_to_user(buf + total,
							 head + *ppos,
							 written)) {
					pr_err("copy mini-dump data to user error !\n");
					return -EFAULT;
				}
				*ppos += written;
			}
			total += written;
			count -= written;
			if (count) {
				written = sprd_cproc_seg_dump(
						s_cur_info->start_addr,
						s_cur_info->size,
						buf + total,
						count,
						offset);
				if (written > 0) {
					total += written;
					count -= written;
					*ppos += written;
				} else if (written == 0) {
					if (list_each_dump_info(
						cproc->initdata->mini_mem,
						&s_cur_info))
						*ppos = 0;
				} else {
					return written;
				}

			} else {
				break;
			}

			written = 0;
			offset = 0;
		}

		return total;
	} else {
		return -EINVAL;
	}

	*ppos += count;
	return count;
}

static int cproc_proc_copy_iram(struct cproc_device *cproc)
{
	struct cproc_ctrl	*ctrl;
	void *vmem;

	ctrl = cproc->initdata->ctrl;

	if (ctrl->iram_loaded)
		return 0;

	ctrl->iram_loaded = 1;
	pr_debug("%s: addr =0x%p, size =%d\n",
		__func__,
		(void *)ctrl->iram_addr,
		ctrl->iram_size);

	vmem = modem_ram_vmap_nocache(SOC_MODEM,
				      ctrl->iram_addr,
				      ctrl->iram_size);
	if (!vmem)
		return -ENOMEM;

	unalign_memcpy(vmem,
		       (void *)ctrl->iram_data,
		       ctrl->iram_size);
	modem_ram_unmap(SOC_MODEM, vmem);

	return 0;
}

static ssize_t cproc_proc_copy_from_user(void *to,
					       const void __user *from,
					       unsigned long n)
{
	unsigned long i, ret, size;
	char *buf, *src;
	u32 *word_dst, *word_src;
	u8 *byte_dst, *byte_src;

	buf = kzalloc(n, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/* when the dst address is on aon sp iram, we use the function
	 * copy_from_user or the function memcpy to copy data to the dst
	 * in sharkl5(arm32), the instuction stm may cause aon sp iram
	 * repose unknown error to cpu. A ways to get around this
	 * probem is below, Firstly,  copy the user data to a ddr buf,
	 * than,  copy it to iram one byte by one byte
	 */
	ret = unalign_copy_from_user(buf, from, n);

	/* Returns number of bytes that could not be copied.
	 * On success, this will be zero.
	 */
	src = buf;

	/*Firstly, cpoy all word align address one word by one word*/
	size = (n - ret) / sizeof(u32);
	word_dst = (u32 *)to;
	word_src = (u32 *)src;
	for (i = 0; i < size; i++)
		*word_dst++ = *word_src++;

	/*than, cpoy all left context one byte by one byte */
	size = (n - ret) % sizeof(u32);
	byte_dst = (u8 *)word_dst;
	byte_src = (u8 *)word_src;
	for (i = 0; i < size; i++)
		*byte_dst++ = *byte_src++;

	kfree(buf);

	return ret;
}

static ssize_t cproc_proc_write(struct file *filp,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	struct cproc_proc_entry
		*entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	u32 base, size, offset;
	void *vmem;
	int i;
	unsigned long r = count;
	unsigned int flag;

	flag = entry->flag;
	pr_debug("%s: type = %s flag = 0x%x\n", __func__, type, flag);

	if ((flag & BE_WRONLY) == 0) {
		return -EPERM;
	} else if ((flag & BE_CTRL_ON) != 0) {
		pr_debug("%s: start!\n", __func__);
		cproc->initdata->start(cproc);
		cproc->wdtcnt = CPROC_WDT_FLASE;
		cproc->status = CP_NORMAL_STATUS;
		return count;
	} else if ((flag & BE_CTRL_OFF) != 0) {
		pr_debug("%s: stop!\n", __func__);
		cproc->initdata->stop(cproc);
		cproc->status = CP_STOP_STATUS;
		return count;
	} else if ((flag & BE_LD) != 0) {
		i = ~((~0) << 4) & flag;
		base = cproc->initdata->segs[i].base;
		size = cproc->initdata->segs[i].maxsz;
		offset = *ppos;

		pr_debug("%s: write %s, offse = %d!\n",
			 __func__, type, offset);

		if (!cproc->initdata->ctrl->iram_loaded &&
		    cproc->initdata->ctrl->iram_size > 0)
			cproc_proc_copy_iram(cproc);
	} else {
		pr_err("%s: flag erro!\n", __func__);
		return -EPERM;
	}

	if (size <= offset) {
		pr_debug("%s: max bin size <= offset write over! size = 0x%x, offset = 0x%x\n",
			__func__, size, offset);
		return 0;
	}

	pr_debug("%s: write addr = 0x%08x, size = %lu\n!",
		 __func__, base + offset, r);
	count = min((size_t)(size - offset), count);
	r = count, i = 0;
	do {
		unsigned long ret;
		const char __user *pos;
		u32 copy_size = CPROC_VMALLOC_SIZE_LIMIT;

		vmem = modem_ram_vmap_nocache(SOC_MODEM,
				       base +
				       offset +
				       CPROC_VMALLOC_SIZE_LIMIT * i,
				       CPROC_VMALLOC_SIZE_LIMIT);
		if (!vmem) {
			unsigned long addr = base +
				      offset +
				      CPROC_VMALLOC_SIZE_LIMIT * i;
			pr_err("Unable to map cproc base: 0x%lx\n",
			       addr);
			if (i > 0) {
				*ppos += CPROC_VMALLOC_SIZE_LIMIT * i;
				return CPROC_VMALLOC_SIZE_LIMIT * i;
			} else {
				return -ENOMEM;
			}
		}
		if (r < CPROC_VMALLOC_SIZE_LIMIT)
			copy_size = r;
		pos = buf + CPROC_VMALLOC_SIZE_LIMIT * i;
		/* dst is aon sp iram, we must used cproc_proc_copy_from_user */
		if (strstr(cproc->initdata->devname, "pm"))
			ret = cproc_proc_copy_from_user(vmem,
							pos,
							copy_size);
		else
			ret = unalign_copy_from_user(vmem,
						     pos,
						     copy_size);
		if (ret) {
			pr_err("%s: copy data from user err = %lu!\n",
			       __func__, ret);
			modem_ram_unmap(SOC_MODEM, vmem);
			return -EFAULT;
		}
		modem_ram_unmap(SOC_MODEM, vmem);
		r -= copy_size;
		i++;
	} while (r > 0);

	*ppos += count;
	return count;
}

static loff_t cproc_proc_lseek(struct file *filp, loff_t off, int whence)
{
	struct cproc_proc_entry
		*entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	loff_t new;

	switch (whence) {
	case SEEK_SET:
		new = off;
		filp->f_pos = new;
		break;
	case SEEK_CUR:
		new = filp->f_pos + off;
		filp->f_pos = new;
		break;
	case SEEK_END:
		if (strcmp(type, "mem") == 0) {
			new = cproc->initdata->maxsz + off;
			filp->f_pos = new;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return new;
}

static unsigned int cproc_proc_poll(struct file *filp, poll_table *wait)
{
	struct cproc_proc_entry
		*entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	char *type = entry->name;
	unsigned int mask = 0;

	pr_debug("%s: type = %s\n", __func__, type);

	if (strcmp(type, "wdtirq") == 0) {
		poll_wait(filp, &cproc->wdtwait, wait);
		if (cproc->wdtcnt  != CPROC_WDT_FLASE)
			mask |= POLLIN | POLLRDNORM;
	} else {
		pr_err("%s: file don't support poll !\n", __func__);
	}
	return mask;
}

static long cproc_proc_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct cproc_proc_entry
		*entry = (struct cproc_proc_entry *)filp->private_data;
	struct cproc_device *cproc = entry->cproc;
	struct cproc_ctrl *ctrl;
	long ret = -EINVAL;
	u32 status, mask;
	int done;

	if (!(entry->flag & BE_IOCTL)) {
		pr_err("ioctl into non-supported file %s!\n",
		       entry->name);

		return -EOPNOTSUPP;
	}

	ctrl = cproc->initdata->ctrl;
	mask = ctrl->ctrl_mask[CPROC_CTRL_LOAD_STATUS];

	switch (cmd) {
	case SPRD_CP_IOCGLDSTAT:
		if (!access_ok(VERIFY_WRITE,
			       (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;

		sprd_cproc_regmap_read(ctrl,
				       CPROC_CTRL_LOAD_STATUS,
				       &status);
		done = (status & mask) ? 1 : 0;
		ret = __put_user(done, (int __user *)arg);
		break;

	case SPRD_CP_IOCCLDSTAT:
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_LOAD_STATUS,
			mask,
			~mask);
		break;

	default:
		break;
	}

	return ret;
}

static const struct file_operations cpproc_fs_fops = {
	.open		= cproc_proc_open,
	.release	= cproc_proc_release,
	.llseek		= cproc_proc_lseek,
	.read		= cproc_proc_read,
	.write		= cproc_proc_write,
	.poll		= cproc_proc_poll,
	.unlocked_ioctl	= cproc_proc_ioctl,
};

static inline void sprd_cproc_fs_init(struct cproc_device *cproc)
{
	u8 i, ucnt;
	unsigned int flag;
	umode_t mode;
	struct cproc_ctrl *ctrl;

	cproc->procfs.procdir = proc_mkdir(cproc->name, NULL);

	memset(cproc->procfs.entrys, 0, sizeof(cproc->procfs.entrys));

	for (flag = 0, ucnt = 0, mode = 0, i = 0;
		i < MAX_CPROC_ENTRY_NUM;
		i++, flag = 0, mode = 0) {
		switch (i) {
		case 0:
			cproc->procfs.entrys[i].name = "start";
			flag |= (BE_WRONLY | BE_CTRL_ON);
			ucnt++;
			break;

		case 1:
			cproc->procfs.entrys[i].name = "stop";
			flag |= (BE_WRONLY | BE_CTRL_OFF);
			ucnt++;
			break;

		case 2:
			cproc->procfs.entrys[i].name = "wdtirq";
			flag |= (BE_RDONLY | BE_RDWDT);
			ucnt++;
			break;

		case 3:
			cproc->procfs.entrys[i].name = "status";
			flag |= (BE_RDONLY | BE_RDWDTS);
			ctrl = cproc->initdata->ctrl;
			if (ctrl->ctrl_reg[CPROC_CTRL_LOAD_STATUS] !=
				INVALID_REG)
				flag |= BE_IOCTL;
			ucnt++;
			break;

		case 4:
			if (!cproc->initdata->mini_mem) {
				ucnt++;
				continue;
			}
			cproc->procfs.entrys[i].name = "mini_dump";
			flag |= (BE_RDONLY | BE_MNDUMP);
			ucnt++;
			break;

		case 5:
			cproc->procfs.entrys[i].name = "ldinfo";
			flag |= (BE_RDONLY | BE_RDLDIF);
			ucnt++;
			break;

		case 6:
			cproc->procfs.entrys[i].name = "mem";
			flag |= (BE_RDONLY | BE_CPDUMP);
			ucnt++;
			break;

		default:
			if (cproc->initdata->segnr + ucnt
				>= MAX_CPROC_ENTRY_NUM) {
				pr_debug("%s: entries size to small\n",
					 __func__);
				return;
			}

			if (i - ucnt >= cproc->initdata->segnr)
				return;

			cproc->procfs.entrys[i].name =
				cproc->initdata->segs[i - ucnt].name;
			flag |= (BE_WRONLY | BE_RDONLY | BE_LD | BE_SEGMFG | (i - ucnt));
			break;
		}

		cproc->procfs.entrys[i].flag = flag;

		mode |= (S_IRUSR | S_IWUSR);
		if (flag & (BE_CPDUMP | BE_MNDUMP))
			mode |= S_IROTH;

		pr_debug("%s: entry name is %s type 0x%x addr: 0x%p\n",
			__func__,
			cproc->procfs.entrys[i].name,
			cproc->procfs.entrys[i].flag,
			&cproc->procfs.entrys[i]);
		cproc->procfs.entrys[i].entry = proc_create_data(
				cproc->procfs.entrys[i].name,
				mode,
				cproc->procfs.procdir,
				&cpproc_fs_fops,
				&cproc->procfs.entrys[i]);
		cproc->procfs.entrys[i].cproc = cproc;
	}
}

static inline void sprd_cproc_fs_exit(struct cproc_device *cproc)
{
	u8 i = 0;

	for (i = 0; i < MAX_CPROC_ENTRY_NUM; i++) {
		if (!cproc->procfs.entrys[i].name)
			break;

		if (cproc->procfs.entrys[i].flag != 0) {
			remove_proc_entry(cproc->procfs.entrys[i].name,
					  cproc->procfs.procdir);
		}
	}

	remove_proc_entry(cproc->name, NULL);
}

static void sprd_cproc_regmap_update_bit(struct cproc_ctrl *ctrl,
					 u32 index,
					 u32 mask,
					 u32 val)
{
	u32 reg, orig, tmp;

	reg = ctrl->ctrl_reg[index];

	/* The bit 31 of one register is 1 means that it doesn't support
	 * bit operation, so we can't use the api regmap_update_bits to update it.
	 * and before use the register, must clear this bit.
	 */
	if (reg & BIT(31)) {
		reg &= ~BIT(31);
		if (regmap_read(ctrl->rmap[index], reg, &orig))
			return;

		tmp = orig & ~mask;
		tmp |= val & mask;

		if (tmp != orig)
			regmap_write(ctrl->rmap[index], reg, tmp);

		return;
	}

	(void)regmap_update_bits(ctrl->rmap[index],
			reg,
			mask,
			val);
}

static void sprd_cproc_regmap_read(struct cproc_ctrl *ctrl,
				u32 index,
				unsigned int *val)
{
	u32 reg;

	reg = ctrl->ctrl_reg[index];
	(void)regmap_read(ctrl->rmap[index], reg, val);
}

static irqreturn_t sprd_cproc_irq_handler(int irq, void *dev_id)
{
	struct cproc_device *cproc = (struct cproc_device *)dev_id;

	pr_debug("%s: cp watchdog enable !\n", __func__);
	cproc->wdtcnt = CPROC_WDT_TRUE;
	cproc->status = CP_WDTIRQ_STATUS;
	wake_up_interruptible_all(&cproc->wdtwait);
	return IRQ_HANDLED;
}

static int sprd_cproc_native_arm_start(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;
	u32 state;

	if (!pdata)
		return -ENODEV;
	ctrl = pdata->ctrl;

	pr_debug("%s: %s, type = 0x%x, status = 0x%x\n",
		__func__,
		pdata->devname,
		cproc->type,
		cproc->status);

	if (strstr(pdata->devname, "cp") ||
		strstr(pdata->devname, "nrphy") ||
		strstr(pdata->devname, "v3phy"))
		cproc_proc_copy_iram(cproc);

	/* clear force shutdown */
	pr_debug("%s: force shutdown reg =0x%x, mask =0x%x\n",
		 __func__,
		 ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN],
		 ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	if (ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] != INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_SHUT_DOWN,
			ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN],
			~ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	/* Clear CPROC_CTRL_EXT0 for some special modem core */
	if (ctrl->ctrl_reg[CPROC_CTRL_EXT0] != INVALID_REG) {
		pr_debug("%s: CPROC_CTRL_EXT0 reg =0x%x, mask =0x%x\n",
			__func__,
			ctrl->ctrl_reg[CPROC_CTRL_EXT0],
			ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT0,
			ctrl->ctrl_mask[CPROC_CTRL_EXT0],
			~ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
	}

	/* clear force deep sleep */
	pr_debug("%s: deep sleep reg =0x%x, mask =0x%x\n",
		 __func__,
		 ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP],
		 ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
	if (ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] != INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_DEEP_SLEEP,
			ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP],
			~ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
	}

	/* clear core reset */
	pr_debug("%s: core reset reg =0x%x, mask =0x%x\n",
		 __func__,
		 ctrl->ctrl_reg[CPROC_CTRL_CORE_RESET],
		 ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET]);
	if (ctrl->ctrl_reg[CPROC_CTRL_CORE_RESET] != INVALID_REG) {
		msleep(50);
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_CORE_RESET,
			ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET],
			~ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET]);

		while (1) {
			sprd_cproc_regmap_read(ctrl,
					       CPROC_CTRL_CORE_RESET,
					       &state);
			if (!(state & ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET]))
				break;
		}
	}

	/* clear sys reset */
	pr_debug("%s: sys reset reg =0x%x, mask =0x%x\n",
		 __func__,
		 ctrl->ctrl_reg[CPROC_CTRL_SYS_RESET],
		 ctrl->ctrl_mask[CPROC_CTRL_SYS_RESET]);
	if (ctrl->ctrl_reg[CPROC_CTRL_SYS_RESET] != INVALID_REG) {
		msleep(50);
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_SYS_RESET,
			ctrl->ctrl_mask[CPROC_CTRL_SYS_RESET],
			~ctrl->ctrl_mask[CPROC_CTRL_SYS_RESET]);

		while (1) {
			sprd_cproc_regmap_read(ctrl,
					       CPROC_CTRL_SYS_RESET,
					       &state);
			if (!(state & ctrl->ctrl_mask[CPROC_CTRL_SYS_RESET]))
				break;
		}
	}

	return 0;
}

static int sprd_cproc_native_arm_stop(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;

	if (!pdata)
		return -ENODEV;
	ctrl = pdata->ctrl;
	ctrl->iram_loaded = 0;
	pr_debug("%s: stop %s\n", __func__, cproc->name);

	/* hold tgdsp and ldsp */
	if ((ctrl->ctrl_reg[CPROC_CTRL_DSPCORE_RESET] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_DSPCORE_RESET,
			ctrl->ctrl_mask[CPROC_CTRL_DSPCORE_RESET],
			ctrl->ctrl_mask[CPROC_CTRL_DSPCORE_RESET]);
	}

	/* reset core */
	if ((ctrl->ctrl_reg[CPROC_CTRL_CORE_RESET] & INVALID_REG)
	    != INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_CORE_RESET,
			ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET],
			ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET]);
		msleep(50);
	}
	/* reset sys */
	if ((ctrl->ctrl_reg[CPROC_CTRL_SYS_RESET] & INVALID_REG)
	    != INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_SYS_RESET,
			ctrl->ctrl_mask[CPROC_CTRL_SYS_RESET],
			ctrl->ctrl_mask[CPROC_CTRL_SYS_RESET]);
		msleep(50);
	}
	if ((ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & INVALID_REG)
	    != INVALID_REG) {
		/* force deep sleep */
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_DEEP_SLEEP,
			ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP],
			ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
		msleep(50);
	}

	/* set CPROC_CTRL_EXT0 for some special modem core */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT0] & INVALID_REG)
	    != INVALID_REG) {
		/* CPROC_CTRL_EXT0 */
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT0,
			ctrl->ctrl_mask[CPROC_CTRL_EXT0],
			ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
	}

	if ((ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & INVALID_REG)
	    != INVALID_REG) {
		/* force shutdown */
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_SHUT_DOWN,
			ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN],
			ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}
	return 0;
}

static int sprd_cproc_native_agdsp_start(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;

	if (!pdata)
		return -ENODEV;

	pr_debug("sprd_cproc: %s\n", __func__);

	ctrl = pdata->ctrl;

	/* clear agdsp sys shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_SHUT_DOWN,
			ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN],
			~ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	/* clear agdsp core shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT0] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT0,
			ctrl->ctrl_mask[CPROC_CTRL_EXT0],
			~ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
	}

	/* clear agdsp core reset */
	if ((ctrl->ctrl_reg[CPROC_CTRL_CORE_RESET] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_CORE_RESET,
			ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET],
			~ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET]);
	}

	/* clear agdsp core deep sleep */
	if ((ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_DEEP_SLEEP,
			ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP],
			~ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
	}

	/* config reg protection */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT2] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT2,
			0xffff,
			~0xffff);
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT2,
			ctrl->ctrl_mask[CPROC_CTRL_EXT2],
			ctrl->ctrl_mask[CPROC_CTRL_EXT2]);
	}

	/* config boot addr */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT3] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT3,
			0xffffffff,
			~0xffffffff);
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT3,
			ctrl->ctrl_mask[CPROC_CTRL_EXT3],
			ctrl->ctrl_mask[CPROC_CTRL_EXT3]);
	}

	/* config agdsp ctrl1 */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT4] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT4,
			ctrl->ctrl_mask[CPROC_CTRL_EXT4],
			ctrl->ctrl_mask[CPROC_CTRL_EXT4]);
	}

	/* config agdsp ctrl */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT5] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT5,
			ctrl->ctrl_mask[CPROC_CTRL_EXT5],
			ctrl->ctrl_mask[CPROC_CTRL_EXT5]);
	}

	/* agdsp reset */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT1] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT1,
			ctrl->ctrl_mask[CPROC_CTRL_EXT1],
			~ctrl->ctrl_mask[CPROC_CTRL_EXT1]);
	}

	return 0;
}

static int sprd_cproc_native_agdsp_stop(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;
	struct cproc_init_data *pdata = cproc->initdata;
	struct cproc_ctrl *ctrl;

	if (!pdata)
		return -ENODEV;

	pr_debug("sprd_cproc: %s\n", __func__);

	ctrl = pdata->ctrl;

	/* agdsp core reset */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT1] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT1,
			ctrl->ctrl_mask[CPROC_CTRL_EXT1],
			ctrl->ctrl_mask[CPROC_CTRL_EXT1]);
		msleep(50);
	}

	/* agdsp sys reset */
	if ((ctrl->ctrl_reg[CPROC_CTRL_CORE_RESET] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_CORE_RESET,
			ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET],
			ctrl->ctrl_mask[CPROC_CTRL_CORE_RESET]);
		msleep(50);
	}

	/* agdsp force deep sleep */
	if ((ctrl->ctrl_reg[CPROC_CTRL_DEEP_SLEEP] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_DEEP_SLEEP,
			ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP],
			ctrl->ctrl_mask[CPROC_CTRL_DEEP_SLEEP]);
		msleep(50);
	}

	/* agdsp core force shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_EXT0] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_EXT0,
			ctrl->ctrl_mask[CPROC_CTRL_EXT0],
			ctrl->ctrl_mask[CPROC_CTRL_EXT0]);
	}

	/* agdsp sys force shutdown */
	if ((ctrl->ctrl_reg[CPROC_CTRL_SHUT_DOWN] & INVALID_REG)
		!= INVALID_REG) {
		sprd_cproc_regmap_update_bit(
			ctrl,
			CPROC_CTRL_SHUT_DOWN,
			ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN],
			ctrl->ctrl_mask[CPROC_CTRL_SHUT_DOWN]);
	}

	return 0;
}

static int sprd_cproc_native_start(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;

	if (!cproc)
		return -ENODEV;

	pr_debug("sprd_cproc: native start type = 0x%x\n", cproc->type);
	if (cproc->type == ARM_CTRL)
		return sprd_cproc_native_arm_start(cproc);
	else if (cproc->type == AGDSP_CTRL)
		return sprd_cproc_native_agdsp_start(cproc);

	return -EINVAL;
}

static int sprd_cproc_native_stop(void *arg)
{
	struct cproc_device *cproc = (struct cproc_device *)arg;

	if (!cproc)
		return -ENODEV;

	pr_debug("sprd_cproc: native stop type = 0x%x\n", cproc->type);
	if (cproc->type == ARM_CTRL)
		return sprd_cproc_native_arm_stop(cproc);
	else if (cproc->type == AGDSP_CTRL)
		return sprd_cproc_native_agdsp_stop(cproc);

	return -EINVAL;
}

static u32 g_common_loader[] = {
	0xe59f0000, 0xe12fff10, 0x8ae00000
};

static u32 g_r5_loader[] = {
	0xee110f10, 0xe3c00005, 0xe3c00a01, 0xf57ff04f,
	0xee010f10, 0xf57ff06f, 0xee110f30, 0xe3800802,
	0xe3800801, 0xe3c00902, 0xee010f30, 0xf57ff04f,
	0xe3a00000, 0xee070f15, 0xee0f0f15, 0xe3a01000,
	0xee061f12, 0xf57ff06f, 0xe3a01000, 0xee061f11,
	0xe3a0103f, 0xee061f51, 0xe3a01f42, 0xee061f91,
	0xee110f30, 0xe3c00802, 0xe3c00801, 0xe3c00902,
	0xee010f30, 0xee110f10, 0xe3800001, 0xf57ff04f,
	0xee010f10, 0xf57ff06f, 0xe51ff004, 0x8b800600
};

struct sprd_cproc_data {
	int (*start)(void *arg);
	int (*stop)(void *arg);
	u32 iram_size;
	u32 *iramdata;
};

static struct sprd_cproc_data g_cproc_data = {
	.start = sprd_cproc_native_start,
	.stop  = sprd_cproc_native_stop,
	.iram_size = sizeof(g_common_loader),
	.iramdata = &g_common_loader[0],
};

static struct sprd_cproc_data g_cproc_data_pubcp = {
	.start = sprd_cproc_native_start,
	.stop  = sprd_cproc_native_stop,
	.iram_size = sizeof(g_r5_loader),
	.iramdata = &g_r5_loader[0],
};

static const struct of_device_id sprd_cproc_match_table[] = {
	{ .compatible = "sprd,scproc", .data = &g_cproc_data},
	{ .compatible = "sprd,scproc_pubcp", .data = &g_cproc_data_pubcp},
	{ },
};

static int sprd_cproc_parse_dt(struct cproc_init_data **init,
			       struct device_node *node)
{
	struct cproc_init_data *pdata;
	struct cproc_ctrl *ctrl;
	struct resource res;
	struct device_node *np = node, *chd;
	int ret, i, segnr;
	u32 iram_dsize;
	u32 cr_num;
	struct cproc_dump_info *dump_info;
	int index;
	u32 syscon_args[2];
	struct device_node *parent_np  = of_get_parent(node);

	struct sprd_cproc_data *pcproc_data;
	const struct of_device_id *of_id =
	of_match_node(sprd_cproc_match_table, parent_np);
	if (of_id) {
		pcproc_data = (struct sprd_cproc_data *)of_id->data;
	} else {
		pr_err("%s: not find matched id!\n", __func__);
		return -1;
	}

	segnr = of_get_child_count(np);
	pr_debug("%s: segnr = %d\n", __func__, segnr);
	pdata = kzalloc(sizeof(*pdata) +
				segnr * sizeof(struct cproc_segments),
				GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl) {
		kfree(pdata);
		return -ENOMEM;
	}

	ret = of_property_read_string(np,
				      "sprd,name",
				      (const char **)&pdata->devname);
	if (ret) {
		pr_err("cproc: parse name error!\n");
		goto error;
	}

	pr_debug("cproc: parse devname = %s\n", pdata->devname);
	/* get ic type */
	ret = of_property_read_u32(np, "sprd,type", (u32 *)&pdata->type);
	if (ret) {
		if (strstr(pdata->devname, "pm"))
			pdata->type = ARM_CTRL;
		else if (strstr(pdata->devname, "agdsp"))
			pdata->type = AGDSP_CTRL;
		else
			pdata->type = ARM_CTRL;
	}

	cr_num = 0;

	do {
		/* get apb & pmu reg handle */
		ctrl->rmap[cr_num] =
			syscon_regmap_lookup_by_name(np,
						     cproc_dts_args[cr_num]);
		if (IS_ERR(ctrl->rmap[cr_num])) {
			pr_info("%s:can't find %s\n",
				__func__, cproc_dts_args[cr_num]);
			break;/* some dts no need config all args, just break */
		}

		/* 1.get ctrl_reg offset, the ctrl-reg variable number, so need
		 * to start reading from the largest until success.
		 *  2.get ctrl_mask
		 */
		ret = syscon_get_args_by_name(np, cproc_dts_args[cr_num],
					      2, (u32 *)syscon_args);
		if (ret == 2) {
			ctrl->ctrl_reg[cr_num] = syscon_args[0];
			ctrl->ctrl_mask[cr_num] = syscon_args[1];
		} else {
			pr_err("%s:failed to map ctrl reg\n", __func__);
			goto error;
		}

		cr_num++;
	} while (NULL != cproc_dts_args[cr_num]);

	ctrl->reg_nr = cr_num;

	for (i = 0; i < cr_num; i++) {
		pr_debug("%s: ctrl_reg[%d] = 0x%x,ctrl_mask[%d] = 0x%08x\n",
			__func__, i, ctrl->ctrl_reg[i], i, ctrl->ctrl_mask[i]);
	}

	for (i = cr_num; i < CPROC_CTRL_NR; i++)
		ctrl->ctrl_reg[i] = INVALID_REG;

	/* get iram size */
	ret = of_property_read_u32(np, "sprd,iram-dsize", &iram_dsize);
	if (ret)
		iram_dsize = MAX_IRAM_DATA_NUM;

	/* get iram data */
	ret = of_property_read_u32_array(np,
					"sprd,iram-data",
					ctrl->iram_data,
					iram_dsize);
	if (ret) {
		ctrl->iram_size = 0;
	} else {
		ctrl->iram_size = iram_dsize * sizeof(u32);
	}

	for (i = 0; i < iram_dsize; i++) {
		pr_debug("%s: iram-data[%d] = 0x%08x\n",
			__func__,
			i,
			ctrl->iram_data[i]);
	}

	index = 0;
	if (ctrl->iram_size > 0) {
		/* get iram_base+offset */
		ret = of_address_to_resource(np, index, &res);
		if (ret) {
			pr_err("cproc: parse iram base error!");
			goto error;
		}
		ctrl->iram_addr = res.start;
		pr_debug("%s: iram_addr=0x%p\n",
			__func__,
			(void *)ctrl->iram_addr);
		index++;
	}

	/* get cp base */
	ret = of_address_to_resource(np, index, &res);
	if (ret) {
		pr_err("cproc: parse cp base error!\n");
		goto error;
	}

	pdata->base = res.start;
	pdata->maxsz = res.end - res.start + 1;
	pr_debug("%s: cp base = 0x%p, size = 0x%x\n",
		__func__,
		(void *)pdata->base,
		pdata->maxsz);
	index++;

	if (pdata->type == ARM_CTRL && (strstr(pdata->devname, "cp") ||
		strstr(pdata->devname, "nrphy") ||
		strstr(pdata->devname, "v3phy"))) {
		/* get mini dump base+offset */
		ret = of_address_to_resource(np, index, &res);
		if (!ret) {
			dump_info = modem_ram_vmap_nocache(SOC_MODEM,
							   res.start,
							   (res.end -
							    res.start + 1));
			if (!dump_info) {
				pr_err("unable to map dump info base: 0x%llx\n",
				       (long long)res.start);
				ret = -ENOMEM;
				goto error;
			}
			pr_debug("%s: mini-dump base = 0x%llx, size = 0x%llx\n",
				__func__,
				(unsigned long long)res.start,
				(unsigned long long)(res.end - res.start + 1));
			pdata->mini_mem = dump_info;
			pdata->mini_mem_paddr = res.start;
			pdata->mini_mem_size = (res.end - res.start + 1);

			index++;
		}
	}

	/* get irq */
	pdata->wdtirq = irq_of_parse_and_map(np, 0);
	pr_debug("%s: wdt irq %u\n", __func__, pdata->wdtirq);

	i = 0;
	for_each_child_of_node(np, chd) {
		struct cproc_segments *seg;

		seg = &pdata->segs[i];
		ret = of_property_read_string(chd,
					"cproc,name",
					(const char **)&seg->name);
		if (ret) {
			pr_err("cproc: parse chil[%d] name error!\n", i);
			goto error;
		}
		pr_debug("%s: child node [%d] name=%s\n",
			__func__, i, seg->name);
		/* get child base addr */
		ret = of_address_to_resource(chd, 0, &res);
		if (ret) {
			pr_err("cproc: parse chil[%d] base error!\n", i);
			goto error;
		}
		seg->base = res.start;
		seg->maxsz = res.end - res.start + 1;
		pr_debug("%s: child node [%d] base=0x%x, size=0x%0x\n",
			__func__, i, seg->base, seg->maxsz);
		i++;
	}

	pdata->segnr = segnr;

	pdata->start = pcproc_data->start;
	pdata->stop = pcproc_data->stop;

	pdata->ctrl = ctrl;
	*init = pdata;
	return 0;
error:
	if (pdata->mini_mem)
		modem_ram_unmap(SOC_MODEM, pdata->mini_mem);
	kfree(ctrl);
	kfree(pdata);
	return -EINVAL;
}

static void sprd_cproc_destroy_pdata(struct cproc_init_data **init)
{
	struct cproc_init_data *pdata = *init;

	if (pdata) {
		if (pdata->mini_mem)
			modem_ram_unmap(SOC_MODEM, pdata->mini_mem);
		kfree(pdata->ctrl);
		kfree(pdata);
	}
	*init = NULL;
}

static int sprd_cproc_probe(struct platform_device *pdev)
{
	struct cproc_device *cproc;
	struct cproc_init_data *pdata = pdev->dev.platform_data;
	int rval = 0;
	struct device_node *np, *chd;
	int segnr;

	if (!pdata && pdev->dev.of_node) {
		np = pdev->dev.of_node;
		segnr = of_get_child_count(np);
		pr_debug("%s: segnr = %d\n", __func__, segnr);

		for_each_child_of_node(np, chd) {
			rval = sprd_cproc_parse_dt(&pdata, chd);
			if (rval || !pdata) {
				pr_err("%s: failed to parse device tree!\n",
				       __func__);
				/*
				 * if one subsystem parse fail,
				 * just print an error log,
				 * continue parse next one.
				 */
				continue;
			}

#if defined(CONFIG_DEBUG_FS)
			sprd_cproc_init_debugfs();

			if (cproc_root) {
				debugfs_create_file(pdata->devname, S_IRUGO,
							cproc_root,
							pdata,
							&sprd_cproc_debug_fops);
			}
#endif

			cproc = kzalloc(sizeof(*cproc), GFP_KERNEL);
			if (!cproc) {
				sprd_cproc_destroy_pdata(&pdata);
				pr_err("%s: failed to allocate cproc device!\n",
				       __func__);
				return -ENOMEM;
			}

			pr_debug("%s: 0x%p 0x%x\n",
				 __func__,
				 (void *)pdata->base,
				 pdata->maxsz);

			cproc->initdata = pdata;

			cproc->miscdev.minor = MISC_DYNAMIC_MINOR;
			cproc->miscdev.name = cproc->initdata->devname;
			cproc->miscdev.fops = &sprd_cproc_fops;
			cproc->miscdev.parent = NULL;
			cproc->name = cproc->initdata->devname;
			cproc->type = cproc->initdata->type;
			rval = misc_register(&cproc->miscdev);
			if (rval) {
				sprd_cproc_destroy_pdata(&cproc->initdata);
				kfree(cproc);
				pr_err("%s:register miscdev! failed\n",
				       __func__);
				return rval;
			}

			pr_debug("%s: cp boot mode: 0x%x\n",
				 __func__,
				 cp_boot_mode);
			if (!cp_boot_mode)
				cproc->status = CP_NORMAL_STATUS;
			else
				cproc->status = CP_STOP_STATUS;
			cproc->wdtcnt = CPROC_WDT_FLASE;
			init_waitqueue_head(&cproc->wdtwait);

			/* register IPI irq */
			if (cproc->initdata->wdtirq > 0) {
				rval = devm_request_irq(&pdev->dev,
							cproc->initdata->wdtirq,
							sprd_cproc_irq_handler,
							0,
							cproc->initdata->devname,
							cproc);
				if (rval != 0) {
					misc_deregister(&cproc->miscdev);
					sprd_cproc_destroy_pdata(&cproc->initdata);
					pr_err("%s: failed to request irq\n",
					       __func__);
					kfree(cproc);
					return rval;
				}
			}

			sprd_cproc_fs_init(cproc);

			platform_set_drvdata(pdev, cproc);

			pr_debug("%s: %s!\n",
				 __func__,
				 cproc->initdata->devname);
		}
	}

	return 0;
}

static int sprd_cproc_remove(struct platform_device *pdev)
{
	struct cproc_device *cproc = platform_get_drvdata(pdev);

	sprd_cproc_fs_exit(cproc);
	misc_deregister(&cproc->miscdev);
	pr_debug("%s: %s!\n", __func__, cproc->initdata->devname);
	sprd_cproc_destroy_pdata(&cproc->initdata);

	kfree(cproc);

	return 0;
}

static struct platform_driver sprd_cproc_driver = {
	.probe    = sprd_cproc_probe,
	.remove   = sprd_cproc_remove,
	.driver   = {
		.owner = THIS_MODULE,
		.name = "sprd_cproc",
		.of_match_table = sprd_cproc_match_table,
	},
};

static int __init sprd_cproc_init(void)
{
	if (platform_driver_register(&sprd_cproc_driver) != 0) {
		pr_err("%s: platform drv register Failed\n", __func__);
		return -1;
	}

	return 0;
}

static void __exit sprd_cproc_exit(void)
{
	platform_driver_unregister(&sprd_cproc_driver);
}

#if defined(CONFIG_DEBUG_FS)
static int sprd_cproc_debug_show(struct seq_file *m, void *private)
{
	struct cproc_init_data *pdata = (struct cproc_init_data *)m->private;
	struct cproc_ctrl *ctrl;
	struct cproc_segments *segs;
	int i;

	sipc_debug_putline(m, '*', 100);

	if (!pdata)
		return 0;

	seq_printf(m, "cproc %s info:\n", pdata->devname);
	seq_printf(m, "wdtirq: %d, type: %d\n", pdata->wdtirq, pdata->type);
	seq_printf(m, "base: 0x%08x, size: 0x%08x\n",
		   (u32)pdata->base, pdata->maxsz);

	if (pdata->mini_mem)
		seq_printf(m, "mini dump mem: 0x%08x, size: 0x%08x\n",
			   (u32)pdata->mini_mem_paddr,
			   pdata->mini_mem_size);

	if (pdata->ctrl) {
		ctrl = pdata->ctrl;
		if (ctrl->iram_size > 0) {
			seq_printf(m, "iram_addr: 0x%08x, iram_size: 0x%08x\n",
				   (u32)ctrl->iram_addr, ctrl->iram_size);

			sipc_debug_putline(m, '-', 80);
			seq_puts(m, "iram data list:\n");

			for (i = 0; i < ctrl->iram_size / sizeof(u32); i++) {
				if ((i + 1) % 4 == 0)
					seq_printf(m, "0x%08x\n",
						   ctrl->iram_data[i]);
				else
					seq_printf(m, "0x%08x, ",
						   ctrl->iram_data[i]);
			}
			seq_puts(m, "\n");
			sipc_debug_putline(m, '-', 80);
		}

		sipc_debug_putline(m, '-', 80);
		seq_puts(m, "ctrl reg list:\n");

		for (i = 0; i < ctrl->reg_nr; i++)
			seq_printf(m, "reg[%2d]: reg=0x%08x, mask=0x%08x\n",
				   i,
				   ctrl->ctrl_reg[i],
				   ctrl->ctrl_mask[i]);
		sipc_debug_putline(m, '-', 80);
	}

	segs = pdata->segs;
	sipc_debug_putline(m, '-', 80);
	seq_puts(m, "segment list:\n");

	for (i = 0; i < pdata->segnr; i++)
		seq_printf(m, "segment[%2d]:base=0x%08x, size=0x%08x, name=%s\n",
			   i,
			   segs[i].base,
			   segs[i].maxsz,
			   segs[i].name);

	sipc_debug_putline(m, '-', 80);

	return 0;
}

static int sprd_cproc_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_cproc_debug_show, inode->i_private);
}

static const struct file_operations sprd_cproc_debug_fops = {
	.open = sprd_cproc_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void sprd_cproc_init_debugfs(void)
{
	if (!cproc_root)
		cproc_root = debugfs_create_dir("cproc", NULL);
}
#endif /* CONFIG_DEBUG_FS */

module_init(sprd_cproc_init);
module_exit(sprd_cproc_exit);

MODULE_DESCRIPTION("SPRD Communication Processor Driver");
MODULE_LICENSE("GPL");
