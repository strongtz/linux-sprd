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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sipc.h>
#include <uapi/linux/sched/types.h>

#include "sipc_priv.h"
#include "sblock.h"

static struct sblock_mgr *sblocks[SIPC_ID_NR][SMSG_VALID_CH_NR];

void sblock_put(u8 dst, u8 channel, struct sblock *blk)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *poolhd;
	unsigned long flags;
	int txpos;
	int index;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock)
		return;

	ring = sblock->ring;
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_txlock, flags);
	txpos = sblock_get_ringpos(poolhd->txblk_rdptr - 1,
				   poolhd->txblk_count);
	ring->r_txblks[txpos].addr = blk->addr -
				     sblock->smem_virt +
				     sblock->mapped_smem_addr;
	ring->r_txblks[txpos].length = poolhd->txblk_size;
	poolhd->txblk_rdptr = poolhd->txblk_rdptr - 1;
	if ((int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr) == 1)
		wake_up_interruptible_all(&(ring->getwait));
	index = sblock_get_index((blk->addr - ring->txblk_virt),
				 sblock->txblksz);
	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_txlock, flags);
}
EXPORT_SYMBOL_GPL(sblock_put);

static int sblock_recover(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	unsigned long pflags, qflags;
	int i, j;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock)
		return -ENODEV;

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	sblock->state = SBLOCK_STATE_IDLE;
	wake_up_interruptible_all(&ring->getwait);
	wake_up_interruptible_all(&ring->recvwait);

	spin_lock_irqsave(&ring->r_txlock, pflags);
	/* clean txblks ring */
	ringhd->txblk_wrptr = ringhd->txblk_rdptr;

	spin_lock_irqsave(&ring->p_txlock, qflags);
	/* recover txblks pool */
	poolhd->txblk_rdptr = poolhd->txblk_wrptr;
	for (i = 0, j = 0; i < poolhd->txblk_count; i++) {
		if (ring->txrecord[i] == SBLOCK_BLK_STATE_DONE) {
			ring->p_txblks[j].addr = i * sblock->txblksz +
						 poolhd->txblk_addr;
			ring->p_txblks[j].length = sblock->txblksz;
			poolhd->txblk_wrptr = poolhd->txblk_wrptr + 1;
			j++;
		}
	}
	spin_unlock_irqrestore(&ring->p_txlock, qflags);
	spin_unlock_irqrestore(&ring->r_txlock, pflags);

	spin_lock_irqsave(&ring->r_rxlock, pflags);
	/* clean rxblks ring */
	ringhd->rxblk_rdptr = ringhd->rxblk_wrptr;

	spin_lock_irqsave(&ring->p_rxlock, qflags);
	/* recover rxblks pool */
	poolhd->rxblk_wrptr = poolhd->rxblk_rdptr;
	for (i = 0, j = 0; i < poolhd->rxblk_count; i++) {
		if (ring->rxrecord[i] == SBLOCK_BLK_STATE_DONE) {
			ring->p_rxblks[j].addr = i * sblock->rxblksz +
						 poolhd->rxblk_addr;
			ring->p_rxblks[j].length = sblock->rxblksz;
			poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
			j++;
		}
	}
	spin_unlock_irqrestore(&ring->p_rxlock, qflags);
	spin_unlock_irqrestore(&ring->r_rxlock, pflags);

	return 0;
}

