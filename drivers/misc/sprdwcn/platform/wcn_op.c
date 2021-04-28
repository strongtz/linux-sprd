#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/miscdevice.h>
#include <misc/marlin_platform.h>
#include <linux/module.h>
#include <linux/major.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <misc/wcn_bus.h>
#include "../sleep/slp_mgr.h"

#define WCN_OP_NAME	"wcn_op"

#define  IOCTL_WCN_OP_READ		0xFF01
#define  IOCTL_WCN_OP_WRITE		0xFF02

struct wcn_op_attr_t {
	unsigned int addr;
	unsigned int val;
	int length;
};

static int wcn_op_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int wcn_op_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int wcn_op_read(struct wcn_op_attr_t *wcn_op_attr, unsigned int *pval)
{
	int ret;

	if (unlikely(marlin_get_download_status() != true))
		return -EIO;

	ret = sprdwcn_bus_direct_read(wcn_op_attr->addr, pval,
				      wcn_op_attr->length);
	if (ret < 0) {
		pr_err("%s read reg error:%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int wcn_op_write(struct wcn_op_attr_t *wcn_op_attr)
{
	int ret = 0;

	if (unlikely(marlin_get_download_status() != true))
		return -EIO;

	ret = sprdwcn_bus_direct_write(wcn_op_attr->addr,
				       &wcn_op_attr->val, wcn_op_attr->length);
	if (ret < 0) {
		pr_err("%s write reg error:%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static long wcn_op_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;
	struct wcn_op_attr_t *wcn_op_attr;
	unsigned int __user *pbuf = (unsigned int __user *)arg;

	if (pbuf == NULL)
		return  ret;

	wcn_op_attr = kmalloc(sizeof(*wcn_op_attr), GFP_KERNEL);
	if (!wcn_op_attr)
		return -ENOMEM;

	if (copy_from_user(wcn_op_attr, pbuf, sizeof(*wcn_op_attr))) {
		pr_err("%s copy from user error!\n",
				__func__);
		kfree(wcn_op_attr);
		return -EFAULT;
	}

	pr_info("WCN OPERATION IOCTL: 0x%x.\n", cmd);

	switch (cmd) {

	case IOCTL_WCN_OP_READ:
		ret = wcn_op_read(wcn_op_attr, &wcn_op_attr->val);
		if (ret == 0) {
			if (copy_to_user(pbuf, wcn_op_attr,
					 sizeof(*wcn_op_attr))) {
				pr_err("%s copy from user error!\n", __func__);
				kfree(wcn_op_attr);
				return -EFAULT;
			}
		} else
			pr_err("wcn_op_read return fail\n");
		break;

	case IOCTL_WCN_OP_WRITE:
		wcn_op_write(wcn_op_attr);
		break;
	}

	kfree(wcn_op_attr);

	return 0;
}

static const struct file_operations wcn_op_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl	= wcn_op_ioctl,
	.open  = wcn_op_open,
	.release = wcn_op_release,
};

static struct miscdevice wcn_op_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = WCN_OP_NAME,
	.fops = &wcn_op_fops,
};

int wcn_op_init(void)
{
	int ret;

	pr_info("%s\n", __func__);
	ret = misc_register(&wcn_op_device);
	if (ret) {
		pr_err("wcn operation dev add failed!!!\n");
		return ret;
	}

	return 0;
}

void wcn_op_exit(void)
{
	misc_deregister(&wcn_op_device);
}
