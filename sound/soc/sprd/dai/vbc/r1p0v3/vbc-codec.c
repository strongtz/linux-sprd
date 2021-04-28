/*
 * sound/soc/sprd/dai/vbc/r1p0v3/vbc-codec.c
 *
 * SPRD SoC VBC Component -- SpreadTrum SOC VBC Component function.
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

#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <sound/core.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/file.h>
#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"

#include "vbc-codec.h"

#define MIXER_MUX_BY_EQ 5
#define MIXER_OUT_BY_EQ 2

#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define SOC_REG(r) ((unsigned short)(r))

/* Returns the driver data of the codec associated to @kctl which
 * belongs to a widget.
 */
#define widget_kcontrol_to_codec_drvdata(kctl) \
	snd_soc_codec_get_drvdata(snd_soc_dapm_kcontrol_codec((kctl)))

/* Returns the driver data of the codec associated to @kctl which
 * belongs to a codec.
 */
#define codec_kcontrol_to_codec_drvdata(kctl) \
	snd_soc_codec_get_drvdata(snd_soc_kcontrol_codec((kctl)))

/* Returns the driver data of the codec associated to @w. */
#define sprd_widget_to_codec_drvdata(w) \
		snd_soc_codec_get_drvdata(snd_soc_dapm_to_codec((w->dapm)))

static const u32 vbc_da_eq_profile_default[VBC_DA_EFFECT_PARAS_LEN] = {
/* TODO the default register value */
	/* REG_VBC_VBC_DAC_PATH_CTRL */
	0x00000000,
	/* REG_VBC_VBC_DAC_HP_CTRL
	 * ninglei diff bit eq6, alc,
	 * iis width 24(substitution vbc_da_iis_wd_sel)
	 */
	0x00000a7F,
	/*ALC default para */
	/* REG_VBC_VBC_DAC_ALC_CTRL0 */
	0x000001e0,
	/* REG_VBC_VBC_DAC_ALC_CTRL1 */
	0x00002000,
	/* REG_VBC_VBC_DAC_ALC_CTRL2 */
	0x000004fe,
	/* REG_VBC_VBC_DAC_ALC_CTRL3 */
	0x0000001f,
	/* REG_VBC_VBC_DAC_ALC_CTRL4 */
	0x00000000,
	/* REG_VBC_VBC_DAC_ALC_CTRL5 */
	0x00007fff,
	/* REG_VBC_VBC_DAC_ALC_CTRL6 */
	0x0000028c,
	/* REG_VBC_VBC_DAC_ALC_CTRL7 */
	0x00000010,
	/* REG_VBC_VBC_DAC_ALC_CTRL8 */
	0x000004dd,
	/* REG_VBC_VBC_DAC_ALC_CTRL9 */
	0x00000000,
	/* REG_VBC_VBC_DAC_ALC_CTRL10 */
	0x00000062,
	/* REG_VBC_VBC_DAC_ST_CTL0 */
	0x00000183,
	/*  REG_VBC_VBC_DAC_ST_CTL1  */
	0x00000183,
	/*  REG_VBC_VBC_DAC_SRC_CTRL */
	0x00000000,
	/* REG_VBC_VBC_MIXER_CTRL  */
	0x00000000,
	/* REG_VBC_VBC_DAC_NGC_VTHD */
	0x00000000,
	/* REG_VBC_VBC_DAC_NGC_TTHD */
	0x00000000,
	/* REG_VBC_VBC_DAC_NGC_CTRL */
	0x00000000,

	/*DA eq6 */
	/*0x100  -- 0x134 */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x138  -- 0x16c */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x170  -- 0x1a4 */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x1a8  -- 0x1dc */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x1e0  -- 0x214 */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x218  -- 0x24c */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x250  -- 0x254 */
	0x00000400,
	0x00000000,

	/*DA eq4 */
	/*0x258  -- 0x28c */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x290  -- 0x2c4 */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x2c8  -- 0x2fc */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x300  -- 0x334 */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x338  -- 0x33c */
	0x00001000,
	0x00000000,
};

/* AD01 AD23 share this config */
static const u32 vbc_ad_eq_profile_default[VBC_AD_EFFECT_PARAS_LEN] = {
/* TODO the default register value */
	/* REG_VBC_VBC_ADC_PATH_CTRL */
	0x00000000,
	/* REG_VBC_VBC_ADC_EQ_CTRL */
	0x00000000,
	/*AD01/23 eq6 */
	/*0x400  -- 0x434 */
	/* ADC01_HPCOEF0_H or ADC23_HPCOEF0_H */
	0x00001000,
	/* ADC01_HPCOEF0_L or ADC23_HPCOEF0_L */
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x438  -- 0x46c */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x470  -- 0x4a4 */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x4a8  -- 0x4dc */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x4e0  -- 0x514 */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x518  -- 0x54c */
	0x00001000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00004000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	/*0x550  -- 0x554 */
	0x00001000,
	0x00000000,
};

static void vbc_eq_profile_close(struct snd_soc_codec *codec, int vbc_eq_idx);
static void vbc_eq_try_apply(struct snd_soc_codec *codec, int vbc_eq_idx);
static int vbc_eq_enable(int enable, struct vbc_codec_priv *vbc_codec);


#define IS_SPRD_VBC_MUX_RANG(reg) \
	((reg) >= SPRD_VBC_MUX_START && (reg) < (SPRD_VBC_MUX_MAX))
#define SPRD_VBC_MUX_IDX(reg) (reg - SPRD_VBC_MUX_START)
#define IS_SPRD_VBC_DFM_ST_MUX_RANG(id) \
	(((id) >= SPRD_VBC_DFM_DA0_ADDFM_INMUX) \
	&& ((id) <= SPRD_VBC_DFM_DA1_ADDST_INMUX))

static const char *vbc_mux_debug_str[SPRD_VBC_MUX_MAX] = {
	"st0 chan mux",
	"st1 chan mux",
	"st0 mux",
	"st1 mux",
	"ad0 inmux",
	"ad1 inmux",
	"ad2 inmux",
	"ad3 inmux",
	"ad iis mux",
	"ad23 iis mux",
	"da0 addfm mux",
	"da1 addfm mux",
	"da0 addst mux",
	"da1 addfm mux"
};

/* fm st adder */
void vbc_fm_adder(u32 mode, u32 chan)
{
	unsigned int bit = BIT_RF_ST_FM_SEL;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}
	vbc_reg_update(REG_VBC_VBC_DAC_PATH_CTRL, bit, bit);
	if (AUDIO_IS_CHAN_L(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_PATH_CTRL,
			       BITS_RF_DAC0_ADDFM_SEL(mode),
			       BITS_RF_DAC0_ADDFM_SEL(0x3));
	}
	if (AUDIO_IS_CHAN_R(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_PATH_CTRL,
			       BITS_RF_DAC1_ADDFM_SEL(mode),
			       BITS_RF_DAC1_ADDFM_SEL(0x3));
	}
}

void vbc_st_adder(u32 mode, u32 chan)
{
	unsigned int bit = BIT_RF_ST_FM_SEL;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}
	vbc_reg_update(REG_VBC_VBC_DAC_PATH_CTRL, ~bit, bit);
	if (AUDIO_IS_CHAN_L(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_PATH_CTRL,
			       BITS_RF_DAC0_ADDST_SEL(mode),
			       BITS_RF_DAC0_ADDST_SEL(0x3));
	}
	if (AUDIO_IS_CHAN_R(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_PATH_CTRL,
			       BITS_RF_DAC1_ADDST_SEL(mode),
			       BITS_RF_DAC1_ADDST_SEL(0x3));
	}
}

/* must dgmixer enable (DA_DGMIXER in vbc_da_module_enable) */
static void vbc_da01_set_dgmixer_dg(u32 chan, u32 dg)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	if (AUDIO_IS_CHAN_L(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_DGMIXER_DG0,
			       BITS_RF_DAC0_DGMIXER_DG(dg),
			       BITS_RF_DAC0_DGMIXER_DG(0xffff));
	}
	if (AUDIO_IS_CHAN_R(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_DGMIXER_DG0,
			       BITS_RF_DAC1_DGMIXER_DG(dg),
			       BITS_RF_DAC1_DGMIXER_DG(0xffff));
	}
}

static void vbc_da23_set_dgmixer_dg(u32 chan, u32 dg)
{
	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	if (AUDIO_IS_CHAN_L(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_DGMIXER_DG1,
			       BITS_RF_DAC2_DGMIXER_DG(dg),
			       BITS_RF_DAC2_DGMIXER_DG(0xffff));
	}
	if (AUDIO_IS_CHAN_R(chan)) {
		vbc_reg_update(REG_VBC_VBC_DAC_DGMIXER_DG1,
			       BITS_RF_DAC3_DGMIXER_DG(dg),
			       BITS_RF_DAC3_DGMIXER_DG(0xffff));
	}
}

static void vbc_da01_set_dgmixer_step(u32 step)
{
	vbc_reg_update(REG_VBC_VBC_DAC_DG_CTRL,
		       BITS_RF_DAC_DGMIXER_STP(step),
		       BITS_RF_DAC_DGMIXER_STP(0x1fff));
}

/* function:
 * mute left and right channel direct.
 * it does not depends on vbc_fm mute module_en.
 */
static void vbc_fm_mute_direct(int32_t mute)
{
	uint32_t bit = BIT_RF_FM_MUTE_DIRECT;

	vbc_reg_update(REG_VBC_VBC_FM_MUTE_CTL,
		       (true == mute) ? bit : ~bit, bit);
}

/* it depends on vbc_fm mute module_en */
static void vbc_fm_mute_smooth(int32_t mute,
	struct vbc_codec_priv *vbc_codec)
{
	uint32_t bit_module_enable = 0;
	uint32_t val_module_enable = 0;
	uint32_t bit_step_enable = 0;
	uint32_t val_step_enable = 0;
	uint32_t bit_step_vale = 0;
	uint32_t val_step_vale = 0;
	uint32_t val = 0;
	uint32_t reg = REG_VBC_VBC_FM_MUTE_CTL;
	uint32_t mask = 0;

	/* fm mute module enalbe */
	bit_module_enable = BIT_RF_FM_MUTE_EN_0 | BIT_RF_FM_MUTE_EN_1;
	bit_step_enable = BIT_RF_FM_MUTE_CTL;
	/* note:write 0 mute, write 1 unmute */
	val_step_enable = (mute == true) ? 0 : bit_step_enable;

	val_module_enable = bit_module_enable;
	bit_step_vale = BITS_RF_FM_MUTE_DG_STP(0x1FFF);
	val_step_vale = BITS_RF_FM_MUTE_DG_STP(vbc_codec->fm_mutedg_step);

	mask = bit_module_enable | bit_step_enable | bit_step_vale;
	val = val_module_enable | val_step_enable | val_step_vale;

	vbc_reg_update(reg, val, mask);
}

static void vbc_mixer_mux_sel(enum VBC_MIXER_ID_E mixer_id, u32 chan,
			      unsigned int sel)
{
	u32 reg = REG_VBC_VBC_MIXER_CTRL;
	u32 mask = 0;
	u32 val = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan(%u)\n", __func__, chan);
		return;
	}

	pr_debug("%s, mixer: %d, chan: %u, sel: %u\n", __func__, mixer_id, chan,
		 sel);

	if (sel == MIXER_MUX_BY_EQ)
		return;

	switch (mixer_id) {
	case VBC_MIXER_DAC:
		if (AUDIO_IS_CHAN_L(chan)) {
			val |= BITS_RF_DAC0_MIXER_MUX_SEL(sel);
			mask |= BITS_RF_DAC0_MIXER_MUX_SEL(0x7);
		}
		if (AUDIO_IS_CHAN_R(chan)) {
			val |= BITS_RF_DAC1_MIXER_MUX_SEL(sel);
			mask |= BITS_RF_DAC1_MIXER_MUX_SEL(0x7);
		}
		break;
	case VBC_MIXER_ST:
		if (AUDIO_IS_CHAN_L(chan)) {
			val |= BITS_RF_ST_MIXER_MUX_SEL_0(sel);
			mask |= BITS_RF_ST_MIXER_MUX_SEL_0(0x7);
		}
		if (AUDIO_IS_CHAN_R(chan)) {
			val |= BITS_RF_ST_MIXER_MUX_SEL_1(sel);
			mask |= BITS_RF_ST_MIXER_MUX_SEL_1(0x7);
		}
		break;
	default:
		pr_err("%s, invalid mixer id(%d)!\n", __func__, mixer_id);
		return;
	}

	vbc_reg_update(reg, val, mask);
}

static void vbc_mixer_out_sel(enum VBC_MIXER_ID_E mixer_id, u32 chan,
			      unsigned int sel)
{
	u32 reg = REG_VBC_VBC_MIXER_CTRL;
	u32 mask = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan(%u)\n", __func__, chan);
		return;
	}

	pr_debug("%s, mixer id: %d, chan: %u, sel: %u\n", __func__, mixer_id,
		 chan, sel);

	if (sel == MIXER_OUT_BY_EQ)
		return;

	switch (mixer_id) {
	case VBC_MIXER_DAC:
		if (AUDIO_IS_CHAN_L(chan))
			mask |= BIT_RF_DAC0_MIXER_OUT_SEL;
		if (AUDIO_IS_CHAN_R(chan))
			mask |= BIT_RF_DAC1_MIXER_OUT_SEL;
		break;
	case VBC_MIXER_ST:
		if (AUDIO_IS_CHAN_L(chan))
			mask |= BIT_RF_ST_MIXER_OUT_SEL_0;
		if (AUDIO_IS_CHAN_R(chan))
			mask |= BIT_RF_ST_MIXER_OUT_SEL_1;
		break;
	default:
		pr_err("%s, invalid mixer id(%d)!\n", __func__, mixer_id);
		return;
	}

	vbc_reg_update(reg, sel ? mask : 0, mask);
}

static void vbc_mixer_sel_set(struct vbc_codec_priv *vbc_codec,
			      enum VBC_MIXER_ID_E mixer_id)
{
	unsigned int (*mux_sel)[CHAN_IDX_MAX] = vbc_codec->mixer_mux_sel;
	unsigned int (*out_sel)[CHAN_IDX_MAX] = vbc_codec->mixer_out_sel;

	if (mixer_id < 0 || mixer_id >= VBC_MIXER_MAX) {
		dev_err(vbc_codec->codec->dev, "%s, invalid mixer id: %u(max: %u)\n",
			__func__, mixer_id, VBC_MIXER_MAX - 1);
		return;
	}

	vbc_mixer_out_sel(mixer_id, AUDIO_CHAN_L,
		out_sel[mixer_id][CHAN_IDX_0]);
	vbc_mixer_out_sel(mixer_id, AUDIO_CHAN_R,
		out_sel[mixer_id][CHAN_IDX_1]);
	vbc_mixer_mux_sel(mixer_id, AUDIO_CHAN_L,
		mux_sel[mixer_id][CHAN_IDX_0]);
	vbc_mixer_mux_sel(mixer_id, AUDIO_CHAN_R,
		mux_sel[mixer_id][CHAN_IDX_1]);
}

static void vbc_st_chan_sel(u32 id, u32 chan)
{
	uint32_t bit = 0;
	u32 reg = REG_VBC_VBC_DAC_ST_CTL1;
	u32 val = 0;
	u32 mask = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}
	/* sel 0:Side tone channel input ADC01;
	 * 1:Side tone channel input ADC23
	 */
	if (AUDIO_IS_CHAN_L(chan))
		bit |= BIT_RF_ST_SEL_CHN_0;
	if (AUDIO_IS_CHAN_R(chan))
		bit |= BIT_RF_ST_SEL_CHN_1;

	if (id == VBC_CAPTURE01)
		val |= ~bit;
	else if (id == VBC_CAPTURE23)
		val |= bit;
	else
		pr_err("%s, invalid id(%d) for left channel!\n", __func__, id);
	mask |= bit;
	vbc_reg_update(reg, val, mask);
}

/* depends to vbc_st_chan_sel */
static void vbc_st_data_inmux_sel(u32 mode, u32 chan)
{
	u32 reg = REG_VBC_VBC_ADC_PATH_CTRL;
	u32 val = 0;
	u32 mask = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	if (AUDIO_IS_CHAN_L(chan)) {
		val |= BITS_RF_ST_INMUX_SEL0(mode);
		mask |= BITS_RF_ST_INMUX_SEL0(0x3);
	}

	if (AUDIO_IS_CHAN_R(chan)) {
		val |= BITS_RF_ST_INMUX_SEL1(mode);
		mask |= BITS_RF_ST_INMUX_SEL0(0x3);
	}
	vbc_reg_update(reg, val, mask);
}

/* @path: 0-iis ad0 as ad0 input ,
 * 1-iis ad1 as ad0 input, 2,3-0 as ad0 input
 */
static void vbc_adc_Inmux_sel(u32 id, u32 path, u32 chan)
{
	u32 reg = REG_VBC_VBC_ADC_PATH_CTRL;
	u32 val = 0;
	u32 mask = 0;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	switch (id) {
	case VBC_CAPTURE01:
		if (AUDIO_IS_CHAN_L(chan)) {
			val |= BITS_RF_ADC0_INMUX_SEL(path);
			mask |= BITS_RF_ADC0_INMUX_SEL(0x3);
		}
		if (AUDIO_IS_CHAN_R(chan)) {
			val |= BITS_RF_ADC1_INMUX_SEL(path);
			mask |= BITS_RF_ADC1_INMUX_SEL(0x3);
		}
		break;
	case VBC_CAPTURE23:
		if (AUDIO_IS_CHAN_L(chan)) {
			val |= BITS_RF_ADC2_INMUX_SEL(path);
			mask |= BITS_RF_ADC2_INMUX_SEL(0x3);
		}
		if (AUDIO_IS_CHAN_R(chan)) {
			val |= BITS_RF_ADC3_INMUX_SEL(path);
			mask |= BITS_RF_ADC3_INMUX_SEL(0x3);
		}
		break;
	}
	vbc_reg_update(reg, val, mask);
}

static void vbc_ad01_data_dgmux_sel(u32 mode, u32 chan)
{
	u32 bit;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	if (AUDIO_IS_CHAN_L(chan)) {
		/* @mode:0-adc data as adc0 dg input
		 * 1-dac0 data as adc0 dg input
		 */
		bit = BIT_RF_ADC0_DGMUX_SEL;
		if (mode == 0)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, ~bit, bit);
		else if (mode == 1)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, bit, bit);
		else
			pr_err("%s, invalid mode(%d) for left channel!\n",
			       __func__, mode);
	}
	if (AUDIO_IS_CHAN_R(chan)) {
		bit = BIT_RF_ADC1_DGMUX_SEL;
		if (mode == 0)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, ~bit, bit);
		else if (mode == 1)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, bit, bit);
		else
			pr_err("%s, invalid mode(%d) for right channel!\n",
			       __func__, mode);
	}
}

