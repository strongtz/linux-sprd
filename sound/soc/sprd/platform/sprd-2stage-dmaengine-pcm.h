/*
 * sound/soc/sprd/dai/sprd-dmaengine-pcm.h
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


#define MAX_32_BIT          (0xffffffff)


#define VBC_PCM_FORMATS			SNDRV_PCM_FMTBIT_S16_LE
#define VBC_FIFO_FRAME_NUM (80)

#define TWO_STAGE_NORMAL BIT(0)
#define TWO_STAGE_DEEP BIT(1)
#define TWO_STAGE_CAPTURE BIT(2)

#define COUNT_IDX_OFFSET_NORMAL (0x0)
#define COUNT_MAX_OFFSET_NORMAL (0x4)
#define COUNT_IDX_OFFSET_DEPBUF (0x8)
#define COUNT_MAX_OFFSET_DEPBUF (0xc)
#define TOTAL_CNT_OFFSET_NORMAL (0x10)
#define TOTAL_CNT_OFFSET_DEPBUF (0x14)
#define SPRD_PCM_SYNC_AP        (0x18)
#define SPRD_PCM_SYNC_CM4       (0x1c)

enum {
	PROC_POINTER_LOG = 0,
	PROC_MAX,
};

enum {
	PLY_DEEPBUF = 0,
	PLY_NORMAL = 1,
	PLY_MAX,
};

enum {
	DMA_2STAGE_INT_SOURCE_NONE = 0,
	/* from arm7 or cm4 */
	DMA_2STAGE_INT_SOURCE_ARM = 1,
	/* from ap */
	DMA_2STAGE_INT_SOURCE_AP = 2,
};

enum DMA_STAGE {
	DMA_STAGE_ONE = 1,
	DMA_STAGE_TWO = 2,
};

enum DMA_LEVEL {
	DMA_LEVEL_1 = 1,
	DMA_LEVEL_2 = 2,
	DMA_LEVEL_ONESTAGE = 3,
};

struct audio_pm_dma {
	int no_pm_cnt;
	struct sprd_runtime_data *normal_rtd;
	struct notifier_block pm_nb;
	bool vbc_forcec_cleared;
	/* protect rtd->dma_chn */
	spinlock_t pm_splk_dma_prot;
	/* protect rtd->dma_chn */
	struct mutex pm_mtx_dma_prot;
	/* protect no_pm_cnt */
	struct mutex pm_mtx_cnt;
};

struct platform_pcm_priv {
	unsigned long platform_type;
	u32 use_2stage_dma_case;
	u32 node_count_2stage_level1;
	u32 dma_2stage_level_1_int_source;
	u32 dma_2stage_level_1_ap_int_count;
	u32 iram_phy_addr;
	char *iram_virt_addr;
	u32 iram_size;
	u32 iram_normal_phy_addr;
	char *iram_normal_virt_addr;
	u32 iram_normal_size;
	u32 iram_deepbuf_phy_addr;
	char *iram_deepbuf_virt_addr;
	u32 iram_deepbuf_size;
	u32 iram_4arm7_phy_addr;
	char *iram_4arm7_virt_addr;
	u32 iram_4arm7_size;
	/* proc:
	 * [0] pointer log
	 */
	u32 proc[PROC_MAX];
	/* for 2stage dma */
	uint32_t pointer2_step_bytes[PLY_MAX];
	uint32_t chan_cnt[PLY_MAX];
	uint32_t width_in_bytes[PLY_MAX];
	/* reserved ddr */
	bool dma_use_ddr_reserved;
	u32 resddr_phy_addr;
	u32 resddr_size;
	char *resddr_virt_addr;
};

struct sprd_pcm_dma_params {
	/* stream identifier */
	char *name;
	/* channel id */
	int channels[2];
	/* dma interrupt type */
	int irq_type;
	/* dma description struct */
	struct sprd_dma_cfg desc;
	/* device physical address for DMA */
	u32 dev_paddr[2];
	/* dma channel name */
	char *used_dma_channel_name[2];
	/* dma channel name for stage 2 */
	char *used_dma_channel_name2;
	/* dma irq type for stage 2 */
	int irq_type2;
	struct sprd_dma_cfg desc2;
};

#endif /* __SPRD_DMA_ENGINE_PCM_H */
