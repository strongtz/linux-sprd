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

#ifndef _LOG_TP_H_
#define _LOG_TP_H_

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sched.h>

#define MAX_DATA_SRC_NUM 5

struct forwarder;

enum channel_type {
	CT_SBLOCK,
	CT_USB,
	CT_KERNEL_LOG
};

enum channel_state {
	CS_NOT_CREATED,
	CS_OPENING,
	CS_OPENED,
	CS_CLOSED
};

struct sblock_channel_id {
	u8 dst;
	u8 chan;
};

struct sblock_channel {
	struct sblock_channel_id id;
	enum channel_state state;
};

struct usb_ser_channel {
	char *path;
	struct file *filp;
};

struct data_channel {
	enum channel_type type;
	union {
		struct sblock_channel sblock;
		struct usb_ser_channel usb;
	} u;
	/* The bit to set when there is incoming data on this channel. */
	unsigned int data_bit;
	struct forwarder *fw;
};

struct forwarder {
	struct list_head node;
	int src_num;
	struct data_channel *src[MAX_DATA_SRC_NUM];
	struct data_channel dest;
	struct mutex set_lock;			/* Lock for modifying setting. */
	struct task_struct *thread;
	struct wait_queue_head wq;
	spinlock_t flag_lock;			/* Incoming data indication. */
	unsigned int data_flags;
};

struct log_transporter_device {
	struct platform_device *plt_dev;
	struct list_head fw_list;
	struct mutex fw_lock;			/* Lock for modifying settings. */
};

#endif /* !_LOG_TP_H_ */