static void vbc_ad23_data_dgmux_sel(u32 mode, u32 chan)
{
	u32 bit;

	if (AUDIO_CHAN_CHECK(chan) == 0) {
		pr_err("%s invalid chan %u\n", __func__, chan);
		return;
	}

	if (AUDIO_IS_CHAN_L(chan)) {
		/* 0: from adc 1: from dac0 */
		bit = BIT_RF_ADC2_DGMUX_SEL;
		if (mode == 0)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, ~bit, bit);
		else if (mode == 1)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, bit, bit);
		else
			pr_err("%s, invalid mode(%d) for left channel!\n",
			       __func__, mode);

	}
	if (AUDIO_IS_CHAN_R(chan)) {
		/* 0: from adc 1: from dac1 */
		bit = BIT_RF_ADC3_DGMUX_SEL;
		if (mode == 0)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, ~bit, bit);
		else if (mode == 1)
			vbc_reg_update(REG_VBC_VBC_ADC_PATH_CTRL, bit, bit);
		else
			pr_err("%s, invalid mode(%d) for right channel!\n",
			       __func__, mode);

	}
}

/* @mode: 0-ignore, 1-add, 2-subtract */
static int vbc_da0_addfm_set(int val)
{
	vbc_fm_adder(val, AUDIO_CHAN_L);

	return 0;
}

static int vbc_da1_addfm_set(int val)
{
	vbc_fm_adder(val, AUDIO_CHAN_R);

	return 0;
}

static int vbc_da0_addst_set(int val)
{
	vbc_st_adder(val, AUDIO_CHAN_L);

	return 0;
}

static int vbc_da1_addst_set(int val)
{
	vbc_st_adder(val, AUDIO_CHAN_R);

	return 0;
}

static int vbc_st0_chnmux_set(int val)
{
	if (val == 0)
		vbc_st_chan_sel(VBC_CAPTURE01, AUDIO_CHAN_L);
	else if (val == 1)
		vbc_st_chan_sel(VBC_CAPTURE23, AUDIO_CHAN_L);
	else
		pr_err("%s, invalid val(%d)\n", __func__, val);

	return 0;
}

static int vbc_st1_chnmux_set(int val)
{
	if (val == 0)
		vbc_st_chan_sel(VBC_CAPTURE01, AUDIO_CHAN_R);
	else if (val == 1)
		vbc_st_chan_sel(VBC_CAPTURE23, AUDIO_CHAN_R);
	else
		pr_err("%s, invalid val(%d)\n", __func__, val);

	return 0;
}

static int vbc_st0_inmux_set(int val)
{
	vbc_st_data_inmux_sel(val, AUDIO_CHAN_L);

	return 0;
}

static int vbc_st1_inmux_set(int val)
{
	vbc_st_data_inmux_sel(val, AUDIO_CHAN_R);

	return 0;
}

static int vbc_ad0_inmux_set(int val)
{
	vbc_adc_Inmux_sel(VBC_CAPTURE01, val, AUDIO_CHAN_L);

	return 0;
}

static int vbc_ad1_inmux_set(int val)
{
	vbc_adc_Inmux_sel(VBC_CAPTURE01, val, AUDIO_CHAN_R);

	return 0;
}

static int vbc_ad2_inmux_set(int val)
{
	vbc_adc_Inmux_sel(VBC_CAPTURE23, val, AUDIO_CHAN_L);

	return 0;
}

static int vbc_ad3_inmux_set(int val)
{
	vbc_adc_Inmux_sel(VBC_CAPTURE23, val, AUDIO_CHAN_R);

	return 0;
}

static int vbc_ad0_dgmux_set(int val)
{
	vbc_ad01_data_dgmux_sel(val, AUDIO_CHAN_L);

	return 0;
}

static int vbc_ad1_dgmux_set(int val)
{
	vbc_ad01_data_dgmux_sel(val, AUDIO_CHAN_R);

	return 0;
}

static int vbc_ad2_dgmux_set(int val)
{
	vbc_ad23_data_dgmux_sel(val, AUDIO_CHAN_L);

	return 0;
}

static int vbc_ad3_dgmux_set(int val)
{
	vbc_ad23_data_dgmux_sel(val, AUDIO_CHAN_R);

	return 0;
}

/* da can support 24bit,but ad iis only support 16bit(vbc r1p0_v3) */
static void vbc_da_iis_wd_sel(u32 wd)
{
	u32 bit = 0;

	/* IIS wd en: 0 16bit, 1 24bit */
	bit = BIT_RF_IIS_DAC_WD;

	vbc_reg_update(REG_VBC_VBC_IIS_CTL1, wd ? bit : ~bit, bit);
}

/* iis lr mode:
 * @md
 * 1 : left channel(da01_0) Low, right channel High(da01_1)
 * 0 : left channel High, right channel Low
 */
static void vbc_da_iis_set_lr_md(u32 md)
{
	u32 bit = 0;

	bit = BIT_RF_IIS_DAC_LR_MD;

	vbc_reg_update(REG_VBC_VBC_IIS_CTL1, md ? bit : ~bit, bit);
}

/*
 * @md
 * 1 : left channel Low, right channel High
 * 0 : left channel High, right channel Low
 */
static void vbc_ad_iis_set_lr_md(u32 id, u32 md)
{
	u32 bit = 0;

	switch (id) {
	case VBC_CAPTURE01:
		bit = BIT_RF_IIS_ADC01_LR_MD;
		break;
	case VBC_CAPTURE23:
		bit = BIT_RF_IIS_ADC23_LR_MD;
		break;
	default:
		break;
	}
	vbc_reg_update(REG_VBC_VBC_IIS_CTL1, md ? bit : ~bit, bit);
}

/* da iis switch enable:
 * when switch da iis interface during working
 * first vbc_da_iis_sw_en enable then swich iis
 * last vbc_da_iis_sw_en disable
 */
static void vbc_da_iis_sw_en(u32 id, u32 en)
{
	u32 bit = 0;

	switch (id) {
	case VBC_PLAYBACK01:
	case VBC_PLAYBACK23:
		bit = BIT_RF_IIS_DAC_SW_EN;
		break;
	default:
		break;
	}
	vbc_reg_update(REG_VBC_VBC_IIS_CTL1, en ? bit : ~bit, bit);
}

/* ad iis switch enable */
static void vbc_ad_iis_sw_en(u32 id, u32 en)
{
	u32 bit = 0;

	switch (id) {
	case VBC_CAPTURE01:
		bit = BIT_RF_IIS_ADC01_SW_EN;
		break;
	case VBC_CAPTURE23:
		bit = BIT_RF_IIS_ADC23_SW_EN;
		break;
	default:
		break;
	}
	vbc_reg_update(REG_VBC_VBC_IIS_CTL1, en ? bit : ~bit, bit);
}

/* when ad iis mux set we should
 *operate vbc_ad_iis_sw_en first
 */
static int vbc_ad_iismux_set(int port)
{
	u32 old_val = 0;

	old_val = vbc_reg_read(REG_VBC_VBC_IIS_CTL1);
	if ((old_val & BITS_RF_IIS_ADC01_SEL(0x7)) ==
		BITS_RF_IIS_ADC01_SEL(port))
		return 0;
	pr_info("%s port[%d]", __func__, port);
	vbc_ad_iis_sw_en(VBC_CAPTURE01, 1);
	vbc_reg_update(REG_VBC_VBC_IIS_CTL1,
		       BITS_RF_IIS_ADC01_SEL(port),
		       BITS_RF_IIS_ADC01_SEL(0x7));
	vbc_ad_iis_sw_en(VBC_CAPTURE01, 0);

	return 0;
}

static int vbc_ad23_iismux_set(int port)
{
	u32 old_val = 0;

	old_val = vbc_reg_read(REG_VBC_VBC_IIS_CTL1);
	if ((old_val & BITS_RF_IIS_ADC23_SEL(0x7)) ==
		BITS_RF_IIS_ADC23_SEL(port))
		return 0;
	pr_info("%s port[%d]", __func__, port);

	vbc_ad_iis_sw_en(VBC_CAPTURE23, 1);
	vbc_reg_update(REG_VBC_VBC_IIS_CTL1,
		       BITS_RF_IIS_ADC23_SEL(port),
		       BITS_RF_IIS_ADC23_SEL(0x7));
	vbc_ad_iis_sw_en(VBC_CAPTURE23, 0);

	return 0;
}

static int vbc_da_iismux_set(int port)
{
	u32 old_val = 0;

	old_val = vbc_reg_read(REG_VBC_VBC_IIS_CTL1);
	if ((old_val & BITS_RF_IIS_DAC_SEL(0x7)) ==
		BITS_RF_IIS_DAC_SEL(port))
		return 0;
	pr_info("%s port[%d]", __func__, port);
	vbc_da_iis_sw_en(VBC_PLAYBACK01, 1);
	vbc_reg_update(REG_VBC_VBC_IIS_CTL1,
		       BITS_RF_IIS_DAC_SEL(port),
		       BITS_RF_IIS_DAC_SEL(0x7));
	vbc_da_iis_sw_en(VBC_PLAYBACK01, 0);

	return 0;
}

static int vbc_try_ad_iismux_set(int port);
static int vbc_try_ad23_iismux_set(int port);

static sprd_vbc_mux_set vbc_mux_cfg[SPRD_VBC_MUX_MAX] = {
	vbc_st0_chnmux_set,
	vbc_st1_chnmux_set,
	vbc_st0_inmux_set,
	vbc_st1_inmux_set,
	vbc_ad0_inmux_set,
	vbc_ad1_inmux_set,
	vbc_ad2_inmux_set,
	vbc_ad3_inmux_set,
	vbc_try_ad_iismux_set,
	vbc_try_ad23_iismux_set,
	vbc_da0_addfm_set,
	vbc_da1_addfm_set,
	vbc_da0_addst_set,
	vbc_da1_addst_set
};

static inline int vbc_da0_dg_set(int enable, int dg)
{
	int32_t bit = BIT_RF_DAC0_DG_EN;

	if (enable)
		vbc_reg_update(REG_VBC_VBC_DAC_DG_CTRL,
			       bit | BITS_RF_DAC0_DG_GAIN(dg),
			       bit | BITS_RF_DAC0_DG_GAIN(0x7F));
	else
		vbc_reg_update(REG_VBC_VBC_DAC_DG_CTRL, ~bit, bit);

	return 0;
}

static inline int vbc_da1_dg_set(int enable, int dg)
{
	int32_t bit = BIT_RF_DAC1_DG_EN;

	if (enable)
		vbc_reg_update(REG_VBC_VBC_DAC_DG_CTRL,
			       bit | BITS_RF_DAC1_DG_GAIN(dg),
			       bit | BITS_RF_DAC1_DG_GAIN(0x7F));
	else
		vbc_reg_update(REG_VBC_VBC_DAC_DG_CTRL, ~bit, bit);

	return 0;
}

static inline int vbc_ad0_dg_set(int enable, int dg)
{
	int32_t bit = BIT_RF_ADC0_DG_EN;

	if (enable)
		vbc_reg_update(REG_VBC_VBC_ADC01_DG_CTRL,
			       bit | BITS_RF_ADC0_DG_GAIN(dg),
			       bit | BITS_RF_ADC0_DG_GAIN(0x7F));
	else
		vbc_reg_update(REG_VBC_VBC_ADC01_DG_CTRL, ~bit, bit);

	return 0;
}

static inline int vbc_ad1_dg_set(int enable, int dg)
{
	int32_t bit = BIT_RF_ADC1_DG_EN;

	if (enable)
		vbc_reg_update(REG_VBC_VBC_ADC01_DG_CTRL,
			       bit | BITS_RF_ADC1_DG_GAIN(dg),
			       bit | BITS_RF_ADC1_DG_GAIN(0x7F));
	else
		vbc_reg_update(REG_VBC_VBC_ADC01_DG_CTRL, ~bit, bit);

	return 0;
}

static inline int vbc_ad2_dg_set(int enable, int dg)
{
	int32_t bit = BIT_RF_ADC2_DG_EN;

	if (enable)
		vbc_reg_update(REG_VBC_VBC_ADC23_DG_CTRL,
			       bit | BITS_RF_ADC2_DG_GAIN(dg),
			       bit | BITS_RF_ADC2_DG_GAIN(0x7F));
	else
		vbc_reg_update(REG_VBC_VBC_ADC23_DG_CTRL, ~bit, bit);

	return 0;
}

static inline int vbc_ad3_dg_set(int enable, int dg)
{
	int32_t bit = BIT_RF_ADC3_DG_EN;

	if (enable)
		vbc_reg_update(REG_VBC_VBC_ADC23_DG_CTRL,
			       bit | BITS_RF_ADC3_DG_GAIN(dg),
			       bit | BITS_RF_ADC3_DG_GAIN(0x7F));
	else
		vbc_reg_update(REG_VBC_VBC_ADC23_DG_CTRL, 0, bit);

	return 0;
}

/* side tone dg depends on side tone enabe,
 * hpf enable is not necessary
 */
static inline int vbc_st0_dg_set(int dg)
{
	vbc_reg_update(REG_VBC_VBC_DAC_ST_CTL0,
		       BITS_RF_ST_HPF_DG_0(dg), BITS_RF_ST_HPF_DG_0(0x7F));

	return 0;
}

static inline int vbc_st1_dg_set(int dg)
{
	vbc_reg_update(REG_VBC_VBC_DAC_ST_CTL1,
		       BITS_RF_ST_HPF_DG_1(dg), BITS_RF_ST_HPF_DG_1(0x7F));

	return 0;
}

/* vbc_st0_hpf_set is a high pass filter
 *(remove the low frequency wave)
 */
static inline int vbc_st0_hpf_set(int enable, int hpf_val)
{
	uint32_t bit = BIT_RF_ST_HPF_EN_0;
	uint32_t val = 0;
	uint32_t mask = 0;

	if (enable) {
		val = bit | BITS_RF_ST_HPF_N_0(hpf_val);
		mask = bit | BITS_RF_ST_HPF_N_0(0xF);
		vbc_reg_update(REG_VBC_VBC_DAC_ST_CTL0, val, mask);
	} else {
		val &= ~bit;
		val |= BITS_RF_ST_HPF_N_0(3);
		mask = bit | BITS_RF_ST_HPF_N_0(0xF);
		vbc_reg_update(REG_VBC_VBC_DAC_ST_CTL0, val, mask);
	}

	return 0;
}

static inline int vbc_st1_hpf_set(int enable, int hpf_val)
{
	uint32_t bit = BIT_RF_ST_HPF_EN_1;
	uint32_t val = 0;
	uint32_t mask = 0;

	if (enable) {
		val = bit | BITS_RF_ST_HPF_N_1(hpf_val);
		mask = bit | BITS_RF_ST_HPF_N_1(0xF);
		vbc_reg_update(REG_VBC_VBC_DAC_ST_CTL1, val, mask);
	} else {
		val &= ~bit;
		val |= BITS_RF_ST_HPF_N_1(3);
		mask = bit | BITS_RF_ST_HPF_N_1(0xF);
		vbc_reg_update(REG_VBC_VBC_DAC_ST_CTL1, val, mask);
	}
	return 0;
}

static inline void vbc_da_alc_mode_set(int dp_t_mode)
{
	uint32_t bit = BIT_RF_DAC_ALC_DP_T_MODE;

	vbc_reg_update(REG_VBC_VBC_DAC_HP_CTRL, dp_t_mode ? bit : ~bit, bit);
}

/* add controle */
/* EQ4 pos sel: 0 - eq4 before alc
 *1- eq4 after alc
 */
static inline void vbc_da_eq4_pos_sel(int pos)
{
	uint32_t bit = BIT_RF_DAC_EQ4_POS_SEL;

	vbc_reg_update(REG_VBC_VBC_DAC_HP_CTRL, pos ? bit : ~bit, bit);
}

static int vbc_try_dg_set(struct vbc_codec_priv *vbc_codec, int dg_idx, int id)
{
	struct vbc_dg *p_dg = &vbc_codec->dg[dg_idx];
	int dg = p_dg->dg_val[id];

	if (p_dg->dg_switch[id])
		p_dg->dg_set[id] (true, dg);
	else
		p_dg->dg_set[id] (false, dg);

	return 0;
}

static int vbc_try_st_dg_set(struct vbc_codec_priv *vbc_codec, int id)
{
	struct st_hpf_dg *p_st_dg = &vbc_codec->st_dg;

	if (id == AUDIO_CHAN_L)
		vbc_st0_dg_set(p_st_dg->dg_val[id]);
	else
		vbc_st1_dg_set(p_st_dg->dg_val[id]);
	return 0;
}

static int vbc_try_st_hpf_set(struct vbc_codec_priv *vbc_codec, int id)
{
	struct st_hpf_dg *p_st_dg = &vbc_codec->st_dg;

	if (id == AUDIO_CHAN_L)
		vbc_st0_hpf_set(p_st_dg->hpf_switch[id], p_st_dg->hpf_val[id]);
	else
		vbc_st1_hpf_set(p_st_dg->hpf_switch[id], p_st_dg->hpf_val[id]);
	return 0;
}

static int vbc_try_fm_ad_src_set(struct vbc_codec_priv *vbc_codec,
				 int vbc_ad_src_idx)
{
	struct snd_soc_card *card;
	struct sprd_card_data *mdata = NULL;

	if (!(vbc_codec && vbc_codec->codec)) {
		pr_err("%s failed\n", __func__);
		return -1;
	}
	card = vbc_codec->codec->component.card;
	mdata = snd_soc_card_get_drvdata(card);
	if (mdata == NULL) {
		pr_err("%s mdata is NULL\n", __func__);
		return 0;
	}

	if (mdata->is_fm_open_src)
		vbc_ad_src_set(dfm.hw_rate, vbc_ad_src_idx);
	else
		vbc_ad_src_set(0, vbc_ad_src_idx);

	return 0;
}

static int vbc_try_da_iismux_set(int port)
{
	/* ninglei why port ? (port + 1) : 0 */
	return vbc_da_iismux_set(port ? (port + 1) : 0);
}

static int vbc_try_ad_iismux_set(int port)
{
	return vbc_ad_iismux_set(port);
}

static int vbc_try_ad23_iismux_set(int port)
{
	return vbc_ad23_iismux_set(port);
}

