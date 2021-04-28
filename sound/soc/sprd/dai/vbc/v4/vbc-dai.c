/*
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
#include "sprd-asoc-debug.h"

#define pr_fmt(fmt) pr_sprd_fmt(" VBC ") "%s: %d:"fmt, __func__, __LINE__

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

#include "audio-sipc.h"
#include "mcdt_hw.h"
#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"
#include "sprd-dmaengine-pcm.h"
#include "sprd-platform-pcm-routing.h"
#include "sprd-string.h"
#include "vbc-phy-v4.h"
#include "audio_mem.h"

/* for vbc define here for pcm define in pcm driver */
#define sprd_is_normal_playback(cpu_dai_id, stream) \
	((((cpu_dai_id) == BE_DAI_ID_NORMAL_AP01_CODEC) ||\
	  ((cpu_dai_id) == BE_DAI_ID_NORMAL_AP01_USB)) && \
	 (stream) == SNDRV_PCM_STREAM_PLAYBACK)


static struct aud_pm_vbc *pm_vbc;
static struct aud_pm_vbc *aud_pm_vbc_get(void);
#define TO_STRING(e) #e

static const char *stream_to_str(int stream)
{
	return (stream == SNDRV_PCM_STREAM_PLAYBACK) ?
		"playback" : "capture";
}

static const char *vbc_data_fmt_to_str(int data_fmt)
{
	const char *str_data_fmt;

	switch (data_fmt) {
	case VBC_DAT_H24:
		str_data_fmt = TO_STRING(VBC_DAT_H24);
		break;
	case VBC_DAT_L24:
		str_data_fmt = TO_STRING(VBC_DAT_L24);
		break;
	case VBC_DAT_H16:
		str_data_fmt = TO_STRING(VBC_DAT_H16);
		break;
	case VBC_DAT_L16:
		str_data_fmt = TO_STRING(VBC_DAT_L16);
		break;
	default:
		str_data_fmt = "";
		break;
	}

	return str_data_fmt;
}

static const char *dai_id_to_str(int dai_id)
{
	const char * const dai_id_str[BE_DAI_ID_MAX] = {
		[BE_DAI_ID_NORMAL_AP01_CODEC] =
			TO_STRING(BE_DAI_ID_NORMAL_AP01_CODEC),
		[BE_DAI_ID_NORMAL_AP23_CODEC] =
			TO_STRING(BE_DAI_ID_NORMAL_AP23_CODEC),
		[BE_DAI_ID_CAPTURE_DSP_CODEC] =
			TO_STRING(BE_DAI_ID_CAPTURE_DSP_CODEC),
		[BE_DAI_ID_FAST_P_CODEC] = TO_STRING(BE_DAI_ID_FAST_P_CODEC),
		[BE_DAI_ID_OFFLOAD_CODEC] = TO_STRING(BE_DAI_ID_OFFLOAD_CODEC),
		[BE_DAI_ID_VOICE_CODEC] = TO_STRING(BE_DAI_ID_VOICE_CODEC),
		[BE_DAI_ID_VOIP_CODEC] = TO_STRING(BE_DAI_ID_VOIP_CODEC),
		[BE_DAI_ID_FM_CODEC] = TO_STRING(BE_DAI_ID_FM_CODEC),
		[BE_DAI_ID_LOOP_CODEC] = TO_STRING(BE_DAI_ID_LOOP_CODEC),
		[BE_DAI_ID_NORMAL_AP01_USB] =
			TO_STRING(BE_DAI_ID_NORMAL_AP01_USB),
		[BE_DAI_ID_NORMAL_AP23_USB] =
			TO_STRING(BE_DAI_ID_NORMAL_AP23_USB),
		[BE_DAI_ID_CAPTURE_DSP_USB] =
			TO_STRING(BE_DAI_ID_CAPTURE_DSP_USB),
		[BE_DAI_ID_FAST_P_USB] = TO_STRING(BE_DAI_ID_FAST_P_USB),
		[BE_DAI_ID_OFFLOAD_USB] = TO_STRING(BE_DAI_ID_OFFLOAD_USB),
		[BE_DAI_ID_VOICE_USB] = TO_STRING(BE_DAI_ID_VOICE_USB),
		[BE_DAI_ID_VOIP_USB] = TO_STRING(BE_DAI_ID_VOIP_USB),
		[BE_DAI_ID_FM_USB] = TO_STRING(BE_DAI_ID_FM_USB),
		[BE_DAI_ID_LOOP_USB] = TO_STRING(BE_DAI_ID_LOOP_USB),
		[BE_DAI_ID_OFFLOAD_A2DP] = TO_STRING(BE_DAI_ID_OFFLOAD_A2DP),
		[BE_DAI_ID_PCM_A2DP] = TO_STRING(BE_DAI_ID_PCM_A2DP),
		[BE_DAI_ID_VOICE_BT] = TO_STRING(BE_DAI_ID_VOICE_BT),
		[BE_DAI_ID_VOIP_BT] = TO_STRING(BE_DAI_ID_VOIP_BT),
		[BE_DAI_ID_LOOP_BT] = TO_STRING(BE_DAI_ID_LOOP_BT),
		[BE_DAI_ID_CAPTURE_BT] = TO_STRING(BE_DAI_ID_CAPTURE_BT),
		[BE_DAI_ID_VOICE_CAPTURE] = TO_STRING(BE_DAI_ID_VOICE_CAPTURE),
		[BE_DAI_ID_FM_CAPTURE] = TO_STRING(BE_DAI_ID_FM_CAPTURE),
		[BE_DAI_ID_FM_CAPTURE_DSP] =
			TO_STRING(BE_DAI_ID_FM_CAPTURE_DSP),
		[BE_DAI_ID_CAPTURE_DSP_BTSCO] =
			TO_STRING(BE_DAI_ID_CAPTURE_DSP_BTSCO),
		[BE_DAI_ID_FM_DSP_CODEC] = TO_STRING(BE_DAI_ID_FM_DSP_CODEC),
		[BE_DAI_ID_FM_DSP_USB] = TO_STRING(BE_DAI_ID_FM_DSP_USB),
		[BE_DAI_ID_DUMP] = TO_STRING(BE_DAI_ID_DUMP),
		[BE_DAI_ID_FAST_P_BTSCO] = TO_STRING(BE_DAI_ID_FAST_P_BTSCO),
		[BE_DAI_ID_NORMAL_AP01_P_BTSCO] =
			TO_STRING(BE_DAI_ID_NORMAL_AP01_P_BTSCO),
		[BE_DAI_ID_NORMAL_AP01_P_HIFI] =
			TO_STRING(BE_DAI_ID_NORMAL_AP01_P_HIFI),
		[BE_DAI_ID_NORMAL_AP23_HIFI] =
			TO_STRING(BE_DAI_ID_NORMAL_AP23_HIFI),
		[BE_DAI_ID_FAST_P_HIFI] = TO_STRING(BE_DAI_ID_FAST_P_HIFI),
		[BE_DAI_ID_OFFLOAD_HIFI] = TO_STRING(BE_DAI_ID_OFFLOAD_HIFI),
		[BE_DAI_ID_VOICE_HIFI] = TO_STRING(BE_DAI_ID_VOICE_HIFI),
		[BE_DAI_ID_VOIP_HIFI] = TO_STRING(BE_DAI_ID_VOIP_HIFI),
		[BE_DAI_ID_FM_HIFI] = TO_STRING(BE_DAI_ID_FM_HIFI),
		[BE_DAI_ID_LOOP_HIFI] = TO_STRING(BE_DAI_ID_LOOP_HIFI),
		[BE_DAI_ID_FM_DSP_HIFI] = TO_STRING(BE_DAI_ID_FM_DSP_HIFI),
	};

	if (dai_id >= BE_DAI_ID_MAX) {
		pr_err("invalid dai_id %d\n", dai_id);
		return "";
	}
	if (!dai_id_str[dai_id]) {
		pr_err("null dai_id string dai_id=%d\n", dai_id);
		return "";
	}

	return dai_id_str[dai_id];
}

static const char *scene_id_to_str(int scene_id)
{
	const char *scene_id_str[VBC_DAI_ID_MAX] = {
		[VBC_DAI_ID_NORMAL_AP01] = TO_STRING(VBC_DAI_ID_NORMAL_AP01),
		[VBC_DAI_ID_NORMAL_AP23] = TO_STRING(VBC_DAI_ID_NORMAL_AP23),
		[VBC_DAI_ID_CAPTURE_DSP] = TO_STRING(VBC_DAI_ID_CAPTURE_DSP),
		[VBC_DAI_ID_FAST_P] = TO_STRING(VBC_DAI_ID_FAST_P),
		[VBC_DAI_ID_OFFLOAD] = TO_STRING(VBC_DAI_ID_OFFLOAD),
		[VBC_DAI_ID_VOICE] = TO_STRING(VBC_DAI_ID_VOICE),
		[VBC_DAI_ID_VOIP] = TO_STRING(VBC_DAI_ID_VOIP),
		[VBC_DAI_ID_FM] = TO_STRING(VBC_DAI_ID_FM),
		[VBC_DAI_ID_FM_CAPTURE_AP] =
			TO_STRING(VBC_DAI_ID_FM_CAPTURE_AP),
		[VBC_DAI_ID_VOICE_CAPTURE] =
			TO_STRING(VBC_DAI_ID_VOICE_CAPTURE),
		[VBC_DAI_ID_LOOP] = TO_STRING(VBC_DAI_ID_LOOP),
		[VBC_DAI_ID_PCM_A2DP] = TO_STRING(VBC_DAI_ID_PCM_A2DP),
		[VBC_DAI_ID_OFFLOAD_A2DP] = TO_STRING(VBC_DAI_ID_OFFLOAD_A2DP),
		[VBC_DAI_ID_BT_CAPTURE_AP] =
			TO_STRING(VBC_DAI_ID_BT_CAPTURE_AP),
		[VBC_DAI_ID_FM_CAPTURE_DSP] =
			TO_STRING(VBC_DAI_ID_FM_CAPTURE_DSP),
		[VBC_DAI_ID_BT_SCO_CAPTURE_DSP] =
			TO_STRING(VBC_DAI_ID_BT_SCO_CAPTURE_DSP),
		[VBC_DAI_ID_FM_DSP] = TO_STRING(VBC_DAI_ID_FM_DSP),
	};

	if (scene_id >= VBC_DAI_ID_MAX) {
		pr_err("invalid scene_id %d\n", scene_id);
		return "";
	}
	if (!scene_id_str[scene_id]) {
		pr_err("null scene_id string, scene_id=%d\n", scene_id);
		return "";
	}

	return scene_id_str[scene_id];

}

static int check_enable_ivs_smtpa(int scene_id, int stream,
	struct vbc_codec_priv *vbc_codec)
{
	int used_ivs;
	int enable;

	used_ivs = vbc_codec->is_use_ivs_smtpa;
	pr_debug("scene_id =%s stream =%s use ivs %s\n",
		scene_id_to_str(scene_id), stream_to_str(stream),
		used_ivs ? "true" : "false");

	if (!used_ivs)
		return 0;
	if (stream == SNDRV_PCM_STREAM_CAPTURE)
		return 0;

	switch (scene_id) {
	case VBC_DAI_ID_FAST_P:
	case VBC_DAI_ID_OFFLOAD:
		enable = 1;
		pr_info("scene %s enabled ivsense smartpa\n",
			scene_id_to_str(scene_id));
		break;
	default:
		enable = 0;
		break;
	}

	pr_debug("%s %s ivsence smartpa\n", scene_id_to_str(scene_id),
		enable ? "enabled" : "disabled");

	return enable;
}

static int get_ivsense_adc_id(void)
{
	return VBC_AD1;
}

#include "vbc-codec.c"

static int to_vbc_chan(int channels)
{
	int chan;

	switch (channels) {
	case 1:
		chan = VBC_LEFT;
		break;
	case 2:
		chan = VBC_ALL_CHAN;
		break;
	default:
		chan = VBC_ALL_CHAN;
		pr_err("channels is %d, default vbc channel =%d\n",
			channels, chan);
		break;
	}

	return chan;
}

