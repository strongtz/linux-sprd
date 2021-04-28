/*
 * Copyright (C) 2018,2019 Spreadtrum Communications Inc.
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

#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched/task.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "log-fw.h"
#include "log-tp.h"

static struct forwarder *alloc_forwarder(struct device *dev)
{
	struct forwarder *pfw = (struct forwarder *)
		devm_kmalloc(dev, sizeof(*pfw), GFP_KERNEL);

	if (pfw) {
		int i;

		INIT_LIST_HEAD(&pfw->node);
		pfw->src_num = 0;
		for (i = 0; i < ARRAY_SIZE(pfw->src); ++i)
			pfw->src[i] = NULL;
		mutex_init(&pfw->set_lock);
		pfw->thread = NULL;
		init_waitqueue_head(&pfw->wq);
		spin_lock_init(&pfw->flag_lock);
		pfw->data_flags = 0;
	}

	return pfw;
}

static void clear_channel(struct device *dev,
			  struct data_channel *dchan)
{
	if (dchan->type == CT_USB && dchan->u.usb.path) {
		devm_kfree(dev, dchan->u.usb.path);
		dchan->u.usb.path = NULL;
	}
}

static void free_forwarder(struct device *dev, struct forwarder *pfw)
{
	int i;

	if (pfw->thread) {
		put_task_struct(pfw->thread);
		pfw->thread = NULL;
	}

	mutex_destroy(&pfw->set_lock);

	for (i = 0; i < pfw->src_num; ++i) {
		clear_channel(dev, pfw->src[i]);
		devm_kfree(dev, pfw->src[i]);
		pfw->src[i] = NULL;
	}

	devm_kfree(dev, pfw);
}

static const char *get_token(const char *data, size_t len, size_t *tlen)
{
	const char *endp = data + len;
	const char *p1;

	while (data < endp) {
		char c = *data;

		if (' ' != c && '\t' != c && '\r' != c && '\n' != c)
			break;
		++data;
	}

	if (data == endp)
		return NULL;

	p1 = data + 1;
	while (p1 < endp) {
		char c = *p1;

		if (' ' == c || '\t' == c || '\r' == c ||
		    '\n' == c || '\0' == c)
			break;
		++p1;
	}

	*tlen = p1 - data;

	return data;
}

static int parse_number(const char *data, size_t len, unsigned int *num,
			size_t *parsed)
{
	const char *endp = data + len;
	const char *p = data;
	unsigned int n;

	if (!len || !isdigit(data[0]))
		return -EINVAL;

	n = 0;
	while (p < endp) {
		int c = *p;

		if (!isdigit(c))
			break;

		if (n >= UINT_MAX)
			return -EINVAL;

		c -= '0';
		n = n * 10 + c;
		++p;
	}

	*num = n;
	*parsed = p - data;
	return 0;
}

static int parse_dest(struct device *dev,
		      struct data_channel *dchan,
		      const char *buf, size_t len)
{
	const char *endp = buf + len;
	int ret = 0;

	if (len > 7 && !memcmp(buf, "sblock:", 7)) {
		u8 dst;
		u8 chan;
		unsigned int n;
		size_t parsed;

		/* sblock */
		buf += 7;
		len -= 7;

		/* <dst> */
		ret = parse_number(buf, len, &n, &parsed);
		if (ret)
			return ret;
		if (n > 255)
			return -EINVAL;
		dst = (u8)n;

		/* <chan> */
		buf += parsed;
		if (endp - buf < 2 || ',' != *buf)
			return -EINVAL;
		++buf;
		len -= (parsed + 1);
		ret = parse_number(buf, len, &n, &parsed);
		if (ret)
			return -EINVAL;
		if (n > 255)
			return -EINVAL;
		chan = (u8)n;

		/* Shall be no more argument */
		buf += parsed;
		len -= parsed;
		if (buf != endp)
			return -EINVAL;

		dchan->type = CT_SBLOCK;
		dchan->u.sblock.id.dst = dst;
		dchan->u.sblock.id.chan = chan;
	} else if (len > 4 && !memcmp(buf, "usb:", 4)) {
		size_t path_len = len - 4;
		char *path = (char *)devm_kmalloc(dev, path_len + 1,
						  GFP_KERNEL);

		if (path) {
			dchan->type = CT_USB;
			dchan->u.usb.path = path;
			memcpy(path, buf + 4, path_len);
			path[path_len] = '\0';
			dchan->u.usb.filp = NULL;
		} else {
			ret = -ENOMEM;
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int parse_source(struct device *dev, struct forwarder *pfw,
			const char *buf, size_t len)
{
	struct data_channel *dchan =
		devm_kzalloc(dev, sizeof(*dchan), GFP_KERNEL);
	int ret;

	if (!dchan)
		return -ENOMEM;

	ret = parse_dest(dev, dchan, buf, len);
	if (!ret) {
		dchan->fw = pfw;
		pfw->src[pfw->src_num] = dchan;
		++pfw->src_num;
	} else {
		devm_kfree(dev, dchan);
	}

	return ret;
}

/*
 *  set_forwarder_param - parse the command line and set to struct forwarder.
 *
 *  Command format:
 *    <dest> <src1> [ <src2> [ ... ]]
 *    <dest>:
 *      sblock:<dst>,<chan>
 *      usb:<dev_path>
 *    <srcn>:
 *      sblock:<dst>,<chan>
 *      klog:(to be defined)
 */
static int set_forwarder_param(struct device *dev, struct forwarder *pfw,
			       const char *buf, size_t count)
{
	const char *endp = buf + count;
	const char *p;
	size_t tlen;
	int i;
	int ret;
	unsigned int data_bit = 1;

	/* <dest> */
	p = get_token(buf, count, &tlen);
	if (!p)
		return -EINVAL;

	ret = parse_dest(dev, &pfw->dest, p, tlen);
	if (ret)
		return ret;

	p += tlen;
	/* <srcn> */
	while (p < endp) {
		size_t rlen = endp - p;

		p = get_token(p, rlen, &tlen);
		if (!p)
			break;

		if (pfw->src_num >= MAX_DATA_SRC_NUM) {
			ret = -EINVAL;
			goto free_src;
		}

		ret = parse_source(dev, pfw, p, tlen);
		if (ret)
			goto free_src;
		pfw->src[pfw->src_num - 1]->data_bit = data_bit;
		/* TODO: support kernel log later. */
		if (pfw->src[pfw->src_num - 1]->type != CT_SBLOCK) {
			ret = -EINVAL;
			goto free_src;
		}
		data_bit <<= 1;
		p += tlen;
	}

	/* No sources defined. */
	if (!pfw->src_num)
		return -EINVAL;

	return 0;

free_src:
	for (i = 0; i < pfw->src_num; ++i) {
		devm_kfree(dev, pfw->src[i]);
		pfw->src[i] = NULL;
	}
	pfw->src_num = 0;

	return ret;
}

static struct forwarder *get_forwarder_by_dest(struct list_head *head,
					       const struct data_channel *dchan)
{
	struct list_head *p = head->next;

	while (p != head) {
		struct forwarder *fw = (struct forwarder *)
			((u8 *)p - offsetof(struct forwarder, node));

		if (dchan->type != fw->dest.type) {
			p = p->next;
			continue;
		}

		if (dchan->type == CT_SBLOCK &&
		    dchan->u.sblock.id.dst == fw->dest.u.sblock.id.dst &&
		    dchan->u.sblock.id.chan == fw->dest.u.sblock.id.chan)
			return fw;

		if (dchan->type == CT_USB &&
		    !strcmp(dchan->u.usb.path, fw->dest.u.usb.path))
			return fw;

		p = p->next;
	}

	return NULL;
}

static int show_chan(char *buf, size_t len, const struct data_channel *dchan)
{
	/* TODO: support kernel log later. */
	int ret;

	if (dchan->type == CT_SBLOCK)
		ret = snprintf(buf, len, "sblock:%u,%u",
			       (unsigned int)dchan->u.sblock.id.dst,
			       (unsigned int)dchan->u.sblock.id.chan);
	else
		ret = snprintf(buf, len, "usb:%s",
			       dchan->u.usb.path);

	if (ret >= len) {
		buf[len - 1] = ' ';
		ret = len;
	}

	return ret;
}

/*
 *  forward_show - show forwarding.
 */
static ssize_t forward_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct log_transporter_device *log_tp = dev_get_drvdata(dev);
	int ret;
	size_t len = PAGE_SIZE;
	ssize_t rlen = 0;
	struct list_head *p;

	mutex_lock(&log_tp->fw_lock);
	p = log_tp->fw_list.next;
	while (p != &log_tp->fw_list) {
		struct forwarder *fw = (struct forwarder *)
			((u8 *)p - offsetof(struct forwarder, node));
		int i;

		ret = show_chan(buf, len, &fw->dest);
		buf += ret;
		rlen += ret;
		len -= ret;
		if (!len)
			break;

		for (i = 0; i < fw->src_num; ++i) {
			/* space */
			*buf = ' ';
			++buf;
			++rlen;
			--len;
			if (!len)
				break;

			ret = show_chan(buf, len, fw->src[i]);
			buf += ret;
			len -= ret;
			rlen += ret;
			if (!len)
				break;
		}

		if (!len)
			break;

		*buf = '\n';
		++buf;
		--len;
		++rlen;
		if (!len)
			break;

		p = p->next;
	}
	mutex_unlock(&log_tp->fw_lock);

	return rlen;
}

/*
 *  forward_store - set forwarding.
 */
static ssize_t forward_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret;
	struct forwarder *pfw = alloc_forwarder(dev);

	if (!pfw)
		return -ENOMEM;

	ret = set_forwarder_param(dev, pfw, buf, count);
	if (!ret) {
		struct log_transporter_device *log_tp =
			dev_get_drvdata(dev);

		mutex_lock(&log_tp->fw_lock);
		if (!get_forwarder_by_dest(&log_tp->fw_list,
					   &pfw->dest))
			list_add_tail(&pfw->node, &log_tp->fw_list);
		else
			ret = -EINVAL;
		mutex_unlock(&log_tp->fw_lock);
		if (!ret) {
			ret = start_forward(pfw);
			if (ret) {
				/* Support USB later. */
				dev_info(dev,
					 "Start log forwarding failed: "
					 "sblock:%u,%u\n",
					 (unsigned int)
						 (pfw->dest.u.sblock.id.dst),
					 (unsigned int)
						 (pfw->dest.u.sblock.id.chan));
				ret = 0;
			}
		} else {
			free_forwarder(dev, pfw);
		}
	} else {
		devm_kfree(dev, pfw);
	}

	return ret;
}