static int vbc_try_ad_dgmux_set(struct vbc_codec_priv *vbc_codec, int id)
{
	switch (id) {
	case ADC0_DGMUX:
		vbc_ad0_dgmux_set(vbc_codec->adc_dgmux_val[ADC0_DGMUX]);
		break;
	case ADC1_DGMUX:
		vbc_ad1_dgmux_set(vbc_codec->adc_dgmux_val[ADC1_DGMUX]);
		break;
	case ADC2_DGMUX:
		vbc_ad2_dgmux_set(vbc_codec->adc_dgmux_val[ADC2_DGMUX]);
		break;
	case ADC3_DGMUX:
		vbc_ad3_dgmux_set(vbc_codec->adc_dgmux_val[ADC3_DGMUX]);
		break;
	default:
		break;
	}
	return 0;
}

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
		return 0;
	}
	return ev_name;
}

static const char *const st_chan_sel_txt[] = {
	"AD01", "AD23"
};

static const char *const st0_sel_txt[] = {
	"AD0(2)ST0", "AD1(3)ST0", "NOINPUT",
};

static const char *const st1_sel_txt[] = {
	"AD1(3)ST1", "AD0(2)ST1", "NOINPUT",
};

static const char *const ad0_inmux_txt[] = {
	"IIS0AD0", "IIS1AD0", "NOINPUT",
};

static const char *const ad1_inmux_txt[] = {
	"IIS1AD1", "IIS0AD1", "NOINPUT",
};

static const char *const ad2_inmux_txt[] = {
	"IIS2AD2", "IIS3AD2", "NOINPUT",
};

static const char *const ad3_inmux_txt[] = {
	"IIS3AD3", "IIS2AD3", "NOINPUT",
};

#define MUX_TO_CODEC "AUDIIS0"
#define MUX_TO_DIGFM "DIGFM"

static const char *const ad_iis_txt[] = {
	MUX_TO_CODEC, MUX_TO_DIGFM, "EXTDIGFM", "EXTIIS6", "AUDIIS1",
};

static const char *const ad23_iis_txt[] = {
	"AUDIIS1", "DIGFM", "EXTDIGFM", "EXTIIS6", "AUDIIS0",
};

static const char *const dfm_dac_st_txt[] = {
	"BYPASS(ST)", "ADD(ST)", "SUBTRACT(ST)",
};

#define SPRD_VBC_ENUM(xreg, xitems, xtexts)\
		SOC_ENUM_SINGLE(FUN_REG(xreg), 0, xitems, xtexts)

static const struct soc_enum vbc_mux_sel_enum[SPRD_VBC_MUX_MAX] = {
	/*ST CHAN MUX */
	SPRD_VBC_ENUM(SPRD_VBC_ST0_CHAN_MUX, 2, st_chan_sel_txt),
	SPRD_VBC_ENUM(SPRD_VBC_ST1_CHAN_MUX, 2, st_chan_sel_txt),
	/*ST INMUX */
	SPRD_VBC_ENUM(SPRD_VBC_ST0_MUX, 3, st0_sel_txt),
	SPRD_VBC_ENUM(SPRD_VBC_ST1_MUX, 3, st1_sel_txt),
	/*AD INMUX */
	SPRD_VBC_ENUM(SPRD_VBC_AD0_INMUX, 3, ad0_inmux_txt),
	SPRD_VBC_ENUM(SPRD_VBC_AD1_INMUX, 3, ad1_inmux_txt),
	SPRD_VBC_ENUM(SPRD_VBC_AD2_INMUX, 3, ad2_inmux_txt),
	SPRD_VBC_ENUM(SPRD_VBC_AD3_INMUX, 3, ad3_inmux_txt),
	/*IIS INMUX */
	SPRD_VBC_ENUM(SPRD_VBC_AD_IISMUX, 5, ad_iis_txt),
	SPRD_VBC_ENUM(SPRD_VBC_AD23_IISMUX, 5, ad23_iis_txt),
	/* DFM DAC INMUX */
	SPRD_VBC_ENUM(SPRD_VBC_DFM_DA0_ADDFM_INMUX, 3, dfm_dac_st_txt),
	SPRD_VBC_ENUM(SPRD_VBC_DFM_DA1_ADDFM_INMUX, 3, dfm_dac_st_txt),
	SPRD_VBC_ENUM(SPRD_VBC_DFM_DA0_ADDST_INMUX, 3, dfm_dac_st_txt),
	SPRD_VBC_ENUM(SPRD_VBC_DFM_DA1_ADDST_INMUX, 3, dfm_dac_st_txt),
};

/* fm use vbc_iis1->SYS_IIS0,
 * codec use aud and configed at pin function.
 * vbc_iis0 is invalid.
 */
static const char *const sys_iis_sel_txt[] = {
	"vbc_iis1", "vbc_iis2", "vbc_iis3",
	"ap_iis0", "ap_iis1", "ap_iis2", "ap_iis3",
	"pubcp_iis0",
	"tgdsp_iis0", "tgdsp_iis1", "tgdsp_iis2", "tgdsp_iis3",
	"wcn_iis0",
};

static const struct soc_enum vbc_sys_iis_enum[SYS_IIS_MAX - SYS_IIS_START] = {
	SPRD_VBC_ENUM(SYS_IIS0, 13, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS1, 13, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS2, 13, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS3, 13, sys_iis_sel_txt),
	SPRD_VBC_ENUM(SYS_IIS4, 13, sys_iis_sel_txt),
};

/* side tone 0, and side tone 1 enable  */
static int vbc_st_module_enable_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	sp_asoc_pr_dbg("%s side tone module clear\n",
		__func__);
	/* after module enabe */
	if (event == SND_SOC_DAPM_POST_PMU) {
		vbc_module_clear(0, SIDE_TONE, AUDIO_CHAN_ALL);
		/* when use side tone, we should clear side tone fifo */
		vbc_module_clear(0, SIDE_TONE_FIFO, AUDIO_CHAN_ALL);
		return 0;
	}

	return 0;
}


static int vbc_chan_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct vbc_codec_priv *vbc_codec = sprd_widget_to_codec_drvdata(w);
	int vbc_idx = FUN_REG(w->reg);
	int chan = w->shift;
	int ret = 0;
	int32_t ad_src_idx = -1;

	sp_asoc_pr_dbg("%s(%s%d) Event is %s\n", __func__,
		       vbc_get_name(vbc_idx), chan, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		vbc_chan_enable(1, vbc_idx, chan);
		ad_src_idx = vbc_idx_to_ad_src_idx(vbc_idx);
		if (ad_src_idx >= 0)
			vbc_try_fm_ad_src_set(vbc_codec, ad_src_idx);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ad_src_idx = vbc_idx_to_ad_src_idx(vbc_idx);
		if (ad_src_idx >= 0)
			vbc_ad_src_set(0, ad_src_idx);
		vbc_chan_enable(0, vbc_idx, chan);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int vbc_power_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		vbc_power(1);
		vbc_sleep_xtl_en(true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		vbc_power(0);
		vbc_sleep_xtl_en(false);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int dfm_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		vbc_try_st_dg_set(vbc_codec, AUDIO_CHAN_L);
		vbc_try_st_dg_set(vbc_codec, AUDIO_CHAN_R);
		if (!p_eq_setting->codec)
			p_eq_setting->codec = codec;
		/*eq setting */
		if (p_eq_setting->is_active[VBC_DA_EQ]
		    && p_eq_setting->data[VBC_DA_EQ])
			vbc_eq_try_apply(codec, VBC_DA_EQ);
		else
			vbc_eq_profile_close(vbc_codec->codec, VBC_DA_EQ);

		vbc_module_clear(0, SIDE_TONE_FIFO, AUDIO_CHAN_ALL);
		vbc_eq_enable(1, vbc_codec);
		vbc_ad_iis_set_lr_md(VBC_CAPTURE01, 1);
		pre_fill_data(true);
		vbc_da01_fifo_enable(AUDIO_CHAN_ALL);
		mutex_lock(&vbc_codec->fm_mutex);
		vbc_fm_mute_smooth(vbc_codec->fm_mute_smooth, vbc_codec);
		mutex_unlock(&vbc_codec->fm_mutex);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mutex_lock(&vbc_codec->fm_mutex);
		vbc_fm_mute_smooth(true, vbc_codec);
		mutex_unlock(&vbc_codec->fm_mutex);
		vbc_eq_enable(0, vbc_codec);
		pre_fill_data(false);
		vbc_da01_fifo_disable(AUDIO_CHAN_ALL);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int aud_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int vbc_idx = FUN_REG(w->reg);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	int ret = 0;
	int32_t vbc_eq_idx = -1;

	sp_asoc_pr_dbg("%s Event is %s\n Chan is %s", __func__,
		       get_event_name(event), vbc_get_name(vbc_idx));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		vbc_eq_idx = vbc_idx_to_eq_idx(vbc_idx);
		if (vbc_eq_idx < 0)
			return -EINVAL;
		/*eq setting */
		if (!p_eq_setting->codec)
			p_eq_setting->codec = codec;
		if (p_eq_setting->is_active[vbc_eq_idx]
		    && p_eq_setting->data[vbc_eq_idx])
			vbc_eq_try_apply(codec, vbc_eq_idx);
		else
			vbc_eq_profile_close(vbc_codec->codec, vbc_eq_idx);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int mux_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct vbc_codec_priv *vbc_codec = sprd_widget_to_codec_drvdata(w);
	unsigned int id = SPRD_VBC_MUX_IDX(FUN_REG(w->reg));
	struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[id]);
	int ret = 0;

	sp_asoc_pr_dbg("%s Set %s(%d) Event is %s\n", __func__,
		       vbc_mux_debug_str[id], mux->val, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mux->set = vbc_mux_cfg[id];
		ret = mux->set(mux->val);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mux->set = 0;
		if (IS_SPRD_VBC_DFM_ST_MUX_RANG(id))
			ret = vbc_mux_cfg[id] (0);
		/*ret = vbc_mux_cfg[id] (0); */
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int sprd_vbc_mux_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    widget_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = SPRD_VBC_MUX_IDX(FUN_REG(e->reg));
	struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[reg]);

	ucontrol->value.enumerated.item[0] = mux->val;

	return 0;
}

static int sprd_vbc_mux_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct vbc_codec_priv *vbc_codec =
	    widget_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = SPRD_VBC_MUX_IDX(FUN_REG(e->reg));
	unsigned int mask = e->mask;
	struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[reg]);
	int ret = 0;

	if (ucontrol->value.enumerated.item[0] >= e->items ||
		ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_info("Set MUX[%s] to %d\n", vbc_mux_debug_str[reg],
			ucontrol->value.enumerated.item[0]);

	/* notice the sequence */
	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

	/* update reg: must be set after
	 * snd_soc_dapm_put_enum_double->change =
	 * snd_soc_test_bits(widget->dapm->component,
	 * e->reg, mask, val);
	 */
	mux->val = (ucontrol->value.enumerated.item[0] & mask);
	if (mux->set)
		ret = mux->set(mux->val);

	if (reg == SPRD_VBC_MUX_IDX(SPRD_VBC_AD_IISMUX)) {
		spin_lock(&vbc_codec->ad01_spinlock);
		if (strcmp(texts->texts[ucontrol->value.integer.value[0]],
			MUX_TO_CODEC) == 0)
			vbc_codec->ad01_to_fm = false;
		else if (strcmp(texts->texts[ucontrol->value.integer.value[0]],
			MUX_TO_DIGFM) == 0)
			vbc_codec->ad01_to_fm = true;
		else {
			vbc_codec->ad01_to_fm = false;
			pr_warn("SPRD_VBC_AD_IISMUX to neither codec nor fm\n");
		}
		spin_unlock(&vbc_codec->ad01_spinlock);
		pr_info("%s vbc_codec->ad01_to_fm=%s", __func__,
			vbc_codec->ad01_to_fm == true ? "true" : "false");
	}

	return ret;
}

#define SPRD_VBC_MUX(xname, xenum) \
	SOC_DAPM_ENUM_EXT(xname, xenum, sprd_vbc_mux_get, sprd_vbc_mux_put)

static const struct snd_kcontrol_new vbc_mux[SPRD_VBC_MUX_MAX] = {
	SPRD_VBC_MUX("ST0 CHAN MUX", vbc_mux_sel_enum[SPRD_VBC_ST0_CHAN_MUX]),
	SPRD_VBC_MUX("ST1 CHAN MUX", vbc_mux_sel_enum[SPRD_VBC_ST1_CHAN_MUX]),
	SPRD_VBC_MUX("ST0 INMUX", vbc_mux_sel_enum[SPRD_VBC_ST0_MUX]),
	SPRD_VBC_MUX("ST1 INMUX", vbc_mux_sel_enum[SPRD_VBC_ST1_MUX]),
	SPRD_VBC_MUX("AD0 INMUX", vbc_mux_sel_enum[SPRD_VBC_AD0_INMUX]),
	SPRD_VBC_MUX("AD1 INMUX", vbc_mux_sel_enum[SPRD_VBC_AD1_INMUX]),
	SPRD_VBC_MUX("AD2 INMUX", vbc_mux_sel_enum[SPRD_VBC_AD2_INMUX]),
	SPRD_VBC_MUX("AD3 INMUX", vbc_mux_sel_enum[SPRD_VBC_AD3_INMUX]),
	SPRD_VBC_MUX("AD IISMUX", vbc_mux_sel_enum[SPRD_VBC_AD_IISMUX]),
	SPRD_VBC_MUX("AD23 IISMUX", vbc_mux_sel_enum[SPRD_VBC_AD23_IISMUX]),
	SPRD_VBC_MUX("DA0 ADDFM MUX",
		     vbc_mux_sel_enum[SPRD_VBC_DFM_DA0_ADDFM_INMUX]),
	SPRD_VBC_MUX("DA1 ADDFM MUX",
		     vbc_mux_sel_enum[SPRD_VBC_DFM_DA1_ADDFM_INMUX]),
	SPRD_VBC_MUX("DA0 ADDST MUX",
		     vbc_mux_sel_enum[SPRD_VBC_DFM_DA0_ADDST_INMUX]),
	SPRD_VBC_MUX("DA1 ADDST MUX",
		     vbc_mux_sel_enum[SPRD_VBC_DFM_DA1_ADDST_INMUX]),
};

#define VBC_DAPM_MUX_E(wname, wreg) \
	SND_SOC_DAPM_MUX_E(wname, FUN_REG(wreg), \
		0, 0, &vbc_mux[wreg], mux_event, \
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD)

static int vbc_loop_switch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    widget_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = SPRD_VBC_SWITCH_IDX(FUN_REG(mc->reg));

	ucontrol->value.integer.value[0] = vbc_codec->vbc_loop_switch[id];

	return 0;
}

static int vbc_loop_switch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec =
	    widget_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = SPRD_VBC_SWITCH_IDX(FUN_REG(mc->reg));

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->vbc_loop_switch[id])
		return ret;

	sp_asoc_pr_info("VBC AD%s LOOP Switch Set %x\n",
			id == 0 ? "01" : "23",
			(int)ucontrol->value.integer.value[0]);

	snd_soc_dapm_put_volsw(kcontrol, ucontrol);

	vbc_codec->vbc_loop_switch[id] = ret;

	return ret;
}

static const struct snd_kcontrol_new vbc_loop_control[] = {
	SOC_SINGLE_EXT("Switch",
		       FUN_REG(VBC_AD01_LOOP_SWITCH),
		       0, 1, 0,
		       vbc_loop_switch_get,
		       vbc_loop_switch_put),
	SOC_SINGLE_EXT("Switch",
		       FUN_REG(VBC_AD23_LOOP_SWITCH),
		       0, 1, 0,
		       vbc_loop_switch_get,
		       vbc_loop_switch_put),
};

