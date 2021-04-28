/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#ifndef _SPRD_DMA_COPY_K_H_
#define _SPRD_DMA_COPY_K_H_

typedef enum {
	DMA_COPY_YUV422 = 0,
	DMA_COPY_YUV420,
	DMA_COPY_YUV400,
	DMA_COPY_RGB888,
	DMA_COPY_RGB666,
	DMA_COPY_RGB565,
	DMA_COPY_RGB555,
	DMA_COPY_FMT_MAX
} DMA_COPY_DATA_FORMAT_E;

typedef struct _dma_copy_size_tag {
	uint32_t w;
	uint32_t h;
} DMA_COPY_SIZE_T;

typedef struct _dma_copy_rect_tag {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
} DMA_COPY_RECT_T;

typedef struct _dma_copy_addr_tag {
	uintptr_t y_addr;
	uintptr_t uv_addr;
} DMA_COPY_ADDR_T;

typedef struct _dma_copy_cfg_tag {
	DMA_COPY_DATA_FORMAT_E format;
	DMA_COPY_SIZE_T src_size;
	DMA_COPY_RECT_T src_rec;
	DMA_COPY_ADDR_T src_addr;
	DMA_COPY_ADDR_T dst_addr;
} DMA_COPY_CFG_T, *DMA_COPY_CFG_T_PTR;

#endif