static int sblock_thread(void *data)
{
	struct sblock_mgr *sblock = data;
	struct smsg mcmd, mrecv;
	int rval;
	int recovery = 0;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	/* since the channel open may hang, we call it in the sblock thread */
	rval = smsg_ch_open(sblock->dst, sblock->channel, -1);
	if (rval != 0) {
		pr_info("Failed to open channel %d\n",
			sblock->channel);
		/* assign NULL to thread poniter as failed to open channel */
		sblock->thread = NULL;
		return rval;
	}

	/* handle the sblock events */
	while (!kthread_should_stop()) {
		/* monitor sblock recv smsg */
		smsg_set(&mrecv, sblock->channel, 0, 0, 0);
		rval = smsg_recv(sblock->dst, &mrecv, -1);
		if (rval == -EIO || rval == -ENODEV) {
			/* channel state is FREE */
			msleep(20);
			continue;
		}

		pr_debug("sblock thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			 sblock->dst, sblock->channel,
			 mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* handle channel recovery */
			if (recovery) {
				if (sblock->handler)
					sblock->handler(SBLOCK_NOTIFY_CLOSE,
							sblock->data);
				sblock_recover(sblock->dst, sblock->channel);
			}
			smsg_open_ack(sblock->dst, sblock->channel);
			break;
		case SMSG_TYPE_CLOSE:
			/* handle channel recovery */
			smsg_close_ack(sblock->dst, sblock->channel);
			if (sblock->handler)
				sblock->handler(SBLOCK_NOTIFY_CLOSE,
						sblock->data);
			sblock->state = SBLOCK_STATE_IDLE;
			break;
		case SMSG_TYPE_CMD:
			/* respond cmd done for sblock init */
			WARN_ON(mrecv.flag != SMSG_CMD_SBLOCK_INIT);
			smsg_set(&mcmd,
				 sblock->channel,
				 SMSG_TYPE_DONE,
				 SMSG_DONE_SBLOCK_INIT,
				 sblock->mapped_smem_addr);
			smsg_send(sblock->dst, &mcmd, -1);
			if (sblock->handler)
				sblock->handler(SBLOCK_NOTIFY_OPEN,
						sblock->data);
			sblock->state = SBLOCK_STATE_READY;
			recovery = 1;
			break;
		case SMSG_TYPE_EVENT:
			/* handle sblock send/release events */
			switch (mrecv.flag) {
			case SMSG_EVENT_SBLOCK_SEND:
				wake_up_interruptible_all(&sblock->
							  ring->
							  recvwait);
				if (sblock->handler)
					sblock->handler(SBLOCK_NOTIFY_RECV,
							sblock->data);
				break;
			case SMSG_EVENT_SBLOCK_RELEASE:
				wake_up_interruptible_all(&(sblock->ring->
							    getwait));
				if (sblock->handler)
					sblock->handler(SBLOCK_NOTIFY_GET,
							sblock->data);
				break;
			default:
				rval = 1;
				break;
			}
			break;
		default:
			rval = 1;
			break;
		};

		if (rval) {
			pr_info("non-handled sblock msg: %d-%d, %d, %d, %d\n",
					sblock->dst, sblock->channel,
					mrecv.type, mrecv.flag, mrecv.value);
			rval = 0;
		}
	}

	pr_info("sblock %d-%d thread stop",
		sblock->dst, sblock->channel);
	return rval;
}

