/*
 * sound/soc/sprd/dai/vbc/r1p0v3/vbc.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC for VBC DAI function.
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
#include <linux/clk.h>
#include <sound/core.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <sound/pcm_params.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <sound/tlv.h>
#include <linux/workqueue.h>

#include "dfm.h"
#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"
#include "sprd-asoc-debug.h"
#include "sprd-2stage-dmaengine-pcm.h"
#include "vbc.h"
#include "vbc-codec.h"

#if defined(pr_fmt)
#undef pr_fmt
#endif

#define pr_fmt(fmt) pr_sprd_fmt(" VBC ")""fmt

struct vbc_codec_priv *g_vbc_codec;

struct sprd_vbc_fifo_info {
	unsigned int fullwatermark;
	unsigned int emptywatermark;
};

struct sprd_vbc_priv {
	int (*dma_enable)(int chan);
	int (*dma_disable)(int chan);
	int (*arch_enable)(int chan);
	int (*arch_disable)(int chan);
	int (*fifo_enable)(int chan);
	int (*fifo_disable)(int chan);
	void (*set_watermark)(u32 full, uint32_t empty);
	struct sprd_vbc_fifo_info fifo_info;
	int used_chan_count;
	int rate;
	bool adc_src_enable;
};

struct sprd_dfm_priv dfm;

static struct sprd_pcm_dma_params vbc_pcm_stereo_out = {
	.name = "VBC PCM Stereo out",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		 .fragmens_len = VB_DA01_FRAG_LEN,
		 },
	.desc2 = {
		  .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		  },
	.used_dma_channel_name[0] = "da01-l",
	.used_dma_channel_name[1] = "da01-r",
	.used_dma_channel_name2 = "normal-2stage-p",
};

static struct sprd_pcm_dma_params vbc_pcm23_stereo_out = {
	.name = "VBC PCM23 Stereo out",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		 .fragmens_len = VB_DA23_FRAG_LEN,
		 },
	.desc2 = {
		  .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		  },
	.used_dma_channel_name[0] = "da23-l",
	.used_dma_channel_name[1] = "da23-r",
	.used_dma_channel_name2 = "deep-2stage-p",
};

static struct sprd_pcm_dma_params vbc_pcm_stereo_in = {
	.name = "VBC PCM Stereo in",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		 .fragmens_len = VB_AD01_FRAG_LEN,
		 },
	.desc2 = {
		  .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		  },
	.used_dma_channel_name[0] = "ad01-l",
	.used_dma_channel_name[1] = "ad01-r",
	.used_dma_channel_name2 = "normal-2stage-c",
};

static struct sprd_pcm_dma_params vbc_pcm23_stereo_in = {
	.name = "VBC PCM23 Stereo in",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		 .fragmens_len = VB_AD23_FRAG_LEN,
		 },
	.desc2 = {
		  .datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES,
		  },
	.used_dma_channel_name[0] = "ad23-l",
	.used_dma_channel_name[1] = "ad23-r",
	.used_dma_channel_name2 = "ad23-2stage-c",
};

static struct sprd_vbc_priv vbc[VBC_IDX_MAX];
static unsigned long sprd_vbc_base;
static phys_addr_t sprd_vbc_phy_base;

#include "vbc-comm.c"
#include "vbc-codec.c"

void dfm_priv_set(struct sprd_dfm_priv *in_dfm)
{
	memcpy(&dfm, in_dfm, sizeof(struct sprd_dfm_priv));
}

static void vbc_dma_chn_en(u32 id, u32 en, u32 chan)
{
	unsigned int bit = 0;
	unsigned int reg = 0;
	unsigned int val = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);

		return;
	}
	reg = REG_VBC_VBC_ENABLE_CTRL;
	switch (id) {
	case VBC_CAPTURE01:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_ADC0_DMA_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC1_DMA_EN;
		break;
	case VBC_CAPTURE23:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_ADC2_DMA_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_ADC3_DMA_EN;
		break;
	case VBC_PLAYBACK01:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC0_DMA_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC1_DMA_EN;
		break;
	case VBC_PLAYBACK23:
		if (AUDIO_IS_CHAN_L(chan))
			bit = BIT_RF_DAC2_DMA_EN;
		if (AUDIO_IS_CHAN_R(chan))
			bit |= BIT_RF_DAC3_DMA_EN;
		break;
	default:
		return;
	}
	val = en ? bit : 0;
	vbc_reg_update(REG_VBC_VBC_ENABLE_CTRL, val, bit);
}

static int vbc_da01_dma_enable(int chan)
{
	vbc_dma_chn_en(VBC_PLAYBACK01, true, chan);

	return 0;
}

static int vbc_da01_dma_disable(int chan)
{
	vbc_dma_chn_en(VBC_PLAYBACK01, false, chan);

	return 0;
}

static int vbc_da23_dma_enable(int chan)
{
	vbc_dma_chn_en(VBC_PLAYBACK23, true, chan);

	return 0;
}

static int vbc_da23_dma_disable(int chan)
{
	vbc_dma_chn_en(VBC_PLAYBACK23, false, chan);

	return 0;
}

static int vbc_ad01_dma_enable(int chan)
{
	vbc_dma_chn_en(VBC_CAPTURE01, true, chan);

	return 0;
}

static int vbc_ad01_dma_disable(int chan)
{
	vbc_dma_chn_en(VBC_CAPTURE01, false, chan);

	return 0;
}

static int vbc_ad23_dma_enable(int chan)
{
	vbc_dma_chn_en(VBC_CAPTURE23, true, chan);

	return 0;
}

static int vbc_ad23_dma_disable(int chan)
{
	vbc_dma_chn_en(VBC_CAPTURE23, false, chan);

	return 0;
}

/* water mark */
static void vbc_da01_set_watermark(u32 full, u32 empty)
{
	unsigned int reg = REG_VBC_VBC_DAC01_FIFO_LVL;

	vbc_reg_update(reg, BITS_RF_DAC01_FIFO_AE_LVL(empty) |
		       BITS_RF_DAC01_FIFO_AF_LVL(full),
		       BITS_RF_DAC01_FIFO_AE_LVL(0x1FF) |
		       BITS_RF_DAC01_FIFO_AF_LVL(0x1FF));
}