static const struct snd_soc_dapm_widget vbc_codec_dapm_widgets[] = {
	/*power */
	SND_SOC_DAPM_SUPPLY("VBC Power", SND_SOC_NOPM, 0, 0,
			    vbc_power_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	/* STchan mux */
	VBC_DAPM_MUX_E("ST0 CHAN MUX", SPRD_VBC_ST0_CHAN_MUX),
	VBC_DAPM_MUX_E("ST1 CHAN MUX", SPRD_VBC_ST1_CHAN_MUX),

	/* ST inmux */
	VBC_DAPM_MUX_E("ST0 INMUX", SPRD_VBC_ST0_MUX),
	VBC_DAPM_MUX_E("ST1 INMUX", SPRD_VBC_ST1_MUX),

	/*ADC inmux */
	VBC_DAPM_MUX_E("AD0 INMUX", SPRD_VBC_AD0_INMUX),
	VBC_DAPM_MUX_E("AD1 INMUX", SPRD_VBC_AD1_INMUX),
	VBC_DAPM_MUX_E("AD2 INMUX", SPRD_VBC_AD2_INMUX),
	VBC_DAPM_MUX_E("AD3 INMUX", SPRD_VBC_AD3_INMUX),

	/*IIS MUX */
	VBC_DAPM_MUX_E("AD IISMUX", SPRD_VBC_AD_IISMUX),
	VBC_DAPM_MUX_E("AD23 IISMUX", SPRD_VBC_AD23_IISMUX),

	/*ST Switch */
	SND_SOC_DAPM_PGA_S("ST0 Switch", 4, SOC_REG(REG_VBC_VBC_DAC_ST_CTL0),
			   ST_EN_0, 0, vbc_st_module_enable_event,
			   SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("ST1 Switch", 4, SOC_REG(REG_VBC_VBC_DAC_ST_CTL1),
			   ST_EN_1, 0, vbc_st_module_enable_event,
			   SND_SOC_DAPM_PRE_PMU),

	/* DFM DAC inmux */
	VBC_DAPM_MUX_E("DA0 ADDFM MUX", SPRD_VBC_DFM_DA0_ADDFM_INMUX),
	VBC_DAPM_MUX_E("DA1 ADDFM MUX", SPRD_VBC_DFM_DA1_ADDFM_INMUX),
	VBC_DAPM_MUX_E("DA0 ADDST MUX", SPRD_VBC_DFM_DA0_ADDST_INMUX),
	VBC_DAPM_MUX_E("DA1 ADDST MUX", SPRD_VBC_DFM_DA1_ADDST_INMUX),

	SND_SOC_DAPM_LINE("DFM", dfm_event),
	/*VBC Chan Switch */
	SND_SOC_DAPM_PGA_S("DA0 Switch", 5, FUN_REG(VBC_PLAYBACK01),
			   AUDIO_CHAN_L, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("DA1 Switch", 5, FUN_REG(VBC_PLAYBACK01),
			   AUDIO_CHAN_R, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("AD0 Switch", 5, FUN_REG(VBC_CAPTURE01),
			   AUDIO_CHAN_L, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("AD1 Switch", 5, FUN_REG(VBC_CAPTURE01),
			   AUDIO_CHAN_R, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("AD2 Switch", 5, FUN_REG(VBC_CAPTURE23),
			   AUDIO_CHAN_L, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("AD3 Switch", 5, FUN_REG(VBC_CAPTURE23),
			   AUDIO_CHAN_R, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*AUD loop var vbc switch */
	SND_SOC_DAPM_PGA_S("Aud input", 4, FUN_REG(VBC_CAPTURE01), 0, 0,
			   aud_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("Aud1 input", 4, FUN_REG(VBC_CAPTURE23), 0, 0,
			   aud_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SWITCH("Aud Loop in VBC", SND_SOC_NOPM, 0, 0,
			    &vbc_loop_control[SPRD_VBC_SWITCH_IDX
					      (VBC_AD01_LOOP_SWITCH)]),
	SND_SOC_DAPM_SWITCH("Aud1 Loop in VBC", SND_SOC_NOPM, 0, 0,
			    &vbc_loop_control[SPRD_VBC_SWITCH_IDX
					      (VBC_AD23_LOOP_SWITCH)]),
};

/* sprd_vbc supported interconnection*/
static const struct snd_soc_dapm_route vbc_codec_intercon[] = {
	/************************power********************************/
	/* digital fm playback need to open DA Clk and DA power */
	{"DFM", NULL, "VBC Power"},	/*ninglei to check */

	/********************** capture in  path in vbc ********************/
	/* AD input route */
	{"Aud Loop in VBC", "Switch", "Aud input"},

	{"Aud1 Loop in VBC", "Switch", "Aud1 input"},

	/* AD01 */
	{"AD IISMUX", "DIGFM", "Dig FM Jack"},

	{"AD IISMUX", "EXTDIGFM", "Dig FM Jack"},

	{"AD IISMUX", "AUDIIS0", "Aud Loop in VBC"},

	/* AD23 */
	{"AD23 IISMUX", "DIGFM", "Dig FM Jack"},
	{"AD23 IISMUX", "EXTDIGFM", "Dig FM Jack"},
	{"AD23 IISMUX", "AUDIIS1", "Aud1 Loop in VBC"},

	{"AD0 INMUX", "IIS0AD0", "AD IISMUX"},
	{"AD0 INMUX", "IIS1AD0", "AD IISMUX"},
	{"AD1 INMUX", "IIS1AD1", "AD IISMUX"},
	{"AD1 INMUX", "IIS0AD1", "AD IISMUX"},
	{"AD2 INMUX", "IIS2AD2", "AD23 IISMUX"},
	{"AD2 INMUX", "IIS3AD2", "AD23 IISMUX"},
	{"AD3 INMUX", "IIS3AD3", "AD23 IISMUX"},
	{"AD3 INMUX", "IIS2AD3", "AD23 IISMUX"},

	{"AD0 Switch", NULL, "AD0 INMUX"},
	{"AD1 Switch", NULL, "AD1 INMUX"},
	{"AD2 Switch", NULL, "AD2 INMUX"},
	{"AD3 Switch", NULL, "AD3 INMUX"},

	/********************** fm playback route ********************/
	/*ST route */
	{"ST0 CHAN MUX", "AD01", "AD0 Switch"},
	{"ST0 CHAN MUX", "AD01", "AD1 Switch"},
	{"ST0 CHAN MUX", "AD23", "AD2 Switch"},
	{"ST0 CHAN MUX", "AD23", "AD3 Switch"},
	{"ST1 CHAN MUX", "AD01", "AD0 Switch"},
	{"ST1 CHAN MUX", "AD01", "AD1 Switch"},
	{"ST1 CHAN MUX", "AD23", "AD2 Switch"},
	{"ST1 CHAN MUX", "AD23", "AD3 Switch"},

	{"ST0 INMUX", "AD0(2)ST0", "ST0 CHAN MUX"},
	{"ST0 INMUX", "AD1(3)ST0", "ST0 CHAN MUX"},
	{"ST1 INMUX", "AD1(3)ST1", "ST1 CHAN MUX"},
	{"ST1 INMUX", "AD0(2)ST1", "ST1 CHAN MUX"},

	{"ST0 Switch", NULL, "ST0 INMUX"},
	{"ST1 Switch", NULL, "ST1 INMUX"},
	{"DA0 Switch", NULL, "ST0 Switch"},
	{"DA1 Switch", NULL, "ST1 Switch"},
	{"DA0 ADDFM MUX", "ADD(ST)", "DA0 Switch"},
	{"DA0 ADDFM MUX", "BYPASS(ST)", "DA0 Switch"},
	{"DA0 ADDFM MUX", "SUBTRACT(ST)", "DA0 Switch"},
	{"DA1 ADDFM MUX", "ADD(ST)", "DA1 Switch"},
	{"DA1 ADDFM MUX", "BYPASS(ST)", "DA1 Switch"},
	{"DA1 ADDFM MUX", "SUBTRACT(ST)", "DA1 Switch"},
	{"DA0 ADDST MUX", "ADD(ST)", "DA0 Switch"},
	{"DA0 ADDST MUX", "BYPASS(ST)", "DA0 Switch"},
	{"DA0 ADDST MUX", "SUBTRACT(ST)", "DA0 Switch"},
	{"DA1 ADDST MUX", "ADD(ST)", "DA1 Switch"},
	{"DA1 ADDST MUX", "BYPASS(ST)", "DA1 Switch"},
	{"DA1 ADDST MUX", "SUBTRACT(ST)", "DA1 Switch"},
	{"DFM", NULL, "DA0 ADDFM MUX"},
	{"DFM", NULL, "DA1 ADDFM MUX"},
	{"DFM", NULL, "DA0 ADDST MUX"},
	{"DFM", NULL, "DA1 ADDST MUX"},
};

int vbc_component_startup(int vbc_idx, struct snd_soc_dai *dai)
{
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	int32_t vbc_eq_idx = -1;
	int32_t vbc_dg_idx = -1;

	sp_asoc_pr_dbg("%s %s\n", __func__, vbc_get_name(vbc_idx));

	switch (vbc_idx) {
	case VBC_PLAYBACK01:
	case VBC_PLAYBACK23:
		vbc_try_da_iismux_set(vbc_codec->vbc_da_iis_port);
		vbc_mixer_sel_set(vbc_codec, VBC_MIXER_DAC);
		break;
	case VBC_CAPTURE01:
		vbc_try_ad_dgmux_set(vbc_codec, ADC0_DGMUX);
		vbc_try_ad_dgmux_set(vbc_codec, ADC1_DGMUX);
		vbc_mixer_sel_set(vbc_codec, VBC_MIXER_ST);

		break;
	case VBC_CAPTURE23:
		vbc_try_ad23_iismux_set(vbc_codec->sprd_vbc_mux
					[SPRD_VBC_AD23_IISMUX].val);
		vbc_try_ad_dgmux_set(vbc_codec, ADC2_DGMUX);
		vbc_try_ad_dgmux_set(vbc_codec, ADC3_DGMUX);
		vbc_mixer_sel_set(vbc_codec, VBC_MIXER_ST);
		break;
	}

	vbc_eq_idx = vbc_idx_to_eq_idx(vbc_idx);
	if (vbc_eq_idx < 0)
		return -EINVAL;
	if (p_eq_setting->is_active[vbc_eq_idx]
	    && p_eq_setting->data[vbc_eq_idx]) {
		pr_info("%s line[%d] active vbc_eq_idx[%d]\n",
			__func__, __LINE__, vbc_eq_idx);
		vbc_eq_try_apply(vbc_codec->codec, vbc_eq_idx);
	} else {
		pr_info("%s line[%d] inactive use default vbc_eq_idx[%d] eq select idx=%d\n",
			__func__, __LINE__,
			vbc_eq_idx, p_eq_setting->now_profile[vbc_eq_idx]);
		vbc_eq_profile_close(vbc_codec->codec, vbc_eq_idx);
	}

	vbc_da_alc_mode_set(vbc_codec->alc_dp_t_mode);
	vbc_dg_idx = vbc_idx_to_dg_idx(vbc_idx);
	if (vbc_dg_idx >= 0) {
		vbc_try_dg_set(vbc_codec, vbc_dg_idx, AUDIO_CHAN_L);
		vbc_try_dg_set(vbc_codec, vbc_dg_idx, AUDIO_CHAN_R);
	}

	return 0;
}

#ifdef CONFIG_PROC_FS
static void vbc_proc_read(struct snd_info_entry *entry,
			  struct snd_info_buffer *buffer)
{
	int reg;

	snd_iprintf(buffer, "vbc register dump\n");
	for (reg = ARM_VB_BASE + 0x10; reg < ARM_VB_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (reg - ARM_VB_BASE)
			    , vbc_reg_read(reg + 0x00)
			    , vbc_reg_read(reg + 0x04)
			    , vbc_reg_read(reg + 0x08)
			    , vbc_reg_read(reg + 0x0C)
		    );
	}
}
static void sprd_vbc_proc_write(struct snd_info_entry *entry,
					 struct snd_info_buffer *buffer)
{
	char line[64] = { 0 };
	unsigned int val = 0;
	unsigned int reg = 0;
	unsigned int old = 0;

	while (!snd_info_get_line(buffer, line, sizeof(line))) {
		if (sscanf(line, "%x %x", &reg, &val) != 2)
			continue;
		vbc_reg_raw_write(reg + VBC_BASE, val);
		old = vbc_reg_read(reg + VBC_BASE);
		pr_info("reg[%x]:val[%x], read=%x\n",
			reg, val, old);
	}

}

static void vbc_proc_init(struct snd_soc_codec *codec)
{
	struct snd_info_entry *entry;
	struct snd_card *card = codec->component.card->snd_card;

	if (!snd_card_proc_new(card, "vbc", &entry))
		snd_info_set_text_ops(entry, NULL, vbc_proc_read);
	entry->c.text.write = sprd_vbc_proc_write;
	entry->mode |= S_IWUSR;
	entry->mode |= S_IRUSR;
}
#else /* !CONFIG_PROC_FS */
static inline void vbc_proc_init(struct snd_soc_codec *codec)
{
}
#endif

static int vbc_da_eq_reg_offset(u32 reg)
{
	int i = 0;

	if (reg == REG_VBC_VBC_DAC_PATH_CTRL) {
		i = 0;
	} else if ((reg >= REG_VBC_VBC_DAC_HP_CTRL) &&
		   (reg <= REG_VBC_VBC_DAC_ST_CTL1)) {
		i = ((reg - REG_VBC_VBC_DAC_HP_CTRL) >> 2) + 1;
	} else if ((reg >= REG_VBC_VBC_DAC_SRC_CTRL) &&
		   (reg <= REG_VBC_VBC_MIXER_CTRL)) {
		i = ((reg - REG_VBC_VBC_DAC_SRC_CTRL) >> 2) +
		    ((REG_VBC_VBC_DAC_ST_CTL1 - REG_VBC_VBC_DAC_HP_CTRL) >> 2) +
		    2;
	} else if ((reg >= REG_VBC_VBC_DAC_NGC_VTHD)
		   && (reg <= REG_VBC_VBC_DAC_NGC_CTRL)) {
		i = ((reg - REG_VBC_VBC_DAC_NGC_VTHD) >> 2) +
		    ((REG_VBC_VBC_MIXER_CTRL - REG_VBC_VBC_DAC_SRC_CTRL) >> 2) +
		    ((REG_VBC_VBC_DAC_ST_CTL1 - REG_VBC_VBC_DAC_HP_CTRL) >> 2) +
		    3;
	} else if ((reg >= REG_VBC_VBC_DAC_EQ6_COEF0_H)
		   && (reg <= REG_VBC_VBC_DAC_EQ4_COEF28_L)) {
		i = ((reg - REG_VBC_VBC_DAC_EQ6_COEF0_H) >> 2) +
		    ((REG_VBC_VBC_DAC_NGC_CTRL -
		      REG_VBC_VBC_DAC_NGC_VTHD) >> 2) +
		    ((REG_VBC_VBC_MIXER_CTRL - REG_VBC_VBC_DAC_SRC_CTRL) >> 2) +
		    ((REG_VBC_VBC_DAC_ST_CTL1 - REG_VBC_VBC_DAC_HP_CTRL) >> 2) +
		    4;
	}
	WARN_ON(i >= VBC_DA_EFFECT_PARAS_LEN);
	return i;
}

static int vbc_ad_eq_reg_offset(u32 reg)
{
	int i = 0;

	if (reg == REG_VBC_VBC_ADC_PATH_CTRL)
		i = 0;
	else if (reg == REG_VBC_VBC_ADC_EQ_CTRL)
		i = 1;
	else if ((reg >= REG_VBC_VBC_ADC01_EQ6_COEF0_H) &&
		 (reg <= REG_VBC_VBC_ADC01_EQ6_COEF42_L))
		i = ((reg - REG_VBC_VBC_ADC01_EQ6_COEF0_H) >> 2) + 2;
	else if ((reg >= REG_VBC_VBC_ADC23_EQ6_COEF0_H) &&
		 (reg <= REG_VBC_VBC_ADC23_EQ6_COEF42_L))
		i = ((reg - REG_VBC_VBC_ADC23_EQ6_COEF0_H) >> 2) + 2;
	WARN_ON(i >= VBC_AD_EFFECT_PARAS_LEN);

	return i;
}

static inline void vbc_da_eq_reg_set(u32 reg, void *data)
{
	u32 *effect_paras = (u32 *) data;
	u32 val = 0;
	u32 old_val = 0;

	val = effect_paras[vbc_da_eq_reg_offset(reg)];
	if (reg == REG_VBC_VBC_DAC_HP_CTRL) {
		/*EQ set */
		/* iis width substitution is vbc_da_iis_wd_sel.
		 * iis width is no longer in REG_VBC_VBC_DAC_HP_CTRL
		 */
		old_val = vbc_reg_read(REG_VBC_VBC_DAC_HP_CTRL);
		vbc_reg_update(REG_VBC_VBC_DAC_HP_CTRL,
			       val, 0xFF);
		/* clear alc moudle */
		if (!(old_val & BIT_RF_DAC_ALC_EN) &&
			(val & BIT_RF_DAC_ALC_EN)) {
			vbc_reg_update(REG_VBC_VBC_MODULE_CLR,
				BIT_RF_DAC_ALC_CLR, BIT_RF_DAC_ALC_CLR);
			pr_info("clear alc module %s\n", __func__);
		}
		/* clear eq6 moudle */
		if (!(old_val & BIT_RF_DAC_EQ6_EN) &&
			(val & BIT_RF_DAC_EQ6_EN)) {
			vbc_reg_update(REG_VBC_VBC_MODULE_CLR,
				BIT_RF_DAC_EQ6_CLR, BIT_RF_DAC_EQ6_CLR);
			pr_info("clear eq6 module %s\n", __func__);
		}
	} else
		vbc_reg_write(reg, val);
}

static inline void vbc_da_eq_reg_set_range(u32 reg_start, u32 reg_end,
					   void *data)
{
	u32 reg_addr;

	for (reg_addr = reg_start; reg_addr <= reg_end; reg_addr += 4)
		vbc_da_eq_reg_set(reg_addr, data);
}

static inline void vbc_ad_eq_reg_set(u32 reg, void *data)
{
	u32 *effect_paras = (u32 *) data;

	if (reg == REG_VBC_VBC_ADC_EQ_CTRL) {
		vbc_reg_update(REG_VBC_VBC_ADC_EQ_CTRL,
			       effect_paras[vbc_da_eq_reg_offset(reg)], 0xC0);
	} else {
		vbc_reg_write(reg, effect_paras[vbc_ad_eq_reg_offset(reg)]);
	}
}

static inline void vbc_ad_eq_reg_set_range(u32 reg_start, u32 reg_end,
					   void *data)
{
	u32 reg_addr;

	for (reg_addr = reg_start; reg_addr <= reg_end; reg_addr += 4)
		vbc_ad_eq_reg_set(reg_addr, data);
}

/*gray code change*/
static void gray_dir(unsigned int *bit, int dir)
{
	if (dir)
		*bit >>= 1;
	else
		*bit <<= 1;
}

static int gray(u32 reg, int from, int to, int (*step_action) (int, int))
{
	int start_bit = (from > to) ? 31 : 0;
	unsigned int bit = (1 << start_bit);
	int r = from;
	int diff = (from ^ to);
	int i;

	for (i = 0; i < 32; i++, gray_dir(&bit, start_bit)) {
		if ((diff & bit)) {
			r &= ~bit;
			r |= (bit & to);
			step_action(reg, r);
		}
	}
	return 0;
}

static int step_action_set_reg(int reg, int r)
{
	vbc_reg_write(reg, r);
	udelay(10);
	return 0;
}

static void gray_set_reg(u32 reg, int from, int to)
{
	sp_asoc_pr_dbg("gray set reg(0x%x) = (0x%x)  from (0x%x))\n",
		       reg, to, from);
	gray(reg, from, to, step_action_set_reg);
}

static void vbc_eq_iir_ab_set_data(u32 reg_addr, void *data)
{
	vbc_da_eq_reg_set(reg_addr + 0x30, data);	/*a2_H */
	vbc_da_eq_reg_set(reg_addr + 0x34, data);	/*a2_L */
	vbc_da_eq_reg_set(reg_addr + 0x20, data);	/*a1_H */
	vbc_da_eq_reg_set(reg_addr + 0x24, data);	/*a1_L */
	vbc_da_eq_reg_set(reg_addr + 0x10, data);	/*a0_H */
	vbc_da_eq_reg_set(reg_addr + 0x14, data);	/*a0_L */
	vbc_da_eq_reg_set(reg_addr + 0x28, data);	/*b2_H */
	vbc_da_eq_reg_set(reg_addr + 0x2C, data);	/*b2_L */
	vbc_da_eq_reg_set(reg_addr + 0x18, data);	/*b1_H */
	vbc_da_eq_reg_set(reg_addr + 0x1C, data);	/*b1_L */
	vbc_da_eq_reg_set(reg_addr + 0x8, data);	/*b0_H */
	vbc_da_eq_reg_set(reg_addr + 0xC, data);	/*b0_L */
}

static void vbc_eq_iir_s_set_data(u32 reg_addr, void *data)
{
	u32 *effect_paras = (u32 *) data;

	gray_set_reg(reg_addr, 0, effect_paras[vbc_da_eq_reg_offset(reg_addr)]);
	gray_set_reg(reg_addr + 0x4, 0,
		     effect_paras[vbc_da_eq_reg_offset(reg_addr + 0x4)]);
}

static void vbc_da_alc_reg_set(void *data, struct vbc_codec_priv *vbc_codec)
{
	u32 *effect_paras = (u32 *) data;
	u32 reg_addr = 0;
	u32 reg_start = 0;
	u32 reg_addr_max = 0;
	u32 val = 0;
	u32 old_val = 0;
	u32 val_hp = 0;

	if (vbc_codec->dynamic_eq_support == 1) {
		reg_start = REG_VBC_VBC_DAC_HP_CTRL;
		reg_addr_max = REG_VBC_VBC_DAC_ALC_CTRL10;
		old_val = vbc_reg_read(REG_VBC_VBC_DAC_HP_CTRL);
		val_hp = effect_paras[vbc_da_eq_reg_offset(reg_start)];
	} else {
		reg_start = REG_VBC_VBC_DAC_ALC_CTRL0;
		reg_addr_max = REG_VBC_VBC_DAC_ALC_CTRL10;
	}

	for (reg_addr = reg_start; reg_addr <= reg_addr_max; reg_addr += 4) {
		val = vbc_reg_read(reg_addr);
		gray_set_reg(reg_addr, val,
			     effect_paras[vbc_da_eq_reg_offset(reg_addr)]);
	}
	if (reg_start == REG_VBC_VBC_DAC_HP_CTRL) {
		/* clear alc moudle */
		if (!(old_val & BIT_RF_DAC_ALC_EN) &&
			(val_hp & BIT_RF_DAC_ALC_EN)) {
			vbc_reg_update(REG_VBC_VBC_MODULE_CLR,
				BIT_RF_DAC_ALC_CLR, BIT_RF_DAC_ALC_CLR);
			pr_debug("clear alc module %s\n", __func__);
		}
		/* clear eq6 moudle */
		if (!(old_val & BIT_RF_DAC_EQ6_EN) &&
			(val_hp & BIT_RF_DAC_EQ6_EN)) {
			vbc_reg_update(REG_VBC_VBC_MODULE_CLR,
				BIT_RF_DAC_EQ6_CLR, BIT_RF_DAC_EQ6_CLR);
			pr_debug("clear eq6 module %s\n", __func__);
		}
	}
}

#define VBC_6BAND_IIR_HPCOEF_MAX 0xffff
#define FADE_TIME 50
#define FADE_STEP_COUNT 100
#define FADE_GAIN_STEP 15

static int vbc_da_eq_fade_out(void)
{

	u32 cur_val = 0;
	u32 step = 0;

	cur_val = (u32)vbc_reg_read(REG_VBC_VBC_DAC_EQ6_COEF42_H);
	if (cur_val > VBC_6BAND_IIR_HPCOEF_MAX) {
		pr_warn("invalid val %#x\n", cur_val);
		cur_val = VBC_6BAND_IIR_HPCOEF_MAX;
	}

	step = (cur_val + (FADE_STEP_COUNT >> 1)) / FADE_STEP_COUNT;
	/* when step < FADE_GAIN_STEP, delay < 100 times*/
	if (step < FADE_GAIN_STEP)
		step = FADE_GAIN_STEP;

	pr_info("%s cur_val=0x%x, step %#x,\n",
		__func__, cur_val, step);
	while (cur_val > step) {
		vbc_reg_write(REG_VBC_VBC_DAC_EQ6_COEF42_H, cur_val);
		/*usleep_range(time, time + 50);*/
		udelay(FADE_TIME);
		cur_val -= step;
	}
	vbc_reg_write(REG_VBC_VBC_DAC_EQ6_COEF42_H, 0);

	return 1;
}

static int vbc_da_eq_fade_in(void *data)
{
	u32 to_val = 0;
	u32 step = 0;
	u32 cur_val = 0;
	u32 *effect_paras;

	effect_paras = (u32 *)data;
	to_val = effect_paras[vbc_da_eq_reg_offset(
		REG_VBC_VBC_DAC_EQ6_COEF42_H)];
	if (to_val > VBC_6BAND_IIR_HPCOEF_MAX) {
		pr_warn("invalid to_val_real %#x\n", to_val);
		to_val = VBC_6BAND_IIR_HPCOEF_MAX;
	}

	cur_val = (u32)vbc_reg_read(REG_VBC_VBC_DAC_EQ6_COEF42_H);
	if (cur_val > VBC_6BAND_IIR_HPCOEF_MAX) {
		pr_warn("invalid cur_val %#x\n", cur_val);
		cur_val = VBC_6BAND_IIR_HPCOEF_MAX;
	}

	step = ((to_val - cur_val) + (FADE_STEP_COUNT >> 1)) / FADE_STEP_COUNT;
	if (step < FADE_GAIN_STEP)
		step = FADE_GAIN_STEP;

	pr_info("%s cur_val=%#x, to_val=%#x, data=0x%p, effect_paras=0x%p, step=%#x\n",
		__func__, cur_val, to_val,
		data, effect_paras, step);
	while (cur_val < to_val) {
		vbc_reg_write(REG_VBC_VBC_DAC_EQ6_COEF42_H, cur_val);
		/*usleep_range(time, time + 50);*/
		udelay(FADE_TIME);
		cur_val += step;
	}

	vbc_reg_write(REG_VBC_VBC_DAC_EQ6_COEF42_H, to_val);

	return 1;
}

/*
 * Process for the competition of kcontrol "VBC xxx Mixer xxx Sel" and
 * eq setting of mixer ctrl.
 */
static void vbc_eq_reg_mixerctl_process(struct vbc_codec_priv *vbc_codec,
					u32 *data)
{
	u32 val = data[vbc_da_eq_reg_offset(REG_VBC_VBC_MIXER_CTRL)];
	struct device *dev = vbc_codec->codec->dev;
	unsigned int (*mux_sel)[CHAN_IDX_MAX] = vbc_codec->mixer_mux_sel;
	unsigned int (*out_sel)[CHAN_IDX_MAX] = vbc_codec->mixer_out_sel;
	unsigned int sel;

	sel = mux_sel[VBC_MIXER_DAC][CHAN_IDX_0];
	if (sel != MIXER_MUX_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for dac0 mixer mux sel.\n",
			 sel);
		val &= ~(BITS_RF_DAC0_MIXER_MUX_SEL(0x7));
		val |= BITS_RF_DAC0_MIXER_MUX_SEL(sel);
	}
	sel = mux_sel[VBC_MIXER_DAC][CHAN_IDX_1];
	if (sel != MIXER_MUX_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for dac1 mixer mux sel.\n",
			 sel);
		val &= ~(BITS_RF_DAC1_MIXER_MUX_SEL(0x7));
		val |= BITS_RF_DAC1_MIXER_MUX_SEL(sel);
	}
	sel = mux_sel[VBC_MIXER_ST][CHAN_IDX_0];
	if (sel != MIXER_MUX_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for st0 mixer mux sel.\n",
			 sel);
		val &= ~(BITS_RF_ST_MIXER_MUX_SEL_0(0x7));
		val |= BITS_RF_ST_MIXER_MUX_SEL_0(sel);
	}
	sel = mux_sel[VBC_MIXER_ST][CHAN_IDX_1];
	if (sel != MIXER_MUX_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for st1 mixer mux sel.\n",
			 sel);
		val &= ~(BITS_RF_ST_MIXER_MUX_SEL_1(0x7));
		val |= BITS_RF_ST_MIXER_MUX_SEL_1(sel);
	}
	sel = out_sel[VBC_MIXER_DAC][CHAN_IDX_0];
	if (sel != MIXER_OUT_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for dac0 mixer out sel.\n",
			 sel);
		val &= ~(BIT_RF_DAC0_MIXER_OUT_SEL);
		if (sel)
			val |= BIT_RF_DAC0_MIXER_OUT_SEL;
	}
	sel = out_sel[VBC_MIXER_DAC][CHAN_IDX_1];
	if (sel != MIXER_OUT_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for dac1 mixer out sel.\n",
			 sel);
		val &= ~(BIT_RF_DAC1_MIXER_OUT_SEL);
		if (sel)
			val |= BIT_RF_DAC1_MIXER_OUT_SEL;
	}
	sel = out_sel[VBC_MIXER_ST][CHAN_IDX_0];
	if (sel != MIXER_OUT_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for st0 mixer out sel.\n",
			 sel);
		val &= ~(BIT_RF_ST_MIXER_OUT_SEL_0);
		if (sel)
			val |= BIT_RF_ST_MIXER_OUT_SEL_0;
	}
	sel = out_sel[VBC_MIXER_ST][CHAN_IDX_1];
	if (sel != MIXER_OUT_BY_EQ) {
		dev_info(dev, "Use kcontrol setting(%u) for st1 mixer out sel.\n",
			 sel);
		val &= ~(BIT_RF_ST_MIXER_OUT_SEL_1);
		if (sel)
			val |= BIT_RF_ST_MIXER_OUT_SEL_1;
	}

	dev_dbg(dev, "%s, mixerctl: %#x\n", __func__, val);

	vbc_reg_write(REG_VBC_VBC_MIXER_CTRL, val);
}

static void vbc_eq_reg_apply(struct snd_soc_codec *codec, void *data,
			     int vbc_eq_idx)
{
	u32 reg;
	void *effect_paras;
	int val;
	struct vbc_codec_priv *vbc_codec = NULL;

	vbc_codec = snd_soc_codec_get_drvdata(codec);
	if (vbc_codec == NULL) {
		pr_err("%s %d snd_soc_codec_get_drvdata failed\n",
		       __func__, __LINE__);
		return;
	}
	pr_info("%s %d vbc_eq_idx %d\n", __func__, __LINE__, vbc_eq_idx);
	if (vbc_eq_idx == VBC_DA_EQ) {
		val = ((u32 *)
		       data)[vbc_da_eq_reg_offset(REG_VBC_VBC_DAC_HP_CTRL)];
		/*DA EQ6 set */
		/* down grade */
		vbc_da_eq_fade_out();
		pr_info("vbc_da_eq_fade_out out\n");

		/*iir state clear */
		vbc_reg_update(REG_VBC_VBC_DAC_HP_CTRL,
			       BIT_RF_DAC_EQ6_REG_CLR, BIT_RF_DAC_EQ6_REG_CLR);
		/* dynamic_eq_support should be zero , otherwise
		 * if eq6 or alc enable,we will alc or eq6 clear which
		 * will generate pop when playing
		 */
		/*a,b set */
		if (vbc_codec->dynamic_eq_support == 0) {
			pr_info("eq6 not dynamic eq val=%#x\n", val);
			if (val & BIT_RF_DAC_EQ6_EN) {
				pr_info("%s set eq\n", __func__);
				effect_paras = data;
			} else {
				pr_info("%s default eq\n", __func__);
				effect_paras = (void *)&vbc_da_eq_profile_default;
			}
		} else {
			pr_info("eq6 dynamic eq %s line[%d]\n",
				 __func__, __LINE__);
			effect_paras = data;
		}

		for (reg = REG_VBC_VBC_DAC_EQ6_COEF35_H;
		     reg >= REG_VBC_VBC_DAC_EQ6_COEF0_H; reg -= 0x38)
			vbc_eq_iir_ab_set_data(reg, effect_paras);
		/*s0-s6 set */
		/* note: REG_VBC_VBC_DAC_EQ6_COEF42_H (gain) do not set */
		for (reg = REG_VBC_VBC_DAC_EQ6_COEF0_H;
		     reg < REG_VBC_VBC_DAC_EQ6_COEF42_H; reg += 0x38)
			vbc_eq_iir_s_set_data(reg, effect_paras);
		vbc_da_eq_reg_set(REG_VBC_VBC_DAC_HP_CTRL, data);

		/*ALC set */
		if (vbc_codec->dynamic_eq_support == 0) {
			if (val & BIT_RF_DAC_ALC_EN)
				effect_paras = data;
			else
				effect_paras = (void *)&vbc_da_eq_profile_default;
		}
		vbc_da_alc_reg_set(effect_paras, vbc_codec);

		/*other */
		vbc_da_eq_reg_set(REG_VBC_VBC_DAC_SRC_CTRL, data);
		vbc_eq_reg_mixerctl_process(vbc_codec, data);
		vbc_da_eq_reg_set_range(REG_VBC_VBC_DAC_NGC_VTHD,
					REG_VBC_VBC_DAC_NGC_CTRL, data);

		/* up grade */
		vbc_da_eq_fade_in(data);
		pr_info("vbc_da_eq_fade_in out\n");
	} else
		/*ignore adc eq6 */
		;
	pr_info("%s returned\n", __func__);
}

static void vbc_eq_profile_apply(struct snd_soc_codec *codec, void *data,
				 int vbc_eq_idx)
{
	if (codec) {
		vbc_eb_set();
		vbc_eq_reg_apply(codec, data, vbc_eq_idx);
		vbc_eb_clear();
	}
}

static void vbc_eq_profile_close(struct snd_soc_codec *codec, int vbc_eq_idx)
{
	pr_info("%s default eq vbc_eq_idx[%d]\n", __func__, vbc_eq_idx);
	switch (vbc_eq_idx) {
	case VBC_DA_EQ:
		vbc_eq_profile_apply(codec,
				     (void *)&vbc_da_eq_profile_default, vbc_eq_idx);
		break;
	case VBC_AD01_EQ:
	case VBC_AD23_EQ:
		vbc_eq_profile_apply(codec,
				     (void *)&vbc_ad_eq_profile_default, vbc_eq_idx);
		break;
	default:
		break;
	}
}

/* sharkl2 not used adc eq6 and dac eq4
 * static int vbc_enable(int enable) only do da eq6
 * so we remove vbc_enable, subsitute with vbc_eq_enable.
 */
static DEFINE_SPINLOCK(vbc_eq_en_lock);
static int vbc_eq_enable(int enable, struct vbc_codec_priv *vbc_codec)
{
	atomic_t *vbc_eq_on = &vbc_refcnt.vbc_eq_on;

	spin_lock(&vbc_eq_en_lock);
	if (enable) {
		atomic_inc(vbc_eq_on);
		if (atomic_read(vbc_eq_on) == 1) {
			if (vbc_codec->dynamic_eq_support == 0) {
				vbc_eq_try_apply(vbc_codec->codec, VBC_DA_EQ);
				vbc_da_eq6_enable(1);
				vbc_da_alc_enable(1);
				pr_info("VBC eq Enable\n");
			}
		}
	} else {
		if (atomic_dec_and_test(vbc_eq_on)) {
			if (vbc_codec->dynamic_eq_support == 0) {
				vbc_da_eq6_enable(0);
				vbc_da_alc_enable(0);
				pr_info("VBC eq Disable\n");
			}
		}
		if (atomic_read(vbc_eq_on) < 0)
			atomic_set(vbc_eq_on, 0);
	}
	spin_unlock(&vbc_eq_en_lock);

	pr_info("VBC eq EN REF: %d\n", atomic_read(vbc_eq_on));

	return 0;
}

static void vbc_eq_try_apply(struct snd_soc_codec *codec, int vbc_eq_idx)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	u32 *data;
	struct vbc_da_eq_profile *now_da = NULL;
	struct vbc_ad_eq_profile *now_ad = NULL;
	int eq_now_idx = 0;

	pr_info("%s %p\n", __func__, p_eq_setting->vbc_eq_apply);
	if (p_eq_setting->vbc_eq_apply) {
		spin_lock(&vbc_codec->lock_eq_idx);
		if (vbc_eq_idx == VBC_DA_EQ) {
			eq_now_idx = p_eq_setting->now_profile[VBC_DA_EQ];
			if (eq_now_idx == -1 ||
				(p_eq_setting->is_loaded == 0)) {
				pr_info("%s %d default da eq, eq_now_idx=%d, is_loaded=%d\n",
					__func__, __LINE__,
					eq_now_idx, p_eq_setting->is_loaded);
				vbc_eq_profile_close(codec, VBC_DA_EQ);
				spin_unlock(&vbc_codec->lock_eq_idx);
				return;
			}
			now_da =
				&(((struct vbc_da_eq_profile *)
				(p_eq_setting->data[VBC_DA_EQ]))
				[eq_now_idx]);
			data = now_da->effect_paras;
			sp_asoc_pr_info("line[%d],VBC %s EQ Apply\n", __LINE__,
					vbc_get_eq_name(vbc_eq_idx));
		} else {
			eq_now_idx = p_eq_setting->now_profile[vbc_eq_idx];
			if (eq_now_idx == -1 ||
				 (p_eq_setting->is_loaded == 0)) {
				pr_info("%s %d default ad eq idx = %d eq, eq_now_idx=%d, is_loaded=%d\n",
					__func__, __LINE__, vbc_eq_idx,
					eq_now_idx, p_eq_setting->is_loaded);
				vbc_eq_profile_close(codec, vbc_eq_idx);
				spin_unlock(&vbc_codec->lock_eq_idx);
				return;
			}
			now_ad =
				&(((struct vbc_ad_eq_profile *)
				(p_eq_setting->data[vbc_eq_idx]))
				[eq_now_idx]);
			data = now_ad->effect_paras;
			sp_asoc_pr_info("line[%d]VBC %s EQ Apply\n", __LINE__,
					vbc_get_eq_name(vbc_eq_idx));
		}
		p_eq_setting->vbc_eq_apply(codec, data, vbc_eq_idx);
		spin_unlock(&vbc_codec->lock_eq_idx);
	}
}

static int vbc_eq_profile_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int vbc_eq_idx = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] =
	    p_eq_setting->now_profile[vbc_eq_idx];
	return 0;
}

