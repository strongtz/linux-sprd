/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "[Audio:DSPDUMP] "fmt

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "agdsp_access.h"
#include "audio_dsp_ioctl.h"
#include "audio_mem.h"
#include "audio_sblock.h"
#include "audio-sipc.h"
#include "sprd-string.h"

#define  DSP_ASSERT_MEMORY_DUMP_ADDR   0xaa

#define PACKET_HEADER_SIZE    20

#define DSP_MEM_OFFSET    (64*1024)

/*flag for TP log output*/
#define AUDIO_LOG_DISABLE	0
#define AUDIO_LOG_BY_UART	1
#define AUDIO_LOG_BY_ARMCOM	2

enum dump_type_e {
	DSP_LOG = 0,
	DSP_PCM,
	DSP_MEM,
	DSP_ERR,
};

/*  cmd type definition for sblock */
enum{
	SBLOCK_TYPE_NONE = 0,
	SBLOCK_TYPE_DUMP = 0x20,
	SBLOCK_TYPE_SEND_ADDR = 0x21,
	SBLOCK_TYPE_RESPONSE_ADDR = 0x22,
	SBLOCK_TYPE_EVENT = 0x23,
	SBLOCK_TYPE_TPLOG = 0x24,
};

#define SPRD_MAX_NAME_LEN 64

struct dsp_log_init_data {
	char name[SPRD_MAX_NAME_LEN];
	char dump_content[SPRD_MAX_NAME_LEN];
	u32 dump_type;
	u32 usedmem_type;
	u32 usedmem_size;
	u32 dump_mem_addr;
	void *dump_mem_addr_v;
	void *dump1_v;
	u32 dump1_p;
	u32 dump1_bytes;
	void *dump2_v;
	u32 dump2_p;
	u32 dump2_bytes;
	void *dump3_v;
	u32 dump3_p;
	u32 dump3_bytes;
	u8 dst;
	u8 channel;
	u32 txblocknum;
	u32 txblocksize;
	u32 rxblocknum;
	u32 rxblocksize;
	u32 dump_enable;
	u32 timeout_dump;
};

struct dsp_log_device {
	struct dsp_log_init_data *init;
	int major;
	int minor;
	struct cdev cdev;
	wait_queue_head_t mem_ready_wait;
	struct mutex mutex;
	int dsp_assert;
};

struct dsp_log_sbuf {
	u8 dst;
	u16 channel;
	void *sblockhandle;
	int dump_type;
	void *dump_mem_addr_v;
	u32 usedmem_size;
	u8 header_buf[PACKET_HEADER_SIZE];
	int need_packed;
	int sn_number;
	wait_queue_head_t *wait;
	struct dsp_log_device *dev_res;
	struct sblock blk;
	u32 offset;
};

static struct class *audio_dsp_class;

/* add_headers - add SMP header and MSG_HEADER_T.
 * @buf: buffer to fill headers
 * @len: data length after the SMP and MSG_HEADER_T headers
 * @type: the lower 4 bit of subtype.
 */
static void audio_dsp_add_headers(u8 *buf, int len, u8 type,
				  u32 m_sn)
{
	int pkt_len = 0;
	u32 n = 0;
	/* SMP header first */

	/* FLAGS */
	buf[0] = 0x7e;
	buf[1] = 0x7e;
	buf[2] = 0x7e;
	buf[3] = 0x7e;
	/* LEN (length excluding FLAGS in little Endian) */
	pkt_len = len + 8 + 8;
	buf[4] = (u8) pkt_len;
	buf[5] = (u8)(pkt_len >> 8);
	/* CHANNEL */
	buf[6] = 0;
	/* TYPE */
	buf[7] = 0;
	/* RESERVED */
	buf[8] = 0x5a;
	buf[9] = 0x5a;
	/* Checksum (only cover LEN, CHANNEL, TYPE and RESERVED) */
	n = (u32)pkt_len + 0x5a5a;
	n = (n & 0xffff) + (n >> 16);
	n = ~n;
	buf[10] = (u8)(n);
	buf[11] = (u8)(n >> 8);

	/* MSG_HEAD_T */

	/* SN */
	buf[12] = (u8)m_sn;
	buf[13] = (u8)(m_sn >> 8);
	buf[14] = (u8)(m_sn >> 16);
	buf[15] = (u8)(m_sn >> 24);

	/* length */
	pkt_len = len + 8;
	buf[16] = (u8)(pkt_len);
	buf[17] = (u8)(pkt_len >> 8);
	/* type */
	buf[18] = 0x9d;
	/* subtype: AG-DSP */
	buf[19] = (u8)(0x40 | type);
}