static void vbc_da23_set_watermark(u32 full, u32 empty)
{
	unsigned int reg = REG_VBC_VBC_DAC23_FIFO_LVL;

	vbc_reg_update(reg, BITS_RF_DAC23_FIFO_AE_LVL(empty) |
		       BITS_RF_DAC23_FIFO_AF_LVL(full),
		       BITS_RF_DAC23_FIFO_AE_LVL(0x1FF) |
		       BITS_RF_DAC23_FIFO_AF_LVL(0x1FF));
}

static void vbc_ad01_set_watermark(u32 full, u32 empty)
{
	unsigned int reg = REG_VBC_VBC_ADC01_FIFO_LVL;

	vbc_reg_update(reg, BITS_RF_ADC01_FIFO_AE_LVL(empty) |
		       BITS_RF_ADC01_FIFO_AF_LVL(full),
		       BITS_RF_ADC01_FIFO_AE_LVL(0x1FF) |
		       BITS_RF_ADC01_FIFO_AF_LVL(0x1FF));
}

static void vbc_ad23_set_watermark(u32 full, u32 empty)
{
	unsigned int reg = REG_VBC_VBC_ADC23_FIFO_LVL;

	vbc_reg_update(reg, BITS_RF_ADC23_FIFO_AE_LVL(empty) |
		       BITS_RF_ADC23_FIFO_AF_LVL(full),
		       BITS_RF_ADC23_FIFO_AE_LVL(0x1FF) |
		       BITS_RF_ADC23_FIFO_AF_LVL(0x1FF));
}

static int vbc_da01_arch_enable(int chan)
{
	return vbc_chan_enable(1, VBC_PLAYBACK01, chan);
}

static int vbc_da01_arch_disable(int chan)
{
	return vbc_chan_enable(0, VBC_PLAYBACK01, chan);
}

static int vbc_da23_arch_enable(int chan)
{
	return vbc_chan_enable(1, VBC_PLAYBACK23, chan);
}

static int vbc_da23_arch_disable(int chan)
{
	return vbc_chan_enable(0, VBC_PLAYBACK23, chan);
}

static int vbc_ad_arch_enable(int chan)
{
	return vbc_chan_enable(1, VBC_CAPTURE01, chan);
}

static int vbc_ad_arch_disable(int chan)
{
	return vbc_chan_enable(0, VBC_CAPTURE01, chan);
}

static int vbc_ad23_arch_enable(int chan)
{
	return vbc_chan_enable(1, VBC_CAPTURE23, chan);
}

static int vbc_ad23_arch_disable(int chan)
{
	return vbc_chan_enable(0, VBC_CAPTURE23, chan);
}

static struct sprd_vbc_priv vbc[VBC_IDX_MAX] = {
	/* da01 */
	[VBC_PLAYBACK01] = {
			    .dma_enable = vbc_da01_dma_enable,
			    .dma_disable = vbc_da01_dma_disable,
			    .arch_enable = vbc_da01_arch_enable,
			    .arch_disable = vbc_da01_arch_disable,
			    .set_watermark = vbc_da01_set_watermark,
			    .fifo_info = {
					  VB_DA01_FULL_WATERMARK,
					  VB_DA01_EMPTY_WATERMARK},
			    .fifo_enable = vbc_da01_fifo_enable,
			    .fifo_disable = vbc_da01_fifo_disable,
			    },
	/* da23 */
	[VBC_PLAYBACK23] = {
			    .dma_enable = vbc_da23_dma_enable,
			    .dma_disable = vbc_da23_dma_disable,
			    .arch_enable = vbc_da23_arch_enable,
			    .arch_disable = vbc_da23_arch_disable,
			    .set_watermark = vbc_da23_set_watermark,
			    .fifo_info = {
					  VB_DA23_FULL_WATERMARK,
					  VB_DA23_EMPTY_WATERMARK},
			    .fifo_enable = vbc_da23_fifo_enable,
			    .fifo_disable = vbc_da23_fifo_disable,
			    },
	/* ad01 */
	[VBC_CAPTURE01] = {
			   .dma_enable = vbc_ad01_dma_enable,
			   .dma_disable = vbc_ad01_dma_disable,
			   .arch_enable = vbc_ad_arch_enable,
			   .arch_disable = vbc_ad_arch_disable,
			   .set_watermark = vbc_ad01_set_watermark,
			   .fifo_info = {
					 VB_AD01_FULL_WATERMARK,
					 VB_AD01_EMPTY_WATERMARK},
			   .fifo_enable = vbc_ad01_fifo_enable,
			   .fifo_disable = vbc_ad01_fifo_disable,
			   },
	/* ad23 */
	[VBC_CAPTURE23] = {
			   .dma_enable = vbc_ad23_dma_enable,
			   .dma_disable = vbc_ad23_dma_disable,
			   .arch_enable = vbc_ad23_arch_enable,
			   .arch_disable = vbc_ad23_arch_disable,
			   .set_watermark = vbc_ad23_set_watermark,
			   .fifo_info = {
					 VB_AD23_FULL_WATERMARK,
					 VB_AD23_EMPTY_WATERMARK},
			   .fifo_enable = vbc_ad23_fifo_enable,
			   .fifo_disable = vbc_ad23_fifo_disable,
			   },
};

