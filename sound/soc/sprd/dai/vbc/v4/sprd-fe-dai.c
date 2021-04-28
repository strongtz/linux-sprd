/*
 * sound/soc/sprd/dai/vbc/
 *
 * Front end cpu dai of sprd audio driver
 *
 * Copyright (C) 2018 SpreadTrum Ltd.
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" FE.VBC ") ""fmt

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <sound/core.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include "agdsp_access.h"
#include "mcdt_hw.h"
#include "sprd-dmaengine-pcm.h"
#include "sprd-fe-dai.h"
#include "sprd-platform-pcm-routing.h"
#include "vbc-phy-v4.h"

#define TO_STRING(e) #e

static struct sprd_pcm_dma_params vbc_pcm_normal_ap01_p;
static struct sprd_pcm_dma_params vbc_pcm_normal_ap01_c;
static struct sprd_pcm_dma_params vbc_pcm_normal_ap23_p;
static struct sprd_pcm_dma_params vbc_pcm_normal_ap23_c;
static struct sprd_pcm_dma_params pcm_dsp_cap_mcdt;
static struct sprd_pcm_dma_params pcm_fast_play_mcdt;
static struct sprd_pcm_dma_params vbc_pcm_voice_capture_mcdt;
static struct sprd_pcm_dma_params pcm_loop_record_mcdt;
static struct sprd_pcm_dma_params pcm_loop_play_mcdt;
static struct sprd_pcm_dma_params pcm_voip_record_mcdt;
static struct sprd_pcm_dma_params pcm_voip_play_mcdt;
static struct sprd_pcm_dma_params vbc_pcm_fm_caputre;
static struct sprd_pcm_dma_params vbc_pcm_a2dp_p;
static struct sprd_pcm_dma_params pcm_dsp_fm_cap_mcdt;
static struct sprd_pcm_dma_params pcm_dsp_btsco_cap_mcdt;
static struct sprd_pcm_dma_params vbc_pcm_dump;
static struct sprd_pcm_dma_params vbc_btsco_cap_ap;

static const char *stream_to_str(int stream)
{
	return (stream == SNDRV_PCM_STREAM_PLAYBACK) ?
		"playback" : "capture";
}

static char *fe_dai_id_str[FE_DAI_ID_MAX] = {
	[FE_DAI_ID_NORMAL_AP01] = TO_STRING(FE_DAI_ID_NORMAL_AP01),
	[FE_DAI_ID_NORMAL_AP23] = TO_STRING(FE_DAI_ID_NORMAL_AP23),
	[FE_DAI_ID_CAPTURE_DSP] = TO_STRING(FE_DAI_ID_CAPTURE_DSP),
	[FE_DAI_ID_FAST_P] = TO_STRING(FE_DAI_ID_FAST_P),
	[FE_DAI_ID_OFFLOAD] = TO_STRING(FE_DAI_ID_OFFLOAD),
	[FE_DAI_ID_VOICE] = TO_STRING(FE_DAI_ID_VOICE),
	[FE_DAI_ID_VOIP] = TO_STRING(FE_DAI_ID_VOIP),
	[FE_DAI_ID_FM] = TO_STRING(FE_DAI_ID_FM),
	[FE_DAI_ID_FM_CAPTURE_AP] = TO_STRING(FE_DAI_ID_FM_CAPTURE_AP),
	[FE_DAI_ID_VOICE_CAPTURE] = TO_STRING(FE_DAI_ID_VOICE_CAPTURE),
	[FE_DAI_ID_LOOP] = TO_STRING(FE_DAI_ID_LOOP),
	[FE_DAI_ID_A2DP_OFFLOAD] = TO_STRING(FE_DAI_ID_A2DP_OFFLOAD),
	[FE_DAI_ID_A2DP_PCM] = TO_STRING(FE_DAI_ID_A2DP_PCM),
	[FE_DAI_ID_FM_CAP_DSP] = TO_STRING(FE_DAI_ID_FM_CAP_DSP),
	[FE_DAI_ID_BTSCO_CAP_DSP] = TO_STRING(FE_DAI_ID_BTSCO_CAP_DSP),
	[FE_DAI_ID_FM_DSP] = TO_STRING(FE_DAI_ID_FM_DSP),
	[FE_DAI_ID_DUMP] = TO_STRING(FE_DAI_ID_DUMP),
	[FE_DAI_ID_BTSCO_CAP_AP] = TO_STRING(FE_DAI_ID_BTSCO_CAP_AP),
};

static const char *fe_dai_id_to_str(int fe_dai_id)
{
	if (fe_dai_id >= FE_DAI_ID_MAX) {
		pr_err("invalid fe_dai_id %d\n", fe_dai_id);
		return "";
	}
	if (!fe_dai_id_str[fe_dai_id]) {
		pr_err("null dai_id string fe_dai_id=%d\n", fe_dai_id);
		return "";
	}

	return fe_dai_id_str[fe_dai_id];
}

static void mcdt_dma_deinit(struct snd_soc_dai *fe_dai, int stream)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;

	switch (fe_dai->id) {
	case FE_DAI_ID_FM:
	case FE_DAI_ID_OFFLOAD:
	case FE_DAI_ID_A2DP_OFFLOAD:
	case FE_DAI_ID_VOICE:
	case FE_DAI_ID_FM_DSP:
	default:
	break;
	case FE_DAI_ID_CAPTURE_DSP:
		mcdt_adc_dma_disable(MCDT_CHAN_DSP_CAP);
		break;
	case FE_DAI_ID_FM_CAP_DSP:
		mcdt_adc_dma_disable(MCDT_CHAN_DSP_FM_CAP);
		break;
	case FE_DAI_ID_BTSCO_CAP_DSP:
		mcdt_adc_dma_disable(MCDT_CHAN_DSP_BTSCO_CAP);
		break;
	case FE_DAI_ID_VOICE_CAPTURE:
		mcdt_adc_dma_disable(MCDT_CHAN_VOICE_CAPTURE);
		break;
	case FE_DAI_ID_LOOP:
		if (is_playback)
			mcdt_dac_dma_disable(MCDT_CHAN_LOOP);
		else
			mcdt_adc_dma_disable(MCDT_CHAN_LOOP);
		break;
	case FE_DAI_ID_FAST_P:
		mcdt_dac_dma_disable(MCDT_CHAN_FAST_PLAY);
		break;
	case FE_DAI_ID_VOIP:
		if (is_playback)
			mcdt_dac_dma_disable(MCDT_CHAN_VOIP);
		else
			mcdt_adc_dma_disable(MCDT_CHAN_VOIP);
		break;
	case FE_DAI_ID_A2DP_PCM:
		mcdt_dac_dma_disable(MCDT_CHAN_A2DP_PCM);
		break;
	}
}

static int mcdt_dma_config_init(struct snd_soc_dai *fe_dai, int stream)
{
	int uid;
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;

	switch (fe_dai->id) {
	case FE_DAI_ID_FM:
	case FE_DAI_ID_OFFLOAD:
	case FE_DAI_ID_A2DP_OFFLOAD:
	case FE_DAI_ID_VOICE:
	case FE_DAI_ID_FM_DSP:
	default:
		uid = 0;
		break;
	case FE_DAI_ID_CAPTURE_DSP:
		uid = mcdt_adc_dma_enable(MCDT_CHAN_DSP_CAP,
			MCDT_FULL_WMK_DSP_CAP);
		pcm_dsp_cap_mcdt.channels[0] = uid;
		break;
	case FE_DAI_ID_FM_CAP_DSP:
		uid = mcdt_adc_dma_enable(MCDT_CHAN_DSP_FM_CAP,
			MCDT_FULL_WMK_DSP_FM_CAP);
		pcm_dsp_fm_cap_mcdt.channels[0] = uid;
		break;
	case FE_DAI_ID_BTSCO_CAP_DSP:
		uid = mcdt_adc_dma_enable(MCDT_CHAN_DSP_BTSCO_CAP,
			MCDT_FULL_WMK_DSP_BTSCO_CAP);
		pcm_dsp_btsco_cap_mcdt.channels[0] = uid;
		break;
	case FE_DAI_ID_VOICE_CAPTURE:
		uid = mcdt_adc_dma_enable(MCDT_CHAN_VOICE_CAPTURE,
			MCDT_FULL_WMK_VOICE_CAPTURE);
		vbc_pcm_voice_capture_mcdt.channels[0] = uid;
		break;
	case FE_DAI_ID_LOOP:
		if (is_playback) {
			uid = mcdt_dac_dma_enable(MCDT_CHAN_LOOP,
				MCDT_EMPTY_WMK_LOOP);
			pcm_loop_play_mcdt.channels[0] = uid;
		} else {
			uid = mcdt_adc_dma_enable(MCDT_CHAN_LOOP,
				MCDT_FULL_WMK_LOOP);
			pcm_loop_record_mcdt.channels[0] = uid;
		}
		break;
	case FE_DAI_ID_FAST_P:
		uid = mcdt_dac_dma_enable(MCDT_CHAN_FAST_PLAY,
			MCDT_EMPTY_WMK_FAST_PLAY);
		pcm_fast_play_mcdt.channels[0] = uid;
		break;
	case FE_DAI_ID_VOIP:
		if (is_playback) {
			uid = mcdt_dac_dma_enable(MCDT_CHAN_VOIP,
				MCDT_EMPTY_WMK_VOIP);
			pcm_voip_play_mcdt.channels[0] = uid;
		} else {
			uid = mcdt_adc_dma_enable(MCDT_CHAN_VOIP,
				MCDT_FULL_WMK_VOIP);
			pcm_voip_record_mcdt.channels[0] = uid;
		}
		break;
	case FE_DAI_ID_A2DP_PCM:
		uid = mcdt_dac_dma_enable(MCDT_CHAN_A2DP_PCM,
			MCDT_EMPTY_WMK_A2DP_PCM);
		vbc_pcm_a2dp_p.channels[0] = uid;
		break;
	}

	if (uid < 0) {
		pr_err("%s failed dai_id=%d stream=%d\n", __func__,
			fe_dai->id, stream);
		return uid;
	}

	return 0;
}

static bool is_normal_playback_use_24bit(
	struct snd_pcm_hw_params *params,
	struct snd_pcm_substream *substream, struct snd_soc_dai *fe_dai)
{
	int32_t is_24bit = false;
	int32_t is_normal_playback = false;

	is_24bit = params_format(params) == SNDRV_PCM_FORMAT_S24_LE;
	is_normal_playback = ((fe_dai->id == FE_DAI_ID_NORMAL_AP01) &&
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK));
	if (is_24bit && is_normal_playback) {
		pr_info("%s is use 24bit\n", __func__);
		return true;
	}

	return false;
}

static void sprd_dma_config(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *fe_dai)
{
	u32 rate;
	int is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		1 : 0;

	switch (fe_dai->id) {
	case FE_DAI_ID_FM:
	case FE_DAI_ID_OFFLOAD:
	case FE_DAI_ID_A2DP_OFFLOAD:
	case FE_DAI_ID_VOICE:
	case FE_DAI_ID_FM_DSP:
	default:
		pr_info("%s %s do not use dma\n", __func__,
			fe_dai_id_to_str(fe_dai->id));
	break;
	case FE_DAI_ID_NORMAL_AP01:
		if (is_playback) {
			/*normal ap01 playback*/
			vbc_pcm_normal_ap01_p.name = "VBC NORMAL AP01 PLY";
			vbc_pcm_normal_ap01_p.irq_type = SPRD_DMA_BLK_INT;
			if (is_normal_playback_use_24bit(params,
				substream, fe_dai))
				vbc_pcm_normal_ap01_p.desc.datawidth =
					DMA_SLAVE_BUSWIDTH_4_BYTES;
			else
				vbc_pcm_normal_ap01_p.desc.datawidth =
					DMA_SLAVE_BUSWIDTH_2_BYTES;
			vbc_pcm_normal_ap01_p.desc.fragmens_len =
				VBC_AUDPLY01_FRAGMENT;
			vbc_pcm_normal_ap01_p.dev_paddr[0] =
				vbc_phy_ap2dsp(VBC_AUDPLY_FIFO_WR_0 +
				get_ap_vbc_phy_base());
			vbc_pcm_normal_ap01_p.dev_paddr[1] =
				vbc_phy_ap2dsp(VBC_AUDPLY_FIFO_WR_1 +
				get_ap_vbc_phy_base());
			vbc_pcm_normal_ap01_p.channels[0] = DMA_REQ_DA0_DEV_ID;
			vbc_pcm_normal_ap01_p.channels[1] = DMA_REQ_DA1_DEV_ID;
			vbc_pcm_normal_ap01_p.used_dma_channel_name[0] =
				"normal_p_l";
			vbc_pcm_normal_ap01_p.used_dma_channel_name[1] =
				"normal_p_r";
		} else {
			/*normal ap01 capture*/
			vbc_pcm_normal_ap01_c.name = "VBC NORMAL AP01 CAP";
			vbc_pcm_normal_ap01_c.irq_type = SPRD_DMA_BLK_INT;
			vbc_pcm_normal_ap01_c.desc.datawidth =
				DMA_SLAVE_BUSWIDTH_2_BYTES;
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE
				&& fe_dai->id == FE_DAI_ID_NORMAL_AP01) {
				rate = params_rate(params);
				vbc_pcm_normal_ap01_c.desc.fragmens_len
					= ((rate * VBC_AUDRCD01_FRAGMENT) /
					DEFAULT_RATE) &
					~BIT(0);
				pr_info("%s rate = %u, framlen=%u", __func__,
				rate,
				vbc_pcm_normal_ap01_c.desc.fragmens_len);
			}
			vbc_pcm_normal_ap01_c.dev_paddr[0] =
				vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_0 +
				get_ap_vbc_phy_base());
			vbc_pcm_normal_ap01_c.dev_paddr[1] =
				vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_1 +
				get_ap_vbc_phy_base());
			vbc_pcm_normal_ap01_c.channels[0] = DMA_REQ_AD0_DEV_ID;
			vbc_pcm_normal_ap01_c.channels[1] = DMA_REQ_AD1_DEV_ID;
			vbc_pcm_normal_ap01_c.used_dma_channel_name[0] =
				"normal_c_l";
			vbc_pcm_normal_ap01_c.used_dma_channel_name[1] =
				"normal_c_r";
		}
		break;
	case FE_DAI_ID_NORMAL_AP23:
		if (is_playback) {
			/*normal ap23 playback*/
			vbc_pcm_normal_ap23_p.name = "VBC NORMAL AP23 PLY";
			vbc_pcm_normal_ap23_p.irq_type = SPRD_DMA_BLK_INT;
			if (is_normal_playback_use_24bit(params,
				substream, fe_dai))
				vbc_pcm_normal_ap23_p.desc.datawidth =
					DMA_SLAVE_BUSWIDTH_4_BYTES;
			else
				vbc_pcm_normal_ap23_p.desc.datawidth =
					DMA_SLAVE_BUSWIDTH_2_BYTES;
			vbc_pcm_normal_ap23_p.desc.fragmens_len =
				VBC_AUDPLY23_FRAGMENT;
			vbc_pcm_normal_ap23_p.dev_paddr[0] =
				vbc_phy_ap2dsp(VBC_AUDPLY_FIFO_WR_2 +
					get_ap_vbc_phy_base());
			vbc_pcm_normal_ap23_p.dev_paddr[1] =
				vbc_phy_ap2dsp(VBC_AUDPLY_FIFO_WR_3 +
				get_ap_vbc_phy_base());
			vbc_pcm_normal_ap23_p.channels[0] = DMA_REQ_DA2_DEV_ID;
			vbc_pcm_normal_ap23_p.channels[1] = DMA_REQ_DA3_DEV_ID;
			vbc_pcm_normal_ap23_p.used_dma_channel_name[0] =
				"normal23_p_l";
			vbc_pcm_normal_ap23_p.used_dma_channel_name[1] =
				"normal23_p_r";
		} else {
			/*normal ap23 capture*/
			vbc_pcm_normal_ap23_c.name = "VBC NORMAL AP23 CAP";
			vbc_pcm_normal_ap23_c.irq_type = SPRD_DMA_BLK_INT;
			vbc_pcm_normal_ap23_c.desc.datawidth =
				DMA_SLAVE_BUSWIDTH_2_BYTES;
			vbc_pcm_normal_ap23_c.desc.fragmens_len =
				VBC_AUDRCD23_FRAGMENT;
			vbc_pcm_normal_ap23_c.dev_paddr[0] =
				vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_2 +
				get_ap_vbc_phy_base());
			vbc_pcm_normal_ap23_c.dev_paddr[1] =
				vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_3 +
				get_ap_vbc_phy_base());
			vbc_pcm_normal_ap23_c.channels[0] = DMA_REQ_AD2_DEV_ID;
			vbc_pcm_normal_ap23_c.channels[1] = DMA_REQ_AD3_DEV_ID;
			vbc_pcm_normal_ap23_c.used_dma_channel_name[0] =
				"normal23_c_l";
			vbc_pcm_normal_ap23_c.used_dma_channel_name[1] =
				"normal23_c_r";
		}
		break;
	case FE_DAI_ID_CAPTURE_DSP:
		/* dsp captrue */
		pcm_dsp_cap_mcdt.name = "VBC DSP_CAP C";
		pcm_dsp_cap_mcdt.irq_type = SPRD_DMA_BLK_INT;
		pcm_dsp_cap_mcdt.desc.datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		pcm_dsp_cap_mcdt.desc.fragmens_len = MCDT_DSPCAP_FRAGMENT;
		pcm_dsp_cap_mcdt.use_mcdt = 1;
		pcm_dsp_cap_mcdt.dev_paddr[0] =
			mcdt_adc_dma_phy_addr(MCDT_CHAN_DSP_CAP);
		pcm_dsp_cap_mcdt.used_dma_channel_name[0] = "dspcap_c";
		break;
	case FE_DAI_ID_FM_CAP_DSP:
		/* dsp fm captrue */
		pcm_dsp_fm_cap_mcdt.name = "VBC DSP_FM_CAP C";
		pcm_dsp_fm_cap_mcdt.irq_type = SPRD_DMA_BLK_INT;
		pcm_dsp_fm_cap_mcdt.desc.datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		pcm_dsp_fm_cap_mcdt.desc.fragmens_len = MCDT_DSPFMCAP_FRAGMENT;
		pcm_dsp_fm_cap_mcdt.use_mcdt = 1;
		pcm_dsp_fm_cap_mcdt.dev_paddr[0] =
			mcdt_adc_dma_phy_addr(MCDT_CHAN_DSP_FM_CAP);
		pcm_dsp_fm_cap_mcdt.used_dma_channel_name[0] = "dspfmcap_c";
		break;
	case FE_DAI_ID_BTSCO_CAP_DSP:
		/* dsp btsco captrue */
		pcm_dsp_btsco_cap_mcdt.name = "VBC DSP_BTSCO_CAP C";
		pcm_dsp_btsco_cap_mcdt.irq_type = SPRD_DMA_BLK_INT;
		pcm_dsp_btsco_cap_mcdt.desc.datawidth =
			DMA_SLAVE_BUSWIDTH_4_BYTES;
		pcm_dsp_btsco_cap_mcdt.desc.fragmens_len =
			MCDT_DSPBTSCOCAP_FRAGMENT;
		pcm_dsp_btsco_cap_mcdt.use_mcdt = 1;
		pcm_dsp_btsco_cap_mcdt.dev_paddr[0] =
			mcdt_adc_dma_phy_addr(MCDT_CHAN_DSP_BTSCO_CAP);
		pcm_dsp_btsco_cap_mcdt.used_dma_channel_name[0] =
			"dspbtscocap_c";
		break;
	case FE_DAI_ID_FAST_P:
		/*fast playback*/
		pcm_fast_play_mcdt.name = "VBC PCM Fast P";
		pcm_fast_play_mcdt.irq_type = SPRD_DMA_BLK_INT;
		pcm_fast_play_mcdt.desc.datawidth =
			DMA_SLAVE_BUSWIDTH_4_BYTES;
		pcm_fast_play_mcdt.desc.fragmens_len = MCDT_FAST_PLAY_FRAGMENT;
		pcm_fast_play_mcdt.use_mcdt = 1;
		pcm_fast_play_mcdt.dev_paddr[0] =
			mcdt_dac_dma_phy_addr(MCDT_CHAN_FAST_PLAY);
		pcm_fast_play_mcdt.used_dma_channel_name[0] = "fast_p";
		break;
	case FE_DAI_ID_VOIP:
		if (is_playback) {
			/*voip play*/
			pcm_voip_play_mcdt.name = "PCM voip play With MCDT";
			pcm_voip_play_mcdt.irq_type = SPRD_DMA_BLK_INT;
			pcm_voip_play_mcdt.desc.datawidth =
				DMA_SLAVE_BUSWIDTH_4_BYTES;
			pcm_voip_play_mcdt.desc.fragmens_len =
				MCDT_VOIP_P_FRAGMENT;
			pcm_voip_play_mcdt.use_mcdt = 1;
			pcm_voip_play_mcdt.dev_paddr[0] =
				mcdt_dac_dma_phy_addr(MCDT_CHAN_VOIP);
			pcm_voip_play_mcdt.used_dma_channel_name[0] = "voip_p";
		} else {
			/*voip capture*/
			pcm_voip_record_mcdt.name = "PCM voip record With MCDT";
			pcm_voip_record_mcdt.irq_type = SPRD_DMA_BLK_INT;
			pcm_voip_record_mcdt.desc.datawidth = SPRD_DMA_BLK_INT;
			pcm_voip_record_mcdt.desc.fragmens_len =
				MCDT_VOIP_C_FRAGMENT;
			pcm_voip_record_mcdt.use_mcdt = 1;
			pcm_voip_record_mcdt.dev_paddr[0] =
				mcdt_adc_dma_phy_addr(MCDT_CHAN_VOIP);
			pcm_voip_record_mcdt.used_dma_channel_name[0] =
				"voip_c";
			pcm_voip_record_mcdt.use_mcdt = 1;
		}
		break;
	case FE_DAI_ID_VOICE_CAPTURE:
		/*voic capture*/
		vbc_pcm_voice_capture_mcdt.name = "VBC PCM voice C With MCDT";
		vbc_pcm_voice_capture_mcdt.irq_type = SPRD_DMA_BLK_INT;
		vbc_pcm_voice_capture_mcdt.desc.datawidth =
			DMA_SLAVE_BUSWIDTH_4_BYTES;
		vbc_pcm_voice_capture_mcdt.desc.fragmens_len =
			MCDT_VOICE_C_FRAGMENT;
		vbc_pcm_voice_capture_mcdt.use_mcdt = 1;
		vbc_pcm_voice_capture_mcdt.dev_paddr[0] =
		/* dma src address*/
			mcdt_adc_dma_phy_addr(MCDT_CHAN_VOICE_CAPTURE);
		vbc_pcm_voice_capture_mcdt.used_dma_channel_name[0] = "voice_c";
		break;
	case FE_DAI_ID_LOOP:
		if (is_playback) {
			/*loop back play*/
			pcm_loop_play_mcdt.name = "PCM loop play With MCDT";
			pcm_loop_play_mcdt.irq_type = SPRD_DMA_BLK_INT;
			pcm_loop_play_mcdt.desc.datawidth =
				DMA_SLAVE_BUSWIDTH_4_BYTES;
			pcm_loop_play_mcdt.desc.fragmens_len =
				MCDT_LOOP_P_FRAGMENT;
			pcm_loop_play_mcdt.use_mcdt = 1;
			pcm_loop_play_mcdt.dev_paddr[0] =
				mcdt_dac_dma_phy_addr(MCDT_CHAN_LOOP);
			pcm_loop_play_mcdt.used_dma_channel_name[0] = "loop_p";
			pcm_loop_play_mcdt.use_mcdt = 1;
		} else {
			/*loop back record*/
			pcm_loop_record_mcdt.name = "PCM loop record With MCDT";
			pcm_loop_record_mcdt.irq_type = SPRD_DMA_BLK_INT;
			pcm_loop_record_mcdt.desc.datawidth =
				DMA_SLAVE_BUSWIDTH_4_BYTES;
			pcm_loop_record_mcdt.desc.fragmens_len =
				MCDT_LOOP_C_FRAGMENT;
			pcm_loop_record_mcdt.use_mcdt = 1;
			pcm_loop_record_mcdt.dev_paddr[0] =
				mcdt_adc_dma_phy_addr(MCDT_CHAN_LOOP);
			pcm_loop_record_mcdt.used_dma_channel_name[0] =
				"loop_c";
		}
		break;
	case FE_DAI_ID_FM_CAPTURE_AP:
		/*
		 * fm capture dma data equal normal capture so they
		 * can not be concurrent
		 */
		vbc_pcm_fm_caputre.name = "VBC PCM fm capture";
		vbc_pcm_fm_caputre.irq_type = SPRD_DMA_BLK_INT;
		vbc_pcm_fm_caputre.desc.datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		vbc_pcm_fm_caputre.desc.fragmens_len = VBC_AUDRCD01_FRAGMENT;
		vbc_pcm_fm_caputre.dev_paddr[0] =
				vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_0 +
				get_ap_vbc_phy_base());
		vbc_pcm_fm_caputre.dev_paddr[1] =
			vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_1 +
			get_ap_vbc_phy_base());
		vbc_pcm_fm_caputre.channels[0] = DMA_REQ_AD0_DEV_ID;
		vbc_pcm_fm_caputre.channels[1] = DMA_REQ_AD1_DEV_ID;
		vbc_pcm_fm_caputre.used_dma_channel_name[0] =
			"normal_c_l";
		vbc_pcm_fm_caputre.used_dma_channel_name[1] =
			"normal_c_r";
		break;
	case FE_DAI_ID_A2DP_PCM:
		/* a2dp pcm */
		vbc_pcm_a2dp_p.name = "VBC a2dp pcm";
		vbc_pcm_a2dp_p.irq_type = SPRD_DMA_BLK_INT;
		vbc_pcm_a2dp_p.desc.datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		vbc_pcm_a2dp_p.desc.fragmens_len = MCDT_A2DP_PCM_FRAGMENT;
		vbc_pcm_a2dp_p.use_mcdt = 1;
		vbc_pcm_a2dp_p.dev_paddr[0] =
			mcdt_dac_dma_phy_addr(MCDT_CHAN_A2DP_PCM);
		vbc_pcm_a2dp_p.used_dma_channel_name[0] = "a2dppcm_p";
		break;
	case FE_DAI_ID_DUMP:
		vbc_pcm_dump.name = "VBC PCM dump";
		vbc_pcm_dump.irq_type = SPRD_DMA_BLK_INT;
		vbc_pcm_dump.desc.datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		vbc_pcm_dump.desc.fragmens_len = VBC_AUDRCD01_FRAGMENT;
		vbc_pcm_dump.dev_paddr[0] =
				vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_0 +
				get_ap_vbc_phy_base());
		vbc_pcm_dump.dev_paddr[1] =
			vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_1 +
			get_ap_vbc_phy_base());
		vbc_pcm_dump.channels[0] = DMA_REQ_AD0_DEV_ID;
		vbc_pcm_dump.channels[1] = DMA_REQ_AD1_DEV_ID;
		vbc_pcm_dump.used_dma_channel_name[0] =
			"normal_c_l";
		vbc_pcm_dump.used_dma_channel_name[1] =
			"normal_c_r";
		break;
	case FE_DAI_ID_BTSCO_CAP_AP:
		/*
		 * btsco capture ap using dma data equal normal capture so they
		 * can not coexist
		 */
		vbc_btsco_cap_ap.name = "VBC PCM btsco_cap_ap";
		vbc_btsco_cap_ap.irq_type = SPRD_DMA_BLK_INT;
		vbc_btsco_cap_ap.desc.datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		vbc_btsco_cap_ap.desc.fragmens_len = VBC_AUDRCD01_FRAGMENT;
		vbc_btsco_cap_ap.dev_paddr[0] =
				vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_0 +
				get_ap_vbc_phy_base());
		vbc_btsco_cap_ap.dev_paddr[1] =
			vbc_phy_ap2dsp(VBC_AUDRCD_FIFO_RD_1 +
			get_ap_vbc_phy_base());
		vbc_btsco_cap_ap.channels[0] = DMA_REQ_AD0_DEV_ID;
		vbc_btsco_cap_ap.channels[1] = DMA_REQ_AD1_DEV_ID;
		vbc_btsco_cap_ap.used_dma_channel_name[0] =
			"normal_c_l";
		vbc_btsco_cap_ap.used_dma_channel_name[1] =
			"normal_c_r";
		break;
	}
}