static DEVICE_ATTR_RW(forward);

static int __init tp_dev_init(struct log_transporter_device *tp_dev,
			      struct platform_device *plt_dev)
{
	int ret;

	tp_dev->plt_dev = plt_dev;
	INIT_LIST_HEAD(&tp_dev->fw_list);
	mutex_init(&tp_dev->fw_lock);

	ret = device_create_file(&plt_dev->dev, &dev_attr_forward);
	if (ret) {
		dev_err(&plt_dev->dev,
			"create forward attr error %d\n", ret);
		mutex_destroy(&tp_dev->fw_lock);
		tp_dev->plt_dev = NULL;
	}

	return ret;
}

static void __exit free_fw_list(struct device *dev, struct list_head *head)
{
	struct list_head *p = head->next;

	while (p != head) {
		struct list_head *next = p->next;
		struct forwarder *fw = (struct forwarder *)
			((u8 *)p - offsetof(struct forwarder, node));

		free_forwarder(dev, fw);
		p = next;
	}

	INIT_LIST_HEAD(head);
}

static void __exit tp_dev_destroy(struct log_transporter_device *tp_dev)
{
	device_remove_file(&tp_dev->plt_dev->dev, &dev_attr_forward);

	mutex_destroy(&tp_dev->fw_lock);

	/* Free forwarding list. */
	free_fw_list(&tp_dev->plt_dev->dev, &tp_dev->fw_list);

	tp_dev->plt_dev = NULL;
}