static int vbc_eq_profile_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	long temp = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int vbc_eq_idx = FUN_REG(mc->reg);
	int profile_max;
	int eq_now_idx = 0;

	profile_max = p_eq_setting->hdr.num_da[vbc_eq_idx];
	temp = ucontrol->value.integer.value[0];
	eq_now_idx = p_eq_setting->now_profile[vbc_eq_idx];
	if (temp == eq_now_idx)
		return 0;

	sp_asoc_pr_info("%s, VBC %s EQ Select %ld max %d\n",
			__func__, vbc_get_eq_name(vbc_eq_idx),
			ucontrol->value.integer.value[0], profile_max);

	if (temp >= 0 && temp < profile_max) {
		p_eq_setting->now_profile[vbc_eq_idx] = temp;
		vbc_codec->hal_put_eq_profile[vbc_eq_idx] = true;
	} else {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	if (p_eq_setting->is_active[vbc_eq_idx]
	    && p_eq_setting->data[vbc_eq_idx])
		vbc_eq_try_apply(codec, vbc_eq_idx);
	pr_info("%s %d temp=%ld\n", __func__, __LINE__, temp);

	return 1;
}

static const char *const eq_fw_name[] = {
	"vbc_eq", "vbc_eq_1", "vbc_eq_2", "vbc_eq_3"
};


