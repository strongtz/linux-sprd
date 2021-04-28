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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sipc.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/swcnblk.h>
#include <uapi/linux/sched/types.h>

#include "sipc_priv.h"
#include "swcn_blk.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "swcnblk: " fmt

#define VOLA_SWCNBLK_RING volatile struct swcnblk_ring_header

#define SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk) \
	do { \
		u8 ch_index; \
		ch_index = sipc_channel2index(channel); \
		if (ch_index == INVALID_CHANEL_INDEX) { \
			pr_err("%s:channel %d invalid!\n", \
			       __func__, channel); \
			swcnblk = NULL; \
		} \
		swcnblk = swcnblks[dst][ch_index]; \
	} while (0)

static struct swcnblk_mgr *swcnblks[SIPC_ID_NR][SMSG_VALID_CH_NR];

void swcnblk_put(u8 dst, u8 channel, struct swcnblk_blk *blk)
{
	int txpos, index;
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;
	unsigned long flags;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk)
		return;

	ring = swcnblk->ring;
	poolhd = (VOLA_SWCNBLK_RING *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_txlock, flags);
	txpos = swcnblk_get_ringpos(poolhd->txblk_rdptr - 1,
				    poolhd->txblk_count);
	ring->r_txblks[txpos].addr = blk->addr -
				     swcnblk->smem_blk_virt +
				     swcnblk->mapped_smem_addr;
	ring->r_txblks[txpos].length = poolhd->txblk_size;
	poolhd->txblk_rdptr = poolhd->txblk_rdptr - 1;
	if ((int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr) == 1)
		wake_up_interruptible_all(&ring->getwait);
	index = swcnblk_get_index((blk->addr - ring->txblk_virt),
				  swcnblk->txblksz);
	ring->txrecord[index] = SWCNBLK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_txlock, flags);
}
EXPORT_SYMBOL(swcnblk_put);

static int swcnblk_recover(u8 dst, u8 channel)
{
	int i, j;
	unsigned long pflags, qflags;
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring = NULL;
	VOLA_SWCNBLK_RING *ringhd = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk)
		return -ENODEV;

	ring = swcnblk->ring;
	ringhd = (VOLA_SWCNBLK_RING *)(&ring->header->ring);
	poolhd = (VOLA_SWCNBLK_RING *)(&ring->header->pool);

	swcnblk->state = SWCNBLK_STATE_IDLE;
	wake_up_interruptible_all(&ring->getwait);
	wake_up_interruptible_all(&ring->recvwait);

	spin_lock_irqsave(&ring->r_txlock, pflags);
	/* clean txblks ring */
	ringhd->txblk_wrptr = ringhd->txblk_rdptr;

	spin_lock_irqsave(&ring->p_txlock, qflags);
	/* recover txblks pool */
	poolhd->txblk_rdptr = poolhd->txblk_wrptr;
	for (i = 0, j = 0; i < poolhd->txblk_count; i++) {
		if (ring->txrecord[i] == SWCNBLK_BLK_STATE_DONE) {
			ring->p_txblks[j].addr = i * swcnblk->txblksz +
						 poolhd->txblk_addr;
			ring->p_txblks[j].length = swcnblk->txblksz;
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
		if (ring->rxrecord[i] == SWCNBLK_BLK_STATE_DONE) {
			ring->p_rxblks[j].addr = i * swcnblk->rxblksz +
						 poolhd->rxblk_addr;
			ring->p_rxblks[j].length = swcnblk->rxblksz;
			poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
			j++;
		}
	}
	spin_unlock_irqrestore(&ring->p_rxlock, qflags);
	spin_unlock_irqrestore(&ring->r_rxlock, pflags);

	return 0;
}