static int audio_dsp_mem_poll(struct file *filp, poll_table *wait)
{
	struct dsp_log_sbuf *audio_buf = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, audio_buf->wait, wait);
	mutex_lock(&audio_buf->dev_res->mutex);
	if (audio_buf->dev_res->dsp_assert)
		mask |= POLLIN | POLLRDNORM;
	mutex_unlock(&audio_buf->dev_res->mutex);

	return mask;
}

static int audio_dsp_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	static struct dsp_log_device *dsp_log;
	struct dsp_log_sbuf *audio_buf;

	dsp_log = container_of(inode->i_cdev, struct dsp_log_device, cdev);
	pr_info("%s, minor = %d, dsp_log->minor =%d\n", __func__, minor,
	       dsp_log->minor);

	audio_buf = kzalloc(sizeof(struct dsp_log_sbuf), GFP_KERNEL);
	if (!audio_buf)
		return -ENOMEM;
	audio_buf->dst = dsp_log->init->dst;
	audio_buf->channel = dsp_log->init->channel;
	audio_buf->dump_type = dsp_log->init->dump_type;
	audio_buf->dump_mem_addr_v = dsp_log->init->dump_mem_addr_v;
	audio_buf->usedmem_size = dsp_log->init->usedmem_size;
	audio_buf->sn_number  = 0;
	audio_buf->wait = (wait_queue_head_t *) &dsp_log->mem_ready_wait;
	audio_buf->dev_res = dsp_log;
	filp->private_data   = audio_buf;

	return 0;
}

static int audio_dsp_release(struct inode *inode, struct file *filp)
{
	struct dsp_log_sbuf *audio_buf = filp->private_data;

	kfree(audio_buf);
	pr_info("%s\n", __func__);

	return 0;
}
static unsigned int audio_dsp_poll(struct file *filp, poll_table *wait)
{
	/* struct spipe_sbuf *sbuf = filp->private_data; */
	struct dsp_log_sbuf *audio_buf = filp->private_data;

	if ((audio_buf->dump_type == DSP_LOG) ||
	    (audio_buf->dump_type == DSP_PCM)) {
		return audio_sblock_poll_wait(audio_buf->dst,
					      audio_buf->channel, filp, wait);
	} else if (audio_buf->dump_type == DSP_MEM) {
		return audio_dsp_mem_poll(filp,  wait);
	}

	return 0;
}

static int audio_dsp_mem_dump(struct dsp_log_init_data *init, void *buf,
				  u32 is_timeout)
{
	int bytes = 0;

	pr_info("%s: dump1_p:%x,size:%d, 2:%x, %d,3:%x,%d\n", __func__,
		init->dump1_p, init->dump1_bytes,
		init->dump2_p, init->dump2_bytes,
		init->dump3_p, init->dump3_bytes);

	if (!init->dump_enable)
		return 0;

	if (!is_timeout || init->timeout_dump) {
		if (init->dump1_v && init->dump1_bytes
			&& agdsp_can_access()) {
			unalign_memcpy(buf, init->dump1_v, init->dump1_bytes);
			bytes = init->dump1_bytes;
		} else {
			unalign_memset(buf, 0, init->dump1_bytes);
			bytes = init->dump1_bytes;
		}
	}

	if (init->dump2_v && init->dump2_bytes) {
		unalign_memcpy((char *)buf + bytes, init->dump2_v,
			       init->dump2_bytes);
		bytes += init->dump2_bytes;
	}

	if (init->dump3_v && init->dump3_bytes && agdsp_can_access()) {
		unalign_memcpy((char *)buf + bytes, init->dump3_v,
			       init->dump3_bytes);
		bytes += init->dump3_bytes;
	} else {
		unalign_memset((char *)buf + bytes, 0, init->dump3_bytes);
		bytes += init->dump3_bytes;
	}

	return bytes;
}

