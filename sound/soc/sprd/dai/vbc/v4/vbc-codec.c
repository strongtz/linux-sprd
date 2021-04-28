/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-codec.c
 *
 * SPRD SoC VBC Codec -- SpreadTrum SOC VBC Codec function.
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

#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sound/core.h>
#include <sound/pcm_params.h>
#include <linux/pinctrl/consumer.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#define MDG_STP_MAX_VAL (0x1fff)
#define DG_MAX_VAL (0x7f)
#define SMTHDG_MAX_VAL (0xffff)
#define SMTHDG_STEP_MAX_VAL (0x1fff)
#define MIXERDG_MAX_VAL (0xffff)
#define MIXERDG_STP_MAX_VAL (0xffff)
#define OFFLOAD_DG_MAX (4096)
#define MAX_32_BIT (0xffffffff)
#define SRC_MAX_VAL (48000)

#define SPRD_VBC_ENUM(xreg, xmax, xtexts)\
	SOC_ENUM_SINGLE(xreg, 0, xmax, xtexts)

#undef sp_asoc_pr_dbg
#define sp_asoc_pr_dbg pr_info

static const char * const dsp_loopback_type_txt[] = {
	/* type 0, type 1, type 2 */
	"ADDA", "AD_ULDL_DA_PROCESS", "AD_UL_ENCODE_DECODE_DL_DA_PROCESS",

};

static const char * const enable_disable_txt[] = {
	"disable", "enable",
};

static const struct soc_enum dsp_loopback_enum  =
SPRD_VBC_ENUM(SND_SOC_NOPM, 3, dsp_loopback_type_txt);

static const struct soc_enum vbc_ag_iis_ext_sel_enum[AG_IIS_MAX] = {
	SPRD_VBC_ENUM(AG_IIS0, 2, enable_disable_txt),
	SPRD_VBC_ENUM(AG_IIS1, 2, enable_disable_txt),
	SPRD_VBC_ENUM(AG_IIS2, 2, enable_disable_txt),
};


static const struct soc_enum vbc_dump_enum =
SPRD_VBC_ENUM(SND_SOC_NOPM, 2, enable_disable_txt);

static const char * const sprd_profile_name[] = {
	"audio_structure", "dsp_vbc", "cvs", "dsp_smartamp",
};

const char *vbc_get_profile_name(int profile_id)
{
	return sprd_profile_name[profile_id];
}

/********************************************************************
 * KCONTROL get/put define
 ********************************************************************/

/* MDG */
static const char *vbc_mdg_id2name(int id)
{
	const char * const vbc_mdg_name[VBC_MDG_MAX] = {
		[VBC_MDG_DAC0_DSP] = TO_STRING(VBC_MDG_DAC0_DSP),
		[VBC_MDG_DAC1_DSP] = TO_STRING(VBC_MDG_DAC1_DSP),
		[VBC_MDG_AP01] = TO_STRING(VBC_MDG_AP01),
		[VBC_MDG_AP23] = TO_STRING(VBC_MDG_AP23),
	};

	if (id >= VBC_MDG_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mdg_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mdg_name[id];
}

static int vbc_mdg_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->mdg[id].mdg_mute;
	ucontrol->value.integer.value[1] = vbc_codec->mdg[id].mdg_step;

	return 0;
}

static int vbc_mdg_put(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val1, val2;
	int id = mc->shift;

	val1 = ucontrol->value.integer.value[0];
	val2 = ucontrol->value.integer.value[1];

	sp_asoc_pr_dbg("%s %s mute:%02d, step:%02d\n",
		       __func__, vbc_mdg_id2name(id), val1, val2);
	vbc_codec->mdg[id].mdg_id = id;
	vbc_codec->mdg[id].mdg_mute = val1;
	vbc_codec->mdg[id].mdg_step = val2;
	dsp_vbc_mdg_set(id, val1, val2);

	return 0;
}

/* SRC */
static const char *vbc_src_id2name(int id)
{
	const char * const vbc_src_name[VBC_SRC_MAX] = {
		[VBC_SRC_DAC0] = TO_STRING(VBC_SRC_DAC0),
		[VBC_SRC_DAC1] = TO_STRING(VBC_SRC_DAC1),
		[VBC_SRC_ADC0] = TO_STRING(VBC_SRC_ADC0),
		[VBC_SRC_ADC1] = TO_STRING(VBC_SRC_ADC1),
		[VBC_SRC_ADC2] = TO_STRING(VBC_SRC_ADC2),
		[VBC_SRC_ADC3] = TO_STRING(VBC_SRC_ADC3),
		[VBC_SRC_BT_DAC] = TO_STRING(VBC_SRC_BT_DAC),
		[VBC_SRC_BT_ADC] = TO_STRING(VBC_SRC_BT_ADC),
		[VBC_SRC_FM] = TO_STRING(VBC_SRC_FM),
	};

	if (id >= VBC_SRC_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_src_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_src_name[id];
}

static int vbc_src_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->src_fs[id];

	return 0;
}

/* @ucontrol->val: 48000, 8000 ...... */
static int vbc_src_put(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val1;
	int id = mc->shift;

	val1 = ucontrol->value.integer.value[0];
	sp_asoc_pr_dbg("%s %s, src_fs=%d\n", __func__,
		       vbc_src_id2name(id), val1);
	vbc_codec->src_fs[id] = val1;
	dsp_vbc_src_set(id, val1);

	return 0;
}

/* DG */
static const char *vbc_dg_id2name(int id)
{
	const char * const vbc_dg_name[VBC_DG_MAX] = {
		[VBC_DG_DAC0] = TO_STRING(VBC_DG_DAC0),
		[VBC_DG_DAC1] = TO_STRING(VBC_DG_DAC1),
		[VBC_DG_ADC0] = TO_STRING(VBC_DG_ADC0),
		[VBC_DG_ADC1] = TO_STRING(VBC_DG_ADC1),
		[VBC_DG_ADC2] = TO_STRING(VBC_DG_ADC2),
		[VBC_DG_ADC3] = TO_STRING(VBC_DG_ADC3),
		[VBC_DG_FM] = TO_STRING(VBC_DG_FM),
		[VBC_DG_ST] = TO_STRING(VBC_DG_ST),
		[OFFLOAD_DG] = TO_STRING(OFFLOAD_DG),
	};

	if (id >= VBC_DG_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_dg_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_dg_name[id];
}

static int vbc_dg_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->dg[id].dg_left;
	ucontrol->value.integer.value[1] = vbc_codec->dg[id].dg_right;

	return 0;
}

static int vbc_dg_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val1 = ucontrol->value.integer.value[0], val2 =
		ucontrol->value.integer.value[1];
	int id = mc->shift;

	if (id == OFFLOAD_DG &&
	    (val1 > OFFLOAD_DG_MAX || val2 > OFFLOAD_DG_MAX))
		return -EINVAL;

	vbc_codec->dg[id].dg_id = id;
	vbc_codec->dg[id].dg_left = val1;
	vbc_codec->dg[id].dg_right = val2;

	dsp_vbc_dg_set(id, val1, val2);
	sp_asoc_pr_dbg("%s %s l:%02d r:%02d\n",
		       __func__, vbc_dg_id2name(id), vbc_codec->dg[id].dg_left,
		       vbc_codec->dg[id].dg_right);

	return 0;
}

/* SMTHDG */
static const char *vbc_smthdg_id2name(int id)
{
	const char * const vbc_smthdg_name[VBC_SMTHDG_MAX] = {
		[VBC_SMTHDG_DAC0] = TO_STRING(VBC_SMTHDG_DAC0),
	};

	if (id >= VBC_SMTHDG_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_smthdg_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_smthdg_name[id];
}

static int vbc_smthdg_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->smthdg[id].smthdg_left;
	ucontrol->value.integer.value[1] = vbc_codec->smthdg[id].smthdg_right;

	return 0;
}

static int vbc_smthdg_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val1, val2;
	int id = mc->shift;

	val1 = ucontrol->value.integer.value[0];
	val2 = ucontrol->value.integer.value[1];
	sp_asoc_pr_dbg("%s %s l:%02d, r:%02d\n",
		       __func__, vbc_smthdg_id2name(id), val1, val2);
	vbc_codec->smthdg[id].smthdg_id = id;
	vbc_codec->smthdg[id].smthdg_left = val1;
	vbc_codec->smthdg[id].smthdg_right = val2;

	dsp_vbc_smthdg_set(id, val1, val2);

	return 0;
}

static int vbc_smthdg_step_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->smthdg_step[id].step;

	return 0;
}

static int vbc_smthdg_step_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val;
	int id = mc->shift;

	val = ucontrol->value.integer.value[0];
	sp_asoc_pr_dbg("%s %s %02d\n",
		       __func__, vbc_smthdg_id2name(id), val);
	vbc_codec->smthdg_step[id].smthdg_id = id;
	vbc_codec->smthdg_step[id].step = val;
	dsp_vbc_smthdg_step_set(id, val);

	return 0;
}

/* MIXERDG */
static const char *vbc_mixerdg_id2name(int id)
{
	const char * const vbc_mixerdg_name[VBC_MIXERDG_MAX] = {
		[VBC_MIXERDG_DAC0] = TO_STRING(VBC_MIXERDG_DAC0),
		[VBC_MIXERDG_DAC1] = TO_STRING(VBC_MIXERDG_DAC1),
	};

	if (id >= VBC_MIXERDG_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mixerdg_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mixerdg_name[id];
}

static int vbc_mixerdg_mainpath_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;

	ucontrol->value.integer.value[0] =
		vbc_codec->mixerdg[id].main_path.mixerdg_main_left;
	ucontrol->value.integer.value[1] =
		vbc_codec->mixerdg[id].main_path.mixerdg_main_right;

	return 0;
}

static int vbc_mixerdg_mainpath_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val1, val2;
	int id = mc->shift;

	val1 = ucontrol->value.integer.value[0];
	val2 = ucontrol->value.integer.value[1];
	sp_asoc_pr_dbg("%s %s: main_path l:%02d, r:%02d\n",
		       __func__, vbc_mixerdg_id2name(id), val1, val2);
	vbc_codec->mixerdg[id].main_path.mixerdg_id = id;
	vbc_codec->mixerdg[id].main_path.mixerdg_main_left = val1;
	vbc_codec->mixerdg[id].main_path.mixerdg_main_right = val2;
	dsp_vbc_mixerdg_mainpath_set(id, val1, val2);

	return 0;
}

static int vbc_mixerdg_mixpath_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;

	ucontrol->value.integer.value[0] =
		vbc_codec->mixerdg[id].mix_path.mixerdg_mix_left;
	ucontrol->value.integer.value[1] =
		vbc_codec->mixerdg[id].mix_path.mixerdg_mix_right;

	return 0;
}

static int vbc_mixerdg_mixpath_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	u32 val1, val2;
	int id = mc->shift;

	val1 = ucontrol->value.integer.value[0];
	val2 = ucontrol->value.integer.value[1];
	sp_asoc_pr_dbg("%s %s: mix_paht l:%02d, r:%02d\n",
		       __func__, vbc_mixerdg_id2name(id), val1, val2);
	vbc_codec->mixerdg[id].mix_path.mixerdg_id = id;
	vbc_codec->mixerdg[id].mix_path.mixerdg_mix_left = val1;
	vbc_codec->mixerdg[id].mix_path.mixerdg_mix_right = val2;
	dsp_vbc_mixerdg_mixpath_set(id, val1, val2);

	return 0;
}

static int vbc_mixerdg_step_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->mixerdg_step;

	return 0;
}

static int vbc_mixerdg_step_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int val;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	val = ucontrol->value.integer.value[0];
	sp_asoc_pr_dbg("%s set %02d\n", __func__, val);
	vbc_codec->mixerdg_step = val;
	dsp_vbc_mixerdg_step_set(val);

	return 0;
}

/* MIXER */
static const char *vbc_mixer_id2name(int id)
{
	const char * const vbc_mixer_name[VBC_MIXER_MAX] = {
		[VBC_MIXER0_DAC0] = TO_STRING(VBC_MIXER0_DAC0),
		[VBC_MIXER1_DAC0] = TO_STRING(VBC_MIXER1_DAC0),
		[VBC_MIXER0_DAC1] = TO_STRING(VBC_MIXER0_DAC1),
		[VBC_MIXER_ST] = TO_STRING(VBC_MIXER_ST),
		[VBC_MIXER_FM] = TO_STRING(VBC_MIXER_FM),
	};

	if (id >= VBC_MIXER_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mixer_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mixer_name[id];
}

static const char * const mixer_ops_type_txt[MIXER_OPS_TYPE_MAX] = {
	[NOT_MIX] = TO_STRING(NOT_MIX),
	[INTERCHANGE] = TO_STRING(INTERCHANGE),
	[HALF_ADD] = TO_STRING(HALF_ADD),
	[HALF_SUB] = TO_STRING(HALF_SUB),
	[DATA_INV] = TO_STRING(DATA_INV),
	[INTERCHANGE_INV] = TO_STRING(INTERCHANGE_INV),
	[HALF_ADD_INV] = TO_STRING(HALF_ADD_INV),
	[HALF_SUB_INV] = TO_STRING(HALF_SUB_INV),
};

static const struct soc_enum vbc_mixer_enum[VBC_MIXER_MAX] = {
	SPRD_VBC_ENUM(VBC_MIXER0_DAC0, MIXER_OPS_TYPE_MAX, mixer_ops_type_txt),
	SPRD_VBC_ENUM(VBC_MIXER1_DAC0, MIXER_OPS_TYPE_MAX, mixer_ops_type_txt),
	SPRD_VBC_ENUM(VBC_MIXER0_DAC1, MIXER_OPS_TYPE_MAX, mixer_ops_type_txt),
	SPRD_VBC_ENUM(VBC_MIXER_ST, MIXER_OPS_TYPE_MAX, mixer_ops_type_txt),
	SPRD_VBC_ENUM(VBC_MIXER_FM, MIXER_OPS_TYPE_MAX, mixer_ops_type_txt),
};

static int vbc_get_mixer_ops(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mixer[id].type;

	return 0;
}

static int vbc_put_mixer_ops(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s %s -> %s\n", __func__,
		       vbc_mixer_id2name(id), texts->texts[val]);

	vbc_codec->mixer[id].mixer_id = id;
	vbc_codec->mixer[id].type = val;
	dsp_vbc_mixer_set(id, vbc_codec->mixer[id].type);

	return 0;
}

/* MUX ADC_SOURCE */
static const char * const adc_source_sel_txt[ADC_SOURCE_VAL_MAX] = {
	[ADC_SOURCE_IIS] = TO_STRING(ADC_SOURCE_IIS),
	[ADC_SOURCE_VBCIF] = TO_STRING(ADC_SOURCE_VBCIF),
};

static const struct soc_enum
vbc_mux_adc_source_enum[VBC_MUX_ADC_SOURCE_MAX] = {
	[VBC_MUX_ADC0_SOURCE] = SPRD_VBC_ENUM(VBC_MUX_ADC0_SOURCE,
		ADC_SOURCE_VAL_MAX, adc_source_sel_txt),
	[VBC_MUX_ADC1_SOURCE] = SPRD_VBC_ENUM(VBC_MUX_ADC0_SOURCE,
		ADC_SOURCE_VAL_MAX, adc_source_sel_txt),
	[VBC_MUX_ADC2_SOURCE] = SPRD_VBC_ENUM(VBC_MUX_ADC0_SOURCE,
		ADC_SOURCE_VAL_MAX, adc_source_sel_txt),
	[VBC_MUX_ADC3_SOURCE] = SPRD_VBC_ENUM(VBC_MUX_ADC0_SOURCE,
		ADC_SOURCE_VAL_MAX, adc_source_sel_txt),
};