static int swcnblk_thread(void *data)
{
	int rval;
	int recovery = 0;
	struct swcnblk_mgr *swcnblk = data;
	struct smsg mcmd, mrecv;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	/* since the channel open may hang, we call it in the swcnblk thread */
	rval = smsg_ch_open(swcnblk->dst, swcnblk->channel, -1);
	if (rval != 0) {
		pr_info("Failed to open channel %d\n",
			swcnblk->channel);
		/* assign NULL to thread poniter as failed to open channel */
		swcnblk->thread = NULL;
		return rval;
	}

	/* handle the swcnblk events */
	while (!kthread_should_stop()) {
		/* monitor swcnblk recv smsg */
		smsg_set(&mrecv, swcnblk->channel, 0, 0, 0);
		rval = smsg_recv(swcnblk->dst, &mrecv, -1);
		if (rval == -EIO || rval == -ENODEV) {
			/* channel state is FREE */
			msleep(20);
			continue;
		}
		pr_debug("swcnblk thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			 swcnblk->dst, swcnblk->channel,
			 mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* handle channel recovery */
			if (recovery) {
				if (swcnblk->handler)
					swcnblk->handler(SBLOCK_NOTIFY_CLOSE,
							swcnblk->data);
				swcnblk_recover(swcnblk->dst, swcnblk->channel);
			}
			smsg_open_ack(swcnblk->dst, swcnblk->channel);
			break;
		case SMSG_TYPE_CLOSE:
			/* handle channel recovery */
			smsg_close_ack(swcnblk->dst, swcnblk->channel);
			if (swcnblk->handler)
				swcnblk->handler(SBLOCK_NOTIFY_CLOSE,
						swcnblk->data);
			swcnblk->state = SWCNBLK_STATE_IDLE;
			break;
		case SMSG_TYPE_CMD:
			/* respond cmd done for swcnblk init */
			WARN_ON(mrecv.flag != SMSG_CMD_SWCNBLK_INIT);
			smsg_set(&mcmd,
				 swcnblk->channel,
				 SMSG_TYPE_DONE,
				 SMSG_DONE_SWCNBLK_INIT,
				 swcnblk->mapped_smem_addr);
			smsg_send(swcnblk->dst, &mcmd, -1);
			if (swcnblk->handler)
				swcnblk->handler(SBLOCK_NOTIFY_OPEN,
						swcnblk->data);
			swcnblk->state = SWCNBLK_STATE_READY;
			recovery = 1;
			break;
		case SMSG_TYPE_EVENT:
			/* handle swcnblk send/release events */
			switch (mrecv.flag) {
			case SMSG_EVENT_SWCNBLK_SEND:
				wake_up_interruptible_all(
						&swcnblk->ring->recvwait);
				if (swcnblk->handler)
					swcnblk->handler(SBLOCK_NOTIFY_RECV,
							 swcnblk->data);
				break;
			case SMSG_EVENT_SWCNBLK_RELEASE:
				wake_up_interruptible_all(
						&swcnblk->ring->getwait);
				if (swcnblk->handler)
					swcnblk->handler(SBLOCK_NOTIFY_GET,
							 swcnblk->data);
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
			pr_info("non-handled swcnblk msg: %d-%d, %d, %d, %d\n",
				swcnblk->dst, swcnblk->channel,
				mrecv.type, mrecv.flag, mrecv.value);
			rval = 0;
		}
	}

	pr_info("swcnblk %d-%d thread stop",
		swcnblk->dst, swcnblk->channel);
	return rval;
}

int swcnblk_create(struct swcnblk_create_info *info,
		   void (*handler)(int event, void *data),
		   void *data)
{
	u8 ch_index;
	int i, ret;
	u32 hsize, smem_base, smem_map;
	u32 txblocksize, rxblocksize;
	u32 hesize = 0;
	struct swcnblk_mgr *swcnblk = NULL;
	struct sipc_device *sdev;
	VOLA_SWCNBLK_RING *ringhd = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;

	if (!info) {
		pr_err("info is NULL\n");
		return -EINVAL;
	}

	ch_index = sipc_channel2index(info->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, info->channel);
		return -EINVAL;
	}
	if (info->dst >= SIPC_ID_NR) {
		pr_err("swcnblk_create_ex: dst = %d is invalid\n", info->dst);
		return -EINVAL;
	}
	sdev = sipc_ap.sipc_dev[info->dst];
	if (!sdev) {
		pr_err("swcnblk_create_ex: sdev is null, dst = %d\n",
		       info->dst);
		return -ENODEV;
	}

	swcnblk = kzalloc(sizeof(*swcnblk), GFP_KERNEL);
	if (!swcnblk)
		return -ENOMEM;

	swcnblk->state = SWCNBLK_STATE_IDLE;
	swcnblk->dst = info->dst;
	swcnblk->channel = info->channel;
	txblocksize = SWCNBLKSZ_ALIGN(info->txblocksize, SWCNBLK_ALIGN_BYTES);
	rxblocksize = SWCNBLKSZ_ALIGN(info->rxblocksize, SWCNBLK_ALIGN_BYTES);
	swcnblk->txblksz = txblocksize;
	swcnblk->rxblksz = rxblocksize;
	swcnblk->txblknum = info->txblocknum;
	swcnblk->rxblknum = info->rxblocknum;

	/* allocate smem */
	/* for blks */
	hesize = info->txblocknum * txblocksize +
		 info->rxblocknum * rxblocksize;
	if (info->alignsize > sizeof(struct swcnblk_header)) {
		hsize = info->alignsize;
		hesize = (hesize + info->alignsize) & (~(info->alignsize - 1));
	} else {
		hsize = sizeof(struct swcnblk_header);
	}
		/* for header*/
	swcnblk->smem_size = hsize + hesize +
		/* for ring */
		(info->txblocknum + info->rxblocknum) *
		sizeof(struct swcnblk_blks) +
		/* for pool */
		(info->txblocknum + info->rxblocknum) *
		sizeof(struct swcnblk_blks);

	if (info->basemem) {
		smem_base = info->basemem;
		smem_map = info->mapped_smem_base;
	} else {
		smem_base = sdev->pdata->smem_base;
		smem_map = sdev->pdata->mapped_smem_base;
	}

	swcnblk->smem_addr = smem_alloc_ex(swcnblk->smem_size, smem_base);
	if (!swcnblk->smem_addr) {
		pr_err("Failed to allocate smem for swcnblk\n");
		ret = -ENOMEM;
		goto err_smem_alloc;
	}
	swcnblk->mapped_smem_addr = swcnblk->smem_addr - smem_base + smem_map;
	swcnblk->mapped_cache_addr = swcnblk->mapped_smem_addr + hsize;
	swcnblk->mapped_cache_size = hesize;

	swcnblk->smem_virt = shmem_ram_vmap_nocache(swcnblk->smem_addr,
						    swcnblk->smem_size);
	if (!swcnblk->smem_virt) {
		pr_err("Failed to map smem for swcnblk\n");
		ret = -EFAULT;
		goto err_shmem_nocache;
	}

#ifdef SPRD_SWCN_MEM_CACHE_EN
	swcnblk->smem_cached_virt = shmem_ram_vmap_cache(swcnblk->smem_addr,
							 swcnblk->smem_size);

	if (!swcnblk->smem_cached_virt) {
		pr_err("Failed to map cached smem for sipx\n");
		ret = -EFAULT;
		goto err_shmem_cache;
	}
	swcnblk->smem_blk_virt = swcnblk->smem_cached_virt;
#else
	swcnblk->smem_blk_virt = swcnblk->smem_virt;
#endif

	/* initialize ring and header */
	swcnblk->ring = kzalloc(sizeof(*swcnblk->ring), GFP_KERNEL);
	if (!swcnblk->ring) {
		ret = -ENOMEM;
		goto err_zalloc_ring;
	}
	ringhd = (VOLA_SWCNBLK_RING *)(swcnblk->smem_virt);
	ringhd->txblk_addr = swcnblk->mapped_smem_addr + hsize;
	ringhd->txblk_count = info->txblocknum;
	ringhd->txblk_size = txblocksize;
	ringhd->txblk_rdptr = 0;
	ringhd->txblk_wrptr = 0;
	ringhd->txblk_blks = ringhd->txblk_addr + hesize;
	ringhd->rxblk_addr = ringhd->txblk_addr +
			     info->txblocknum * txblocksize;
	ringhd->rxblk_count = info->rxblocknum;
	ringhd->rxblk_size = rxblocksize;
	ringhd->rxblk_rdptr = 0;
	ringhd->rxblk_wrptr = 0;
	ringhd->rxblk_blks = ringhd->txblk_blks +
			     info->txblocknum * sizeof(struct swcnblk_blks);

	poolhd = (VOLA_SWCNBLK_RING *)(swcnblk->smem_virt +
					sizeof(struct swcnblk_ring_header));
	poolhd->txblk_addr = swcnblk->mapped_smem_addr + hsize;
	poolhd->txblk_count = info->txblocknum;
	poolhd->txblk_size = txblocksize;
	poolhd->txblk_rdptr = 0;
	poolhd->txblk_wrptr = 0;
	poolhd->txblk_blks = ringhd->rxblk_blks +
			     info->rxblocknum * sizeof(struct swcnblk_blks);
	poolhd->rxblk_addr = ringhd->txblk_addr +
			     info->txblocknum * txblocksize;
	poolhd->rxblk_count = info->rxblocknum;
	poolhd->rxblk_size = rxblocksize;
	poolhd->rxblk_rdptr = 0;
	poolhd->rxblk_wrptr = 0;
	poolhd->rxblk_blks = poolhd->txblk_blks +
			     info->txblocknum * sizeof(struct swcnblk_blks);

	swcnblk->ring->txrecord = kcalloc(info->txblocknum,
					  sizeof(int), GFP_KERNEL);
	if (!swcnblk->ring->txrecord) {
		ret = -ENOMEM;
		goto err_calloc_txrecord;
	}

	swcnblk->ring->rxrecord = kcalloc(info->rxblocknum,
					  sizeof(int), GFP_KERNEL);
	if (!swcnblk->ring->rxrecord) {
		ret = -ENOMEM;
		goto err_calloc_rxrecord;
	}

	swcnblk->ring->header = swcnblk->smem_virt;
	swcnblk->ring->txblk_virt = swcnblk->smem_blk_virt +
		(ringhd->txblk_addr - swcnblk->mapped_smem_addr);
	swcnblk->ring->r_txblks = swcnblk->smem_virt +
		(ringhd->txblk_blks - swcnblk->mapped_smem_addr);
	swcnblk->ring->rxblk_virt = swcnblk->smem_blk_virt +
		(ringhd->rxblk_addr - swcnblk->mapped_smem_addr);
	swcnblk->ring->r_rxblks = swcnblk->smem_virt +
		(ringhd->rxblk_blks - swcnblk->mapped_smem_addr);
	swcnblk->ring->p_txblks = swcnblk->smem_virt +
		(poolhd->txblk_blks - swcnblk->mapped_smem_addr);
	swcnblk->ring->p_rxblks = swcnblk->smem_virt +
		(poolhd->rxblk_blks - swcnblk->mapped_smem_addr);

	for (i = 0; i < info->txblocknum; i++) {
		swcnblk->ring->p_txblks[i].addr = poolhd->txblk_addr +
						 i * txblocksize;
		swcnblk->ring->p_txblks[i].length = txblocksize;
		swcnblk->ring->txrecord[i] = SWCNBLK_BLK_STATE_DONE;
		poolhd->txblk_wrptr++;
	}
	for (i = 0; i < info->rxblocknum; i++) {
		swcnblk->ring->p_rxblks[i].addr = poolhd->rxblk_addr +
						 i * rxblocksize;
		swcnblk->ring->p_rxblks[i].length = rxblocksize;
		swcnblk->ring->rxrecord[i] = SWCNBLK_BLK_STATE_DONE;
		poolhd->rxblk_wrptr++;
	}

	init_waitqueue_head(&swcnblk->ring->getwait);
	init_waitqueue_head(&swcnblk->ring->recvwait);
	spin_lock_init(&swcnblk->ring->r_txlock);
	spin_lock_init(&swcnblk->ring->r_rxlock);
	spin_lock_init(&swcnblk->ring->p_txlock);
	spin_lock_init(&swcnblk->ring->p_rxlock);

	swcnblk->thread = kthread_create(swcnblk_thread, swcnblk,
					 "swcnblk-%d-%d", info->dst,
					 info->channel);
	if (IS_ERR(swcnblk->thread)) {
		pr_err("Failed to create kthread: swcnblk-%d-%d\n",
		       info->dst, info->channel);
		ret = PTR_ERR(swcnblk->thread);
		goto err_kthread;
	}

	swcnblks[info->dst][ch_index] = swcnblk;
	if (handler && data) {
		ret = swcnblk_register_notifier(info->dst, info->channel,
						handler, data);
		if (ret < 0) {
			swcnblk_destroy(info->dst, info->channel);
			return ret;
		}
	}
	wake_up_process(swcnblk->thread);
	return 0;

err_kthread:
	kfree(swcnblk->ring->rxrecord);
err_calloc_rxrecord:
	kfree(swcnblk->ring->txrecord);
err_calloc_txrecord:
	kfree(swcnblk->ring);
err_zalloc_ring:
#ifdef SPRD_SWCN_MEM_CACHE_EN
	shmem_ram_unmap(swcnblk->smem_cached_virt);
err_shmem_cache:
#endif
	shmem_ram_unmap(swcnblk->smem_virt);
err_shmem_nocache:
	smem_free(swcnblk->smem_addr, swcnblk->smem_size);
err_smem_alloc:
	kfree(swcnblk);

	return ret;
}
EXPORT_SYMBOL(swcnblk_create);

