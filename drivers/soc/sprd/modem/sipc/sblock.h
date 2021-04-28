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

#ifndef __SBLOCK_H
#define __SBLOCK_H

/* flag for CMD/DONE msg type */
#define SMSG_CMD_SBLOCK_INIT		0x1
#define SMSG_DONE_SBLOCK_INIT		0x2

/* flag for EVENT msg type */
#define SMSG_EVENT_SBLOCK_SEND		0x1
#define SMSG_EVENT_SBLOCK_RELEASE	0x2

#define SBLOCK_STATE_IDLE		0
#define SBLOCK_STATE_READY		1

#define SBLOCK_BLK_STATE_DONE		0
#define SBLOCK_BLK_STATE_PENDING	1

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
	int                     yell;	   /* need to notify cp */
	spinlock_t		r_txlock;  /* send */
	spinlock_t		r_rxlock;  /* recv */
	spinlock_t		p_txlock;  /* get */
	spinlock_t		p_rxlock;  /* release */

	wait_queue_head_t	getwait;
	wait_queue_head_t	recvwait;
};

struct sblock_mgr {
	u8		dst;
	u8		channel;
	u32		state;

	void		*smem_virt;
	u32		smem_addr;
	u32		smem_size;
	u32		mapped_smem_addr;

	u32		txblksz;
	u32		rxblksz;
	u32		txblknum;
	u32		rxblknum;

	struct sblock_ring	*ring;
	struct task_struct	*thread;

	void			(*handler)(int event, void *data);
	void			*data;
};

#ifdef CONFIG_64BIT
#define SBLOCK_ALIGN_BYTES (8)
#else
#define SBLOCK_ALIGN_BYTES (4)
#endif

static inline u32 sblock_get_index(u32 x, u32 y)
{
	return (x / y);
}

static inline u32 sblock_get_ringpos(u32 x, u32 y)
{
	return is_power_of_2(y) ? (x & (y - 1)) : (x % y);
}

#endif