static int check_be_dai_id(int be_dai_id)
{
	int scene_id;

	switch (be_dai_id) {
	case BE_DAI_ID_NORMAL_AP01_CODEC:
	case BE_DAI_ID_NORMAL_AP01_USB:
	case BE_DAI_ID_NORMAL_AP01_P_BTSCO:
	case BE_DAI_ID_NORMAL_AP01_P_HIFI:
	case BE_DAI_ID_DUMP:
		scene_id = VBC_DAI_ID_NORMAL_AP01;
		break;
	case BE_DAI_ID_NORMAL_AP23_CODEC:
	case BE_DAI_ID_NORMAL_AP23_USB:
	case BE_DAI_ID_NORMAL_AP23_HIFI:
		scene_id = VBC_DAI_ID_NORMAL_AP23;
		break;
	case BE_DAI_ID_CAPTURE_DSP_CODEC:
	case BE_DAI_ID_CAPTURE_DSP_USB:
		scene_id = VBC_DAI_ID_CAPTURE_DSP;
		break;
	case BE_DAI_ID_FAST_P_CODEC:
	case BE_DAI_ID_FAST_P_USB:
	case BE_DAI_ID_FAST_P_BTSCO:
	case BE_DAI_ID_FAST_P_HIFI:
		scene_id = VBC_DAI_ID_FAST_P;
		break;
	case BE_DAI_ID_OFFLOAD_CODEC:
	case BE_DAI_ID_OFFLOAD_USB:
	case BE_DAI_ID_OFFLOAD_HIFI:
		scene_id = VBC_DAI_ID_OFFLOAD;
		break;
	case BE_DAI_ID_VOICE_CODEC:
	case BE_DAI_ID_VOICE_USB:
	case BE_DAI_ID_VOICE_BT:
	case BE_DAI_ID_VOICE_HIFI:
		scene_id = VBC_DAI_ID_VOICE;
		break;
	case BE_DAI_ID_VOIP_CODEC:
	case BE_DAI_ID_VOIP_USB:
	case BE_DAI_ID_VOIP_BT:
	case BE_DAI_ID_VOIP_HIFI:
		scene_id = VBC_DAI_ID_VOIP;
		break;
	case BE_DAI_ID_FM_CODEC:
	case BE_DAI_ID_FM_USB:
	case BE_DAI_ID_FM_HIFI:
		scene_id = VBC_DAI_ID_FM;
		break;
	case BE_DAI_ID_LOOP_CODEC:
	case BE_DAI_ID_LOOP_USB:
	case BE_DAI_ID_LOOP_BT:
	case BE_DAI_ID_LOOP_HIFI:
		scene_id = VBC_DAI_ID_LOOP;
		break;
	case BE_DAI_ID_PCM_A2DP:
		scene_id = VBC_DAI_ID_PCM_A2DP;
		break;
	case BE_DAI_ID_OFFLOAD_A2DP:
		scene_id = VBC_DAI_ID_OFFLOAD_A2DP;
		break;
	case BE_DAI_ID_CAPTURE_BT:
		scene_id = VBC_DAI_ID_BT_CAPTURE_AP;
		break;
	case BE_DAI_ID_VOICE_CAPTURE:
		scene_id = VBC_DAI_ID_VOICE_CAPTURE;
		break;
	case BE_DAI_ID_FM_CAPTURE:
		scene_id = VBC_DAI_ID_FM_CAPTURE_AP;
		break;
	case BE_DAI_ID_FM_CAPTURE_DSP:
		scene_id = VBC_DAI_ID_FM_CAPTURE_DSP;
		break;
	case BE_DAI_ID_CAPTURE_DSP_BTSCO:
		scene_id = VBC_DAI_ID_BT_SCO_CAPTURE_DSP;
		break;
	case BE_DAI_ID_FM_DSP_CODEC:
	case BE_DAI_ID_FM_DSP_USB:
	case BE_DAI_ID_FM_DSP_HIFI:
		scene_id = VBC_DAI_ID_FM_DSP;
		break;
	default:
		scene_id = VBC_DAI_ID_MAX;
		pr_err("unknown be dai id %d use default dsp id=%d\n",
		       be_dai_id, scene_id);
		break;
	}

	return scene_id;
}

#define SPRD_VBC_DAI_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
				  SNDRV_PCM_FMTBIT_S24_LE)

static int sprd_dai_vbc_probe(struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_route intercon;

	if (!dai || !dai->driver) {
		pr_err("%s Invalid params\n", __func__);
		return -EINVAL;
	}
	memset(&intercon, 0, sizeof(intercon));
	if (dai->driver->playback.stream_name &&
	    dai->driver->playback.aif_name) {
		dev_dbg(dai->dev, "%s: add route for widget %s",
			__func__, dai->driver->playback.stream_name);
		intercon.source = dai->driver->playback.aif_name;
		intercon.sink = dai->driver->playback.stream_name;
		dev_dbg(dai->dev, "%s: src %s sink %s\n",
			__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(&dai->component->dapm, &intercon, 1);
	}
	if (dai->driver->capture.stream_name &&
	    dai->driver->capture.aif_name) {
		dev_dbg(dai->dev, "%s: add route for widget %s",
			__func__, dai->driver->capture.stream_name);
		intercon.sink = dai->driver->capture.aif_name;
		intercon.source = dai->driver->capture.stream_name;
		dev_dbg(dai->dev, "%s: src %s sink %s\n",
			__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(&dai->component->dapm, &intercon, 1);
	}

	return 0;
}

/* normal scene */
static void normal_vbc_protect_spin_lock(int stream)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct aud_pm_vbc *pm_vbc;

	if (!is_playback)
		return;

	pm_vbc = aud_pm_vbc_get();
	spin_lock(&pm_vbc->pm_spin_cmd_prot);
}

static void normal_vbc_protect_spin_unlock(int stream)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct aud_pm_vbc *pm_vbc;

	if (!is_playback)
		return;

	pm_vbc = aud_pm_vbc_get();
	spin_unlock(&pm_vbc->pm_spin_cmd_prot);
}

static void normal_vbc_protect_mutex_lock(int stream)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct aud_pm_vbc *pm_vbc;

	if (!is_playback)
		return;

	pm_vbc = aud_pm_vbc_get();
	mutex_lock(&pm_vbc->pm_mtx_cmd_prot);
}

static void normal_vbc_protect_mutex_unlock(int stream)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct aud_pm_vbc *pm_vbc;

	if (!is_playback)
		return;

	pm_vbc = aud_pm_vbc_get();
	mutex_unlock(&pm_vbc->pm_mtx_cmd_prot);
}

static void normal_p_suspend_resume_mtx_lock(void)
{
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();

	mutex_lock(&pm_vbc->lock_mtx_suspend_resume);
}

static void normal_p_suspend_resume_mtx_unlock(void)
{
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();

	mutex_unlock(&pm_vbc->lock_mtx_suspend_resume);
}

static void normal_p_suspend_resume_add_ref(void)
{
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	pm_vbc->ref_suspend_resume++;
	pr_info("%s ref=%d\n", __func__, pm_vbc->ref_suspend_resume);
}

static void normal_p_suspend_resume_dec_ref(void)
{
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	pm_vbc->ref_suspend_resume--;
	pr_info("%s ref=%d\n", __func__, pm_vbc->ref_suspend_resume);
}

static int normal_p_suspend_resume_get_ref(void)
{
	int ref;
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	ref = pm_vbc->ref_suspend_resume;
	pr_info("%s ref=%d\n", __func__, ref);

	return ref;
}

static void set_normal_p_running_status(int stream, bool status)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	if (!is_playback)
		return;
	pm_vbc->is_startup = status;
	pr_info("%s is_startup=%s\n", __func__,
		pm_vbc->is_startup ? "true" : "false");
}

static bool get_normal_p_running_status(int stream)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct aud_pm_vbc *pm_vbc;
	bool status;

	pm_vbc = aud_pm_vbc_get();
	if (!is_playback)
		return false;

	status = pm_vbc->is_startup;
	pr_info("%s is_startup=%s\n", __func__,
		status ? "true" : "false");

	return status;
}

static int vbc_normal_resume(void)
{
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	restore_access();
	pr_info("%s resumed\n", __func__);

	return 0;
}
/*
 * suspend
 * 1. only vbc normal scene exists.
 * 2. send shutdown command to dsp.
 * resume
 * user space will excute close flow:
 * 1. if shutdown has been done in suspend,
 * do not send to dsp again.
 */
static int vbc_normal_suspend(void)
{
	struct aud_pm_vbc *pm_vbc;
	int stream = SNDRV_PCM_STREAM_PLAYBACK;
	int is_startup;

	pm_vbc = aud_pm_vbc_get();
	/*
	 * vbc clock clear and agcp access disable must after codec
	 * suspened because codec needed agcp access enable.
	 * Puting these function here and set bus_control to be true
	 * will meet requirements. Do not put them in PM_SUSPEND_PREPARE
	 * it is too early.
	 */
	pr_info("%s enter suspend\n", __func__);
	normal_vbc_protect_mutex_lock(stream);
	is_startup = get_normal_p_running_status(SNDRV_PCM_STREAM_PLAYBACK);
	if (is_startup == false) {
		pr_info("%s startup not called just return\n", __func__);
		normal_vbc_protect_mutex_unlock(stream);
		return 0;
	}
	pr_info("%s send shutdown\n", __func__);
	normal_vbc_protect_mutex_lock(stream);
	normal_vbc_protect_spin_lock(stream);
	set_normal_p_running_status(SNDRV_PCM_STREAM_PLAYBACK, false);
	normal_vbc_protect_spin_unlock(stream);
	pm_shutdown();
	normal_vbc_protect_mutex_unlock(stream);
	disable_access_force();
	pr_info("%s suspeded\n", __func__);

	return 0;
}

static int get_startup_scene_dac_id(int scene_id)
{
	int dac_id;

	switch (scene_id) {
	case VBC_DAI_ID_NORMAL_AP01:
		dac_id = VBC_DA0;
		break;
	case VBC_DAI_ID_NORMAL_AP23:
		dac_id = VBC_DA0;
		break;
	case VBC_DAI_ID_CAPTURE_DSP:
		/* not used */
		dac_id = 0;
		break;
	case VBC_DAI_ID_FAST_P:
		dac_id = VBC_DA0;
		break;
	case VBC_DAI_ID_OFFLOAD:
		dac_id = VBC_DA0;
		break;
	case VBC_DAI_ID_VOICE:
		dac_id = VBC_DA1;
		break;
	case VBC_DAI_ID_VOIP:
		dac_id = VBC_DA1;
		break;
	case VBC_DAI_ID_FM:
		dac_id = VBC_DA0;
		break;
	case VBC_DAI_ID_LOOP:
		dac_id = VBC_DA1;
		break;
	case VBC_DAI_ID_PCM_A2DP:
		dac_id = VBC_DA2;
		break;
	case VBC_DAI_ID_OFFLOAD_A2DP:
		dac_id = VBC_DA2;
		break;
	case VBC_DAI_ID_BT_CAPTURE_AP:
		/* not used */
		dac_id = 0;
		break;
	case VBC_DAI_ID_FM_CAPTURE_AP:
		/* not used */
		dac_id = 0;
		break;
	case VBC_DAI_ID_BT_SCO_CAPTURE_DSP:
		/* not used */
		dac_id = 0;
		break;
	case VBC_DAI_ID_FM_CAPTURE_DSP:
		/* not used */
		dac_id = 0;
		break;
	case VBC_DAI_ID_VOICE_CAPTURE:
		/* not used */
		dac_id = 0;
		break;
	case VBC_DAI_ID_FM_DSP:
		dac_id = VBC_DA0;
		break;
	default:
		pr_err("invalid scene_id = %d\n", scene_id);
		dac_id = 0;
		break;
	}

	pr_info("%s scene is %s(id %d) dac_id = %d\n",
		__func__, scene_id_to_str(scene_id), scene_id, dac_id);

	return dac_id;
}

static int get_startup_scene_adc_id(int scene_id)
{
	int adc_id;

	switch (scene_id) {
	case VBC_DAI_ID_NORMAL_AP01:
		adc_id = VBC_AD0;
		break;
	case VBC_DAI_ID_NORMAL_AP23:
		adc_id = VBC_AD0;
		break;
	case VBC_DAI_ID_CAPTURE_DSP:
		adc_id = VBC_AD0;
		break;
	case VBC_DAI_ID_FAST_P:
		/* not used */
		adc_id = 0;
		break;
	case VBC_DAI_ID_OFFLOAD:
		/* not used */
		adc_id = 0;
		break;
	case VBC_DAI_ID_VOICE:
		adc_id = VBC_AD2;
		break;
	case VBC_DAI_ID_VOIP:
		adc_id = VBC_AD2;
		break;
	case VBC_DAI_ID_FM:
		adc_id = VBC_AD3;
		break;
	case VBC_DAI_ID_LOOP:
		adc_id = VBC_AD2;
		break;
	case VBC_DAI_ID_PCM_A2DP:
		/* not used */
		adc_id = 0;
		break;
	case VBC_DAI_ID_OFFLOAD_A2DP:
		/* not used */
		adc_id = 0;
		break;
	case VBC_DAI_ID_BT_CAPTURE_AP:
		/* not used */
		adc_id = 0;
		break;
	case VBC_DAI_ID_FM_CAPTURE_AP:
		/* not used */
		adc_id = 0;
		break;
	case VBC_DAI_ID_VOICE_CAPTURE:
		/* not used */
		adc_id = 0;
		break;
	case VBC_DAI_ID_BT_SCO_CAPTURE_DSP:
		adc_id = VBC_AD2;
		break;
	case VBC_DAI_ID_FM_CAPTURE_DSP:
		adc_id = VBC_AD3;
		break;
	case VBC_DAI_ID_FM_DSP:
		adc_id = VBC_AD3;
		break;
	default:
		pr_err("invalid scene_id = %d\n", scene_id);
		adc_id = VBC_AD0;
		break;
	}
	pr_info("%s scene is %s(id %d) adc_id = %d\n",
		__func__, scene_id_to_str(scene_id), scene_id, adc_id);

	return adc_id;
}

