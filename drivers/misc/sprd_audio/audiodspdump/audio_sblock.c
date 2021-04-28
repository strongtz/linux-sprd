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

#define pr_fmt(fmt) "[Audio:SBLCK] "fmt

/* it is used to receive the block from dsp side.
 * if you want to send to dsp side, you need to work on the code
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <uapi/linux/sched/types.h>


#include "audio_mem.h"
#include "audio_sblock.h"
#include "audio-sipc.h"
#include "audio-smsg.h"

#define SBLOCKSZ_ALIGN(blksz, size) (((blksz)+((size)-1))&(~((size)-1)))

/* flag for CMD/DONE msg type */
#define SMSG_CMD_SBLOCK_INIT		0x0001
#define SMSG_DONE_SBLOCK_INIT		0x0002

/* flag for EVENT msg type */
#define SMSG_EVENT_SBLOCK_SEND		0x0001
#define SMSG_EVENT_SBLOCK_RELEASE	0x0002

#define SBLOCK_STATE_IDLE		0
#define SBLOCK_STATE_READY		1

#define SBLOCK_BLK_STATE_DONE		0
#define SBLOCK_BLK_STATE_PENDING	1

#define	SBLOCK_NOTIFY_GET	0x01
#define	SBLOCK_NOTIFY_RECV	0x02
#define	SBLOCK_NOTIFY_STATUS	0x04
#define	SBLOCK_NOTIFY_OPEN	0x08
#define	SBLOCK_NOTIFY_CLOSE	0x10

/*flag for TP log output*/
#define AUDIO_LOG_DISABLE	0
#define AUDIO_LOG_BY_UART		1
#define AUDIO_LOG_BY_ARMCOM		2

#ifdef CONFIG_64BIT
#define SBLOCK_ALIGN_BYTES (8)
#else
#define SBLOCK_ALIGN_BYTES (4)
#endif

/* smsg type definition */
enum {
	SMSG_TYPE_NONE = 0,
	SMSG_TYPE_OPEN,		/* first msg to open a channel */
	SMSG_TYPE_CLOSE,	/* last msg to close a channel */
	SMSG_TYPE_DATA,		/* data, value=addr, no ack */
	SMSG_TYPE_EVENT,	/* event with value, no ack */
	SMSG_TYPE_CMD,		/* command, value=cmd */
	SMSG_TYPE_DONE,		/* return of command */
	SMSG_TYPE_SMEM_ALLOC,	/* allocate smem, flag=order */
	SMSG_TYPE_SMEM_FREE,	/* free smem, flag=order, value=addr */
	SMSG_TYPE_SMEM_DONE,	/* return of alloc/free smem */
	SMSG_TYPE_FUNC_CALL,	/* RPC func, value=addr */
	SMSG_TYPE_FUNC_RETURN,	/* return of RPC func */
	SMSG_TYPE_DIE,
	SMSG_TYPE_DFS,
	SMSG_TYPE_DFS_RSP,
	SMSG_TYPE_ASS_TRG,
	SMSG_TYPE_NR,		/* total type number */
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

struct sblock_blks {
	u32		addr; /*phy address*/
	u32		length;
};

/* ring block header */
struct sblock_ring_header {
	/* get|send-block info */
	u32		txblk_addr;
	u32		txblk_count;
	u32		txblk_size;
	u32		txblk_blks;
	u32		txblk_rdptr;
	u32		txblk_wrptr;

	/* release|recv-block info */
	u32		rxblk_addr;
	u32		rxblk_count;
	u32		rxblk_size;
	u32		rxblk_blks;
	u32		rxblk_rdptr;
	u32		rxblk_wrptr;
};

struct sblock_header {
	struct sblock_ring_header ring;
	struct sblock_ring_header pool;
};

struct sblock_ring {
	struct sblock_header	*header;
	void			*txblk_virt; /* virt of header->txblk_addr */
	void			*rxblk_virt; /* virt of header->rxblk_addr */
	u32		txblkmemsz; /* total size of tx block memory size */
	u32		rxblkmemsz; /* total size of rx block memory size */
	/* virt of header->ring->txblk_blks */
	struct sblock_blks	*r_txblks;
	/* virt of header->ring->rxblk_blks */
	struct sblock_blks	*r_rxblks;
	/* virt of header->pool->txblk_blks */
	struct sblock_blks	*p_txblks;
	/* virt of header->pool->rxblk_blks */
	struct sblock_blks	*p_rxblks;

