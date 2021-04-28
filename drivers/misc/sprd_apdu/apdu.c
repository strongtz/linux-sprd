/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

/* apdu channel reg offset */
#define APDU_TEE_OFFSET	0x0
#define APDU_REE_OFFSET	0xa0
#define APDU_CP_OFFSET	0x140

#define APDU_STATUS0		(APDU_REE_OFFSET + 0x0)
#define APDU_STATUS1		(APDU_REE_OFFSET + 0x4)
#define APDU_WATER_MARK		(APDU_REE_OFFSET + 0x8)
#define APDU_INT_EN		(APDU_REE_OFFSET + 0xc)
#define APDU_INT_RAW		(APDU_REE_OFFSET + 0x10)
#define APDU_INT_MASK		(APDU_REE_OFFSET + 0x14)
#define APDU_INT_CLR		(APDU_REE_OFFSET + 0x18)
#define APDU_CNT_CLR		(APDU_REE_OFFSET + 0x1c)
#define APDU_TX_FIFO		(APDU_REE_OFFSET + 0x20)
#define APDU_RX_FIFO		(APDU_REE_OFFSET + 0x60)

#define APDU_FIFO_RX_OFFSET	16
#define APDU_FIFO_LEN_MASK	GENMASK(7, 0)

#define APDU_INT_BITS	(BIT(9) | BIT(8) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0))

#define APDU_DRIVER_NAME	"apdu"

/* packet was alinged with 4 byte should not exceed 5120+ pad byte */
#define APDU_MAX_SIZE		(5120 + 4)
#define APDU_FIFO_LENTH	128
#define APDU_FIFO_SIZE		(APDU_FIFO_LENTH * 4)
#define APDU_MAGIC_NUM	0x55AA

#define APDU_RESET		_IO('U', 0)
#define APDU_CLR_FIFO		_IO('U', 1)
#define APDU_SET_WATER		_IO('U', 2)

struct sprd_apdu_device {
	struct device *dev;
	struct miscdevice misc;
	wait_queue_head_t read_wq;
	int rx_done;
	void *tx_buf;
	void *rx_buf;

	/* synchronize access to our device file */
	struct mutex mutex;
	void __iomem *base;
	int irq;
};

static void sprd_apdu_int_en(void __iomem *base, u32 int_en)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INT_EN);
	all_int |= (int_en & APDU_INT_BITS);
	writel_relaxed(all_int, base + APDU_INT_EN);
}

static void sprd_apdu_int_dis(void __iomem *base, u32 int_dis)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INT_EN);
	all_int &= ~(int_dis & APDU_INT_BITS);
	writel_relaxed(all_int, base + APDU_INT_EN);
}

static void sprd_apdu_clear_int(void __iomem *base, u32 clear_int)
{
	writel_relaxed(clear_int & APDU_INT_BITS, base + APDU_INT_CLR);
}

static void sprd_apdu_rst(void __iomem *base)
{
	/* missing reset apdu */
	sprd_apdu_clear_int(base, APDU_INT_BITS);
}

static void sprd_apdu_clear_fifo(void __iomem *base)
{
	writel_relaxed(BIT(4), base + APDU_CNT_CLR);
}

static void sprd_apdu_read_rx_fifo(void __iomem *base,
				  u32 *data_ptr, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++) {
		data_ptr[i] = readl_relaxed(base + APDU_RX_FIFO);
		pr_debug("r_data[%d]:0x%08x ", i, data_ptr[i]);
	}
}

static void sprd_apdu_write_tx_fifo(void __iomem *base,
				    u32 *data_ptr, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++) {
		writel_relaxed(data_ptr[i], base + APDU_TX_FIFO);
		pr_debug("w_data[%d]:0x%08x ", i, data_ptr[i]);
	}
}

