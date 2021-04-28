/*
 * sound/soc/sprd/platform/
 *
 * SpreadTrum pcm routing
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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/suspend.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "agdsp_access.h"

enum SPRD_BE_SWITCH {
	S_NORMAL_AP01_P_CODEC = 0,
	S_NORMAL_AP01_C_CODEC,
	S_NORMAL_AP23_P_CODEC,
	S_NORMAL_AP23_C_CODEC,
	S_CAPTURE_DSP_CODEC,
	S_FAST_P_CODEC,
	S_OFFLOAD_CODEC,
	S_VOICE_P_CODEC,
	S_VOICE_C_CODEC,
	S_VOIP_P_CODEC,
	S_VOIP_C_CODEC,
	S_FM_CODEC,
	S_LOOP_P_CODEC,
	S_LOOP_C_CODEC,
	S_FM_DSP_CODEC,
	S_NORMAL_AP01_P_USB,
	S_NORMAL_AP01_C_USB,
	S_NORMAL_AP23_P_USB,
	S_NORMAL_AP23_C_USB,
	S_CAPTURE_DSP_USB,
	S_FAST_P_USB,
	S_OFFLOAD_USB,
	S_VOICE_P_USB,
	S_VOICE_C_USB,
	S_VOIP_P_USB,
	S_VOIP_C_USB,
	S_FM_USB,
	S_LOOP_P_USB,
	S_LOOP_C_USB,
	S_FM_DSP_USB,
	S_OFFLOAD_A2DP,
	S_PCM_A2DP,
	S_VOICE_P_BT,
	S_VOICE_C_BT,
	S_VOIP_P_BT,
	S_VOIP_C_BT,
	S_LOOP_P_BT,
	S_LOOP_C_BT,
	S_CAPTURE_BT,
	S_FAST_P_BT,
	S_NORMAL_AP01_P_BT,
	S_NORMAL_AP01_P_HIFI,
	S_NORMAL_AP23_P_HIFI,
	S_FAST_P_HIFI,
	S_OFFLOAD_HIFI,
	S_VOICE_P_HIFI,
	S_VOIP_P_HIFI,
	S_FM_HIFI,
	S_LOOP_P_HIFI,
	S_FM_DSP_HIFI,
	S_VOICE_CAP_C,
	S_FM_CAP_C,
	S_FM_CAP_DSP_C,
	S_BTSCO_CAP_DSP_C,
	S_VBC_DUMP,
	S_CODEC_TEST_C,
	S_CODEC_TEST_P,
	S_SWITCH_CASE_MAX,
};

static const struct snd_kcontrol_new sprd_audio_be_switch[S_SWITCH_CASE_MAX] = {
	[S_NORMAL_AP01_P_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP01_C_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP23_P_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP23_C_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_CAPTURE_DSP_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FAST_P_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_OFFLOAD_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOICE_P_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOICE_C_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOIP_P_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOIP_C_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FM_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_LOOP_P_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_LOOP_C_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FM_DSP_CODEC] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP01_P_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP01_C_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP23_P_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP23_C_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_CAPTURE_DSP_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FAST_P_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_OFFLOAD_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOICE_P_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOICE_C_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOIP_P_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOIP_C_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FM_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_LOOP_P_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_LOOP_C_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FM_DSP_USB] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_OFFLOAD_A2DP] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_PCM_A2DP] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOICE_P_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOICE_C_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOIP_P_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VOIP_C_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_LOOP_P_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_LOOP_C_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_CAPTURE_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FAST_P_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP01_P_BT] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_NORMAL_AP01_P_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM,
						  0, 1, 0),
	[S_NORMAL_AP23_P_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM,
						  0, 1, 0),
	[S_FAST_P_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0, 1, 0),
	[S_OFFLOAD_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0, 1, 0),
	[S_VOICE_P_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0, 1, 0),
	[S_VOIP_P_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0, 1, 0),
	[S_FM_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0, 1, 0),
	[S_LOOP_P_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0, 1, 0),
	[S_FM_DSP_HIFI] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0, 1, 0),
	[S_VOICE_CAP_C] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FM_CAP_C] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_FM_CAP_DSP_C] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_BTSCO_CAP_DSP_C] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_VBC_DUMP] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_CODEC_TEST_C] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
		1, 0),
	[S_CODEC_TEST_P] = SOC_DAPM_SINGLE("SWITCH", SND_SOC_NOPM, 0,
	1, 0),

};

static const char *get_event_name(int event)
{
	const char *ev_name;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ev_name = "PRE_PMU";
		break;
	case SND_SOC_DAPM_POST_PMU:
		ev_name = "POST_PMU";
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ev_name = "PRE_PMD";
		break;
	case SND_SOC_DAPM_POST_PMD:
		ev_name = "POST_PMD";
		break;
	default:
		WARN_ON(1);
		return NULL;
	}

	return ev_name;
}

static int be_switch_evt(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	pr_debug("wname %s %s\n", w->name, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

/* For aif Stream name must contains substring of front-end dai name so that
 * snd_soc_dapm_link_dai_widgets
 * can add a path for dai widget when snd_soc_instantiate_card is called.
 */