int sblock_create_ex(u8 dst, u8 channel,
		     u32 txblocknum, u32 txblocksize,
		     u32 rxblocknum, u32 rxblocksize,
		     void (*handler)(int event, void *data), void *data)
{
	struct sblock_mgr *sblock;
	struct sipc_device *sdev;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	u32 hsize;
	int i, result;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}
	if (dst >= SIPC_ID_NR) {
		pr_err("sblock_create_ex: dst = %d is invalid\n", dst);
		return -EINVAL;
	}
	sdev = sipc_ap.sipc_dev[dst];
	if (!sdev) {
		pr_err("sblock_create_ex: sdev is null, dst = %d\n", dst);
		return -ENODEV;
	}

	sblock = kzalloc(sizeof(struct sblock_mgr), GFP_KERNEL);
	if (!sblock)
		return -ENOMEM;

	sblock->state = SBLOCK_STATE_IDLE;
	sblock->dst = dst;
	sblock->channel = channel;
	txblocksize = ALIGN(txblocksize, SBLOCK_ALIGN_BYTES);
	rxblocksize = ALIGN(rxblocksize, SBLOCK_ALIGN_BYTES);
	sblock->txblksz = txblocksize;
	sblock->rxblksz = rxblocksize;
	sblock->txblknum = txblocknum;
	sblock->rxblknum = rxblocknum;

	/* allocate smem */
	hsize = sizeof(struct sblock_header);
		/* for header*/
	sblock->smem_size = hsize +
		/* for blks */
		txblocknum * txblocksize + rxblocknum * rxblocksize +
		/* for ring */
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks) +
		/* for pool */
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks);

	sblock->smem_addr = smem_alloc_ex(sblock->smem_size,
					  sdev->pdata->smem_base);

	if (!sblock->smem_addr) {
		pr_err("Failed to allocate smem for sblock\n");
		kfree(sblock);
		return -ENOMEM;
	}
	sblock->mapped_smem_addr = sblock->smem_addr -
		sdev->pdata->smem_base + sdev->pdata->mapped_smem_base;
	sblock->smem_virt = shmem_ram_vmap_nocache(sblock->smem_addr,
					    sblock->smem_size);
	if (!sblock->smem_virt) {
		pr_err("Failed to map smem for sblock\n");
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock);
		return -EFAULT;
	}

	/* initialize ring and header */
	sblock->ring = kzalloc(sizeof(struct sblock_ring), GFP_KERNEL);
	if (!sblock->ring) {
		pr_err("Failed to allocate ring for sblock\n");
		shmem_ram_unmap(sblock->smem_virt);
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock);
		return -ENOMEM;
	}
	ringhd = (volatile struct sblock_ring_header *)(sblock->smem_virt);
	ringhd->txblk_addr = sblock->mapped_smem_addr + hsize;
	ringhd->txblk_count = txblocknum;
	ringhd->txblk_size = txblocksize;
	ringhd->txblk_rdptr = 0;
	ringhd->txblk_wrptr = 0;
	ringhd->txblk_blks = ringhd->txblk_addr +
		txblocknum * txblocksize + rxblocknum * rxblocksize;
	ringhd->rxblk_addr = ringhd->txblk_addr + txblocknum * txblocksize;
	ringhd->rxblk_count = rxblocknum;
	ringhd->rxblk_size = rxblocksize;
	ringhd->rxblk_rdptr = 0;
	ringhd->rxblk_wrptr = 0;
	ringhd->rxblk_blks = ringhd->txblk_blks +
			     txblocknum * sizeof(struct sblock_blks);

	poolhd = (volatile struct sblock_ring_header *)(sblock->smem_virt +
					sizeof(struct sblock_ring_header));
	poolhd->txblk_addr = sblock->mapped_smem_addr + hsize;
	poolhd->txblk_count = txblocknum;
	poolhd->txblk_size = txblocksize;
	poolhd->txblk_rdptr = 0;
	poolhd->txblk_wrptr = 0;
	poolhd->txblk_blks = ringhd->rxblk_blks +
			     rxblocknum * sizeof(struct sblock_blks);
	poolhd->rxblk_addr = ringhd->txblk_addr + txblocknum * txblocksize;
	poolhd->rxblk_count = rxblocknum;
	poolhd->rxblk_size = rxblocksize;
	poolhd->rxblk_rdptr = 0;
	poolhd->rxblk_wrptr = 0;
	poolhd->rxblk_blks = poolhd->txblk_blks +
			     txblocknum * sizeof(struct sblock_blks);

	sblock->ring->txrecord = kcalloc(txblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->txrecord) {
		pr_err("Failed to allocate memory for txrecord\n");
		shmem_ram_unmap(sblock->smem_virt);
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock->ring);
		kfree(sblock);
		return -ENOMEM;
	}

	sblock->ring->rxrecord = kcalloc(rxblocknum, sizeof(int), GFP_KERNEL);
	if (!sblock->ring->rxrecord) {
		pr_err("Failed to allocate memory for rxrecord\n");
		shmem_ram_unmap(sblock->smem_virt);
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock->ring->txrecord);
		kfree(sblock->ring);
		kfree(sblock);
		return -ENOMEM;
	}

	sblock->ring->header = sblock->smem_virt;
	sblock->ring->txblk_virt = sblock->smem_virt +
		(ringhd->txblk_addr - sblock->mapped_smem_addr);
	sblock->ring->r_txblks = sblock->smem_virt +
		(ringhd->txblk_blks - sblock->mapped_smem_addr);
	sblock->ring->rxblk_virt = sblock->smem_virt +
		(ringhd->rxblk_addr - sblock->mapped_smem_addr);
	sblock->ring->r_rxblks = sblock->smem_virt +
		(ringhd->rxblk_blks - sblock->mapped_smem_addr);
	sblock->ring->p_txblks = sblock->smem_virt +
		(poolhd->txblk_blks - sblock->mapped_smem_addr);
	sblock->ring->p_rxblks = sblock->smem_virt +
		(poolhd->rxblk_blks - sblock->mapped_smem_addr);

	for (i = 0; i < txblocknum; i++) {
		sblock->ring->p_txblks[i].addr = poolhd->txblk_addr +
						 i * txblocksize;
		sblock->ring->p_txblks[i].length = txblocksize;
		sblock->ring->txrecord[i] = SBLOCK_BLK_STATE_DONE;
		poolhd->txblk_wrptr++;
	}
	for (i = 0; i < rxblocknum; i++) {
		sblock->ring->p_rxblks[i].addr = poolhd->rxblk_addr +
						 i * rxblocksize;
		sblock->ring->p_rxblks[i].length = rxblocksize;
		sblock->ring->rxrecord[i] = SBLOCK_BLK_STATE_DONE;
		poolhd->rxblk_wrptr++;
	}

	init_waitqueue_head(&sblock->ring->getwait);
	init_waitqueue_head(&sblock->ring->recvwait);
	spin_lock_init(&sblock->ring->r_txlock);
	spin_lock_init(&sblock->ring->r_rxlock);
	spin_lock_init(&sblock->ring->p_txlock);
	spin_lock_init(&sblock->ring->p_rxlock);

	sblock->thread = kthread_create(sblock_thread, sblock,
			"sblock-%d-%d", dst, channel);
	if (IS_ERR(sblock->thread)) {
		pr_err("Failed to create kthread: sblock-%d-%d\n",
			dst, channel);
		shmem_ram_unmap(sblock->smem_virt);
		smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock->ring->txrecord);
		kfree(sblock->ring->rxrecord);
		kfree(sblock->ring);
		result = PTR_ERR(sblock->thread);
		kfree(sblock);
		return result;
	}

	sblocks[dst][ch_index] = sblock;
	if ((handler != NULL) && (data != NULL)) {
		result = sblock_register_notifier(dst, channel, handler, data);
		if (result < 0) {
			sblock_destroy(dst, channel);
			return result;
		}
	}
	wake_up_process(sblock->thread);

	return 0;
}
EXPORT_SYMBOL_GPL(sblock_create_ex);