	int			*txrecord; /* record the state of every txblk */
	int			*rxrecord; /* record the state of every rxblk */
	int			yell; /* need to notify cp */
	spinlock_t		r_txlock; /* send */
	spinlock_t		r_rxlock; /* recv */
	spinlock_t		p_txlock; /* get */
	spinlock_t		p_rxlock; /* release */

	wait_queue_head_t	getwait;
	wait_queue_head_t	recvwait;
};

struct sblock_mgr {
	uint8_t			dst;
	uint8_t			channel;
	u32		state;

	void			*smem_virt;
	u32		smem_dsp_addr;
	u32		smem_addr;
	u32		smem_size;

	u32		txblksz;
	u32		rxblksz;
	u32		txblknum;
	u32		rxblknum;

	struct sblock_ring	*ring;
	struct task_struct	*thread;

	void			(*handler)(int event, void *data);
	void			*data;
	u32		txblks_err; /* the abnormal tx blcoks for debug */
	u32		rxblks_err; /* the abnormal rx blcoks for debug*/
};

static struct sblock_mgr *sblocks[AUD_IPC_NR][AMSG_CH_NR];

static inline u32 sblock_get_index(u32 x, u32 y)
{
	return (x / y);
}

static inline u32 sblock_get_ringpos(u32 x, u32 y)
{
	return is_power_of_2(y) ? (x & (y - 1)) : (x % y);
}

int audio_sblock_poll_wait(uint8_t dst, uint8_t channel,
		struct file *filp, poll_table *wait)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];
	struct sblock_ring *ring = NULL;

	volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	unsigned int mask = 0;

	if (!sblock)
		return -ENODEV;
	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);
	/* ringhd = ring->header; */
	if (sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready to poll !\n", dst, channel);
		return -ENODEV;
	}

	poll_wait(filp, &ring->getwait, wait);
	poll_wait(filp, &ring->recvwait, wait);

	if (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int audio_sblock_handle_event(struct sblock_mgr *sblock,
				     struct aud_smsg *mrecv)
{
	struct sblock_ring *ring = NULL;

	volatile struct sblock_ring_header *ringhd;

	switch (mrecv->parameter0) {
	case SMSG_EVENT_SBLOCK_SEND:

		ring = sblock->ring;
		ringhd = (volatile struct sblock_ring_header *)
			(&ring->header->ring);
		if (ringhd->rxblk_wrptr == ringhd->rxblk_rdptr) {
			pr_info("p_tt: %d,%d\n", ringhd->rxblk_wrptr,
				ringhd->rxblk_rdptr);
			return 0;
		}

		wake_up_interruptible_all(&sblock->ring->recvwait);
		if (sblock->handler)
			sblock->handler(SBLOCK_NOTIFY_RECV, sblock->data);
		break;
	case SMSG_EVENT_SBLOCK_RELEASE:
		wake_up_interruptible_all(&(sblock->ring->getwait));
		if (sblock->handler)
			sblock->handler(SBLOCK_NOTIFY_GET, sblock->data);
		break;
	default:
		return 1;
	}

	return 0;
}

static int audio_sblock_thread(void *data)
{
	struct sblock_mgr *sblock = data;
	struct aud_smsg mrecv;
	int rval = 0;
	int ret = 0;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	pr_info("%s: send smem_addr  to dsp,  sblock->channel=%d\n",
		__func__, sblock->channel);
	ret = aud_send_cmd_no_param(sblock->channel,
		SBLOCK_TYPE_SEND_ADDR,
		SMSG_CMD_SBLOCK_INIT,
		sblock->smem_dsp_addr, sblock->smem_size, 0, -1);
	pr_info("aud_send_cmd_noparam returned.\n");
	if (ret < 0) {
		pr_err("%s: fail to send SMSG_CMD_SBLOCK_INIT to dsp  %d\n",
			__func__, ret);
		return -EIO;
	}
	sblock->state = SBLOCK_STATE_READY;

	pr_info("%s listen to smsg from dsp\n", __func__);
	/* handle the sblock events */
	while (!kthread_should_stop()) {
		aud_smsg_set(&mrecv, sblock->channel, 0, 0, 0, 0,  0);
		rval = aud_smsg_recv(sblock->dst, &mrecv, -1);
		if (rval == -EIO || rval == -ENODEV) {
			/* channel state is FREE */
			msleep(100);
			continue;
		} else if (rval == -EPIPE) {
			msleep(1000);
			continue;
		}

		switch (mrecv.command) {
		case SBLOCK_TYPE_EVENT:
			/* handle sblock send/release events */
			rval = audio_sblock_handle_event(sblock, &mrecv);
			break;
		default:
			rval = 1;
			break;
		};

		if (rval)
			rval = 0;
	}

	pr_info("%d-%d thread stop", sblock->dst, sblock->channel);

	return rval;
}

static int audio_sblock_ch_open(uint8_t dst,
	uint16_t channel)
{
	int ret = 0;

	if (dst >= AUD_IPC_NR) {
		pr_err("%s, invalid dst:%d\n", __func__, dst);
		return -EINVAL;
	}
	ret = aud_smsg_ch_open(dst, channel);
	if (ret != 0) {
		pr_err("%s, Failed to open channel\n", __func__);
		return ret;
	}
	pr_info("%s aud_smsg_ch_opened dst %d,channel=%d\n",
		__func__, dst, channel);

	return 0;
}

void audio_sblock_put(uint8_t dst, uint8_t channel,
	struct sblock *blk)
{
	struct sblock_mgr *sblock =
		(struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	unsigned long flags;
	int txpos;
	int index;

	if (!sblock)
		return;

	ring = sblock->ring;
	poolhd = (volatile struct sblock_ring_header *)
		(&ring->header->pool);

	spin_lock_irqsave(&ring->p_txlock, flags);
	txpos = sblock_get_ringpos(poolhd->txblk_rdptr - 1,
		poolhd->txblk_count);
	ring->r_txblks[txpos].addr =
		blk->addr - sblock->smem_virt + sblock->smem_addr;
	ring->r_txblks[txpos].length = poolhd->txblk_size;
	poolhd->txblk_rdptr = poolhd->txblk_rdptr - 1;
	if ((int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr) == 1)
		wake_up_interruptible_all(&(ring->getwait));
	index = sblock_get_index((blk->addr - ring->txblk_virt),
		sblock->txblksz);
	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_txlock, flags);
}

int audio_sblock_create(uint8_t dst, uint8_t channel,
		u32 txblocknum, u32 txblocksize,
		u32 rxblocknum, u32 rxblocksize, int mem_type)
{
	struct sblock_mgr *sblock = NULL;
	volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	u32 hsize;
	int i;
	int result;

	sblock = kzalloc(sizeof(struct sblock_mgr), GFP_KERNEL);
	if (!sblock)
		return -ENOMEM;

	sblock->state = SBLOCK_STATE_IDLE;
	sblock->dst = dst;
	sblock->channel = channel;
	txblocksize = SBLOCKSZ_ALIGN(txblocksize,
				     SBLOCK_ALIGN_BYTES);
	rxblocksize = SBLOCKSZ_ALIGN(rxblocksize,
				     SBLOCK_ALIGN_BYTES);
	sblock->txblksz = txblocksize;
	sblock->rxblksz = rxblocksize;
	sblock->txblknum = txblocknum;
	sblock->rxblknum = rxblocknum;

	/* allocate smem */
	hsize = sizeof(struct sblock_header);
	sblock->smem_size = hsize +
		/* for header*/
		txblocknum * txblocksize +
		rxblocknum * rxblocksize +
		/* for blks  rxblocksize 1.5k*/
		(txblocknum + rxblocknum)
		* sizeof(struct sblock_blks) +
		/* for ring*/
		(txblocknum + rxblocknum) *
		sizeof(struct sblock_blks);
		/* for pool*/

	sblock->smem_addr = audio_mem_alloc(mem_type,
					    &sblock->smem_size);
	pr_err("%s smem_addr is %#x\n", __func__, sblock->smem_addr);
	if (!sblock->smem_addr) {
		pr_err("Failed to allocate smem for sblock\n");
		kfree(sblock);
		return -ENOMEM;
	}
	sblock->smem_virt = audio_mem_vmap(sblock->smem_addr,
					   sblock->smem_size, 1);
	if (!sblock->smem_virt) {
		pr_err("%s audio_mem_vmap failed for sblock->smem_virt",
			__func__);
		audio_mem_free(DDR32, sblock->smem_addr,
			       sblock->smem_size);
		kfree(sblock);
		return -ENOMEM;
	}
	memset_io(sblock->smem_virt, 0, sblock->smem_size);
	sblock->smem_dsp_addr =
		audio_addr_ap2dsp(mem_type, sblock->smem_addr, 0);
	/* initialize ring and header */
	sblock->ring = kzalloc(sizeof(struct sblock_ring),
		GFP_KERNEL);
	if (!sblock->ring) {
		audio_mem_unmap(sblock->smem_virt);
		audio_mem_free(DDR32, sblock->smem_addr,
			sblock->smem_size);
		kfree(sblock);
		return -ENOMEM;
	}

	pr_err("summer: %s head size is %d\n", __func__, hsize);
	ringhd = (volatile struct sblock_ring_header *)
		(sblock->smem_virt);
	ringhd->txblk_addr = sblock->smem_dsp_addr + hsize;
		/*the begin addr of tx sblock phys*/
	ringhd->txblk_count = txblocknum;
	ringhd->txblk_size = txblocksize;
	ringhd->txblk_rdptr = 0;
	ringhd->txblk_wrptr = 0;

	ringhd->txblk_blks =  sblock->smem_dsp_addr  + hsize +
		/*the begin addr of ring tx sblock*/
		txblocknum * txblocksize + rxblocknum * rxblocksize;

	ringhd->rxblk_addr = ringhd->txblk_addr +
		txblocknum * txblocksize;/*the begin addr of rx sblock*/
	ringhd->rxblk_count = rxblocknum;
	ringhd->rxblk_size = rxblocksize;
	ringhd->rxblk_rdptr = 0;
	ringhd->rxblk_wrptr = 0;
	ringhd->rxblk_blks = ringhd->txblk_blks +
		txblocknum * sizeof(struct sblock_blks);
		/*the begin addr of ring rx sblock*/

	poolhd = (volatile struct sblock_ring_header *)
		(sblock->smem_virt + sizeof(struct sblock_ring_header));
	poolhd->txblk_addr =  sblock->smem_dsp_addr  + hsize;
		/*the addr of tx sblock*/
	poolhd->txblk_count = txblocknum;
	poolhd->txblk_size = txblocksize;
	poolhd->txblk_rdptr = 0;
	poolhd->txblk_wrptr = 0;
	poolhd->txblk_blks = ringhd->rxblk_blks +
		rxblocknum * sizeof(struct sblock_blks);
		/*the begin addr of pool tx sblock*/
	poolhd->rxblk_addr = ringhd->txblk_addr +
		txblocknum * txblocksize;/*the addr of rx sblock*/
	poolhd->rxblk_count = rxblocknum;
	poolhd->rxblk_size = rxblocksize;
	poolhd->rxblk_rdptr = 0;
	poolhd->rxblk_wrptr = 0;
	poolhd->rxblk_blks = poolhd->txblk_blks +
		txblocknum * sizeof(struct sblock_blks);
	sblock->ring->txrecord =
		kcalloc(txblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->txrecord) {
		pr_err("Failed to allocate memory for txrecord\n");
		audio_mem_unmap(sblock->smem_virt);
		audio_mem_free(DDR32, sblock->smem_addr, sblock->smem_size);
		kfree(sblock->ring);
		kfree(sblock);
		return -ENOMEM;
	}
	sblock->ring->rxrecord = kcalloc(rxblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->rxrecord) {
		audio_mem_unmap(sblock->smem_virt);
		audio_mem_free(DDR32, sblock->smem_addr, sblock->smem_size);
		kfree(sblock->ring->txrecord);
		kfree(sblock->ring);
		kfree(sblock);
		return -ENOMEM;
	}

	sblock->ring->header = sblock->smem_virt;
	sblock->ring->txblk_virt = sblock->smem_virt +
		(ringhd->txblk_addr -  sblock->smem_dsp_addr);

	sblock->ring->r_txblks = sblock->smem_virt +
		(ringhd->txblk_blks - sblock->smem_dsp_addr);

	sblock->ring->rxblk_virt = sblock->smem_virt +
		(ringhd->rxblk_addr - sblock->smem_dsp_addr);

	sblock->ring->r_rxblks = sblock->smem_virt +
		(ringhd->rxblk_blks - sblock->smem_dsp_addr);

	sblock->ring->p_txblks = sblock->smem_virt +
		(poolhd->txblk_blks - sblock->smem_dsp_addr);

	sblock->ring->p_rxblks = sblock->smem_virt +
		(poolhd->rxblk_blks - sblock->smem_dsp_addr);

	sblock->ring->rxblkmemsz = rxblocknum * rxblocksize;
	sblock->ring->txblkmemsz = txblocknum * txblocksize;

	/*init physic addr*/
	for (i = 0; i < txblocknum; i++) {
		sblock->ring->p_txblks[i].addr =
			poolhd->txblk_addr + i * txblocksize;
		sblock->ring->p_txblks[i].length = txblocksize;
		sblock->ring->txrecord[i] = SBLOCK_BLK_STATE_DONE;
		pr_err("summer %s:p_txblks[%d].addr 0x%08x\n", __func__,
			i, sblock->ring->p_rxblks[i].addr);
		poolhd->txblk_wrptr++;
	}

	for (i = 0; i < rxblocknum; i++) {
		sblock->ring->p_rxblks[i].addr =
			poolhd->rxblk_addr + i * rxblocksize;
		sblock->ring->p_rxblks[i].length = rxblocksize;
		sblock->ring->rxrecord[i] = SBLOCK_BLK_STATE_DONE;
		pr_err("summer %s:p_rxblks[%d].addr 0x%08x, 0x%lx\n",
			__func__, i,
			sblock->ring->p_rxblks[i].addr,
			(unsigned long)&sblock->ring->p_rxblks[i]);
		poolhd->rxblk_wrptr++;
	}

	pr_err("summer : poolhd->rxblk_wrptr %d\n", poolhd->rxblk_wrptr);
	sblock->ring->yell = 1;

	init_waitqueue_head(&sblock->ring->getwait);
	init_waitqueue_head(&sblock->ring->recvwait);
	spin_lock_init(&sblock->ring->r_txlock);
	spin_lock_init(&sblock->ring->r_rxlock);
	spin_lock_init(&sblock->ring->p_txlock);
	spin_lock_init(&sblock->ring->p_rxlock);

	sblock->thread = kthread_create(audio_sblock_thread, sblock,
			"audio_sblock-%d-%d", dst, channel);
	if (IS_ERR(sblock->thread)) {
		pr_err("Failed to create kthread: sblock-%d-%d\n",
			dst, channel);
		audio_mem_unmap(sblock->smem_virt);
		audio_mem_free(DDR32, sblock->smem_addr, sblock->smem_size);
		kfree(sblock->ring->txrecord);
		kfree(sblock->ring->rxrecord);
		kfree(sblock->ring);
		result = PTR_ERR(sblock->thread);
		kfree(sblock);
		return result;
	}

	sblocks[dst][channel] = sblock;
	sblock->state = SBLOCK_STATE_READY;
	wake_up_process(sblock->thread);

	return 0;
}

/*
 * 1.smsg channel open
 * 2.sblock creat
 */
int audio_sblock_init(uint8_t dst, uint8_t channel,
		u32 txblocknum, u32 txblocksize,
		u32 rxblocknum, u32 rxblocksize, int mem_type)
{
	int ret;

	ret = audio_sblock_ch_open(dst, channel);
	if (ret < 0) {
		pr_err("%s audio_sblock_ch_open failed open\n", __func__);
		return ret;
	}

	ret = audio_sblock_create(dst,  channel,
		 txblocknum,  txblocksize,
		 rxblocknum,  rxblocksize, mem_type);
	if (ret < 0) {
		pr_err("%s audio_sblock_create failed open\n", __func__);
		return ret;
	}

	return 0;
}

void audio_sblock_destroy(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];

	pr_info("%s dst:%d, channel:%d", __func__, dst, channel);

	if (sblock == NULL)
		return;

	sblock->state = SBLOCK_STATE_IDLE;
	aud_smsg_ch_close(dst, channel);

	/* stop sblock thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(sblock->thread))
		kthread_stop(sblock->thread);

	if (sblock->ring) {
		wake_up_interruptible_all(&sblock->ring->recvwait);
		wake_up_interruptible_all(&sblock->ring->getwait);
		kfree(sblock->ring->txrecord);
		kfree(sblock->ring->rxrecord);
		kfree(sblock->ring);
	}
	if (sblock->smem_virt)
		audio_mem_unmap(sblock->smem_virt);
	audio_mem_free(DDR32, sblock->smem_addr, sblock->smem_size);
	kfree(sblock);

	sblocks[dst][channel] = NULL;
}

int audio_sblock_register_notifier(uint8_t dst, uint8_t channel,
		void (*handler)(int event, void *data), void *data)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];

	if (!sblock) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}
#ifndef CONFIG_SIPC_WCN
	if (sblock->handler) {
		pr_err("sblock handler already registered\n");
		return -EBUSY;
	}
#endif
	sblock->handler = handler;
	sblock->data = data;

	return 0;
}

int audio_sblock_get(uint8_t dst, uint8_t channel,
	struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	int txpos, index;
	int rval = 0;
	unsigned long flags;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	if (poolhd->txblk_rdptr == poolhd->txblk_wrptr) {
		if (timeout == 0) {
			/* no wait */
			pr_warn("sblock_get %d-%d is empty!\n",
				dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->getwait,
					(poolhd->txblk_rdptr
					!= poolhd->txblk_wrptr) ||
					(sblock->state == SBLOCK_STATE_IDLE));
			if (rval < 0)
				pr_warn("sblock_get wait interrupted!\n");

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_err("sblock_get sblock state is idle!\n");
				rval = -EIO;
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(
				ring->getwait,
				(poolhd->txblk_rdptr != poolhd->txblk_wrptr)
				|| (sblock == SBLOCK_STATE_IDLE),
				timeout);
			if (rval < 0)
				pr_warn("sblock_get wait interrupted!\n");
			else if (rval == 0) {
				pr_warn("sblock_get wait timeout!\n");
				rval = -ETIME;
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_err("sblock_get sblock state is idle!\n");
				rval = -EIO;
			}
		}
	}

	if (rval < 0)
		return rval;
	/* multi-gotter may cause got failure */
	spin_lock_irqsave(&ring->p_txlock, flags);
	if (poolhd->txblk_rdptr != poolhd->txblk_wrptr &&
			sblock->state == SBLOCK_STATE_READY) {
		txpos = sblock_get_ringpos(poolhd->txblk_rdptr,
			poolhd->txblk_count);

		if ((ring->p_txblks[txpos].addr >= ringhd->txblk_addr) &&
			(ring->p_txblks[txpos].addr + poolhd->txblk_size <=
			ringhd->txblk_addr + ring->txblkmemsz)) {
			blk->addr = sblock->smem_virt +
				(ring->p_txblks[txpos].addr - sblock->smem_dsp_addr);
			blk->length = poolhd->txblk_size;
			index = sblock_get_index((blk->addr - ring->txblk_virt),
			sblock->txblksz);
			ring->txrecord[index] = SBLOCK_BLK_STATE_PENDING;
		} else {
			sblock->txblks_err++;
			pr_err("%s:block address error:ch:%d, block error:addr:0x%p,length:%x,err bxblks:%d\n",
			    __func__, sblock->channel, blk->addr, blk->length,
			    sblock->txblks_err);
			rval = -ENXIO;
		}
		poolhd->txblk_rdptr = poolhd->txblk_rdptr + 1;
	} else {
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->p_txlock, flags);

	return rval;
}