/* SND_KCTL_TYPE_MAIN_MIC_PATH_FROM */
static const char * const vbc_mainmic_path_val_txt[MAINMIC_FROM_MAX] = {
	[MAINMIC_FROM_LEFT] = TO_STRING(MAINMIC_FROM_LEFT),
	[MAINMIC_FROM_RIGHT] = TO_STRING(MAINMIC_FROM_RIGHT),
};

static const struct soc_enum
vbc_mainmic_path_enum[MAINMIC_USED_MAINMIC_TYPE_MAX] = {
	[MAINMIC_USED_DSP_NORMAL_ADC] = SPRD_VBC_ENUM(
		MAINMIC_USED_DSP_NORMAL_ADC, MAINMIC_FROM_MAX,
		vbc_mainmic_path_val_txt),
	[MAINMIC_USED_DSP_REF_ADC] = SPRD_VBC_ENUM(MAINMIC_USED_DSP_REF_ADC,
		MAINMIC_FROM_MAX, vbc_mainmic_path_val_txt),
};

static const char *vbc_mux_adc_source_id2name(int id)
{
	const char * const vbc_mux_adc_source_name[VBC_MUX_ADC_SOURCE_MAX] = {
		[VBC_MUX_ADC0_SOURCE] = TO_STRING(VBC_MUX_ADC0_SOURCE),
		[VBC_MUX_ADC1_SOURCE] = TO_STRING(VBC_MUX_ADC1_SOURCE),
		[VBC_MUX_ADC2_SOURCE] = TO_STRING(VBC_MUX_ADC2_SOURCE),
		[VBC_MUX_ADC3_SOURCE] = TO_STRING(VBC_MUX_ADC3_SOURCE),
	};

	if (id >= VBC_MUX_ADC_SOURCE_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_adc_source_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_adc_source_name[id];
}

static int vbc_mux_adc_source_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_adc_source[id].val;

	return 0;
}

static int vbc_mux_adc_source_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_adc_source_id2name(id), texts->texts[val]);
	vbc_codec->mux_adc_source[id].id = id;
	vbc_codec->mux_adc_source[id].val = val;

	dsp_vbc_mux_adc_source_set(id, val);

	return 1;
}

/* MUX DAC_OUT */
static const char * const dac_out_sel_txt[DAC_OUT_FORM_MAX] = {
	[DAC_OUT_FROM_IIS] = TO_STRING(DAC_OUT_FROM_IIS),
	[DAC_OUT_FROM_VBCIF] = TO_STRING(DAC_OUT_FROM_VBCIF),
};

static const struct soc_enum
vbc_mux_dac_out_enum[VBC_MUX_DAC_OUT_MAX] = {
	[VBC_MUX_DAC0_OUT_SEL] = SPRD_VBC_ENUM(VBC_MUX_DAC0_OUT_SEL,
		DAC_OUT_FORM_MAX, dac_out_sel_txt),
	[VBC_MUX_DAC1_OUT_SEL] = SPRD_VBC_ENUM(VBC_MUX_DAC1_OUT_SEL,
		DAC_OUT_FORM_MAX, dac_out_sel_txt),
};

static const char *vbc_mux_dac_out_id2name(int id)
{
	const char * const vbc_mux_dac_out_name[VBC_MUX_DAC_OUT_MAX] = {
		[VBC_MUX_DAC0_OUT_SEL] = TO_STRING(VBC_MUX_DAC0_OUT_SEL),
		[VBC_MUX_DAC1_OUT_SEL] = TO_STRING(VBC_MUX_DAC1_OUT_SEL),
	};

	if (id >= VBC_MUX_DAC_OUT_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_dac_out_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_dac_out_name[id];
}

static int vbc_mux_dac_out_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_dac_out[id].val;

	return 0;
}

static int vbc_mux_dac_out_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_dac_out_id2name(id), texts->texts[val]);
	vbc_codec->mux_dac_out[id].id = id;
	vbc_codec->mux_dac_out[id].val = val;
	dsp_vbc_mux_dac_out_set(id, val);

	return 1;
}

/* MUX ADC */
static const char * const mux_adc_sel_txt[ADC_IN_MAX] = {
	[ADC_IN_IIS0_ADC] = TO_STRING(ADC_IN_IIS0_ADC),
	[ADC_IN_IIS1_ADC] = TO_STRING(ADC_IN_IIS1_ADC),
	[ADC_IN_IIS2_ADC] = TO_STRING(ADC_IN_IIS2_ADC),
	[ADC_IN_IIS3_ADC] = TO_STRING(ADC_IN_IIS3_ADC),
	[ADC_IN_DAC0] = TO_STRING(ADC_IN_DAC0),
	[ADC_IN_DAC1] = TO_STRING(ADC_IN_DAC1),
	[ADC_IN_DAC_LOOP] = TO_STRING(ADC_IN_DAC_LOOP),
	[ADC_IN_TDM] = TO_STRING(ADC_IN_TDM),
};

static const struct soc_enum
vbc_mux_adc_enum[VBC_MUX_IN_ADC_ID_MAX] = {
	[VBC_MUX_IN_ADC0] = SPRD_VBC_ENUM(VBC_MUX_IN_ADC0,
					  ADC_IN_MAX, mux_adc_sel_txt),
	[VBC_MUX_IN_ADC1] = SPRD_VBC_ENUM(VBC_MUX_IN_ADC1,
					  ADC_IN_MAX, mux_adc_sel_txt),
	[VBC_MUX_IN_ADC2] = SPRD_VBC_ENUM(VBC_MUX_IN_ADC2,
					  ADC_IN_MAX, mux_adc_sel_txt),
	[VBC_MUX_IN_ADC3] = SPRD_VBC_ENUM(VBC_MUX_IN_ADC3,
					  ADC_IN_MAX, mux_adc_sel_txt),
};

static const char *vbc_mux_adc_id2name(int id)
{
	const char * const vbc_mux_adc_name[VBC_MUX_IN_ADC_ID_MAX] = {
		[VBC_MUX_IN_ADC0] = TO_STRING(VBC_MUX_IN_ADC0),
		[VBC_MUX_IN_ADC1] = TO_STRING(VBC_MUX_IN_ADC1),
		[VBC_MUX_IN_ADC2] = TO_STRING(VBC_MUX_IN_ADC2),
		[VBC_MUX_IN_ADC3] = TO_STRING(VBC_MUX_IN_ADC3),
	};

	if (id >= VBC_MUX_IN_ADC_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_adc_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_adc_name[id];
}

static int vbc_mux_adc_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_adc_in[id].val;

	return 0;
}

static int vbc_mux_adc_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		       __func__, vbc_mux_adc_id2name(id), texts->texts[val]);
	vbc_codec->mux_adc_in[id].id = id;
	vbc_codec->mux_adc_in[id].val = val;
	dsp_vbc_mux_adc_set(id, val);

	return 1;
}

/* MUX FM */
static const char * const mux_fm_sel_txt[FM_IN_VAL_MAX] = {
	[FM_IN_FM_SRC_OUT] = TO_STRING(FM_IN_FM_SRC_OUT),
	[FM_IN_VBC_IF_ADC0] = TO_STRING(FM_IN_VBC_IF_ADC0),
	[FM_IN_VBC_IF_ADC1] = TO_STRING(FM_IN_VBC_IF_ADC1),
	[FM_IN_VBC_IF_ADC2] = TO_STRING(FM_IN_VBC_IF_ADC2),
};

static const struct soc_enum
vbc_mux_fm_enum[VBC_FM_MUX_ID_MAX] = {
	[VBC_FM_MUX] = SPRD_VBC_ENUM(VBC_FM_MUX, FM_IN_VAL_MAX, mux_fm_sel_txt),
};

static const char *vbc_mux_fm_id2name(int id)
{
	const char * const vbc_mux_fm_name[VBC_FM_MUX_ID_MAX] = {
		[VBC_FM_MUX] = TO_STRING(VBC_FM_MUX),
	};

	if (id >= VBC_FM_MUX_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_fm_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_fm_name[id];
}

static int vbc_mux_fm_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_fm[id].val;

	return 0;
}

static int vbc_mux_fm_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		       __func__, vbc_mux_fm_id2name(id), texts->texts[val]);
	vbc_codec->mux_fm[id].id = id;
	vbc_codec->mux_fm[id].val = val;
	dsp_vbc_mux_fm_set(id, val);

	return 1;
}

/* MUX ST */
static const char * const mux_st_sel_txt[ST_IN_VAL_MAX] = {
	[ST_IN_ADC0] = TO_STRING(ST_IN_ADC0),
	[ST_IN_ADC0_DG] = TO_STRING(ST_IN_ADC0_DG),
	[ST_IN_ADC1] = TO_STRING(ST_IN_ADC1),
	[ST_IN_ADC1_DG] = TO_STRING(ST_IN_ADC1_DG),
	[ST_IN_ADC2] = TO_STRING(ST_IN_ADC2),
	[ST_IN_ADC2_DG] = TO_STRING(ST_IN_ADC2_DG),
	[ST_IN_ADC3] = TO_STRING(ST_IN_ADC3),
	[ST_IN_ADC3_DG] = TO_STRING(ST_IN_ADC3_DG),
};

static const struct soc_enum
vbc_mux_st_enum[VBC_ST_MUX_ID_MAX] = {
	[VBC_ST_MUX] = SPRD_VBC_ENUM(VBC_ST_MUX,
				     ST_IN_VAL_MAX, mux_st_sel_txt),
};

static const char *vbc_mux_st_id2name(int id)
{
	const char * const vbc_mux_st_name[VBC_ST_MUX_ID_MAX] = {
		[VBC_ST_MUX] = TO_STRING(VBC_ST_MUX),
	};

	if (id >= VBC_ST_MUX_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_st_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_st_name[id];
}

static int vbc_mux_st_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_st[id].val;

	return 0;
}

static int vbc_mux_st_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		       __func__, vbc_mux_st_id2name(id), texts->texts[val]);
	vbc_codec->mux_st[id].id = id;
	vbc_codec->mux_st[id].val = val;
	dsp_vbc_mux_st_set(id, val);

	return 1;
}

/* MUX LOOP_DA0 */
static const char * const mux_loop_da0_sel_txt[DAC0_LOOP_OUT_MAX] = {
	[DAC0_SMTHDG_OUT] = TO_STRING(DAC0_SMTHDG_OUT),
	[DAC0_MIX1_OUT] = TO_STRING(DAC0_MIX1_OUT),
	[DAC0_EQ4_OUT] = TO_STRING(DAC0_EQ4_OUT),
	[DAC0_MBDRC_OUT] = TO_STRING(DAC0_MBDRC_OUT),
};

static const struct soc_enum
vbc_mux_loop_da0_enum[VBC_MUX_LOOP_DAC0_MAX] = {
	[VBC_MUX_LOOP_DAC0] = SPRD_VBC_ENUM(VBC_MUX_LOOP_DAC0,
		DAC0_LOOP_OUT_MAX, mux_loop_da0_sel_txt),
};

static const char *vbc_mux_loop_da0_id2name(int id)
{
	const char * const vbc_mux_loop_da0_name[VBC_MUX_LOOP_DAC0_MAX] = {
		[VBC_MUX_LOOP_DAC0] = TO_STRING(VBC_MUX_LOOP_DAC0),
	};

	if (id >= VBC_MUX_LOOP_DAC0_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_loop_da0_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_loop_da0_name[id];
}

static int vbc_mux_loop_da0_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_loop_dac0[id].val;

	return 0;
}

static int vbc_mux_loop_da0_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_loop_da0_id2name(id), texts->texts[val]);
	vbc_codec->mux_loop_dac0[id].id = id;
	vbc_codec->mux_loop_dac0[id].val = val;
	dsp_vbc_mux_loop_da0_set(id, val);

	return 1;
}

/* MUX LOOP_DA1 */
static const char * const mux_loop_da1_txt[DA1_LOOP_OUT_MAX] = {
	[DAC1_MIXER_OUT] = TO_STRING(DAC1_MIXER_OUT),
	[DAC1_MIXERDG_OUT] = TO_STRING(DAC1_MIXERDG_OUT),
};

static const struct soc_enum
vbc_mux_loop_da1_enum[VBC_MUX_LOOP_DAC1_MAX] = {
	[VBC_MUX_LOOP_DAC1] = SPRD_VBC_ENUM(VBC_MUX_LOOP_DAC1,
					    DA1_LOOP_OUT_MAX, mux_loop_da1_txt),
};

static const char *vbc_mux_loop_da1_id2name(int id)
{
	const char * const vbc_mux_loop_da1_name[VBC_MUX_LOOP_DAC1_MAX] = {
		[VBC_MUX_LOOP_DAC1] = TO_STRING(VBC_MUX_LOOP_DAC1),
	};

	if (id >= VBC_MUX_LOOP_DAC1_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_loop_da1_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_loop_da1_name[id];
}

static int vbc_mux_loop_da1_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_loop_dac1[id].val;

	return 0;
}

static int vbc_mux_loop_da1_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_loop_da1_id2name(id), texts->texts[val]);
	vbc_codec->mux_loop_dac1[id].id = id;
	vbc_codec->mux_loop_dac1[id].val = val;
	dsp_vbc_mux_loop_da1_set(id, val);

	return 1;
}

/* MUX LOOP_DA0_DA1 */
static const char * const mux_loop_da0_da1_txt[DAC0_DAC1_SEL_MAX] = {
	[DAC0_DAC1_SEL_DAC1] = TO_STRING(DAC0_DAC1_SEL_DAC1),
	[DAC0_DAC1_SEL_DAC0] = TO_STRING(DAC0_DAC1_SEL_DAC0),
};

static const struct soc_enum
vbc_mux_loop_da0_da1_enum[VBC_MUX_LOOP_DAC0_DAC1_MAX] = {
	[VBC_MUX_LOOP_DAC0_DAC1] = SPRD_VBC_ENUM(VBC_MUX_LOOP_DAC0_DAC1,
		DAC0_DAC1_SEL_MAX, mux_loop_da0_da1_txt),
};

static const char *vbc_mux_loop_da0_da1_id2name(int id)
{
	const char * const
		vbc_mux_loop_da0_da1_name[VBC_MUX_LOOP_DAC0_DAC1_MAX] = {
		[VBC_MUX_LOOP_DAC0_DAC1] = TO_STRING(VBC_MUX_LOOP_DAC0_DAC1),
	};

	if (id >= VBC_MUX_LOOP_DAC0_DAC1_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_loop_da0_da1_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_loop_da0_da1_name[id];
}

static int vbc_mux_loop_da0_da1_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] =
		vbc_codec->mux_loop_dac0_dac1[id].val;

	return 0;
}

static int vbc_mux_loop_da0_da1_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_loop_da0_da1_id2name(id), texts->texts[val]);
	vbc_codec->mux_loop_dac0_dac1[id].id = id;
	vbc_codec->mux_loop_dac0_dac1[id].val = val;

	dsp_vbc_mux_loop_da0_da1_set(id, val);

	return 1;
}