void swcnblk_destroy(u8 dst, u8 channel)
{
	struct swcnblk_mgr *swcnblk;
	u8 ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d invalid!\n", __func__, channel);
		return;
	}

	swcnblk = swcnblks[dst][ch_index];
	if (!swcnblk)
		return;

	swcnblk->state = SWCNBLK_STATE_IDLE;
	smsg_ch_close(dst, channel, -1);

	/* stop swcnblk thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(swcnblk->thread))
		kthread_stop(swcnblk->thread);

	if (swcnblk->ring) {
		wake_up_interruptible_all(&swcnblk->ring->recvwait);
		wake_up_interruptible_all(&swcnblk->ring->getwait);
		kfree(swcnblk->ring->txrecord);
		kfree(swcnblk->ring->rxrecord);
		kfree(swcnblk->ring);
	}
#ifdef SPRD_SWCN_MEM_CACHE_EN
	if (swcnblk->smem_cached_virt)
		shmem_ram_unmap(swcnblk->smem_cached_virt);
#endif
	if (swcnblk->smem_virt)
		shmem_ram_unmap(swcnblk->smem_virt);

	smem_free(swcnblk->smem_addr, swcnblk->smem_size);
	kfree(swcnblk);

	swcnblks[dst][ch_index] = NULL;
}
EXPORT_SYMBOL(swcnblk_destroy);

int swcnblk_register_notifier(u8 dst, u8 channel,
			      void (*handler)(int event, void *data),
			      void *data)
{
	struct swcnblk_mgr *swcnblk;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk) {
		pr_err("%s:swcnblk-%d-%d not ready!\n", __func__, dst, channel);
		return -ENODEV;
	}

	swcnblk->handler = handler;
	swcnblk->data = data;

	return 0;
}
EXPORT_SYMBOL(swcnblk_register_notifier);

int swcnblk_get(u8 dst, u8 channel, struct swcnblk_blk *blk, int timeout)
{
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring = NULL;
	VOLA_SWCNBLK_RING *ringhd = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;
	int txpos, index;
	int rval = 0;
	unsigned long flags;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY) {
		pr_err("%s:swcnblk-%d-%d not ready!\n",
		       __func__, dst, channel);
		return swcnblk ? -EIO : -ENODEV;
	}

	ring = swcnblk->ring;
	ringhd = (VOLA_SWCNBLK_RING *)(&ring->header->ring);
	poolhd = (VOLA_SWCNBLK_RING *)(&ring->header->pool);

	if (poolhd->txblk_rdptr == poolhd->txblk_wrptr) {
		if (timeout == 0) {
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(
					ring->getwait,
					poolhd->txblk_rdptr !=
					poolhd->txblk_wrptr ||
					swcnblk->state == SWCNBLK_STATE_IDLE);
			if (rval < 0)
				pr_info("%s wait interrupted!\n",
					__func__);

			if (swcnblk->state == SWCNBLK_STATE_IDLE) {
				pr_err("%s swcnblk state is idle!\n",
				       __func__);
				rval = -EIO;
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(
					ring->getwait,
					poolhd->txblk_rdptr !=
					poolhd->txblk_wrptr ||
					swcnblk == SWCNBLK_STATE_IDLE, timeout);
			if (rval < 0) {
				pr_info("%s wait interrupted!\n",
					__func__);
			} else if (rval == 0) {
				pr_info("%s wait timeout!\n",
					__func__);
				rval = -ETIME;
			}

			if (swcnblk->state == SWCNBLK_STATE_IDLE) {
				pr_info("%s swcnblk state is idle!\n",
					__func__);
				rval = -EIO;
			}
		}
	}

	if (rval < 0)
		return rval;

	/* multi-gotter may cause got failure */
	spin_lock_irqsave(&ring->p_txlock, flags);
	if (poolhd->txblk_rdptr != poolhd->txblk_wrptr &&
	    swcnblk->state == SWCNBLK_STATE_READY) {
		txpos = swcnblk_get_ringpos(poolhd->txblk_rdptr,
					    poolhd->txblk_count);
		blk->addr = swcnblk->smem_blk_virt +
			    (ring->p_txblks[txpos].addr -
			     swcnblk->mapped_smem_addr);
		blk->length = poolhd->txblk_size;
		poolhd->txblk_rdptr = poolhd->txblk_rdptr + 1;
		index = swcnblk_get_index((blk->addr - ring->txblk_virt),
					  swcnblk->txblksz);
		ring->txrecord[index] = SWCNBLK_BLK_STATE_PENDING;
	} else {
		rval = swcnblk->state == SWCNBLK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->p_txlock, flags);

	return rval;
}
EXPORT_SYMBOL(swcnblk_get);