struct sprd_pcm_dma_params *get_dma_data_params(struct snd_soc_dai *fe_dai,
	int stream)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct sprd_pcm_dma_params *dma_data;

	switch (fe_dai->id) {
	case FE_DAI_ID_OFFLOAD:
	case FE_DAI_ID_A2DP_OFFLOAD:
	case FE_DAI_ID_VOICE:
	case FE_DAI_ID_FM:
	case FE_DAI_ID_FM_DSP:
	default:
		dma_data = NULL;
		break;
	case FE_DAI_ID_NORMAL_AP01:
		dma_data = is_playback ? &vbc_pcm_normal_ap01_p :
			&vbc_pcm_normal_ap01_c;
		break;
	case FE_DAI_ID_NORMAL_AP23:
		dma_data = is_playback ? &vbc_pcm_normal_ap23_p :
			&vbc_pcm_normal_ap23_c;
		break;
	case FE_DAI_ID_CAPTURE_DSP:
		dma_data = &pcm_dsp_cap_mcdt;
		break;
	case FE_DAI_ID_FAST_P:
		dma_data = &pcm_fast_play_mcdt;
		break;
	case FE_DAI_ID_VOIP:
		dma_data = is_playback ? &pcm_voip_play_mcdt :
			&pcm_voip_record_mcdt;
		break;
	case FE_DAI_ID_VOICE_CAPTURE:
		dma_data = &vbc_pcm_voice_capture_mcdt;
		break;
	case FE_DAI_ID_LOOP:
		dma_data = is_playback ? &pcm_loop_play_mcdt :
			&pcm_loop_record_mcdt;
		break;
	case FE_DAI_ID_FM_CAPTURE_AP:
		dma_data = &vbc_pcm_fm_caputre;
		break;
	case FE_DAI_ID_A2DP_PCM:
		dma_data = &vbc_pcm_a2dp_p;
		break;
	case FE_DAI_ID_FM_CAP_DSP:
		dma_data = &pcm_dsp_fm_cap_mcdt;
		break;
	case FE_DAI_ID_BTSCO_CAP_DSP:
		dma_data = &pcm_dsp_btsco_cap_mcdt;
		break;
	case FE_DAI_ID_DUMP:
		dma_data = &vbc_pcm_dump;
		break;
	case FE_DAI_ID_BTSCO_CAP_AP:
		dma_data = &vbc_btsco_cap_ap;
		break;
	}

	return dma_data;
}