static int16_t get_startup_mdg_reload(int mdg_id)
{
	int16_t reload;

	switch (mdg_id) {
	case VBC_MDG_DAC0_DSP:
	case VBC_MDG_DAC1_DSP:
	case VBC_MDG_AP01:
	case VBC_MDG_AP23:
		reload = 0;
		break;
	default:
		pr_err("%s invalid id=%d\n", __func__, mdg_id);
		reload = 0;
	}

	return reload;
}

static int16_t get_startup_smthdg_reload(int smthdg_id)
{
	int16_t reload;

	switch (smthdg_id) {
	case VBC_SMTHDG_DAC0:
		reload = 0;
		break;
	default:
		pr_err("%s invalid id = %d\n", __func__, smthdg_id);
		reload = 0;
	}

	return reload;
}

static int16_t get_startup_mixerdg_reload(int mixerdg_id)
{
	int16_t reload;

	switch (mixerdg_id) {
	case VBC_MIXERDG_DAC0:
	case VBC_MIXERDG_DAC1:
		reload = 0;
		break;
	default:
		pr_err("%s invalid id=%d\n", __func__, mixerdg_id);
		reload = 0;
	}

	return reload;
}

static int16_t get_startup_mixer_reload(int mixer_id)
{
	int16_t reload;

	switch (mixer_id) {
	case VBC_MIXER0_DAC0:
	case VBC_MIXER1_DAC0:
	case VBC_MIXER0_DAC1:
	case VBC_MIXER_ST:
	case VBC_MIXER_FM:
		reload = 0;
		break;
	default:
		pr_err("%s invalid id=%d\n", __func__, mixer_id);
		reload = 0;
	}

	return reload;
}

static int16_t get_startup_master_reload(void)
{
	return 1;
}

static void fill_dsp_startup_data(struct vbc_codec_priv *vbc_codec,
	int scene_id, int stream,
	struct sprd_vbc_stream_startup_shutdown *startup_info)
{
	int i;
	struct snd_pcm_startup_paras *para = &startup_info->startup_para;
	struct snd_pcm_stream_info *info = &startup_info->stream_info;

	if (!vbc_codec)
		return;
	info->id = scene_id;
	info->stream = stream;
	para->dac_id = get_startup_scene_dac_id(scene_id);
	para->adc_id = get_startup_scene_adc_id(scene_id);
	/* vbc_if or iis */
	for (i = VBC_MUX_ADC0_SOURCE; i < VBC_MUX_ADC_SOURCE_MAX; i++) {
		para->adc_source[i].id = vbc_codec->mux_adc_source[i].id;
		para->adc_source[i].val = vbc_codec->mux_adc_source[i].val;
	}
	for (i = VBC_MUX_DAC0_OUT_SEL; i < VBC_MUX_DAC_OUT_MAX ; i++) {
		para->dac_out[i].id = vbc_codec->mux_dac_out[i].id;
		para->dac_out[i].val = vbc_codec->mux_dac_out[i].val;
	}
	/* iis tx port select */
	for (i = VBC_MUX_IIS_TX_DAC0; i < VBC_MUX_IIS_TX_ID_MAX; i++) {
		para->mux_tx[i].id = vbc_codec->mux_iis_tx[i].id;
		para->mux_tx[i].val = vbc_codec->mux_iis_tx[i].val;
	}
	/* iis port do for tx */
	for (i = VBC_IIS_PORT_IIS0; i < VBC_IIS_PORT_ID_MAX; i++) {
		para->iis_do[i].id = vbc_codec->mux_iis_port_do[i].id;
		para->iis_do[i].val = vbc_codec->mux_iis_port_do[i].val;
	}
	/* iis rx port select */
	for (i = VBC_MUX_IIS_RX_ADC0; i < VBC_MUX_IIS_RX_ID_MAX; i++) {
		para->mux_rx[i].id = vbc_codec->mux_iis_rx[i].id;
		para->mux_rx[i].val = vbc_codec->mux_iis_rx[i].val;
	}
	/* iis tx width, tx lr_mode */
	for (i = VBC_MUX_IIS_TX_DAC0; i < VBC_MUX_IIS_TX_ID_MAX; i++) {
		para->tx_wd[i].id = vbc_codec->iis_tx_wd[i].id;
		para->tx_wd[i].value = vbc_codec->iis_tx_wd[i].value;
		para->tx_lr_mod[i].id = vbc_codec->iis_tx_lr_mod[i].id;
		para->tx_lr_mod[i].value = vbc_codec->iis_tx_lr_mod[i].value;
	}
	/* iis rx width, rx lr_mode */
	for (i = VBC_MUX_IIS_RX_ADC0; i < VBC_MUX_IIS_RX_ID_MAX; i++) {
		para->rx_wd[i].id = vbc_codec->iis_rx_wd[i].id;
		para->rx_wd[i].value = vbc_codec->iis_rx_wd[i].value;
		para->rx_lr_mod[i].id = vbc_codec->iis_rx_lr_mod[i].id;
		para->rx_lr_mod[i].value = vbc_codec->iis_rx_lr_mod[i].value;
	}

	/* iis master external or internal */
	for (i = IIS_MST_SEL_0; i < IIS_MST_SEL_ID_MAX; i++) {
		para->mst_sel_para[i].id = vbc_codec->mst_sel_para[i].id;
		para->mst_sel_para[i].mst_type =
			vbc_codec->mst_sel_para[i].mst_type;
	}

	/* vbc iis master */
	para->iis_master_para.vbc_startup_reload = get_startup_master_reload();
	para->iis_master_para.enable = vbc_codec->iis_master.enable;
	/* mute dg */
	for (i = VBC_MDG_DAC0_DSP; i < VBC_MDG_MAX; i++) {
		para->mdg_para[i].vbc_startup_reload =
			get_startup_mdg_reload(i);
		para->mdg_para[i].mdg_id = vbc_codec->mdg[i].mdg_id;
		para->mdg_para[i].mdg_mute = vbc_codec->mdg[i].mdg_mute;
		para->mdg_para[i].mdg_step = vbc_codec->mdg[i].mdg_step;
	}
	/* smthdg */
	for (i = VBC_SMTHDG_DAC0; i < VBC_SMTHDG_MAX; i++) {
		para->smthdg_modle[i].vbc_startup_reload =
			get_startup_smthdg_reload(i);
		para->smthdg_modle[i].smthdg_dg.smthdg_id =
			vbc_codec->smthdg[i].smthdg_id;
		para->smthdg_modle[i].smthdg_dg.smthdg_left =
			vbc_codec->smthdg[i].smthdg_left;
		para->smthdg_modle[i].smthdg_dg.smthdg_right =
			vbc_codec->smthdg[i].smthdg_right;
		para->smthdg_modle[i].smthdg_step.smthdg_id =
			vbc_codec->smthdg_step[i].smthdg_id;
		para->smthdg_modle[i].smthdg_step.step =
			vbc_codec->smthdg_step[i].step;
	}
	/* mixerdg */
	for (i = VBC_MIXERDG_DAC0; i < VBC_MIXERDG_MAX; i++) {
		para->mixerdg_para[i].vbc_startup_reload =
			get_startup_mixerdg_reload(i);
		para->mixerdg_para[i].mixerdg_id =
			vbc_codec->mixerdg[i].mixerdg_id;
		para->mixerdg_para[i].main_path.mixerdg_id =
			vbc_codec->mixerdg[i].main_path.mixerdg_id;
		para->mixerdg_para[i].main_path.mixerdg_main_left =
			vbc_codec->mixerdg[i].main_path.mixerdg_main_left;
		para->mixerdg_para[i].main_path.mixerdg_main_right =
			vbc_codec->mixerdg[i].main_path.mixerdg_main_right;
		para->mixerdg_para[i].mix_path.mixerdg_id =
			vbc_codec->mixerdg[i].mix_path.mixerdg_id;
		para->mixerdg_para[i].mix_path.mixerdg_mix_left =
			vbc_codec->mixerdg[i].mix_path.mixerdg_mix_left;
		para->mixerdg_para[i].mix_path.mixerdg_mix_right =
			vbc_codec->mixerdg[i].mix_path.mixerdg_mix_right;
	}
	para->mixerdg_step = vbc_codec->mixerdg_step;
	/* mixer */
	for (i = VBC_MIXER0_DAC0; i < VBC_MIXER_MAX; i++) {
		para->mixer_para[i].vbc_startup_reload =
			get_startup_mixer_reload(i);
		para->mixer_para[i].mixer_id = vbc_codec->mixer[i].mixer_id;
		para->mixer_para[i].type = vbc_codec->mixer[i].type;
	}
	/* loopback */
	para->loopback_para.loopback_type = vbc_codec->loopback.loopback_type;
	para->loopback_para.amr_rate = vbc_codec->loopback.amr_rate;
	para->loopback_para.voice_fmt = vbc_codec->loopback.voice_fmt;
	/* sbc para */
	para->sbcenc_para = vbc_codec->sbcenc_para;
	/* ivsense smartpa */
	para->ivs_smtpa.enable = check_enable_ivs_smtpa(scene_id,
		stream, vbc_codec);
	para->ivs_smtpa.iv_adc_id = get_ivsense_adc_id();
}

static int dsp_startup(struct vbc_codec_priv *vbc_codec,
		       int scene_id, int stream)
{
	int ret = 0;
	struct sprd_vbc_stream_startup_shutdown startup_info;

	if (!vbc_codec)
		return 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}
	memset(&startup_info, 0,
	       sizeof(struct sprd_vbc_stream_startup_shutdown));
	fill_dsp_startup_data(vbc_codec, scene_id, stream, &startup_info);
	ret = vbc_dsp_func_startup(scene_id, stream, &startup_info);
	if (ret < 0) {
		pr_err("vbc_dsp_func_startup return error");
		agdsp_access_disable();
		return ret;
	}
	agdsp_access_disable();

	return 0;
}

static void fill_dsp_shutdown_data(struct vbc_codec_priv *vbc_codec,
	int scene_id, int stream,
	struct sprd_vbc_stream_startup_shutdown *shutdown_info)
{
	int dac_id, adc_id;

	if (!vbc_codec)
		return;
	shutdown_info->stream_info.id = scene_id;
	shutdown_info->stream_info.stream = stream;
	dac_id = get_startup_scene_dac_id(scene_id);
	adc_id = get_startup_scene_adc_id(scene_id);
	shutdown_info->startup_para.dac_id = dac_id;
	shutdown_info->startup_para.adc_id = adc_id;
}

static void dsp_shutdown(struct vbc_codec_priv *vbc_codec,
			 int scene_id, int stream)
{
	int ret;
	struct sprd_vbc_stream_startup_shutdown shutdown_info;

	if (!vbc_codec)
		return;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return;
	}
	memset(&shutdown_info, 0,
	       sizeof(struct sprd_vbc_stream_startup_shutdown));
	fill_dsp_shutdown_data(vbc_codec, scene_id, stream, &shutdown_info);
	ret = vbc_dsp_func_shutdown(scene_id, stream, &shutdown_info);
	if (ret < 0) {
		agdsp_access_disable();
		return;
	}
	agdsp_access_disable();
}