static int audio_sblock_send_ex(uint8_t dst,
	uint8_t channel, struct sblock *blk, bool yell)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	int txpos, index;
	int rval = 0;
	unsigned long flags;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	pr_debug("sblock_send: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	ringhd =
		(volatile struct sblock_ring_header *)(&ring->header->ring);

	if (blk->addr < ring->txblk_virt ||
		blk->addr + blk->length >
			ring->txblk_virt + ring->txblkmemsz) {
		pr_err("%s:block ch:%d,error:addr:0x%p,length:%x\n", __func__,
			channel, blk->addr, blk->length);
		return -EINVAL;
	}

	spin_lock_irqsave(&ring->r_txlock, flags);

	txpos = sblock_get_ringpos(ringhd->txblk_wrptr,
		ringhd->txblk_count);
	ring->r_txblks[txpos].addr = blk->addr - sblock->smem_virt
		+ sblock->smem_dsp_addr;
	ring->r_txblks[txpos].length = blk->length;
	pr_debug("sblock_send: channel=%d, wrptr=%d, txpos=%d, addr=%x\n",
			channel, ringhd->txblk_wrptr,
			txpos, ring->r_txblks[txpos].addr);
	ringhd->txblk_wrptr = ringhd->txblk_wrptr + 1;
	if (sblock->state == SBLOCK_STATE_READY) {
		if (yell)
			pr_debug("%s do nothing\n", __func__);
		else if (!ring->yell) {
			if (((int)(ringhd->txblk_wrptr - ringhd->txblk_rdptr)
				== 1))
				ring->yell = 1;
		}
	}
	index = sblock_get_index((blk->addr - ring->txblk_virt),
		sblock->txblksz);
	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->r_txlock, flags);

	return rval;
}