static int __init log_tp_probe(struct platform_device *pdev)
{
	int ret;
	struct log_transporter_device *tp_dev;

	tp_dev = devm_kzalloc(&pdev->dev, sizeof(*tp_dev), GFP_KERNEL);
	if (!tp_dev)
		return -ENOMEM;

	tp_dev->plt_dev = pdev;
	ret = tp_dev_init(tp_dev, pdev);
	if (ret < 0)
		dev_err(&pdev->dev,
			"init log_transporter_device failed %d\n", ret);
	else
		platform_set_drvdata(pdev, tp_dev);

	return ret;
}

static int __exit log_tp_remove(struct platform_device *pdev)
{
	struct log_transporter_device *tp_dev = platform_get_drvdata(pdev);

	tp_dev_destroy(tp_dev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver data_tp_driver = {
	.probe = log_tp_probe,
	.remove = log_tp_remove,
	.driver = {
		.name = "sprd-log-tp"
	}
};

static struct platform_device data_tp_dev = {
	.name = "sprd-log-tp"
};

static int __init data_tp_init(void)
{
	int ret = platform_driver_register(&data_tp_driver);

	if (ret)
		return ret;

	ret = platform_device_register(&data_tp_dev);
	if (ret)
		platform_driver_unregister(&data_tp_driver);

	return ret;
}
module_init(data_tp_init);

static void __exit data_tp_exit(void)
{
	platform_device_unregister(&data_tp_dev);
	platform_driver_unregister(&data_tp_driver);
}
module_exit(data_tp_exit);

MODULE_AUTHOR("Zhang Ziyi");
MODULE_DESCRIPTION("ORCA miniAP data transporter");
MODULE_LICENSE("GPL v2");