static int audio_dsp_audio_info_dump(void *private, u32 is_timeout)
{
	u32 bytes = 0;
	struct dsp_log_init_data *init  = private;

	if (!init)
		return -1;

	bytes  = aud_ipc_dump(init->dump_mem_addr_v, DSP_MEM_OFFSET);
	pr_info("%s: ipc dump bytes : %d", __func__, bytes);

	bytes = audio_dsp_mem_dump(init, (char *) init->dump_mem_addr_v
			+ DSP_MEM_OFFSET, is_timeout);
	pr_info("%s: dsp mem dump bytes : %d", __func__, bytes);

	return bytes;
}

static ssize_t audio_dsp_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	int timeout = -1;
	int ret = 0;
	int rdsize = 0;
	loff_t  pos = *ppos;
	int read_count = count;
	char __user *orig_buf = buf;
	struct dsp_log_sbuf *audio_buf = filp->private_data;

	if (audio_buf->need_packed) {
		if (count <= PACKET_HEADER_SIZE) {
			ret = -EFAULT;
			goto fail;
		}
		read_count = count - PACKET_HEADER_SIZE;
		buf = (u8 *) buf + PACKET_HEADER_SIZE;
	} else {
		read_count = count;
	}

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	if ((audio_buf->dump_type == DSP_MEM) &&  audio_buf->dump_mem_addr_v) {
		if (pos < audio_buf->usedmem_size) {
			rdsize = (audio_buf->usedmem_size - pos) < read_count ?
				(audio_buf->usedmem_size - pos) : read_count;
			if (unalign_copy_to_user(buf, (void *)
						 ((unsigned long)
						  (audio_buf->dump_mem_addr_v) +
						 (unsigned long)pos), rdsize)) {
				pr_err("dsp_log_read: failed to copy to user!\n");
				ret = -EFAULT;
				goto fail;
			} else {
				ret = rdsize;
			}
			*ppos += rdsize;
		} else {
			ret = 0;
		}
	} else {
		if (audio_buf->blk.length == 0) {
			ret = audio_sblock_receive(audio_buf->dst,
						   audio_buf->channel,
						   &audio_buf->blk, timeout);
			if (ret < 0) {
				pr_err("dsp_log_read: failed to receive block!\n");
				goto fail;
			}
			audio_buf->offset = 0;
		}
		rdsize = audio_buf->blk.length > read_count ?
			read_count : audio_buf->blk.length;

		if (unalign_copy_to_user(buf, (void *)(
			(size_t)audio_buf->blk.addr + audio_buf->offset),
			rdsize)) {
			pr_err("dsp_log: sblock_read: failed to copy to user!\n");
			ret = -EFAULT;
			goto fail;
		} else{
			audio_buf->blk.length -= rdsize;
			audio_buf->offset += rdsize;
			ret = rdsize;
		}
		if (audio_buf->blk.length == 0) {
			if (audio_sblock_release(audio_buf->dst,
				audio_buf->channel, &audio_buf->blk) < 0) {
				pr_err("dsp_log: failed to release block!\n");
				goto fail;
			}
		}
	}

	if ((ret > 0)  && (audio_buf->need_packed)) {
		u8 type = 0;

		if (audio_buf->dump_type == DSP_LOG)
			type = 0x1;
		else if (audio_buf->dump_type == DSP_PCM)
			type = 0x1;
		else if (audio_buf->dump_type == DSP_MEM)
			type = 0x1;

		audio_dsp_add_headers(audio_buf->header_buf,
			ret,  type, audio_buf->sn_number);
		audio_buf->sn_number++;
		if (unalign_copy_to_user(orig_buf,
			audio_buf->header_buf,
			PACKET_HEADER_SIZE)) {
			pr_err(" dsp log add packet header failed\n");
			ret = -EFAULT;
			goto fail;
		}
	}