static void vbc_set_watermark(int vbc_idx)
{
	vbc[vbc_idx].set_watermark(vbc[vbc_idx].fifo_info.fullwatermark,
				   vbc[vbc_idx].fifo_info.emptywatermark);
}

/* NOTE:
 * this index need use for the [struct sprd_vbc_priv] vbc[4] index
 * default MUST return 0.
 */
static inline int vbc_str_2_index(int stream, int id)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (id == VBC_DAI_NORMAL)
			return VBC_PLAYBACK01;
		else if (id == VBC_DAI_DEEP_BUF)
			return VBC_PLAYBACK23;
	} else {
		if (id == VBC_DAI_NORMAL)
			return VBC_CAPTURE01;
		else if (id == VBC_DAI_AD23)
			return VBC_CAPTURE23;
	}

	return id;
}

static void vbc_dac23_mixto_dac01(s32 enable,
				  struct vbc_codec_priv *vbc_codec)
{
	int chan = AUDIO_CHAN_ALL;

	if (vbc[VBC_PLAYBACK23].used_chan_count == 1) {
		chan = AUDIO_CHAN_L;
	} else if (vbc[VBC_PLAYBACK23].used_chan_count == 2) {
		chan = AUDIO_CHAN_ALL;
	} else {
		pr_err("%s invalid used_chan_count failed\n", __func__);
		return;
	}
	if (enable) {
		/* dac01 data path enable. note used_chan_count is da23 */
		vbc[VBC_PLAYBACK01].arch_enable(chan);
		/* dgmixer enable */
		vbc_module_en(VBC_PLAYBACK01, DA_DGMIXER, true, chan);
		/* pre-fill fifo
		 * a. if nobody writing data to dac01 fifo,
		 * we should pre-fill 0 to it.
		 *  Because the init data in dac01 fifo is random,
		 * so it may generate noise.
		 * b. if somebody is writing data to dac01,
		 * and we do not pre-fill it.
		 * Because we insert 0 in the normal stream,
		 * it may generate pop.
		 * note: dgmixer of dac23->dac01, fm->dac01
		 * and side tone->dac01
		 * need consider pre-fill fifo operation.
		 * c.you must pre-fill fifo first before fifo enable,
		 * otherwise data will not stream(I test in fm case).
		 * if you fill data
		 * to fifo after fifo enable no stream as well.
		 *
		 */
		pre_fill_data(true);
		/* dac01 fifo enble */
		vbc[VBC_PLAYBACK01].fifo_enable(chan);
	} else {
		/* dac01 data path disable */
		vbc[VBC_PLAYBACK01].arch_disable(chan);
		pre_fill_data(false);
		/* dac01 fifo disable */
		vbc[VBC_PLAYBACK01].fifo_disable(chan);
	}
}

static int vbc_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;
	struct vbc_codec_priv *vbc_codec;

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	pr_info("%s vbc_idx(%d) stream(%d) daiId(%d) VBC(%s)\n",
		__func__, vbc_idx, substream->stream, dai->id,
		vbc_get_name(vbc_idx));
	vbc_codec = snd_soc_dai_get_drvdata(dai);
	if (!vbc_codec) {
		pr_err("%s %d vbc_codec failed\n",
		       __func__, __LINE__);
		return -EINVAL;
	}
	vbc_power(1);
	vbc_set_watermark(vbc_idx);
	vbc_component_startup(vbc_idx, dai);

	WARN_ON(!vbc[vbc_idx].arch_enable);
	WARN_ON(!vbc[vbc_idx].arch_disable);
	WARN_ON(!vbc[vbc_idx].dma_enable);
	WARN_ON(!vbc[vbc_idx].dma_disable);

	return 0;
}

static void vbc_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	int vbc_idx;

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_info("%s VBC(%s)\n", __func__, vbc_get_name(vbc_idx));
	vbc_power(0);
}