static int audio_load_firmware_data(struct firmware *fw, char *firmware_path)
{
	int read_len, size;
	char *buf;
	char *audio_image_buffer;
	int image_size;
	struct file *file;
	loff_t pos = 0;

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
	pr_info("audio_image_buffer=%p\n", audio_image_buffer);
	size = image_size;
	buf = audio_image_buffer;
	do {
		read_len = kernel_read(file, buf, size, &pos);
		if (read_len > 0) {
			size -= read_len;
			buf += read_len;
		}
	} while (read_len > 0 && size > 0);
	filp_close(file, NULL);
	fw->data = audio_image_buffer;
	fw->size = image_size;
	pr_info("After read, audio_image_buffer=%p, size=%zd finish.\n",
		fw->data, fw->size);

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
static int vbc_eq_loading(struct snd_soc_codec *codec)
{
	int ret;
	const u8 *fw_data;
	struct firmware fw;
	int i;
	int vbc_eq_idx;
	int offset = 0;
	int len = 0;
	int old_num_profile;
	int old_num_da[VBC_EQ_MAX];
	int offset_len[VBC_EQ_MAX + 1];
	int sum_count = 0;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	int eq_now_idx = 0;
	size_t eq_req_len;

	pr_info("%s vbc fw name =%s\n", __func__,
		eq_fw_name[p_eq_setting->eq_fw_name_idx]);
	memset(&fw, 0, sizeof(fw));
	mutex_lock(&vbc_codec->load_mutex);
	p_eq_setting->is_loading = 1;
	p_eq_setting->is_loaded = 0;
	/* get wake lock avoid suspend */
	__pm_stay_awake(&vbc_codec->wake_lock);
	/* request firmware for VBC EQ */
	memset(vbc_codec->firmware_path, 0,
	       sizeof(vbc_codec->firmware_path));
	strcpy(vbc_codec->firmware_path, AUDIO_FIRMWARE_PATH_BASE);
	strcat(vbc_codec->firmware_path,
	       eq_fw_name[p_eq_setting->eq_fw_name_idx]);
	ret = audio_load_firmware_data(&fw, &vbc_codec->firmware_path[0]);
	if (ret) {
		pr_err("ERR:Failed to load firmware, ret:%d\n", ret);
		__pm_relax(&vbc_codec->wake_lock);
		goto req_fw_err;
	}
	fw_data = fw.data;
	__pm_relax(&vbc_codec->wake_lock);
	old_num_profile = p_eq_setting->hdr.num_profile;
	if (fw_data == NULL) {
		pr_err("ERR:firmware data error!\n");
		goto eq_out;
	}
	for (vbc_eq_idx = 0; vbc_eq_idx < VBC_EQ_MAX; vbc_eq_idx++)
		old_num_da[vbc_eq_idx] = p_eq_setting->hdr.num_da[vbc_eq_idx];
	if (fw.size < sizeof(struct vbc_fw_header)) {
		pr_err("ERR: fw size(%#zx) invalid\n", fw.size);
		ret = -EINVAL;
		goto eq_out;
	}
	memcpy(&p_eq_setting->hdr, fw_data, sizeof(p_eq_setting->hdr));

	if (strncmp
	    (p_eq_setting->hdr.magic, VBC_EQ_FIRMWARE_MAGIC_ID,
	     VBC_EQ_FIRMWARE_MAGIC_LEN)) {
		pr_err("ERR:Firmware magic error!\n");
		ret = -EINVAL;
		goto eq_out;
	}

	if (p_eq_setting->hdr.profile_version != VBC_EQ_PROFILE_VERSION) {
		pr_err("ERR:Firmware support version is 0x%x!\n",
		       VBC_EQ_PROFILE_VERSION);
		ret = -EINVAL;
		goto eq_out;
	}

	if (p_eq_setting->hdr.num_profile > VBC_EQ_PROFILE_CNT_MAX) {
		pr_err
		    ("ERR:Firmware profile to large at %d, max count is %d!\n",
		     p_eq_setting->hdr.num_profile, VBC_EQ_PROFILE_CNT_MAX);
		ret = -EINVAL;
		goto eq_out;
	}

	for (vbc_eq_idx = 0; vbc_eq_idx < VBC_EQ_MAX; vbc_eq_idx++)
		sum_count += p_eq_setting->hdr.num_da[vbc_eq_idx];

	if (p_eq_setting->hdr.num_profile != sum_count) {
		pr_err("ERR:Firmware profile total number is  wrong!\n");
		ret = -EINVAL;
		goto eq_out;
	}

	offset_len[VBC_DA_EQ] = sizeof(struct vbc_fw_header);
	offset_len[VBC_AD01_EQ] = sizeof(struct vbc_da_eq_profile);
	offset_len[VBC_AD23_EQ] = sizeof(struct vbc_ad_eq_profile);
	offset_len[VBC_AD23_EQ + 1] = sizeof(struct vbc_ad_eq_profile);

	for (vbc_eq_idx = 0; vbc_eq_idx < VBC_EQ_MAX; vbc_eq_idx++) {
		if (old_num_da[vbc_eq_idx] !=
			p_eq_setting->hdr.num_da[vbc_eq_idx]) {
			eq_now_idx =
				p_eq_setting->now_profile[vbc_eq_idx];
			if (eq_now_idx >=
				p_eq_setting->hdr.num_da[vbc_eq_idx]) {
				p_eq_setting->now_profile[vbc_eq_idx] = -1;
			}
			vbc_safe_kfree(&p_eq_setting->data[vbc_eq_idx]);
		}

		if (vbc_eq_idx) {
			offset +=
			    offset_len[vbc_eq_idx] *
			    p_eq_setting->hdr.num_da[vbc_eq_idx - 1];
		} else {
			offset += offset_len[vbc_eq_idx];
		}

		if (p_eq_setting->hdr.num_da[vbc_eq_idx] == 0)
			continue;

		len = p_eq_setting->hdr.num_da[vbc_eq_idx] *
		    offset_len[vbc_eq_idx + 1];
		if (p_eq_setting->data[vbc_eq_idx] == NULL) {
			p_eq_setting->data[vbc_eq_idx] =
			    kzalloc(len, GFP_KERNEL);
			if (p_eq_setting->data[vbc_eq_idx] == NULL) {
				ret = -ENOMEM;
				for (--vbc_eq_idx; vbc_eq_idx >= 0;
				     vbc_eq_idx--) {
					vbc_safe_kfree(&p_eq_setting->data
						       [vbc_eq_idx]);
				}
				goto eq_out;
			}
		}
		eq_req_len = offset + len;
		if (eq_req_len > fw.size) {
			pr_err("fw size=%#zx invalid, vbc_eq_idx=%d, offset=%#x, len =%#x\n",
				fw.size, vbc_eq_idx, offset, len);
			ret = -EINVAL;
			for (; vbc_eq_idx >= 0; vbc_eq_idx--)
				vbc_safe_kfree(&p_eq_setting->data[vbc_eq_idx]);
			goto eq_out;
		}
		memcpy(p_eq_setting->data[vbc_eq_idx], fw_data + offset, len);

		for (i = 0; i < p_eq_setting->hdr.num_da[vbc_eq_idx]; i++) {
			const char *magic =
			    (char *)(p_eq_setting->data[vbc_eq_idx]) +
			    (i * offset_len[vbc_eq_idx + 1]);
			if (strncmp
			    (magic, VBC_EQ_FIRMWARE_MAGIC_ID,
			     VBC_EQ_FIRMWARE_MAGIC_LEN)) {
				pr_err("ERR:%s Firmware profile[%d] magic error!magic: %s\n",
				       vbc_get_eq_name(vbc_eq_idx), i, magic);
				ret = -EINVAL;
				for (; vbc_eq_idx >= 0; vbc_eq_idx--) {
					vbc_safe_kfree
					    (&p_eq_setting->data[vbc_eq_idx]);
				}
				goto eq_out;
			}
		}
	}

	ret = 0;
	p_eq_setting->is_loaded = 1;
	goto eq_out;

eq_out:
	audio_release_firmware_data(&fw);
req_fw_err:
	p_eq_setting->is_loading = 0;
	mutex_unlock(&vbc_codec->load_mutex);
	pr_info("%s loaded\n", __func__);
	if (ret >= 0) {
		struct vbc_da_eq_profile *profile =
		    &(((struct vbc_da_eq_profile
			*)(p_eq_setting->data[VBC_DA_EQ]))
		      [0]);
		u32 *data = profile->effect_paras;

		if (data[vbc_da_eq_reg_offset(REG_VBC_VBC_DAC_HP_CTRL)] &
		    BIT_RF_DAC_ALC_DP_T_MODE)
			vbc_codec->alc_dp_t_mode = 1;
		else
			vbc_codec->alc_dp_t_mode = 0;
		sp_asoc_pr_dbg("REG_VBC_VBC_DAC_HP_CTRL:%x----alc_dp_t_mode:%d",
			       data[vbc_da_eq_reg_offset
				    (REG_VBC_VBC_DAC_HP_CTRL)],
			       vbc_codec->alc_dp_t_mode);
		for (i = VBC_DA_EQ; i < VBC_EQ_MAX; i++) {
			if (p_eq_setting->is_active[i] &&
				p_eq_setting->data[i]) {
				pr_info("load apply eq_idx=%d\n", i);
				vbc_eq_try_apply(codec, i);
			}
		}
	}
	pr_info("%s return %i\n", __func__, ret);

	return ret;
}

static int vbc_switch_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_control;
	return 0;
}

static int vbc_switch_reg_val[];
static int vbc_int_switch_reg_val[];
static int vbc_dma_switch_reg_val[];

static int vbc_switch_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int32_t value = 0;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
		ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	pr_info("VBC Switch to %s\n",
		texts->texts[ucontrol->value.integer.value[0]]);

	value = ucontrol->value.integer.value[0];

	if (vbc_switch_reg_val[vbc_codec->vbc_control] != VBC_TO_WTLCP_CTRL) {
		if (vbc_switch_reg_val[value] == VBC_TO_WTLCP_CTRL)
			vbc_eb_set();
	} else if (vbc_switch_reg_val[vbc_codec->vbc_control]
		   == VBC_TO_WTLCP_CTRL) {
		if (vbc_switch_reg_val[value] == VBC_TO_AP_CTRL)
			vbc_eb_clear();
	}
	vbc_codec->vbc_control = value;
	arch_audio_vbc_switch(vbc_switch_reg_val[value],
			      vbc_codec->vbc_use_dma_type, VBC_IDX_MAX);
	vbc_set_phys_addr(vbc_switch_reg_val[vbc_codec->vbc_control]);

	return 1;
}

static int vbc_int_switch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_int_control;
	return 0;
}

static int vbc_int_switch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
		ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	if (vbc_codec->vbc_int_control == ucontrol->value.integer.value[0])
		return 0;

	sp_asoc_pr_info("VBC Switch to %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);

	vbc_codec->vbc_int_control = ucontrol->value.integer.value[0];

	arch_audio_vbc_int_switch(vbc_int_switch_reg_val
				  [vbc_codec->vbc_int_control]);
	vbc_set_phys_addr(vbc_int_switch_reg_val[vbc_codec->vbc_int_control]);

	return 1;
}

static int vbc_dma_switch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_dma_control;
	return 0;
}

static int vbc_dma_switch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
		ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	if (vbc_codec->vbc_dma_control == ucontrol->value.integer.value[0])
		return 0;

	sp_asoc_pr_info("VBC Switch to %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);

	vbc_codec->vbc_dma_control = ucontrol->value.integer.value[0];

	arch_audio_vbc_dma_switch(vbc_dma_switch_reg_val
				  [vbc_codec->vbc_dma_control],
				  vbc_codec->vbc_use_dma_type, VBC_IDX_MAX);
	vbc_set_phys_addr(vbc_dma_switch_reg_val[vbc_codec->vbc_dma_control]);

	return 1;
}

static int vbc_eq_switch_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = p_eq_setting->is_active[id];
	return 0;
}

static int vbc_eq_switch_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int ret;

	ret = ucontrol->value.integer.value[0];
	if (ret == p_eq_setting->is_active[id])
		return ret;

	sp_asoc_pr_info("VBC %s EQ Switch %s\n", vbc_get_eq_name(id),
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	if ((ret == 0) || (ret == 1)) {
		p_eq_setting->is_active[id] = ret;
		if (p_eq_setting->is_active[id]
		    && p_eq_setting->data[id]) {
			p_eq_setting->vbc_eq_apply = vbc_eq_profile_apply;
			pr_info("%s %d\n", __func__, __LINE__);
			vbc_eq_try_apply(codec, id);
		} else {
			p_eq_setting->vbc_eq_apply = 0;
			vbc_eq_profile_close(codec, id);
		}
	}
	pr_info("%s %d ret=%d\n", __func__, __LINE__, ret);

	return ret;
}

static int vbc_eq_fw_name_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;

	ucontrol->value.integer.value[0] = p_eq_setting->eq_fw_name_idx;

	return 0;
}

static int vbc_eq_fw_name_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
			codec_kcontrol_to_codec_drvdata(kcontrol);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
		ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}
	sp_asoc_pr_info("VBC eq firmware name %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);
	p_eq_setting->eq_fw_name_idx = ucontrol->value.integer.value[0];

	return 0;
}

static int vbc_eq_load_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;

	ucontrol->value.integer.value[0] = p_eq_setting->is_loading;
	if (p_eq_setting->is_loaded)
		ucontrol->value.integer.value[0] = 2;

	return 0;
}

static int vbc_eq_load_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int ret;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
		ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_info("VBC EQ %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);

	ret = ucontrol->value.integer.value[0];
	if (ret == 1)
		ret = vbc_eq_loading(codec);

	return ret;
}

static int vbc_dg_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_dg_idx = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->dg[vbc_dg_idx].dg_val[id];
	return 0;
}

static int vbc_dg_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	uint32_t ret = 0;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int dg_idx = mc->shift;

	ret = ucontrol->value.integer.value[0];

	sp_asoc_pr_info("VBC %s %d DG set 0x%02x\n",
			vbc_get_dg_name(dg_idx), id,
			(int)ucontrol->value.integer.value[0]);

	if (ret <= VBC_DG_VAL_MAX)
		vbc_codec->dg[dg_idx].dg_val[id] = ret;

	vbc_try_dg_set(vbc_codec, dg_idx, id);

	return ret;
}

static int vbc_st_dg_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->st_dg.dg_val[id];
	return 0;
}

static int vbc_st_dg_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];

	sp_asoc_pr_info("VBC ST%d DG set 0x%02x\n", id,
			(int)ucontrol->value.integer.value[0]);

	if (ret <= VBC_DG_VAL_MAX)
		vbc_codec->st_dg.dg_val[id] = ret;

	vbc_try_st_dg_set(vbc_codec, id);

	return ret;
}

static int vbc_dg_switch_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int dg_idx = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->dg[dg_idx].dg_switch[id];
	return 0;
}

static int vbc_dg_switch_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int dg_idx = mc->shift;

	ret = ucontrol->value.integer.value[0];
	sp_asoc_pr_info("VBC %s %d DG Switch %s\n",
			vbc_get_dg_name(dg_idx), id,
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	vbc_codec->dg[dg_idx].dg_switch[id] = ret;

	vbc_try_dg_set(vbc_codec, dg_idx, id);

	return ret;
}

static int vbc_st_hpf_switch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->st_dg.hpf_switch[id];
	return 0;
}

static int vbc_st_hpf_switch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];

	sp_asoc_pr_info("VBC ST%d HPF Switch %s\n", id,
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	vbc_codec->st_dg.hpf_switch[id] = ret;

	vbc_try_st_hpf_set(vbc_codec, id);

	return ret;
}

static int vbc_st_hpf_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->st_dg.hpf_val[id];
	return 0;
}

static int vbc_st_hpf_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];

	sp_asoc_pr_info("VBC ST%d HPF set 0x%02x\n", id,
			(int)ucontrol->value.integer.value[0]);

	if (ret <= VBC_DG_VAL_MAX)
		vbc_codec->st_dg.hpf_val[id] = ret;

	vbc_try_st_hpf_set(vbc_codec, id);

	return ret;
}

static int adc_dgmux_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->adc_dgmux_val[id];
	return 0;
}

static int adc_dgmux_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->adc_dgmux_val[id])
		return ret;

	sp_asoc_pr_info("VBC AD%d DG mux : %ld\n", id,
			ucontrol->value.integer.value[0]);

	vbc_codec->adc_dgmux_val[id] = ret;
	vbc_try_ad_dgmux_set(vbc_codec, id);

	return ret;
}

static int dac_iismux_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_da_iis_port;
	return 0;
}

static int dac_iismux_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	if (ucontrol->value.integer.value[0] >= texts->items ||
		ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	if (vbc_codec->vbc_da_iis_port == ucontrol->value.integer.value[0])
		return 0;

	sp_asoc_pr_info("VBC DA output to %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);

	vbc_codec->vbc_da_iis_port = ucontrol->value.integer.value[0];

	vbc_try_da_iismux_set(vbc_codec->vbc_da_iis_port);

	return 1;
}

/* mute fm directly when close fm other than call */
int vbc_close_fm_dggain(bool mute)
{
	if (g_vbc_codec == NULL)
		return -1;

	mutex_lock(&g_vbc_codec->fm_mutex);
	if (g_vbc_codec->fm_mute_direct == true) {
		sp_asoc_pr_info("%s hal has set fm mute already\n",
							__func__);
	} else {
		vbc_fm_mute_direct(mute);
		sp_asoc_pr_info("%s fm mute direct = %d\n",
						__func__, mute);
	}
	mutex_unlock(&g_vbc_codec->fm_mutex);

	/* cut down the dggain of fm input */
	/* ninglei sharkl bug? REG_VBC_VBC_ADC01_DG_CTRL dg is not for fm,
	 * you should operate REG_VBC_VBC_DAC_ST_CTL0 dg bits
	 * and REG_VBC_VBC_DAC_ST_CTL1 dg bits
	 *return vbc_reg_update(REG_VBC_VBC_ADC01_DG_CTRL, 0x7F7F, 0x7F7F);
	 */

	return 0;
}

static unsigned int vbc_codec_read(struct snd_soc_codec *codec,
				   unsigned int reg)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	/* Because snd_soc_update_bits reg is 16 bits short type,
	 * so muse do following convert
	 */
	if (IS_SPRD_VBC_RANG(reg | SPRD_VBC_BASE_HI)) {
		reg |= SPRD_VBC_BASE_HI;
		return vbc_reg_read(reg);
	} else if (IS_SPRD_VBC_MUX_RANG(FUN_REG(reg))) {
		int id = SPRD_VBC_MUX_IDX(FUN_REG(reg));
		struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[id]);

		return mux->val;
	} else if (IS_SPRD_VBC_SWITCH_RANG(FUN_REG(reg))) {
		int id = SPRD_VBC_SWITCH_IDX(FUN_REG(reg));

		return vbc_codec->vbc_loop_switch[id];
	}

	sp_asoc_pr_dbg("The Register is NOT VBC Codec's reg = 0x%x\n", reg);
	return 0;
}

static int vbc_da_iis_lr_mod_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_da_iis_lrmod;

	return 0;
}

static int vbc_da_iis_lr_mod_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	sp_asoc_pr_info("%s, %d\n", __func__,
			(int32_t)ucontrol->value.integer.value[0]);

	vbc_codec->vbc_da_iis_lrmod = ucontrol->value.integer.value[0];
	vbc_da_iis_set_lr_md(vbc_codec->vbc_da_iis_lrmod);

	return 1;
}

