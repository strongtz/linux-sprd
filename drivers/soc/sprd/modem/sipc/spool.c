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

#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/sipc.h>

#include "spool.h"

struct spool_device {
	struct spool_init_data	*init;
	int			major;
	int			minor;
	struct cdev		cdev;
};

struct spool_sblock {
	u8			dst;
	u8			channel;
	bool			is_hold;
	struct sblock	hold;
};

static struct class		*spool_class;

static int spool_open(struct inode *inode, struct file *filp)
{
	static struct spool_device *spool;
	struct spool_sblock *sblock;
	int ret;

	spool = container_of(inode->i_cdev, struct spool_device, cdev);
	ret = sblock_query(spool->init->dst, spool->init->channel);
	if (ret)
		return ret;
	sblock = kmalloc(sizeof(struct spool_sblock), GFP_KERNEL);
	if (!sblock)
		return -ENOMEM;
	filp->private_data = sblock;

	sblock->dst = spool->init->dst;
	sblock->channel = spool->init->channel;
	sblock->is_hold = 0;

	return 0;
}

static int spool_release(struct inode *inode, struct file *filp)
{
	struct spool_sblock *sblock = filp->private_data;

	if (sblock->is_hold) {
		if (sblock_release(sblock->dst, sblock->channel, &sblock->hold))
			pr_debug("failed to release block!\n");
	}
	kfree(sblock);

	return 0;
}

static ssize_t spool_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct spool_sblock *sblock = filp->private_data;
	int timeout = -1;
	int ret = 0;
	int rdsize = 0;
	struct sblock blk = {0};

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	if (sblock->is_hold) {
		if (count < sblock->hold.length - *ppos) {
			rdsize = count;
		} else {
			rdsize = sblock->hold.length - *ppos;
			sblock->is_hold = 0;
		}
		blk = sblock->hold;
	} else{
		*ppos = 0;
		ret = sblock_receive(sblock->dst,
				sblock->channel, &blk, timeout);
		if (ret < 0) {
			pr_err("spool_read: failed to receive block!\n");
			return ret;
		}
		if (blk.length <= count)
			rdsize = blk.length;
		else {
			rdsize = count;
			sblock->is_hold = 1;
			sblock->hold = blk;
		}
	}

	if (unalign_copy_to_user(buf, blk.addr + *ppos, rdsize)) {
		pr_err("spool_read: failed to copy to user!\n");
		sblock->is_hold = 0;
		*ppos = 0;
		ret = -EFAULT;
	} else {
		ret = rdsize;
		*ppos += rdsize;
	}

	if (sblock->is_hold == 0) {
		if (sblock_release(sblock->dst, sblock->channel, &blk))
			pr_err("failed to release block!\n");
	}

	return ret;
}

static ssize_t spool_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct spool_sblock *sblock = filp->private_data;
	int timeout = -1;
	int ret = 0;
	int wrsize = 0;
	int pos = 0;
	struct sblock blk = {0};
	size_t len = count;

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	do {
		ret = sblock_get(sblock->dst, sblock->channel, &blk, timeout);
		if (ret < 0) {
			pr_info("spool_write: failed to get block!\n");
			return ret;
		}

		wrsize = (blk.length > len ? len : blk.length);
		if (unalign_copy_from_user(blk.addr, buf + pos, wrsize)) {
			pr_info("spool_write: failed to copy from user!\n");
			ret = -EFAULT;
		} else {
			blk.length = wrsize;
			len -= wrsize;
			pos += wrsize;
		}

		if (sblock_send(sblock->dst, sblock->channel, &blk))
			pr_debug("spool_write: failed to send block!");
	} while (len > 0 && ret == 0);

	return count - len;
}

static unsigned int spool_poll(struct file *filp, poll_table *wait)
{
	struct spool_sblock *sblock = filp->private_data;

	return sblock_poll_wait(sblock->dst, sblock->channel, filp, wait);
}