int sblock_create(u8 dst, u8 channel,
		  u32 txblocknum, u32 txblocksize,
		  u32 rxblocknum, u32 rxblocksize)
{
	int result;
	result = sblock_create_ex(dst, channel, txblocknum, txblocksize,
				  rxblocknum, rxblocksize, NULL, NULL);
	return result;
}
EXPORT_SYMBOL_GPL(sblock_create);

void sblock_destroy(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return;
	}

	sblock = sblocks[dst][ch_index];
	if (sblock == NULL)
		return;

	sblock->state = SBLOCK_STATE_IDLE;
	smsg_ch_close(dst, channel, -1);

	/* stop sblock thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(sblock->thread))
		kthread_stop(sblock->thread);

	if (sblock->ring) {
		wake_up_interruptible_all(&sblock->ring->recvwait);
		wake_up_interruptible_all(&sblock->ring->getwait);
		/* kfree(NULL) is safe */
		/* if (sblock->ring->txrecord) */
			kfree(sblock->ring->txrecord);
		/* if (sblock->ring->rxrecord) */
			kfree(sblock->ring->rxrecord);
		kfree(sblock->ring);
	}
	if (sblock->smem_virt)
		shmem_ram_unmap(sblock->smem_virt);

	smem_free(sblock->smem_addr, sblock->smem_size);
	kfree(sblock);

	sblocks[dst][ch_index] = NULL;
}
EXPORT_SYMBOL_GPL(sblock_destroy);

int sblock_register_notifier(u8 dst, u8 channel,
			     void (*handler)(int event, void *data),
			     void *data)
{
	struct sblock_mgr *sblock;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return -ENODEV;
	}
#ifndef CONFIG_SPRD_SIPC_WCN
	if (sblock->handler) {
		pr_err("sblock handler already registered\n");
		return -EBUSY;
	}
#endif
	sblock->handler = handler;
	sblock->data = data;

	return 0;
}
EXPORT_SYMBOL_GPL(sblock_register_notifier);