static int swcnblk_send_ex(u8 dst, u8 channel,
			   struct swcnblk_blk *blk, bool yell)
{
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring;
	VOLA_SWCNBLK_RING *ringhd;
	struct smsg mevt;
	int txpos, index;
	int rval = 0;
	unsigned long flags;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY) {
		pr_err("%s:swcnblk-%d-%d not ready!\n",
		       __func__, dst, channel);
		return swcnblk ? -EIO : -ENODEV;
	}

	pr_debug("swcnblk_send: dst=%d, channel=%d, addr=%p, len=%d\n",
		 dst, channel, blk->addr, blk->length);

#ifdef SPRD_SWCN_MEM_CACHE_EN
	SKB_DATA_TO_SWCN_CACHE_FLUSH(blk->addr, blk->addr + blk->length);
#endif
	ring = swcnblk->ring;
	ringhd = (VOLA_SWCNBLK_RING *)(&ring->header->ring);

	spin_lock_irqsave(&ring->r_txlock, flags);

	txpos = swcnblk_get_ringpos(ringhd->txblk_wrptr, ringhd->txblk_count);
	ring->r_txblks[txpos].addr = blk->addr -
				     swcnblk->smem_blk_virt +
				     swcnblk->mapped_smem_addr;
	ring->r_txblks[txpos].length = blk->length;
	pr_debug("swcnblk_send: channel=%d, wrptr=%d, txpos=%d, addr=%x\n",
		 channel, ringhd->txblk_wrptr,
		 txpos, ring->r_txblks[txpos].addr);
	ringhd->txblk_wrptr = ringhd->txblk_wrptr + 1;

	if (swcnblk->state == SWCNBLK_STATE_READY) {
		if (yell || ((int)(ringhd->txblk_wrptr -
		    ringhd->txblk_rdptr) == 1)) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SWCNBLK_SEND,
				 0);
			rval = smsg_send(dst, &mevt, 0);
		}
	}
	index = swcnblk_get_index((blk->addr - ring->txblk_virt),
				  swcnblk->txblksz);
	ring->txrecord[index] = SWCNBLK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->r_txlock, flags);

	return rval;
}