static int rate_to_src_mode(unsigned int rate)
{
	int mode = -1;
	int i;
	struct sprd_codec_src_tbl {
		unsigned int rate;
		int src_mode;
	} src_tbl[] = {
		{48000, SRC_MODE_48000},
		{44100, SRC_MODE_44100},
		{32000, SRC_MODE_32000},
		{24000, SRC_MODE_24000},
		{22050, SRC_MODE_22050},
		{16000, SRC_MODE_16000},
		{12000, SRC_MODE_12000},
		{11025, SRC_MODE_11025},
		{8000, SRC_MODE_8000},
	};

	for (i = 0; i < ARRAY_SIZE(src_tbl); i++) {
		if (src_tbl[i].rate == rate)
			mode = src_tbl[i].src_mode;
	}

	if (mode == -1) {
		pr_info("%s, not supported samplerate (%d)\n",
				__func__, rate);
		mode = SRC_MODE_48000;
	}

	return mode;
}

void fill_dsp_hw_data(struct vbc_codec_priv *vbc_codec,
		      int scene_id, int stream, int chan_cnt, int rate, int fmt,
		      struct sprd_vbc_stream_hw_paras *hw_data)
{
	if (!vbc_codec)
		return;
	hw_data->stream_info.id = scene_id;
	hw_data->stream_info.stream = stream;
	/* channels not use by dsp */
	hw_data->hw_params_info.channels = chan_cnt;
	hw_data->hw_params_info.format = fmt;
	/* dsp use transformed rate */
	hw_data->hw_params_info.rate = rate_to_src_mode(rate);
}

static void dsp_hw_params(struct vbc_codec_priv *vbc_codec,
	int scene_id, int stream, int chan_cnt, u32 rate, int data_fmt)
{
	struct sprd_vbc_stream_hw_paras hw_data;
	int ret;

	if (!vbc_codec)
		return;
	memset(&hw_data, 0, sizeof(struct sprd_vbc_stream_hw_paras));
	fill_dsp_hw_data(vbc_codec, scene_id, stream, chan_cnt, rate, data_fmt,
		&hw_data);
	ret = vbc_dsp_func_hwparam(scene_id, stream, &hw_data);
	if (ret < 0) {
		pr_err("vbc_dsp_func_hwparam return error\n");
		return;
	}
}

static int dsp_trigger(struct vbc_codec_priv *vbc_codec,
		       int scene_id, int stream, int up_down)
{
	int ret;

	if (!vbc_codec)
		return 0;
	ret = vbc_dsp_func_trigger(scene_id, stream, up_down);
	if (ret < 0) {
		pr_err("vbc_dsp_func_trigger return error\n");
		return ret;
	}

	return 0;
}

void set_kctrl_vbc_dac_iis_wd(struct vbc_codec_priv *vbc_codec, int dac_id,
			      int data_fmt)
{
	struct snd_ctl_elem_id id = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	};
	struct snd_kcontrol *kctrl;
	struct snd_ctl_elem_value ucontrol;
	int val;
	struct snd_soc_card *card;

	if (!vbc_codec || !vbc_codec->codec)
		return;
	card = vbc_codec->codec->component.card;
	if (!card)
		return;
	switch (dac_id) {
	case VBC_DA0:
		strcpy(id.name, "VBC_IIS_TX0_WD_SEL");
		break;
	case VBC_DA1:
		strcpy(id.name, "VBC_IIS_TX1_WD_SEL");
		break;
	default:
		pr_err("%s invalid dac_id=%d\n", __func__, dac_id);
		return;
	}

	down_read(&card->snd_card->controls_rwsem);
	kctrl = snd_ctl_find_id(card->snd_card, &id);
	if (!kctrl) {
		pr_err("%s can't find kctrl '%s'\n", __func__, id.name);
		up_read(&card->snd_card->controls_rwsem);
		return;
	}

	switch (data_fmt) {
	case VBC_DAT_L16:
	case VBC_DAT_H16:
		val = 0;
		break;
	case VBC_DAT_L24:
	case VBC_DAT_H24:
		val = 1;
		break;
	default:
		pr_err("unknown data fmt %d\n", data_fmt);
		val = 0;
	}

	ucontrol.value.enumerated.item[0] = val;
	vbc_put_iis_tx_width_sel(kctrl, &ucontrol);
	up_read(&card->snd_card->controls_rwsem);
}

int scene_id_to_ap_fifo_id(int scene_id, int stream)
{
	int ap_fifo_id;
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;

	switch (scene_id) {
	case VBC_DAI_ID_NORMAL_AP01:
		ap_fifo_id = is_playback ? AP01_PLY_FIFO : AP01_REC_FIFO;
		break;
	case VBC_DAI_ID_FM_CAPTURE_AP:
		ap_fifo_id = AP01_REC_FIFO;
		break;
	case VBC_DAI_ID_BT_CAPTURE_AP:
		ap_fifo_id = AP01_REC_FIFO;
		break;
	case VBC_DAI_ID_NORMAL_AP23:
		ap_fifo_id = is_playback ? AP23_PLY_FIFO : AP23_REC_FIFO;
		break;
	default:
		ap_fifo_id = AP_FIFO_MAX;
		pr_err("not mapped scene_id and ap_fifo_id\n");
		break;
	}

	return ap_fifo_id;
}

int stream_to_watermark_type(int stream)
{
	int watermark_type;

	switch (stream) {
	case SNDRV_PCM_STREAM_CAPTURE:
		watermark_type = FULL_WATERMARK;
		break;
	case SNDRV_PCM_STREAM_PLAYBACK:
		watermark_type = EMPTY_WATERMARK;
		break;
	default:
		watermark_type = WATERMARK_TYPE_MAX;
		break;
	}

	return watermark_type;
}

u32 get_watermark(int fifo_id, int watermark_type)
{
	u32 watermark;

	switch (fifo_id) {
	case AP01_PLY_FIFO:
		if (watermark_type == FULL_WATERMARK)
			watermark = VBC_AUDPLY01_FULL_WATERMARK;
		else
			watermark = VBC_AUDPLY01_EMPTY_WATERMARK;
		break;
	case AP01_REC_FIFO:
		if (watermark_type == FULL_WATERMARK)
			watermark = VBC_AUDREC01_FULL_WATERMARK;
		else
			watermark = VBC_AUDREC01_EMPTY_WATERMARK;
		break;
	case AP23_PLY_FIFO:
		if (watermark_type == FULL_WATERMARK)
			watermark = VBC_AUDPLY23_FULL_WATERMARK;
		else
			watermark = VBC_AUDPLY23_EMPTY_WATERMARK;
		break;
	case AP23_REC_FIFO:
		if (watermark_type == FULL_WATERMARK)
			watermark = VBC_AUDREC23_FULL_WATERMARK;
		else
			watermark = VBC_AUDREC23_EMPTY_WATERMARK;
		break;
	default:
		watermark = 0;
		pr_err("%s invalid fifo_id =%d\n", __func__, fifo_id);
		break;
	}

	return watermark;
}

static void ap_vbc_ad_src_set(int en, unsigned int rate)
{
	int mode = rate_to_src_mode(rate);

	vbc_phy_audply_set_src_mode(en, mode);
}

static bool ap_ad_src_check(int scene_id, int stream)
{
	if (stream == SNDRV_PCM_STREAM_CAPTURE &&
		scene_id == VBC_DAI_ID_NORMAL_AP01)
		return true;

	return false;
}

/* ap_startup, ap_shutdown ignore */
static int ap_hw_params(struct vbc_codec_priv *vbc_codec,
	int scene_id, int stream, int vbc_chan, u32 rate, int data_fmt)
{
	int fifo_id;
	int watermark_type;
	int watermark;
	bool use_ad_src;
	int ret;

	if (!vbc_codec)
		return 0;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}
	fifo_id = scene_id_to_ap_fifo_id(scene_id, stream);
	watermark_type = stream_to_watermark_type(stream);
	watermark = get_watermark(fifo_id, watermark_type);
	if (fifo_id == AP01_REC_FIFO && watermark_type == FULL_WATERMARK)
		watermark = ((rate * VBC_AUDREC01_FULL_WATERMARK) /
			     DEFAULT_RATE) & ~BIT(0);
	ap_vbc_set_watermark(fifo_id, watermark_type, watermark);
	ap_vbc_data_format_set(fifo_id, data_fmt);
	use_ad_src = ap_ad_src_check(scene_id, stream);
	if (use_ad_src) {
		ap_vbc_ad_src_set(1, rate);
		pr_info("%s vbc ap src set rate %u\n", __func__, rate);
	}

	/* vbc_dsp_hw_params need not call in normal scene*/
	agdsp_access_disable();

	return 0;
}

void ap_vbc_fifo_pre_fill(int fifo_id, int vbc_chan)
{
	switch (fifo_id) {
	case AP01_PLY_FIFO:
		switch (vbc_chan) {
		case VBC_LEFT:
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_0, 0);
			break;
		case VBC_RIGHT:
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_1, 0);
			break;
		case VBC_ALL_CHAN:
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_0, 0);
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_1, 0);
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__,
			       vbc_chan);
			return;
		}
		break;
	case AP23_PLY_FIFO:
		switch (vbc_chan) {
		case VBC_LEFT:
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_2, 0);
			break;
		case VBC_RIGHT:
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_3, 0);
			break;
		case VBC_ALL_CHAN:
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_2, 0);
			ap_vbc_reg_write(VBC_AUDPLY_FIFO_WR_3, 0);
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__,
			       vbc_chan);
			return;
		}
		break;
	case AP01_REC_FIFO:
	case AP23_REC_FIFO:
	default:
		pr_err("%s %s should not pre filled\n", __func__,
		       ap_vbc_fifo_id2name(fifo_id));
		return;
	}
}

static int ap_trigger(struct vbc_codec_priv *vbc_codec,
		      int scene_id, int stream, int vbc_chan, int up_down)
{
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	int fifo_id, ret;

	fifo_id = scene_id_to_ap_fifo_id(scene_id, stream);
	if (!vbc_codec)
		return 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}
	if (up_down == 1) {
		ap_vbc_fifo_clear(fifo_id);
		if (is_playback)
			ap_vbc_fifo_pre_fill(fifo_id, vbc_chan);
		ap_vbc_fifo_enable(fifo_id, vbc_chan, 1);
		ap_vbc_aud_dma_chn_en(fifo_id, vbc_chan, 1);
	} else {
		ap_vbc_aud_dma_chn_en(fifo_id, vbc_chan, 0);
		ap_vbc_fifo_enable(fifo_id, vbc_chan, 0);
	}
	agdsp_access_disable();

	return 0;
}

struct scene_data_s {
	struct mutex lock_startup[VBC_DAI_ID_MAX][STREAM_CNT];
	int ref_startup[VBC_DAI_ID_MAX][STREAM_CNT];
	struct mutex lock_hw_param[VBC_DAI_ID_MAX][STREAM_CNT];
	int ref_hw_param[VBC_DAI_ID_MAX][STREAM_CNT];
	struct spinlock lock_trigger[VBC_DAI_ID_MAX][STREAM_CNT];
	int ref_trigger[VBC_DAI_ID_MAX][STREAM_CNT];
	int vbc_chan[VBC_DAI_ID_MAX][STREAM_CNT];
};

static struct scene_data_s scene_data;

struct scene_data_s *get_scene_data(void)
{
	return &scene_data;
}

void set_vbc_chan(int scene_id, int stream, int vbc_chan)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	scene_data->vbc_chan[scene_id][stream] = vbc_chan;

}

int get_vbc_chan(int scene_id, int stream)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();

	return scene_data->vbc_chan[scene_id][stream];
}

/* lock reference operation for eache scene and stream */
void startup_lock_mtx(int scene_id, int stream)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	mutex_lock(&scene_data->lock_startup[scene_id][stream]);
	pr_debug("%s %s %s\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream));
}
void startup_unlock_mtx(int scene_id, int stream)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	mutex_unlock(&scene_data->lock_startup[scene_id][stream]);
	pr_debug("%s %s %s\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream));
}

void hw_param_lock_mtx(int scene_id, int stream)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	mutex_lock(&scene_data->lock_hw_param[scene_id][stream]);
	pr_debug("%s %s %s\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream));
}

void hw_param_unlock_mtx(int scene_id, int stream)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	mutex_unlock(&scene_data->lock_hw_param[scene_id][stream]);
	pr_debug("%s %s %s\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream));
}

void trigger_lock_spin(int scene_id, int stream)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	spin_lock(&scene_data->lock_trigger[scene_id][stream]);
	pr_debug("%s %s %s\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream));
}

