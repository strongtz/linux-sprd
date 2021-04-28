/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sipc.h>
#include <uapi/linux/sched/types.h>

#include "log-fw.h"
#include "log-tp.h"

struct dest_sblock_stream {
	struct sblock block;
	/* Data length in the block */
	size_t data_len;
};

static void sblock_notifier(int evt, void *client)
{
	struct data_channel *dchan = (struct data_channel *)client;
	struct forwarder *fw = dchan->fw;

	spin_lock(&fw->flag_lock);
	fw->data_flags |= dchan->data_bit;
	spin_unlock(&fw->flag_lock);
	/* Wake up the forwarding thread. */
	wake_up(&fw->wq);
}

static unsigned int get_data_flags(struct forwarder *fw)
{
	unsigned int data_flags;

	spin_lock(&fw->flag_lock);
	data_flags = fw->data_flags;
	fw->data_flags = 0;
	spin_unlock(&fw->flag_lock);

	return data_flags;
}

static struct file *open_file(const char *path, int flags, umode_t mode)
{
	struct file *filp;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, mode);
	set_fs(old_fs);
	if (IS_ERR(filp))
		filp = NULL;

	return filp;
}

/*
 *  prepare_dest_channel - prepare for the destination channel.
 *
 *  Return Value:
 *    0: successful.
 *    -1: the forwarder is asked to quit.
 */
static int prepare_dest_channel(struct forwarder *fw)
{
	if (fw->dest.type == CT_USB) {
		struct file *filp = NULL;

		for (;;) {
			filp = open_file(fw->dest.u.usb.path,
					 O_WRONLY, 0);
			if (filp)
				break;
			msleep_interruptible(400);
			/* TODO: check signal. */
		}

		fw->dest.u.usb.filp = filp;
	}

	return 0;
}

static void clean_up_dest_channel(struct forwarder *fw)
{
	struct usb_ser_channel *usb = &fw->dest.u.usb;

	if (fw->dest.type == CT_USB && usb->filp) {
		filp_close(usb->filp, NULL);
		usb->filp = NULL;
	}
}

static void forward_block(struct sblock_channel *dest_sblk,
			  struct dest_sblock_stream *stream,
			  const struct sblock *data)
{
	int ret;
	const u8 *p = (const u8 *)(data->addr);
	size_t len = data->length;

	while (len) {
		u8 *pdest;
		size_t free_len;
		size_t cp_len;

		if (!stream->block.addr) {
			ret = sblock_get(dest_sblk->id.dst, dest_sblk->id.chan,
					 &stream->block, -1);
			if (ret)
				return;
			stream->data_len = 0;
		}

		/* Copy to the destination sblock */
		pdest = (u8 *)(stream->block.addr) + stream->data_len;
		free_len = stream->block.length - stream->data_len;
		cp_len = free_len >= len ? len : free_len;
		memcpy(pdest, p, cp_len);
		stream->data_len += cp_len;
		if (len >= free_len) {  /* The destination block is full. */
			sblock_send(dest_sblk->id.dst, dest_sblk->id.chan,
				    &stream->block);
			stream->block.addr = NULL;
		}

		len -= cp_len;
		p += cp_len;
	}
}

static int process_sblock_output(struct sblock_channel *dest_sblk,
				 struct data_channel **src,
				 int src_num)
{
	int run = 1;
	struct sblock data;
	struct dest_sblock_stream dst_stream;

	dst_stream.block.addr = NULL;
	dst_stream.block.length = 0;
	dst_stream.data_len = 0;

	while (run) {
		int has_data = 0;
		int i;

		for (i = 0; i < src_num; ++i) {
			struct sblock_channel *src_sblk = &src[i]->u.sblock;
			int ret;

			ret = sblock_receive(src_sblk->id.dst,
					     src_sblk->id.chan,
					     &data, 0);
			if (!ret) {  /* A block got */
				++has_data;
				forward_block(dest_sblk, &dst_stream, &data);
			}
		}

		if (!has_data) {
			if (dst_stream.data_len) {  /* Flush data */
				dst_stream.block.length = dst_stream.data_len;
				sblock_send(dest_sblk->id.dst,
					    dest_sblk->id.chan,
					    &dst_stream.block);
				dst_stream.block.addr = NULL;
			}
			run = 0;
		}
	}

	return 0;
}

static ssize_t write_file(struct file *filp, const struct sblock *data)
{
	const u8 *p = (const u8 *)(data->addr);
	size_t len = data->length;
	loff_t noff = 0;
	ssize_t ret;

	while (len) {
		ret = kernel_write(filp, p, len, &noff);
		if (ret < 0)
			break;
		p += ret;
		len -= ret;
	}

	return ret;
}

static int process_usb_output(struct usb_ser_channel *usb,
			      struct data_channel **src,
			      int src_num)
{
	int run = 1;
	struct sblock data;

	while (run) {
		int has_data = 0;
		int i;

		for (i = 0; i < src_num; ++i) {
			struct sblock_channel *src_sblk = &src[i]->u.sblock;
			int ret;

			ret = sblock_receive(src_sblk->id.dst,
					     src_sblk->id.chan,
					     &data, 0);
			if (!ret) {  /* A block got */
				++has_data;
				write_file(usb->filp, &data);
			}
		}

		if (!has_data)
			run = 0;
	}

	return 0;
}

/*
 *  process_channels - process data from input channels.
 *
 *  If none of the channel has incoming data in one iteration,
 *  return.
 */
static int process_channels(struct data_channel *dest,
			    struct data_channel **src,
			    int src_num)
{
	int ret;

	if (dest->type == CT_SBLOCK) {
		struct sblock_channel *dest_sblk = &dest->u.sblock;

		ret = process_sblock_output(dest_sblk, src, src_num);
	} else {
		struct usb_ser_channel *usb = &dest->u.usb;

		ret = process_usb_output(usb, src, src_num);
	}

	return ret;
}

static int log_fw_thread(void *arg)
{
	struct forwarder *fw = (struct forwarder *)arg;
	struct data_channel **src = fw->src;
	int i;
	unsigned int data_flags = 1;

	/* Register data notification callback to the source channels. */
	for (i = 0; i < fw->src_num; ++i) {
		sblock_register_notifier(src[i]->u.sblock.id.dst,
					 src[i]->u.sblock.id.chan,
					 sblock_notifier, fw->src + i);
	}

	if (prepare_dest_channel(fw))
		goto clean_up;

	/* The main loop: wait for data indication. */
	for (;;) {
		if (data_flags)
			process_channels(&fw->dest, src, fw->src_num);
		wait_event_interruptible(fw->wq, get_data_flags(fw));
	}

	clean_up_dest_channel(fw);

clean_up:
	for (i = 0; i < fw->src_num; ++i) {
		sblock_register_notifier(src[i]->u.sblock.id.dst,
					 src[i]->u.sblock.id.chan,
					 NULL, NULL);
	}
	return 0;
}

int start_forward(struct forwarder *fw)
{
	struct sched_param param = { .sched_priority = 11 };

	/* Create the thread. */
	fw->thread = kthread_create(log_fw_thread, fw,
				    "sblock:%u,%u",
				    (unsigned int)fw->dest.u.sblock.id.dst,
				    (unsigned int)fw->dest.u.sblock.id.chan);
	if (IS_ERR(fw->thread)) {
		struct task_struct *task = fw->thread;

		fw->thread = NULL;
		return PTR_ERR(task);
	}

	get_task_struct(fw->thread);

	/* Start the thread. */
	sched_setscheduler(fw->thread, SCHED_RR, &param);
	wake_up_process(fw->thread);

	return 0;
}