int swcnblk_send(u8 dst, u8 channel, struct swcnblk_blk *blk)
{
	return swcnblk_send_ex(dst, channel, blk, true);
}
EXPORT_SYMBOL(swcnblk_send);

int swcnblk_send_prepare(u8 dst, u8 channel, struct swcnblk_blk *blk)
{
	return swcnblk_send_ex(dst, channel, blk, false);
}
EXPORT_SYMBOL(swcnblk_send_prepare);

int swcnblk_receive(u8 dst, u8 channel,
		    struct swcnblk_blk *blk, int timeout)
{
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring;
	VOLA_SWCNBLK_RING *ringhd;
	int rxpos, index, rval = 0;
	unsigned long flags;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY) {
		pr_err("%s:swcnblk-%d-%d not ready!\n",
		       __func__, dst, channel);
		return swcnblk ? -EIO : -ENODEV;
	}

	ring = swcnblk->ring;
	ringhd = (VOLA_SWCNBLK_RING *)(&ring->header->ring);

	pr_debug("swcnblk receive: dst=%d, channel=%d, timeout=%d\n",
		 dst, channel, timeout);
	pr_debug("swcnblk receive: channel=%d, wrptr=%d, rdptr=%d\n",
		 channel, ringhd->rxblk_wrptr, ringhd->rxblk_rdptr);

	if (ringhd->rxblk_wrptr == ringhd->rxblk_rdptr) {
		if (timeout == 0) {
			/* no wait */
			pr_debug("swcnblk receive: %d-%d is empty!\n",
				 dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->recvwait,
							ringhd->rxblk_wrptr !=
							ringhd->rxblk_rdptr);
			if (rval < 0)
				pr_info("%s wait interrupted!\n", __func__);

			if (swcnblk->state == SWCNBLK_STATE_IDLE) {
				pr_info("%s swcnblk state is idle!\n",
					__func__);
				rval = -EIO;
			}

		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(
				ring->recvwait,
				ringhd->rxblk_wrptr != ringhd->rxblk_rdptr,
				timeout);
			if (rval < 0) {
				pr_info("%s wait interrupted!\n", __func__);
			} else if (rval == 0) {
				pr_info("%s wait timeout!\n", __func__);
				rval = -ETIME;
			}

			if (swcnblk->state == SWCNBLK_STATE_IDLE) {
				pr_info("%s swcnblk state is idle!\n",
					__func__);
				rval = -EIO;
			}
		}
	}

	if (rval < 0)
		return rval;

	/* multi-receiver may cause recv failure */
	spin_lock_irqsave(&ring->r_rxlock, flags);

	if (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr &&
	    swcnblk->state == SWCNBLK_STATE_READY) {
		rxpos = swcnblk_get_ringpos(ringhd->rxblk_rdptr,
					    ringhd->rxblk_count);
		blk->addr = ring->r_rxblks[rxpos].addr -
			    swcnblk->mapped_smem_addr +
			    swcnblk->smem_blk_virt;
		blk->length = ring->r_rxblks[rxpos].length;
		ringhd->rxblk_rdptr = ringhd->rxblk_rdptr + 1;
		pr_debug("swcnblk receive: channel=%d, rxpos=%d, addr=%p, len=%d\n",
			 channel, rxpos, blk->addr, blk->length);
		index = swcnblk_get_index((blk->addr - ring->rxblk_virt),
					  swcnblk->rxblksz);
		ring->rxrecord[index] = SWCNBLK_BLK_STATE_PENDING;
	} else {
		rval = swcnblk->state == SWCNBLK_STATE_READY ? -EAGAIN : -EIO;
	}
	spin_unlock_irqrestore(&ring->r_rxlock, flags);