static void do_vbc_adc_src(int enable, struct vbc_codec_priv *vbc_codec,
			   int vbc_idx)
{
	s32 vbc_ad_src_idx;
	s32 adc_src_rate;
	struct snd_soc_card *card;
	struct sprd_card_data *mdata;

	if (!(vbc_codec && vbc_codec->codec)) {
		pr_err("%s failed\n", __func__);
		return;
	}
	card = vbc_codec->codec->component.card;
	mdata = snd_soc_card_get_drvdata(card);
	if (!mdata) {
		pr_err("%s mdata is NULL\n", __func__);
		return;
	}

	vbc_ad_src_idx = vbc_idx_to_ad_src_idx(vbc_idx);
	if (vbc_ad_src_idx < 0) {
		pr_warn("%s invalid adc_src_idx=%d, vbc_idx=%d\n",
			__func__, vbc_ad_src_idx, vbc_idx);
		return;
	}
	if (!enable) {
		pr_info("%s close ad src. vbc_idx[%d] vbc_ad_src_idx[%d]\n",
			__func__, vbc_idx, vbc_ad_src_idx);
		vbc_ad_src_set(0, vbc_ad_src_idx);
		return;
	}
	switch (vbc_idx) {
	case VBC_CAPTURE23:
		adc_src_rate = mdata->codec_replace_adc_rate;
		break;
	case VBC_CAPTURE01:
		/* normal capture use ad01 */
		spin_lock(&vbc_codec->ad01_spinlock);
		if (vbc_codec->vbc_use_ad01_only &&
		    !vbc_codec->ad01_to_fm) {
			adc_src_rate = mdata->codec_replace_adc_rate;
		} else {
			spin_unlock(&vbc_codec->ad01_spinlock);
			pr_info("%s do not set vbc adc src\n", __func__);
			return;
		}
		spin_unlock(&vbc_codec->ad01_spinlock);
		break;
	default:
		return;
	}
	pr_info("%s adc_src_rate[%d] vbc_ad_src_idx[%d]\n",
		__func__, adc_src_rate, vbc_ad_src_idx);
	vbc_ad_src_set(adc_src_rate, vbc_ad_src_idx);
}

static int vbc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	int vbc_idx;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;
	struct sprd_pcm_dma_params *dma_data[VBC_IDX_MAX] = {
		&vbc_pcm_stereo_out,
		&vbc_pcm23_stereo_out,
		&vbc_pcm_stereo_in,
		&vbc_pcm23_stereo_in,
	};
	struct sprd_card_data *mdata = snd_soc_card_get_drvdata(srtd->card);
	struct vbc_codec_priv *vbc_codec = NULL;

	if (!mdata) {
		pr_err("%s mdata is NULL\n", __func__);
		return 0;
	}

	vbc_codec = snd_soc_dai_get_drvdata(dai);
	if (!vbc_codec) {
		pr_err("%s %d vbc_codec failed\n",
		       __func__, __LINE__);
		return -1;
	}

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_info("%s VBC(%s)\n", __func__, vbc_get_name(vbc_idx));
	snd_soc_dai_set_dma_data(dai, substream, dma_data[vbc_idx]);
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		pr_err("ERR:VBC Only Supports Format S16_LE\n");
		break;
	}

	vbc[vbc_idx].used_chan_count = params_channels(params);
	if (vbc[vbc_idx].used_chan_count > 2)
		pr_err("ERR:VBC Can NOT Supports Grate 2 Channels\n");

	/* 1: left low, right high */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		vbc_da_iis_set_lr_md(1);
		if (vbc_codec->vbc_iis_lr_invert[0] == 1) {
			if (vbc[vbc_idx].used_chan_count == 2)
				/* 0: left high, right low */
				vbc_da_iis_set_lr_md(0);
		}
	} else {
		if (vbc_idx == VBC_CAPTURE01) {
			vbc_ad_iis_set_lr_md(VBC_CAPTURE01, 1);
			if (vbc_codec->vbc_iis_lr_invert[1] == 1)
				vbc_ad_iis_set_lr_md(VBC_CAPTURE01, 0);
		} else if (vbc_idx == VBC_CAPTURE23) {
			vbc_ad_iis_set_lr_md(VBC_CAPTURE23, 1);
			if (vbc_codec->vbc_iis_lr_invert[2] == 1)
				vbc_ad_iis_set_lr_md(VBC_CAPTURE23, 0);
		}
	}

	/* In spite of 2731 can support 44.1k when playing,
	 * it only support multiple 4k sample when caputreing,
	 * for example 32k, 48k. So 2731 should config 32k/48k
	 * when capturing. We reconfig adc01_src/adc23_src when
	 * hal request 44.1k sample.
	 */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (params_rate(params) == 44100) {
			do_vbc_adc_src(true, vbc_codec, vbc_idx);
			vbc[vbc_idx].adc_src_enable = true;
		} else {
			do_vbc_adc_src(false, vbc_codec, vbc_idx);
			vbc[vbc_idx].adc_src_enable = false;
		}
	}

	vbc[vbc_idx].rate = params_rate(params);
	pr_info("vbc[%d].rate=%d\n", vbc_idx, vbc[vbc_idx].rate);

	return 0;
}

static int vbc_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_info("%s VBC(%s)\n", __func__, vbc_get_name(vbc_idx));

	vbc[vbc_idx].rate = 0;

	return 0;
}