static const struct snd_soc_dapm_widget sprd_pcm_routing_widgets[] = {
	/* Frontend AIF */
	SND_SOC_DAPM_AIF_IN("FE_IF_NORMAL_AP01_P", "FE_DAI_NORMAL_AP01_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_NORMAL_AP01_C", "FE_DAI_NORMAL_AP01_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_NORMAL_AP23_P", "FE_DAI_NORMAL_AP23_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_NORMAL_AP23_C", "FE_DAI_NORMAL_AP23_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_CAP_DSP_C", "FE_DAI_CAP_DSP_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_FAST_P", "FE_DAI_FAST_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_OFFLOAD_P", "FE_DAI_OFFLOAD_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_VOICE_P", "FE_DAI_VOICE_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_VOICE_C", "FE_DAI_VOICE_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_VOIP_P", "FE_DAI_VOIP_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_VOIP_C", "FE_DAI_VOIP_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_FM_P", "FE_DAI_FM_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_FM_DSP_P", "FE_DAI_FM_DSP_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_FM_CAP_C", "FE_DAI_FM_CAP_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_FM_CAP_DSP_C", "FE_DAI_FM_CAP_DSP_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_BTSCO_CAP_DSP_C", "FE_DAI_BTSCO_CAP_DSP_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_VOICE_CAP_C", "FE_DAI_VOICE_CAP_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_LOOP_P", "FE_DAI_LOOP_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_LOOP_C", "FE_DAI_LOOP_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_A2DP_OFFLOAD_P", "FE_DAI_A2DP_OFFLOAD_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("FE_IF_A2DP_PCM_P", "FE_DAI_A2DP_PCM_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_DUMP_C", "FE_DAI_DUMP_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("FE_IF_BTCAP_AP_C", "FE_DAI_BTCAP_AP_C",
		0, 0, 0, 0),
	/* Backend AIF */
	SND_SOC_DAPM_AIF_IN("BE_IF_NORMAL_AP01_CODEC_P",
		"BE_DAI_NORMAL_AP01_CODEC_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_NORMAL_AP01_CODEC_C",
		"BE_DAI_NORMAL_AP01_CODEC_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_NORMAL_AP23_CODEC_P",
		"BE_DAI_NORMAL_AP23_CODEC_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_NORMAL_AP23_CODEC_C",
		"BE_DAI_NORMAL_AP23_CODEC_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_CAP_DSP_CODEC_C", "BE_DAI_CAP_DSP_CODEC_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_FAST_CODEC_P", "BE_DAI_FAST_CODEC_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_OFFLOAD_CODEC_P", "BE_DAI_OFFLOAD_CODEC_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_VOICE_CODEC_P", "BE_DAI_VOICE_CODEC_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_VOICE_CODEC_C", "BE_DAI_VOICE_CODEC_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_VOIP_CODEC_P", "BE_DAI_VOIP_CODEC_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_VOIP_CODEC_C", "BE_DAI_VOIP_CODEC_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_FM_CODEC_P", "BE_DAI_FM_CODEC_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_FM_DSP_CODEC_P", "BE_DAI_FM_DSP_CODEC_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_LOOP_CODEC_P", "BE_DAI_LOOP_CODEC_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_LOOP_CODEC_C", "BE_DAI_LOOP_CODEC_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_NORMAL_AP01_USB_P",
		"BE_DAI_NORMAL_AP01_USB_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_NORMAL_AP01_USB_C",
		"BE_DAI_NORMAL_AP01_USB_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_NORMAL_AP23_USB_P",
		"BE_DAI_NORMAL_AP23_USB_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_NORMAL_AP23_USB_C",
		"BE_DAI_NORMAL_AP23_USB_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_CAP_DSP_USB_C", "BE_DAI_CAP_DSP_USB_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_FAST_USB_P", "BE_DAI_FAST_USB_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_OFFLOAD_USB_P", "BE_DAI_OFFLOAD_USB_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_VOICE_USB_P", "BE_DAI_VOICE_USB_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_VOICE_USB_C", "BE_DAI_VOICE_USB_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_VOIP_USB_P", "BE_DAI_VOIP_USB_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_VOIP_USB_C", "BE_DAI_VOIP_USB_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_FM_USB_P", "BE_DAI_FM_USB_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_FM_DSP_USB_P", "BE_DAI_FM_DSP_USB_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_LOOP_USB_P", "BE_DAI_LOOP_USB_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_LOOP_USB_C", "BE_DAI_LOOP_USB_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_OFFLOAD_A2DP_P", "BE_DAI_OFFLOAD_A2DP_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_PCM_A2DP_P", "BE_DAI_PCM_A2DP_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_VOICE_BT_P", "BE_DAI_VOICE_BT_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_VOICE_BT_C", "BE_DAI_VOICE_BT_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_VOIP_BT_P", "BE_DAI_VOIP_BT_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_VOIP_BT_C", "BE_DAI_VOIP_BT_C", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_LOOP_BT_P", "BE_DAI_LOOP_BT_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_LOOP_BT_C", "BE_DAI_LOOP_BT_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_CAP_BT_C", "BE_DAI_CAP_BT_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_FAST_BTSCO_P", "BE_DAI_FAST_BTSCO_P",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_NORMAL_AP01_BTSCO_P",
			    "BE_DAI_NORMAL_AP01_BTSCO_P", 0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_NORMAL_AP01_HIFI_P",
			    "BE_DAI_ID_NORMAL_AP01_P_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_NORMAL_AP23_HIFI",
			    "BE_DAI_ID_NORMAL_AP23_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_FAST_P_HIFI", "BE_DAI_ID_FAST_P_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_OFFLOAD_HIFI", "BE_DAI_ID_OFFLOAD_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_VOICE_HIFI", "BE_DAI_ID_VOICE_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_VOIP_HIFI", "BE_DAI_ID_VOIP_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_FM_HIFI", "BE_DAI_ID_FM_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_LOOP_HIFI", "BE_DAI_ID_LOOP_HIFI",
			    0, 0, 0, 0),
	SND_SOC_DAPM_AIF_IN("BE_IF_ID_FM_DSP_HIFI", "BE_DAI_ID_FM_DSP_HIFI",
			    0, 0, 0, 0),

	SND_SOC_DAPM_AIF_OUT("BE_IF_VOICE_CAP_C", "BE_DAI_VOICE_CAP_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_CAP_FM_CAP_C", "BE_DAI_FM_CAP_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_CAP_DSP_FM_C", "BE_DAI_CAP_DSP_FM_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_CAP_DSP_BTSCO_C", "BE_DAI_CAP_DSP_BTSCO_C",
		0, 0, 0, 0),
	SND_SOC_DAPM_AIF_OUT("BE_IF_DUMP_C", "BE_DAI_DUMP_C", 0, 0, 0, 0),
	/* Switches */
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP01_P_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP01_P_CODEC]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP01_C_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP01_C_CODEC]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP23_P_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP23_P_CODEC]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP23_C_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP23_C_CODEC]),
	SND_SOC_DAPM_SWITCH("S_CAPTURE_DSP_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_CAPTURE_DSP_CODEC]),
	SND_SOC_DAPM_SWITCH("S_FAST_P_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FAST_P_CODEC]),
	SND_SOC_DAPM_SWITCH("S_OFFLOAD_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_OFFLOAD_CODEC]),
	SND_SOC_DAPM_SWITCH("S_VOICE_P_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOICE_P_CODEC]),
	SND_SOC_DAPM_SWITCH("S_VOICE_C_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOICE_C_CODEC]),
	SND_SOC_DAPM_SWITCH("S_VOIP_P_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOIP_P_CODEC]),
	SND_SOC_DAPM_SWITCH("S_VOIP_C_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOIP_C_CODEC]),
	SND_SOC_DAPM_SWITCH("S_FM_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FM_CODEC]),
	SND_SOC_DAPM_SWITCH("S_LOOP_P_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_LOOP_P_CODEC]),
	SND_SOC_DAPM_SWITCH("S_LOOP_C_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_LOOP_C_CODEC]),
	SND_SOC_DAPM_SWITCH("S_FM_DSP_CODEC", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FM_DSP_CODEC]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP01_P_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP01_P_USB]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP01_C_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP01_C_USB]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP23_P_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP23_P_USB]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP23_C_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP23_C_USB]),
	SND_SOC_DAPM_SWITCH("S_CAPTURE_DSP_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_CAPTURE_DSP_USB]),
	SND_SOC_DAPM_SWITCH("S_FAST_P_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FAST_P_USB]),
	SND_SOC_DAPM_SWITCH("S_OFFLOAD_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_OFFLOAD_USB]),
	SND_SOC_DAPM_SWITCH("S_VOICE_P_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOICE_P_USB]),
	SND_SOC_DAPM_SWITCH("S_VOICE_C_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOICE_C_USB]),
	SND_SOC_DAPM_SWITCH("S_VOIP_P_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOIP_P_USB]),
	SND_SOC_DAPM_SWITCH("S_VOIP_C_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOIP_C_USB]),
	SND_SOC_DAPM_SWITCH("S_FM_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FM_USB]),
	SND_SOC_DAPM_SWITCH("S_LOOP_P_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_LOOP_P_USB]),
	SND_SOC_DAPM_SWITCH("S_LOOP_C_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_LOOP_C_USB]),
	SND_SOC_DAPM_SWITCH("S_FM_DSP_USB", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FM_DSP_USB]),
	SND_SOC_DAPM_SWITCH("S_OFFLOAD_A2DP", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_OFFLOAD_A2DP]),
	SND_SOC_DAPM_SWITCH("S_PCM_A2DP", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_PCM_A2DP]),
	SND_SOC_DAPM_SWITCH("S_VOICE_P_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOICE_P_BT]),
	SND_SOC_DAPM_SWITCH("S_VOICE_C_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOICE_C_BT]),
	SND_SOC_DAPM_SWITCH("S_VOIP_P_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOIP_P_BT]),
	SND_SOC_DAPM_SWITCH("S_VOIP_C_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOIP_C_BT]),
	SND_SOC_DAPM_SWITCH("S_LOOP_P_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_LOOP_P_BT]),
	SND_SOC_DAPM_SWITCH("S_LOOP_C_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_LOOP_C_BT]),
	SND_SOC_DAPM_SWITCH("S_CAPTURE_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_CAPTURE_BT]),
	SND_SOC_DAPM_SWITCH("S_FAST_P_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FAST_P_BT]),
	SND_SOC_DAPM_SWITCH("S_NORMAL_AP01_P_BT", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_NORMAL_AP01_P_BT]),
	SND_SOC_DAPM_SWITCH_E("S_NORMAL_AP01_P_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_NORMAL_AP01_P_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_NORMAL_AP23_P_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_NORMAL_AP23_P_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_FAST_P_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_FAST_P_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_OFFLOAD_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_OFFLOAD_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_VOICE_P_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_VOICE_P_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_VOIP_P_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_VOIP_P_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_FM_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_FM_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_LOOP_P_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_LOOP_P_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH_E("S_FM_DSP_HIFI", SND_SOC_NOPM, 0, 0,
			      &sprd_audio_be_switch[S_FM_DSP_HIFI],
			      be_switch_evt,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("S_VOICE_CAP_C", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VOICE_CAP_C]),
	SND_SOC_DAPM_SWITCH("S_FM_CAP_C", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FM_CAP_C]),
	SND_SOC_DAPM_SWITCH("S_FM_CAP_DSP_C", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_FM_CAP_DSP_C]),
	SND_SOC_DAPM_SWITCH("S_BTSCO_CAP_DSP_C", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_BTSCO_CAP_DSP_C]),
	SND_SOC_DAPM_SWITCH("S_VBC_DUMP", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_VBC_DUMP]),
	SND_SOC_DAPM_SWITCH("S_CODEC_TEST_C", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_CODEC_TEST_C]),
	SND_SOC_DAPM_SWITCH("S_CODEC_TEST_P", SND_SOC_NOPM,
		0, 0, &sprd_audio_be_switch[S_CODEC_TEST_P]),
};

/*
 * This graph route FE to BE. FE connect BE though FE cpu_dai_widget to
 * BE cpu_dai_widget or codec_dai_widget.
 * Add route FE_IF_NORMAL_P <- FE_DAI_NORMAL_P in fe_dai_probe.
 * NORMAL_P connect FE_IF_NORMAL_P and BE_IF_NORMAL_P.
 * Add route BE_NORMAL_DAI_P <- BE_DAI_NORMAL_P in sprd_dai_vbc_probe.
 * Normal-Playback <-BE_DAI_NORMAL_P linked by BE through
 * vbc NORMAL dai and codec NORMAL dai.
 */
static const struct snd_soc_dapm_route sprd_pcm_routing_intercon[] = {
	/* S_NORMAL_AP01_P_CODEC */
	{"S_NORMAL_AP01_P_CODEC", "SWITCH", "FE_IF_NORMAL_AP01_P"},
	{"BE_IF_NORMAL_AP01_CODEC_P", NULL, "S_NORMAL_AP01_P_CODEC"},
	/* S_NORMAL_AP01_C_CODEC */
	{"S_NORMAL_AP01_C_CODEC", "SWITCH", "BE_IF_NORMAL_AP01_CODEC_C"},
	{"FE_IF_NORMAL_AP01_C", NULL, "S_NORMAL_AP01_C_CODEC"},
	/* S_NORMAL_AP23_P_CODEC */
	{"S_NORMAL_AP23_P_CODEC", "SWITCH", "FE_IF_NORMAL_AP23_P"},
	{"BE_IF_NORMAL_AP23_CODEC_P", NULL, "S_NORMAL_AP23_P_CODEC"},
	/* S_NORMAL_AP23_C_CODEC */
	{"S_NORMAL_AP23_C_CODEC", "SWITCH", "BE_IF_NORMAL_AP23_CODEC_C"},
	{"FE_IF_NORMAL_AP23_C", NULL, "S_NORMAL_AP23_C_CODEC"},
	/* S_CAPTURE_DSP_CODEC */
	{"S_CAPTURE_DSP_CODEC", "SWITCH", "BE_IF_CAP_DSP_CODEC_C"},
	{"FE_IF_CAP_DSP_C", NULL, "S_CAPTURE_DSP_CODEC"},
	/* S_FAST_P_CODEC */
	{"S_FAST_P_CODEC", "SWITCH", "FE_IF_FAST_P"},
	{"BE_IF_FAST_CODEC_P", NULL, "S_FAST_P_CODEC"},
	/* S_OFFLOAD_CODEC */
	{"S_OFFLOAD_CODEC", "SWITCH", "FE_IF_OFFLOAD_P"},
	{"BE_IF_OFFLOAD_CODEC_P", NULL, "S_OFFLOAD_CODEC"},
	/* S_VOICE_P_CODEC */
	{"S_VOICE_P_CODEC", "SWITCH", "FE_IF_VOICE_P"},
	{"BE_IF_VOICE_CODEC_P", NULL, "S_VOICE_P_CODEC"},
	/* S_VOICE_C_CODEC */
	{"S_VOICE_C_CODEC", "SWITCH", "BE_IF_VOICE_CODEC_C"},
	{"FE_IF_VOICE_C", NULL, "S_VOICE_C_CODEC"},
	/* S_VOIP_P_CODEC */
	{"S_VOIP_P_CODEC", "SWITCH", "FE_IF_VOIP_P"},
	{"BE_IF_VOIP_CODEC_P", NULL, "S_VOIP_P_CODEC"},
	/* S_VOIP_C_CODEC */
	{"S_VOIP_C_CODEC", "SWITCH", "BE_IF_VOIP_CODEC_C"},
	{"FE_IF_VOIP_C", NULL, "S_VOIP_C_CODEC"},
	/* S_FM_CODEC */
	{"S_FM_CODEC", "SWITCH", "FE_IF_FM_P"},
	{"BE_IF_FM_CODEC_P", NULL, "S_FM_CODEC"},
	/* S_LOOP_P_CODEC */
	{"S_LOOP_P_CODEC", "SWITCH", "FE_IF_LOOP_P"},
	{"BE_IF_LOOP_CODEC_P", NULL, "S_LOOP_P_CODEC"},
	/* S_FM_DSP_CODEC */
	{"S_FM_DSP_CODEC", "SWITCH", "FE_IF_FM_DSP_P"},
	{"BE_IF_FM_DSP_CODEC_P", NULL, "S_FM_DSP_CODEC"},
	/* S_LOOP_C_CODEC */
	{"S_LOOP_C_CODEC", "SWITCH", "BE_IF_LOOP_CODEC_C"},
	{"FE_IF_LOOP_C", NULL, "S_LOOP_C_CODEC"},
	/* S_NORMAL_AP01_P_USB */
	{"S_NORMAL_AP01_P_USB", "SWITCH", "FE_IF_NORMAL_AP01_P"},
	{"BE_IF_NORMAL_AP01_USB_P", NULL, "S_NORMAL_AP01_P_USB"},
	/* S_NORMAL_AP01_C_USB */
	{"S_NORMAL_AP01_C_USB", "SWITCH", "BE_IF_NORMAL_AP01_USB_C"},
	{"FE_IF_NORMAL_AP01_C", NULL, "S_NORMAL_AP01_C_USB"},
	/* S_NORMAL_AP23_P_USB */
	{"S_NORMAL_AP23_P_USB", "SWITCH", "FE_IF_NORMAL_AP23_P"},
	{"BE_IF_NORMAL_AP23_USB_P", NULL, "S_NORMAL_AP23_P_USB"},
	/* S_NORMAL_AP23_C_USB */
	{"S_NORMAL_AP23_C_USB", "SWITCH", "BE_IF_NORMAL_AP23_USB_C"},
	{"FE_IF_NORMAL_AP23_C", NULL, "S_NORMAL_AP23_C_USB"},
	/* S_CAPTURE_DSP_USB */
	{"S_CAPTURE_DSP_USB", "SWITCH", "BE_IF_CAP_DSP_USB_C"},
	{"FE_IF_CAP_DSP_C", NULL, "S_CAPTURE_DSP_USB"},
	/* S_FAST_P_USB */
	{"S_FAST_P_USB", "SWITCH", "FE_IF_FAST_P"},
	{"BE_IF_FAST_USB_P", NULL, "S_FAST_P_USB"},
	/* S_OFFLOAD_USB */
	{"S_OFFLOAD_USB", "SWITCH", "FE_IF_OFFLOAD_P"},
	{"BE_IF_OFFLOAD_USB_P", NULL, "S_OFFLOAD_USB"},
	/* S_VOICE_P_USB */
	{"S_VOICE_P_USB", "SWITCH", "FE_IF_VOICE_P"},
	{"BE_IF_VOICE_USB_P", NULL, "S_VOICE_P_USB"},
	/* S_VOICE_C_USB */
	{"S_VOICE_C_USB", "SWITCH", "BE_IF_VOICE_USB_C"},
	{"FE_IF_VOICE_C", NULL, "S_VOICE_C_USB"},
	/* S_VOIP_P_USB */
	{"S_VOIP_P_USB", "SWITCH", "FE_IF_VOIP_P"},
	{"BE_IF_VOIP_USB_P", NULL, "S_VOIP_P_USB"},
	/* S_VOIP_C_USB */
	{"S_VOIP_C_USB", "SWITCH", "BE_IF_VOIP_USB_C"},
	{"FE_IF_VOIP_C", NULL, "S_VOIP_C_USB"},
	/* S_FM_USB */
	{"S_FM_USB", "SWITCH", "FE_IF_FM_P"},
	{"BE_IF_FM_USB_P", NULL, "S_FM_USB"},
	/* S_LOOP_P_USB */
	{"S_LOOP_P_USB", "SWITCH", "FE_IF_LOOP_P"},
	{"BE_IF_LOOP_USB_P", NULL, "S_LOOP_P_USB"},
	/* S_LOOP_C_USB */
	{"S_LOOP_C_USB", "SWITCH", "BE_IF_LOOP_USB_C"},
	{"FE_IF_LOOP_C", NULL, "S_LOOP_C_USB"},
	/* S_FM_DSP_USB */
	{"S_FM_DSP_USB", "SWITCH", "FE_IF_FM_DSP_P"},
	{"BE_IF_FM_DSP_USB_P", NULL, "S_FM_DSP_USB"},
	/* S_OFFLOAD_A2DP */
	{"S_OFFLOAD_A2DP", "SWITCH", "FE_IF_A2DP_OFFLOAD_P"},
	{"BE_IF_OFFLOAD_A2DP_P", NULL, "S_OFFLOAD_A2DP"},
	/* S_PCM_A2DP */
	{"S_PCM_A2DP", "SWITCH", "FE_IF_A2DP_PCM_P"},
	{"BE_IF_PCM_A2DP_P", NULL, "S_PCM_A2DP"},
	/* S_VOICE_P_BT */
	{"S_VOICE_P_BT", "SWITCH", "FE_IF_VOICE_P"},
	{"BE_IF_VOICE_BT_P", NULL, "S_VOICE_P_BT"},
	/* S_VOICE_C_BT */
	{"S_VOICE_C_BT", "SWITCH", "BE_IF_VOICE_BT_C"},
	{"FE_IF_VOICE_C", NULL, "S_VOICE_C_BT"},
	/* S_VOIP_P_BT */
	{"S_VOIP_P_BT", "SWITCH", "FE_IF_VOIP_P"},
	{"BE_IF_VOIP_BT_P", NULL, "S_VOIP_P_BT"},
	/* S_VOIP_C_BT */
	{"S_VOIP_C_BT", "SWITCH", "BE_IF_VOIP_BT_C"},
	{"FE_IF_VOIP_C", NULL, "S_VOIP_C_BT"},
	/* S_LOOP_P_BT */
	{"S_LOOP_P_BT", "SWITCH", "FE_IF_LOOP_P"},
	{"BE_IF_LOOP_BT_P", NULL, "S_LOOP_P_BT"},
	/* S_LOOP_C_BT */
	{"S_LOOP_C_BT", "SWITCH", "BE_IF_LOOP_BT_C"},
	{"FE_IF_LOOP_C", NULL, "S_LOOP_C_BT"},
	/* S_CAPTURE_BT */
	{"S_CAPTURE_BT", "SWITCH", "BE_IF_CAP_BT_C"},
	{"FE_IF_BTCAP_AP_C", NULL, "S_CAPTURE_BT"},
	/* S_FAST_P_BT */
	{"S_FAST_P_BT", "SWITCH", "FE_IF_FAST_P"},
	{"BE_IF_FAST_BTSCO_P", NULL, "S_FAST_P_BT"},
	/* S_NORMAL_AP01_P_BT */
	{"S_NORMAL_AP01_P_BT", "SWITCH", "FE_IF_NORMAL_AP01_P"},
	{"BE_IF_NORMAL_AP01_BTSCO_P", NULL, "S_NORMAL_AP01_P_BT"},

	/* S_NORMAL_AP01_P_HIFI */
	{"S_NORMAL_AP01_P_HIFI", "SWITCH", "FE_IF_NORMAL_AP01_P"},
	{"BE_IF_NORMAL_AP01_HIFI_P", NULL, "S_NORMAL_AP01_P_HIFI"},
	/* S_NORMAL_AP23_P_HIFI */
	{"S_NORMAL_AP23_P_HIFI", "SWITCH", "FE_IF_NORMAL_AP23_P"},
	{"BE_IF_ID_NORMAL_AP23_HIFI", NULL, "S_NORMAL_AP23_P_HIFI"},
	/* S_FAST_P_HIFI */
	{"S_FAST_P_HIFI", "SWITCH", "FE_IF_FAST_P"},
	{"BE_IF_ID_FAST_P_HIFI", NULL, "S_FAST_P_HIFI"},
	/* S_OFFLOAD_HIFI */
	{"S_OFFLOAD_HIFI", "SWITCH", "FE_IF_OFFLOAD_P"},
	{"BE_IF_ID_OFFLOAD_HIFI", NULL, "S_OFFLOAD_HIFI"},
	/* S_VOICE_P_HIFI */
	{"S_VOICE_P_HIFI", "SWITCH", "FE_IF_VOICE_P"},
	{"BE_IF_ID_VOICE_HIFI", NULL, "S_VOICE_P_HIFI"},
	/* S_VOIP_P_HIFI */
	{"S_VOIP_P_HIFI", "SWITCH", "FE_IF_VOIP_P"},
	{"BE_IF_ID_VOIP_HIFI", NULL, "S_VOIP_P_HIFI"},
	/* S_FM_HIFI */
	{"S_FM_HIFI", "SWITCH", "FE_IF_FM_P"},
	{"BE_IF_ID_FM_HIFI", NULL, "S_FM_HIFI"},
	/* S_LOOP_P_HIFI */
	{"S_LOOP_P_HIFI", "SWITCH", "FE_IF_LOOP_P"},
	{"BE_IF_ID_LOOP_HIFI", NULL, "S_LOOP_P_HIFI"},
	/* S_FM_DSP_HIFI */
	{"S_FM_DSP_HIFI", "SWITCH", "FE_IF_FM_DSP_P"},
	{"BE_IF_ID_FM_DSP_HIFI", NULL, "S_FM_DSP_HIFI"},

	/* S_VOICE_CAP_C */
	{"S_VOICE_CAP_C", "SWITCH", "BE_IF_VOICE_CAP_C"},
	{"FE_IF_VOICE_CAP_C", NULL, "S_VOICE_CAP_C"},
	/* S_FM_CAP_C */
	{"S_FM_CAP_C", "SWITCH", "BE_IF_CAP_FM_CAP_C"},
	{"FE_IF_FM_CAP_C", NULL, "S_FM_CAP_C"},
	/* S_FM_CAP_DSP_C */
	{"S_FM_CAP_DSP_C", "SWITCH", "BE_IF_CAP_DSP_FM_C"},
	{"FE_IF_FM_CAP_DSP_C", NULL, "S_FM_CAP_DSP_C"},
	/* S_BTSCO_CAP_DSP_C */
	{"S_BTSCO_CAP_DSP_C", "SWITCH", "BE_IF_CAP_DSP_BTSCO_C"},
	{"FE_IF_BTSCO_CAP_DSP_C", NULL, "S_BTSCO_CAP_DSP_C"},
	/* S_VBC_DUMP */
	{"S_VBC_DUMP", "SWITCH", "BE_IF_DUMP_C"},
	{"FE_IF_DUMP_C", NULL, "S_VBC_DUMP"},
	/* S_CODEC_TEST_C */
	{"S_CODEC_TEST_C", "SWITCH", "CODEC_TEST-Capture"},
	{"FE_DAI_CODEC_TEST_C", NULL, "S_CODEC_TEST_C"},
	/* S_CODEC_TEST_P */
	{"S_CODEC_TEST_P", "SWITCH", "FE_DAI_CODEC_TEST_P"},
	{"CODEC_TEST-Playback", NULL, "S_CODEC_TEST_P"},

};

static struct snd_soc_platform_driver sprd_soc_routing_platform = {
	.component_driver = {
		.dapm_routes = sprd_pcm_routing_intercon,
		.dapm_widgets = sprd_pcm_routing_widgets,
		.num_dapm_widgets = ARRAY_SIZE(sprd_pcm_routing_widgets),
		.num_dapm_routes = ARRAY_SIZE(sprd_pcm_routing_intercon),
	},
};

static int sprd_platform_routing_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev,
		&sprd_soc_routing_platform);
}

static int sprd_platform_routing_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static const struct of_device_id sprd_pcm_routing_of_match[] = {
	{.compatible = "sprd,pcm-routing"}
};

MODULE_DEVICE_TABLE(of, sprd_pcm_routing_of_match);


static struct platform_driver sprd_pcm_routing_driver = {
	.driver = {
		.name = "sprd-pcm-routing",
		.owner = THIS_MODULE,
		.of_match_table = sprd_pcm_routing_of_match,
	},

	.probe = sprd_platform_routing_probe,
	.remove = sprd_platform_routing_remove,
};

module_platform_driver(sprd_pcm_routing_driver);
MODULE_DESCRIPTION("ASoC PCM ROUTING");
MODULE_AUTHOR("Lei Ning <lei.ning@unisoc.com>");
MODULE_LICENSE("GPL");