#ifdef SPRD_SWCN_MEM_CACHE_EN
	if (!rval)
		SWCN_DATA_TO_SKB_CACHE_INV(blk->addr, blk->addr + blk->length);
#endif

	return rval;
}
EXPORT_SYMBOL(swcnblk_receive);

int swcnblk_get_arrived_count(u8 dst, u8 channel)
{
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring = NULL;
	VOLA_SWCNBLK_RING *ringhd = NULL;
	int blk_count = 0;
	unsigned long flags;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY) {
		pr_err("%s:swcnblk-%d-%d not ready!\n",
		       __func__, dst, channel);
		return swcnblk ? -EIO : -ENODEV;
	}

	ring = swcnblk->ring;
	ringhd = (VOLA_SWCNBLK_RING *)(&ring->header->ring);

	spin_lock_irqsave(&ring->r_rxlock, flags);
	blk_count = (int)(ringhd->rxblk_wrptr - ringhd->rxblk_rdptr);
	spin_unlock_irqrestore(&ring->r_rxlock, flags);

	return blk_count;
}
EXPORT_SYMBOL(swcnblk_get_arrived_count);

int swcnblk_get_free_count(u8 dst, u8 channel)
{
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;
	int blk_count = 0;
	unsigned long flags;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY) {
		pr_err("%s:swcnblk-%d-%d not ready!\n",
		       __func__, dst, channel);
		return swcnblk ? -EIO : -ENODEV;
	}

	ring = swcnblk->ring;
	poolhd = (VOLA_SWCNBLK_RING *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_txlock, flags);
	blk_count = (int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr);
	spin_unlock_irqrestore(&ring->p_txlock, flags);

	return blk_count;
}
EXPORT_SYMBOL(swcnblk_get_free_count);

int swcnblk_release(u8 dst, u8 channel, struct swcnblk_blk *blk)
{
	struct swcnblk_mgr *swcnblk;
	struct swcnblk_ring *ring = NULL;
	VOLA_SWCNBLK_RING *ringhd = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;
	struct smsg mevt;
	unsigned long flags;
	int rxpos;
	int index;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY) {
		pr_err("%s:swcnblk-%d-%d not ready!\n",
		       __func__, dst, channel);
		return swcnblk ? -EIO : -ENODEV;
	}

	pr_debug("swcnblk release: dst=%d, channel=%d, addr=%p, len=%d\n",
		 dst, channel, blk->addr, blk->length);

	ring = swcnblk->ring;
	ringhd = (VOLA_SWCNBLK_RING *)(&ring->header->ring);
	poolhd = (VOLA_SWCNBLK_RING *)(&ring->header->pool);

	spin_lock_irqsave(&ring->p_rxlock, flags);
	rxpos = swcnblk_get_ringpos(poolhd->rxblk_wrptr, poolhd->rxblk_count);
	ring->p_rxblks[rxpos].addr = blk->addr -
				     swcnblk->smem_blk_virt +
				     swcnblk->mapped_smem_addr;
	ring->p_rxblks[rxpos].length = poolhd->rxblk_size;
	poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
	pr_debug("swcnblk release: addr=%x\n", ring->p_rxblks[rxpos].addr);

	if ((int)(poolhd->rxblk_wrptr - poolhd->rxblk_rdptr) == 1 &&
	    swcnblk->state == SWCNBLK_STATE_READY) {
		/* send smsg to notify the peer side */
		smsg_set(&mevt, channel,
			 SMSG_TYPE_EVENT,
			 SMSG_EVENT_SWCNBLK_RELEASE,
			 0);
		smsg_send(dst, &mevt, -1);
	}

	index = swcnblk_get_index((blk->addr - ring->rxblk_virt),
				  swcnblk->rxblksz);
	ring->rxrecord[index] = SWCNBLK_BLK_STATE_DONE;

	spin_unlock_irqrestore(&ring->p_rxlock, flags);

	return 0;
}
EXPORT_SYMBOL(swcnblk_release);

