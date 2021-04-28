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
#ifndef _VIDEO_GSP_CFG_H
#define _VIDEO_GSP_CFG_H

#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <uapi/drm/gsp_cfg.h>

struct gsp_buf {
	size_t size;
	struct dma_buf *dmabuf;
	int is_iova;
};

struct gsp_buf_map {
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
	enum dma_data_direction dir;
};

struct gsp_mem_data {
	int share_fd;
	u32 uv_offset;
	u32 v_offset;
	struct gsp_buf buf;
	struct gsp_buf_map map;
};

struct gsp_layer {
	int type;
	int enable;
	struct list_head list;
	int wait_fd;
	int sig_fd;

	int filled;
	struct gsp_addr_data src_addr;
	struct gsp_mem_data mem_data;
};

struct gsp_cfg {
	int layer_num;
	int init;
	int tag;
	struct list_head layers;
	struct gsp_kcfg *kcfg;
	unsigned long frame_cnt;
};

struct COEF_ENTRY_T {
	struct list_head list;
	uint16_t in_w;
	uint16_t in_h;
	uint16_t out_w;
	uint16_t out_h;
	uint16_t hor_tap;
	uint16_t ver_tap;
	uint32_t coef[64 + 64];
};

#endif