static int fe_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *fe_dai)
{
	struct sprd_pcm_dma_params *dma_data = NULL;
	int data_fmt;

	int ret;

	pr_info("%s fe dai: %s(%d) %s\n", __func__,
		fe_dai_id_to_str(fe_dai->id),
		fe_dai->id, stream_to_str(substream->stream));

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}

	ret = mcdt_dma_config_init(fe_dai, substream->stream);
	if (ret < 0) {
		pr_err("%s mcdt config init failed\n", __func__);
		agdsp_access_disable();
		return ret;
	}
	agdsp_access_disable();

	sprd_dma_config(substream, params, fe_dai);
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		data_fmt = VBC_DAT_L16;
		pr_err("%s unsupported data format\n", __func__);
		break;
	}
	dma_data = get_dma_data_params(fe_dai, substream->stream);
	if (dma_data) {
		if (VBC_DAT_L24 == data_fmt || 1 == dma_data->use_mcdt)
			dma_data->desc.datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		else if (data_fmt == VBC_DAT_L16)
			dma_data->desc.datawidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
	}

	snd_soc_dai_set_dma_data(fe_dai, substream, dma_data);

	return 0;
}

static int fe_hw_free(struct snd_pcm_substream *substream,
	struct snd_soc_dai *fe_dai)
{
	int ret;