static u32 sprd_apdu_get_rx_fifo_len(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status = (fifo_status >> APDU_FIFO_RX_OFFSET) & APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_tx_fifo_len(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status &= APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static int sprd_apdu_read_data(struct sprd_apdu_device *apdu,
			       u32 *buf, u32 count)
{
	u32 rx_fifo_status;
	u32 once_read_len, left;
	u32 index = 0, loop = 0;
	u32 need_read = count;

	while (need_read > 0) {
		do {
			rx_fifo_status = sprd_apdu_get_rx_fifo_len(apdu->base);
			dev_dbg(apdu->dev, "rx_fifo_status:0x%x\n",
				rx_fifo_status);
			if (rx_fifo_status)
				break;

			if (++loop >= 100) {
				dev_err(apdu->dev, "apdu read data timeout!\n");
				return -EBUSY;
			}
			/* read timeout is less len 1~2s */
			usleep_range(10000, 20000);
		} while (1);

		once_read_len = (need_read < rx_fifo_status) ?
			need_read : rx_fifo_status;
		sprd_apdu_read_rx_fifo(apdu->base, &buf[index], once_read_len);

		index += once_read_len;
		need_read -= once_read_len;
		if (need_read == 0) {
			left = sprd_apdu_get_rx_fifo_len(apdu->base);
			if (left)
				dev_err(apdu->dev, "read left len:%d\n", left);
			break;
		}
	}
	return 0;
}

static int sprd_apdu_write_data(struct sprd_apdu_device *apdu,
				void *buf, u32 count)
{
	u32 *data_buffer = (u32 *)buf;
	u32 len, pad_len;
	u32 tx_fifo_data_remain;
	u32 data_to_write;
	u32 index = 0, loop = 0;

	pad_len = count % 4;
	if (pad_len) {
		memset(buf + count, 0, 4 - pad_len);
		len = count / 4 + 1;
	} else {
		len = count / 4;
	}

	while (len > 0) {
		do {
			tx_fifo_data_remain = APDU_FIFO_LENTH -
				sprd_apdu_get_tx_fifo_len(apdu->base);
			if (tx_fifo_data_remain)
				break;

			if (++loop >= 100) {
				dev_err(apdu->dev, "write data timeout!\n");
				return -EBUSY;
			}
			/* write timeout is less len 1~2s */
			usleep_range(10000, 20000);
		} while (1);

		data_to_write = (len < tx_fifo_data_remain) ?
			len : tx_fifo_data_remain;
		sprd_apdu_write_tx_fifo(apdu->base,
				&data_buffer[index], data_to_write);
		index += data_to_write;
		len -= data_to_write;
	}

	return 0;
}

static ssize_t sprd_apdu_read(struct file *fp, char __user *buf,
			     size_t count, loff_t *f_pos)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	ssize_t r = count;
	u32 xfer, actual, first, data_len;
	int ret;

	apdu->rx_done = 0;
	ret = wait_event_interruptible(apdu->read_wq, apdu->rx_done);
	if (ret < 0)
		return ret;

	/* check apdu packet valid */
	sprd_apdu_read_rx_fifo(apdu->base, &first, 1);
	if (((first >> 16) & 0xffff) != APDU_MAGIC_NUM) {
		dev_err(apdu->dev, "apdu read data magic error!\n");
		return -EINVAL;
	}

	data_len = first & 0xffff;
	if (data_len > APDU_MAX_SIZE) {
		dev_err(apdu->dev, "apdu read len:%d exceed max:%d!\n",
					data_len, APDU_MAX_SIZE);
		return -EINVAL;
	}

	ret = sprd_apdu_read_data(apdu, (u32 *)apdu->rx_buf, data_len);
	if (ret < 0)
		return ret;

	actual = (data_len + 1) * 4;
	xfer = (actual < count) ? actual : count;
	r = xfer;
	if (copy_to_user(buf, apdu->rx_buf, xfer))
		r = -EFAULT;

	return r;
}

static ssize_t sprd_apdu_write(struct file *fp, const char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	ssize_t r = count;
	u32 xfer;
	int ret;

	mutex_lock(&apdu->mutex);

	while (count > 0) {
		if (count > APDU_MAX_SIZE)
			xfer = APDU_MAX_SIZE;
		else
			xfer = count;
		if (xfer && copy_from_user(apdu->tx_buf, buf, xfer)) {
			r = -EFAULT;
			goto end;
		}

		ret = sprd_apdu_write_data(apdu, apdu->tx_buf, xfer);
		if (ret) {
			r = ret;
			goto end;
		}
		buf += xfer;
		count -= xfer;
	}

end:
	mutex_unlock(&apdu->mutex);
	return r;
}

static long sprd_apdu_ioctl(struct file *fp,
	unsigned int code, unsigned long value)
{
	struct sprd_apdu_device *apdu = fp->private_data;

	mutex_lock(&apdu->mutex);
	switch (code) {
	case APDU_RESET:
		sprd_apdu_rst(apdu->base);
		break;
	case APDU_CLR_FIFO:
		sprd_apdu_clear_fifo(apdu->base);
		break;
	}
	mutex_unlock(&apdu->mutex);
	return 0;
}

static int sprd_apdu_open(struct inode *inode, struct file *fp)
{
	struct sprd_apdu_device *apdu =
		container_of(fp->private_data, struct sprd_apdu_device, misc);

	fp->private_data = apdu;

	return 0;
}

static int sprd_apdu_release(struct inode *inode, struct file *fp)
{
	fp->private_data = NULL;

	return 0;
}

static const struct file_operations sprd_apdu_fops = {
	.owner = THIS_MODULE,
	.read = sprd_apdu_read,
	.write = sprd_apdu_write,
	.unlocked_ioctl = sprd_apdu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sprd_apdu_ioctl,
#endif
	.open = sprd_apdu_open,
	.release = sprd_apdu_release,
};

static irqreturn_t sprd_apdu_interrupt(int irq, void *data)
{
	u32 reg;
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)data;

	reg = readl_relaxed(apdu->base + APDU_INT_MASK);
	writel_relaxed(reg, apdu->base + APDU_INT_CLR);

	apdu->rx_done = 1;
	wake_up(&apdu->read_wq);
	return IRQ_HANDLED;
}