static int vbc_ad01_iis_lr_mod_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_ad01_iis_lrmod;

	return 0;
}

static int vbc_ad01_iis_lr_mod_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	sp_asoc_pr_info("%s, %d\n", __func__,
			(int32_t)ucontrol->value.integer.value[0]);

	vbc_codec->vbc_ad01_iis_lrmod = ucontrol->value.integer.value[0];
	vbc_ad_iis_set_lr_md(VBC_CAPTURE01, vbc_codec->vbc_ad01_iis_lrmod);

	return 1;
}

static int vbc_ad23_iis_lr_mod_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_ad23_iis_lrmod;

	return 0;
}

static int vbc_ad23_iis_lr_mod_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	sp_asoc_pr_info("%s, %d\n", __func__,
			(int32_t)ucontrol->value.integer.value[0]);

	vbc_codec->vbc_ad23_iis_lrmod = ucontrol->value.integer.value[0];
	vbc_ad_iis_set_lr_md(VBC_CAPTURE23, vbc_codec->vbc_ad23_iis_lrmod);

	return 1;
}

static int vbc_da_iis_wd_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_da_iis_width;

	return 0;
}

static int vbc_da_iis_wd_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	sp_asoc_pr_info("%s, %d\n", __func__,
			(int32_t)ucontrol->value.integer.value[0]);

	vbc_codec->vbc_da_iis_width = ucontrol->value.integer.value[0];
	vbc_da_iis_wd_sel(vbc_codec->vbc_da_iis_width);

	return 1;
}

/* fm mute */
static int vbc_fm_mute_direct_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->fm_mute_direct;

	return 0;
}

static int vbc_fm_mute_direct_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	sp_asoc_pr_info("%s, %d\n", __func__,
			(int32_t)ucontrol->value.integer.value[0]);

	mutex_lock(&vbc_codec->fm_mutex);
	vbc_codec->fm_mute_direct = ucontrol->value.integer.value[0];
	vbc_fm_mute_direct(vbc_codec->fm_mute_direct);
	mutex_unlock(&vbc_codec->fm_mutex);

	return 1;
}

static int vbc_fm_mute_smothdg_step_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
			codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->fm_mutedg_step;

	return 0;
}

static int vbc_fm_mute_smothdg_step_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	vbc_codec->fm_mutedg_step = ucontrol->value.integer.value[0];

	return 0;
}

static int vbc_fm_unmute_smooth_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->fm_mute_smooth;

	return 0;
}

static int vbc_fm_unmute_smooth_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	sp_asoc_pr_info("%s, %d\n", __func__,
			(int32_t)ucontrol->value.integer.value[0]);

	mutex_lock(&vbc_codec->fm_mutex);
	vbc_codec->fm_mute_smooth = !ucontrol->value.integer.value[0];
	vbc_fm_mute_smooth(vbc_codec->fm_mute_smooth, vbc_codec);
	mutex_unlock(&vbc_codec->fm_mutex);

	return 1;
}

static int vbc_da_src_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->vbc_da_src;

	return 0;
}

static int vbc_da_src_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	sp_asoc_pr_info("%s, %d\n", __func__,
			(int32_t)ucontrol->value.integer.value[0]);

	vbc_codec->vbc_da_src = ucontrol->value.integer.value[0];
	vbc_da_src_set(vbc_codec->vbc_da_src);

	return 1;
}

static int sys_iis_sel_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int id = FUN_REG(e->reg) - SYS_IIS_START;

	ucontrol->value.integer.value[0] = vbc_codec->sys_iis_sel[id];
	return 0;
}

static int sys_iis_sel_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	unsigned short value = 0;
	int ret = 0;
	struct pinctrl_state *state;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	char buf[128] = { 0 };
	unsigned int id = FUN_REG(e->reg) - SYS_IIS_START;

	value = ucontrol->value.enumerated.item[0];

	if (value >= e->items) {
		dev_err(codec->dev,
			"%s, invalid item(%d >= %d)!\n", __func__, value,
			e->items);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s, id=%d,value=%d, texts->texts[] =%s\n",
		       __func__, id, value, texts->texts[value]);

	sprintf(buf, "%s_%u", texts->texts[value], id);
	state = pinctrl_lookup_state(vbc_codec->pctrl, buf);
	if (IS_ERR(state)) {
		dev_err(codec->dev, "%s, pinctrl lookup state for '%s' failed(%ld)!\n",
			__func__, buf, PTR_ERR(state));
		return PTR_ERR(state);
	}
	ret = pinctrl_select_state(vbc_codec->pctrl, state);
	if (ret != 0) {
		dev_err(codec->dev, "%s failed ret = %d\n", __func__, ret);
		return ret;
	}

	vbc_codec->sys_iis_sel[id] = value;

	sp_asoc_pr_dbg("%s,soc iis%d -> %s\n", __func__, id, buf);

	return true;
}

static int iis_bt_fm_loop_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = vbc_codec->iis_bt_fm_loop_en;

	return 0;
}

static int iis_bt_fm_loop_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	unsigned short enable;
	int ret;
	struct pinctrl_state *state;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	char buf[128] = {};

	enable = ucontrol->value.enumerated.item[0];

	if (enable)
		sprintf(buf, "iis_bt_fm_loop_%u_%u_%s",
			vbc_codec->iis_bt_fm_loop[0],
			  vbc_codec->iis_bt_fm_loop[1], "enable");
	else
		sprintf(buf, "iis_bt_fm_loop_%u_%u_%s",
			vbc_codec->iis_bt_fm_loop[0],
				vbc_codec->iis_bt_fm_loop[1], "disable");

	state = pinctrl_lookup_state(vbc_codec->pctrl, buf);
	if (IS_ERR(state)) {
		dev_err(codec->dev, "%s, pinctrl lookup state for '%s' failed(%ld)!\n",
			__func__, buf, PTR_ERR(state));
		return PTR_ERR(state);
	}
	ret = pinctrl_select_state(vbc_codec->pctrl, state);
	if (ret != 0) {
		dev_err(codec->dev, "failed ret = %d\n", ret);
		return ret;
	}
	vbc_codec->iis_bt_fm_loop_en = enable;
	pr_info("%s, iis_bt_fm_loop_en=%u for %s\n", __func__,
		vbc_codec->iis_bt_fm_loop_en, buf);

	return 0;
}

static int vbc_mixerdgstep_da01_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
			codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->dgmixerstep_da01;

	return 0;
}

static int vbc_mixerdgstep_da01_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	vbc_codec->dgmixerstep_da01 = ucontrol->value.integer.value[0];

	return 0;
}

static int vbc_mixerdgstep_da23_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
			codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.integer.value[0] = vbc_codec->dgmixerstep_da23;

	return 0;
}

static int vbc_mixerdgstep_da23_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	vbc_codec->dgmixerstep_da23 = ucontrol->value.integer.value[0];

	return 0;
}

static int vbc_mixerdg_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = mc->shift;
	uint32_t val = 0;
	uint32_t reg = 0;
	uint32_t l_mask = 0;
	uint32_t r_mask = 0;
	uint32_t l_shift = 0;
	uint32_t r_shift = 0;

	if (id == VBC_MIXERDG_DAC01) {
		reg = REG_VBC_VBC_DAC_DGMIXER_DG0;
		l_mask = BITS_RF_DAC0_DGMIXER_DG(0xffff);
		r_mask = BITS_RF_DAC1_DGMIXER_DG(0xffff);
		l_shift = DAC0_DGMIXER_DG_SHIFT;
		r_shift = DAC1_DGMIXER_DG_SHIFT;
	} else if (id == VBC_MIXERDG_DAC23) {
		reg = REG_VBC_VBC_DAC_DGMIXER_DG1;
		l_mask = BITS_RF_DAC2_DGMIXER_DG(0xffff);
		r_mask = BITS_RF_DAC3_DGMIXER_DG(0xffff);
		l_shift = DAC2_DGMIXER_DG_SHIFT;
		r_shift = DAC3_DGMIXER_DG_SHIFT;
	} else {
		pr_warn("%s dgmixer invalid id\n", __func__);
		return 0;
	}
	val = vbc_reg_read(reg);
	ucontrol->value.integer.value[0] = (val & l_mask) >> l_shift;
	ucontrol->value.integer.value[1] = (val & r_mask) >> r_shift;

	return 0;
}

static int vbc_mixerdg_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int val1, val2;
	int id = mc->shift;

	val1 = ucontrol->value.integer.value[0];
	val2 = ucontrol->value.integer.value[1];

	pr_info("%s %d l:%02d, r:%02d\n",
			__func__, id, val1, val2);
	/* dgmixerstep init in probe */
	if (id == VBC_MIXERDG_DAC01) {
		vbc_da01_set_dgmixer_step(vbc_codec->dgmixerstep_da01);
		vbc_codec->dgmixer01[AUDIO_CHAN_L] = val1;
		vbc_codec->dgmixer01[AUDIO_CHAN_R] = val2;
		vbc_da01_set_dgmixer_dg(AUDIO_CHAN_L, val1);
		vbc_da01_set_dgmixer_dg(AUDIO_CHAN_R, val2);
	} else if (id == VBC_MIXERDG_DAC23) {
		vbc_da01_set_dgmixer_step(vbc_codec->dgmixerstep_da23);
		vbc_codec->dgmixer23[AUDIO_CHAN_L] = val1;
		vbc_codec->dgmixer23[AUDIO_CHAN_R] = val2;
		vbc_da23_set_dgmixer_dg(AUDIO_CHAN_L, val1);
		vbc_da23_set_dgmixer_dg(AUDIO_CHAN_R, val2);
	} else {
		pr_warn("%s dgmixer invalid id\n", __func__);
		return 0;
	}

	return 0;
}

static int vbc_get_access(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	unsigned short enable = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	enable = ucontrol->value.enumerated.item[0];
	ucontrol->value.integer.value[0] = vbc_codec->vbc_access_en;

	return true;
}
static int vbc_put_access(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	unsigned short enable = 0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	enable = ucontrol->value.enumerated.item[0];

	pr_info("%s, cnt=%d, enable=%d",
		__func__, vbc_codec->vbc_access_en, enable);
	mutex_lock(&vbc_codec->access_mutex);
	if (enable) {
		if (vbc_codec->vbc_access_en == 0)
			vbc_eb_set();
		vbc_codec->vbc_access_en++;
	} else {
		if (vbc_codec->vbc_access_en)
			vbc_eb_clear();
		vbc_codec->vbc_access_en = 0;
	}
	mutex_unlock(&vbc_codec->access_mutex);

	return true;
}

static int vbc_mixer_mux_sel_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol,
				 unsigned int mixer_id,
				 unsigned int chan)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	unsigned int item = ucontrol->value.enumerated.item[0];
	struct device *dev = vbc_codec->codec->dev;

	if (item >= e->items) {
		dev_err(dev, "%s, invalid item: %u(max: %u)\n", __func__, item,
			e->items);
		return -EINVAL;
	}

	if (chan >= CHAN_IDX_MAX) {
		dev_err(dev, "%s, invalid chan: %u(max: %u)\n", __func__, chan,
			CHAN_IDX_MAX - 1);
		return -EINVAL;
	}

	if (mixer_id >= VBC_MIXER_MAX) {
		dev_err(dev, "%s, invalid mixer id: %u(max: %u)\n", __func__,
			mixer_id, VBC_MIXER_MAX - 1);
		return -EINVAL;
	}

	if (vbc_codec->mixer_mux_sel[mixer_id][chan] == item)
		return 0;

	dev_info(dev, "'%s' -> '%s'\n", kcontrol->id.name, e->texts[item]);

	vbc_mixer_mux_sel(mixer_id,
			  chan == CHAN_IDX_0 ? AUDIO_CHAN_L : AUDIO_CHAN_R,
			  item);
	vbc_codec->mixer_mux_sel[mixer_id][chan] = item;

	return 1;
}

static int vbc_dac0_mixer_mux_sel_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_mux_sel[VBC_MIXER_DAC][CHAN_IDX_0];

	return 0;
}

static int vbc_dac0_mixer_mux_sel_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_mux_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_DAC, CHAN_IDX_0);
}

static int vbc_dac1_mixer_mux_sel_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_mux_sel[VBC_MIXER_DAC][CHAN_IDX_1];

	return 0;
}

static int vbc_dac1_mixer_mux_sel_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_mux_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_DAC, CHAN_IDX_1);
}

static int vbc_st0_mixer_mux_sel_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_mux_sel[VBC_MIXER_ST][CHAN_IDX_0];

	return 0;
}

static int vbc_st0_mixer_mux_sel_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_mux_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_ST, CHAN_IDX_0);
}

static int vbc_st1_mixer_mux_sel_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_mux_sel[VBC_MIXER_ST][CHAN_IDX_1];

	return 0;
}

static int vbc_st1_mixer_mux_sel_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_mux_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_ST, CHAN_IDX_1);
}

static int vbc_mixer_out_sel_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol,
				 unsigned int mixer_id,
				 unsigned int chan)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);
	unsigned int item = ucontrol->value.enumerated.item[0];
	struct device *dev = vbc_codec->codec->dev;

	if (item >= e->items) {
		dev_err(dev, "%s, invalid item: %u(max: %u)\n", __func__, item,
			e->items);
		return -EINVAL;
	}

	if (chan >= CHAN_IDX_MAX) {
		dev_err(dev, "%s, invalid chan: %u(max: %u)\n", __func__, chan,
			CHAN_IDX_MAX - 1);
		return -EINVAL;
	}

	if (mixer_id >= VBC_MIXER_MAX) {
		dev_err(dev, "%s, invalid mixer id: %u(max: %u)\n", __func__,
			mixer_id, VBC_MIXER_MAX - 1);
		return -EINVAL;
	}

	if (vbc_codec->mixer_out_sel[mixer_id][chan] == item)
		return 0;

	dev_info(dev, "'%s' -> '%s'\n", kcontrol->id.name, e->texts[item]);

	vbc_mixer_out_sel(mixer_id,
			  chan == CHAN_IDX_0 ? AUDIO_CHAN_L : AUDIO_CHAN_R,
			  item);
	vbc_codec->mixer_out_sel[mixer_id][chan] = item;

	return 1;
}

static int vbc_dac0_mixer_out_sel_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_out_sel[VBC_MIXER_DAC][CHAN_IDX_0];

	return 0;
}

static int vbc_dac0_mixer_out_sel_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_out_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_DAC, CHAN_IDX_0);
}

static int vbc_dac1_mixer_out_sel_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
		codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_out_sel[VBC_MIXER_DAC][CHAN_IDX_1];

	return 0;
}

static int vbc_dac1_mixer_out_sel_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_out_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_DAC, CHAN_IDX_1);
}

static int vbc_st0_mixer_out_sel_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_out_sel[VBC_MIXER_ST][CHAN_IDX_0];

	return 0;
}

static int vbc_st0_mixer_out_sel_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_out_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_ST, CHAN_IDX_0);
}

static int vbc_st1_mixer_out_sel_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct vbc_codec_priv *vbc_codec =
	    codec_kcontrol_to_codec_drvdata(kcontrol);

	ucontrol->value.enumerated.item[0] =
		vbc_codec->mixer_out_sel[VBC_MIXER_ST][CHAN_IDX_1];

	return 0;
}

static int vbc_st1_mixer_out_sel_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	return vbc_mixer_out_sel_put(kcontrol, ucontrol,
				     VBC_MIXER_ST, CHAN_IDX_1);
}

static int vbc_switch_reg_val[] = {
	VBC_TO_WTLCP_CTRL,
	VBC_TO_AP_CTRL,
	VBC_TO_PUBCP_CTRL,
	VBC_NO_CHANGE,
};

static int vbc_int_switch_reg_val[] = {
	VBC_INT_NO_CHANGE,
	VBC_INT_TO_AP_CTRL,
	VBC_INT_TO_WTLCP_CTRL,
	VBC_INT_TO_PUBCP_CTRL,
};

static int vbc_dma_switch_reg_val[] = {
	VBC_DMA_NO_CHANGE,
	VBC_DMA_TO_AP_AON_CTRL,
	VBC_DMA_TO_WTLCP_TGDSP_CTRL,
	VBC_DMA_TO_WTLCP_LDSP_CTRL,
};

static const char *const vbc_switch_function[] = {
	"WTLCP", "AP", "PUBCP", "VBC_NO_CHANGE"
};

static const char *const vbc_int_switch_function[] = {
	"VBC_INT_NO_CHANGE", "AP", "WTLCP_TGDSP", "WTLCP_LDSP"
};

static const char *const vbc_dma_switch_function[] = {
	"VBC_DMA_NO_CHANGE", "AP_AON", "WTLCP_TGDSP", "WTLCP_LDSP",
};

static const char *const eq_load_function[] = { "idle", "loading", "loaded"};

static const char *const da_iis_mux_function[] = {
	"AUDIIS0", "FM-BT", "VBC-IIS2", "VBC-IIS3"
};

static const char *const fm_sample_rate_function[] = { "32000", "48000" };

static const struct soc_enum vbc_fw_name_enum =
	SOC_ENUM_SINGLE_EXT(4, eq_fw_name);

static const struct soc_enum vbc_enum[] = {
	SOC_ENUM_SINGLE_EXT(4, vbc_switch_function),
	SOC_ENUM_SINGLE_EXT(4, vbc_int_switch_function),
	SOC_ENUM_SINGLE_EXT(5, vbc_dma_switch_function),
	SOC_ENUM_SINGLE_EXT(3, eq_load_function),
	SOC_ENUM_SINGLE_EXT(4, da_iis_mux_function),
	/* nignlei fm_sample_rate_function used? */
	SOC_ENUM_SINGLE_EXT(2, fm_sample_rate_function),
};

/*
 * NOTE: "by eq" is an atificial option to let vbc_eq_reg_apply() to do
 * the mixer setting.
 */
/* DAC mixer mux selection */
static const char * const dac0_mixer_mux_sel_text[] = {
	"dac0", "dac1",
	"(dac0+dac1)/2", "(dac0-dac1)/2",
	"zero", "by eq"
};
static const char * const dac1_mixer_mux_sel_text[] = {
	"dac1", "dac0",
	"(dac0+dac1)/2", "(dac0-dac1)/2",
	"zero", "by eq"
};
static const struct soc_enum dac_mixer_mux_enums[CHAN_IDX_MAX] = {
	SOC_ENUM_SINGLE_EXT(6, dac0_mixer_mux_sel_text),
	SOC_ENUM_SINGLE_EXT(6, dac1_mixer_mux_sel_text),
};