fail:
	return ret;
}

static ssize_t audio_dsp_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{

	struct dsp_log_sbuf *audio_buf = filp->private_data;
	struct sblock blk = {0};
	int timeout = -1;
	int ret = 0;
	int wrsize = 0;
	int pos = 0;
	size_t len = count;

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;
	ret = audio_sblock_get(audio_buf->dst,
		audio_buf->channel, &blk, timeout);
	if (ret < 0) {
		pr_err("%s: sblock_write: failed to get block!\n", __func__);
		return ret;
	}

	wrsize = (blk.length > len ? len : blk.length);
	pr_info("%s: sblock_write: blk_len %d, count %zd, wsize %d\n",
		__func__, blk.length, len, wrsize);
	if (unalign_copy_from_user(blk.addr, buf + pos, wrsize)) {
		pr_err("%s: sblock_write: failed to copy from user!\n",
		       __func__);
		ret = -EFAULT;
	} else{
		blk.length = wrsize;
		len -= wrsize;
		pos += wrsize;
	}

	if (audio_sblock_send(audio_buf->dst,
		audio_buf->channel, &blk)) {
		pr_err("%s: sblock_write: failed to send block!", __func__);
	}

	pr_info("%s: sblock_write len= %zu, ret= %d\n", __func__, len, ret);

	return count - len;
}

