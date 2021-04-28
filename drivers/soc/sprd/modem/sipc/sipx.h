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

#ifndef __SIPX_H
#define __SIPX_H

#include "sblock.h"
#include <linux/hrtimer.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_SPRD_SIPC_MEM_CACHE_EN

/* Temporary workaround  */
#ifdef ioremap_cached
#define ioremap_cache	ioremap_cached
#endif

#ifdef CONFIG_64BIT

#ifdef CONFIG_ARM64

#define SIPC_DATA_TO_SKB_CACHE_INV(start, end) \
	     __inval_dcache_area(start,  (end-start))
#define SKB_DATA_TO_SIPC_CACHE_FLUSH(start, end) \
	__dma_flush_area(start,  (end-start))

#else /* x86 */

#define SIPC_DATA_TO_SKB_CACHE_INV(start, end) \
	clflush_cache_range(start,  (end - start))
#define SKB_DATA_TO_SIPC_CACHE_FLUSH(start, end) \
	clflush_cache_range(start,  (end - start))

#endif /* CONFIG_ARM64 */

#else /* CONFIG_64BIT */
#define SIPC_DATA_TO_SKB_CACHE_INV(start, end) \
	v7_dma_unmap_area(start, (end - start), DMA_FROM_DEVICE)

#define SKB_DATA_TO_SIPC_CACHE_FLUSH(start, end) \
	v7_dma_flush_range(start,  end)

#endif /* CONFIG_64BIT */

#endif /* CONFIG_SPRD_SIPC_MEM_CACHE_EN */

#define SIPX_STATE_IDLE		0
#define SIPX_STATE_READY	0x7c7d7e7f

struct sipx_mgr;

struct sipx_init_data {
	char *name;
	u8 dst;
	u32 dl_pool_size;
	u32 dl_ack_pool_size;
	u32 ul_pool_size;
	u32 ul_ack_pool_size;
};

struct sipx_blk {
	u32		addr; /*phy address*/
	u32		length;
	u16             index;
	u16             offset;
};

struct sipx_blk_item {
	u16		index; /* index in pools */

	/* bit0-bit10: valid length, bit11-bit15 valid offset */
	u16		desc;
} __packed;

struct sipx_fifo_info {
	u32		blks_addr;
	u32		blk_size;
	u32		fifo_size;
	u32		fifo_rdptr;
	u32		fifo_wrptr;
	u32		fifo_addr;
};

struct sipx_pool {
	volatile struct sipx_fifo_info *fifo_info;
	struct sipx_blk_item *fifo_buf;/* virt of info->fifo_addr */
	u32 fifo_size;
	void *blks_buf; /* virt of info->blks_addr */
	u32 blk_size;

	/* lock for sipx-pool */
	spinlock_t lock;

	struct sipx_mgr *sipx;
};

struct sipx_ring {
	volatile struct sipx_fifo_info *fifo_info;
	struct sipx_blk_item *fifo_buf;/* virt of info->fifo_addr */
	void *blks_buf; /* virt of info->blks_addr */
	u32 fifo_size;
	u32 blk_size;

	/* lock for sipx-ring */
	spinlock_t lock;

	struct sipx_mgr *sipx;
};

struct sipx_channel {
	u32		dst;
	u32		channel;
	u32		state;

	void		*smem_virt;
	u32		smem_addr;
	u32		mapped_smem_addr;
	u32		smem_size;

	struct sipx_ring        *dl_ring;
	struct sipx_ring        *dl_ack_ring;
	struct sipx_ring        *ul_ring;
	struct sipx_ring        *ul_ack_ring;

	u32			dl_record_flag;
	struct sblock           dl_record_blk;
	u32			ul_record_flag;
	struct sblock           ul_record_blk;

	/* lock for sipx-channel */
	spinlock_t              lock;
	struct hrtimer          ul_timer;
	int                     ul_timer_active;
	ktime_t                 ul_timer_val;

	struct task_struct	*thread;
	void			(*handler)(int event, void *data);
	void			*data;

	struct sipx_mgr *sipx;
};

struct sipx_mgr {
	u32 dst;
	u32 state;
	int recovery;

	struct sipx_init_data *pdata;	/* platform data */

	void *smem_virt;
	void *smem_cached_virt;
	u32 smem_addr;
	u32 mapped_smem_addr;
	u32 smem_size;
	void *dl_ack_start;
	void *ul_ack_start;

	u32 dl_pool_size;
	u32 dl_ack_pool_size;
	u32 ul_pool_size;
	u32 ul_ack_pool_size;
	u32 blk_size;
	u32 ack_blk_size;

	struct sipx_pool *dl_pool;
	struct sipx_pool *dl_ack_pool;
	struct sipx_pool *ul_pool;
	struct sipx_pool *ul_ack_pool;
	struct sipx_channel *channels[SMSG_VALID_CH_NR];
};

#endif /* !__SIPX_H */
