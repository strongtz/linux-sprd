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

#ifndef __SIPC_PRIV_H
#define __SIPC_PRIV_H
#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#ifdef CONFIG_SPRD_MAILBOX
#include <linux/sprd_mailbox.h>
#endif

struct sipc_child_node_info {
	u8 dst;
	u8 is_new;
	u8 mode;

#ifdef CONFIG_SPRD_MAILBOX
	u8 core_id;
	u8 core_sensor_id;
#else
	u32 ap2cp_int_ctrl;
	u32 cp2ap_int_ctrl;
	u32 ap2cp_bit_trig;
	u32 ap2cp_bit_clr;

	u32 irq;
#endif

	char *name;

	u32 ring_base;
	u32 ring_size;

	u32 smem_base;
	u32 smem_size;
	void *smem_vbase;
};

struct smem_item {
	u32	base;
	u32	size;
	u32	mapped_base;
};

struct sipc_init_data {
	int is_alloc;
	u32 chd_nr;
	u32 newchd_nr;

	u32 smem_cnt;
	struct smem_item *smem_ptr;

	u32 smem_base;
	u32 smem_size;
	u32 mapped_smem_base;
	struct sipc_child_node_info info_table[0];
};

struct sipc_device {
	int status;
	u32 inst_nr;
	struct sipc_init_data *pdata;
	struct smsg_ipc *smsg_inst;
};

struct sipc_core {
	u32 sipc_tag_ids;
	struct sipc_child_node_info *sipc_tags[SIPC_ID_NR];
	struct sipc_device *sipc_dev[SIPC_ID_NR];
};

extern struct sipc_core sipc_ap;
extern struct smsg_ipc *smsg_ipcs[];

#define SMSG_CACHE_NR		256

struct smsg_channel {
	/* wait queue for recv-buffer */
	wait_queue_head_t	rxwait;
	struct mutex		rxlock;
	struct wakeup_source	sipc_wake_lock;
	char				wake_lock_name[16];

	/* cached msgs for recv */
	uintptr_t		wrptr[1];
	uintptr_t		rdptr[1];
	struct smsg		caches[SMSG_CACHE_NR];
};

/* smsg ring-buffer between AP/CP ipc */
struct smsg_ipc {
	char			*name;
	u8			dst;
	u8			id; /* add id */

#ifdef	CONFIG_SPRD_MAILBOX
	/* target core_id over mailbox */
	u8			core_id;
	u8		   core_sensor_id;
#endif

	/* send-buffer info */
	uintptr_t		txbuf_addr;
	u32		txbuf_size;	/* must be 2^n */
	uintptr_t		txbuf_rdptr;
	uintptr_t		txbuf_wrptr;

	/* recv-buffer info */
	uintptr_t		rxbuf_addr;
	u32		rxbuf_size;	/* must be 2^n */
	uintptr_t		rxbuf_rdptr;
	uintptr_t		rxbuf_wrptr;

	/* sipc irq related */
	int			irq;
#ifdef CONFIG_SPRD_MAILBOX
	MBOX_FUNCALL		irq_handler;
#else
	irq_handler_t		irq_handler;
#endif
	irq_handler_t		irq_threadfn;

	u32		(*rxirq_status)(u8 id);
	void			(*rxirq_clear)(u8 id);

#ifdef CONFIG_SPRD_MAILBOX
	void			(*txirq_trigger)(u8 id, u64 msg);
#else
	void			(*txirq_trigger)(u8 id);
#endif

	/* sipc ctrl thread */
	struct task_struct	*thread;

	/* lock for send-buffer */
	spinlock_t		txpinlock;

	/* all fixed channels receivers */
	struct smsg_channel	*channels[SMSG_VALID_CH_NR];

	/* record the runtime status of smsg channel */
	atomic_t		busy[SMSG_VALID_CH_NR];

	/* all channel states: 0 unused, 1 be opened by other core, 2 opend */
	u8			states[SMSG_VALID_CH_NR];
};

#define CHAN_STATE_UNUSED	0
#define CHAN_STATE_WAITING	1
#define CHAN_STATE_OPENED	2
#define CHAN_STATE_FREE		3

/* create/destroy smsg ipc between AP/CP */
int smsg_ipc_create(u8 dst, struct smsg_ipc *ipc);
int smsg_ipc_destroy(u8 dst);
int  smsg_suspend_init(void);

/*smem alloc size align*/
#define SMEM_ALIGN_POOLSZ 0x40000		/*256KB*/

#ifdef CONFIG_64BIT
#define SMEM_ALIGN_BYTES 8
#define SMEM_MIN_ORDER 3
#else
#define SMEM_ALIGN_BYTES 4
#define SMEM_MIN_ORDER 2
#endif

/* initialize smem pool for AP/CP */
int smem_set_default_pool(u32 addr);
int smem_init(u32 addr, u32 size, u32 dst);
void sbuf_get_status(u8 dst, char *status_info, int size);

#ifdef CONFIG_SPRD_MAILBOX
#define RECV_MBOX_SENSOR_ID  8
#endif

#endif