void trigger_unlock_spin(int scene_id, int stream)
{
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	spin_unlock(&scene_data->lock_trigger[scene_id][stream]);
	pr_debug("%s %s %s\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream));
}

void startup_add_ref(int scene_id, int stream)
{
	struct scene_data_s *scene_data;
	int ref;

	scene_data = get_scene_data();
	ref = ++scene_data->ref_startup[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);
}
void startup_dec_ref(int scene_id, int stream)
{
	struct scene_data_s *scene_data;
	int ref = 0;

	scene_data = get_scene_data();
	if (scene_data->ref_startup[scene_id][stream] > 0)
		ref = --scene_data->ref_startup[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);
}

int startup_get_ref(int scene_id, int stream)
{
	int ref;
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	ref = scene_data->ref_startup[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);

	return ref;
}

void hw_param_add_ref(int scene_id, int stream)
{
	struct scene_data_s *scene_data;
	int ref;

	scene_data = get_scene_data();
	ref = ++scene_data->ref_hw_param[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);
}

void hw_param_dec_ref(int scene_id, int stream)
{
	struct scene_data_s *scene_data;
	int ref;

	scene_data = get_scene_data();
	ref = --scene_data->ref_hw_param[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);
}

int hw_param_get_ref(int scene_id, int stream)
{
	int ref;
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	ref = scene_data->ref_hw_param[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);

	return ref;
}

void trigger_add_ref(int scene_id, int stream)
{
	struct scene_data_s *scene_data;
	int ref;

	scene_data = get_scene_data();
	ref = ++scene_data->ref_trigger[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);
}

void trigger_dec_ref(int scene_id, int stream)
{
	struct scene_data_s *scene_data;
	int ref;

	scene_data = get_scene_data();
	ref = --scene_data->ref_trigger[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);
}

int trigger_get_ref(int scene_id, int stream)
{
	int ref;
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	ref = scene_data->ref_trigger[scene_id][stream];
	pr_debug("%s %s %s ref=%d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), ref);

	return ref;
}

static void init_pcm_ops_lock(void)
{
	int i = 0;
	int j = 0;
	struct scene_data_s *scene_data;

	scene_data = get_scene_data();
	for (i = 0; i < VBC_DAI_ID_MAX; i++) {
		for (j = 0; j < STREAM_CNT; j++) {
			mutex_init(&scene_data->lock_startup[i][j]);
			mutex_init(&scene_data->lock_hw_param[i][j]);
			spin_lock_init(&scene_data->lock_trigger[i][j]);
		}
	}
}

static int triggered_flag(int cmd)
{
	int32_t trigger_flag = false;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		trigger_flag = true;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		trigger_flag = false;
		break;
	default:
		trigger_flag = false;
	}

	return trigger_flag;
}

static bool is_only_normal_p_scene(void)
{
	struct aud_pm_vbc *pm_vbc;
	int scene_idx, stream;
	int normal_p_cnt = 0;
	int other_cnt = 0;
	bool only_normal_p;

	pm_vbc = aud_pm_vbc_get();
	mutex_lock(&pm_vbc->lock_scene_flag);
	for (scene_idx = 0; scene_idx < VBC_DAI_ID_MAX; scene_idx++) {
		for (stream = 0; stream < STREAM_CNT; stream++) {
			if (pm_vbc->scene_flag[scene_idx][stream] == 1) {
				if (scene_idx == VBC_DAI_ID_NORMAL_AP01 &&
				    stream == SNDRV_PCM_STREAM_PLAYBACK)
					normal_p_cnt++;
				else
					other_cnt++;
			}
		}
	}
	only_normal_p = normal_p_cnt == 1 && other_cnt == 0;
	mutex_unlock(&pm_vbc->lock_scene_flag);
	pr_debug("%s normal_p_cnt=%d, other_cnt=%d, only normal=%s\n", __func__,
		normal_p_cnt, other_cnt, only_normal_p ? "true" : "false");

	return only_normal_p;
}

static void set_scene_flag(int scene_id, int stream)
{
	struct aud_pm_vbc *pm_vbc;
	int flag;

	pm_vbc = aud_pm_vbc_get();
	mutex_lock(&pm_vbc->lock_scene_flag);
	flag = ++pm_vbc->scene_flag[scene_id][stream];
	mutex_unlock(&pm_vbc->lock_scene_flag);
	pr_debug("%s %s %s flag = %d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), flag);
}

static void clr_scene_flag(int scene_id, int stream)
{
	struct aud_pm_vbc *pm_vbc;
	int flag = 0;

	pm_vbc = aud_pm_vbc_get();
	mutex_lock(&pm_vbc->lock_scene_flag);
	if (pm_vbc->scene_flag[scene_id][stream])
		flag = --pm_vbc->scene_flag[scene_id][stream];
	mutex_unlock(&pm_vbc->lock_scene_flag);
	pr_debug("%s %s %s flag = %d\n", __func__,
		scene_id_to_str(scene_id), stream_to_str(stream), flag);
}

static int normal_suspend(struct snd_soc_dai *dai)
{
	bool only_play;
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	if (!dai->playback_active)
		return 0;

	normal_p_suspend_resume_mtx_lock();
	normal_p_suspend_resume_add_ref();
	if (normal_p_suspend_resume_get_ref() == 1) {
		only_play = is_only_normal_p_scene();
		if (only_play) {
			vbc_normal_suspend();
			pm_vbc->suspend_resume = true;
		}
	}
	normal_p_suspend_resume_mtx_unlock();

	return 0;
}

static int normal_resume(struct snd_soc_dai *dai)
{
	bool only_play;
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	if (!dai->playback_active)
		return 0;

	normal_p_suspend_resume_mtx_lock();
	normal_p_suspend_resume_dec_ref();
	if (normal_p_suspend_resume_get_ref() == 0) {
		only_play = is_only_normal_p_scene();
		if (only_play) {
			if (pm_vbc->suspend_resume) {
				vbc_normal_resume();
				pm_vbc->suspend_resume = false;
			}
		}
	}
	normal_p_suspend_resume_mtx_unlock();

	return 0;
}

static int scene_normal_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	int be_dai_id = dai->id;
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		normal_vbc_protect_mutex_lock(stream);
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret) {
			startup_dec_ref(scene_id, stream);
			normal_vbc_protect_mutex_unlock(stream);
			startup_unlock_mtx(scene_id, stream);
			return ret;
		}
		set_scene_flag(scene_id, stream);
		normal_vbc_protect_spin_lock(stream);
		set_normal_p_running_status(stream, true);
		normal_vbc_protect_spin_unlock(stream);
		normal_vbc_protect_mutex_unlock(stream);
	}
	startup_unlock_mtx(scene_id, stream);
	return ret;
}

static void scene_normal_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int is_started;
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		normal_vbc_protect_mutex_lock(stream);
		is_started = get_normal_p_running_status(stream);
		if ((is_playback && is_started) ||
				!is_playback) {
			dsp_shutdown(vbc_codec, scene_id, stream);
			normal_vbc_protect_spin_lock(stream);
			set_normal_p_running_status(stream, false);
			normal_vbc_protect_spin_unlock(stream);
		}
		normal_vbc_protect_mutex_unlock(stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_normal_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	int vbc_chan = VBC_ALL_CHAN;
	int chan_cnt;
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	int is_started;
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);

	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		vbc_chan = to_vbc_chan(chan_cnt);
		set_vbc_chan(scene_id, stream, vbc_chan);
		normal_vbc_protect_mutex_lock(stream);
		is_started = get_normal_p_running_status(stream);
		if ((is_playback && is_started) ||
				!is_playback)
			ap_hw_params(vbc_codec, scene_id, stream,
				     vbc_chan, rate, data_fmt);
		normal_vbc_protect_mutex_unlock(stream);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_normal_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_normal_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int vbc_chan = VBC_ALL_CHAN;
	int is_started;
	int is_playback = stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0;

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			normal_vbc_protect_spin_lock(stream);
			is_started = get_normal_p_running_status(stream);
			if ((is_playback && is_started) ||
				!is_playback) {
				ap_trigger(vbc_codec, scene_id, stream,
					   vbc_chan, up_down);
				ret = dsp_trigger(vbc_codec, scene_id,
						  stream, up_down);
			}
			normal_vbc_protect_spin_unlock(stream);
		}
		trigger_unlock_spin(scene_id, stream);

	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 0) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			normal_vbc_protect_spin_lock(stream);
			is_started = get_normal_p_running_status(stream);
			if ((is_playback && is_started) ||
				!is_playback)
				ap_trigger(vbc_codec, scene_id, stream,
					   vbc_chan, up_down);
			normal_vbc_protect_spin_unlock(stream);
		}
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops normal_ops = {
	.startup = scene_normal_startup,
	.shutdown = scene_normal_shutdown,
	.hw_params = scene_normal_hw_params,
	.trigger = scene_normal_trigger,
	.hw_free = scene_normal_hw_free,
};

/* normal ap23 */
static int scene_normal_ap23_startup(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP23;
	int be_dai_id = dai->id;
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_normal_ap23_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP23;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_normal_ap23_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	int vbc_chan = VBC_ALL_CHAN;
	int chan_cnt;
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP23;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);

	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		vbc_chan = to_vbc_chan(chan_cnt);
		set_vbc_chan(scene_id, stream, vbc_chan);
		ap_hw_params(vbc_codec, scene_id, stream,
			     vbc_chan, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_normal_ap23_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP23;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_normal_ap23_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP23;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int vbc_chan = VBC_ALL_CHAN;

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		}
		trigger_unlock_spin(scene_id, stream);

	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 0) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
		}
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops normal_ap23_ops = {
	.startup = scene_normal_ap23_startup,
	.shutdown = scene_normal_ap23_shutdown,
	.hw_params = scene_normal_ap23_hw_params,
	.trigger = scene_normal_ap23_trigger,
	.hw_free = scene_normal_ap23_hw_free,
};

/* capture dsp */
static int scene_capture_dsp_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_CAPTURE_DSP;
	int be_dai_id = dai->id;
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_capture_dsp_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_CAPTURE_DSP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

}
static int scene_capture_dsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_CAPTURE_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
			   data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);

	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);

	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
				  chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_capture_dsp_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_CAPTURE_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_capture_dsp_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_CAPTURE_DSP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);

		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops capture_dsp_ops = {
	.startup = scene_capture_dsp_startup,
	.shutdown = scene_capture_dsp_shutdown,
	.hw_params = scene_capture_dsp_hw_params,
	.trigger = scene_capture_dsp_trigger,
	.hw_free = scene_capture_dsp_hw_free,
};

/* fast */
static int scene_fast_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FAST_P;
	int be_dai_id = dai->id;
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_fast_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FAST_P;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_fast_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FAST_P;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);

	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);

	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fast_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FAST_P;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fast_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FAST_P;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);

		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops fast_ops = {
	.startup = scene_fast_startup,
	.shutdown = scene_fast_shutdown,
	.hw_params = scene_fast_hw_params,
	.trigger = scene_fast_trigger,
	.hw_free = scene_fast_hw_free,
};