int swcnblk_poll_wait(u8 dst, u8 channel, struct file *filp, poll_table *wait)
{
	struct swcnblk_mgr *swcnblk = NULL;
	struct swcnblk_ring *ring = NULL;
	VOLA_SWCNBLK_RING *ringhd = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;
	unsigned int mask = 0;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY) {
		pr_err("%s:swcnblk-%d-%d not ready!\n",
		       __func__, dst, channel);
		return swcnblk ? -EIO : -ENODEV;
	}
	ring = swcnblk->ring;
	ringhd = &ring->header->ring;
	poolhd = &ring->header->pool;
	poll_wait(filp, &ring->recvwait, wait);
	poll_wait(filp, &ring->getwait, wait);
	if (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr)
		mask |= POLLIN | POLLRDNORM;
	if (poolhd->txblk_rdptr != poolhd->txblk_wrptr)
		mask |= POLLOUT | POLLWRNORM;
	return mask;
}
EXPORT_SYMBOL(swcnblk_poll_wait);

int swcnblk_query(u8 dst, u8 channel)
{
	struct swcnblk_mgr *swcnblk;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk || swcnblk->state != SWCNBLK_STATE_READY)
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL(swcnblk_query);

int swcnblk_get_cp_cache_range(u8 dst, u8 channel, u32 *addr, u32 *len)
{
	struct swcnblk_mgr *swcnblk = NULL;

	SWCNBLK_GET_BLK_MGR(dst, channel, swcnblk);
	if (!swcnblk)
		return -ENODEV;
	if (swcnblk->mapped_cache_addr && swcnblk->mapped_cache_size) {
		*addr = swcnblk->mapped_cache_addr;
		*len = swcnblk->mapped_cache_size;
	} else {
		return -EAGAIN;
	}

	return 0;
}
EXPORT_SYMBOL(swcnblk_get_cp_cache_range);

#if defined(CONFIG_DEBUG_FS)
static int swcnblk_debug_show(struct seq_file *m, void *private)
{
	struct swcnblk_mgr *swcnblk = NULL;
	struct swcnblk_ring  *ring = NULL;
	VOLA_SWCNBLK_RING *ringhd = NULL;
	VOLA_SWCNBLK_RING *poolhd = NULL;
	int i, j;

	for (i = 0; i < SIPC_ID_NR; i++) {
		for (j = 0;  j < SMSG_VALID_CH_NR; j++) {
			swcnblk = swcnblks[i][j];
			if (!swcnblk)
				continue;
			ring = swcnblk->ring;
			ringhd = (VOLA_SWCNBLK_RING *)
				 (&swcnblk->ring->header->ring);
			poolhd = (VOLA_SWCNBLK_RING *)
				 (&swcnblk->ring->header->pool);

			seq_puts(m, "***************************************************************************************************************************\n");
			seq_printf(m, "swcnblk dst %d, channel: %3d, state: %d, smem_virt: 0x%lx, smem_cached_virt: 0x%lx smem_blk_virt: 0x%lx smem_addr: 0x%0x, mapped_smem_addr: 0x%0x, smem_size: 0x%0x, txblksz: %d, rxblksz: %d\n",
				   swcnblk->dst,
				   swcnblk->channel,
				   swcnblk->state,
				   (unsigned long)swcnblk->smem_virt,
				   (unsigned long)swcnblk->smem_cached_virt,
				   (unsigned long)swcnblk->smem_blk_virt,
				   swcnblk->smem_addr,
				   swcnblk->mapped_smem_addr,
				   swcnblk->smem_size,
				   swcnblk->txblksz,
				   swcnblk->rxblksz);
			seq_printf(m, "swcnblk ring: txblk_virt :0x%lx, rxblk_virt :0x%lx\n",
				   (unsigned long)ring->txblk_virt,
				   (unsigned long)ring->rxblk_virt);
			seq_printf(m, "swcnblk ring header: rxblk_addr :0x%0x, rxblk_rdptr :0x%0x, rxblk_wrptr :0x%0x, rxblk_size :%d, rxblk_count :%d, rxblk_blks: 0x%0x\n",
				   ringhd->rxblk_addr, ringhd->rxblk_rdptr,
				   ringhd->rxblk_wrptr, ringhd->rxblk_size,
				   (int)(ringhd->rxblk_wrptr -
				   ringhd->rxblk_rdptr),
				   ringhd->rxblk_blks);
			seq_printf(m, "swcnblk ring header: txblk_addr :0x%0x, txblk_rdptr :0x%0x, txblk_wrptr :0x%0x, txblk_size :%d, txblk_count :%d, txblk_blks: 0x%0x\n",
				   ringhd->txblk_addr, ringhd->txblk_rdptr,
				   ringhd->txblk_wrptr, ringhd->txblk_size,
				   (int)(ringhd->txblk_wrptr -
				   ringhd->txblk_rdptr),
				   ringhd->txblk_blks);
			seq_printf(m, "swcnblk pool header: rxblk_addr :0x%0x, rxblk_rdptr :0x%0x, rxblk_wrptr :0x%0x, rxblk_size :%d, rxpool_count :%d, rxblk_blks: 0x%0x\n",
				   poolhd->rxblk_addr, poolhd->rxblk_rdptr,
				   poolhd->rxblk_wrptr, poolhd->rxblk_size,
				   (int)(poolhd->rxblk_wrptr -
				   poolhd->rxblk_rdptr),
				   poolhd->rxblk_blks);
			seq_printf(m, "swcnblk pool header: txblk_addr :0x%0x, txblk_rdptr :0x%0x, txblk_wrptr :0x%0x, txblk_size :%d, txpool_count :%d, txblk_blks: 0x%0x\n",
				   poolhd->txblk_addr, poolhd->txblk_rdptr,
				   poolhd->txblk_wrptr, poolhd->txblk_size,
				   (int)(poolhd->txblk_wrptr -
				   poolhd->txblk_rdptr),
				   poolhd->txblk_blks);
		}
	}

	return 0;
}