/* MUX AUDRCD */
static const char * const mux_audrcd_txt[AUDRCD_ADC_IN_MAX] = {
	[AUDRCD_IN_ADC0] = TO_STRING(AUDRCD_IN_ADC0),
	[AUDRCD_IN_ADC1] = TO_STRING(AUDRCD_IN_ADC1),
	[AUDRCD_IN_ADC2] = TO_STRING(AUDRCD_IN_ADC2),
	[AUDRCD_IN_ADC3] = TO_STRING(AUDRCD_IN_ADC3),
};

static const struct soc_enum
vbc_mux_audrcd_enum[VBC_MUX_AUDRCD_ID_MAX] = {
	[VBC_MUX_AUDRCD01] = SPRD_VBC_ENUM(VBC_MUX_AUDRCD01,
					   AUDRCD_ADC_IN_MAX, mux_audrcd_txt),
	[VBC_MUX_AUDRCD23] = SPRD_VBC_ENUM(VBC_MUX_AUDRCD23,
					   AUDRCD_ADC_IN_MAX, mux_audrcd_txt),
};

static const char *vbc_mux_audrcd_id2name(int id)
{
	const char * const vbc_mux_audrcd_name[VBC_MUX_AUDRCD_ID_MAX] = {
		[VBC_MUX_AUDRCD01] = TO_STRING(VBC_MUX_AUDRCD01),
		[VBC_MUX_AUDRCD23] = TO_STRING(VBC_MUX_AUDRCD23),
	};

	if (id >= VBC_MUX_AUDRCD_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_audrcd_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_audrcd_name[id];
}

static int vbc_mux_audrcd_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_audrcd_in[id].val;

	return 0;
}

static int vbc_mux_audrcd_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		       __func__, vbc_mux_audrcd_id2name(id), texts->texts[val]);
	vbc_codec->mux_audrcd_in[id].id = id;
	vbc_codec->mux_audrcd_in[id].val = val;
	dsp_vbc_mux_audrcd_set(id, val);

	return 1;
}

/* MUX TDM_AUDRCD23 */
static const char * const mux_tdm_audrcd23_txt[AUDRCD23_TMD_SEL_MAX] = {
	[AUDRCD23_TDM_SEL_AUDRCD23] = TO_STRING(AUDRCD23_TDM_SEL_AUDRCD23),
	[AUDRCD23_TDM_SEL_TDM] = TO_STRING(AUDRCD23_TDM_SEL_TDM),
};

static const struct soc_enum
vbc_mux_tdm_audrcd23_enum[VBC_MUX_TDM_AUDRCD23_MAX] = {
	[VBC_MUX_TDM_AUDRCD23] = SPRD_VBC_ENUM(VBC_MUX_TDM_AUDRCD23,
		AUDRCD23_TMD_SEL_MAX, mux_tdm_audrcd23_txt),
};

static const char *vbc_mux_tdm_audrcd23_id2name(int id)
{
	const char * const
		vbc_mux_tdm_audrcd23_name[VBC_MUX_TDM_AUDRCD23_MAX] = {
		[VBC_MUX_TDM_AUDRCD23] = TO_STRING(VBC_MUX_TDM_AUDRCD23),
	};

	if (id >= VBC_MUX_TDM_AUDRCD23_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_tdm_audrcd23_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_tdm_audrcd23_name[id];
}

static int vbc_mux_tdm_audrcd23_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_tdm_audrcd23[id].val;

	return 0;
}

static int vbc_mux_tdm_audrcd23_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_tdm_audrcd23_id2name(id), texts->texts[val]);
	vbc_codec->mux_tdm_audrcd23[id].id = id;
	vbc_codec->mux_tdm_audrcd23[id].val = val;
	dsp_vbc_mux_tdm_audrcd23_set(id, val);

	return 1;
}

/* MUX AP01_DSP */
static const char * const mux_ap01_dsp_txt[AP01_TO_DSP_MAX] = {
	[AP01_TO_DSP_DISABLE] = TO_STRING(AP01_TO_DSP_DISABLE),
	[AP01_TO_DSP_ENABLE] = TO_STRING(AP01_TO_DSP_ENABLE),
};

static const struct soc_enum
vbc_mux_ap01_dsp_enum[VBC_MUX_AP01_DSP_ID_MAX] = {
	[VBC_MUX_AP01_DSP_PLY] = SPRD_VBC_ENUM(VBC_MUX_AP01_DSP_PLY,
		AP01_TO_DSP_MAX, mux_ap01_dsp_txt),
	[VBC_MUX_AP01_DSP_RCD] = SPRD_VBC_ENUM(VBC_MUX_AP01_DSP_RCD,
		AP01_TO_DSP_MAX, mux_ap01_dsp_txt),
};

static const char *vbc_mux_ap01_dsp_id2name(int id)
{
	const char * const vbc_mux_ap01_dsp_name[VBC_MUX_AP01_DSP_ID_MAX] = {
		[VBC_MUX_AP01_DSP_PLY] = TO_STRING(VBC_MUX_AP01_DSP_PLY),
		[VBC_MUX_AP01_DSP_RCD] = TO_STRING(VBC_MUX_AP01_DSP_RCD),
	};

	if (id >= VBC_MUX_AP01_DSP_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_ap01_dsp_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_ap01_dsp_name[id];
}

static int vbc_mux_ap01_dsp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_ap01_dsp[id].val;

	return 0;
}

static int vbc_mux_ap01_dsp_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_ap01_dsp_id2name(id), texts->texts[val]);
	vbc_codec->mux_ap01_dsp[id].id = id;
	vbc_codec->mux_ap01_dsp[id].val = val;
	dsp_vbc_mux_ap01_dsp_set(id, val);

	return 1;
}

/* MUX IIS_TX */
static const char * const mux_iis_tx_rx_txt[VBC_IIS_PORT_ID_MAX] = {
	[VBC_IIS_PORT_IIS0] = TO_STRING(VBC_IIS_PORT_IIS0),
	[VBC_IIS_PORT_IIS1] = TO_STRING(VBC_IIS_PORT_IIS1),
	[VBC_IIS_PORT_IIS2] = TO_STRING(VBC_IIS_PORT_IIS2),
	[VBC_IIS_PORT_IIS3] = TO_STRING(VBC_IIS_PORT_IIS3),
	[VBC_IIS_PORT_MST_IIS0] = TO_STRING(VBC_IIS_PORT_MST_IIS0),
};

static const struct soc_enum
vbc_mux_iis_tx_enum[VBC_MUX_IIS_TX_ID_MAX] = {
	[VBC_MUX_IIS_TX_DAC0] = SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC0,
			VBC_IIS_PORT_ID_MAX, mux_iis_tx_rx_txt),
	[VBC_MUX_IIS_TX_DAC1] = SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC1,
			VBC_IIS_PORT_ID_MAX, mux_iis_tx_rx_txt),
	[VBC_MUX_IIS_TX_DAC2] = SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC2,
			VBC_IIS_PORT_ID_MAX, mux_iis_tx_rx_txt),
};

static const char *vbc_mux_iis_tx_id2name(int id)
{
	const char * const vbc_mux_iis_tx_name[VBC_MUX_IIS_TX_ID_MAX] = {
		[VBC_MUX_IIS_TX_DAC0] = TO_STRING(VBC_MUX_IIS_TX_DAC0),
		[VBC_MUX_IIS_TX_DAC1] = TO_STRING(VBC_MUX_IIS_TX_DAC1),
		[VBC_MUX_IIS_TX_DAC2] = TO_STRING(VBC_MUX_IIS_TX_DAC2),
	};

	if (id >= VBC_MUX_IIS_TX_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_iis_tx_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_iis_tx_name[id];
}

static int vbc_mux_iis_tx_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_iis_tx[id].val;

	return 0;
}

static int vbc_mux_iis_tx_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		       __func__, vbc_mux_iis_tx_id2name(id), texts->texts[val]);
	vbc_codec->mux_iis_tx[id].id = id;
	vbc_codec->mux_iis_tx[id].val = val;
	dsp_vbc_mux_iis_tx_set(id, val);

	return 1;
}

/* MUX IIS_RX */
static const struct soc_enum
vbc_mux_iis_rx_enum[VBC_MUX_IIS_RX_ID_MAX] = {
	[VBC_MUX_IIS_RX_ADC0] = SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC0,
		VBC_IIS_PORT_ID_MAX, mux_iis_tx_rx_txt),
	[VBC_MUX_IIS_RX_ADC1] = SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC1,
		VBC_IIS_PORT_ID_MAX, mux_iis_tx_rx_txt),
	[VBC_MUX_IIS_RX_ADC2] = SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC2,
		VBC_IIS_PORT_ID_MAX, mux_iis_tx_rx_txt),
	[VBC_MUX_IIS_RX_ADC3] = SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC3,
		VBC_IIS_PORT_ID_MAX, mux_iis_tx_rx_txt),
};

static const char *vbc_mux_iis_rx_id2name(int id)
{
	const char * const vbc_mux_iis_rx_name[VBC_MUX_IIS_RX_ID_MAX] = {
		[VBC_MUX_IIS_RX_ADC0] = TO_STRING(VBC_MUX_IIS_RX_ADC0),
		[VBC_MUX_IIS_RX_ADC1] = TO_STRING(VBC_MUX_IIS_RX_ADC1),
		[VBC_MUX_IIS_RX_ADC2] = TO_STRING(VBC_MUX_IIS_RX_ADC2),
		[VBC_MUX_IIS_RX_ADC3] = TO_STRING(VBC_MUX_IIS_RX_ADC3),
	};

	if (id >= VBC_MUX_IIS_RX_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_iis_rx_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_iis_rx_name[id];
}

static int vbc_mux_iis_rx_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_iis_rx[id].val;

	return 0;
}

static int vbc_mux_iis_rx_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		       __func__, vbc_mux_iis_rx_id2name(id), texts->texts[val]);
	vbc_codec->mux_iis_rx[id].id = id;
	vbc_codec->mux_iis_rx[id].val = val;
	dsp_vbc_mux_iis_rx_set(id, val);

	return 1;
}

/* MUX IIS_PORT_DO */
static const char * const mux_iis_port_do_txt[IIS_DO_VAL_MAX] = {
	[IIS_DO_VAL_DAC0] = TO_STRING(IIS_DO_VAL_DAC0),
	[IIS_DO_VAL_DAC1] = TO_STRING(IIS_DO_VAL_DAC1),
	[IIS_DO_VAL_DAC2] = TO_STRING(IIS_DO_VAL_DAC2),
};

static const struct soc_enum
vbc_mux_iis_port_do_enum[VBC_IIS_PORT_ID_MAX] = {
	[VBC_IIS_PORT_IIS0] = SPRD_VBC_ENUM(VBC_IIS_PORT_IIS0,
				IIS_DO_VAL_MAX, mux_iis_port_do_txt),
	[VBC_IIS_PORT_IIS1] = SPRD_VBC_ENUM(VBC_IIS_PORT_IIS1,
				IIS_DO_VAL_MAX, mux_iis_port_do_txt),
	[VBC_IIS_PORT_IIS2] = SPRD_VBC_ENUM(VBC_IIS_PORT_IIS2,
				IIS_DO_VAL_MAX, mux_iis_port_do_txt),
	[VBC_IIS_PORT_IIS3] = SPRD_VBC_ENUM(VBC_IIS_PORT_IIS3,
				IIS_DO_VAL_MAX, mux_iis_port_do_txt),
	[VBC_IIS_PORT_MST_IIS0] = SPRD_VBC_ENUM(VBC_IIS_PORT_MST_IIS0,
				IIS_DO_VAL_MAX, mux_iis_port_do_txt),
};

static const char *vbc_mux_iis_port_id2name(int id)
{
	const char * const vbc_mux_iis_port_name[VBC_IIS_PORT_ID_MAX] = {
		[VBC_IIS_PORT_IIS0] = TO_STRING(VBC_IIS_PORT_IIS0),
		[VBC_IIS_PORT_IIS1] = TO_STRING(VBC_IIS_PORT_IIS1),
		[VBC_IIS_PORT_IIS2] = TO_STRING(VBC_IIS_PORT_IIS2),
		[VBC_IIS_PORT_IIS3] = TO_STRING(VBC_IIS_PORT_IIS3),
		[VBC_IIS_PORT_MST_IIS0] = TO_STRING(VBC_IIS_PORT_MST_IIS0),
	};

	if (id >= VBC_IIS_PORT_ID_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_mux_iis_port_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_mux_iis_port_name[id];
}

static int vbc_mux_iis_port_do_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->mux_iis_port_do[id].val;

	return 0;
}

static int vbc_mux_iis_port_do_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, %s to %s\n",
		__func__, vbc_mux_iis_port_id2name(id), texts->texts[val]);
	vbc_codec->mux_iis_port_do[id].id = id;
	vbc_codec->mux_iis_port_do[id].val = val;
	dsp_vbc_mux_iis_port_do_set(id, val);

	return 1;
}

/* ADDER */
static const char *vbc_adder_id2name(int id)
{
	const char * const vbc_adder_name[VBC_ADDER_MAX] = {
		[VBC_ADDER_OFLD] = TO_STRING(VBC_ADDER_OFLD),
		[VBC_ADDER_FM_DAC0] = TO_STRING(VBC_ADDER_FM_DAC0),
		[VBC_ADDER_ST_DAC0] = TO_STRING(VBC_ADDER_ST_DAC0),
	};

	if (id >= VBC_ADDER_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_adder_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_adder_name[id];
}

static const char * const adder_mode_txt[ADDER_MOD_MAX] = {
	[ADDER_MOD_IGNORE] = TO_STRING(ADDER_MOD_IGNORE),
	[ADDER_MOD_ADD] = TO_STRING(ADDER_MOD_ADD),
	[ADDER_MOD_MINUS] = TO_STRING(ADDER_MOD_MINUS),
};

static const struct soc_enum
vbc_adder_enum[VBC_ADDER_MAX] = {
	[VBC_ADDER_OFLD] =
		SPRD_VBC_ENUM(VBC_ADDER_OFLD, ADDER_MOD_MAX, adder_mode_txt),
	[VBC_ADDER_FM_DAC0] =
		SPRD_VBC_ENUM(VBC_ADDER_FM_DAC0, ADDER_MOD_MAX, adder_mode_txt),
	[VBC_ADDER_ST_DAC0] =
		SPRD_VBC_ENUM(VBC_ADDER_ST_DAC0, ADDER_MOD_MAX, adder_mode_txt),
};

static int vbc_adder_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] =
		vbc_codec->vbc_adder[id].adder_mode_l;

	return 0;
}

static int vbc_adder_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	val = ucontrol->value.integer.value[0];
	sp_asoc_pr_dbg("%s %s to %s\n",
		       __func__, vbc_adder_id2name(id), texts->texts[val]);
	vbc_codec->vbc_adder[id].adder_id = id;
	vbc_codec->vbc_adder[id].adder_mode_l = val;
	vbc_codec->vbc_adder[id].adder_mode_r = val;
	dsp_vbc_adder_set(id, val, val);

	return 1;

}