int audio_sblock_send(uint8_t dst, uint8_t channel, struct sblock *blk)
{
	return audio_sblock_send_ex(dst, channel, blk, true);
}

int audio_sblock_send_prepare(uint8_t dst,
	uint8_t channel, struct sblock *blk)
{
	return audio_sblock_send_ex(dst, channel, blk, false);
}

int audio_sblock_send_finish(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock =
		(struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	int rval = 0;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd =
		(volatile struct sblock_ring_header *)(&ring->header->ring);

	if (ring->yell) {
		ring->yell = 0;
		pr_debug("%s do nothing\n", __func__);
	}

	return rval;
}

int audio_sblock_receive(
	uint8_t dst, uint8_t channel, struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	int rxpos, index, rval = 0;
	unsigned long flags;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd =
		(volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd =
		(volatile struct sblock_ring_header *)(&ring->header->pool);

	if (ringhd->rxblk_wrptr == ringhd->rxblk_rdptr) {
		if (timeout == 0) {
			/* no wait */
			pr_err("sblock_receive %d-%d is empty!\n",
				dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->recvwait,
				((ringhd->rxblk_wrptr != ringhd->rxblk_rdptr)
				|| sblock->state == SBLOCK_STATE_IDLE));
			if (rval < 0)
				pr_warn("sblock_receive wait interrupted!\n");

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_err("sblock_receive sblock state is idle!\n");
				rval = -EIO;
			}

		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->recvwait,
				(ringhd->rxblk_wrptr != ringhd->rxblk_rdptr)
				|| sblock->state == SBLOCK_STATE_IDLE, timeout);
			if (rval < 0) {
				pr_warn("sblock_receive wait interrupted!\n");
			} else if (rval == 0) {
				pr_warn("sblock_receive wait timeout!\n");
				rval = -ETIME;
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_err("sblock_receive sblock state is idle!\n");
				rval = -EIO;
			}
		}
	}
	if (rval < 0)
		return rval;
	/* multi-receiver may cause recv failure */
	spin_lock_irqsave(&ring->r_rxlock, flags);
	if (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr &&
			sblock->state == SBLOCK_STATE_READY) {
		rxpos = sblock_get_ringpos(
			ringhd->rxblk_rdptr, ringhd->rxblk_count);
		blk->addr = ring->r_rxblks[rxpos].addr
			- sblock->smem_dsp_addr + sblock->smem_virt;
		blk->length = ring->r_rxblks[rxpos].length;
		ringhd->rxblk_rdptr = ringhd->rxblk_rdptr + 1;
		if (blk->addr >= ring->rxblk_virt &&
			blk->addr + blk->length <=
				ring->rxblk_virt + ring->rxblkmemsz) {
			index = sblock_get_index(
			    (blk->addr - ring->rxblk_virt), sblock->rxblksz);
			ring->rxrecord[index] = SBLOCK_BLK_STATE_PENDING;
		} else {
			sblock->rxblks_err++;
			pr_err("%s:ch:%d, block error:addr:0x%p,length:%x,err blk cnt:%d\n",
			    __func__, sblock->channel, blk->addr, blk->length,
			    sblock->rxblks_err);
			rval =
			    sblock->state ==
			    SBLOCK_STATE_READY ? -EAGAIN : -EIO;
		}
	} else {
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->r_rxlock, flags);

	return rval;
}