/* offload */
static int scene_offload_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD;
	int ret = 0;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int be_dai_id = dai->id;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_offload_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_offload_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate = 48000;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt = 2;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;

	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_offload_hw_free(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_offload_trigger(struct snd_pcm_substream *substream, int cmd,
				 struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops offload_ops = {
	.startup = scene_offload_startup,
	.shutdown = scene_offload_shutdown,
	.hw_params = scene_offload_hw_params,
	.trigger = scene_offload_trigger,
	.hw_free = scene_offload_hw_free,
};

/* offload a2dp */
static int scene_offload_a2dp_startup(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD_A2DP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int be_dai_id = dai->id;
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_offload_a2dp_shutdown(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD_A2DP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_offload_a2dp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD_A2DP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_offload_a2dp_hw_free(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD_A2DP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_offload_a2dp_trigger(struct snd_pcm_substream *substream,
				      int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_OFFLOAD_A2DP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops offload_a2dp_ops = {
	.startup = scene_offload_a2dp_startup,
	.shutdown = scene_offload_a2dp_shutdown,
	.hw_params = scene_offload_a2dp_hw_params,
	.trigger = scene_offload_a2dp_trigger,
	.hw_free = scene_offload_a2dp_hw_free,
};

/* pcm a2dp */
static int scene_pcm_a2dp_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_PCM_A2DP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int be_dai_id = dai->id;
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_pcm_a2dp_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_PCM_A2DP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_pcm_a2dp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_PCM_A2DP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_pcm_a2dp_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_PCM_A2DP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_pcm_a2dp_trigger(struct snd_pcm_substream *substream, int cmd,
				  struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_PCM_A2DP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops pcm_a2dp_ops = {
	.startup = scene_pcm_a2dp_startup,
	.shutdown = scene_pcm_a2dp_shutdown,
	.hw_params = scene_pcm_a2dp_hw_params,
	.trigger = scene_pcm_a2dp_trigger,
	.hw_free = scene_pcm_a2dp_hw_free,
};

/* voice */
static int scene_voice_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_voice_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_voice_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);


	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_voice_hw_free(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_voice_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops voice_ops = {
	.startup = scene_voice_startup,
	.shutdown = scene_voice_shutdown,
	.hw_params = scene_voice_hw_params,
	.trigger = scene_voice_trigger,
	.hw_free = scene_voice_hw_free,
};

/* voice capture*/
static int scene_voice_capture_startup(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE_CAPTURE;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;
	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_voice_capture_shutdown(struct snd_pcm_substream *substream,
					 struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE_CAPTURE;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_voice_capture_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE_CAPTURE;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_voice_capture_hw_free(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE_CAPTURE;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_voice_capture_trigger(struct snd_pcm_substream *substream,
				       int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOICE_CAPTURE;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;

	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops voice_capture_ops = {
	.startup = scene_voice_capture_startup,
	.shutdown = scene_voice_capture_shutdown,
	.hw_params = scene_voice_capture_hw_params,
	.trigger = scene_voice_capture_trigger,
	.hw_free = scene_voice_capture_hw_free,
};

/* voip */
static int scene_voip_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOIP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_voip_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOIP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_voip_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOIP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_voip_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOIP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_voip_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_VOIP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops voip_ops = {
	.startup = scene_voip_startup,
	.shutdown = scene_voip_shutdown,
	.hw_params = scene_voip_hw_params,
	.trigger = scene_voip_trigger,
	.hw_free = scene_voip_hw_free,
};

/* loop */
static int scene_loop_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_LOOP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_loop_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_LOOP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_loop_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_LOOP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_loop_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_LOOP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_loop_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_LOOP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd = %d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops loop_ops = {
	.startup = scene_loop_startup,
	.shutdown = scene_loop_shutdown,
	.hw_params = scene_loop_hw_params,
	.trigger = scene_loop_trigger,
	.hw_free = scene_loop_hw_free,
};

/* FM */
static int scene_fm_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		pr_info("%s, force_on_xtl\n", __func__);
		force_on_xtl(true);
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret) {
			startup_dec_ref(scene_id, stream);
			force_on_xtl(false);
		} else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_fm_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		pr_info("%s, force_off_xtl\n", __func__);
		force_on_xtl(false);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_fm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fm_hw_free(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fm_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops fm_ops = {
	.startup = scene_fm_startup,
	.shutdown = scene_fm_shutdown,
	.hw_params = scene_fm_hw_params,
	.trigger = scene_fm_trigger,
	.hw_free = scene_fm_hw_free,
};

/* bt capture*/
static int scene_bt_capture_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_CAPTURE_AP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_bt_capture_shutdown(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_CAPTURE_AP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_bt_capture_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_CAPTURE_AP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;
	int vbc_chan = VBC_ALL_CHAN;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		vbc_chan = to_vbc_chan(chan_cnt);
		set_vbc_chan(scene_id, stream, vbc_chan);
		ap_hw_params(vbc_codec, scene_id, stream, vbc_chan, rate,
			     data_fmt);
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_bt_capture_hw_free(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_CAPTURE_AP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;

	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_bt_capture_trigger(struct snd_pcm_substream *substream,
				    int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_CAPTURE_AP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int vbc_chan = VBC_ALL_CHAN;

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
			normal_vbc_protect_spin_unlock(stream);
		}
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 0) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
		}
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}


static struct snd_soc_dai_ops bt_capture_ops = {
	.startup = scene_bt_capture_startup,
	.shutdown = scene_bt_capture_shutdown,
	.hw_params = scene_bt_capture_hw_params,
	.trigger = scene_bt_capture_trigger,
	.hw_free = scene_bt_capture_hw_free,
};

/* fm capture*/
static int scene_fm_capture_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_AP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;

	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1)
		set_scene_flag(scene_id, stream);

	startup_unlock_mtx(scene_id, stream);

	return 0;
}

static void scene_fm_capture_shutdown(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_AP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0)
		clr_scene_flag(scene_id, stream);

	startup_unlock_mtx(scene_id, stream);
}

static int scene_fm_capture_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_AP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;
	int vbc_chan = VBC_ALL_CHAN;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		vbc_chan = to_vbc_chan(chan_cnt);
		set_vbc_chan(scene_id, stream, vbc_chan);
		ap_hw_params(vbc_codec, scene_id, stream, vbc_chan, rate,
			     data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fm_capture_hw_free(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_AP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fm_capture_trigger(struct snd_pcm_substream *substream,
				    int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_AP;
	int ret;
	int vbc_chan = VBC_ALL_CHAN;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;

	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
		}
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 0) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
		}
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops fm_capture_ops = {
	.startup = scene_fm_capture_startup,
	.shutdown = scene_fm_capture_shutdown,
	.hw_params = scene_fm_capture_hw_params,
	.trigger = scene_fm_capture_trigger,
	.hw_free = scene_fm_capture_hw_free,
};

/* capture fm dsp */
static int scene_capture_fm_dsp_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_DSP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_capture_fm_dsp_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_DSP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

}
static int scene_capture_fm_dsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
			   data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);

	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);

	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
				  chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}


static int scene_capture_fm_dsp_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}


static int scene_capture_fm_dsp_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_CAPTURE_DSP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);

		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops capture_fm_dsp_ops = {
	.startup = scene_capture_fm_dsp_startup,
	.shutdown = scene_capture_fm_dsp_shutdown,
	.hw_params = scene_capture_fm_dsp_hw_params,
	.trigger = scene_capture_fm_dsp_trigger,
	.hw_free = scene_capture_fm_dsp_hw_free,
};

/* capture btsco dsp */
static int scene_capture_btsco_dsp_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_SCO_CAPTURE_DSP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret)
			startup_dec_ref(scene_id, stream);
		else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_capture_btsco_dsp_shutdown(
	struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_SCO_CAPTURE_DSP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

}

static int scene_capture_btsco_dsp_hw_params(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_SCO_CAPTURE_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
			   data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);

	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);

	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
				  chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}


static int scene_capture_btsco_dsp_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_SCO_CAPTURE_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_capture_btsco_dsp_trigger(struct snd_pcm_substream *substream,
				     int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_BT_SCO_CAPTURE_DSP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);

		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops capture_btsco_dsp_ops = {
	.startup = scene_capture_btsco_dsp_startup,
	.shutdown = scene_capture_btsco_dsp_shutdown,
	.hw_params = scene_capture_btsco_dsp_hw_params,
	.trigger = scene_capture_btsco_dsp_trigger,
	.hw_free = scene_capture_btsco_dsp_hw_free,
};

/* FM DSP */
static int scene_fm_dsp_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_DSP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int ret = 0;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		set_scene_flag(scene_id, stream);
		pr_info("%s, force_on_xtl\n", __func__);
		force_on_xtl(true);
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret) {
			startup_dec_ref(scene_id, stream);
			force_on_xtl(false);
		} else
			set_scene_flag(scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);

	return ret;
}

static void scene_fm_dsp_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_DSP;
	int be_dai_id = dai->id;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__,
		dai_id_to_str(be_dai_id),
		be_dai_id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(be_dai_id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return;
	}

	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		clr_scene_flag(scene_id, stream);
		pr_info("%s, force_off_xtl\n", __func__);
		force_on_xtl(false);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}

static int scene_fm_dsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		dsp_hw_params(vbc_codec, scene_id, stream,
			      chan_cnt, rate, data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fm_dsp_hw_free(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_DSP;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_fm_dsp_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_FM_DSP;
	int ret;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s, cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1)
			ret = dsp_trigger(vbc_codec, scene_id, stream, up_down);
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops fm_dsp_ops = {
	.startup = scene_fm_dsp_startup,
	.shutdown = scene_fm_dsp_shutdown,
	.hw_params = scene_fm_dsp_hw_params,
	.trigger = scene_fm_dsp_trigger,
	.hw_free = scene_fm_dsp_hw_free,
};

/* vbc dump */
static int scene_dump_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	enum vbc_dump_position_e pos;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	int ret;

	pr_info("%s dai:%s(%d) %s, scene: %s\n", __func__,
		dai_id_to_str(dai->id), dai->id, stream_to_str(stream),
		scene_id_to_str(scene_id));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}
	if (!vbc_codec)
		return 0;
	startup_lock_mtx(scene_id, stream);
	startup_add_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 1) {
		ret = dsp_startup(vbc_codec, scene_id, stream);
		if (ret) {
			startup_dec_ref(scene_id, stream);
			startup_unlock_mtx(scene_id, stream);
			return ret;
		}
		pos = vbc_codec->vbc_dump_position;
		pr_info("dump scene pos = %s\n", vbc_dumppos2name(pos));
		switch (pos) {
		default:
		case DUMP_POS_DAC0_E:
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC0);
			break;
		case DUMP_POS_DAC1_E:
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC1);
			break;
		case DUMP_POS_A4:
			dsp_vbc_mux_loop_da0_set(VBC_MUX_LOOP_DAC0,
				DAC0_MBDRC_OUT);
			dsp_vbc_mux_loop_da0_da1_set(VBC_MUX_LOOP_DAC0_DAC1,
						     DAC0_DAC1_SEL_DAC0);
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC_LOOP);
			break;
		case DUMP_POS_A3:
			dsp_vbc_mux_loop_da0_set(VBC_MUX_LOOP_DAC0,
				DAC0_EQ4_OUT);
			dsp_vbc_mux_loop_da0_da1_set(VBC_MUX_LOOP_DAC0_DAC1,
						     DAC0_DAC1_SEL_DAC0);
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC_LOOP);
			break;
		case DUMP_POS_A2:
			dsp_vbc_mux_loop_da0_set(VBC_MUX_LOOP_DAC0,
				DAC0_MIX1_OUT);
			dsp_vbc_mux_loop_da0_da1_set(VBC_MUX_LOOP_DAC0_DAC1,
						     DAC0_DAC1_SEL_DAC0);
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC_LOOP);
			break;
		case DUMP_POS_A1:
			dsp_vbc_mux_loop_da0_set(VBC_MUX_LOOP_DAC0,
				DAC0_SMTHDG_OUT);
			dsp_vbc_mux_loop_da0_da1_set(VBC_MUX_LOOP_DAC0_DAC1,
						     DAC0_DAC1_SEL_DAC0);
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC_LOOP);
			break;
		case DUMP_POS_V2:
			dsp_vbc_mux_loop_da1_set(VBC_MUX_LOOP_DAC1,
				DAC1_MIXERDG_OUT);
			dsp_vbc_mux_loop_da0_da1_set(VBC_MUX_LOOP_DAC0_DAC1,
						     DAC0_DAC1_SEL_DAC1);
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC_LOOP);
			break;
		case DUMP_POS_V1:
			dsp_vbc_mux_loop_da1_set(VBC_MUX_LOOP_DAC1,
				DAC1_MIXER_OUT);
			dsp_vbc_mux_loop_da0_da1_set(VBC_MUX_LOOP_DAC0_DAC1,
						     DAC0_DAC1_SEL_DAC1);
			dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_DAC_LOOP);
			break;
		}
		dsp_vbc_mux_audrcd_set(VBC_MUX_AUDRCD01, AUDRCD_IN_ADC0);
	}
	startup_unlock_mtx(scene_id, stream);

	return 0;
}

static void scene_dump_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int scene_id = VBC_DAI_ID_NORMAL_AP01;

	pr_info("%s dai:%s(%d) %s, scene: %s\n", __func__,
		dai_id_to_str(dai->id), dai->id, stream_to_str(stream),
		scene_id_to_str(scene_id));
	if (!vbc_codec)
		return;
	startup_lock_mtx(scene_id, stream);
	startup_dec_ref(scene_id, stream);
	if (startup_get_ref(scene_id, stream) == 0) {
		dsp_vbc_mux_audrcd_set(VBC_MUX_AUDRCD01, AUDRCD_IN_ADC0);
		dsp_vbc_mux_adc_set(VBC_MUX_IN_ADC0, ADC_IN_IIS0_ADC);
		dsp_shutdown(vbc_codec, scene_id, stream);
	}
	startup_unlock_mtx(scene_id, stream);
}
static int scene_dump_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	unsigned int rate;
	int data_fmt = VBC_DAT_L16;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	int chan_cnt;
	int vbc_chan = VBC_ALL_CHAN;

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data_fmt = VBC_DAT_L16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data_fmt = VBC_DAT_L24;
		break;
	default:
		pr_err("%s, ERR:VBC not support data fmt =%d", __func__,
		       data_fmt);
		break;
	}
	chan_cnt = params_channels(params);
	rate = params_rate(params);
	pr_info("%s data_fmt=%s, chan=%u, rate =%u\n", __func__,
		vbc_data_fmt_to_str(data_fmt), chan_cnt, rate);
	if (chan_cnt > 2)
		pr_warn("%s channel count invalid\n", __func__);
	hw_param_lock_mtx(scene_id, stream);
	hw_param_add_ref(scene_id, stream);
	if (hw_param_get_ref(scene_id, stream) == 1) {
		vbc_chan = to_vbc_chan(chan_cnt);
		set_vbc_chan(scene_id, stream, vbc_chan);
		ap_hw_params(vbc_codec, scene_id, stream, vbc_chan, rate,
			     data_fmt);
	}
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_dump_hw_free(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s\n", __func__, dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream));
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;
	hw_param_lock_mtx(scene_id, stream);
	hw_param_dec_ref(scene_id, stream);
	hw_param_unlock_mtx(scene_id, stream);

	return 0;
}