/* DATA_PATH  */
static const char *vbc_datapath_id2name(int id)
{
	const char * const vbc_datapath_name[VBC_DP_EN_MAX] = {
		[VBC_DAC0_DP_EN] = TO_STRING(VBC_DAC0_DP_EN),
		[VBC_DAC1_DP_EN] = TO_STRING(VBC_DAC1_DP_EN),
		[VBC_DAC2_DP_EN] = TO_STRING(VBC_DAC2_DP_EN),
		[VBC_ADC0_DP_EN] = TO_STRING(VBC_ADC0_DP_EN),
		[VBC_ADC1_DP_EN] = TO_STRING(VBC_ADC1_DP_EN),
		[VBC_ADC2_DP_EN] = TO_STRING(VBC_ADC2_DP_EN),
		[VBC_ADC3_DP_EN] = TO_STRING(VBC_ADC3_DP_EN),
		[VBC_OFLD_DP_EN] = TO_STRING(VBC_OFLD_DP_EN),
		[VBC_FM_DP_EN] = TO_STRING(VBC_FM_DP_EN),
		[VBC_ST_DP_EN] = TO_STRING(VBC_ST_DP_EN),
	};

	if (id >= VBC_DP_EN_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_datapath_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_datapath_name[id];
}

static const struct soc_enum
vbc_datapath_enum[VBC_DP_EN_MAX] = {
	[VBC_DAC0_DP_EN] = SPRD_VBC_ENUM(VBC_DAC0_DP_EN, 2, enable_disable_txt),
	[VBC_DAC1_DP_EN] = SPRD_VBC_ENUM(VBC_DAC1_DP_EN, 2, enable_disable_txt),
	[VBC_DAC2_DP_EN] = SPRD_VBC_ENUM(VBC_DAC2_DP_EN, 2, enable_disable_txt),
	[VBC_ADC0_DP_EN] = SPRD_VBC_ENUM(VBC_ADC0_DP_EN, 2, enable_disable_txt),
	[VBC_ADC1_DP_EN] = SPRD_VBC_ENUM(VBC_ADC1_DP_EN, 2, enable_disable_txt),
	[VBC_ADC2_DP_EN] = SPRD_VBC_ENUM(VBC_ADC2_DP_EN, 2, enable_disable_txt),
	[VBC_ADC3_DP_EN] = SPRD_VBC_ENUM(VBC_ADC3_DP_EN, 2, enable_disable_txt),
	[VBC_OFLD_DP_EN] = SPRD_VBC_ENUM(VBC_OFLD_DP_EN, 2, enable_disable_txt),
	[VBC_FM_DP_EN] = SPRD_VBC_ENUM(VBC_FM_DP_EN, 2, enable_disable_txt),
	[VBC_ST_DP_EN] = SPRD_VBC_ENUM(VBC_ST_DP_EN, 2, enable_disable_txt),
};

static int vbc_dp_en_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->vbc_dp_en[id].enable;

	return 0;
}

static int vbc_dp_en_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	u16 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, %s = %s\n",
		       __func__, vbc_datapath_id2name(id), texts->texts[value]);
	vbc_codec->vbc_dp_en[id].id = id;
	vbc_codec->vbc_dp_en[id].enable = value;
	dsp_vbc_dp_en_set(id, value);

	return 1;
}

/* CALL_MUTE */
static const char *vbc_callmute_id2name(int id)
{
	const char * const vbc_callmute_name[VBC_MUTE_MAX] = {
		[VBC_UL_MUTE] = TO_STRING(VBC_UL_MUTE),
		[VBC_DL_MUTE] = TO_STRING(VBC_DL_MUTE),
	};

	if (id >= VBC_MUTE_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_callmute_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_callmute_name[id];
}

static const struct soc_enum
vbc_call_mute_enum[VBC_MUTE_MAX] = {
	SPRD_VBC_ENUM(VBC_UL_MUTE, 2, enable_disable_txt),
	SPRD_VBC_ENUM(VBC_DL_MUTE, 2, enable_disable_txt),
};

static int vbc_call_mute_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	u16 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, %s %s\n",
		       __func__, vbc_callmute_id2name(id), texts->texts[value]);
	vbc_codec->vbc_call_mute[id].id = id;
	vbc_codec->vbc_call_mute[id].mute = value;
	dsp_call_mute_set(id, value);

	return true;
}

/* IIS_TX_WIDTH */
static const char * const vbc_iis_width_txt[IIS_WD_MAX] = {
	[WD_16BIT] = TO_STRING(WD_16BIT),
	[WD_24BIT] = TO_STRING(WD_24BIT),
};

static const struct soc_enum vbc_iis_tx_wd_enum[VBC_MUX_IIS_TX_ID_MAX] = {
	SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC0, IIS_WD_MAX, vbc_iis_width_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC1, IIS_WD_MAX, vbc_iis_width_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC2, IIS_WD_MAX, vbc_iis_width_txt),
};

static int vbc_get_iis_tx_width_sel(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->iis_tx_wd[id].value;

	return 0;
}

static int vbc_put_iis_tx_width_sel(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, %s=%s\n",
		__func__, vbc_mux_iis_tx_id2name(id), texts->texts[value]);
	vbc_codec->iis_tx_wd[id].id = id;
	vbc_codec->iis_tx_wd[id].value = value;
	dsp_vbc_iis_tx_width_set(id, value);

	return 1;
}

/* IIS_TX_LR_MOD */
static const char * const vbc_iis_lr_mod_txt[LR_MOD_MAX] = {
	[LEFT_HIGH] = TO_STRING(LEFT_HIGH),
	[RIGHT_HIGH] = TO_STRING(RIGHT_HIGH),
};

static const struct soc_enum vbc_iis_tx_lr_mod_enum[VBC_MUX_IIS_TX_ID_MAX] = {
	SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC0, IIS_WD_MAX, vbc_iis_lr_mod_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC1, IIS_WD_MAX, vbc_iis_lr_mod_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_TX_DAC2, IIS_WD_MAX, vbc_iis_lr_mod_txt),
};

static int vbc_get_iis_tx_lr_mod_sel(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->iis_tx_lr_mod[id].value;

	return 0;
}

static int vbc_put_iis_tx_lr_mod_sel(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, %s = %s\n",
		__func__, vbc_mux_iis_tx_id2name(id), texts->texts[value]);
	vbc_codec->iis_tx_lr_mod[id].id = id;
	vbc_codec->iis_tx_lr_mod[id].value = value;
	dsp_vbc_iis_tx_lr_mod_set(id, value);

	return 1;
}

/* IIS_RX_WIDTH */
static const struct soc_enum vbc_iis_rx_wd_enum[VBC_MUX_IIS_RX_ID_MAX] = {
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC0, IIS_WD_MAX, vbc_iis_width_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC1, IIS_WD_MAX, vbc_iis_width_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC2, IIS_WD_MAX, vbc_iis_width_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC3, IIS_WD_MAX, vbc_iis_width_txt),
};

static int vbc_get_iis_rx_width_sel(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->iis_rx_wd[id].value;

	return 0;
}

static int vbc_put_iis_rx_width_sel(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, %s=%s\n",
		__func__, vbc_mux_iis_rx_id2name(id), texts->texts[value]);
	vbc_codec->iis_rx_wd[id].id = id;
	vbc_codec->iis_rx_wd[id].value = value;
	dsp_vbc_iis_rx_width_set(id, value);

	return 1;
}

/* IIS_RX_LR_MOD */
static const struct soc_enum vbc_iis_rx_lr_mod_enum[VBC_MUX_IIS_RX_ID_MAX] = {
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC0, IIS_WD_MAX, vbc_iis_lr_mod_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC1, IIS_WD_MAX, vbc_iis_lr_mod_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC2, IIS_WD_MAX, vbc_iis_lr_mod_txt),
	SPRD_VBC_ENUM(VBC_MUX_IIS_RX_ADC3, IIS_WD_MAX, vbc_iis_lr_mod_txt),
};

static int vbc_get_iis_rx_lr_mod_sel(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->iis_rx_lr_mod[id].value;

	return 0;
}

static int vbc_put_iis_rx_lr_mod_sel(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, %s = %s\n",
		__func__, vbc_mux_iis_rx_id2name(id), texts->texts[value]);
	vbc_codec->iis_rx_lr_mod[id].id = id;
	vbc_codec->iis_rx_lr_mod[id].value = value;
	dsp_vbc_iis_rx_lr_mod_set(id, value);

	return 1;
}

static const char * const vbc_iis_mst_sel_txt[VBC_MASTER_TYPE_MAX] = {
	[VBC_MASTER_EXTERNAL] = TO_STRING(VBC_MASTER_EXTERNAL),
	[VBC_MASTER_INTERNAL] = TO_STRING(VBC_MASTER_INTERNAL),
};

static const struct soc_enum vbc_iis_mst_sel_enum[IIS_MST_SEL_ID_MAX] = {
	SPRD_VBC_ENUM(IIS_MST_SEL_0, VBC_MASTER_TYPE_MAX, vbc_iis_mst_sel_txt),
	SPRD_VBC_ENUM(IIS_MST_SEL_1, VBC_MASTER_TYPE_MAX, vbc_iis_mst_sel_txt),
	SPRD_VBC_ENUM(IIS_MST_SEL_2, VBC_MASTER_TYPE_MAX, vbc_iis_mst_sel_txt),
	SPRD_VBC_ENUM(IIS_MST_SEL_3, VBC_MASTER_TYPE_MAX, vbc_iis_mst_sel_txt),
};

static int vbc_get_mst_sel_type(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mst_sel_para[id].mst_type;

	return 0;
}

static int vbc_put_mst_sel_type(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	if (ucontrol->value.enumerated.item[0] >= texts->items) {
		pr_err("mst_sel_type, index outof bounds error\n");
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	vbc_codec->mst_sel_para[id].id = id;
	vbc_codec->mst_sel_para[id].mst_type = value;
	sp_asoc_pr_dbg("mst_sel_type id %d, value %d\n", id, value);
	dsp_vbc_mst_sel_type_set(id, value);

	return 0;
}

/* IIS MASTER */
static const struct soc_enum
vbc_iis_master_enum = SPRD_VBC_ENUM(SND_SOC_NOPM, 2, enable_disable_txt);

static int vbc_get_iis_master_en(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->iis_master.enable;

	return 0;
}

static int vbc_put_iis_master_en(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, %s = %s\n",
		       __func__, "vbc_iis_master", texts->texts[value]);

	vbc_codec->iis_master.enable = value;
	dsp_vbc_iis_master_start(value);

	return 1;
}

static const struct soc_enum vbc_iis_master_wd_width_enum =
	SPRD_VBC_ENUM(SND_SOC_NOPM, 2, vbc_iis_width_txt);

static int vbc_get_iis_master_width(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->iis_mst_width;

	return 0;
}

static int vbc_put_iis_master_width(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("set iis master wd width to %s\n", texts->texts[value]);

	vbc_codec->iis_mst_width = value;
	dsp_vbc_iis_master_width_set(value);

	return 0;
}

static int vbc_mainmic_path_sel_val_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] =
		vbc_codec->mainmic_from[id].main_mic_from;

	return 0;
}

static int vbc_mainmic_path_sel_val_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_dbg("%s, type %d is %s\n",
		__func__, id, texts->texts[val]);

	vbc_codec->mainmic_from[id].type = id;
	vbc_codec->mainmic_from[id].main_mic_from = val;


	dsp_vbc_mux_adc_source_set(id, val);

	return 1;
}

static const struct soc_enum
ivsence_enum = SPRD_VBC_ENUM(SND_SOC_NOPM, 2, enable_disable_txt);

static int vbc_get_ivsence_func(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->is_use_ivs_smtpa;

	return 0;
}

static int vbc_put_ivsence_func(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	u32 enable;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	int iv_adc_id;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	enable = ucontrol->value.enumerated.item[0];
	pr_info("%s, %s = %s\n",
		__func__, "ivsence func dsp", texts->texts[enable]);
	vbc_codec->is_use_ivs_smtpa = enable;
	iv_adc_id = get_ivsense_adc_id();
	dsp_ivsence_func(enable, iv_adc_id);

	return 1;
}

static int vbc_profile_set(struct snd_soc_codec *codec, void *data,
			   int profile_type, int mode)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_profile *p_profile_setting = &vbc_codec->vbc_profile_setting;
	int ret;

	ret = aud_send_block_param(AMSG_CH_VBC_CTL, mode, -1,
		SND_VBC_DSP_IO_SHAREMEM_SET, profile_type, data,
		p_profile_setting->hdr[profile_type].len_mode,
				   AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		return -EIO;

	return 0;
}

static void vbc_profile_try_apply(struct snd_soc_codec *codec,
				  int profile_id)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_profile *p_profile_setting = &vbc_codec->vbc_profile_setting;
	void *data;
	u32 mode_offset;

	sp_asoc_pr_dbg("%s, profile_id:%d\n",
		       __func__, profile_id);
	mode_offset =
		(p_profile_setting->now_mode[profile_id] >> 24) & 0xff;
	mutex_lock(&vbc_codec->load_mutex);
	/* get profile data wanted */
	data = (void *)((u8 *)(p_profile_setting->data[profile_id])
			+ p_profile_setting->hdr[profile_id].len_mode *
			mode_offset);
	sp_asoc_pr_dbg("now_mode[%d]=%d,mode=%u, mode_offset=%u\n",
		       profile_id, p_profile_setting->now_mode[profile_id],
		       (p_profile_setting->now_mode[profile_id] >> 16) & 0xff,
		       mode_offset);
	/* update the para*/
	vbc_profile_set(codec, data, profile_id,
			p_profile_setting->now_mode[profile_id]);
	mutex_unlock(&vbc_codec->load_mutex);
}

static int audio_load_firmware_data(struct firmware *fw, char *firmware_path)
{
	int read_len, size, cnt = 0;
	char *buf;
	char *audio_image_buffer;
	int image_size;
	loff_t pos = 0;
	struct file *file;

	if (!firmware_path)
		return -EINVAL;
	pr_info("%s entry, path %s\n", __func__, firmware_path);
	file = filp_open(firmware_path, O_RDONLY, 0);
	if (IS_ERR(file)) {
		pr_err("%s open file %s error, file =%p\n",
		       firmware_path, __func__, file);
		return PTR_ERR(file);
	}
	pr_info("audio %s open image file %s  successfully\n",
		__func__, firmware_path);

	/* read file to buffer */
	image_size = i_size_read(file_inode(file));
	if (image_size <= 0) {
		filp_close(file, NULL);
		pr_err("read size failed");
		return -EINVAL;
	}
	audio_image_buffer = vmalloc(image_size);
	if (!audio_image_buffer) {
		filp_close(file, NULL);
		pr_err("%s no memory\n", __func__);
		return -ENOMEM;
	}
	memset(audio_image_buffer, 0, image_size);
	pr_info("audio_image_buffer=%px\n", audio_image_buffer);
	size = image_size;
	buf = audio_image_buffer;
	do {
		read_len = kernel_read(file, buf, size, &pos);
		if (read_len > 0) {
			size -= read_len;
			buf += read_len;
		} else if (read_len == -EINTR || read_len == -EAGAIN) {
			cnt++;
			pr_warn("%s, read failed,read_len=%d, cnt=%d\n",
				__func__, read_len, cnt);
			if (cnt < 3) {
				msleep(50);
				continue;
			}
		}
	} while (read_len > 0 && size > 0);
	filp_close(file, NULL);
	fw->data = audio_image_buffer;
	fw->size = image_size;
	pr_info("After read, audio_image_buffer=%px, size=%zd, pos:%zd, read_len:%d, finish.\n",
		fw->data, fw->size, (size_t)pos, read_len);

	return 0;
}