static void vbc_da23_mixto_da01_dg(u32 chan, struct vbc_codec_priv *vbc_codec)
{
	if (chan == AUDIO_CHAN_ALL) {
		vbc_da01_set_dgmixer_step(vbc_codec->dgmixerstep_da23);
		vbc_da01_set_dgmixer_dg(chan,
					vbc_codec->dgmixer01[AUDIO_CHAN_L]);
		vbc_da01_set_dgmixer_dg(chan,
					vbc_codec->dgmixer01[AUDIO_CHAN_R]);
		vbc_da23_set_dgmixer_dg(chan,
					vbc_codec->dgmixer23[AUDIO_CHAN_L]);
		vbc_da23_set_dgmixer_dg(chan,
					vbc_codec->dgmixer23[AUDIO_CHAN_R]);
	} else {
		vbc_da01_set_dgmixer_step(vbc_codec->dgmixerstep_da23);
		vbc_da01_set_dgmixer_dg(chan,
					vbc_codec->dgmixer01[AUDIO_CHAN_L]);
		vbc_da23_set_dgmixer_dg(chan,
					vbc_codec->dgmixer23[AUDIO_CHAN_L]);
	}
}

static void vbc_da01mixer_dg(u32 chan, struct vbc_codec_priv *vbc_codec)
{
	if (chan == AUDIO_CHAN_ALL) {
		vbc_da01_set_dgmixer_step(
			vbc_codec->dgmixerstep_da01);
		vbc_da01_set_dgmixer_dg(chan,
					vbc_codec->dgmixer01[AUDIO_CHAN_L]);
		vbc_da01_set_dgmixer_dg(chan,
					vbc_codec->dgmixer01[AUDIO_CHAN_R]);
	} else {
		vbc_da01_set_dgmixer_step(
			vbc_codec->dgmixerstep_da01);
		vbc_da01_set_dgmixer_dg(chan,
					vbc_codec->dgmixer01[AUDIO_CHAN_L]);
	}
}

static void vbc_da01mixer_dg_mute(u32 chan, struct vbc_codec_priv *vbc_codec)
{
	if (chan == AUDIO_CHAN_ALL) {
		vbc_da01_set_dgmixer_step(vbc_codec->dgmixerstep_da01);
		vbc_da01_set_dgmixer_dg(chan, 0);
		vbc_da01_set_dgmixer_dg(chan, 0);
	} else {
		vbc_da01_set_dgmixer_step(vbc_codec->dgmixerstep_da01);
		vbc_da01_set_dgmixer_dg(chan, 0);
	}
}

static int vbc_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	int vbc_idx;
	int ret = 0;
	int chan = AUDIO_CHAN_ALL;
	struct vbc_codec_priv *vbc_codec = NULL;

	vbc_codec = snd_soc_dai_get_drvdata(dai);
	if (!vbc_codec) {
		pr_err("%s %d vbc_codec failed\n",
		       __func__, __LINE__);
		return -1;
	}

	vbc_idx = vbc_str_2_index(substream->stream, dai->id);
	sp_asoc_pr_dbg("%s vbc_idx[%d], cpu_dai[%d] cmd[%d]",
		       __func__, vbc_idx, dai->id, cmd);
	if (vbc[vbc_idx].used_chan_count == 1)
		chan = AUDIO_CHAN_L;
	else if (vbc[vbc_idx].used_chan_count == 2)
		chan = AUDIO_CHAN_ALL;
	else
		pr_err("%s %d failed used chan count %d\n",
		       __func__, __LINE__, chan);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* redo adc src check */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (vbc[vbc_idx].adc_src_enable)
				do_vbc_adc_src(true, vbc_codec, vbc_idx);
			else
				do_vbc_adc_src(false, vbc_codec, vbc_idx);
		}
		if (vbc_idx == VBC_CAPTURE01)
			vbc_try_ad_iismux_set(vbc_codec->sprd_vbc_mux
				[SPRD_VBC_AD_IISMUX].val);
		vbc[vbc_idx].arch_enable(chan);
		vbc[vbc_idx].dma_enable(chan);
		vbc_eq_enable(1, vbc_codec);
		/* dac23 mixer-> dac01 */
		if (vbc_idx == VBC_PLAYBACK23) {
			/* main operation is enable DA_DGMIXER */
			vbc_dac23_mixto_dac01(true, vbc_codec);
			vbc_da23_mixto_da01_dg(chan, vbc_codec);
		}
		if (vbc_idx == VBC_PLAYBACK01) {
			vbc_da01mixer_dg(chan, vbc_codec);
			vbc_module_en(VBC_PLAYBACK01,
				      DA_DGMIXER, true, chan);
			pre_fill_data(true);
		}
		vbc[vbc_idx].fifo_enable(chan);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		vbc[vbc_idx].dma_disable(chan);
		if (vbc_idx == VBC_PLAYBACK01) {
			pre_fill_data(false);
			vbc_da01mixer_dg_mute(AUDIO_CHAN_ALL, vbc_codec);
		}
		vbc[vbc_idx].fifo_disable(chan);
		if (vbc_idx == VBC_PLAYBACK23)
			vbc_dac23_mixto_dac01(false, vbc_codec);
		vbc_eq_enable(0, vbc_codec);
		vbc[vbc_idx].arch_disable(chan);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct snd_soc_dai_ops vbc_dai_ops = {
	.startup = vbc_startup,
	.shutdown = vbc_shutdown,
	.hw_params = vbc_hw_params,
	.trigger = vbc_trigger,
	.hw_free = vbc_hw_free,
};