static int scene_dump_trigger(struct snd_pcm_substream *substream,
				    int cmd, struct snd_soc_dai *dai)
{
	int up_down;
	int stream = substream->stream;
	int scene_id = VBC_DAI_ID_NORMAL_AP01;
	int ret;
	int vbc_chan = VBC_ALL_CHAN;
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);

	pr_info("%s dai:%s(%d) scene:%s %s cmd=%d\n", __func__,
		dai_id_to_str(dai->id),
		dai->id, scene_id_to_str(scene_id), stream_to_str(stream), cmd);
	if (scene_id != check_be_dai_id(dai->id)) {
		pr_err("%s check_be_dai_id failed\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec)
		return 0;

	up_down = triggered_flag(cmd);
	/* default ret is 0 */
	ret = 0;
	if (up_down == 1) {
		trigger_lock_spin(scene_id, stream);
		trigger_add_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 1) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
		}
		trigger_unlock_spin(scene_id, stream);
	} else {
		trigger_lock_spin(scene_id, stream);
		trigger_dec_ref(scene_id, stream);
		if (trigger_get_ref(scene_id, stream) == 0) {
			vbc_chan = get_vbc_chan(scene_id, stream);
			ap_trigger(vbc_codec, scene_id, stream, vbc_chan,
				   up_down);
		}
		trigger_unlock_spin(scene_id, stream);
	}

	return ret;
}

static struct snd_soc_dai_ops vbc_dump_ops = {
	.startup = scene_dump_startup,
	.shutdown = scene_dump_shutdown,
	.hw_params = scene_dump_hw_params,
	.trigger = scene_dump_trigger,
	.hw_free = scene_dump_hw_free,
};