int sblock_get(u8 dst, u8 channel, struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	int txpos, index;
	int rval = 0;
	unsigned long flags;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	if (poolhd->txblk_rdptr == poolhd->txblk_wrptr) {
		if (timeout == 0) {
			/* no wait */
			pr_err("sblock_get %d-%d is empty!\n",
				dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->getwait,
				poolhd->txblk_rdptr != poolhd->txblk_wrptr ||
				sblock->state == SBLOCK_STATE_IDLE);
			if (rval < 0)
				pr_info("sblock_get wait interrupted!\n");

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_err("sblock_get sblock state is idle!\n");
				rval = -EIO;
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->getwait,
				poolhd->txblk_rdptr != poolhd->txblk_wrptr ||
				sblock == SBLOCK_STATE_IDLE,
				timeout);
			if (rval < 0) {
				pr_info("sblock_get wait interrupted!\n");
			} else if (rval == 0) {
				pr_info("sblock_get wait timeout!\n");
				rval = -ETIME;
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_info("sblock_get sblock state is idle!\n");
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
		blk->addr = sblock->smem_virt +
			    (ring->p_txblks[txpos].addr -
			     sblock->mapped_smem_addr);
		blk->length = poolhd->txblk_size;
		poolhd->txblk_rdptr = poolhd->txblk_rdptr + 1;
		index = sblock_get_index((blk->addr - ring->txblk_virt),
					 sblock->txblksz);
		ring->txrecord[index] = SBLOCK_BLK_STATE_PENDING;
	} else {
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->p_txlock, flags);

	return rval;
}
EXPORT_SYMBOL_GPL(sblock_get);

static int sblock_send_ex(u8 dst, u8 channel,
			  struct sblock *blk, bool yell)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	struct smsg mevt;
	int txpos, index;
	int rval = 0;
	unsigned long flags;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	pr_debug("sblock_send: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	spin_lock_irqsave(&ring->r_txlock, flags);

	txpos = sblock_get_ringpos(ringhd->txblk_wrptr, ringhd->txblk_count);
	ring->r_txblks[txpos].addr = blk->addr -
				     sblock->smem_virt +
				     sblock->mapped_smem_addr;
	ring->r_txblks[txpos].length = blk->length;
	pr_debug("sblock_send: channel=%d, wrptr=%d, txpos=%d, addr=%x\n",
		 channel, ringhd->txblk_wrptr,
		 txpos, ring->r_txblks[txpos].addr);
	ringhd->txblk_wrptr = ringhd->txblk_wrptr + 1;

	if (sblock->state == SBLOCK_STATE_READY) {
		if (yell) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SBLOCK_SEND,
				 0);
			rval = smsg_send(dst, &mevt, 0);
		} else if (!ring->yell) {
			if ((int)(ringhd->txblk_wrptr -
				  ringhd->txblk_rdptr) == 1)
				ring->yell = 1;
		}
	}
	index = sblock_get_index((blk->addr - ring->txblk_virt),
				 sblock->txblksz);
	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->r_txlock, flags);

	return rval;
}

int sblock_send(u8 dst, u8 channel, struct sblock *blk)
{
	return sblock_send_ex(dst, channel, blk, true);
}
EXPORT_SYMBOL_GPL(sblock_send);

int sblock_send_prepare(u8 dst, u8 channel, struct sblock *blk)
{
	return sblock_send_ex(dst, channel, blk, false);
}
EXPORT_SYMBOL_GPL(sblock_send_prepare);

int sblock_send_finish(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	struct smsg mevt;
	int rval = 0;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	if (ringhd->txblk_wrptr != ringhd->txblk_rdptr) {
		smsg_set(&mevt, channel,
			 SMSG_TYPE_EVENT,
			 SMSG_EVENT_SBLOCK_SEND, 0);
		rval = smsg_send(dst, &mevt, 0);
	}

	return rval;
}
EXPORT_SYMBOL_GPL(sblock_send_finish);

int sblock_receive(u8 dst, u8 channel,
		   struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	int rxpos, index, rval = 0;
	unsigned long flags;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	pr_debug("sblock_receive: dst=%d, channel=%d, timeout=%d\n",
			dst, channel, timeout);
	pr_debug("sblock_receive: channel=%d, wrptr=%d, rdptr=%d",
			channel, ringhd->rxblk_wrptr, ringhd->rxblk_rdptr);

	if (ringhd->rxblk_wrptr == ringhd->rxblk_rdptr) {
		if (timeout == 0) {
			/* no wait */
			pr_debug("sblock_receive %d-%d is empty!\n",
				dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->recvwait,
				ringhd->rxblk_wrptr != ringhd->rxblk_rdptr);
			if (rval < 0)
				pr_info("sblock_receive wait interrupted!\n");

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_info("sblock_receive sblock state is idle!\n");
				rval = -EIO;
			}

		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->recvwait,
				ringhd->rxblk_wrptr != ringhd->rxblk_rdptr,
				timeout);
			if (rval < 0) {
				pr_info("sblock_receive wait interrupted!\n");
			} else if (rval == 0) {
				pr_info("sblock_receive wait timeout!\n");
				rval = -ETIME;
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				pr_info("sblock_receive sblock state is idle!\n");
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
		rxpos = sblock_get_ringpos(ringhd->rxblk_rdptr,
					   ringhd->rxblk_count);
		blk->addr = ring->r_rxblks[rxpos].addr -
			    sblock->mapped_smem_addr +
			    sblock->smem_virt;
		blk->length = ring->r_rxblks[rxpos].length;
		ringhd->rxblk_rdptr = ringhd->rxblk_rdptr + 1;
		pr_debug("sblock_receive: channel=%d, rxpos=%d, addr=%p, len=%d\n",
			channel, rxpos, blk->addr, blk->length);
		index = sblock_get_index((blk->addr - ring->rxblk_virt),
					 sblock->rxblksz);
		ring->rxrecord[index] = SBLOCK_BLK_STATE_PENDING;
	} else {
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->r_rxlock, flags);

	return rval;
}
EXPORT_SYMBOL_GPL(sblock_receive);

