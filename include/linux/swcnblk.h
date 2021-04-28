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

#ifndef __SWCNBLK_H__
#define __SWCNBLK_H__

struct swcnblk_create_info {
	u8	dst;
	u8	channel;
	u32	txblocknum;
	u32	txblocksize;
	u32	rxblocknum;
	u32	rxblocksize;
	u32	basemem;
	u32	alignsize;
	u32	mapped_smem_base;
};

struct swcnblk_blk {
	void *addr;
	u32 length;
};

int swcnblk_create(struct swcnblk_create_info *info,
		   void (*handler)(int event, void *data),
		   void *data);
void swcnblk_destroy(u8 dst, u8 channel);
int swcnblk_register_notifier(u8 dst, u8 channel,
			      void (*handler)(int event, void *data),
			      void *data);
void swcnblk_put(u8 dst, u8 channel, struct swcnblk_blk *blk);
int swcnblk_get(u8 dst, u8 channel, struct swcnblk_blk *blk, int timeout);
int swcnblk_send(u8 dst, u8 channel, struct swcnblk_blk *blk);
int swcnblk_send_prepare(u8 dst, u8 channel, struct swcnblk_blk *blk);
int swcnblk_receive(u8 dst, u8 channel,
		    struct swcnblk_blk *blk, int timeout);
int swcnblk_get_arrived_count(u8 dst, u8 channel);
int swcnblk_get_free_count(u8 dst, u8 channel);
int swcnblk_release(u8 dst, u8 channel, struct swcnblk_blk *blk);
int swcnblk_query(u8 dst, u8 channel);
int swcnblk_get_cp_cache_range(u8 dst, u8 channel, u32 *addr, u32 *len);
#endif