static void sprd_apdu_enable(struct sprd_apdu_device *apdu)
{
	/* REG_PMU_APB_ESE_DSLP_ENA and REG_PMU_APB_PD_ESE_SYS_CFG
	 * should be configured in uboot to power up ese.
	 */

	sprd_apdu_clear_int(apdu->base, APDU_INT_BITS);
	sprd_apdu_int_dis(apdu->base, APDU_INT_BITS);
	sprd_apdu_int_en(apdu->base, BIT(0));
}

static void sprd_apdu_dump_data(u32 *buf, u32 len)
{
	int i;

	for (i = 0; i < len; i++)
		pr_info("0x%8x ", buf[i]);
	pr_info("\n");
}

static ssize_t get_random_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 rep_data[0x10];
	int ret;
	char cmd_get_random[12] = {
		0x05, 0x00, 0xaa, 0x55,
		0x00, 0x84, 0x00, 0x00,
		0x04, 0x00, 0x00, 0x00
	};

	if (!apdu)
		return -EINVAL;

	ret = sprd_apdu_write_data(apdu, (void *)cmd_get_random, 12);
	if (ret < 0) {
		dev_err(apdu->dev,
			"get_random test:write error(%d)\n", ret);
		return ret;
	}

	apdu->rx_done = 0;
	wait_event_interruptible(apdu->read_wq, apdu->rx_done);
	/* get_random le=4 byte, return random data len = le + status(2 byte) */
	ret = sprd_apdu_read_data(apdu, rep_data, 3);
	if (ret < 0) {
		dev_err(apdu->dev,
			"get_random test:read error(%d)\n", ret);
		return ret;
	}
	sprd_apdu_dump_data(rep_data, 3);

	return 0;
}
static DEVICE_ATTR_RO(get_random);

static struct attribute *sprd_apdu_attrs[] = {
	&dev_attr_get_random.attr,
	NULL
};
ATTRIBUTE_GROUPS(sprd_apdu);

static int sprd_apdu_probe(struct platform_device *pdev)
{
	struct sprd_apdu_device *apdu;
	struct resource *res;
	u32 buf_sz = APDU_MAX_SIZE;
	int ret;

	apdu = devm_kzalloc(&pdev->dev, sizeof(*apdu), GFP_KERNEL);
	if (!apdu)
		return -ENOMEM;

	apdu->irq = platform_get_irq(pdev, 0);
	if (apdu->irq < 0) {
		dev_err(&pdev->dev, "failed to get apdu interrupt.\n");
		return apdu->irq;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	apdu->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(apdu->base)) {
		dev_err(&pdev->dev, "no base specified\n");
		return PTR_ERR(apdu->base);
	}

	apdu->tx_buf = devm_kzalloc(&pdev->dev, buf_sz, GFP_KERNEL);
	if (!apdu->tx_buf)
		return -ENOMEM;
	apdu->rx_buf = devm_kzalloc(&pdev->dev, buf_sz, GFP_KERNEL);
	if (!apdu->rx_buf)
		return -ENOMEM;

	mutex_init(&apdu->mutex);
	init_waitqueue_head(&apdu->read_wq);

	apdu->misc.minor = MISC_DYNAMIC_MINOR;
	apdu->misc.name = APDU_DRIVER_NAME;
	apdu->misc.fops = &sprd_apdu_fops;
	ret = misc_register(&apdu->misc);
	if (ret) {
		dev_err(&pdev->dev, "misc_register FAILED\n");
		return ret;
	}

	apdu->dev = &pdev->dev;
	ret = devm_request_threaded_irq(apdu->dev, apdu->irq,
			sprd_apdu_interrupt, NULL, 0, "apdu", apdu);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq %d\n", ret);
		misc_deregister(&apdu->misc);
		return ret;
	}

	ret = sysfs_create_groups(&apdu->dev->kobj, sprd_apdu_groups);
	if (ret)
		dev_warn(apdu->dev, "failed to create apdu attributes\n");

	sprd_apdu_enable(apdu);
	platform_set_drvdata(pdev, apdu);

	return 0;
}

static int sprd_apdu_remove(struct platform_device *pdev)
{
	struct sprd_apdu_device *apdu = platform_get_drvdata(pdev);

	misc_deregister(&apdu->misc);
	sysfs_remove_groups(&apdu->dev->kobj, sprd_apdu_groups);
	return 0;
}

static const struct of_device_id sprd_apdu_match[] = {
	{.compatible = "sprd,roc1-apdu"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_apdu_match);

static struct platform_driver sprd_apdu_driver = {
	.probe = sprd_apdu_probe,
	.remove = sprd_apdu_remove,
	.driver = {
		.name = "sprd-apdu",
		.of_match_table = sprd_apdu_match,
	},
};

module_platform_driver(sprd_apdu_driver);