static void audio_release_firmware_data(struct firmware *fw)
{
	if (fw->data) {
		vfree(fw->data);
		memset(fw, 0, sizeof(*fw));
		pr_info("%s\n", __func__);
	}
}

#define AUDIO_FIRMWARE_PATH_BASE "/vendor/firmware/"

int vbc_profile_loading(struct snd_soc_codec *codec, int profile_id)
{
	int ret;
	const u8 *fw_data;
	struct firmware fw;
	int offset;
	int len;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_profile *p_profile_setting = &vbc_codec->vbc_profile_setting;

	sp_asoc_pr_dbg("%s %s\n", __func__, vbc_get_profile_name(profile_id));
	mutex_lock(&vbc_codec->load_mutex);
	p_profile_setting->is_loading[profile_id] = 1;

	/* request firmware for AUDIO profile */
	memset(vbc_codec->firmware_path, 0, sizeof(vbc_codec->firmware_path));
	strcpy(vbc_codec->firmware_path, AUDIO_FIRMWARE_PATH_BASE);
	strcat(vbc_codec->firmware_path, vbc_get_profile_name(profile_id));
	ret = audio_load_firmware_data(&fw, &vbc_codec->firmware_path[0]);


	fw_data = fw.data;
	unalign_memcpy(&p_profile_setting->hdr[profile_id], fw_data,
		       sizeof(p_profile_setting->hdr[profile_id]));
	sp_asoc_pr_dbg("&p_profile_setting->hdr[profile_id(%d)]",
		       profile_id);
	sp_asoc_pr_dbg(" =%#lx,phys=%#lx\n",
		       (unsigned long)&p_profile_setting->hdr[profile_id],
		       (unsigned long)
		       virt_to_phys(&p_profile_setting->hdr[profile_id]));
	if (strncmp
	    (p_profile_setting->hdr[profile_id].magic,
	     VBC_PROFILE_FIRMWARE_MAGIC_ID,
	     VBC_PROFILE_FIRMWARE_MAGIC_LEN)) {
		pr_err("ERR:Firmware %s magic error!\n",
		       vbc_get_profile_name(profile_id));
		ret = -EINVAL;
		goto profile_out;
	}

	offset = sizeof(struct vbc_fw_header);
	len = p_profile_setting->hdr[profile_id].num_mode *
		p_profile_setting->hdr[profile_id].len_mode;
	if (p_profile_setting->data[profile_id] == NULL) {
		p_profile_setting->data[profile_id] = kzalloc(len, GFP_KERNEL);
		if (p_profile_setting->data[profile_id] == NULL) {
			ret = -ENOMEM;
			goto profile_out;
		}
	}
	unalign_memcpy(p_profile_setting->data[profile_id],
		       fw_data + offset, len);
	sp_asoc_pr_dbg("p_profile_setting->data[profile_id (%d)]",
		       profile_id);

	sp_asoc_pr_dbg(" =%#lx,phys=%#lx\n",
		       (unsigned long)p_profile_setting->data[profile_id],
		       (unsigned long)
		       virt_to_phys(p_profile_setting->data[profile_id]));
	ret = 0;
	goto profile_out;

profile_out:

	audio_release_firmware_data(&fw);
	mutex_unlock(&vbc_codec->load_mutex);
	sp_asoc_pr_info("%s, return %i\n", __func__, ret);

	return ret;
}

static int vbc_profile_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_profile *p_profile_setting =
		&vbc_codec->vbc_profile_setting;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int profile_idx = mc->reg - SND_VBC_PROFILE_START;

	ucontrol->value.integer.value[0] =
		p_profile_setting->now_mode[profile_idx];

	return 0;
}

static int vbc_profile_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	u32 ret;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_profile *p_profile_setting = &vbc_codec->vbc_profile_setting;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int profile_idx = mc->reg - SND_VBC_PROFILE_START;
	u32 mode_max_offset;
	u32 current_offset;

	mode_max_offset = p_profile_setting->hdr[profile_idx].num_mode;

	ret = ucontrol->value.integer.value[0];
	current_offset = ((ret >> 24) & 0xff);
	sp_asoc_pr_dbg("%s %s, value=%#x,",
		       __func__, vbc_get_profile_name(profile_idx), ret);
	sp_asoc_pr_dbg(" current_offset=%d, mode_max_offset=%d\n",
		       current_offset, mode_max_offset);
	/*
	 * value of now_mode:
	 * 16bit, 8bit , 8bit
	 * ((offset<<24)|(param_id<<16)|dsp_case)
	 * offset : bit 24-31
	 * param_id: 16-23
	 * dsp_case: 0-15
	 */
	if (current_offset < mode_max_offset)
		p_profile_setting->now_mode[profile_idx] = ret;

	if (p_profile_setting->data[profile_idx])
		vbc_profile_try_apply(codec, profile_idx);

	return ret;
}

static int vbc_profile_load_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_profile *p_profile_setting = &vbc_codec->vbc_profile_setting;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int profile_idx = mc->reg - SND_VBC_PROFILE_START;

	ucontrol->value.integer.value[0] =
		p_profile_setting->is_loading[profile_idx];

	return 0;
}

static int vbc_profile_load_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int profile_idx = mc->reg - SND_VBC_PROFILE_START;

	ret = ucontrol->value.integer.value[0];

	sp_asoc_pr_dbg("%s %s, %s\n",
		       __func__, vbc_get_profile_name(profile_idx),
		       (ret == 1) ? "load" : "idle");
	if (ret == 1)
		ret = vbc_profile_loading(codec, profile_idx);

	return ret;
}

static int vbc_volume_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->volume;

	return 0;
}

static int vbc_volume_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	value = ucontrol->value.integer.value[0];
	sp_asoc_pr_dbg("%s volume = %d\n",
		       __func__, value);
	vbc_codec->volume = value;
	dsp_vbc_set_volume(vbc_codec->volume);

	return value;
}

static int vbc_reg_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->dsp_reg;

	return 0;
}

static int vbc_reg_put(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	u32 reg;
	u32 value;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	reg = mc->reg;
	value = ucontrol->value.integer.value[0];
	pr_debug("%s %#x(reg) = %#x(value)\n",
		 __func__, reg, value);
	vbc_codec->dsp_reg = value;

	dsp_vbc_reg_write(reg, value, 0xffffffff);

	return value;
}

static int vbc_get_aud_iis_clock(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->aud_iis0_master_setting;

	return 0;
}

static const char * const iis_master_setting_txt[] = {
	"disable_iis0", "disable_loop", "iis0", "loop",
};

static const struct soc_enum iis_master_setting_enum  =
SPRD_VBC_ENUM(SND_SOC_NOPM, 4, iis_master_setting_txt);

static int vbc_put_aud_iis_clock(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	u32 value;
	int ret;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	if (!vbc_codec->need_aud_top_clk) {
		pr_debug("%s No need audio top to provide da clock.\n",
			 __func__);
		return 0;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, texts->texts[%d] = '%s', texts->items=%d\n",
		       __func__, value, texts->texts[value], texts->items);
	if (value >= texts->items) {
		pr_err("err: %s value(%u) >= items(%u)\n",
		       __func__, value, texts->items);
		return -1;
	}

	ret = aud_dig_iis_master(codec->component.card,
				 value);
	if (ret < 0) {
		pr_err("%s failed. value: %u ret = %d\n", __func__, value, ret);
		return -1;
	}
	vbc_codec->aud_iis0_master_setting = value;

	return value;
}

static int vbc_loopback_loop_mode_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->loopback.loop_mode;

	return 0;
}

static int vbc_loopback_loop_mode_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.integer.value[0];
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	vbc_codec->loopback.loop_mode = value;
	dsp_vbc_loopback_set(&vbc_codec->loopback);

	return value;
}

static int vbc_loopback_type_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->loopback.loopback_type;

	return 0;
}

static int vbc_loopback_type_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int value;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, texts->texts[%d] =%s\n",
		       __func__, value, texts->texts[value]);
	vbc_codec->loopback.loopback_type = value;
	dsp_vbc_loopback_set(&vbc_codec->loopback);

	return value;
}

static int vbc_loopback_voice_fmt_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->loopback.voice_fmt;

	return 0;
}

static int vbc_loopback_voice_fmt_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	value = ucontrol->value.integer.value[0];
	sp_asoc_pr_dbg("%s, value=%d\n", __func__, value);
	vbc_codec->loopback.voice_fmt = value;
	dsp_vbc_loopback_set(&vbc_codec->loopback);

	return value;
}

static int vbc_loopback_amr_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->loopback.amr_rate;

	return 0;
}

static int vbc_loopback_amr_rate_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	int value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, value=%d\n", __func__, value);
	vbc_codec->loopback.amr_rate = value;
	dsp_vbc_loopback_set(&vbc_codec->loopback);

	return value;
}

static int vbc_call_mute_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->vbc_call_mute[id].mute;

	return 0;
}

static int sys_iis_sel_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 id = e->reg;

	ucontrol->value.integer.value[0] = vbc_codec->sys_iis_sel[id];

	return 0;
}

static const char * const sys_iis_sel_txt[] = {
	"vbc_iis0", "vbc_iis1", "vbc_iis2", "vbc_iis3", "vbc_iism0",
};

static const struct soc_enum
vbc_sys_iis_enum[SYS_IIS_MAX] = {
	SPRD_VBC_ENUM(SYS_IIS0, 5, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS1, 5, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS2, 5, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS3, 5, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS4, 5, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS5, 5, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS6, 5, sys_iis_sel_txt),
};

static int sys_iis_sel_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	u16 value;
	int ret;
	struct pinctrl_state *state;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	char buf[128] = {0};
	u32 id = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, id=%d,value=%d, texts->texts[] =%s\n",
		       __func__, id, value, texts->texts[value]);
	vbc_codec->sys_iis_sel[id] = value;
	sprintf(buf, "%s_%u", sys_iis_sel_txt[value], id);
	state = pinctrl_lookup_state(vbc_codec->pctrl, buf);
	if (IS_ERR(state)) {
		pr_err("%s line=%d failed\n", __func__, __LINE__);
		return -EINVAL;
	}
	ret =  pinctrl_select_state(vbc_codec->pctrl, state);
	if (ret != 0)
		pr_err("%s failed ret = %d\n", __func__, ret);

	sp_asoc_pr_dbg("%s,soc iis%d -> %s\n", __func__, id, buf);

	return true;
}

static int vbc_get_ag_iis_ext_sel(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 ag_iis_num = e->reg;

	ucontrol->value.integer.value[0] =
		vbc_codec->ag_iis_ext_sel[ag_iis_num];

	return 0;
}

static int vbc_put_ag_iis_ext_sel(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	u16 enable;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 ag_iis_num = e->reg;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	enable = ucontrol->value.enumerated.item[0];
	sp_asoc_pr_dbg("%s, ag_iis_num=%d,value=%d, texts->texts[] =%s\n",
		       __func__, ag_iis_num, enable, texts->texts[enable]);
	arch_audio_iis_to_audio_top_enable(ag_iis_num, enable);

	vbc_codec->ag_iis_ext_sel[ag_iis_num] = enable;

	return true;
}

static int vbc_get_agdsp_access(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u16 enable;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	enable = ucontrol->value.enumerated.item[0];
	mutex_lock(&vbc_codec->agcp_access_mutex);
	ucontrol->value.integer.value[0] = vbc_codec->agcp_access_enable;
	mutex_unlock(&vbc_codec->agcp_access_mutex);

	return true;
}

static int vbc_put_agdsp_aud_access(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	u16 enable;
	int ret = true;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	enable = ucontrol->value.enumerated.item[0];

	pr_info("%s agcp_access_aud_cnt = %d, agcp_access_a2dp_cnt = %d\n",
		__func__, vbc_codec->agcp_access_aud_cnt,
		vbc_codec->agcp_access_a2dp_cnt);
	mutex_lock(&vbc_codec->agcp_access_mutex);
	if (enable) {
		if (vbc_codec->agcp_access_aud_cnt == 0 &&
		    vbc_codec->agcp_access_a2dp_cnt == 0) {
			vbc_codec->agcp_access_aud_cnt++;
			ret = agdsp_access_enable();
			if (ret)
				pr_err("agdsp_access_enable error:%d\n", ret);
			else
				vbc_codec->agcp_access_enable = 1;
		}
	} else {
		if (vbc_codec->agcp_access_aud_cnt != 0) {
			vbc_codec->agcp_access_aud_cnt = 0;
			if (vbc_codec->agcp_access_a2dp_cnt == 0) {
				pr_info("audio hal agdsp_access_disable\n");
				agdsp_access_disable();
				vbc_codec->agcp_access_enable = 0;
			}
		}
	}
	mutex_unlock(&vbc_codec->agcp_access_mutex);

	return ret;
}

static int vbc_put_agdsp_a2dp_access(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	u16 enable;
	int ret = true;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	enable = ucontrol->value.enumerated.item[0];

	pr_info("%s agcp_access_aud_cnt = %d, agcp_access_a2dp_cnt = %d\n",
		__func__, vbc_codec->agcp_access_aud_cnt,
		vbc_codec->agcp_access_a2dp_cnt);
	mutex_lock(&vbc_codec->agcp_access_mutex);
	if (enable) {
		if (vbc_codec->agcp_access_a2dp_cnt == 0 &&
		    vbc_codec->agcp_access_aud_cnt == 0) {
			vbc_codec->agcp_access_a2dp_cnt++;
			ret = agdsp_access_enable();
			if (ret)
				pr_err("agdsp_access_enable error:%d\n", ret);
			else
				vbc_codec->agcp_access_enable = 1;
		}
	} else {
		if (vbc_codec->agcp_access_a2dp_cnt != 0) {
			vbc_codec->agcp_access_a2dp_cnt = 0;
			if (vbc_codec->agcp_access_aud_cnt == 0) {
				pr_info("audio hal agdsp_access_disable\n");
				agdsp_access_disable();
				vbc_codec->agcp_access_enable = 0;
			}
		}
	}
	mutex_unlock(&vbc_codec->agcp_access_mutex);

	return ret;
}

int sbc_paras_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	int len;
	int size;

	size = sizeof(struct sbcenc_param_t);
	len = params->max * sizeof(ucontrol->value.bytes.data[0]);
	if (size > len) {
		pr_err("%s size > len\n", __func__);
		return -EINVAL;
	}
	memcpy(ucontrol->value.bytes.data, &vbc_codec->sbcenc_para, size);

	return 0;
}

/* not send to dsp, dsp only use at startup */
int sbc_paras_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	int len;
	int size;

	size = sizeof(struct sbcenc_param_t);
	len = params->max * sizeof(ucontrol->value.bytes.data[0]);
	if (size > len) {
		pr_err("%s size > len\n", __func__);
		return -EINVAL;
	}
	memcpy(&vbc_codec->sbcenc_para, ucontrol->value.bytes.data, size);
	pr_info("%s sbc para %u, %u, %u, %u, %u, %u, %u\n",
		__func__, vbc_codec->sbcenc_para.SBCENC_Mode,
		vbc_codec->sbcenc_para.SBCENC_Blocks,
		vbc_codec->sbcenc_para.SBCENC_SubBands,
		vbc_codec->sbcenc_para.SBCENC_SamplingFreq,
		vbc_codec->sbcenc_para.SBCENC_AllocMethod,
		vbc_codec->sbcenc_para.SBCENC_min_Bitpool,
		vbc_codec->sbcenc_para.SBCENC_max_Bitpool);

	return 0;
}

