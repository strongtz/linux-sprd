/*
 * sound/soc/sprd/dai/sprd-dmaengine-pcm.h
 *
 * SpreadTrum DMA for the pcm stream.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
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
#ifndef __SPRD_DMA_ENGINE_PCM_H
#define __SPRD_DMA_ENGINE_PCM_H

#include <linux/dmaengine.h>
#include <linux/dma/sprd-dma.h>
#include "sprd_audio_dma.h"

#define VBC_FIFO_FRAME_NUM		(512)

#define VBC_BUFFER_BYTES_MAX		(64 * 1024)

#define I2S_BUFFER_BYTES_MAX	(64 * 1024)

#define AUDIO_BUFFER_BYTES_MAX	(VBC_BUFFER_BYTES_MAX + I2S_BUFFER_BYTES_MAX)


struct sprd_pcm_dma_params {
	char *name;		/* stream identifier */
	int channels[2];	/* channel id */
	int irq_type;		/* dma interrupt type */
	struct sprd_dma_cfg desc;	/* dma description struct */
	u32 dev_paddr[2];	/* device physical address for DMA */
	u32 use_mcdt; /* @@1 use mcdt, @@0 not use mcdt */
	char *used_dma_channel_name[2];
};

enum{
	DMA_TYPE_INVAL = -1,
	DMA_TYPE_AGCP,
	DMA_TYPE_MAX
};

struct audio_pm_dma {
	int no_pm_cnt;
	struct sprd_runtime_data *normal_rtd;
	struct notifier_block pm_nb;
	/* protect rtd->dma_chn */
	spinlock_t pm_splk_dma_prot;
	/* protect rtd->dma_chn */
	struct mutex pm_mtx_dma_prot;
	/* protect no_pm_cnt */
	struct mutex pm_mtx_cnt;
};
#endif /* __SPRD_DMA_ENGINE_PCM_H */