static long audio_dsp_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct dsp_log_sbuf *audio_buf = filp->private_data;
	int ret = 0;

	pr_info("%s enter into\n", __func__);
	switch (cmd) {
	case DSPLOG_CMD_LOG_ENABLE:
		pr_info(" dsp log enable %ld, audio_buf->channel: %d\n",
				arg, audio_buf->channel);
		ret = aud_send_cmd_no_wait(audio_buf->channel,
			SBLOCK_TYPE_TPLOG, (u32)arg, 0, 0, 0);
		if (ret < 0) {
			pr_err("dsp_log %s: fail to send AT command to dsp  %d\n",
				__func__, ret);
			return -EIO;
		}
		pr_info("dsp_log : %s send enable successfully\n", __func__);
		break;
	case DSPLOG_CMD_LOG_PATH_SET:
		pr_info(" dsp log path set %ld\n", arg);
		break;
	case DSPLOG_CMD_PCM_PATH_SET:
		pr_info(" dsp pcm path set %ld\n", arg);
		break;
	case DSPLOG_CMD_PCM_ENABLE:
		pr_info(" dsp pcm enable set %ld\n", arg);
		ret = aud_send_cmd_no_wait(audio_buf->channel,
			SBLOCK_TYPE_DUMP, (u32)arg, 0, 0, 0);
		if (ret < 0) {
			pr_err("%s: fail to send AT command to dsp  %d\n",
				__func__, ret);
			return -EIO;
		}
		pr_info("dsp_log : %s send enable successfully\n", __func__);
		break;
	case DSPLOG_CMD_DSPASSERT:
		pr_info(" dsp assert set %ld\n", arg);
		mutex_lock(&audio_buf->dev_res->mutex);
		audio_buf->dev_res->dsp_assert = (int)arg;
		if (audio_buf->dev_res->dsp_assert == true)
			wake_up_interruptible_all(audio_buf->wait);
		mutex_unlock(&audio_buf->dev_res->mutex);
		break;
	case DSPLOG_CMD_LOG_PACKET_ENABLE:
		audio_buf->need_packed = (int)arg;
		break;
	case DSPLOG_CMD_DSPDUMP_ENABLE:
		audio_buf->dev_res->init->dump_enable = (u32)arg;
		break;
	case DSPLOG_CMD_TIMEOUTDUMP_ENABLE:
		audio_buf->dev_res->init->timeout_dump = (u32)arg;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static inline void audio_dsp_destroy_pdata(struct dsp_log_init_data **init)
{
	struct dsp_log_init_data *pdata = *init;

	audio_sblock_destroy(pdata->dst, pdata->channel);
	kfree(pdata);
	*init = NULL;
}

static const struct file_operations dsp_log_tp_fops = {
	.open		= audio_dsp_open,
	.release		= audio_dsp_release,
	.read			= audio_dsp_read,
	.write		= audio_dsp_write,
	.poll			= audio_dsp_poll,
	.unlocked_ioctl	= audio_dsp_ioctl,
	.compat_ioctl = audio_dsp_ioctl,
	.owner		= THIS_MODULE,
};

static int audio_dsp_parse_mem_dt(struct device_node *np,
				  struct dsp_log_init_data *pdata)
{
	int ret = 0;
	u32 val_arr[6] = {0};

	ret = of_property_read_u32(np, "sprd-usemem-bytes",
				   &pdata->usedmem_size);
	if (ret) {
		pr_info("%s no sprd-usemem-type\n", __func__);
		return ret;
	}
	pr_info("sprd-usemem-bytes %d\n", pdata->usedmem_size);

	if (of_property_read_u32_array(np, "sprd,dspdumpmem", &val_arr[0], 6))
		return 0;

	pdata->dump1_p = val_arr[0];
	pdata->dump1_bytes = val_arr[1];
	if (pdata->dump1_bytes) {
		pdata->dump1_v = audio_mem_vmap(pdata->dump1_p,
						pdata->dump1_bytes, 1);
	}
	pdata->dump2_p = val_arr[2];
	pdata->dump2_bytes = val_arr[3];
	if (pdata->dump2_bytes) {
		pdata->dump2_v = audio_mem_vmap(pdata->dump2_p,
						pdata->dump2_bytes, 1);
	}
	pdata->dump3_p = val_arr[4];
	pdata->dump3_bytes = val_arr[5];
	if (pdata->dump3_bytes) {
		pdata->dump3_v = audio_mem_vmap(pdata->dump3_p,
						pdata->dump3_bytes, 1);
	}
	pr_info("audio_dsp_log.c: dsp_mem_dump: dump1_p:%x,size:%d, 2:%x, %d,3:%x,%d\n",
		pdata->dump1_p, pdata->dump1_bytes,
		pdata->dump2_p, pdata->dump2_bytes,
		pdata->dump3_p, pdata->dump3_bytes);

	return ret;
}

static const struct of_device_id audio_dsp_table[] = {
	{.compatible = "sprd,audio_dsp_log",
		.data = (void *)DSP_LOG},
	{.compatible = "sprd,audio_dsp_pcm",
		.data = (void *)DSP_PCM},
	{.compatible = "sprd,audio_dsp_mem",
		.data = (void *)DSP_MEM},
	{ },
};

static int audio_dsp_parse_dt(
	struct dsp_log_init_data *init,
	struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct dsp_log_init_data *pdata = init;
	int ret;
	const struct of_device_id *of_id;
	long type;

	if (!np) {
		ret = -EPROBE_DEFER;
		pr_err("%s defer\n", __func__);
		goto error;
	}
	of_id = of_match_node(audio_dsp_table, np);
	if (!of_id) {
		pr_err("%s Get device id failed deffer!\n", __func__);
		ret = -EPROBE_DEFER;
		goto error;
	}
	type = (long)of_id->data;
	switch (type) {
	case DSP_LOG:
		strncpy(pdata->name, "audio_dsp_log", SPRD_MAX_NAME_LEN);
		strncpy(pdata->dump_content, "log", SPRD_MAX_NAME_LEN);
		break;
	case DSP_PCM:
		strncpy(pdata->name, "audio_dsp_pcm", SPRD_MAX_NAME_LEN);
		strncpy(pdata->dump_content, "pcm", SPRD_MAX_NAME_LEN);
		break;
	case DSP_MEM:
		strncpy(pdata->name, "audio_dsp_mem", SPRD_MAX_NAME_LEN);
		strncpy(pdata->dump_content, "memory", SPRD_MAX_NAME_LEN);
		break;
	default:
		ret = -EINVAL;
		pr_err("%s not supported type %ld\n", __func__, type);
		goto error;
	}
	ret = of_property_read_u32(np, "sprd-usemem-type",
		&pdata->usedmem_type);
	if (ret) {
		pr_info("%s no sprd-usemem-type\n", __func__);
		goto error;
	}
	ret = of_property_read_u8(np, "sprd-dst", &pdata->dst);
	if (ret) {
		pr_info("%s no sprd-dst\n", __func__);
		goto error;
	}
	ret = of_property_read_u8(np, "sprd-channel", &pdata->channel);
	if (ret) {
		pr_info("%s no sprd-channel\n", __func__);
		goto error;
	}

	if (!strcmp(pdata->dump_content, "memory"))
		pdata->dump_type = DSP_MEM;
	else if (!strcmp(pdata->dump_content, "log"))
		pdata->dump_type = DSP_LOG;
	else if (!strcmp(pdata->dump_content, "pcm"))
		pdata->dump_type = DSP_PCM;
	else
		pdata->dump_type = DSP_ERR;

	if (pdata->dump_type == DSP_MEM) {
		ret = audio_dsp_parse_mem_dt(np, pdata);
		if (ret)
			goto error;
	} else {
		ret = of_property_read_u32(np, "sprd-txblocknum",
					   &pdata->txblocknum);
		if (ret)
			pr_info("%s, no sprd-txblocknum\n", __func__);
		ret = of_property_read_u32(np, "sprd-txblocksize",
					   &pdata->txblocksize);
		if (ret)
			pr_info("%s, no sprd-txblocknum\n", __func__);
		ret = of_property_read_u32(np, "sprd-rxblocknum",
					   &pdata->rxblocknum);
		if (ret)
			goto error;
		ret = of_property_read_u32(np, "sprd-rxblocksize",
					   &pdata->rxblocksize);
		if (ret)
			goto error;
	}

	return 0;
error:
	pr_err("%s failed", __func__);

	return ret;
}

static int audio_dsp_probe(struct platform_device *pdev)
{
	struct dsp_log_init_data *init = pdev->dev.platform_data;
	struct dsp_log_device *dsp_log;
	dev_t devid;
	int rval;
	u32  mem_addr = 0;

	/* struct task_struct	*thread; */
	pr_info("%s in\n", __func__);
	/* parse dt
	 * confirm txbuffer, rxbuffer,their controls pointers, and target id
	 */
	if (!pdev->dev.of_node) {
		pr_err("%s defer\n", __func__);
		return -EPROBE_DEFER;
	}
	init = kzalloc(sizeof(struct dsp_log_init_data), GFP_KERNEL);
	if (!init)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		rval = audio_dsp_parse_dt(init, &pdev->dev);
		if (rval) {
			kfree(init);
			pr_err("Failed to parse dsp_log device tree, ret=%d\n",
			rval);
			return rval;
		}
	}
	pr_info("%s:name=%s, dst=%hhu, channel=%hhu\n",
		__func__, init->name, init->dst, init->channel);
	pr_info("txblocknum=%u,txblocksize=%u rxblocknum=%u,rxblocksize=%u\n",
		init->txblocknum, init->txblocksize,
		init->rxblocknum, init->rxblocksize);

	if (init->txblocknum || init->rxblocknum) {
		pr_info("begin to creat sblock\n");
		rval = audio_sblock_init(init->dst, init->channel,
			init->txblocknum, init->txblocksize,
			init->rxblocknum, init->rxblocksize,
			init->usedmem_type);
		if (rval != 0) {
			pr_err("Failed to create sblock: %d\n", rval);
			kfree(init);
			init = NULL;
			return rval;
		}
	}

	if (init->dump_type == DSP_MEM) {
		mem_addr = audio_mem_alloc(
			init->usedmem_type,
			&init->usedmem_size);
		pr_info("aud_dsp_addr: mem: mem type %d, mem_addr %x\n",
			init->usedmem_type, mem_addr);
		if ((mem_addr == 0) || (init->usedmem_size == 0)) {
			kfree(init);
			pr_err("Failed to create sblock with addr: %x\n",
				mem_addr);
			return -ENOMEM;
		}
		/* mem_addr = audio_iram_addr_ap2dsp(mem_addr); */
		init->dump_mem_addr = mem_addr;
		pr_info("aud_dsp_addr: mem: dsp space mem_addr %x\n", mem_addr);
		/* thread = kthread_create(dsp_mem_thread, init, */
		/* "sblock-%d-%d-", init->dst,init->channel); */
		/* wake_up_process(thread); */

		if (init->usedmem_size) {
			init->dump_mem_addr_v =
				(void *)audio_mem_vmap(
					(u32)init->dump_mem_addr,
					init->usedmem_size, 1);
			memset_io(init->dump_mem_addr_v, 0, init->usedmem_size);
		}
		aud_smsg_register_dump_func(audio_dsp_audio_info_dump,
					  (void *)init);
	}

	dsp_log = kzalloc(sizeof(struct dsp_log_device), GFP_KERNEL);
	if (dsp_log == NULL) {
		audio_dsp_destroy_pdata(&init);
		pr_err("Failed to allocate audio_pipe_device\n");
		return -ENOMEM;
	}

	rval = alloc_chrdev_region(&devid, 0, 1, init->name);
	if (rval != 0) {
		kfree(dsp_log);
		audio_dsp_destroy_pdata(&init);
		pr_err("Failed to alloc dsp_log chrdev\n");
		return rval;
	}
	cdev_init(&(dsp_log->cdev), &dsp_log_tp_fops);
	rval = cdev_add(&(dsp_log->cdev), devid, 1);
	if (rval != 0) {
		kfree(dsp_log);
		unregister_chrdev_region(devid, 1);
		audio_dsp_destroy_pdata(&init);
		pr_err("Failed to add audio_pipe cdev\n");
		return rval;
	}
	dsp_log->major = MAJOR(devid);
	dsp_log->minor = MINOR(devid);

	device_create(audio_dsp_class, NULL,
			MKDEV(dsp_log->major, dsp_log->minor),
			NULL, "%s", init->name);
	dsp_log->init = init;
	mutex_init(&dsp_log->mutex);
	init_waitqueue_head(&dsp_log->mem_ready_wait);
	platform_set_drvdata(pdev, dsp_log);
	pr_info("dsp_log_tp_probe success  %s\n", init->name);

	return 0;
}