static const char * const vbc_dump_pos_txt[DUMP_POS_MAX] = {
	[DUMP_POS_DAC0_E] = TO_STRING(DUMP_POS_DAC0_E),
	[DUMP_POS_DAC1_E] = TO_STRING(DUMP_POS_DAC1_E),
	[DUMP_POS_A4] = TO_STRING(DUMP_POS_A4),
	[DUMP_POS_A3] = TO_STRING(DUMP_POS_A3),
	[DUMP_POS_A2] = TO_STRING(DUMP_POS_A2),
	[DUMP_POS_A1] = TO_STRING(DUMP_POS_A1),
	[DUMP_POS_V2] = TO_STRING(DUMP_POS_V2),
	[DUMP_POS_V1] = TO_STRING(DUMP_POS_V1),
};

static const struct soc_enum vbc_dump_pos_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, DUMP_POS_MAX,
	vbc_dump_pos_txt);

static const char *vbc_dumppos2name(int pos)
{
	const char * const vbc_dumppos_name[DUMP_POS_MAX] = {
		[DUMP_POS_DAC0_E] = TO_STRING(DUMP_POS_DAC0_E),
		[DUMP_POS_DAC1_E] = TO_STRING(DUMP_POS_DAC1_E),
		[DUMP_POS_A4] = TO_STRING(DUMP_POS_A4),
		[DUMP_POS_A3] = TO_STRING(DUMP_POS_A3),
		[DUMP_POS_A2] = TO_STRING(DUMP_POS_A2),
		[DUMP_POS_A1] = TO_STRING(DUMP_POS_A1),
		[DUMP_POS_V2] = TO_STRING(DUMP_POS_V2),
		[DUMP_POS_V1] = TO_STRING(DUMP_POS_V1),
	};

	if (pos >= DUMP_POS_MAX) {
		pr_err("invalid id %s %d\n", __func__, pos);
		return "";
	}
	if (!vbc_dumppos_name[pos]) {
		pr_err("null string =%d\n", pos);
		return "";
	}

	return vbc_dumppos_name[pos];
}

static int vbc_get_dump_pos(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_dump_position;

	return 0;
}

static int vbc_put_dump_pos(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	int val;

	val = ucontrol->value.integer.value[0];
	if (val >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s %s -> %s\n", __func__,
		       vbc_dumppos2name(val), texts->texts[val]);
	vbc_codec->vbc_dump_position = val;

	return 0;
}

/* VBC IIS PINMUX FOR USB */
static const char * const vbc_iis_inf_sys_sel_txt[] = {
	"vbc_iis_to_pad", "vbc_iis_to_aon_usb",
};

static const struct soc_enum
vbc_iis_inf_sys_sel_enum = SPRD_VBC_ENUM(SND_SOC_NOPM, 2,
	vbc_iis_inf_sys_sel_txt);

static int vbc_iis_inf_sys_sel_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_iis_inf_sys_sel;

	return 0;
}

/*
 * vbc iis2 to inf sys iis2, you can extension it by soc_enum->reg.
 */
static int vbc_iis_inf_sys_sel_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	u16 value;
	int ret;
	struct pinctrl_state *state;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	char buf[128] = {0};

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	value = ucontrol->value.enumerated.item[0];
	pr_info("%s, value=%d (%s)\n",
		__func__, value, texts->texts[value]);
	vbc_codec->vbc_iis_inf_sys_sel = value;
	sprintf(buf, "%s", vbc_iis_inf_sys_sel_txt[value]);
	state = pinctrl_lookup_state(vbc_codec->pctrl, buf);
	if (IS_ERR(state)) {
		pr_err("%s lookup pin control failed\n", __func__);
		return -EINVAL;
	}
	ret =  pinctrl_select_state(vbc_codec->pctrl, state);
	if (ret != 0)
		pr_err("%s pin contrl select failed %d\n", __func__, ret);


	return 0;
}



/* -9450dB to 0dB in 150dB steps ( mute instead of -9450dB) */
static const DECLARE_TLV_DB_SCALE(mdg_tlv, -9450, 150, 1);
static const DECLARE_TLV_DB_SCALE(dg_tlv, -9450, 150, 1);
static const DECLARE_TLV_DB_SCALE(smthdg_tlv, -9450, 150, 1);
static const DECLARE_TLV_DB_SCALE(smthdg_step_tlv, -9450, 150, 1);
static const DECLARE_TLV_DB_SCALE(mixerdg_tlv, -9450, 150, 1);
static const DECLARE_TLV_DB_SCALE(mixerdg_step_tlv, -9450, 150, 1);
static const DECLARE_TLV_DB_SCALE(offload_dg_tlv, 0, 150, 1);