static struct snd_soc_dai_driver vbc_dais[BE_DAI_ID_MAX] = {
	/* 0: BE_DAI_ID_NORMAL_AP01_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_NORMAL_AP01_CODEC),
		.id = BE_DAI_ID_NORMAL_AP01_CODEC,
		.playback = {
			.stream_name = "BE_DAI_NORMAL_AP01_CODEC_P",
			.aif_name = "BE_IF_NORMAL_AP01_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_NORMAL_AP01_CODEC_C",
			.aif_name = "BE_IF_NORMAL_AP01_CODEC_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &normal_ops,
		.resume = normal_resume,
		.suspend = normal_suspend,
		.bus_control = true,
	},
	/* 1: BE_DAI_ID_NORMAL_AP23_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_NORMAL_AP23_CODEC),
		.id = BE_DAI_ID_NORMAL_AP23_CODEC,
		.playback = {
			.stream_name = "BE_DAI_NORMAL_AP23_CODEC_P",
			.aif_name = "BE_IF_NORMAL_AP23_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_NORMAL_AP23_CODEC_C",
			.aif_name = "BE_IF_NORMAL_AP23_CODEC_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &normal_ap23_ops,
		.resume = normal_resume,
		.suspend = normal_suspend,
		.bus_control = true,
	},
	/* 2: BE_DAI_ID_CAPTURE_DSP_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_CAPTURE_DSP_CODEC),
		.id = BE_DAI_ID_CAPTURE_DSP_CODEC,
		.capture = {
			.stream_name = "BE_DAI_CAP_DSP_CODEC_C",
			.aif_name = "BE_IF_CAP_DSP_CODEC_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &capture_dsp_ops,
	},
	/* 3: BE_DAI_ID_FAST_P_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_FAST_P_CODEC),
		.id = BE_DAI_ID_FAST_P_CODEC,
		.playback = {
			.stream_name = "BE_DAI_FAST_CODEC_P",
			.aif_name = "BE_IF_FAST_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fast_ops,
	},
	/* 4: BE_DAI_ID_OFFLOAD_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_OFFLOAD_CODEC),
		.id = BE_DAI_ID_OFFLOAD_CODEC,
		.playback = {
			.stream_name = "BE_DAI_OFFLOAD_CODEC_P",
			.aif_name = "BE_IF_OFFLOAD_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &offload_ops,
	},
	/* 5: BE_DAI_ID_VOICE_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_VOICE_CODEC),
		.id = BE_DAI_ID_VOICE_CODEC,
		.playback = {
			.stream_name = "BE_DAI_VOICE_CODEC_P",
			.aif_name = "BE_IF_VOICE_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_VOICE_CODEC_C",
			.aif_name = "BE_IF_VOICE_CODEC_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voice_ops,
	},
	/* 6: BE_DAI_ID_VOIP_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_VOIP_CODEC),
		.id = BE_DAI_ID_VOIP_CODEC,
		.playback = {
			.stream_name = "BE_DAI_VOIP_CODEC_P",
			.aif_name = "BE_IF_VOIP_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_VOIP_CODEC_C",
			.aif_name = "BE_IF_VOIP_CODEC_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voip_ops,
	},
	/* 7: BE_DAI_ID_FM_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_FM_CODEC),
		.id = BE_DAI_ID_FM_CODEC,
		.playback = {
			.stream_name = "BE_DAI_FM_CODEC_P",
			.aif_name = "BE_IF_FM_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fm_ops,
	},
	/* 8: BE_DAI_ID_LOOP_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_LOOP_CODEC),
		.id = BE_DAI_ID_LOOP_CODEC,
		.playback = {
			.stream_name = "BE_DAI_LOOP_CODEC_P",
			.aif_name = "BE_IF_LOOP_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_LOOP_CODEC_C",
			.aif_name = "BE_IF_LOOP_CODEC_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &loop_ops,
	},
	/* 9: BE_DAI_ID_FM_DSP_CODEC */
	{
		.name = TO_STRING(BE_DAI_ID_FM_DSP_CODEC),
		.id = BE_DAI_ID_FM_DSP_CODEC,
		.playback = {
			.stream_name = "BE_DAI_FM_DSP_CODEC_P",
			.aif_name = "BE_IF_FM_DSP_CODEC_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fm_dsp_ops,
	},
	/* 10: BE_DAI_ID_NORMAL_AP01_USB */
	{
		.name = TO_STRING(BE_DAI_ID_NORMAL_AP01_USB),
		.id = BE_DAI_ID_NORMAL_AP01_USB,
		.playback = {
			.stream_name = "BE_DAI_NORMAL_AP01_USB_P",
			.aif_name = "BE_IF_NORMAL_AP01_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_NORMAL_AP01_USB_C",
			.aif_name = "BE_IF_NORMAL_AP01_USB_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &normal_ops,
		.resume = normal_resume,
		.suspend = normal_suspend,
		.bus_control = true,
	},
	/* 11: BE_DAI_ID_NORMAL_AP23_USB */
	{
		.name = TO_STRING(BE_DAI_ID_NORMAL_AP23_USB),
		.id = BE_DAI_ID_NORMAL_AP23_USB,
		.playback = {
			.stream_name = "BE_DAI_NORMAL_AP23_USB_P",
			.aif_name = "BE_IF_NORMAL_AP23_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_NORMAL_AP23_USB_C",
			.aif_name = "BE_IF_NORMAL_AP23_USB_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &normal_ops,
		.resume = normal_resume,
		.suspend = normal_suspend,
		.bus_control = true,
	},
	/* 12: BE_DAI_ID_CAPTURE_DSP_USB */
	{
		.name = TO_STRING(BE_DAI_ID_CAPTURE_DSP_USB),
		.id = BE_DAI_ID_CAPTURE_DSP_USB,
		.capture = {
			.stream_name = "BE_DAI_CAP_DSP_USB_C",
			.aif_name = "BE_IF_CAP_DSP_USB_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &capture_dsp_ops,
	},
	/* 13: BE_DAI_ID_FAST_P_USB */
	{
		.name = TO_STRING(BE_DAI_ID_FAST_P_USB),
		.id = BE_DAI_ID_FAST_P_USB,
		.playback = {
			.stream_name = "BE_DAI_FAST_USB_P",
			.aif_name = "BE_IF_FAST_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fast_ops,
	},
	/* 14: BE_DAI_ID_OFFLOAD_USB */
	{
		.name = TO_STRING(BE_DAI_ID_OFFLOAD_USB),
		.id = BE_DAI_ID_OFFLOAD_USB,
		.playback = {
			.stream_name = "BE_DAI_OFFLOAD_USB_P",
			.aif_name = "BE_IF_OFFLOAD_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &offload_ops,
	},
	/* 15: BE_DAI_ID_VOICE_USB */
	{
		.name = TO_STRING(BE_DAI_ID_VOICE_USB),
		.id = BE_DAI_ID_VOICE_USB,
		.playback = {
			.stream_name = "BE_DAI_VOICE_USB_P",
			.aif_name = "BE_IF_VOICE_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_VOICE_USB_C",
			.aif_name = "BE_IF_VOICE_USB_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voice_ops,
	},
	/* 16: BE_DAI_ID_VOIP_USB */
	{
		.name = TO_STRING(BE_DAI_ID_VOIP_USB),
		.id = BE_DAI_ID_VOIP_USB,
		.playback = {
			.stream_name = "BE_DAI_VOIP_USB_P",
			.aif_name = "BE_IF_VOIP_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_VOIP_USB_C",
			.aif_name = "BE_IF_VOIP_USB_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voip_ops,
	},
	/* 17: BE_DAI_ID_FM_USB */
	{
		.name = TO_STRING(BE_DAI_ID_FM_USB),
		.id = BE_DAI_ID_FM_USB,
		.playback = {
			.stream_name = "BE_DAI_FM_USB_P",
			.aif_name = "BE_IF_FM_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fm_ops,
	},
	/* 18: BE_DAI_ID_LOOP_USB */
	{
		.name = TO_STRING(BE_DAI_ID_LOOP_USB),
		.id = BE_DAI_ID_LOOP_USB,
		.playback = {
			.stream_name = "BE_DAI_LOOP_USB_P",
			.aif_name = "BE_IF_LOOP_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_LOOP_USB_C",
			.aif_name = "BE_IF_LOOP_USB_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &loop_ops,
	},
	/* 19: BE_DAI_ID_FM_DSP_USB */
	{
		.name = TO_STRING(BE_DAI_ID_FM_DSP_USB),
		.id = BE_DAI_ID_FM_DSP_USB,
		.playback = {
			.stream_name = "BE_DAI_FM_DSP_USB_P",
			.aif_name = "BE_IF_FM_DSP_USB_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fm_dsp_ops,
	},
	/* 20: BE_DAI_ID_OFFLOAD_A2DP */
	{
		.name = TO_STRING(BE_DAI_ID_OFFLOAD_A2DP),
		.id = BE_DAI_ID_OFFLOAD_A2DP,
		.playback = {
			.stream_name = "BE_DAI_OFFLOAD_A2DP_P",
			.aif_name = "BE_IF_OFFLOAD_A2DP_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &offload_a2dp_ops,
	},
	/* 21: BE_DAI_ID_PCM_A2DP */
	{
		.name = TO_STRING(BE_DAI_ID_PCM_A2DP),
		.id = BE_DAI_ID_PCM_A2DP,
		.playback = {
			.stream_name = "BE_DAI_PCM_A2DP_P",
			.aif_name = "BE_IF_PCM_A2DP_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &pcm_a2dp_ops,
	},
	/* 22: BE_DAI_ID_VOICE_BT */
	{
		.name = TO_STRING(BE_DAI_ID_VOICE_BT),
		.id = BE_DAI_ID_VOICE_BT,
		.playback = {
			.stream_name = "BE_DAI_VOICE_BT_P",
			.aif_name = "BE_IF_VOICE_BT_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_VOICE_BT_C",
			.aif_name = "BE_IF_VOICE_BT_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voice_ops,
	},
	/* 23: BE_DAI_ID_VOIP_BT */
	{
		.name = TO_STRING(BE_DAI_ID_VOIP_BT),
		.id = BE_DAI_ID_VOIP_BT,
		.playback = {
			.stream_name = "BE_DAI_VOIP_BT_P",
			.aif_name = "BE_IF_VOIP_BT_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_VOIP_BT_C",
			.aif_name = "BE_IF_VOIP_BT_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voip_ops,
	},
	/* 24: BE_DAI_ID_LOOP_BT */
	{
		.name = TO_STRING(BE_DAI_ID_LOOP_BT),
		.id = BE_DAI_ID_LOOP_BT,
		.playback = {
			.stream_name = "BE_DAI_LOOP_BT_P",
			.aif_name = "BE_IF_LOOP_BT_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "BE_DAI_LOOP_BT_C",
			.aif_name = "BE_IF_LOOP_BT_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &loop_ops,
	},
	/* 25: BE_DAI_ID_CAPTURE_BT */
	{
		.name = TO_STRING(BE_DAI_ID_CAPTURE_BT),
		.id = BE_DAI_ID_CAPTURE_BT,
		.capture = {
			.stream_name = "BE_DAI_CAP_BT_C",
			.aif_name = "BE_IF_CAP_BT_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &bt_capture_ops,
	},
	/* 26: BE_DAI_ID_VOICE_CAPTURE */
	{
		.name = TO_STRING(BE_DAI_ID_VOICE_CAPTURE),
		.id = BE_DAI_ID_VOICE_CAPTURE,
		.capture = {
			.stream_name = "BE_DAI_VOICE_CAP_C",
			.aif_name = "BE_IF_VOICE_CAP_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voice_capture_ops,
	},
	/* 27: BE_DAI_ID_FM_CAPTURE */
	{
		.name = TO_STRING(BE_DAI_ID_FM_CAPTURE),
		.id = BE_DAI_ID_FM_CAPTURE,
		.capture = {
			.stream_name = "BE_DAI_FM_CAP_C",
			.aif_name = "BE_IF_CAP_FM_CAP_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fm_capture_ops,
	},
	/* 28: BE_DAI_ID_CAPTURE_FM_DSP */
	{
		.name = TO_STRING(BE_DAI_ID_FM_CAPTURE_DSP),
		.id = BE_DAI_ID_FM_CAPTURE_DSP,
		.capture = {
			.stream_name = "BE_DAI_CAP_DSP_FM_C",
			.aif_name = "BE_IF_CAP_DSP_FM_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &capture_fm_dsp_ops,
	},
	/* 29: BE_DAI_ID_CAPTURE_DSP_BTSCO */
	{
		.name = TO_STRING(BE_DAI_ID_CAPTURE_DSP_BTSCO),
		.id = BE_DAI_ID_CAPTURE_DSP_BTSCO,
		.capture = {
			.stream_name = "BE_DAI_CAP_DSP_BTSCO_C",
			.aif_name = "BE_IF_CAP_DSP_BTSCO_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &capture_btsco_dsp_ops,
	},
	/* 30: BE_DAI_ID_DUMP */
	{
		.name = TO_STRING(BE_DAI_ID_DUMP),
		.id = BE_DAI_ID_DUMP,
		.capture = {
			.stream_name = "BE_DAI_DUMP_C",
			.aif_name = "BE_IF_DUMP_C",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &vbc_dump_ops,
	},
	/*
	 * 31. DUMMY CPU DAI NOT BE(no stream, no be dai)
	 * only dais with stream that can play as BE DAI.
	 */
	{
		.name = TO_STRING(BE_DAI_ID_DUMMY_VBC_DAI_NOTBE),
		.id = BE_DAI_ID_DUMMY_VBC_DAI_NOTBE,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
	},
	/* 32: BE_DAI_ID_FAST_P_BTSCO */
	{
		.name = TO_STRING(BE_DAI_ID_FAST_P_BTSCO),
		.id = BE_DAI_ID_FAST_P_BTSCO,
		.playback = {
			.stream_name = "BE_DAI_FAST_BTSCO_P",
			.aif_name = "BE_IF_FAST_BTSCO_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fast_ops,
	},
	/* 33: BE_DAI_ID_NORMAL_AP01_P_BTSCO */
	{
		.name = TO_STRING(BE_DAI_ID_NORMAL_AP01_P_BTSCO),
		.id = BE_DAI_ID_NORMAL_AP01_P_BTSCO,
		.playback = {
			.stream_name = "BE_DAI_NORMAL_AP01_BTSCO_P",
			.aif_name = "BE_IF_NORMAL_AP01_BTSCO_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &normal_ops,
	},
	/* 34: BE_DAI_ID_NORMAL_AP01_P_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_NORMAL_AP01_P_HIFI),
		.id = BE_DAI_ID_NORMAL_AP01_P_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_NORMAL_AP01_P_HIFI",
			.aif_name = "BE_IF_NORMAL_AP01_HIFI_P",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &normal_ops,
	},
	/* 35: BE_DAI_ID_NORMAL_AP23_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_NORMAL_AP23_HIFI),
		.id = BE_DAI_ID_NORMAL_AP23_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_NORMAL_AP23_HIFI",
			.aif_name = "BE_IF_ID_NORMAL_AP23_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &normal_ap23_ops,
	},
	/* 36: BE_DAI_ID_FAST_P_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_FAST_P_HIFI),
		.id = BE_DAI_ID_FAST_P_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_FAST_P_HIFI",
			.aif_name = "BE_IF_ID_FAST_P_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fast_ops,
	},
	/* 37: BE_DAI_ID_OFFLOAD_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_OFFLOAD_HIFI),
		.id = BE_DAI_ID_OFFLOAD_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_OFFLOAD_HIFI",
			.aif_name = "BE_IF_ID_OFFLOAD_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &offload_ops,
	},
	/* 38: BE_DAI_ID_VOICE_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_VOICE_HIFI),
		.id = BE_DAI_ID_VOICE_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_VOICE_HIFI",
			.aif_name = "BE_IF_ID_VOICE_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voice_ops,
	},
	/* 39: BE_DAI_ID_VOIP_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_VOIP_HIFI),
		.id = BE_DAI_ID_VOIP_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_VOIP_HIFI",
			.aif_name = "BE_IF_ID_VOIP_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &voip_ops,
	},
	/* 40: BE_DAI_ID_FM_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_FM_HIFI),
		.id = BE_DAI_ID_FM_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_FM_HIFI",
			.aif_name = "BE_IF_ID_FM_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fm_ops,
	},
	/* 41: BE_DAI_ID_LOOP_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_LOOP_HIFI),
		.id = BE_DAI_ID_LOOP_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_LOOP_HIFI",
			.aif_name = "BE_IF_ID_LOOP_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &loop_ops,
	},
	/* 42: BE_DAI_ID_FM_DSP_HIFI */
	{
		.name = TO_STRING(BE_DAI_ID_FM_DSP_HIFI),
		.id = BE_DAI_ID_FM_DSP_HIFI,
		.playback = {
			.stream_name = "BE_DAI_ID_FM_DSP_HIFI",
			.aif_name = "BE_IF_ID_FM_DSP_HIFI",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_max = 192000,
			.formats = SPRD_VBC_DAI_PCM_FORMATS,
		},
		.probe = sprd_dai_vbc_probe,
		.ops = &fm_dsp_ops,
	},
};

static struct aud_pm_vbc *aud_pm_vbc_get(void)
{
	return pm_vbc;
}

static void pm_vbc_init(void)
{
	struct aud_pm_vbc *pm_vbc;

	pm_vbc = aud_pm_vbc_get();
	if (pm_vbc == NULL)
		return;

	mutex_init(&pm_vbc->pm_mtx_cmd_prot);
	spin_lock_init(&pm_vbc->pm_spin_cmd_prot);
	mutex_init(&pm_vbc->lock_scene_flag);
	mutex_init(&pm_vbc->lock_mtx_suspend_resume);
}

int vbc_of_setup(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	struct regmap *agcp_ahb_gpr;
	struct pinctrl *pctrl;
	struct vbc_codec_priv *vbc_codec;
	int ret;
	u32 val;
	void *sprd_ap_vbc_virt_base;
	u32 sprd_ap_vbc_phy_base;

	if (!pdev) {
		pr_err("ERR: %s, pdev is NULL!\n", __func__);
		return -ENODEV;
	}
	vbc_codec = platform_get_drvdata(pdev);
	if (!vbc_codec) {
		pr_err("%s vbc_codec is null failed\n", __func__);
		return -EINVAL;
	}

	/* Prepare for global registers accessing. */
	agcp_ahb_gpr = syscon_regmap_lookup_by_phandle(
		np, "sprd,syscon-agcp-ahb");
	if (IS_ERR(agcp_ahb_gpr)) {
		pr_err("ERR: [%s] Get the agcp ahb syscon failed!\n",
		       __func__);
		agcp_ahb_gpr = NULL;
		return -EPROBE_DEFER;
	}
	arch_audio_set_agcp_ahb_gpr(agcp_ahb_gpr);

	/* Prepare for vbc registers accessing. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res != NULL) {
		sprd_ap_vbc_phy_base = (u32)res->start;
		sprd_ap_vbc_virt_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(sprd_ap_vbc_virt_base)) {
			pr_err("ERR: cannot create iomap address for AP-VBC!\n");
			return -EINVAL;
		}
	} else {
		pr_err("ERR:Must give me the AP-VBC reg address!\n");
		return -EINVAL;
	}
	set_ap_vbc_virt_base(sprd_ap_vbc_virt_base);
	set_ap_vbc_phy_base(sprd_ap_vbc_phy_base);
	ret = of_property_read_u32(np, "sprd,vbc-phy-offset", &val);
	if (ret) {
		pr_err("ERR: %s :no property of 'reg'\n", __func__);
		return -EINVAL;
	}
	set_vbc_dsp_ap_offset(val);
	/* PIN MUX */
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		pr_err("ERR: %s :get pinctrl failed\n", __func__);
		return -ENODEV;
	}
	sp_asoc_pr_dbg("get pinctrl device!\n");
	vbc_codec->pctrl = pctrl;

	return 0;
}

static int vbc_drv_probe(struct platform_device *pdev)
{
	int ret;
	struct vbc_codec_priv *vbc_codec = NULL;

	pr_info("%s: to setup vbc dt\n", __func__);
	/* 1. probe CODEC */
	ret = sprd_vbc_codec_probe(pdev);
	if (ret < 0)
		goto probe_err;
	vbc_codec = platform_get_drvdata(pdev);
	/*
	 * should first call sprd_vbc_codec_probe
	 * because we will call platform_get_drvdata(pdev)
	 */
	ret = vbc_of_setup(pdev);
	if (ret < 0) {
		pr_err("%s: failed to setup vbc dt, ret=%d\n", __func__, ret);
		return -ENODEV;
	}
	aud_ipc_ch_open(AMSG_CH_DSP_GET_PARAM_FROM_SMSG_NOREPLY);
	aud_ipc_ch_open(AMSG_CH_VBC_CTL);

	/* 2. probe DAIS */
	ret = snd_soc_register_codec(&pdev->dev, &sprd_vbc_codec, vbc_dais,
				     ARRAY_SIZE(vbc_dais));

	if (ret < 0) {
		pr_err("%s, Register VBC to DAIS Failed!\n", __func__);
		goto probe_err;
	}

	pm_vbc = devm_kzalloc(&pdev->dev, sizeof(*pm_vbc), GFP_KERNEL);
	if (!pm_vbc) {
		ret = -ENOMEM;
		goto probe_err;
	}
	pm_vbc_init();
	init_pcm_ops_lock();

	return ret;
probe_err:
	pr_err("%s, error return %i\n", __func__, ret);

	return ret;
}

static int vbc_drv_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);

	sprd_vbc_codec_remove(pdev);
	aud_ipc_ch_close(AMSG_CH_DSP_GET_PARAM_FROM_SMSG_NOREPLY);
	aud_ipc_ch_close(AMSG_CH_VBC_CTL);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vbc_of_match[] = {
	{.compatible = "sprd,sharkl5-vbc",},
	{.compatible = "sprd,roc1-vbc",},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_of_match);
#endif

static struct platform_driver vbc_driver = {
	.driver = {
		.name = "vbc-v4",
		.owner = THIS_MODULE,
		.of_match_table = vbc_of_match,
	},

	.probe = vbc_drv_probe,
	.remove = vbc_drv_remove,
};

static int __init sprd_vbc_driver_init(void)
{
	return platform_driver_register(&vbc_driver);
}

late_initcall(sprd_vbc_driver_init);

MODULE_DESCRIPTION("SPRD ASoC VBC CPU-DAI driver");
MODULE_AUTHOR("Jian chen <jian.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cpu-dai:vbc-v4");