static int  audio_dsp_remove(struct platform_device *pdev)
{

	struct dsp_log_device *dsp_log = platform_get_drvdata(pdev);

	cdev_del(&(dsp_log->cdev));
	unregister_chrdev_region(
		MKDEV(dsp_log->major, dsp_log->minor), 1);
	audio_dsp_destroy_pdata(&dsp_log->init);
	kfree(dsp_log);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver audio_dsp_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_audio_dsp_dump",
		.of_match_table = audio_dsp_table,
	},
	.probe = audio_dsp_probe,
	.remove = audio_dsp_remove,
};

static int __init audio_dsp_init(void)
{
	audio_dsp_class = class_create(THIS_MODULE, "sprd_audio_dsp_dump");
	if (IS_ERR(audio_dsp_class))
		return PTR_ERR(audio_dsp_class);

	return platform_driver_register(&audio_dsp_driver);
}

static void __exit audio_dsp_exit(void)
{
	class_destroy(audio_dsp_class);
	platform_driver_unregister(&audio_dsp_driver);
}

/* module_init(audio_sblock_init); */
module_init(audio_dsp_init);

module_exit(audio_dsp_exit);

MODULE_AUTHOR("SPRD");
MODULE_DESCRIPTION("SIPC/AUDIO_PIPE driver");
MODULE_LICENSE("GPL");
