
#ifndef SPRD_AUDIO_DMA_H
#define SPRD_AUDIO_DMA_H
#include <linux/dmaengine.h>
#include <linux/dma/sprd-dma.h>

struct sprd_dma_cfg {
	struct dma_slave_config config;
	struct sprd_dma_linklist ll_cfg;
	unsigned long  dma_config_flag;
	u32 transcation_len;
	u32 datawidth;
	u32 src_step;
	u32 des_step;
	u32 fragmens_len;
	u32 sg_num;
	struct scatterlist *sg;
};

#endif