static int dfm_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int ret;
	int vbc_idx;
	int i;
	static const unsigned int dfm_all_rates[] = {
		32000, 44100, 48000, 8000
	};
	static const struct snd_pcm_hw_constraint_list dfm_rates_constraint = {
		.count = ARRAY_SIZE(dfm_all_rates),
		.list = dfm_all_rates,
	};
	struct snd_soc_card *card = dai->component->card;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(dai->codec);
	struct vbc_codec_priv *vbc_codec = NULL;
	struct sprd_card_data *mdata = snd_soc_card_get_drvdata(card);

	if (!mdata) {
		pr_err("%s mdata is NULL\n", __func__);
		return -1;
	}
	vbc_codec = snd_soc_dai_get_drvdata(dai);
	if (!vbc_codec) {
		pr_err("%s %d vbc_codec failed\n",
		       __func__, __LINE__);
		return -1;
	}

	pr_info("%s, vbc[vbc_idx].rate:da01:%d, da23:%d ad:%d, ad23:%d",
		__func__, vbc[VBC_PLAYBACK01].rate,
		       vbc[VBC_PLAYBACK23].rate, vbc[VBC_CAPTURE01].rate,
		       vbc[VBC_CAPTURE23].rate);

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_RATE,
					 &dfm_rates_constraint);
	if (ret < 0)
		return ret;

	for (vbc_idx = VBC_PLAYBACK01; vbc_idx < VBC_CAPTURE01; vbc_idx++) {
		if (vbc[vbc_idx].rate != 0) {
			if (mdata->is_fm_open_src) {
				if (vbc[vbc_idx].rate != 44100) {
					pr_err("%s %d sample rate[%d] err, it should be 44100\n",
					       __func__, __LINE__,
					       vbc[vbc_idx].rate);
					return -1;
				}
			} else {
				ret =
				    snd_pcm_hw_constraint_minmax
				    (substream->runtime,
				     SNDRV_PCM_HW_PARAM_RATE, vbc[vbc_idx].rate,
				     vbc[vbc_idx].rate);
				if (ret < 0) {
					pr_err("constraint error");
					return ret;
				}
			}
		}
	}

	kfree(snd_soc_dai_get_dma_data(dai, substream));
	snd_soc_dai_set_dma_data(dai, substream, NULL);

	snd_soc_dapm_enable_pin(dapm, "DFM");
	snd_soc_dapm_sync(dapm);

	for (i = 0; i < card->num_links; i++)
		card->dai_link[i].ignore_suspend = 1;
	snd_soc_dapm_ignore_suspend(dapm, "DFM");

	return 0;
}

static void dfm_shutdown(struct snd_pcm_substream
			 *substream, struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = dai->component->card;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(dai->codec);
	int i;

	pr_info("%s\n", __func__);
	snd_soc_dapm_disable_pin(dapm, "DFM");
	snd_soc_dapm_sync(dapm);
	for (i = 0; i < card->num_links; i++)
		card->dai_link[i].ignore_suspend = 0;
}

static int dfm_hw_params(struct snd_pcm_substream
			 *substream, struct snd_pcm_hw_params
			 *params, struct snd_soc_dai *dai)
{
	pr_info("%s\n", __func__);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		pr_err("ERR:VBC Only Supports Format S16_LE\n");
		break;
	}

	return 0;
}

static int dfm_hw_free(struct snd_pcm_substream
		       *substream, struct snd_soc_dai *dai)
{
	pr_info("%s\n", __func__);
	dfm.sample_rate = 0;
	dfm.hw_rate = 0;
	return 0;
}

static struct snd_soc_dai_ops dfm_dai_ops = {
	.startup = dfm_startup,
	.shutdown = dfm_shutdown,
	.hw_params = dfm_hw_params,
	.hw_free = dfm_hw_free,
};

static struct snd_soc_dai_driver vbc_dai[] = {
	{			/* 0 */
	 .name = "vbc-r1p0v3",
	 .id = VBC_DAI_NORMAL,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 /* AD01 */
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	{			/* 1 */
	 .name = "vbc-r1p0v3-ad23",
	 .id = VBC_DAI_AD23,
	 /* AD23 */
	 .capture = {
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_CONTINUOUS,
		     .rate_max = 96000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     },
	 .ops = &vbc_dai_ops,
	 },
	{			/* 2 */
	 .name = "vbc-dfm",
	 .id = DFM_MAGIC_ID,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 48000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .ops = &dfm_dai_ops,
	 },
	{			/* 3 */
	 .name = "vbc-deep-buf",
	 .id = VBC_DAI_DEEP_BUF,
	 .playback = {
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_CONTINUOUS,
		      .rate_max = 96000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      },
	 .ops = &vbc_dai_ops,
	 },
};

static int vbc_set_phys_addr(int vbc_switch)
{
	/* ninglei for sharkl2 ap can config ap dma and aon dma
	 * but can't config wtlcp dma (tgdsp dma, ldsp dam).And
	 * vbc won't request pubcp dma, so audio would not config
	 * pubcp dma.
	 */
	/* do nothing */
	return 0;
}

static int vbc_parse_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	struct regmap *aon_apb_gpr, *pmu_apb_gpr;
	struct vbc_codec_priv *vbc_codec = NULL;
	int ret = 0;
	int i = 0;
	u32 val[2] = {0};