/* ST mixer mux selection */
static const char * const st0_mixer_mux_sel_text[] = {
	"st0", "st1",
	"(st0+st1)/2", "(st0-st1)/2",
	"zero", "by eq"
};
static const char * const st1_mixer_mux_sel_text[] = {
	"st1", "st0",
	"(st0+st1)/2", "(st0-st1)/2",
	"zero", "by eq"
};
static const struct soc_enum st_mixer_mux_enums[CHAN_IDX_MAX] = {
	SOC_ENUM_SINGLE_EXT(6, st0_mixer_mux_sel_text),
	SOC_ENUM_SINGLE_EXT(6, st1_mixer_mux_sel_text),
};

static const char * const mixer_out_sel_text[] = {
	"data*1", "data*-1", "by eq"
};
/* DAC mixer out selection */
static const struct soc_enum dac_mixer_out_enums[CHAN_IDX_MAX] = {
	SOC_ENUM_SINGLE_EXT(3, mixer_out_sel_text),
	SOC_ENUM_SINGLE_EXT(3, mixer_out_sel_text),
};

/* ST mixer out selection */
static const struct soc_enum st_mixer_out_enums[CHAN_IDX_MAX] = {
	SOC_ENUM_SINGLE_EXT(3, mixer_out_sel_text),
	SOC_ENUM_SINGLE_EXT(3, mixer_out_sel_text),
};

static const struct snd_kcontrol_new vbc_codec_snd_controls[] = {
	SOC_ENUM_EXT("VBC Switch", vbc_enum[0], vbc_switch_get,
		     vbc_switch_put),
	SOC_ENUM_EXT("VBC INT Switch", vbc_enum[1], vbc_int_switch_get,
		     vbc_int_switch_put),
	SOC_ENUM_EXT("VBC DMA Switch", vbc_enum[2], vbc_dma_switch_get,
		     vbc_dma_switch_put),
	SOC_SINGLE_EXT("VBC DA EQ Switch", FUN_REG(VBC_DA_EQ), 0, 1, 0,
		       vbc_eq_switch_get,
		       vbc_eq_switch_put),
	SOC_SINGLE_EXT("VBC AD01 EQ Switch", FUN_REG(VBC_AD01_EQ), 0, 1, 0,
		       vbc_eq_switch_get,
		       vbc_eq_switch_put),
	SOC_SINGLE_EXT("VBC AD02 EQ Switch", FUN_REG(VBC_AD23_EQ), 0, 1, 0,
		       vbc_eq_switch_get,
		       vbc_eq_switch_put),
	SOC_ENUM_EXT("VBC EQ FW Name", vbc_fw_name_enum, vbc_eq_fw_name_get,
		 vbc_eq_fw_name_put),

	SOC_ENUM_EXT("VBC EQ Update", vbc_enum[3], vbc_eq_load_get,
		     vbc_eq_load_put),
	/* DG */
	SOC_DOUBLE_R_EXT("VBC DAC01 MIXERDG",
		FUN_REG(AUDIO_CHAN_L),
		FUN_REG(AUDIO_CHAN_R), VBC_MIXERDG_DAC01, MIXERDG_MAX_VAL, 0,
		vbc_mixerdg_get,
		vbc_mixerdg_put),
	SOC_DOUBLE_R_EXT("VBC DAC23 MIXERDG",
		FUN_REG(AUDIO_CHAN_L),
		FUN_REG(AUDIO_CHAN_R), VBC_MIXERDG_DAC23, MIXERDG_MAX_VAL, 0,
		vbc_mixerdg_get,
		vbc_mixerdg_put),
	SOC_SINGLE_EXT("VBC MIXERDG_DA01 STEP", SND_SOC_NOPM,
			-1, 0x1FFF, 0, vbc_mixerdgstep_da01_get,
			vbc_mixerdgstep_da01_put),
	SOC_SINGLE_EXT("VBC MIXERDG_DA23 STEP", SND_SOC_NOPM,
			-1, 0x1FFF, 0, vbc_mixerdgstep_da23_get,
			vbc_mixerdgstep_da23_put),
	SOC_SINGLE_EXT("VBC DACL DG Set", FUN_REG(AUDIO_CHAN_L),
		       VBC_DAC_DG, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC DACR DG Set", FUN_REG(AUDIO_CHAN_R),
		       VBC_DAC_DG, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADCL DG Set", FUN_REG(AUDIO_CHAN_L),
		       VBC_ADC01_DG, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADCR DG Set", FUN_REG(AUDIO_CHAN_R),
		       VBC_ADC01_DG, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADC23L DG Set", FUN_REG(AUDIO_CHAN_L),
		       VBC_ADC23_DG, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADC23R DG Set", FUN_REG(AUDIO_CHAN_R),
		       VBC_ADC23_DG, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC STL DG Set", FUN_REG(AUDIO_CHAN_L),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_dg_get,
		       vbc_st_dg_put),
	SOC_SINGLE_EXT("VBC STR DG Set", FUN_REG(AUDIO_CHAN_R),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_dg_get,
		       vbc_st_dg_put),

	SOC_SINGLE_EXT("VBC DACL DG Switch", FUN_REG(AUDIO_CHAN_L),
		       VBC_DAC_DG, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC DACR DG Switch", FUN_REG(AUDIO_CHAN_R),
		       VBC_DAC_DG, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADCL DG Switch", FUN_REG(AUDIO_CHAN_L),
		       VBC_ADC01_DG, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADCR DG Switch", FUN_REG(AUDIO_CHAN_R),
		       VBC_ADC01_DG, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADC23L DG Switch", FUN_REG(AUDIO_CHAN_L),
		       VBC_ADC23_DG, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADC23R DG Switch", FUN_REG(AUDIO_CHAN_R),
		       VBC_ADC23_DG, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),

	SOC_SINGLE_EXT("VBC STL HPF Switch", FUN_REG(AUDIO_CHAN_L),
		       0, 1, 0, vbc_st_hpf_switch_get, vbc_st_hpf_switch_put),
	SOC_SINGLE_EXT("VBC STR HPF Switch", FUN_REG(AUDIO_CHAN_R),
		       0, 1, 0, vbc_st_hpf_switch_get, vbc_st_hpf_switch_put),
	SOC_SINGLE_EXT("VBC STL HPF Set", FUN_REG(AUDIO_CHAN_L),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_hpf_get, vbc_st_hpf_put),
	SOC_SINGLE_EXT("VBC STR HPF Set", FUN_REG(AUDIO_CHAN_R),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_hpf_get, vbc_st_hpf_put),

	SOC_SINGLE_EXT("VBC AD0 DG Mux", FUN_REG(ADC0_DGMUX), 0, 1, 0,
		       adc_dgmux_get, adc_dgmux_put),
	SOC_SINGLE_EXT("VBC AD1 DG Mux", FUN_REG(ADC1_DGMUX), 0, 1, 0,
		       adc_dgmux_get, adc_dgmux_put),
	SOC_SINGLE_EXT("VBC AD2 DG Mux", FUN_REG(ADC2_DGMUX), 0, 1, 0,
		       adc_dgmux_get, adc_dgmux_put),
	SOC_SINGLE_EXT("VBC AD3 DG Mux", FUN_REG(ADC3_DGMUX), 0, 1, 0,
		       adc_dgmux_get, adc_dgmux_put),
	SOC_ENUM_EXT("VBC DA IIS Mux", vbc_enum[4], dac_iismux_get,
		     dac_iismux_put),
	SOC_SINGLE_EXT("VBC DA EQ Profile Select", FUN_REG(VBC_DA_EQ), 0,
		       VBC_EQ_PROFILE_CNT_MAX, 0,
		       vbc_eq_profile_get, vbc_eq_profile_put),
	SOC_SINGLE_EXT("VBC AD01 EQ Profile Select", FUN_REG(VBC_AD01_EQ), 0,
		       VBC_EQ_PROFILE_CNT_MAX, 0,
		       vbc_eq_profile_get, vbc_eq_profile_put),
	SOC_SINGLE_EXT("VBC AD23 EQ Profile Select", FUN_REG(VBC_AD23_EQ), 0,
		       VBC_EQ_PROFILE_CNT_MAX, 0,
		       vbc_eq_profile_get, vbc_eq_profile_put),
	SOC_SINGLE_EXT("VBC_DA_SRC", SND_SOC_NOPM, 0,
		       U32_MAX, 0,
		       vbc_da_src_get, vbc_da_src_put),
	SOC_SINGLE_BOOL_EXT("VBC_FM_MUTE_DIRECT", 0,
			    vbc_fm_mute_direct_get, vbc_fm_mute_direct_put),
	SOC_SINGLE_BOOL_EXT("VBC_FM_UNMUTE_SMOOTH", 0,
			    vbc_fm_unmute_smooth_get, vbc_fm_unmute_smooth_put),
	SOC_SINGLE_EXT("VBC FM_MUTE_SMOOTHDG STEP", SND_SOC_NOPM,
			-1, 0x1FFF, 0, vbc_fm_mute_smothdg_step_get,
			vbc_fm_mute_smothdg_step_put),
	SOC_SINGLE_BOOL_EXT("VBC_IIS_WIDTH_SEL", 0,
			    vbc_da_iis_wd_get, vbc_da_iis_wd_put),
	SOC_SINGLE_BOOL_EXT("VBC_DA_IIS_LRMOD_SEL", 0,
			    vbc_da_iis_lr_mod_get, vbc_da_iis_lr_mod_put),
	SOC_SINGLE_BOOL_EXT("VBC_AD01_IIS_LRMOD_SEL", 0,
			    vbc_ad01_iis_lr_mod_get, vbc_ad01_iis_lr_mod_put),
	SOC_SINGLE_BOOL_EXT("VBC_AD23_IIS_LRMOD_SEL", 0,
			    vbc_ad23_iis_lr_mod_get, vbc_ad23_iis_lr_mod_put),
	SOC_ENUM_EXT("SYS_IIS0", vbc_sys_iis_enum[SYS_IIS0 - SYS_IIS_START],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS1", vbc_sys_iis_enum[SYS_IIS1 - SYS_IIS_START],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS2", vbc_sys_iis_enum[SYS_IIS2 - SYS_IIS_START],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS3", vbc_sys_iis_enum[SYS_IIS3 - SYS_IIS_START],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_ENUM_EXT("SYS_IIS4", vbc_sys_iis_enum[SYS_IIS4 - SYS_IIS_START],
		     sys_iis_sel_get, sys_iis_sel_put),
	SOC_SINGLE_BOOL_EXT("IIS_BT_FM_LOOP", 0,
		iis_bt_fm_loop_get, iis_bt_fm_loop_put),
	SOC_SINGLE_BOOL_EXT("vbc_access_en", 0,
		vbc_get_access, vbc_put_access),

	SOC_ENUM_EXT("VBC DAC0 Mixer Mux Sel",
		     dac_mixer_mux_enums[CHAN_IDX_0],
		     vbc_dac0_mixer_mux_sel_get,
		     vbc_dac0_mixer_mux_sel_put),
	SOC_ENUM_EXT("VBC DAC1 Mixer Mux Sel",
		     dac_mixer_mux_enums[CHAN_IDX_1],
		     vbc_dac1_mixer_mux_sel_get,
		     vbc_dac1_mixer_mux_sel_put),
	SOC_ENUM_EXT("VBC ST0 Mixer Mux Sel",
		     st_mixer_mux_enums[CHAN_IDX_0],
		     vbc_st0_mixer_mux_sel_get,
		     vbc_st0_mixer_mux_sel_put),
	SOC_ENUM_EXT("VBC ST1 Mixer Mux Sel",
		     st_mixer_mux_enums[CHAN_IDX_1],
		     vbc_st1_mixer_mux_sel_get,
		     vbc_st1_mixer_mux_sel_put),
	SOC_ENUM_EXT("VBC DAC0 Mixer Out Sel",
		     dac_mixer_out_enums[CHAN_IDX_0],
		     vbc_dac0_mixer_out_sel_get,
		     vbc_dac0_mixer_out_sel_put),
	SOC_ENUM_EXT("VBC DAC1 Mixer Out Sel",
		     dac_mixer_out_enums[CHAN_IDX_1],
		     vbc_dac1_mixer_out_sel_get,
		     vbc_dac1_mixer_out_sel_put),
	SOC_ENUM_EXT("VBC ST0 Mixer Out Sel",
		     st_mixer_out_enums[CHAN_IDX_0],
		     vbc_st0_mixer_out_sel_get,
		     vbc_st0_mixer_out_sel_put),
	SOC_ENUM_EXT("VBC ST1 Mixer Out Sel",
		     st_mixer_out_enums[CHAN_IDX_1],
		     vbc_st1_mixer_out_sel_get,
		     vbc_st1_mixer_out_sel_put),
};

static int vbc_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			   unsigned int val)
{
	if (IS_SPRD_VBC_RANG(reg | SPRD_VBC_BASE_HI)) {
		reg |= SPRD_VBC_BASE_HI;
		return vbc_reg_write(reg, val);
	}
	sp_asoc_pr_dbg("The Register is NOT VBC Codec's reg = 0x%x\n", reg);
	return 0;
}

static int vbc_codec_soc_probe(struct snd_soc_codec *codec)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int ret = 0;

	sp_asoc_pr_dbg("%s\n", __func__);

	dapm->idle_bias_off = 1;

	vbc_codec->codec = codec;
	p_eq_setting->codec = codec;

	vbc_proc_init(codec);
	snd_soc_dapm_disable_pin(dapm, "DFM");

	return ret;
}

/* power down chip */
static int vbc_codec_soc_remove(struct snd_soc_codec *codec)
{
	return 0;
}


static const struct snd_soc_codec_driver sprd_vbc_codec = {
	.probe = vbc_codec_soc_probe,
	.remove = vbc_codec_soc_remove,
	.read = vbc_codec_read,
	.write = vbc_codec_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.component_driver = {
		.dapm_widgets = vbc_codec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(vbc_codec_dapm_widgets),
		.dapm_routes = vbc_codec_intercon,
		.num_dapm_routes = ARRAY_SIZE(vbc_codec_intercon),
		.controls = vbc_codec_snd_controls,
		.num_controls = ARRAY_SIZE(vbc_codec_snd_controls),
	}
};

static int sprd_vbc_codec_probe(struct platform_device *pdev)
{
	struct vbc_codec_priv *vbc_codec = NULL;
	struct pinctrl *pctrl;
	int i, j;

	sp_asoc_pr_dbg("%s\n", __func__);
	vbc_codec = platform_get_drvdata(pdev);
	if (vbc_codec == NULL) {
		pr_err("%s %d failed\n", __func__, __LINE__);
		return -1;
	}

	/* AP refer to array vbc_switch_reg_val */
	vbc_codec->vbc_control = VBC_TO_AP_CTRL;
	vbc_codec->vbc_int_control = VBC_INT_TO_AP_CTRL;
	vbc_codec->vbc_dma_control = VBC_DMA_TO_AP_AON_CTRL;
	vbc_codec->vbc_eq_setting.dev = &pdev->dev;
	vbc_codec->st_dg.dg_val[AUDIO_CHAN_L] = 0x18;
	vbc_codec->st_dg.dg_val[AUDIO_CHAN_R] = 0x18;
	vbc_codec->st_dg.hpf_val[AUDIO_CHAN_L] = 0x3;
	vbc_codec->st_dg.hpf_val[AUDIO_CHAN_R] = 0x3;
	vbc_codec->dg[VBC_DAC_DG].dg_val[AUDIO_CHAN_L] = 0x18;
	vbc_codec->dg[VBC_DAC_DG].dg_val[AUDIO_CHAN_R] = 0x18;
	vbc_codec->dg[VBC_ADC01_DG].dg_val[AUDIO_CHAN_L] = 0x18;
	vbc_codec->dg[VBC_ADC01_DG].dg_val[AUDIO_CHAN_R] = 0x18;
	vbc_codec->dg[VBC_ADC23_DG].dg_val[AUDIO_CHAN_L] = 0x18;
	vbc_codec->dg[VBC_ADC23_DG].dg_val[AUDIO_CHAN_R] = 0x18;
	vbc_codec->dg[VBC_DAC_DG].dg_set[AUDIO_CHAN_L] = vbc_da0_dg_set;
	vbc_codec->dg[VBC_DAC_DG].dg_set[AUDIO_CHAN_R] = vbc_da1_dg_set;
	vbc_codec->dg[VBC_ADC01_DG].dg_set[AUDIO_CHAN_L] = vbc_ad0_dg_set;
	vbc_codec->dg[VBC_ADC01_DG].dg_set[AUDIO_CHAN_R] = vbc_ad1_dg_set;
	vbc_codec->dg[VBC_ADC23_DG].dg_set[AUDIO_CHAN_L] = vbc_ad2_dg_set;
	vbc_codec->dg[VBC_ADC23_DG].dg_set[AUDIO_CHAN_R] = vbc_ad3_dg_set;
	vbc_codec->dgmixer01[AUDIO_CHAN_L] = 0x1000;
	vbc_codec->dgmixer01[AUDIO_CHAN_R] = 0x1000;
	vbc_codec->dgmixer23[AUDIO_CHAN_L] = 0x1000;
	vbc_codec->dgmixer23[AUDIO_CHAN_R] = 0x1000;
	for (i = 0; i < VBC_MIXER_MAX; i++) {
		for (j = 0; j < CHAN_IDX_MAX; j++) {
			vbc_codec->mixer_mux_sel[i][j] = MIXER_MUX_BY_EQ;
			vbc_codec->mixer_out_sel[i][j] = MIXER_OUT_BY_EQ;
		}
	}
	vbc_codec->dgmixerstep_da01 = 4096;
	vbc_codec->dgmixerstep_da23 = 1;
	vbc_codec->fm_mutedg_step = 1;
	mutex_init(&vbc_codec->load_mutex);
	mutex_init(&vbc_codec->access_mutex);
	wakeup_source_init(&vbc_codec->wake_lock,
		"vbc-eq-loading");
	spin_lock_init(&(vbc_codec->lock_eq_idx));
	for (i = 0; i < VBC_EQ_MAX; i++)
		vbc_codec->vbc_eq_setting.now_profile[i] = -1;

	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		pr_err("%s, get pinctrl failed!\n", __func__);
		return PTR_ERR(pctrl);
	}
	vbc_codec->pctrl = pctrl;

	return 0;
}

static int sprd_vbc_codec_remove(struct platform_device *pdev)
{
	struct vbc_codec_priv *vbc_codec = platform_get_drvdata(pdev);

	wakeup_source_trash(&vbc_codec->wake_lock);
	vbc_codec->vbc_eq_setting.dev = (struct device *)0;

	return 0;
}

MODULE_DESCRIPTION("SPRD ASoC VBC Component Driver");
MODULE_AUTHOR("Zhenfang Wang <zhenfang.wang@spreadtrum.com>");
MODULE_AUTHOR("Ken.Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Component:VBC Component");