int audio_sblock_get_arrived_count(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *ringhd = NULL;
	int blk_count = 0;
	unsigned long flags;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	spin_lock_irqsave(&ring->r_rxlock, flags);
	blk_count = (int)(ringhd->rxblk_wrptr - ringhd->rxblk_rdptr);
	spin_unlock_irqrestore(&ring->r_rxlock, flags);

	return blk_count;

}

int audio_sblock_get_free_count(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	int blk_count = 0;
	unsigned long flags;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_txlock, flags);
	blk_count = (int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr);
	spin_unlock_irqrestore(&ring->p_txlock, flags);

	return blk_count;
}

int audio_sblock_release(uint8_t dst, uint8_t channel, struct sblock *blk)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	unsigned long flags;
	int rxpos;
	int index;
	int ret = 0;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}
	ring = sblock->ring;
	ringhd =
		(volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd =
		(volatile struct sblock_ring_header *)(&ring->header->pool);
	spin_lock_irqsave(&ring->p_rxlock, flags);
	rxpos = sblock_get_ringpos(poolhd->rxblk_wrptr,
		poolhd->rxblk_count);

	if ((blk->addr >= ring->rxblk_virt) &&
		(blk->addr + poolhd->rxblk_size <=
			ring->rxblk_virt + ring->rxblkmemsz)) {
		ring->p_rxblks[rxpos].addr =
			blk->addr - sblock->smem_virt
			+ sblock->smem_dsp_addr;
		ring->p_rxblks[rxpos].length = poolhd->rxblk_size;
		poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
		index = sblock_get_index((blk->addr - ring->rxblk_virt),
			sblock->rxblksz);
		ring->rxrecord[index] = SBLOCK_BLK_STATE_DONE;
	} else {
		pr_err("sblock_release ch:%d,addr error!addr=0x%p,length:%d",
			sblock->channel, blk->addr, blk->length);
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&ring->p_rxlock, flags);

	return ret;
}

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBLOCK driver");
MODULE_LICENSE("GPL");