int sblock_get_arrived_count(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	int blk_count = 0;
	unsigned long flags;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	spin_lock_irqsave(&ring->r_rxlock, flags);
	blk_count = (int)(ringhd->rxblk_wrptr - ringhd->rxblk_rdptr);
	spin_unlock_irqrestore(&ring->r_rxlock, flags);

	return blk_count;

}
EXPORT_SYMBOL_GPL(sblock_get_arrived_count);

int sblock_get_free_count(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *poolhd;
	int blk_count = 0;
	unsigned long flags;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_txlock, flags);
	blk_count = (int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr);
	spin_unlock_irqrestore(&ring->p_txlock, flags);

	return blk_count;
}
EXPORT_SYMBOL_GPL(sblock_get_free_count);

int sblock_release(u8 dst, u8 channel, struct sblock *blk)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	struct smsg mevt;
	unsigned long flags;
	int rxpos;
	int index;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready!\n", __func__, dst, channel);
		return -ENODEV;
	}

	pr_debug("sblock_release: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_rxlock, flags);
	rxpos = sblock_get_ringpos(poolhd->rxblk_wrptr, poolhd->rxblk_count);
	ring->p_rxblks[rxpos].addr = blk->addr -
				     sblock->smem_virt +
				     sblock->mapped_smem_addr;
	ring->p_rxblks[rxpos].length = poolhd->rxblk_size;
	poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
	pr_debug("sblock_release: addr=%x\n", ring->p_rxblks[rxpos].addr);

	if ((int)(poolhd->rxblk_wrptr - poolhd->rxblk_rdptr) == 1 &&
	    sblock->state == SBLOCK_STATE_READY) {
		/* send smsg to notify the peer side */
		smsg_set(&mevt, channel,
			 SMSG_TYPE_EVENT,
			 SMSG_EVENT_SBLOCK_RELEASE,
			 0);
		smsg_send(dst, &mevt, -1);
	}

	index = sblock_get_index((blk->addr - ring->rxblk_virt),
				 sblock->rxblksz);
	ring->rxrecord[index] = SBLOCK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_rxlock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(sblock_release);

unsigned int sblock_poll_wait(u8 dst, u8 channel, struct file *filp, poll_table *wait)
{
	struct sblock_mgr *sblock;
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	unsigned int mask = 0;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}
	sblock = sblocks[dst][ch_index];

	if (!sblock)
		return -ENODEV;
	ring = sblock->ring;
	ringhd = &ring->header->ring;
	poolhd = &ring->header->pool;
	if (sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready to poll !\n",
		       __func__, dst, channel);
		return -ENODEV;
	}
	poll_wait(filp, &ring->recvwait, wait);
	poll_wait(filp, &ring->getwait, wait);
	if (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr)
		mask |= POLLIN | POLLRDNORM;
	if (poolhd->txblk_rdptr != poolhd->txblk_wrptr)
		mask |= POLLOUT | POLLWRNORM;
	return mask;
}
EXPORT_SYMBOL_GPL(sblock_poll_wait);

int sblock_query(u8 dst, u8 channel)
{
	struct sblock_mgr *sblock = NULL;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return -EINVAL;
	}

	sblock = sblocks[dst][ch_index];
	if (!sblock)
		return -ENODEV;
	if (sblock->state != SBLOCK_STATE_READY) {
		pr_err("%s:sblock-%d-%d not ready !\n", __func__, dst, channel);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sblock_query);