	vbc_codec = platform_get_drvdata(pdev);
	if (!vbc_codec) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -1;
	}

	aon_apb_gpr =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-aon-apb");
	if (IS_ERR(aon_apb_gpr)) {
		pr_err("ERR: Get the vbc aon apb syscon failed!\n");
		aon_apb_gpr = NULL;
		return -ENODEV;
	}
	pr_info("%s arch_audio_set_aon_apb_gpr\n", __func__);
	arch_audio_set_aon_apb_gpr(aon_apb_gpr);

	pmu_apb_gpr =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb_gpr)) {
		pr_err("ERR: Get the vbc pmu apb syscon failed!\n");
		pmu_apb_gpr = NULL;
		return -ENODEV;
	}
	arch_audio_set_pmu_apb_gpr(pmu_apb_gpr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		sprd_vbc_base = (unsigned long)
		    devm_ioremap_nocache(&pdev->dev,
					 res->start, resource_size(res));
		pr_info
		    ("vbc virtual address: %#lx, %lx\n",
		     sprd_vbc_base, (long)sprd_vbc_base);
		if (!sprd_vbc_base) {
			pr_err("ERR: cannot create iomap address for VBC!\n");
			return -EINVAL;
		}
		sprd_vbc_phy_base = res->start;
	} else {
		pr_err("ERR:Must give me the VBC reg address!\n");
		return -EINVAL;
	}

	ret =
	    of_property_read_u32(np,
				 "sprd,dynamic-eq-support",
				 &vbc_codec->dynamic_eq_support);
	if (ret) {
		pr_err
		    ("ERR: %s :no property of 'sprd,dynamic-eq-support'\n",
		     __func__);
		return -ENXIO;
	}

	/* da01, ad01, ad23 */
	ret =
	    of_property_read_u32_array(np,
				       "sprd,vbc-iis-lr-invert",
				       vbc_codec->vbc_iis_lr_invert, 3);
	if (ret) {
		pr_err
		    ("ERR: %s :no property of 'sprd,vbc-iis-lr-invert'\n",
		     __func__);
		return -ENXIO;
	}

	/* 0: aon dma, 1: ap dma
	 * da01, da23, ad01, ad23
	 */
	ret =
	    of_property_read_u32_array(np,
				       "sprd,vbc-use-dma-type",
				       vbc_codec->vbc_use_dma_type,
				       VBC_IDX_MAX);
	if (ret) {
		pr_err
		    ("ERR: %s :no property of 'sprd,vbc-use-dma-type'\n",
		     __func__);
		return -ENXIO;
	}
	ret = of_property_read_u32_array(np, "sprd,clk-stable", val, 2);
	if (!ret) {
		g_clk_status_addr =
			devm_ioremap_nocache(&pdev->dev,
					     (resource_size_t)val[0],
				(resource_size_t)val[1]);
		if (!g_clk_status_addr) {
			pr_err("%s %d failed\n", __func__, __LINE__);
			return -ENXIO;
		}
	}

	/*
	 * vbc ip version r2p0_v3 adc only has ad01, so chip such as sharkle,
	 * pike2 that use r2p0_v3 must config vbc_use_ad01_only=1.
	 */
	ret = of_property_read_u32(np, "sprd,vbc-use-ad01-only",
				   &vbc_codec->vbc_use_ad01_only);
	if (ret) {
		pr_warn(" %s :no property of 'vbc-use-ad01-only', default 0\n",
			__func__);
	}
	ret = of_property_read_u32_array(np, "sprd,iis_bt_fm_loop", val, 2);
	if (!ret) {
		vbc_codec->iis_bt_fm_loop[0] = val[0];
		vbc_codec->iis_bt_fm_loop[1] = val[1];
	}

	pr_info("vbc_use_ad01_only=%#x\n", vbc_codec->vbc_use_ad01_only);

	pr_info("dynamic_eq_support=%#x\n", vbc_codec->dynamic_eq_support);

	pr_info("%s sprd_vbc_phy_base =%#x, sprd_vbc_base(virt)=%lx,\n",
		__func__, (unsigned int)sprd_vbc_phy_base, sprd_vbc_base);
	pr_info
	    ("vbc_iis_lr_invert[0]=[%#x][1]=%#x[2]=%#x",
	     vbc_codec->vbc_iis_lr_invert[0], vbc_codec->vbc_iis_lr_invert[1],
	     vbc_codec->vbc_iis_lr_invert[2]);
	for (i = 0; i < VBC_IDX_MAX; i++)
		pr_info(" vbc_idx[%d] vbc_use_dma_type = %s", i,
			(vbc_codec->vbc_use_dma_type[i] == 0) ?
			"AON DMA" : "AP DMA");
	pr_info("\n");

	return 0;
}