	pr_info("%s fe dai: %s(%d) %s\n", __func__,
		fe_dai_id_to_str(fe_dai->id),
		fe_dai->id, stream_to_str(substream->stream));
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}
	mcdt_dma_deinit(fe_dai, substream->stream);
	agdsp_access_disable();

	return 0;
}

static struct snd_soc_dai_ops sprd_fe_dai_ops = {
	.hw_params = fe_hw_params,
	.hw_free = fe_hw_free,
};

/* add rout for {aif_name, NULL, stream_name} */
static int fe_dai_probe(struct snd_soc_dai *fe_dai)
{
	struct snd_soc_dapm_route intercon;
	struct snd_soc_dapm_context *dapm;

	if (!fe_dai || !fe_dai->driver) {
		pr_err("%s invalid params\n", __func__);
		return -EINVAL;
	}
	dapm = snd_soc_component_get_dapm(fe_dai->component);
	memset(&intercon, 0, sizeof(intercon));
	if (fe_dai->driver->playback.stream_name &&
		fe_dai->driver->playback.aif_name) {
		dev_dbg(fe_dai->dev, "%s add route for widget %s",
			__func__, fe_dai->driver->playback.stream_name);
		intercon.source = fe_dai->driver->playback.stream_name;
		intercon.sink = fe_dai->driver->playback.aif_name;
		dev_dbg(fe_dai->dev, "%s src %s sink %s\n",
			__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(dapm, &intercon, 1);
	}
	if (fe_dai->driver->capture.stream_name &&
		fe_dai->driver->capture.aif_name) {
		dev_dbg(fe_dai->dev, "%s add route for widget %s",
			__func__, fe_dai->driver->capture.stream_name);
		intercon.sink = fe_dai->driver->capture.stream_name;
		intercon.source = fe_dai->driver->capture.aif_name;
		dev_dbg(fe_dai->dev, "%s src %s sink %s\n",
			__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(dapm, &intercon, 1);
	}

	return 0;
}

static const struct snd_soc_component_driver sprd_fe_dai_component = {
	.name = "sprd-dai-fe",
};

static struct snd_soc_dai_driver sprd_fe_dais[FE_DAI_ID_MAX] = {
	/* 0: FE_DAI_ID_NORMAL_AP01 */
	{
		.id = FE_DAI_ID_NORMAL_AP01,
		.name = TO_STRING(FE_DAI_ID_NORMAL_AP01),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_NORMAL_AP01_P",
			.aif_name = "FE_IF_NORMAL_AP01_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "FE_DAI_NORMAL_AP01_C",
			.aif_name = "FE_IF_NORMAL_AP01_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 1: FE_DAI_ID_NORMAL_AP23 */
	{
		.id = FE_DAI_ID_NORMAL_AP23,
		.name = TO_STRING(FE_DAI_ID_NORMAL_AP23),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_NORMAL_AP23_P",
			.aif_name = "FE_IF_NORMAL_AP23_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "FE_DAI_NORMAL_AP23_C",
			.aif_name = "FE_IF_NORMAL_AP23_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 2: FE_DAI_ID_CAPTURE_DSP */
	{
		.id = FE_DAI_ID_CAPTURE_DSP,
		.name = TO_STRING(FE_DAI_ID_CAPTURE_DSP),
		.probe = fe_dai_probe,
		.capture = {
			.stream_name = "FE_DAI_CAP_DSP_C",
			.aif_name = "FE_IF_CAP_DSP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 3: FE_DAI_ID_FAST_P */
	{
		.id = FE_DAI_ID_FAST_P,
		.name = TO_STRING(FE_DAI_ID_FAST_P),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_FAST_P",
			.aif_name = "FE_IF_FAST_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 4: FE_DAI_ID_OFFLOAD */
	{
		.id = FE_DAI_ID_OFFLOAD,
		.name = TO_STRING(FE_DAI_ID_OFFLOAD),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_OFFLOAD_P",
			.aif_name = "FE_IF_OFFLOAD_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.compress_new = snd_soc_new_compress,
		.ops = &sprd_fe_dai_ops,
	},
	/* 5: FE_DAI_ID_VOICE */
	{
		.id = FE_DAI_ID_VOICE,
		.name = TO_STRING(FE_DAI_ID_VOICE),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_VOICE_P",
			.aif_name = "FE_IF_VOICE_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "FE_DAI_VOICE_C",
			.aif_name = "FE_IF_VOICE_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 6: FE_DAI_ID_VOIP */
	{
		.id = FE_DAI_ID_VOIP,
		.name = TO_STRING(FE_DAI_ID_VOIP),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_VOIP_P",
			.aif_name = "FE_IF_VOIP_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "FE_DAI_VOIP_C",
			.aif_name = "FE_IF_VOIP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 7: FE_DAI_ID_FM */
	{
		.id = FE_DAI_ID_FM,
		.name = TO_STRING(FE_DAI_ID_FM),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_FM_P",
			.aif_name = "FE_IF_FM_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 8: FE_DAI_ID_FM_CAPTURE_AP */
	{
		.id = FE_DAI_ID_FM_CAPTURE_AP,
		.name = TO_STRING(FE_DAI_ID_FM_CAPTURE_AP),
		.probe = fe_dai_probe,
		.capture = {
			.stream_name = "FE_DAI_FM_CAP_C",
			.aif_name = "FE_IF_FM_CAP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 9: FE_DAI_ID_VOICE_CAPTURE */
	{
		.id = FE_DAI_ID_VOICE_CAPTURE,
		.name = TO_STRING(FE_DAI_ID_VOICE_CAPTURE),
		.probe = fe_dai_probe,
		.capture = {
			.stream_name = "FE_DAI_VOICE_CAP_C",
			.aif_name = "FE_IF_VOICE_CAP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 10: FE_DAI_ID_LOOP */
	{
		.id = FE_DAI_ID_LOOP,
		.name = TO_STRING(FE_DAI_ID_LOOP),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_LOOP_P",
			.aif_name = "FE_IF_LOOP_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "FE_DAI_LOOP_C",
			.aif_name = "FE_IF_LOOP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 11: FE_DAI_ID_A2DP_OFFLOAD */
	{
		.id = FE_DAI_ID_A2DP_OFFLOAD,
		.name = TO_STRING(FE_DAI_ID_A2DP_OFFLOAD),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_A2DP_OFFLOAD_P",
			.aif_name = "FE_IF_A2DP_OFFLOAD_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.compress_new = snd_soc_new_compress,
		.ops = &sprd_fe_dai_ops,
	},
	/* 12: FE_DAI_ID_A2DP_PCM */
	{
		.id = FE_DAI_ID_A2DP_PCM,
		.name = TO_STRING(FE_DAI_ID_A2DP_PCM),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_A2DP_PCM_P",
			.aif_name = "FE_IF_A2DP_PCM_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 13: FE_DAI_ID_FM_CAP_DSP */
	{
		.id = FE_DAI_ID_FM_CAP_DSP,
		.name = TO_STRING(FE_DAI_ID_FM_CAP_DSP),
		.probe = fe_dai_probe,
		.capture = {
			.stream_name = "FE_DAI_FM_CAP_DSP_C",
			.aif_name = "FE_IF_FM_CAP_DSP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 14: FE_DAI_ID_BTSCO_CAP_DSP */
	{
		.id = FE_DAI_ID_BTSCO_CAP_DSP,
		.name = TO_STRING(FE_DAI_ID_BTSCO_CAP_DSP),
		.probe = fe_dai_probe,
		.capture = {
			.stream_name = "FE_DAI_BTSCO_CAP_DSP_C",
			.aif_name = "FE_IF_BTSCO_CAP_DSP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 15: FE_DAI_ID_FM_DSP */
	{
		.id = FE_DAI_ID_FM_DSP,
		.name = TO_STRING(FE_DAI_ID_FM_DSP),
		.probe = fe_dai_probe,
		.playback = {
			.stream_name = "FE_DAI_FM_DSP_P",
			.aif_name = "FE_IF_FM_DSP_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 16: FE_DAI_ID_DUMP */
	{
		.id = FE_DAI_ID_DUMP,
		.name = TO_STRING(FE_DAI_ID_DUMP),
		.probe = fe_dai_probe,
		.capture = {
			.stream_name = "FE_DAI_DUMP_C",
			.aif_name = "FE_IF_DUMP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/*17 FE_DAI_ID_BTSCO_CAP_AP */
	{
		.id = FE_DAI_ID_BTSCO_CAP_AP,
		.name = TO_STRING(FE_DAI_ID_BTSCO_CAP_AP),
		.probe = fe_dai_probe,
		.capture = {
			.stream_name = "FE_DAI_BTCAP_AP_C",
			.aif_name = "FE_IF_BTCAP_AP_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.ops = &sprd_fe_dai_ops,
	},
	/* 18: FE_DAI_ID_CODEC_TEST */
	{
		.id = FE_DAI_ID_CODEC_TEST,
		.name = TO_STRING(FE_DAI_ID_CODEC_TEST),
		.playback = {
			.stream_name = "FE_DAI_CODEC_TEST_P",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
		.capture = {
			.stream_name = "FE_DAI_CODEC_TEST_C",
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE),
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
		},
	},
};
static int sprd_fe_dai_dev_probe(struct platform_device *pdev)
{
	int ret;

	dev_dbg(&pdev->dev, "%s: dev name %s\n", __func__,
		dev_name(&pdev->dev));
	ret = snd_soc_register_component(&pdev->dev, &sprd_fe_dai_component,
		sprd_fe_dais, ARRAY_SIZE(sprd_fe_dais));

	return ret;
}

static int sprd_fe_dai_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id sprd_dai_fe_dt_match[] = {
	{.compatible = "sprd,fe-dai"},
};

static struct platform_driver sprd_fe_dai_driver = {
	.probe  = sprd_fe_dai_dev_probe,
	.remove = sprd_fe_dai_dev_remove,
	.driver = {
		.name = "vbc-dai-fe",
		.owner = THIS_MODULE,
		.of_match_table = sprd_dai_fe_dt_match,
	},
};

static int __init sprd_fe_dai_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&sprd_fe_dai_driver);
	if (ret)
		pr_err("%s sprd fe dai driver failed\n",
			__func__);

	return ret;
}

late_initcall(sprd_fe_dai_driver_init);
MODULE_DESCRIPTION("SPRD ASoC FRONT END CPU DAI");
MODULE_AUTHOR("Lei Ning <lei.ning@unisoc.com>");
MODULE_LICENSE("GPL");