static const struct snd_kcontrol_new vbc_codec_snd_controls[] = {
	/* MDG */
	SOC_DOUBLE_R_EXT_TLV("VBC DAC0 DSP MDG Set",
			     0, 1, VBC_MDG_DAC0_DSP, MDG_STP_MAX_VAL, 0,
			     vbc_mdg_get,
			     vbc_mdg_put, mdg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC DAC1 DSP MDG Set",
			     0, 1, VBC_MDG_DAC1_DSP, MDG_STP_MAX_VAL, 0,
			     vbc_mdg_get,
			     vbc_mdg_put, mdg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC DAC0 AUD MDG Set",
			     0, 1, VBC_MDG_AP01, MDG_STP_MAX_VAL, 0,
			     vbc_mdg_get,
			     vbc_mdg_put, mdg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC DAC0 AUD23 MDG Set",
			     0, 1, VBC_MDG_AP23, MDG_STP_MAX_VAL, 0,
			     vbc_mdg_get,
			     vbc_mdg_put, mdg_tlv),
	/* SRC */
	SOC_SINGLE_EXT("VBC_SRC_DAC0", SND_SOC_NOPM,
		       VBC_SRC_DAC0, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_DAC1", SND_SOC_NOPM,
		       VBC_SRC_DAC1, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_ADC0", SND_SOC_NOPM,
		       VBC_SRC_ADC0, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_ADC1", SND_SOC_NOPM,
		       VBC_SRC_ADC1, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_ADC2", SND_SOC_NOPM,
		       VBC_SRC_ADC2, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_ADC3", SND_SOC_NOPM,
		       VBC_SRC_ADC3, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_BT_DAC", SND_SOC_NOPM,
		       VBC_SRC_BT_DAC, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_BT_ADC", SND_SOC_NOPM,
		       VBC_SRC_BT_ADC, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	SOC_SINGLE_EXT("VBC_SRC_FM", SND_SOC_NOPM,
		       VBC_SRC_FM, SRC_MAX_VAL, 0,
		       vbc_src_get, vbc_src_put),
	/* DG */
	SOC_DOUBLE_R_EXT_TLV("VBC DAC0 DG Set",
			     0, 1, VBC_DG_DAC0, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC DAC1 DG Set",
			     0, 1, VBC_DG_DAC1, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC ADC0 DG Set",
			     0, 1, VBC_DG_ADC0, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC ADC1 DG Set",
			     0, 1, VBC_DG_ADC1, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC ADC2 DG Set",
			     0, 1, VBC_DG_ADC2, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC ADC3 DG Set",
			     0, 1, VBC_DG_ADC3, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC FM DG Set",
			     0, 1, VBC_DG_FM, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC ST DG Set",
			     0, 1, VBC_DG_ST, DG_MAX_VAL, 0,
			     vbc_dg_get,
			     vbc_dg_put, dg_tlv),
	SOC_DOUBLE_R_EXT_TLV("OFFLOAD DG Set",
			     0, 1, OFFLOAD_DG, OFFLOAD_DG_MAX, 0,
			     vbc_dg_get,
			     vbc_dg_put, offload_dg_tlv),
	/* SMTHDG */
	SOC_DOUBLE_R_EXT_TLV("VBC_SMTHDG_DAC0",
			     0, 1, VBC_SMTHDG_DAC0, SMTHDG_MAX_VAL, 0,
			     vbc_smthdg_get,
			     vbc_smthdg_put, smthdg_tlv),
	SOC_SINGLE_EXT_TLV("VBC_SMTHDG_DAC0_STEP",
			   0, VBC_SMTHDG_DAC0, SMTHDG_STEP_MAX_VAL, 0,
			   vbc_smthdg_step_get,
			   vbc_smthdg_step_put, smthdg_step_tlv),
	/* MIXERDG */
	SOC_DOUBLE_R_EXT_TLV("VBC_MIXERDG_DAC0_MAIN",
			     0, 1, VBC_MIXERDG_DAC0, MIXERDG_MAX_VAL, 0,
			     vbc_mixerdg_mainpath_get,
			     vbc_mixerdg_mainpath_put, mixerdg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC_MIXERDG_DAC0_MIX",
			     0, 1, VBC_MIXERDG_DAC0, MIXERDG_MAX_VAL, 0,
			     vbc_mixerdg_mixpath_get,
			     vbc_mixerdg_mixpath_put, mixerdg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC_MIXERDG_DAC1_MAIN",
			     0, 1, VBC_MIXERDG_DAC1, MIXERDG_MAX_VAL, 0,
			     vbc_mixerdg_mainpath_get,
			     vbc_mixerdg_mainpath_put, mixerdg_tlv),
	SOC_DOUBLE_R_EXT_TLV("VBC_MIXERDG_DAC1_MIX",
			     0, 1, VBC_MIXERDG_DAC1, MIXERDG_MAX_VAL, 0,
			     vbc_mixerdg_mixpath_get,
			     vbc_mixerdg_mixpath_put, mixerdg_tlv),
	SOC_SINGLE_EXT_TLV("VBC_MIXERDG_STEP",
			   SND_SOC_NOPM,
			   0, MIXERDG_STP_MAX_VAL, 0,
			   vbc_mixerdg_step_get,
			   vbc_mixerdg_step_put, mixerdg_step_tlv),
	/* MIXER */
	SOC_ENUM_EXT("VBC_MIXER0_DAC0",
		     vbc_mixer_enum[VBC_MIXER0_DAC0],
		     vbc_get_mixer_ops,
		     vbc_put_mixer_ops),
	SOC_ENUM_EXT("VBC_MIXER1_DAC0",
		     vbc_mixer_enum[VBC_MIXER1_DAC0],
		     vbc_get_mixer_ops,
		     vbc_put_mixer_ops),
	SOC_ENUM_EXT("VBC_MIXER0_DAC1",
		     vbc_mixer_enum[VBC_MIXER0_DAC1],
		     vbc_get_mixer_ops,
		     vbc_put_mixer_ops),
	SOC_ENUM_EXT("VBC_MIXER_ST",
		     vbc_mixer_enum[VBC_MIXER_ST],
		     vbc_get_mixer_ops,
		     vbc_put_mixer_ops),
	SOC_ENUM_EXT("VBC_MIXER_FM",
		     vbc_mixer_enum[VBC_MIXER_FM],
		     vbc_get_mixer_ops,
		     vbc_put_mixer_ops),
	/* MUX */
	SOC_ENUM_EXT("VBC_MUX_ADC0_SOURCE",
		     vbc_mux_adc_source_enum[VBC_MUX_ADC0_SOURCE],
		     vbc_mux_adc_source_get, vbc_mux_adc_source_put),
	SOC_ENUM_EXT("VBC_MUX_ADC1_SOURCE",
		     vbc_mux_adc_source_enum[VBC_MUX_ADC1_SOURCE],
		     vbc_mux_adc_source_get, vbc_mux_adc_source_put),
	SOC_ENUM_EXT("VBC_MUX_ADC2_SOURCE",
		     vbc_mux_adc_source_enum[VBC_MUX_ADC2_SOURCE],
		     vbc_mux_adc_source_get, vbc_mux_adc_source_put),
	SOC_ENUM_EXT("VBC_MUX_ADC3_SOURCE",
		     vbc_mux_adc_source_enum[VBC_MUX_ADC3_SOURCE],
		     vbc_mux_adc_source_get, vbc_mux_adc_source_put),
	SOC_ENUM_EXT("VBC_MUX_DAC0_OUT_SEL",
		     vbc_mux_dac_out_enum[VBC_MUX_DAC0_OUT_SEL],
		     vbc_mux_dac_out_get, vbc_mux_dac_out_put),
	SOC_ENUM_EXT("VBC_MUX_DAC1_OUT_SEL",
		     vbc_mux_dac_out_enum[VBC_MUX_DAC1_OUT_SEL],
		     vbc_mux_dac_out_get, vbc_mux_dac_out_put),
	SOC_ENUM_EXT("VBC_MUX_ADC0",
		     vbc_mux_adc_enum[VBC_MUX_IN_ADC0],
		     vbc_mux_adc_get, vbc_mux_adc_put),
	SOC_ENUM_EXT("VBC_MUX_ADC1",
		     vbc_mux_adc_enum[VBC_MUX_IN_ADC1],
		     vbc_mux_adc_get, vbc_mux_adc_put),
	SOC_ENUM_EXT("VBC_MUX_ADC2",
		     vbc_mux_adc_enum[VBC_MUX_IN_ADC2],
		     vbc_mux_adc_get, vbc_mux_adc_put),
	SOC_ENUM_EXT("VBC_MUX_ADC3",
		     vbc_mux_adc_enum[VBC_MUX_IN_ADC3],
		     vbc_mux_adc_get, vbc_mux_adc_put),
	SOC_ENUM_EXT("VBC_MUX_FM",
		     vbc_mux_fm_enum[VBC_FM_MUX],
		     vbc_mux_fm_get, vbc_mux_fm_put),
	SOC_ENUM_EXT("VBC_MUX_ST",
		     vbc_mux_st_enum[VBC_ST_MUX],
		     vbc_mux_st_get, vbc_mux_st_put),
	SOC_ENUM_EXT("VBC_MUX_LOOP_DAC0",
		     vbc_mux_loop_da0_enum[VBC_MUX_LOOP_DAC0],
		     vbc_mux_loop_da0_get, vbc_mux_loop_da0_put),
	SOC_ENUM_EXT("VBC_MUX_LOOP_DAC1",
		     vbc_mux_loop_da1_enum[VBC_MUX_LOOP_DAC1],
		     vbc_mux_loop_da1_get, vbc_mux_loop_da1_put),
	SOC_ENUM_EXT("VBC_MUX_LOOP_DAC0_DAC1",
		     vbc_mux_loop_da0_da1_enum[VBC_MUX_LOOP_DAC0_DAC1],
		     vbc_mux_loop_da0_da1_get, vbc_mux_loop_da0_da1_put),
	SOC_ENUM_EXT("VBC_MUX_AUDRCD01",
		     vbc_mux_audrcd_enum[VBC_MUX_AUDRCD01],
		     vbc_mux_audrcd_get, vbc_mux_audrcd_put),
	SOC_ENUM_EXT("VBC_MUX_AUDRCD23",
		     vbc_mux_audrcd_enum[VBC_MUX_AUDRCD23],
		     vbc_mux_audrcd_get, vbc_mux_audrcd_put),
	SOC_ENUM_EXT("VBC_MUX_TDM_AUDRCD23",
		     vbc_mux_tdm_audrcd23_enum[VBC_MUX_TDM_AUDRCD23],
		     vbc_mux_tdm_audrcd23_get, vbc_mux_tdm_audrcd23_put),
	SOC_ENUM_EXT("VBC_MUX_AP01_DSP_PLY",
		     vbc_mux_ap01_dsp_enum[VBC_MUX_AP01_DSP_PLY],
		     vbc_mux_ap01_dsp_get, vbc_mux_ap01_dsp_put),
	SOC_ENUM_EXT("VBC_MUX_AP01_DSP_RCD",
		     vbc_mux_ap01_dsp_enum[VBC_MUX_AP01_DSP_RCD],
		     vbc_mux_ap01_dsp_get, vbc_mux_ap01_dsp_put),
	SOC_ENUM_EXT("VBC_MUX_DAC0_IIS_PORT_SEL",
		     vbc_mux_iis_tx_enum[VBC_MUX_IIS_TX_DAC0],
		     vbc_mux_iis_tx_get, vbc_mux_iis_tx_put),
	SOC_ENUM_EXT("VBC_MUX_DAC1_IIS_PORT_SEL",
		     vbc_mux_iis_tx_enum[VBC_MUX_IIS_TX_DAC1],
		     vbc_mux_iis_tx_get, vbc_mux_iis_tx_put),
	SOC_ENUM_EXT("VBC_MUX_DAC2_IIS_PORT_SEL",
		     vbc_mux_iis_tx_enum[VBC_MUX_IIS_TX_DAC2],
		     vbc_mux_iis_tx_get, vbc_mux_iis_tx_put),
	SOC_ENUM_EXT("VBC_MUX_ADC0_IIS_PORT_SEL",
		     vbc_mux_iis_rx_enum[VBC_MUX_IIS_RX_ADC0],
		     vbc_mux_iis_rx_get, vbc_mux_iis_rx_put),
	SOC_ENUM_EXT("VBC_MUX_ADC1_IIS_PORT_SEL",
		     vbc_mux_iis_rx_enum[VBC_MUX_IIS_RX_ADC1],
		     vbc_mux_iis_rx_get, vbc_mux_iis_rx_put),
	SOC_ENUM_EXT("VBC_MUX_ADC2_IIS_PORT_SEL",
		     vbc_mux_iis_rx_enum[VBC_MUX_IIS_RX_ADC2],
		     vbc_mux_iis_rx_get, vbc_mux_iis_rx_put),
	SOC_ENUM_EXT("VBC_MUX_ADC3_IIS_PORT_SEL",
		     vbc_mux_iis_rx_enum[VBC_MUX_IIS_RX_ADC3],
		     vbc_mux_iis_rx_get, vbc_mux_iis_rx_put),
	SOC_ENUM_EXT("VBC_MUX_IIS0_PORT_DO_SEL",
		     vbc_mux_iis_port_do_enum[VBC_IIS_PORT_IIS0],
		     vbc_mux_iis_port_do_get, vbc_mux_iis_port_do_put),
	SOC_ENUM_EXT("VBC_MUX_IIS1_PORT_DO_SEL",
		     vbc_mux_iis_port_do_enum[VBC_IIS_PORT_IIS1],
		     vbc_mux_iis_port_do_get, vbc_mux_iis_port_do_put),
	SOC_ENUM_EXT("VBC_MUX_IIS2_PORT_DO_SEL",
		     vbc_mux_iis_port_do_enum[VBC_IIS_PORT_IIS2],
		     vbc_mux_iis_port_do_get, vbc_mux_iis_port_do_put),
	SOC_ENUM_EXT("VBC_MUX_IIS3_PORT_DO_SEL",
		     vbc_mux_iis_port_do_enum[VBC_IIS_PORT_IIS3],
		     vbc_mux_iis_port_do_get, vbc_mux_iis_port_do_put),
	SOC_ENUM_EXT("VBC_MUX_MST_IIS0_PORT_DO_SEL",
		     vbc_mux_iis_port_do_enum[VBC_IIS_PORT_MST_IIS0],
		     vbc_mux_iis_port_do_get, vbc_mux_iis_port_do_put),
	/* ADDER */
	SOC_ENUM_EXT("VBC_ADDER_OFLD",
		     vbc_adder_enum[VBC_ADDER_OFLD],
		     vbc_adder_get, vbc_adder_put),
	SOC_ENUM_EXT("VBC_ADDER_FM_DAC0",
		     vbc_adder_enum[VBC_ADDER_FM_DAC0],
		     vbc_adder_get, vbc_adder_put),
	SOC_ENUM_EXT("VBC_ADDER_ST_DAC0",
		     vbc_adder_enum[VBC_ADDER_ST_DAC0],
		     vbc_adder_get, vbc_adder_put),
	/* LOOPBACK */
	SOC_SINGLE_EXT("VBC_DSP_LOOPBACK_ARM_RATE",
		       SND_SOC_NOPM, 0,
		       MAX_32_BIT, 0,
		       vbc_loopback_amr_rate_get,
		       vbc_loopback_amr_rate_put),
	SOC_SINGLE_EXT("VBC_DSP_LOOPBACK_VOICE_FMT",
		       SND_SOC_NOPM, 0,
		       MAX_32_BIT, 0,
		       vbc_loopback_voice_fmt_get,
		       vbc_loopback_voice_fmt_put),
	SOC_SINGLE_EXT("VBC_DSP_LOOPBACK_LOOP_MODE", SND_SOC_NOPM, 0,
		      MAX_32_BIT, 0,
		      vbc_loopback_loop_mode_get,
		      vbc_loopback_loop_mode_put),
	SOC_ENUM_EXT("VBC_DSP_LOOPBACK_TYPE",
		     dsp_loopback_enum,
		     vbc_loopback_type_get, vbc_loopback_type_put),
	/* DATAPATH */
	SOC_ENUM_EXT("VBC_DAC0_DP_EN",
		     vbc_datapath_enum[VBC_DAC0_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_DAC1_DP_EN",
		     vbc_datapath_enum[VBC_DAC1_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_DAC2_DP_EN",
		     vbc_datapath_enum[VBC_DAC2_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_ADC0_DP_EN",
		     vbc_datapath_enum[VBC_ADC0_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_ADC1_DP_EN",
		     vbc_datapath_enum[VBC_ADC1_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_ADC2_DP_EN",
		     vbc_datapath_enum[VBC_ADC2_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_ADC3_DP_EN",
		     vbc_datapath_enum[VBC_ADC3_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_OFLD_DP_EN",
		     vbc_datapath_enum[VBC_OFLD_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_FM_DP_EN",
		     vbc_datapath_enum[VBC_FM_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	SOC_ENUM_EXT("VBC_ST_DP_EN",
		     vbc_datapath_enum[VBC_ST_DP_EN],
		     vbc_dp_en_get, vbc_dp_en_put),
	/*CALL MUTE*/
	SOC_ENUM_EXT("VBC_UL_MUTE",
		     vbc_call_mute_enum[VBC_UL_MUTE],
		     vbc_call_mute_get, vbc_call_mute_put),
	SOC_ENUM_EXT("VBC_DL_MUTE",
		     vbc_call_mute_enum[VBC_DL_MUTE],
		     vbc_call_mute_get, vbc_call_mute_put),
	SOC_ENUM_EXT("VBC IIS Master Setting", iis_master_setting_enum,
		     vbc_get_aud_iis_clock, vbc_put_aud_iis_clock),
	SOC_SINGLE_BOOL_EXT("agdsp_access_en", 0,
			    vbc_get_agdsp_access, vbc_put_agdsp_aud_access),
	SOC_SINGLE_BOOL_EXT("agdsp_access_a2dp_en", 0,
			    vbc_get_agdsp_access, vbc_put_agdsp_a2dp_access),
	/* VBC VOLUME */
	SOC_SINGLE_EXT("VBC_VOLUME", SND_SOC_NOPM, 0,
		       MAX_32_BIT, 0,
		       vbc_volume_get, vbc_volume_put),
	/* PROFILE */
	SOC_SINGLE_EXT("Audio Structure Profile Update",
		       SND_VBC_PROFILE_AUDIO_STRUCTURE, 0,
		       2, 0,
		       vbc_profile_load_get, vbc_profile_load_put),
	SOC_SINGLE_EXT("DSP VBC Profile Update",
		       SND_VBC_PROFILE_DSP, 0,
		       2, 0,
		       vbc_profile_load_get, vbc_profile_load_put),
	SOC_SINGLE_EXT("CVS Profile Update",
		       SND_VBC_PROFILE_NXP, 0,
		       2, 0,
		       vbc_profile_load_get, vbc_profile_load_put),
	SOC_SINGLE_EXT("DSP SMARTAMP Update",
				SND_VBC_PROFILE_IVS_SMARTPA, 0,
				2, 0,
				vbc_profile_load_get, vbc_profile_load_put),
	SOC_SINGLE_EXT("Audio Structure Profile Select",
		       SND_VBC_PROFILE_AUDIO_STRUCTURE, 0,
		       VBC_PROFILE_CNT_MAX, 0,
		       vbc_profile_get, vbc_profile_put),
	SOC_SINGLE_EXT("DSP VBC Profile Select",
		       SND_VBC_PROFILE_DSP, 0,
		       VBC_PROFILE_CNT_MAX, 0,
		       vbc_profile_get, vbc_profile_put),
	SOC_SINGLE_EXT("NXP Profile Select",
		       SND_VBC_PROFILE_NXP, 0,
		       VBC_PROFILE_CNT_MAX, 0,
		       vbc_profile_get, vbc_profile_put),
	SOC_SINGLE_EXT("DSP FFSMARTAMP Select",
			SND_VBC_PROFILE_IVS_SMARTPA, 0,
			VBC_PROFILE_CNT_MAX, 0,
			vbc_profile_get, vbc_profile_put),

	/* IIS RX/TX WD */
	SOC_ENUM_EXT("VBC_IIS_TX0_WD_SEL", vbc_iis_tx_wd_enum[
		     VBC_MUX_IIS_TX_DAC0],
		     vbc_get_iis_tx_width_sel, vbc_put_iis_tx_width_sel),
	SOC_ENUM_EXT("VBC_IIS_TX1_WD_SEL", vbc_iis_tx_wd_enum[
		     VBC_MUX_IIS_TX_DAC1],
		     vbc_get_iis_tx_width_sel, vbc_put_iis_tx_width_sel),
	SOC_ENUM_EXT("VBC_IIS_TX2_WD_SEL", vbc_iis_tx_wd_enum[
		     VBC_MUX_IIS_TX_DAC2],
		     vbc_get_iis_tx_width_sel, vbc_put_iis_tx_width_sel),
	SOC_ENUM_EXT("VBC_IIS_RX0_WD_SEL", vbc_iis_rx_wd_enum[
		     VBC_MUX_IIS_RX_ADC0],
		     vbc_get_iis_rx_width_sel, vbc_put_iis_rx_width_sel),
	SOC_ENUM_EXT("VBC_IIS_RX1_WD_SEL", vbc_iis_rx_wd_enum[
		     VBC_MUX_IIS_RX_ADC1],
		     vbc_get_iis_rx_width_sel, vbc_put_iis_rx_width_sel),
	SOC_ENUM_EXT("VBC_IIS_RX2_WD_SEL", vbc_iis_rx_wd_enum[
		     VBC_MUX_IIS_RX_ADC2],
		     vbc_get_iis_rx_width_sel, vbc_put_iis_rx_width_sel),
	SOC_ENUM_EXT("VBC_IIS_RX3_WD_SEL", vbc_iis_rx_wd_enum[
		     VBC_MUX_IIS_RX_ADC3],
		     vbc_get_iis_rx_width_sel, vbc_put_iis_rx_width_sel),
	/* IIS RX/TX LR_MOD */
	SOC_ENUM_EXT("VBC_IIS_TX0_LRMOD_SEL", vbc_iis_tx_lr_mod_enum[
		     VBC_MUX_IIS_TX_DAC0],
		     vbc_get_iis_tx_lr_mod_sel, vbc_put_iis_tx_lr_mod_sel),
	SOC_ENUM_EXT("VBC_IIS_TX1_LRMOD_SEL", vbc_iis_tx_lr_mod_enum[
		     VBC_MUX_IIS_TX_DAC1],
		     vbc_get_iis_tx_lr_mod_sel, vbc_put_iis_tx_lr_mod_sel),
	SOC_ENUM_EXT("VBC_IIS_TX2_LRMOD_SEL", vbc_iis_tx_lr_mod_enum[
		     VBC_MUX_IIS_TX_DAC2],
		     vbc_get_iis_tx_lr_mod_sel, vbc_put_iis_tx_lr_mod_sel),
	SOC_ENUM_EXT("VBC_IIS_RX0_LRMOD_SEL", vbc_iis_rx_lr_mod_enum[
		     VBC_MUX_IIS_RX_ADC0],
		     vbc_get_iis_rx_lr_mod_sel, vbc_put_iis_rx_lr_mod_sel),
	SOC_ENUM_EXT("VBC_IIS_RX1_LRMOD_SEL", vbc_iis_rx_lr_mod_enum[
		     VBC_MUX_IIS_RX_ADC1],
		     vbc_get_iis_rx_lr_mod_sel, vbc_put_iis_rx_lr_mod_sel),
	SOC_ENUM_EXT("VBC_IIS_RX2_LRMOD_SEL", vbc_iis_rx_lr_mod_enum[
		     VBC_MUX_IIS_RX_ADC2],
		     vbc_get_iis_rx_lr_mod_sel, vbc_put_iis_rx_lr_mod_sel),
	SOC_ENUM_EXT("VBC_IIS_RX3_LRMOD_SEL", vbc_iis_rx_lr_mod_enum[
		     VBC_MUX_IIS_RX_ADC3],
		     vbc_get_iis_rx_lr_mod_sel, vbc_put_iis_rx_lr_mod_sel),
	SOC_ENUM_EXT("VBC_IIS_MASTER_ENALBE", vbc_iis_master_enum,
		     vbc_get_iis_master_en, vbc_put_iis_master_en),
	SOC_ENUM_EXT("VBC_IIS_MST_SEL_0_TYPE",
		     vbc_iis_mst_sel_enum[IIS_MST_SEL_0], vbc_get_mst_sel_type,
		     vbc_put_mst_sel_type),
	SOC_ENUM_EXT("VBC_IIS_MST_SEL_1_TYPE",
		     vbc_iis_mst_sel_enum[IIS_MST_SEL_1], vbc_get_mst_sel_type,
		     vbc_put_mst_sel_type),
	SOC_ENUM_EXT("VBC_IIS_MST_SEL_2_TYPE",
		     vbc_iis_mst_sel_enum[IIS_MST_SEL_2], vbc_get_mst_sel_type,
		     vbc_put_mst_sel_type),
	SOC_ENUM_EXT("VBC_IIS_MST_SEL_3_TYPE",
		     vbc_iis_mst_sel_enum[IIS_MST_SEL_3], vbc_get_mst_sel_type,
		     vbc_put_mst_sel_type),

	SOC_ENUM_EXT("VBC_IIS_MST_WIDTH_SET", vbc_iis_master_wd_width_enum,
		     vbc_get_iis_master_width, vbc_put_iis_master_width),

	SOC_ENUM_EXT("VBC_DSP_MAINMIC_PATH_SEL",
		     vbc_mainmic_path_enum[MAINMIC_USED_DSP_NORMAL_ADC],
		     vbc_mainmic_path_sel_val_get,
		     vbc_mainmic_path_sel_val_put),
	SOC_ENUM_EXT("VBC_DSP_MAINMIC_REF_PATH_SEL",
		     vbc_mainmic_path_enum[MAINMIC_USED_DSP_REF_ADC],
		     vbc_mainmic_path_sel_val_get,
		     vbc_mainmic_path_sel_val_put),
	SOC_ENUM_EXT("IVSENCE_FUNC_DSP", ivsence_enum,
		     vbc_get_ivsence_func, vbc_put_ivsence_func),
	/* PIN MUX */
	SOC_ENUM_EXT("ag_iis0_ext_sel", vbc_ag_iis_ext_sel_enum[0],
		     vbc_get_ag_iis_ext_sel, vbc_put_ag_iis_ext_sel),
	SOC_ENUM_EXT("ag_iis1_ext_sel", vbc_ag_iis_ext_sel_enum[1],
		     vbc_get_ag_iis_ext_sel, vbc_put_ag_iis_ext_sel),
	SOC_ENUM_EXT("ag_iis2_ext_sel", vbc_ag_iis_ext_sel_enum[2],
		     vbc_get_ag_iis_ext_sel, vbc_put_ag_iis_ext_sel),
	SOC_ENUM_EXT("SYS_IIS0", vbc_sys_iis_enum[SYS_IIS0],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS1", vbc_sys_iis_enum[SYS_IIS1],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS2", vbc_sys_iis_enum[SYS_IIS2],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS3", vbc_sys_iis_enum[SYS_IIS3],
		     sys_iis_sel_get, sys_iis_sel_put),

	SOC_ENUM_EXT("SYS_IIS4", vbc_sys_iis_enum[SYS_IIS4],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS5", vbc_sys_iis_enum[SYS_IIS5],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS6", vbc_sys_iis_enum[SYS_IIS6],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_SINGLE_EXT("VBC_BAK_REG_SET", REG_VBC_BAK_REG, 1, 2, 0,
		       vbc_reg_get, vbc_reg_put),
	SOC_ENUM_EXT("VBC_DUMP_POS", vbc_dump_pos_enum,
		vbc_get_dump_pos, vbc_put_dump_pos),
	SND_SOC_BYTES_EXT("SBC_PARAS", SBC_PARA_BYTES,
		sbc_paras_get, sbc_paras_put),

	SOC_ENUM_EXT("VBC_IIS_INF_SYS_SEL", vbc_iis_inf_sys_sel_enum,
		vbc_iis_inf_sys_sel_get, vbc_iis_inf_sys_sel_put),
};

static u32 vbc_codec_read(struct snd_soc_codec *codec,
			  u32 reg)
{
	if (IS_AP_VBC_RANG(reg))
		return ap_vbc_reg_read(reg);
	else if (IS_DSP_VBC_RANG(reg))
		return dsp_vbc_reg_read(reg);

	return 0;
}

static int vbc_codec_write(struct snd_soc_codec *codec, u32 reg,
			   u32 val)
{
	if (IS_AP_VBC_RANG(reg))
		return ap_vbc_reg_write(reg, val);
	else if (IS_DSP_VBC_RANG(reg))
		return dsp_vbc_reg_write(reg, val, 0xffffffff);

	return 0;
}

#ifdef CONFIG_PROC_FS
static int dsp_vbc_reg_shm_proc_read(struct snd_info_buffer *buffer)
{
	int ret;
	int reg;
	u32 size = 8*2048;
	u32 *addr = kzalloc(size, GFP_KERNEL);

	if (!addr)
		return -ENOMEM;

	ret = aud_recv_block_param(AMSG_CH_VBC_CTL, -1, -1,
		SND_VBC_DSP_IO_SHAREMEM_GET, SND_VBC_SHM_VBC_REG, addr, size,
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0) {
		kfree(addr);
		return -1;
	}

	snd_iprintf(buffer, "dsp-vbc register dump:\n");
	for (reg = REG_VBC_MODULE_CLR0;
	     reg <= REG_VBC_IIS_IN_STS; reg += 0x10, addr += 4) {
		snd_iprintf(buffer,
			    "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    reg - VBC_DSP_ADDR_BASE, (*addr),
			    *(addr + 1), *(addr + 2), *(addr + 3));
	}
	kfree(addr);

	return 0;
}

static u32 ap_vbc_reg_proc_read(struct snd_info_buffer *buffer)
{
	int reg, ret;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("agdsp_access_enable:error:%d", ret);
		return ret;
	}
	snd_iprintf(buffer, "ap-vbc register dump\n");
	for (reg = REG_VBC_AUDPLY_FIFO_CTRL;
	     reg <= VBC_AP_ADDR_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    reg, ap_vbc_reg_read(reg + 0x00)
			    , ap_vbc_reg_read(reg + 0x04)
			    , ap_vbc_reg_read(reg + 0x08)
			    , ap_vbc_reg_read(reg + 0x0C));
	}
	agdsp_access_disable();

	return 0;
}

static void vbc_proc_write(struct snd_info_entry *entry,
			   struct snd_info_buffer *buffer)
{
	char line[64];
	u32 reg, val;
	int ret;
	struct snd_soc_codec *codec = entry->private_data;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err(":%s:agdsp_access_enable error:%d", __func__, ret);
		return;
	}
	while (!snd_info_get_line(buffer, line, sizeof(line))) {
		if (sscanf(line, "%x %x", &reg, &val) != 2)
			continue;
		pr_err("%s, reg:0x%x, val:0x%x\n", __func__, reg, val);
		if (val <= 0xfffffff)
			snd_soc_write(codec, reg, val);
	}
	agdsp_access_disable();
}

static void vbc_audcp_ahb_proc_read(struct snd_info_buffer *buffer)
{
	u32 val, reg;
	int ret;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return;
	}
	reg = REG_AGCP_AHB_EXT_ACC_AG_SEL;
	agcp_ahb_reg_read(reg, &val);
	snd_iprintf(buffer, "audcp ahb register dump\n");
	snd_iprintf(buffer, "0x%04x | 0x%04x\n", reg, val);
	agdsp_access_disable();
}

static void vbc_proc_read(struct snd_info_entry *entry,
			  struct snd_info_buffer *buffer)
{
	int ret;

	ret = dsp_vbc_reg_shm_proc_read(buffer);
	if (ret < 0)
		snd_iprintf(buffer, "dsp-vbc register dump error\n");

	ret = ap_vbc_reg_proc_read(buffer);
	if (ret < 0)
		snd_iprintf(buffer, "ap-vbc register dump error\n");
	vbc_audcp_ahb_proc_read(buffer);
}

static void vbc_proc_init(struct snd_soc_codec *codec)
{
	struct snd_info_entry *entry;
	struct snd_card *card = codec->component.card->snd_card;

	if (!snd_card_proc_new(card, "vbc", &entry))
		snd_info_set_text_ops(entry, codec, vbc_proc_read);
	entry->c.text.write = vbc_proc_write;
	entry->mode |= 0200;
}
#else
/* !CONFIG_PROC_FS */
static inline void vbc_proc_init(struct snd_soc_codec *codec)
{
}
#endif

static int vbc_codec_soc_probe(struct snd_soc_codec *codec)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_profile *vbc_profile_setting =
		&vbc_codec->vbc_profile_setting;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	sp_asoc_pr_dbg("%s\n", __func__);

	dapm->idle_bias_off = 1;

	vbc_codec->codec = codec;
	vbc_profile_setting->codec = codec;

	vbc_proc_init(codec);

	return 0;
}

static int vbc_codec_soc_remove(struct snd_soc_codec *codec)
{
	return 0;
}

struct snd_soc_codec_driver sprd_vbc_codec = {
	.probe = vbc_codec_soc_probe,
	.remove = vbc_codec_soc_remove,
	.read = vbc_codec_read,
	.write = vbc_codec_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.component_driver = {
		.controls = vbc_codec_snd_controls,
		.num_controls = ARRAY_SIZE(vbc_codec_snd_controls),
	}
};
EXPORT_SYMBOL(sprd_vbc_codec);

static void init_vbc_codec_data(struct vbc_codec_priv *vbc_codec)
{
	/* vbc dac */
	vbc_codec->mux_iis_tx[VBC_MUX_IIS_TX_DAC0].id = VBC_MUX_IIS_TX_DAC0;
	vbc_codec->mux_iis_tx[VBC_MUX_IIS_TX_DAC0].val = VBC_IIS_PORT_IIS0;
	vbc_codec->mux_iis_tx[VBC_MUX_IIS_TX_DAC1].id = VBC_MUX_IIS_TX_DAC1;
	vbc_codec->mux_iis_tx[VBC_MUX_IIS_TX_DAC1].val = VBC_IIS_PORT_IIS1;
	vbc_codec->mux_iis_tx[VBC_MUX_IIS_TX_DAC2].id = VBC_MUX_IIS_TX_DAC2;
	vbc_codec->mux_iis_tx[VBC_MUX_IIS_TX_DAC2].val = VBC_IIS_PORT_IIS2;

	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS0].id = VBC_IIS_PORT_IIS0;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS0].val = IIS_DO_VAL_DAC0;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS1].id = VBC_IIS_PORT_IIS1;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS1].val = IIS_DO_VAL_DAC1;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS2].id = VBC_IIS_PORT_IIS2;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS2].val = IIS_DO_VAL_DAC2;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS3].id = VBC_IIS_PORT_IIS3;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_IIS3].val = IIS_DO_VAL_DAC0;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_MST_IIS0].id =
		VBC_IIS_PORT_MST_IIS0;
	vbc_codec->mux_iis_port_do[VBC_IIS_PORT_MST_IIS0].val = IIS_DO_VAL_DAC0;

	/* vbc adc */
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC0].id = VBC_MUX_IIS_RX_ADC0;
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC0].val = VBC_IIS_PORT_IIS1;
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC1].id = VBC_MUX_IIS_RX_ADC1;
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC1].val = VBC_IIS_PORT_IIS1;
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC2].id = VBC_MUX_IIS_RX_ADC2;
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC2].val = VBC_IIS_PORT_IIS1;
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC3].id = VBC_MUX_IIS_RX_ADC3;
	vbc_codec->mux_iis_rx[VBC_MUX_IIS_RX_ADC3].val = VBC_IIS_PORT_IIS3;

	/* iis width */
	vbc_codec->iis_tx_wd[VBC_MUX_IIS_TX_DAC0].id = VBC_MUX_IIS_TX_DAC0;
	vbc_codec->iis_tx_wd[VBC_MUX_IIS_TX_DAC0].value = WD_24BIT;
	vbc_codec->iis_tx_wd[VBC_MUX_IIS_TX_DAC1].id = VBC_MUX_IIS_TX_DAC1;
	vbc_codec->iis_tx_wd[VBC_MUX_IIS_TX_DAC1].value = WD_16BIT;
	vbc_codec->iis_tx_wd[VBC_MUX_IIS_TX_DAC2].id = VBC_MUX_IIS_TX_DAC2;
	vbc_codec->iis_tx_wd[VBC_MUX_IIS_TX_DAC2].value = WD_16BIT;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC0].id = VBC_MUX_IIS_RX_ADC0;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC0].value = WD_24BIT;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC1].id = VBC_MUX_IIS_RX_ADC1;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC1].value = WD_24BIT;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC2].id = VBC_MUX_IIS_RX_ADC2;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC2].value = WD_16BIT;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC3].id = VBC_MUX_IIS_RX_ADC3;
	vbc_codec->iis_rx_wd[VBC_MUX_IIS_RX_ADC3].value = WD_16BIT;
	/* iis lr mod */
	vbc_codec->iis_tx_lr_mod[VBC_MUX_IIS_TX_DAC0].id = VBC_MUX_IIS_TX_DAC0;
	vbc_codec->iis_tx_lr_mod[VBC_MUX_IIS_TX_DAC0].value = LEFT_HIGH;
	vbc_codec->iis_tx_lr_mod[VBC_MUX_IIS_TX_DAC1].id = VBC_MUX_IIS_TX_DAC1;
	vbc_codec->iis_tx_lr_mod[VBC_MUX_IIS_TX_DAC1].value = LEFT_HIGH;
	vbc_codec->iis_tx_lr_mod[VBC_MUX_IIS_TX_DAC2].id = VBC_MUX_IIS_TX_DAC2;
	vbc_codec->iis_tx_lr_mod[VBC_MUX_IIS_TX_DAC2].value = LEFT_HIGH;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC0].id = VBC_MUX_IIS_RX_ADC0;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC0].value = LEFT_HIGH;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC1].id = VBC_MUX_IIS_RX_ADC1;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC1].value = LEFT_HIGH;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC2].id = VBC_MUX_IIS_RX_ADC2;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC2].value = LEFT_HIGH;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC3].id = VBC_MUX_IIS_RX_ADC3;
	vbc_codec->iis_rx_lr_mod[VBC_MUX_IIS_RX_ADC3].value = LEFT_HIGH;
	/* default vbc iis2 connect to aon usb audio */
	vbc_codec->vbc_iis_inf_sys_sel = 1;

	/* iis master control */
	vbc_codec->mst_sel_para[IIS_MST_SEL_0].id = IIS_MST_SEL_0;
	vbc_codec->mst_sel_para[IIS_MST_SEL_0].mst_type = VBC_MASTER_EXTERNAL;
	vbc_codec->mst_sel_para[IIS_MST_SEL_1].id = IIS_MST_SEL_1;
	vbc_codec->mst_sel_para[IIS_MST_SEL_1].mst_type = VBC_MASTER_EXTERNAL;
	vbc_codec->mst_sel_para[IIS_MST_SEL_2].id = IIS_MST_SEL_2;
	vbc_codec->mst_sel_para[IIS_MST_SEL_2].mst_type = VBC_MASTER_EXTERNAL;
	vbc_codec->mst_sel_para[IIS_MST_SEL_3].id = IIS_MST_SEL_3;
	vbc_codec->mst_sel_para[IIS_MST_SEL_3].mst_type = VBC_MASTER_EXTERNAL;
}

int sprd_vbc_codec_probe(struct platform_device *pdev)
{
	struct vbc_codec_priv *vbc_codec;
	struct device_node *np = pdev->dev.of_node;
	struct regmap *agcp_ahb_gpr;

	sp_asoc_pr_dbg("%s\n", __func__);

	vbc_codec = devm_kzalloc(&pdev->dev, sizeof(struct vbc_codec_priv),
				 GFP_KERNEL);
	if (vbc_codec == NULL)
		return -ENOMEM;
	init_vbc_codec_data(vbc_codec);
	platform_set_drvdata(pdev, vbc_codec);

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

	/* If need internal codec(audio top) to provide clock for
	 * vbc da path.
	 */
	vbc_codec->need_aud_top_clk =
		of_property_read_bool(np, "sprd,need-aud-top-clk");
	mutex_init(&vbc_codec->load_mutex);
	mutex_init(&vbc_codec->agcp_access_mutex);

	return 0;
}

int sprd_vbc_codec_remove(struct platform_device *pdev)
{
	struct vbc_codec_priv *vbc_codec = platform_get_drvdata(pdev);

	vbc_codec->vbc_profile_setting.dev = 0;

	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}