static int swcnblk_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, swcnblk_debug_show, inode->i_private);
}

static const struct file_operations swcnblk_debug_fops = {
	.open = swcnblk_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int swcnblk_init_debugfs(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("swcnblk", S_IRUGO,
			    (struct dentry *)root,
			    NULL, &swcnblk_debug_fops);
	return 0;
}
EXPORT_SYMBOL_GPL(swcnblk_init_debugfs);

#endif /* CONFIG_DEBUG_FS */

struct swcnblk_init_data {
	char            *name;
	struct swcnblk_create_info info;
};

static int swcnblk_parse_dt(struct swcnblk_init_data **init, struct device *dev)
{
	int ret;
	u32 data;
	struct device_node *np = dev->of_node;
	struct swcnblk_init_data *pdata;
	struct swcnblk_create_info *info;

	pdata = devm_kzalloc(dev, sizeof(struct swcnblk_init_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	info = &pdata->info;

	ret = of_property_read_string(np, "sprd,name",
				      (const char **)&pdata->name);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,dst", (u32 *)&data);
	if (ret)
		goto error;
	info->dst = (u8)data;
	ret = of_property_read_u32(np, "sprd,channel", (u32 *)&data);
	if (ret)
		goto error;
	info->channel = (u8)data;
	ret = of_property_read_u32(np, "sprd,tx-blksize",
				   (u32 *)&info->txblocksize);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,tx-blknum",
				   (u32 *)&info->txblocknum);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,rx-blksize",
				   (u32 *)&info->rxblocksize);
	if (ret)
		goto error;
	ret = of_property_read_u32(np, "sprd,rx-blknum",
				   (u32 *)&info->rxblocknum);
	if (ret)
		goto error;

	if (!of_property_read_u32(np, "sprd,basemem", (u32 *)&data))
		info->basemem = data;

	if (!of_property_read_u32(np, "sprd,alignsize", (u32 *)&data))
		info->alignsize = data;

	*init = pdata;
	return ret;
error:
	devm_kfree(dev, pdata);
	*init = NULL;
	return ret;
}

static int swcnblk_probe(struct platform_device *pdev)
{
	int rval;
	struct swcnblk_init_data *init = pdev->dev.platform_data;

	if (!init && pdev->dev.of_node) {
		rval = swcnblk_parse_dt(&init, &pdev->dev);
		if (rval) {
			pr_err("Failed to parse swcnblk device tree, ret=%d\n",
			       rval);
			return rval;
		}
	}

	rval = swcnblk_create(&init->info, NULL, NULL);
	if (rval != 0) {
		pr_info("Failed to create swcnblk: %d\n", rval);
		devm_kfree(&pdev->dev, init);
		return rval;
	}

	platform_set_drvdata(pdev, init);

	return 0;
}

static int swcnblk_remove(struct platform_device *pdev)
{
	struct swcnblk_init_data *init = platform_get_drvdata(pdev);

	swcnblk_destroy(init->info.dst, init->info.channel);
	devm_kfree(&pdev->dev, init);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id swcnblk_match_table[] = {
	{.compatible = "sprd,swcnblk", },
	{ },
};

static struct platform_driver swcnblk_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "swcnblk",
		.of_match_table = swcnblk_match_table,
	},
	.probe = swcnblk_probe,
	.remove = swcnblk_remove,
};

static int __init swcnblk_init(void)
{
	return platform_driver_register(&swcnblk_driver);
}

static void __exit swcnblk_exit(void)
{
	platform_driver_unregister(&swcnblk_driver);
}

module_init(swcnblk_init);
module_exit(swcnblk_exit);

MODULE_AUTHOR("Jingxiang.Li");
MODULE_DESCRIPTION("SIPC/SWCNBLK driver");
MODULE_LICENSE("GPL");