static int vbc_drv_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	int da01_dma_ch, da23_dma_ch, ad01_dma_ch, ad23_dma_ch;
	struct vbc_codec_priv *vbc_codec = NULL;

	sp_asoc_pr_dbg("%s\n", __func__);

	vbc_codec =
	    devm_kzalloc(&pdev->dev, sizeof(struct vbc_codec_priv), GFP_KERNEL);
	if (!vbc_codec) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	g_vbc_codec = vbc_codec;
	platform_set_drvdata(pdev, vbc_codec);
	vbc_parse_dt(pdev);

	arch_audio_vbc_switch(VBC_TO_AP_CTRL, vbc_codec->vbc_use_dma_type,
			      VBC_IDX_MAX);
	ret =
	    arch_audio_vbc_switch(VBC_NO_CHANGE, vbc_codec->vbc_use_dma_type,
				  VBC_IDX_MAX);
	if (ret != VBC_TO_AP_CTRL &&  ret != VBC_NO_CHANGE) {
		pr_err("Failed to Switch VBC to AP\n");
		return -1;
	}
	arch_audio_vbc_int_switch(VBC_INT_TO_AP_CTRL);
	arch_audio_vbc_dma_switch(VBC_DMA_TO_AP_AON_CTRL,
				  vbc_codec->vbc_use_dma_type, VBC_IDX_MAX);

	vbc_pcm_stereo_out.dev_paddr[0] =
	    REG_VBC_VBC_DAC0_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	vbc_pcm_stereo_out.dev_paddr[1] =
	    REG_VBC_VBC_DAC1_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	vbc_pcm23_stereo_out.dev_paddr[0] =
	    REG_VBC_VBC_DAC2_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	vbc_pcm23_stereo_out.dev_paddr[1] =
	    REG_VBC_VBC_DAC3_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	vbc_pcm_stereo_in.dev_paddr[0] =
	    REG_VBC_VBC_ADC0_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	vbc_pcm_stereo_in.dev_paddr[1] =
	    REG_VBC_VBC_ADC1_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	vbc_pcm23_stereo_in.dev_paddr[0] =
	    REG_VBC_VBC_ADC2_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	vbc_pcm23_stereo_in.dev_paddr[1] =
	    REG_VBC_VBC_ADC3_FIFO_ADDR - VBC_BASE + sprd_vbc_phy_base;
	if (vbc_codec->vbc_use_dma_type[VBC_PLAYBACK01] == 0)
		da01_dma_ch = DMA_REQ_ARM7_VBC_DA0;
	else
		da01_dma_ch = DMA_REQ_AP_VBDA0;

	if (vbc_codec->vbc_use_dma_type[VBC_PLAYBACK23] == 0)
		da23_dma_ch = DMA_REQ_ARM7_VBC_DA2;
	else
		da23_dma_ch = DMA_REQ_AP_VBDA2;

	if (vbc_codec->vbc_use_dma_type[VBC_CAPTURE01] == 0)
		ad01_dma_ch = DMA_REQ_ARM7_VBC_AD0;
	else
		ad01_dma_ch = DMA_REQ_AP_VBAD0;

	if (vbc_codec->vbc_use_dma_type[VBC_CAPTURE23] == 0)
		ad23_dma_ch = DMA_REQ_ARM7_VBC_AD2;
	else
		ad23_dma_ch = DMA_REQ_AP_VBAD2;

	for (i = 0; i < 2; i++) {
		vbc_pcm_stereo_out.channels[i] = da01_dma_ch + i;
		vbc_pcm23_stereo_out.channels[i] = da23_dma_ch + i;
		vbc_pcm_stereo_in.channels[i] = ad01_dma_ch + i;
		vbc_pcm23_stereo_in.channels[i] = ad23_dma_ch + i;
	}
	/* 1. probe CODEC */
	ret = sprd_vbc_codec_probe(pdev);

	if (ret < 0)
		goto probe_err;

	/* 2. probe DAIS */
	ret =
	    snd_soc_register_codec(&pdev->dev,
				   &sprd_vbc_codec,
				   vbc_dai, ARRAY_SIZE(vbc_dai));

	if (ret < 0) {
		pr_err("ERR:Register VBC to DAIS Failed!\n");
		goto probe_err;
	}
	sp_asoc_pr_dbg("%s probed\n", __func__);
	mutex_init(&vbc_codec->fm_mutex);
	spin_lock_init(&vbc_codec->ad01_spinlock);

	return ret;
probe_err:
	sp_asoc_pr_dbg("return %i\n", ret);

	return ret;
}

static int vbc_drv_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	sprd_vbc_codec_remove(pdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_of_match[] = {
	{.compatible = "sprd,vbc-r1p0v3",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_of_match);
#endif

static struct platform_driver vbc_driver = {
	.driver = {
		   .name = "vbc-r1p0v3",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(vbc_of_match),
		   },

	.probe = vbc_drv_probe,
	.remove = vbc_drv_remove,
};

static int __init sprd_vbc_driver_init(void)
{
	return platform_driver_register(&vbc_driver);
}

late_initcall(sprd_vbc_driver_init);

/* include the other module of VBC */

MODULE_DESCRIPTION("SPRD ASoC VBC CUP-DAI driver");
MODULE_AUTHOR("Zhenfang Wang <zhenfang.wang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cpu-dai:vbc-r1p0v3");