#if defined(CONFIG_DEBUG_FS)
static int sblock_debug_show(struct seq_file *m, void *private)
{
	struct sblock_mgr *sblock;
	struct sblock_ring  *ring;
	volatile struct sblock_ring_header *ringhd;
	volatile struct sblock_ring_header *poolhd;
	int i, j;

	for (i = 0; i < SIPC_ID_NR; i++) {
		for (j = 0;  j < SMSG_VALID_CH_NR; j++) {
			sblock = sblocks[i][j];
			if (!sblock)
				continue;
			ring = sblock->ring;
			ringhd = (volatile struct sblock_ring_header *)(&sblock->ring->
						      header->ring);
			poolhd = (volatile struct sblock_ring_header *)(&sblock->ring->
						      header->pool);

			sipc_debug_putline(m, '*', 170);
			seq_printf(m, "sblock dst %d, channel: %3d, state: %d, smem_virt: 0x%lx, smem_addr: 0x%0x, mapped_smem_addr: 0x%0x, smem_size: 0x%0x, txblksz: %d, rxblksz: %d\n",
				   sblock->dst,
				   sblock->channel,
				   sblock->state,
				   (unsigned long)sblock->smem_virt,
				   sblock->smem_addr,
				   sblock->mapped_smem_addr,
				   sblock->smem_size,
				   sblock->txblksz,
				   sblock->rxblksz);
			seq_printf(m, "sblock ring: txblk_virt :0x%lx, rxblk_virt :0x%lx\n",
				   (unsigned long)ring->txblk_virt,
				   (unsigned long)ring->rxblk_virt);
			seq_printf(m, "sblock ring header: rxblk_addr :0x%0x, rxblk_rdptr :0x%0x, rxblk_wrptr :0x%0x, rxblk_size :%d, rxblk_count :%d, rxblk_blks: 0x%0x\n",
				   ringhd->rxblk_addr, ringhd->rxblk_rdptr,
				   ringhd->rxblk_wrptr, ringhd->rxblk_size,
				   (int)(ringhd->rxblk_wrptr - ringhd->rxblk_rdptr),
				   ringhd->rxblk_blks);
			seq_printf(m, "sblock ring header: txblk_addr :0x%0x, txblk_rdptr :0x%0x, txblk_wrptr :0x%0x, txblk_size :%d, txblk_count :%d, txblk_blks: 0x%0x\n",
				   ringhd->txblk_addr, ringhd->txblk_rdptr,
				   ringhd->txblk_wrptr, ringhd->txblk_size,
				   (int)(ringhd->txblk_wrptr - ringhd->txblk_rdptr),
				   ringhd->txblk_blks);
			seq_printf(m, "sblock pool header: rxblk_addr :0x%0x, rxblk_rdptr :0x%0x, rxblk_wrptr :0x%0x, rxblk_size :%d, rxpool_count :%d, rxblk_blks: 0x%0x\n",
				   poolhd->rxblk_addr, poolhd->rxblk_rdptr,
				   poolhd->rxblk_wrptr, poolhd->rxblk_size,
				   (int)(poolhd->rxblk_wrptr -
				   poolhd->rxblk_rdptr),
				   poolhd->rxblk_blks);
			seq_printf(m, "sblock pool header: txblk_addr :0x%0x, txblk_rdptr :0x%0x, txblk_wrptr :0x%0x, txblk_size :%d, txpool_count :%d, txblk_blks: 0x%0x\n",
				   poolhd->txblk_addr, poolhd->txblk_rdptr,
				   poolhd->txblk_wrptr, poolhd->txblk_size,
				   (int)(poolhd->txblk_wrptr -
				   poolhd->txblk_rdptr),
				   poolhd->txblk_blks);
		}
	}
	return 0;

}

static int sblock_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sblock_debug_show, inode->i_private);
}

static const struct file_operations sblock_debug_fops = {
	.open = sblock_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sblock_init_debugfs(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("sblock", S_IRUGO,
			    (struct dentry *)root,
			    NULL, &sblock_debug_fops);
	return 0;
}
EXPORT_SYMBOL_GPL(sblock_init_debugfs);

#endif /* CONFIG_DEBUG_FS */


MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBLOCK driver");
MODULE_LICENSE("GPL v2");