static long spool_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations spool_fops = {
	.open		= spool_open,
	.release	= spool_release,
	.read		= spool_read,
	.write		= spool_write,
	.poll		= spool_poll,
	.unlocked_ioctl	= spool_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

static int spool_parse_dt(struct spool_init_data **init, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct spool_init_data *pdata = NULL;
	int ret;
	u32 data;

	pdata = devm_kzalloc(dev, sizeof(struct spool_init_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_property_read_string(np, "sprd,name",
				      (const char **)&pdata->name);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,dst", (u32 *)&data);
	if (ret)
		goto error;
	pdata->dst = (u8)data;
	ret = of_property_read_u32(np, "sprd,channel", (u32 *)&data);
	if (ret)
		goto error;
	pdata->channel = (u8)data;
	ret = of_property_read_u32(np, "sprd,tx-blksize",
				   (u32 *)&pdata->txblocksize);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,tx-blknum",
				   (u32 *)&pdata->txblocknum);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,rx-blksize",
				   (u32 *)&pdata->rxblocksize);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,rx-blknum",
				   (u32 *)&pdata->rxblocknum);
	if (ret)
		goto error;

	if (!of_property_read_u32(np, "sprd,nodev", (u32 *)&data))
		pdata->nodev = (u8)data;

	*init = pdata;
	return ret;
error:
	devm_kfree(dev, pdata);
	*init = NULL;
	return ret;
}
static int spool_probe(struct platform_device *pdev)
{
	struct spool_init_data *init = pdev->dev.platform_data;
	struct spool_device *spool;
	dev_t devid;
	int rval;
	if (!init && pdev->dev.of_node) {
		rval = spool_parse_dt(&init, &pdev->dev);
		if (rval) {
			pr_err("Failed to parse spool device tree, ret=%d\n",
			       rval);
			return rval;
		}
	}

	rval = sblock_create(init->dst, init->channel, init->txblocknum,
		init->txblocksize, init->rxblocknum, init->rxblocksize);
	if (rval != 0) {
		pr_info("Failed to create sblock: %d\n", rval);
		return rval;
	}

	spool = devm_kzalloc(&pdev->dev,
			     sizeof(struct spool_device),
			     GFP_KERNEL);
	if (spool == NULL) {
		sblock_destroy(init->dst, init->channel);
		pr_info("Failed to allocate spool_device\n");
		return -ENOMEM;
	}

	rval = alloc_chrdev_region(&devid, 0, 1, init->name);
	if (rval != 0) {
		sblock_destroy(init->dst, init->channel);
		devm_kfree(&pdev->dev, spool);
		pr_info("Failed to alloc spool chrdev\n");
		return rval;
	}

	if (!init->nodev) {
		cdev_init(&spool->cdev, &spool_fops);
		rval = cdev_add(&spool->cdev, devid, 1);
		if (rval != 0) {
			sblock_destroy(init->dst, init->channel);
			devm_kfree(&pdev->dev, spool);
			unregister_chrdev_region(devid, 1);
			pr_info("Failed to add spool cdev\n");
			return rval;
		}
	}

	spool->major = MAJOR(devid);
	spool->minor = MINOR(devid);

	device_create(spool_class, NULL,
		      MKDEV(spool->major, spool->minor),
		      NULL, "%s", init->name);

	spool->init = init;

	platform_set_drvdata(pdev, spool);

	return 0;
}

static int spool_remove(struct platform_device *pdev)
{
	struct spool_device *spool = platform_get_drvdata(pdev);

	device_destroy(spool_class, MKDEV(spool->major, spool->minor));

	if (!spool->init->nodev)
		cdev_del(&spool->cdev);

	unregister_chrdev_region(
	MKDEV(spool->major, spool->minor), 1);

	sblock_destroy(spool->init->dst, spool->init->channel);
	devm_kfree(&pdev->dev, spool);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id spool_match_table[] = {
	{.compatible = "sprd,spool", },
	{ },
};
static struct platform_driver spool_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "spool",
		.of_match_table = spool_match_table,
	},
	.probe = spool_probe,
	.remove = spool_remove,
};

static int __init spool_init(void)
{
	spool_class = class_create(THIS_MODULE, "spool");
	if (IS_ERR(spool_class))
		return PTR_ERR(spool_class);

	return platform_driver_register(&spool_driver);
}

static void __exit spool_exit(void)
{
	class_destroy(spool_class);
	platform_driver_unregister(&spool_driver);
}

module_init(spool_init);
module_exit(spool_exit);

MODULE_AUTHOR("Qiu Yi");
MODULE_DESCRIPTION("SIPC/SPOOL driver");
MODULE_LICENSE("GPL");
