/*
 * sound/soc/sprd/codec/sprd/sc2721/sprd-codec.c
 *
 * SPRD-CODEC -- SpreadTrum Tiger intergrated codec.
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
#define pr_fmt(fmt) pr_sprd_fmt("SC2730")""fmt

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

#include "sprd-audio.h"
#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"
#include "sprd-codec.h"
#include "sprd-headset.h"

#define SOC_REG(r) ((unsigned short)(r))
#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#include "aud_topa_rf.h"

#define SPRD_CODEC_AP_BASE_HI (SPRD_CODEC_AP_BASE & 0xFFFF0000)
#define SPRD_CODEC_DP_BASE_HI (SPRD_CODEC_DP_BASE & 0xFFFF0000)

#define UNSUPPORTED_AD_RATE SNDRV_PCM_RATE_44100

#define SDM_RAMP_MAX 0x2000
#define NEED_SDM_RAMP(sprd_codec, codec) \
	(sprd_codec_get_ctrl(codec, "HPL EAR Sel") == 1 && \
	!sprd_codec->hp_mix_mode)

#define SPRD_CODEC_EFUSE_FGU_4P2_MASK GENMASK(8, 0)

enum PA_SHORT_T {
	PA_SHORT_NONE, /* PA output normal */
	PA_SHORT_VBAT, /* PA output P/N short VBAT */
	PA_SHORT_GND, /* PA output P/N short GND */
	PA_SHORT_CHECK_FAILED, /* error when do the check */
	PA_SHORT_MAX
};

enum CP_SHORT_T {
	CP_SHORT_NONE,
	FLYP_SHORT_POWER,
	FLYP_SHORT_GND,
	FLYN_SHORT_POWER,
	FLYN_SHORT_GND,
	VCPM_SHORT_POWER,
	VCPN_SHORT_GND,
	CP_SHORT_MAX
};


enum {
	SPRD_CODEC_ANA_MIXER_ORDER = 98,
	SPRD_CODEC_PA_ORDER = 99,
	SPRD_CODEC_DEPOP_ORDER = 100,
	SPRD_CODEC_BUF_SWITCH_ORDER = 101,
	SPRD_CODEC_SWITCH_ORDER = 102,
	SPRD_CODEC_DA_EN_ORDER = 103,
	SPRD_CODEC_DC_OS_SWITCH_ORDER = 104,
	SPRD_CODEC_DC_OS_ORDER = 105,
	SPRD_CODEC_RCV_DEPOP_ORDER = 106,
	SPRD_CODEC_MIXER_ORDER = 110,/* Must be the last one */
};

enum {
	SPRD_CODEC_PLAYBACK,
	SPRD_CODEC_CAPTRUE,
	SPRD_CODEC_CAPTRUE1,
	SPRD_CODEC_CHAN_MAX,
};

enum {
	PSG_STATE_BOOST_NONE,
	PSG_STATE_BOOST_LARGE_GAIN,
	PSG_STATE_BOOST_SMALL_GAIN,
	PSG_STATE_BOOST_BYPASS,
};
static const char *sprd_codec_chan_name[SPRD_CODEC_CHAN_MAX] = {
	"DAC",
	"ADC",
	"ADC1",
};

static inline const char *sprd_codec_chan_get_name(int chan_id)
{
	return sprd_codec_chan_name[chan_id];
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int agdsp_access_enable(void)
	__attribute__ ((weak, alias("__agdsp_access_enable")));
static int __agdsp_access_enable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}

int agdsp_access_disable(void)
	__attribute__ ((weak, alias("__agdsp_access_disable")));
static int __agdsp_access_disable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}
#pragma GCC diagnostic pop

struct inter_pa {
	/* FIXME little endian */
	u32 DTRI_F_sel:3;
	u32 is_DEMI_mode:1;
	u32 is_classD_mode:1;
	u32 EMI_rate:3;
	u32 RESV:25;
};

struct pa_setting {
	union {
		struct inter_pa setting;
		u32 value;
	};
	int set;
};

static int sprd_pga_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static int sprd_pga_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static int sprd_mixer_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static int sprd_mixer_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static int sprd_codec_spk_pga_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static int sprd_codec_spk_pga_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static void sprd_codec_sdm_init(struct snd_soc_codec *codec);

static int sprd_codec_get_ctrl(struct snd_soc_codec *codec, char *name);
static void sprd_codec_psg_state_init(struct snd_soc_codec *codec);
static void sprd_codec_psg_state_exit(struct snd_soc_codec *codec);

static unsigned long sprd_codec_dp_base;

enum {
	CODEC_PATH_DA = 0,
	CODEC_PATH_AD,
	CODEC_PATH_AD1,
	CODEC_PATH_MAX
};

enum {
	LRCLK_SEL_DAC,
	LRCLK_SEL_ADC,
	LRCLK_SEL_ADC1,
	LRCLK_SEL_MAX
};

struct fgu {
	unsigned int vh;
	unsigned int dvh;
	unsigned int vl;
	unsigned int dvl;
	unsigned int high_thrd;
	unsigned int low_thrd;
};
/* codec private data */
struct sprd_codec_priv {
	struct snd_soc_codec *codec;
	u32 da_sample_val;
	u32 ad_sample_val;
	u32 ad1_sample_val;
	int dp_irq;
	struct completion completion_dac_mute;

	struct pa_setting inter_pa;

	struct regulator *main_mic;
	struct regulator *head_mic;
	struct regulator *vb;

	int psg_state;
	struct fgu fgu;

	u32 fixed_sample_rate[CODEC_PATH_MAX];
	u32 lrclk_sel[LRCLK_SEL_MAX];
	unsigned int replace_rate;
	enum PA_SHORT_T pa_short_stat;
	enum CP_SHORT_T cp_short_stat;

	u32 startup_cnt;
	struct mutex digital_enable_mutex;
	u32 digital_enable_count;
	u16 dac_switch;
	u16 adc_switch;
	u32 aud_pabst_vcal;
	u32 neg_cp_efuse;
	u32 fgu_4p2_efuse;
	u32 hp_mix_mode;
	u32 spk_dg;
	u32 spk_fall_dg;
	struct mutex dig_access_mutex;
	bool dig_access_en;
	bool user_dig_access_dis;
};


static const char * const das_input_mux_texts[] = {
	"L+R", "L*2", "R*2", "ZERO"
};

static const SOC_ENUM_SINGLE_DECL(
	das_input_mux_enum, SND_SOC_NOPM,
	0, das_input_mux_texts);

static const DECLARE_TLV_DB_SCALE(adc_tlv, 0, 300, 0);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -2400, 300, 1);
static const DECLARE_TLV_DB_SCALE(ear_tlv, -2400, 300, 1);
/* todo: remap it, ANA_CDC9[14:12] */
static const DECLARE_TLV_DB_SCALE(spk_tlv, 0, 150, 0);
/* todo: remap it, ANA_CDC8[5:4] */
static const DECLARE_TLV_DB_SCALE(dac_tlv, -1500, 75, 0);

static const char * const lrclk_sel_text[] = {
	"normal", "invert"
};

static const struct soc_enum lrclk_sel_enum =
	SOC_ENUM_SINGLE_EXT(2, lrclk_sel_text);

static const char * const codec_hw_info[] = {
	CODEC_HW_INFO
};
static const struct soc_enum codec_info_enum =
	SOC_ENUM_SINGLE_EXT(SP_AUDIO_CODEC_NUM, codec_hw_info);

#define SPRD_CODEC_PGA_M(xname, xreg, xshift, max, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, xreg, xshift, max, 0, \
		sprd_pga_get, sprd_pga_put, tlv_array)

#define SPRD_CODEC_PGA_MAX_INVERT(xname, xreg, xshift, max, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, xreg, xshift, max, 1, \
		sprd_pga_get, sprd_pga_put, tlv_array)

#define SPRD_CODEC_MIXER(xname, xreg, xshift)\
	SOC_SINGLE_EXT(xname, xreg, xshift, 1, 0, \
		sprd_mixer_get, sprd_mixer_put)

#define SPRD_CODEC_AD_MIXER(xname, xreg, xshift)\
	SOC_SINGLE_EXT(xname, xreg, xshift, 1, 0, \
		snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw)

static const struct snd_kcontrol_new spkl_pga_controls[] = {
	SOC_SINGLE_EXT_TLV("SPKL Playback Volume", SOC_REG(ANA_CDC8), PA_G_S,
			   15, 0, sprd_codec_spk_pga_get,
			   sprd_codec_spk_pga_put, spk_tlv),
};

static const struct snd_kcontrol_new ear_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("EAR Playback Volume",
		SOC_REG(ANA_CDC7), RCV_G_S, 15, ear_tlv),
};

static const struct snd_kcontrol_new hpl_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("HPL Playback Volume",
		SOC_REG(ANA_CDC7), HPL_G_S, 15, hp_tlv),
};

static const struct snd_kcontrol_new hpr_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("HPR Playback Volume",
		SOC_REG(ANA_CDC7), HPR_G_S, 15, hp_tlv),
};

static const struct snd_kcontrol_new adcl_pga_controls[] = {
	SPRD_CODEC_PGA_M("ADCL Capture Volume",
		SOC_REG(ANA_CDC2), ADPGAL_G_S, 7, adc_tlv),
};

static const struct snd_kcontrol_new adcr_pga_controls[] = {

	SPRD_CODEC_PGA_M("ADCR Capture Volume",
		SOC_REG(ANA_CDC2), ADPGAR_G_S, 7, adc_tlv),
};

static const struct snd_kcontrol_new dac_pga_controls[] = {
	SPRD_CODEC_PGA_MAX_INVERT("DAC Playback Volume",
		SOC_REG(ANA_CDC5), DA_IG_S, 2, dac_tlv),
};

/* ADCL Mixer */
static const struct snd_kcontrol_new adcl_mixer_controls[] = {
	SPRD_CODEC_AD_MIXER("MainMICADCL Switch", SOC_REG(ANA_CDC1),
		SMIC1PGAL_S),
	SPRD_CODEC_AD_MIXER("HPMICADCL Switch", SOC_REG(ANA_CDC1), SHMICPGAL_S),
	SPRD_CODEC_AD_MIXER("VSENSEL Switch", SOC_REG(ANA_CDC1), SVSNSADL_S),
};

/* ADCR Mixer */
static const struct snd_kcontrol_new adcr_mixer_controls[] = {
	SPRD_CODEC_AD_MIXER("AuxMICADCR Switch", SOC_REG(ANA_CDC1),
		SMIC2PGAR_S),
	SPRD_CODEC_AD_MIXER("HPMICADCR Switch", SOC_REG(ANA_CDC1), SHMICPGAR_S),
	SPRD_CODEC_AD_MIXER("ISENSER Switch", SOC_REG(ANA_CDC1), SISNSADR_S),
};

/* HPL Mixer */
static const struct snd_kcontrol_new hpl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPL Switch", SND_SOC_NOPM, SDALHPL_S),
	SPRD_CODEC_MIXER("DACRHPL Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCLHPL Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCRHPL Switch", SND_SOC_NOPM, 0),
};

/* HPR Mixer */
static const struct snd_kcontrol_new hpr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPR Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("DACRHPR Switch", SND_SOC_NOPM, SDARHPR_S),
	SPRD_CODEC_MIXER("ADCLHPR Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCRHPR Switch", SND_SOC_NOPM, 0),
};

/* SPKL Mixer */
/* TODO: adjust speaker mixer. */
static const struct snd_kcontrol_new spkl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKL Switch", SND_SOC_NOPM, SDAPA_S),
	SPRD_CODEC_MIXER("DACRSPKL Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCLSPKL Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCRSPKL Switch", SND_SOC_NOPM, 0),
};

/* SPKR Mixer */
/* TODO: should be removed. */
static const struct snd_kcontrol_new spkr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKR Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("DACRSPKR Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCLSPKR Switch", SND_SOC_NOPM, 0),
	SPRD_CODEC_MIXER("ADCRSPKR Switch", SND_SOC_NOPM, 0),
};

static const struct snd_kcontrol_new ear_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLEAR Switch", SND_SOC_NOPM, SDALRCV_S),
};

/* TOTO: Already defined, should be removed */
static const struct snd_kcontrol_new spaivsns_mixer_controls[] = {
	SPRD_CODEC_MIXER("SPAISNS Switch", SND_SOC_NOPM, SPAISNS_S),
	SPRD_CODEC_MIXER("SPAVSNS Switch", SND_SOC_NOPM, SPAVSNS_S),
};

static const struct snd_kcontrol_new loop_controls[] = {
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
	SOC_SINGLE_EXT("switch", SND_SOC_NOPM, 0,
		1, 0, snd_soc_dapm_get_volsw, snd_soc_dapm_put_volsw),
};

static const struct snd_kcontrol_new da_mode_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new hp_jack_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new virt_output_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new ivsence_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new aud_adc_switch[] = {
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
};

static const u16 ocp_pfm_cfg_table[16] = {
	0x0406, 0x1618, 0x292b, 0x3a3b, 0x4344, 0x5457, 0x6668, 0x787a,
	0x8082, 0x9095, 0xa4a7, 0xb6b8, 0xc0c0, 0xd0d3, 0xe3e5, 0xf5f6
};

static int sprd_codec_power_get(struct device *dev, struct regulator **regu,
				const char *id)
{
	if (!*regu) {
		*regu = regulator_get(dev, id);
		if (IS_ERR(*regu)) {
			pr_err("ERR:Failed to request %ld: %s\n",
			       PTR_ERR(*regu), id);
			*regu = 0;
			return -1;
		}
	}
	return 0;
}

static int sprd_codec_power_put(struct regulator **regu)
{
	if (*regu) {
		regulator_set_mode(*regu, REGULATOR_MODE_NORMAL);
		regulator_disable(*regu);
		regulator_put(*regu);
		*regu = 0;
	}
	return 0;
}

static void codec_digital_reg_restore(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&sprd_codec->digital_enable_mutex);
	if (sprd_codec->digital_enable_count)
		arch_audio_codec_digital_reg_enable();
	mutex_unlock(&sprd_codec->digital_enable_mutex);
}

static int dig_access_disable_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	bool disable = !!ucontrol->value.integer.value[0];

	mutex_lock(&sprd_codec->dig_access_mutex);
	if (sprd_codec->dig_access_en) {
		if (disable == sprd_codec->user_dig_access_dis) {
			mutex_unlock(&sprd_codec->dig_access_mutex);
			return 0;
		}
		if (disable) {
			pr_info("%s, disable agdsp access\n", __func__);
			sprd_codec->user_dig_access_dis = disable;
			agdsp_access_disable();
		} else {
			pr_info("%s, enable agdsp access\n", __func__);
			if (agdsp_access_enable()) {
				pr_err("%s, agdsp_access_enable failed!\n",
				       __func__);
				mutex_unlock(&sprd_codec->dig_access_mutex);
				return -EIO;
			}
			codec_digital_reg_restore(codec);
			sprd_codec->user_dig_access_dis = disable;
		}
	}
	mutex_unlock(&sprd_codec->dig_access_mutex);

	return 0;
}

static int dig_access_disable_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->user_dig_access_dis;

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

/* unit: ms */
static void sprd_codec_wait(u32 wait_time)
{
	pr_debug("%s %d ms\n", __func__, wait_time);

	if (wait_time < 0)
		return;
	if (wait_time < 20)
		usleep_range(wait_time * 1000, wait_time * 1000 + 200);
	else
		msleep(wait_time);
}

static struct snd_kcontrol *sprd_codec_find_ctrl(struct snd_soc_codec *codec,
						 char *name)
{
	struct snd_soc_card *card = codec ? codec->component.card : NULL;
	struct snd_ctl_elem_id id = {.iface = SNDRV_CTL_ELEM_IFACE_MIXER};

	if (!codec || !name)
		return NULL;
	memcpy(id.name, name, strlen(name));

	return snd_ctl_find_id(card->snd_card, &id);
}

static int sprd_codec_get_ctrl(struct snd_soc_codec *codec, char *name)
{
	struct snd_kcontrol *kctrl;
	int ret = 0;

	kctrl = sprd_codec_find_ctrl(codec, name);
	if (kctrl)
		ret = dapm_kcontrol_get_value(kctrl);

	return ret;
}

static int sprd_codec_read_efuse(struct platform_device *pdev,
					const char *cell_name, u32 *data)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len;

	cell = nvmem_cell_get(&pdev->dev, cell_name);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	*data = calib_data;
	kfree(buf);

	return 0;
}

static int sprd_pga_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int mask = BIT(fls(mc->max)) - 1;
	unsigned int val;

	val = snd_soc_read(codec, mc->reg);
	val = (val >> mc->shift) & mask;

	ucontrol->value.integer.value[0] = val;
	return 0;
}

static int sprd_pga_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int mask = BIT(fls(mc->max)) - 1;
	unsigned int val = ucontrol->value.integer.value[0];

	snd_soc_update_bits(codec, mc->reg,
		mask << mc->shift,
		val << mc->shift);

	return 0;
}

static int sprd_codec_spk_pga_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->spk_dg;
	return 0;
}

static int sprd_codec_spk_pga_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->spk_dg = ucontrol->value.integer.value[0];
	if (sprd_codec->psg_state > PSG_STATE_BOOST_LARGE_GAIN)
		ucontrol->value.integer.value[0] = sprd_codec->spk_fall_dg;
	sprd_pga_put(kcontrol, ucontrol);
	return 0;
}

static int sprd_codec_spk_pga_update(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct snd_kcontrol *kcontrol;
	struct snd_ctl_elem_value ucontrol;

	kcontrol = sprd_codec_find_ctrl(codec,
					"SPKL Gain SPKL Playback Volume");
	if (!kcontrol) {
		pr_err("SPK volume control not sound\n");
		return -EINVAL;
	}
	ucontrol.value.integer.value[0] = sprd_codec->spk_dg;
	sprd_codec_spk_pga_put(kcontrol, &ucontrol);
	return 0;
}

static int sprd_mixer_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	val = sprd_codec->dac_switch & (1 << mc->shift);
	ucontrol->value.integer.value[0] = val;

	sp_asoc_pr_info("dac switch %d,shift=%d,get=%d\n",
		sprd_codec->dac_switch, mc->shift,
		(int)ucontrol->value.integer.value[0]);

	return 0;
}

static int sprd_mixer_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		 snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(dapm);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int val;

	val = 1 << mc->shift;
	if (ucontrol->value.integer.value[0])
		sprd_codec->dac_switch |= val;
	else
		sprd_codec->dac_switch &= ~val;

	sp_asoc_pr_info("dac switch %d,shift=%d,set=%d\n",
		sprd_codec->dac_switch, mc->shift,
		(int)ucontrol->value.integer.value[0]);

	snd_soc_dapm_put_volsw(kcontrol, ucontrol);
	return 0;
}

static void update_switch(struct snd_soc_codec *codec, u32 path, u32 on)
{
	u32 val = 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	if (on)
		val = sprd_codec->dac_switch & path;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC9), path, val);
}

static inline void sprd_codec_vcm_v_sel(struct snd_soc_codec *codec, int v_sel)
{
/* yintang marked for compiling.
 * int mask;
 * int val;
 * sp_asoc_pr_dbg("VCM Set %d\n", v_sel);
 * mask = VB_V_MASK << VB_V;
 * val = (v_sel << VB_V) & mask;
 * snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), mask, val);
 */
}

/* das dc offset setting */
/* TODO: add dc offset to AudioTestor. */
static int sprd_das_dc_os_en(struct snd_soc_codec *codec, int on)
{
	int mask, val;

	mask = DAS_OS_EN;
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), mask, val);

	return 0;
}

static void sprd_das_dc_os_set(struct snd_soc_codec *codec)
{
	int mask, val;

	sprd_codec_sdm_init(codec);
	mask = DAS_OS(0xFFFF);
	val = DAS_OS(1);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC6), mask, val);
	sprd_codec_audif_dc_os_set(codec, 6);
}

static void load_ocp_pfw_cfg(struct sprd_codec_priv *sprd_codec)
{
	struct snd_soc_codec *codec;

	int i;
	int val;

	codec = sprd_codec ? sprd_codec->codec : NULL;
	if (!codec)
		return;
	for (i = 0;
			i < ARRAY_SIZE(ocp_pfm_cfg_table) && i <
				ARRAY_SIZE(ocp_pfm_cfg_table);
			i++) {
		val = ocp_pfm_cfg_table[i];
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU22), 0xFFFF, val);

		snd_soc_update_bits(codec, SOC_REG(ANA_PMU23),
			PABST_CLIMIT_PFM_CFG_WR_EN, PABST_CLIMIT_PFM_CFG_WR_EN);
	}
}

static int das_dc_os_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (on)
		sprd_das_dc_os_set(codec);
	sprd_das_dc_os_en(codec, on);

	return 0;
}

static int dacs_switch_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__,
		get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC17),
			PA_RSV(0xFFFF), 0);
		usleep_range(100, 110);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(100, 110);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC19),
			PA_DPOP_RC_L(0xFFFF), PA_DPOP_RC_L(3));
		usleep_range(10, 15);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC17),
			PA_RSV(0xFFFF), PA_RSV(8));
		sprd_codec_wait(20);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC19),
			PA_DPOP_RC_L(0xFFFF), PA_DPOP_RC_L(0));
		usleep_range(100, 110);
		update_switch(codec, SDAPA, 1);
		usleep_range(100, 110);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		update_switch(codec, SDAPA, 0);
		usleep_range(10, 15);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC19),
			PA_DPOP_RC_L(0xFFFF), PA_DPOP_RC_L(3));
		sprd_codec_wait(20);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC17),
			PA_RSV(0xFFFF), 0);
		usleep_range(10, 15);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC19),
			PA_DPOP_RC_L(0xFFFF), 0);
		usleep_range(100, 110);
		break;
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(100, 110);
		break;
	default:
		break;
	}
	return ret;
}

static void sprd_codec_pa_boost(struct snd_soc_codec *codec, int pa_d_en)
{
	int mask;
	int value;
	int i = 0, state = 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	mask = PABST_WR_PROT_VALUE(0xFFFF);
	value = 0x3FA1;
	snd_soc_update_bits(codec, SOC_REG(ANA_WR_PROT0), mask, value);
	/*
	 * If classAB, select normal mode;
	 * then,
	 * if voltage is low, select normal mode;
	 * else, select boost mode.
	 * here we need read battery Q info.
	 */
	if (pa_d_en) {
		mask = DIG_CLK_PABST_EN;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL1), mask, mask);
		mask = CLK_PABST_EN | CLK_PABST_32K_EN;
		snd_soc_update_bits(codec, SOC_REG(ANA_CLK0), mask, mask);
		mask = PA_VCM_BYP_SEL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC15), mask, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU6),
			PABST_V_INIT(0xffff), 0);

		/* set vcal */
		mask = PABST_V_CAL(0xffff);
		value = PABST_V_CAL(sprd_codec->aud_pabst_vcal >> 10);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU4), mask, value);

		mask = PABST_V(0xffff) | PABST_VADJ_TRIG;
		 /* defaut is 0x5A->6v, 0x49 ->5.5v */
		value = PABST_V(0x49) | PABST_VADJ_TRIG;
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU6), mask, value);
		mask = PABST_EN;
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU20), mask, mask);

		/* wait time is about 16ms per ASIC requirement */
		sprd_codec_wait(10);
		while (i++ < 10) {
			state = snd_soc_read(codec, SOC_REG(ANA_STS8)) &
				PABST_SOFT_DVLD;
			if (state)
				break;
			sprd_codec_wait(1);
		}
	} else {
		mask = PA_VCM_BYP_SEL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC15), mask, 0);
		mask = PABST_EN;
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU20), mask, 0);
		mask = CLK_PABST_EN | CLK_PABST_32K_EN;
		snd_soc_update_bits(codec, SOC_REG(ANA_CLK0), mask, 0);
		mask = DIG_CLK_PABST_EN;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL1), mask, 0);
	}
}

/* inter PA */
static inline void sprd_codec_pa_d_en(struct snd_soc_codec *codec, int on,
				      int force)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	mask = PA_D_EN;
	val = on ? mask : 0;
	if (force)
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC14), mask, val);
	if (on)
		sprd_codec_psg_state_init(codec);
	else
		sprd_codec_psg_state_exit(codec);
	sprd_codec_intc_enable(on, FGU_LOW_LIMIT_INT_EN |
			       FGU_HIGH_LIMIT_INT_EN);
	snd_soc_update_bits(codec, SOC_REG(ANA_DCL2), FGU_DIG_EN,
			    on ? FGU_DIG_EN : 0);
}

static inline void sprd_codec_pa_emi_rate(struct snd_soc_codec *codec, int rate)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("%s set %d\n", __func__, rate);

	mask = PA_EMI_L(0xFFFF);
	val = PA_EMI_L(rate);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask, val);
}

static inline void sprd_codec_pa_dtri_f_sel(struct snd_soc_codec *codec,
					    int f_sel)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("%s set %d\n", __func__, f_sel);
	mask = PA_DTRI_FC(0xFFFF);
	val = PA_DTRI_FC(f_sel);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC16), mask, val);
}

static inline void sprd_codec_pa_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s set %d\n", __func__, on);

	if (sprd_codec->pa_short_stat != PA_SHORT_NONE) {
		pr_err("ERR: Speaker PA P/N short stat: %d",
		       sprd_codec->pa_short_stat);
		return;
	}

	mask = PA_EN;
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC14), mask, val);
}

static inline void sprd_codec_inter_pa_init(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec->inter_pa.setting.DTRI_F_sel = 0x2;
	sprd_codec->spk_fall_dg = 0x1;
}

/* NOTE: VB, BG & BIAS must be enabled before calling this function. */
static void spk_pa_short_check(struct sprd_codec_priv *sprd_codec)
{
#ifdef SPRD_CODEC_IMPD
	struct snd_soc_codec *codec;

	int val, mask;
	u32 flag;

	codec = sprd_codec ? sprd_codec->codec : NULL;
	if (!codec)
		return;

	snd_soc_update_bits(codec, SOC_REG(ANA_CDC14), PA_EN, 0);
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC20),
		PA_SHORTH_DET_EN, PA_SHORTH_DET_EN);
	sprd_codec_wait(1);
	flag = snd_soc_read(codec, SOC_REG(ANA_CDC14)) | PA_SHT_FLAG(0xFFFF);
	if (flag) {
		sprd_codec->pa_short_stat = PA_SHORT_VBAT;
		return;
	}
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC20),
		PA_SHORTL_DET_EN, PA_SHORTL_DET_EN);
	sprd_codec_wait(1);
	flag = snd_soc_read(codec, SOC_REG(ANA_CDC14)) | PA_SHT_FLAG(0xFFFF);
	if (flag) {
		sprd_codec->pa_short_stat = PA_SHORT_GND;
		return;
	}

	snd_soc_update_bits(codec, SOC_REG(ANA_DCL1),
		DCL_EN, DCL_EN);
	mask = ANA_CLK_EN | CLK_DIG_6M5_EN | CLK_DCL_6M5_EN | CLK_DCL_32K_EN;
	snd_soc_update_bits(codec, SOC_REG(ANA_CLK0), mask, mask);
	snd_soc_update_bits(codec, SOC_REG(ANA_DCL1),
		DIG_CLK_IMPD_EN, DIG_CLK_IMPD_EN);
	snd_soc_update_bits(codec, SOC_REG(ANA_CLK0),
		CLK_IMPD_EN, CLK_IMPD_EN);


	snd_soc_update_bits(codec, SOC_REG(ANA_CDC0),
		PGA_ADC_IBIAS_EN, PGA_ADC_IBIAS_EN);
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
		VB_EN, VB_EN);

	snd_soc_update_bits(codec, SOC_REG(ANA_CDC20),
		PA_IMPD_DET_EN, PA_IMPD_DET_EN);
	snd_soc_update_bits(codec, SOC_REG(ANA_DCL0),
		RSTN_AUD_DIG_IMPD_ADC, RSTN_AUD_DIG_IMPD_ADC);

	snd_soc_update_bits(codec, SOC_REG(ANA_IMPD0),
		IMPD_ADC_EN, IMPD_ADC_EN);
	snd_soc_update_bits(codec, SOC_REG(ANA_IMPD1),
		IMPD_ADC_CLK_SEL(0xFFFF), 0);

	mask = IMPD_BUF_EN | IMPD_CH_SEL(0xFFFF);
	val = IMPD_BUF_EN | IMPD_CH_SEL(4);
	snd_soc_update_bits(codec, SOC_REG(ANA_IMPD0),
		mask, val);

	mask = IMPD_TIME_CNT_SEL(0xFFFF) | IMPD_CUR_EN;
	val = IMPD_TIME_CNT_SEL(5) | IMPD_CUR_EN;
	snd_soc_update_bits(codec, SOC_REG(ANA_IMPD2),
		mask, val);
	sprd_codec_wait(60);

	val = snd_soc_read(codec, SOC_REG(ANA_STS14)) | IMPD_ADC_DATO(0xFFFF);

	/* yintang: TBD. calculate the impd per the val */
#endif
}

static void spk_pa_config(struct snd_soc_codec *codec,
			 struct inter_pa setting)
{
	sp_asoc_pr_dbg("%s:is_classD_mode: %d\n", __func__,
			setting.is_classD_mode);
	sprd_codec_pa_d_en(codec, setting.is_classD_mode, 1);
	if (setting.is_DEMI_mode)
		sprd_codec_pa_emi_rate(codec, setting.EMI_rate);
	sprd_codec_pa_dtri_f_sel(codec, setting.DTRI_F_sel);
}

static int spk_pa_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct inter_pa setting;
	u32 state;
	int i = 0;

	setting = sprd_codec->inter_pa.setting;
	if (on) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC19),
			PA_DPOP_RC_L(0xFFFF), 0);

		spk_pa_config(codec, setting);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC14),
			PA_DPOP_BYP_EN, 0);
		sprd_codec_wait(1);
		sprd_codec_pa_en(codec, 1);

		/* wait time is about 60 ~ 195 */
		sprd_codec_wait(60);
		while (i++ < 135) {
			state = snd_soc_read(codec, SOC_REG(ANA_STS11)) &
					     PA_AB_DPOP_DVLD;
			if (state)
				break;
			sprd_codec_wait(1);
		}
		sp_asoc_pr_dbg("%s wait time %d\n", __func__, 60 + i);
	} else {
		sprd_codec_pa_d_en(codec, 0, 0);
		sprd_codec_pa_en(codec, 0);
		/* PA close need take some time, 2ms is advice from ASIC */
		sprd_codec_wait(2);
	}

	return 0;
}

static void sprd_codec_sdm_ramp(struct snd_soc_codec *codec, bool on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int val = 0;

	sp_asoc_pr_dbg("%s hp_mix_mode=%d,on=%d\n",
		__func__, sprd_codec->hp_mix_mode, on);

	/*
	 * if hp_mix_mode is true, do not ramp,only hp single mode would do;
	 * if kctrl "HPL EAR Sel" is EAR mode(0), do not ramp;
	 */
	if (!NEED_SDM_RAMP(sprd_codec, codec))
		return;

	/* each step should wait 0.5ms per the guideline */
	if (on) {
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L), 0xffff, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_H), 0xffff, 0);

		while (val <= SDM_RAMP_MAX) {
			snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L),
					0xffff, val);
			val += 0x20;
			/* ramp need wait about 500us by ASIC requirement */
			usleep_range(500, 510);
		}
	} else {
		val = snd_soc_read(codec, SOC_REG(AUD_DAC_SDM_L));
		while (val > 0x20) {
			val -= 0x20;
			snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L),
					0xffff, val);
			/* ramp need wait about 500us by ASIC requirement */
			usleep_range(500, 510);
		}
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L), 0xffff, 0);
	}
}

static void sprd_dalr_dc_os_set(struct snd_soc_codec *codec)
{
	int mask = DALR_OS(0xffff), val;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	/* if need SDM ramp, os should set to 0 */
	if (NEED_SDM_RAMP(sprd_codec, codec)) {
		val = DALR_OS(0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), mask, val);
		sprd_codec_audif_dc_os_set(codec, 0);
	} else {
		val = DALR_OS(1);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5), mask, val);
		sprd_codec_audif_dc_os_set(codec, 6);
	}
}

static int dalr_dc_os_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	int i = 0, state;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (on) {
		sprd_codec_sdm_init(codec);
		sprd_dalr_dc_os_set(codec);

		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
			DAL_EN|DAR_EN, DAL_EN|DAR_EN);
		sprd_codec_wait(1);
		update_switch(codec, SDALHPL | SDARHPR, on);
	} else {
		sprd_codec_sdm_ramp(codec, on);
		update_switch(codec, SDALHPL | SDARHPR, on);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC5),
				    DAL_EN | DAR_EN, 0);
	}
	while (i++ < 20) {
		state = snd_soc_read(codec, SOC_REG(ANA_STS1));

		/* if on, DVLD must be 1 */
		if (on && (state & (HPL_DPOP_DVLD | HPR_DPOP_DVLD)))
			break;
		/* if !on, DVLD must be 0 */
		else if (!on && !(state & (HPL_DPOP_DVLD | HPR_DPOP_DVLD)))
			break;

		sprd_codec_wait(10);
	}

	if (i >= 20)
		sp_asoc_pr_info("%s Dpop failed!\n", __func__);
	else
		sp_asoc_pr_info("%s Dpop sucessed! i=%d, ANA_STS1=0x%x\n",
			__func__, i, state);

	if (on)
		sprd_codec_sdm_ramp(codec, on);

	return 0;
}

static int sprd_codec_set_sample_rate(struct snd_soc_codec *codec,
				      u32 rate,
				      int mask, int shift)
{
	int i = 0;
	unsigned int val = 0;
	int ret;
	struct sprd_codec_rate_tbl {
		u32 rate; /* 8000, 11025, ... */
		/* SPRD_CODEC_RATE_xxx, ... */
		u32 sprd_rate;
	} rate_tbl[] = {
		/* rate in Hz, SPRD rate format */
		{48000, SPRD_CODEC_RATE_48000},
		{8000, SPRD_CODEC_RATE_8000},
		{11025, SPRD_CODEC_RATE_11025},
		{16000, SPRD_CODEC_RATE_16000},
		{22050, SPRD_CODEC_RATE_22050},
		{32000, SPRD_CODEC_RATE_32000},
		{44100, SPRD_CODEC_RATE_44100},
		{96000, SPRD_CODEC_RATE_96000},
	};

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(rate_tbl); i++) {
		if (rate_tbl[i].rate == rate)
			val = rate_tbl[i].sprd_rate;
	}

	if (val)
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), mask,
				    val << shift);
	else
		pr_err("ERR:SPRD-CODEC not support this rate %d\n", rate);

	sp_asoc_pr_dbg("Set Playback rate 0x%x\n",
		       snd_soc_read(codec, AUD_DAC_CTL));

	agdsp_access_disable();

	return 0;
}

static int is_unsupported_ad_rate(unsigned int rate)
{
	return (snd_pcm_rate_to_rate_bit(rate) & UNSUPPORTED_AD_RATE);
}

static int sprd_codec_set_ad_sample_rate(struct snd_soc_codec *codec,
					 u32 rate,
					 int mask, int shift)
{
	int set;
	int ret;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int replace_rate = sprd_codec->replace_rate;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}

	if (is_unsupported_ad_rate(rate) && replace_rate) {
		pr_debug("Replace %uHz with %uHz for record.\n",
			 rate, replace_rate);
		rate = replace_rate;
	}

	set = rate / 4000;
	if (set > 13)
		pr_err("ERR:SPRD-CODEC not support this ad rate %d\n", rate);
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC_CTL),
		mask << shift, set << shift);

	agdsp_access_disable();

	return 0;
}

static int sprd_codec_sample_rate_setting(struct sprd_codec_priv *sprd_codec)
{
	int ret;
	sp_asoc_pr_info("%s AD %u DA %u AD1 %u\n", __func__,
		       sprd_codec->ad_sample_val, sprd_codec->da_sample_val,
		       sprd_codec->ad1_sample_val);
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}

	if (sprd_codec->ad_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
			sprd_codec->ad_sample_val, ADC_SRC_N_MASK,
			ADC_SRC_N);
	}
	if (sprd_codec->ad1_sample_val) {
		sprd_codec_set_ad_sample_rate(sprd_codec->codec,
			sprd_codec->ad1_sample_val, ADC1_SRC_N_MASK,
			ADC1_SRC_N);
	}
	if (sprd_codec->da_sample_val) {
		sprd_codec_set_sample_rate(sprd_codec->codec,
			sprd_codec->da_sample_val,
			DAC_FS_MODE_MASK, DAC_FS_MODE);
	}

	agdsp_access_disable();

	return 0;
}

static void sprd_codec_sdm_init(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s hp_mix_mode=%d\n",
		__func__, sprd_codec->hp_mix_mode);

	if (NEED_SDM_RAMP(sprd_codec, codec)) {
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L), 0xffff, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_H), 0xffff, 0);
	} else {
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L),
			0xffff, 0x9999);
		snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_H), 0xff, 0x1);
	}
}

static void sprd_codec_power_disable(struct snd_soc_codec *codec)
{

	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ADEBUG();

	arch_audio_codec_analog_disable();
	sprd_codec_audif_clk_enable(codec, 0);
	regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_STANDBY);
}

static void sprd_codec_power_enable(struct snd_soc_codec *codec)
{

	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ADEBUG();

	arch_audio_codec_analog_enable();
	sprd_codec_audif_clk_enable(codec, 1);
	regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_NORMAL);
}

static int sprd_codec_digital_open(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	ADEBUG();

	/* FIXME : Some Clock SYNC bug will cause MUTE */
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), BIT(DAC_MUTE_EN), 0);

	sprd_codec_sample_rate_setting(sprd_codec);

	sprd_codec_sdm_init(codec);

	/*peng.lee added this according to janus.li's email*/
	snd_soc_update_bits(codec, SOC_REG(AUD_SDM_CTL0), 0xFFFF, 0);

	/* Set the left/right clock selection. */
	if (sprd_codec->lrclk_sel[LRCLK_SEL_DAC])
		snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL),
			BIT(DAC_LR_SEL), BIT(DAC_LR_SEL));
	if (sprd_codec->lrclk_sel[LRCLK_SEL_ADC])
		snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL),
			BIT(ADC_LR_SEL), BIT(ADC_LR_SEL));
	if (sprd_codec->lrclk_sel[LRCLK_SEL_ADC1])
		snd_soc_update_bits(codec, SOC_REG(AUD_ADC1_I2S_CTL),
			BIT(ADC1_LR_SEL), BIT(ADC1_LR_SEL));

	/*
	 * temporay method to disable DNS, waiting ASIC to improve
	 * this feature
	 */
	snd_soc_update_bits(codec, SOC_REG(AUD_DNS_AUTOGATE_EN), 0xffff,
			    0x3303);
	snd_soc_update_bits(codec, SOC_REG(AUD_DNS_SW), BIT(RG_DNS_SW), 0);
	return ret;
}

/*yintang marked for compiling*/
#ifdef SPRD_CODEC_TBD
static void sprd_codec_irq_oxp_enable(struct snd_soc_codec *codec)
{
	unsigned int val, irq_bits;

	/* Enable the audo protect in digital part. */
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_PRT_CFG_0),
			    BIT(AUD_PRT_EN), BIT(AUD_PRT_EN));

	/* Enable irqs  of ocp, ovp and otp. */
	irq_bits = BIT(OVP_IRQ) | BIT(OTP_IRQ) | BIT(PA_OCP_IRQ) |
		BIT(HP_EAR_OCP_IRQ);
	pr_debug("%s, irq_bits: %#x\n", __func__, irq_bits);
	val = irq_bits << AUD_A_INT_CLR;
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLR), val, val);
	val = irq_bits << AUD_A_INT_EN;
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_INT_MODULE_CTRL), val, val);
}
#endif

static void codec_digital_reg_enable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&sprd_codec->digital_enable_mutex);
	if (!sprd_codec->digital_enable_count
		|| sprd_codec->user_dig_access_dis)
		arch_audio_codec_digital_reg_enable();
	sprd_codec->digital_enable_count++;
	mutex_unlock(&sprd_codec->digital_enable_mutex);
}

static void codec_digital_reg_disable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&sprd_codec->digital_enable_mutex);

	if (sprd_codec->digital_enable_count)
		sprd_codec->digital_enable_count--;

	if (!sprd_codec->digital_enable_count)
		arch_audio_codec_digital_reg_disable();

	mutex_unlock(&sprd_codec->digital_enable_mutex);
}


static int digital_power_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mutex_lock(&sprd_codec->dig_access_mutex);
		ret = agdsp_access_enable();
		if (ret) {
			pr_err("%s, agdsp_access_enable failed!\n", __func__);
			mutex_unlock(&sprd_codec->dig_access_mutex);
			return ret;
		}
		sprd_codec->dig_access_en = true;
		mutex_unlock(&sprd_codec->dig_access_mutex);

		codec_digital_reg_enable(codec);
		sprd_codec_digital_open(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * maybe ADC module use it, so we cann't close it
		 * arch_audio_codec_digital_disable();
		 */
		codec_digital_reg_disable(codec);

		mutex_lock(&sprd_codec->dig_access_mutex);
		if (sprd_codec->dig_access_en) {
			sprd_codec->user_dig_access_dis = false;
			agdsp_access_disable();
			sprd_codec->dig_access_en = false;
		}
		mutex_unlock(&sprd_codec->dig_access_mutex);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}


static int analog_power_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_power_enable(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_power_disable(codec);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int audio_dcl_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	unsigned int msk, val;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msk = HP_DPOP_GAIN_T(0xffff);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL9), msk, 0);
		val = RCV_DCCAL_SEL(1);
		msk = RCV_DCCAL_SEL(0xFFFF);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL7), msk, val);

		val = RSTN_AUD_DCL_32K | RSTN_AUD_DIG_DRV_SOFT |
			RSTN_AUD_HPDPOP | RSTN_AUD_DIG_DCL;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL0), val, val);

		val = RSTN_AUD_DIG_INTC;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL1), val, val);
		udelay(200);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int chan_event(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol, int event)
{
	int chan_id = FUN_REG(w->reg);
	int on = !!SND_SOC_DAPM_EVENT_ON(event);

	sp_asoc_pr_info("%s %s\n", sprd_codec_chan_get_name(chan_id),
			STR_ON_OFF(on));

	return 0;
}

static int dfm_out_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_info("DFM-OUT %s\n", STR_ON_OFF(on));
	sprd_codec_sample_rate_setting(sprd_codec);

	return 0;
}

static void sprd_codec_init_fgu(struct sprd_codec_priv *sprd_codec)
{
	struct fgu *fgu = &sprd_codec->fgu;

	fgu->vh = 3300;
	fgu->vl = 3250;
	fgu->dvh = 50;
	fgu->dvl = 50;
}

static void sprd_codec_set_fgu_high_thrd(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, SOC_REG(ANA_DCL3), FGU_HIGH_THRD(0xFFFF),
			    FGU_HIGH_THRD(sprd_codec->fgu.high_thrd));
}

static void sprd_codec_set_fgu_low_thrd(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, SOC_REG(ANA_DCL4), FGU_LOW_THRD(0xFFFF),
			    FGU_LOW_THRD(sprd_codec->fgu.low_thrd));
}

static unsigned int sprd_codec_get_fgu_vbatt(struct snd_soc_codec *codec)
{
	u32 vbatt;

	vbatt = snd_soc_read(codec, SOC_REG(ANA_STS7));

	return FGU_V_DAT(vbatt);
}

static void sprd_codec_set_fgu_v_unit(struct snd_soc_codec *codec)
{
	u32 fgu_4p2, v_unit;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	fgu_4p2 = sprd_codec->fgu_4p2_efuse & SPRD_CODEC_EFUSE_FGU_4P2_MASK;

	/*
	 * This formular is to get the LSB of FGU_V_UNIT by EFUSE FGU, which
	 * is come out from user guide.
	 */
	v_unit = 4200 * 2048 / ((fgu_4p2 - 256 + 6963) - 4096);

	snd_soc_update_bits(codec, SOC_REG(ANA_DCL2), FGU_V_UNIT(0xffff),
			    FGU_V_UNIT(v_unit));
}

static void sprd_codec_psg_state_transf(struct snd_soc_codec *codec,
					unsigned int state)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct fgu *fgu = &sprd_codec->fgu;

	switch (state) {
	case PSG_STATE_BOOST_NONE:
		sprd_codec_pa_boost(codec, 0);
		return;
	case PSG_STATE_BOOST_LARGE_GAIN:
		sprd_codec_pa_boost(codec, 1);
		fgu->high_thrd = 4500;
		fgu->low_thrd = fgu->vh;
		break;
	case PSG_STATE_BOOST_SMALL_GAIN:
		sprd_codec_pa_boost(codec, 1);
		fgu->high_thrd = fgu->vh + fgu->dvh;
		fgu->low_thrd = fgu->vl;
		break;
	case PSG_STATE_BOOST_BYPASS:
		sprd_codec_pa_boost(codec, 0);
		fgu->high_thrd = fgu->vl + fgu->dvl;
		fgu->low_thrd = 3000;
		break;
	default:
		WARN_ON(1);
		return;
	}
	sprd_codec->psg_state = state;
	sprd_codec_set_fgu_high_thrd(codec);
	sprd_codec_set_fgu_low_thrd(codec);
	sprd_codec_spk_pga_update(codec);

	sp_asoc_pr_reg("state=%d, battery=%d\n", state,
			sprd_codec_get_fgu_vbatt(codec));
}

static void sprd_codec_psg_state_init(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct fgu *fgu = &sprd_codec->fgu;
	u32 state, vbatt = sprd_codec_get_fgu_vbatt(codec);

	sprd_codec_set_fgu_v_unit(codec);
	if (vbatt > fgu->vh)
		state = PSG_STATE_BOOST_LARGE_GAIN;
	else if (vbatt > fgu->vl && vbatt <= fgu->vh)
		state = PSG_STATE_BOOST_SMALL_GAIN;
	else
		state = PSG_STATE_BOOST_BYPASS;
	sprd_codec_psg_state_transf(codec, state);
}

static void sprd_codec_psg_state_exit(struct snd_soc_codec *codec)
{
	sprd_codec_psg_state_transf(codec, PSG_STATE_BOOST_NONE);
}

static void sprd_codec_psg_process(struct snd_soc_codec *codec, int hi_lo)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int state = -1;

	sp_asoc_pr_reg("psg_state=%d,hi_lo=%d, batt=%d\n",
			sprd_codec->psg_state, hi_lo,
			sprd_codec_get_fgu_vbatt(codec));
	switch (sprd_codec->psg_state) {
	case PSG_STATE_BOOST_LARGE_GAIN:
		if (hi_lo)
			sp_asoc_pr_info("Int should not come!\n");
		else
			state = PSG_STATE_BOOST_SMALL_GAIN;
		break;
	case PSG_STATE_BOOST_SMALL_GAIN:
		if (hi_lo)
			state = PSG_STATE_BOOST_LARGE_GAIN;
		else
			state = PSG_STATE_BOOST_BYPASS;
		break;
	case PSG_STATE_BOOST_BYPASS:
		if (hi_lo)
			state = PSG_STATE_BOOST_SMALL_GAIN;
		else
			sp_asoc_pr_info("Int should not come!\n");
		break;
	default:
		WARN_ON(1);
		break;
	}
	if (state < 0)
		return;
	if (state != sprd_codec->psg_state)
		sprd_codec_psg_state_transf(codec, state);
}

void sprd_codec_intc_irq(struct snd_soc_codec *codec, u32 int_shadow)
{
	u32 val_s, val_n, val_fgu;

	val_n = PA_DCCAL_INT_SHADOW_STATUS |
		PA_CLK_CAL_INT_SHADOW_STATUS |
		RCV_DPOP_INT_SHADOW_STATUS |
		HPR_DPOP_INT_SHADOW_STATUS |
		HPL_DPOP_INT_SHADOW_STATUS |
		IMPD_DISCHARGE_INT_SHADOW_STATUS |
		IMPD_CHARGE_INT_SHADOW_STATUS |
		IMPD_BIST_INT_SHADOW_STATUS;
	val_s = HPR_SHUTDOWN_INT_SHADOW_STATUS |
		HPL_SHUTDOWN_INT_SHADOW_STATUS |
		PA_SHUTDOWN_INT_SHADOW_STATUS |
		EAR_SHUTDOWN_INT_SHADOW_STATUS;
	val_fgu = FGU_LOW_LIMIT_INT_SHADOW_STATUS |
		FGU_HIGH_LIMIT_INT_SHADOW_STATUS;

	/* For such int, nothing need to be done, just keep current status */
	if (int_shadow & val_s) {
		sp_asoc_pr_info("IRQ! shut down! int_shadow=%#x\n",
				int_shadow);
		return;
	}

	/* For such int, clear and enable it again.*/
	if (int_shadow & val_n)
		sp_asoc_pr_info("IRQ! clear and enable it! int_shadow=%#x\n",
				int_shadow);

	if (int_shadow & FGU_LOW_LIMIT_INT_SHADOW_STATUS)
		sprd_codec_psg_process(codec, 0);
	else if (int_shadow & FGU_HIGH_LIMIT_INT_SHADOW_STATUS)
		sprd_codec_psg_process(codec, 1);

	/* Int cannot be cleared within about 200ms, so here we should
	 * wait 200ms to avoid Int trigger continuesely.
	 */
	sprd_codec_wait(250);
}

static int rcv_depop_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u32 val = 0;
	u32 state;
	int ret = 0;
	int i = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	if (on) {
		val = RCV_DPOP_EN | RCV_DPOP_AUTO_EN | RCV_DPOP_FDIN_EN |
			RCV_DPOP_FDOUT_EN | RCV_DPOP_CHG;
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_DCL6),
			0xFF, val);

		/* wait 1ms by guidline */
		sprd_codec_wait(1);
	}

	update_switch(codec, SDALRCV, on);
	/* totally wait about 100ms by guidline */
	sprd_codec_wait(60);
	while (i++ < 10) {
		sprd_codec_wait(10);
		state = snd_soc_read(codec, SOC_REG(ANA_STS1)) &
			RCV_DPOP_DVLD;
		if ((on && state) || (!on && !state))
			break;
	}
	if (i >= 10)
		sp_asoc_pr_info("%s Dpop failed!\n", __func__);
	else
		sp_asoc_pr_info("%s Dpop sucessed! i=%d\n",
			__func__, i);

	return ret;
}

static int hp_depop_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u32 val = 0;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	if (on) {
		val = HP_DPOP_EN | HP_DPOP_AUTO_EN | HP_DPOP_FDIN_EN |
			HP_DPOP_FDOUT_EN | HPL_DPOP_CHG | HPR_DPOP_CHG;
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_DCL5),
			0xFF, val);

		val = HPRCV_OPA_IBIAS_MODE_SEL | HPRCV_DCCALI_IBIAS_MODE_SEL;
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC13), val, val);
	}
	return ret;
}

static int hp_buf_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	sprd_codec_wait(10);
	return 0;
}


static int hp_path_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	int ret = 0;

	ADEBUG();
	if (on)
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_CDC11),
			0xFF, 0x5650);

	return ret;
}

static int ear_path_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	int ret = 0;

	ADEBUG();
	if (on) {
		ret = snd_soc_update_bits(codec, SOC_REG(ANA_CDC11),
			0xFFFF, 0x5930);
	}
	return ret;
}

static int adcl_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s  Event is %s\n", __func__,  get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADL_CLK_RST,
			ADL_CLK_RST);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADL_CLK_RST, 0);
		/*
		 * ADL_RST should keep be 0-1-0,
		 * and 1 state should keep 20 cycle of 6.5M clock at least.
		 */
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADL_RST, 0);
		udelay(4);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADL_RST, ADL_RST);
		udelay(4);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADL_RST, 0);
		/*
		 * wait 60ms is by experience for the POP,
		 * the duration of POP is about 70ms.
		 */
		sprd_codec_wait(60);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int adcr_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s  Event is %s\n", __func__,  get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0),
			ADR_CLK_RST, ADR_CLK_RST);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADR_CLK_RST, 0);

		/*
		 * ADL_RST should keep be 0-1-0,
		 * and 1 state should keep 20 cycle of 6.5M clock at least.
		 */
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADR_RST, 0);
		udelay(4);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADR_RST, ADR_RST);
		udelay(4);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), ADR_RST, 0);
		/*
		 * wait 60ms is by experience for the POP,
		 * the duration of POP is about 70ms.
		 */
		sprd_codec_wait(60);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int sprd_ivsense_src_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s, set ivsense src 48000\n",
		__func__, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_set_ad_sample_rate(codec,
			48000, 0x0F, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static inline int adie_audio_codec_adie_loop_clk_en(
		struct snd_soc_codec *codec, int on)
{
	int ret = 0;

	if (on)
		ret = snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN),
				BIT_CLK_AUD_LOOP_EN, BIT_CLK_AUD_LOOP_EN);
	else
		ret = snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN),
				BIT_CLK_AUD_LOOP_EN, 0);
	return ret;
}

static int adie_loop_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_info("%s, Event is %s\n",
		__func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* enable loop clock */
		ret = adie_audio_codec_adie_loop_clk_en(codec, 1);
		snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
			BIT_AUDIO_ADIE_LOOP_EN, BIT_AUDIO_ADIE_LOOP_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* disable loop clock */
		ret = adie_audio_codec_adie_loop_clk_en(codec, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
			BIT_AUDIO_ADIE_LOOP_EN, 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int digital_loop_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_info("%s, Event is %s\n",
			__func__, get_event_name(event));

	sprd_codec_set_ad_sample_rate(codec, 48000, 0x0F, 0);
	sprd_codec_set_sample_rate(codec, 48000, 0x0F, 0);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
			BIT(LOOP_ADC_PATH_SEL), BIT(LOOP_ADC_PATH_SEL));
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
			BIT(AUD_LOOP_TEST), BIT(AUD_LOOP_TEST));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
			BIT(AUD_LOOP_TEST), 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int cp_ad_cmp_cali_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC13),
				    HPRCV_OPA_IBIAS_MODE_SEL |
				    HPRCV_DCCALI_IBIAS_MODE_SEL, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
			CP_AD_CMP_CAL_AVG(0xFFFF), CP_AD_CMP_CAL_AVG(1));
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
			CP_AD_CMP_AUTO_CAL_EN, CP_AD_CMP_AUTO_CAL_EN);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL11),
			CP_AD_CMP_AUTO_CAL_RANGE, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
			CP_AD_EN, CP_AD_EN);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
			CP_AD_CMP_CAL_EN, CP_AD_CMP_CAL_EN);
		sprd_codec_wait(3);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int cp_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u32 neg_cp = 0;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CP positive power on */
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
			CP_POS_SOFT_EN, CP_POS_SOFT_EN);

		/* CP negative power on */
		neg_cp = (sprd_codec->neg_cp_efuse >> 7) & 0xff;
		/* set negative voltalge to 1.62 by ASIC suggest */
		neg_cp = (neg_cp * 162) / 165;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL14),
			CP_NEG_HV(0xFFFF), CP_NEG_HV(neg_cp));
		neg_cp = (neg_cp * 110) / 165;
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL14),
			CP_NEG_LV(0xFFFF), CP_NEG_LV(neg_cp));
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL10),
				    CP_NEG_SOFT_EN, CP_NEG_SOFT_EN);
		/* wait 100us by guidline */
		usleep_range(100, 110);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU7),
			CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
			CP_AD_EN, CP_AD_EN);
		usleep_range(50, 80);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
			CP_EN, CP_EN);
		sprd_codec_wait(1);

		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU0),
			CP_AD_EN | CP_EN, 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU7),
			CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP,
			CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		break;
	}

	sp_asoc_pr_dbg("%s ret is %d,yintang, get here!!!!!\n", __func__, ret);
	return ret;
}

static int ivsense_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	update_switch(codec, SPAISNS | SPAVSNS, on);

	return 0;
}

#define REGU_CNT 2
static void cp_short_check(struct sprd_codec_priv *sprd_codec)
{
	struct snd_soc_codec *codec;
	const char *regu_id[REGU_CNT] = {"BG", "BIAS"};
	struct device *dev;
	struct regulator *regu[REGU_CNT];
	unsigned int regu_mode[REGU_CNT];
	int ret = 0;
	int i;
	int mask1, mask2;
	unsigned int idx;
	unsigned int flag;
	unsigned int val1[] = {
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYN,
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYN,
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYP,
		CP_NEG_PD_VNEG | CP_NEG_PD_FLYP,
		CP_NEG_PD_FLYP | CP_NEG_PD_FLYN,
		CP_NEG_PD_FLYP | CP_NEG_PD_FLYN,
	};
	unsigned int val2[] = {
		CP_NEG_SHDT_FLYP_EN | CP_NEG_SHDT_PGSEL,
		CP_NEG_SHDT_FLYP_EN,
		CP_NEG_SHDT_FLYN_EN | CP_NEG_SHDT_PGSEL,
		CP_NEG_SHDT_FLYN_EN,
		CP_NEG_SHDT_VCPN_EN | CP_NEG_SHDT_PGSEL,
		CP_NEG_SHDT_VCPN_EN,
	};

	codec = sprd_codec ? sprd_codec->codec : NULL;
	if (!codec)
		return;
	dev = codec->dev;

	if (codec == NULL) {
		pr_err("%s, codec is NULL!", __func__);
		return;
	}

	/* Enable VB, BG & BIAS */
	for (i = 0; i < REGU_CNT; i++) {
		regu[i] = regulator_get(dev, regu_id[i]);
		if (IS_ERR(regu[i])) {
			pr_err("ERR:Failed to request %ld: %s\n",
				   PTR_ERR(regu[i]), regu_id[i]);
			goto REGULATOR_CLEAR;
		}
		ret = regulator_enable(regu[i]);
		if (ret) {
			pr_err("%s, regulator_enable failed!", __func__);
			regulator_put(regu[i]);
			goto REGULATOR_CLEAR;
		}
		regu_mode[i] = regulator_get_mode(regu[i]);
		regulator_set_mode(regu[i], REGULATOR_MODE_NORMAL);
	}
	usleep_range(100, 150);

	snd_soc_update_bits(codec, SOC_REG(ANA_PMU7),
		CP_NEG_CLIMIT_EN, CP_NEG_CLIMIT_EN);
	usleep_range(50, 100);

	mask1 = CP_NEG_PD_VNEG | CP_NEG_PD_FLYN | CP_NEG_PD_FLYP;
	mask2 = CP_NEG_SHDT_VCPN_EN | CP_NEG_SHDT_FLYP_EN |
		CP_NEG_SHDT_FLYN_EN | CP_NEG_SHDT_PGSEL;
	for (idx = 0; idx < ARRAY_SIZE(val1); idx++) {
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU7), mask1, val1[idx]);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU8), mask2, val2[idx]);
		sprd_codec_wait(20);
		flag = snd_soc_read(codec, SOC_REG(ANA_STS5)) &
			BIT(CP_NEG_SH_FLAG);
		if (flag) {
			sprd_codec->cp_short_stat = idx + 1;
			break;
		}
	}
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU7),
		mask1, mask1);
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU8),
		mask2, 0);

	/* Disable VB, BG & BIAS */
REGULATOR_CLEAR:
	for (i -= 1; i >= 0; i--) {
		regulator_set_mode(regu[i], regu_mode[i]);
		regulator_disable(regu[i]);
		regulator_put(regu[i]);
	}

}

static const char * const hpl_rcv_mux_texts[] = {
	"EAR", "HPL"
};
static const SOC_ENUM_SINGLE_VIRT_DECL(hpl_rcv_enum, hpl_rcv_mux_texts);
static const struct snd_kcontrol_new hpl_rcv_mux =
	SOC_DAPM_ENUM("HPL EAR Sel", hpl_rcv_enum);

#define SPRD_CODEC_MUX_KCTL(xname, xkctl, xreg, xshift, xtext) \
	static const SOC_ENUM_SINGLE_DECL(xkctl##_enum, xreg, xshift, xtext); \
	static const struct snd_kcontrol_new xkctl = \
		SOC_DAPM_ENUM_EXT(xname, xkctl##_enum, \
		snd_soc_dapm_get_enum_double, \
		snd_soc_dapm_put_enum_double)

static const char * const dig_adc_in_texts[] = {
	"ADC", "DAC", "DMIC"
};

SPRD_CODEC_MUX_KCTL("Digital ADC In Sel", dig_adc_in_sel,
		    AUD_TOP_CTL, ADC_SINC_SEL, dig_adc_in_texts);
SPRD_CODEC_MUX_KCTL("Digital ADC1 In Sel", dig_adc1_in_sel,
		    AUD_TOP_CTL, ADC1_SINC_SEL, dig_adc_in_texts);

static const struct snd_soc_dapm_widget sprd_codec_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("Digital Power", 1, SND_SOC_NOPM,
		0, 0, digital_power_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("Analog Power", 0, SND_SOC_NOPM,
		0, 0, analog_power_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_REGULATOR_SUPPLY("BG", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("BIAS", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS1", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS2", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("HEADMICBIAS", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("CP_LDO", 2, SOC_REG(ANA_PMU0),
			CP_LDO_EN_S, 0, NULL, 0),

/* Clock */
	SND_SOC_DAPM_SUPPLY_S("ANA_CLK", 1, SOC_REG(ANA_CLK0),
			ANA_CLK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_DCL_6M5", 1, SOC_REG(ANA_CLK0),
			CLK_DCL_6M5_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("CLK_DCL_32K", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_PA_IVSNS", 1, SOC_REG(ANA_CLK0),
			CLK_PA_IVSNS_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_PA_DFLCK", 1, SOC_REG(ANA_CLK0),
			CLK_PA_DFLCK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_PA_32K", 1, SOC_REG(ANA_CLK0),
			CLK_PA_32K_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_PACAL", 1, SOC_REG(ANA_CLK0),
			CLK_PACAL_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_PABST", 1, SOC_REG(ANA_CLK0),
			CLK_PABST_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_PABST_32K", 1, SOC_REG(ANA_CLK0),
			CLK_PABST_32K_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_CP", 1, SOC_REG(ANA_CLK0),
			CLK_CP_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_IMPD", 1, SOC_REG(ANA_CLK0),
			CLK_IMPD_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_DAC", 1, SOC_REG(ANA_CLK0),
			CLK_DAC_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_ADC", 1, SOC_REG(ANA_CLK0),
			CLK_ADC_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CLK_DIG_6M5", 1, SOC_REG(ANA_CLK0),
			CLK_DIG_6M5_EN_S, 0, NULL, 0),
/* DCL clock */
	SND_SOC_DAPM_REGULATOR_SUPPLY("DCL", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("DIG_CLK_INTC", 0, 0),

	SND_SOC_DAPM_REGULATOR_SUPPLY("DIG_CLK_HID", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_CFGA_PRT", 5, SOC_REG(ANA_DCL1),
			DIG_CLK_CFGA_PRT_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_PABST", 5, SOC_REG(ANA_DCL1),
			DIG_CLK_PABST_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_HPDPOP", 5, SOC_REG(ANA_DCL1),
			DIG_CLK_HPDPOP_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_IMPD", 5, SOC_REG(ANA_DCL1),
			DIG_CLK_IMPD_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_FGU", 5, SOC_REG(ANA_DCL1),
			DIG_CLK_FGU_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_DRV_SOFT", 5, SOC_REG(ANA_DCL1),
			DIG_CLK_DRV_SOFT_EN_S, 0,
			audio_dcl_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("DIG_CLK_PA", 5, SOC_REG(ANA_DCL1),
			DIG_CLK_PA_EN_S, 0, NULL, 0),
/* ADC CLK */
	SND_SOC_DAPM_SUPPLY_S("ADCL_CLK", 5, SOC_REG(ANA_CDC0),
			ADL_CLK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADCR_CLK", 5, SOC_REG(ANA_CDC0),
			ADR_CLK_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC_IBIAS", 3, SOC_REG(ANA_CDC0),
		PGA_ADC_IBIAS_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC_VREF_BUF", 3, SOC_REG(ANA_CDC0),
		PGA_ADC_VCM_VREF_BUF_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("IVSense SRC", 3, SND_SOC_NOPM, 0, 0,
			sprd_ivsense_src_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

/* AD route */
	SND_SOC_DAPM_MIXER("ADCL Mixer", SND_SOC_NOPM, 0, 0,
		&adcl_mixer_controls[0],
		ARRAY_SIZE(adcl_mixer_controls)),
	SND_SOC_DAPM_MIXER("ADCR Mixer", SND_SOC_NOPM, 0, 0,
		&adcr_mixer_controls[0],
		ARRAY_SIZE(adcr_mixer_controls)),
	SND_SOC_DAPM_SWITCH("AUD ADC0L", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[0]),
	SND_SOC_DAPM_SWITCH("AUD ADC0R", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[1]),
	SND_SOC_DAPM_SWITCH("AUD ADC1L", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[2]),
	SND_SOC_DAPM_SWITCH("AUD ADC1R", SND_SOC_NOPM, 0, 1,
			    &aud_adc_switch[3]),

	SND_SOC_DAPM_PGA_E("ADCL Gain", SOC_REG(ANA_CDC0), ADPGAL_EN_S, 0,
		adcl_pga_controls, 1, NULL, 0),
	SND_SOC_DAPM_PGA_E("ADCR Gain", SOC_REG(ANA_CDC0), ADPGAR_EN_S, 0,
		adcr_pga_controls, 1, NULL, 0),
	/*
	 * SND_SOC_DAPM_PGA_S("ADCL PGA", 3, FUN_REG(SPRD_CODEC_PGA_ADCL),
	 * 0, 0, pga_event,
	 * SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	 * SND_SOC_DAPM_PGA_S("ADCR PGA", 3, FUN_REG(SPRD_CODEC_PGA_ADCR),
	 * 0, 0, pga_event,
	 * SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	 */
	SND_SOC_DAPM_PGA_E("ADCL Switch", SOC_REG(ANA_CDC0),
		ADL_EN_S, 0, 0, 0, adcl_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("ADCR Switch", SOC_REG(ANA_CDC0),
		ADR_EN_S, 0, 0, 0, adcr_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCL Switch", 2,
		SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		BIT_ADC_EN_L_S, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCR Switch", 2,
		SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		BIT_ADC_EN_R_S, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADCL Switch", 4, SOC_REG(AUD_TOP_CTL),
		ADC_EN_L, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADCR Switch", 4, SOC_REG(AUD_TOP_CTL),
		ADC_EN_R, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADC1L Switch", 4, SOC_REG(AUD_TOP_CTL),
		ADC1_EN_L, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital ADC1R Switch", 4, SOC_REG(AUD_TOP_CTL),
		ADC1_EN_R, 0, 0, 0),
	SND_SOC_DAPM_ADC_E("ADC", "Normal-Capture-AP01",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1", "Normal-Capture-AP23",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC AP23", "Normal-Capture-AP23",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC DSP_C", "Capture-DSP",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC DSP_C_btsco_test", "Capture-DSP-BTSCO-test",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC DSP_C_fm_test", "Capture-DSP-FM-test",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC Voip", "Voip-Capture",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC CODEC_TEST", "CODEC_TEST-Capture",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC LOOP", "LOOP-Capture",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC Voice", "Voice-Capture",
		FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
/* DA route */
	SND_SOC_DAPM_DAC_E("DAC", "Normal-Playback-AP01",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0,
		0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC AP23", "Normal-Playback-AP23",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0,
		0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Voice", "Voice-Playback",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Offload", "Offload-Playback",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fast", "Fast-Playback",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Voip", "Voip-Playback",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC CODEC_TEST", "CODEC_TEST-Playback",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0,
		0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC LOOP", "LOOP-Playback",
		FUN_REG(SPRD_CODEC_PLAYBACK), 0,
		0,
		chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fm", "Fm-Playback",
		SND_SOC_NOPM, 0, 0,
		dfm_out_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fm_DSP", "Fm-DSP-Playback",
		SND_SOC_NOPM, 0, 0,
		dfm_out_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Digital DACL Switch", -4, SOC_REG(AUD_TOP_CTL),
		DAC_EN_L, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("Digital DACR Switch", -4, SOC_REG(AUD_TOP_CTL),
		DAC_EN_R, 0, 0, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACL Switch", -3,
		SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		BIT_DAC_EN_L_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACR Switch", -3,
		SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		BIT_DAC_EN_R_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_E("DAC Gain", SND_SOC_NOPM, 0, 0,
		dac_pga_controls, 1, 0, 0),

/* SPK */
	SND_SOC_DAPM_PGA_S("DAS DC Offset", -1,
		SND_SOC_NOPM, 0, 0,
		das_dc_os_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPKL Gain", SND_SOC_NOPM, 0, 0,
		spkl_pga_controls, 1, 0, 0),
	SND_SOC_DAPM_PGA_S("DACS Switch", 10,
		SOC_REG(ANA_CDC6), DAS_EN_S_S, 0,
		dacs_switch_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("SPKL Mixer", SND_SOC_NOPM, 0, 0,
		&spkl_mixer_controls[0],
		ARRAY_SIZE(spkl_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPKR Mixer", SND_SOC_NOPM, 0, 0,
		&spkr_mixer_controls[0],
		ARRAY_SIZE(spkr_mixer_controls)),
	SND_SOC_DAPM_PGA_E("SPK PA", SND_SOC_NOPM, 0, 0,
		NULL, 0,
		spk_pa_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("ISNS Mixer", SOC_REG(ANA_CDC21), PA_ISNS_EN_S, 0,
		&spaivsns_mixer_controls[0], 1),
	SND_SOC_DAPM_MIXER("VSNS Mixer", SOC_REG(ANA_CDC21), PA_VSNS_EN_S, 0,
		&spaivsns_mixer_controls[1], 1),

/* HP */
	SND_SOC_DAPM_PGA_S("DACL Switch", SPRD_CODEC_DA_EN_ORDER,
		SND_SOC_NOPM, DAL_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DACR Switch", SPRD_CODEC_DA_EN_ORDER,
		SND_SOC_NOPM, DAR_EN_S, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DALR DC Offset", SPRD_CODEC_DC_OS_ORDER,
		SOC_REG(ANA_CDC5), DALR_OS_EN_S, 0,
		dalr_dc_os_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_E("HPL Gain", SND_SOC_NOPM, 0, 0,
		hpl_pga_controls, 1, 0, 0),
	SND_SOC_DAPM_PGA_E("HPR Gain", SND_SOC_NOPM, 0, 0,
		hpr_pga_controls, 1, 0, 0),
	SND_SOC_DAPM_MIXER("HPL Mixer", SND_SOC_NOPM, 0, 0,
		&hpl_mixer_controls[0],
		ARRAY_SIZE(hpl_mixer_controls)),
	SND_SOC_DAPM_MIXER("HPR Mixer", SND_SOC_NOPM, 0, 0,
		&hpr_mixer_controls[0],
		ARRAY_SIZE(hpr_mixer_controls)),
	SND_SOC_DAPM_PGA_S("HP DEPOP", SPRD_CODEC_DEPOP_ORDER,
		SND_SOC_NOPM,
		0, 0, hp_depop_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HP BUF Switch", SPRD_CODEC_BUF_SWITCH_ORDER,
		SOC_REG(ANA_CDC10), HPBUF_EN_S, 0,
		hp_buf_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HPL Switch", SPRD_CODEC_SWITCH_ORDER,
		SOC_REG(ANA_CDC10), HPL_EN_S, 0,
		NULL, 0),
	SND_SOC_DAPM_PGA_S("HPR Switch", SPRD_CODEC_SWITCH_ORDER,
		SOC_REG(ANA_CDC10), HPR_EN_S, 0,
		NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CP AD Cali", 5, SND_SOC_NOPM,
		0, 0, cp_ad_cmp_cali_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CP", 4, SND_SOC_NOPM,
		0, 0, cp_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

/* EAR */
	SND_SOC_DAPM_PGA_S("RCV DEPOP", SPRD_CODEC_RCV_DEPOP_ORDER,
		SND_SOC_NOPM,
		0, 0, rcv_depop_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_E("EAR Gain", SND_SOC_NOPM, 0, 0,
		ear_pga_controls, 1, 0, 0),
	SND_SOC_DAPM_MIXER("EAR Mixer", SND_SOC_NOPM, 0, 0,
		&ear_mixer_controls[0],
		ARRAY_SIZE(ear_mixer_controls)),
	SND_SOC_DAPM_PGA_S("EAR Switch", SPRD_CODEC_SWITCH_ORDER,
		SOC_REG(ANA_CDC10), RCV_EN_S, 0,
		NULL, 0),

/* HP and EAR DEMUX */
	SND_SOC_DAPM_DEMUX("HPL EAR Sel", SND_SOC_NOPM, 0, 0, &hpl_rcv_mux),
	SND_SOC_DAPM_DEMUX("HPL EAR Sel2", SND_SOC_NOPM, 0, 0, &hpl_rcv_mux),
	SND_SOC_DAPM_PGA_E("HPL Path", SND_SOC_NOPM, 0, 0, NULL, 0,
		hp_path_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_PGA_E("EAR Path", SND_SOC_NOPM, 0, 0, NULL, 0,
		ear_path_event, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_SWITCH_E("IVSense Virt", SND_SOC_NOPM,
			      0, 0, &ivsence_switch, ivsense_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

/* PIN */
	SND_SOC_DAPM_OUTPUT("EAR Pin"),
	SND_SOC_DAPM_OUTPUT("HP Pin"),
	SND_SOC_DAPM_OUTPUT("SPK Pin"),

	SND_SOC_DAPM_OUTPUT("Virt Output Pin"),
	SND_SOC_DAPM_SWITCH("Virt Output", SND_SOC_NOPM,
		0, 0, &virt_output_switch),

	SND_SOC_DAPM_INPUT("MIC Pin"),
	SND_SOC_DAPM_INPUT("MIC2 Pin"),
	SND_SOC_DAPM_INPUT("HPMIC Pin"),

	SND_SOC_DAPM_INPUT("DMIC Pin"),
	SND_SOC_DAPM_INPUT("DMIC1 Pin"),

	/* add DMIC */
	SND_SOC_DAPM_PGA_S("DMIC Switch", 3, SOC_REG(AUD_DMIC_CTL),
		ADC_DMIC_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DMIC1 Switch", 3, SOC_REG(AUD_DMIC_CTL),
		ADC1_DMIC1_EN, 0, NULL, 0),
	SND_SOC_DAPM_SWITCH("ADC-DAC Adie Loop",
		SND_SOC_NOPM, 0, 0, &loop_controls[0]),
	SND_SOC_DAPM_SWITCH("ADC1-DAC Adie Loop",
		SND_SOC_NOPM, 0, 0, &loop_controls[1]),
	SND_SOC_DAPM_SWITCH("ADC-DAC Digital Loop",
		SND_SOC_NOPM, 0, 0, &loop_controls[2]),
	SND_SOC_DAPM_SWITCH("ADC1-DAC Digital Loop",
		SND_SOC_NOPM, 0, 0, &loop_controls[3]),
	SND_SOC_DAPM_PGA_S("ADC-DAC Adie Loop post",
		SPRD_CODEC_ANA_MIXER_ORDER,
		SND_SOC_NOPM, 0, 0, adie_loop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADC-DAC Digital Loop post",
		SPRD_CODEC_ANA_MIXER_ORDER,
		SND_SOC_NOPM, 0, 0, digital_loop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* for HP power up switch*/
	SND_SOC_DAPM_SWITCH("DA mode", SND_SOC_NOPM, 0, 1, &da_mode_switch),

	/* Switch widget for headphone DA path control. If headphone
	 * is removed, switch off it, else switch on it.
	 */
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM,
			0, 0, &hp_jack_switch),

	SND_SOC_DAPM_MUX("Digital ADC In Sel", SND_SOC_NOPM, 0, 0,
			 &dig_adc_in_sel),
	SND_SOC_DAPM_MUX("Digital ADC1 In Sel", SND_SOC_NOPM, 0, 0,
			 &dig_adc1_in_sel),
};

/* sprd_codec supported interconnection */
static const struct snd_soc_dapm_route sprd_codec_intercon[] = {
/* power and clock */
	{"Analog Power", NULL, "BIAS"},
	{"ANA_CLK", NULL, "Analog Power"},
	{"CLK_DCL_32K", NULL, "ANA_CLK"},
	{"CLK_DCL_6M5", NULL, "CLK_DCL_32K"},
	{"CLK_DIG_6M5", NULL, "CLK_DCL_6M5"},
	{"ADC_IBIAS", NULL, "ADC_VREF_BUF"},

	{"CLK_DAC", NULL, "CLK_DIG_6M5"},
	{"CLK_DAC", NULL, "DCL"},
	{"CLK_DAC", NULL, "DIG_CLK_DRV_SOFT"},
	{"CLK_DAC", NULL, "Digital Power"},
	{"CLK_DAC", NULL, "BG"},

	{"CLK_ADC", NULL, "CLK_DIG_6M5"},
	{"CLK_ADC", NULL, "DCL"},
	{"CLK_ADC", NULL, "DIG_CLK_DRV_SOFT"},
	{"CLK_ADC", NULL, "Digital Power"},
	{"CLK_ADC", NULL, "ADC_IBIAS"},

	{"DIG_CLK_HPDPOP", NULL, "DIG_CLK_INTC"},

#if 1
/* DA route */
	{"DAC", NULL, "CLK_DAC"},
	{"DAC Voice", NULL, "CLK_DAC"},
	{"DAC Offload", NULL, "CLK_DAC"},
	{"DAC Fast", NULL, "CLK_DAC"},
	{"DAC Voip", NULL, "CLK_DAC"},
	{"DAC Fm", NULL, "CLK_DAC"},
	{"DAC Fm_DSP", NULL, "CLK_DAC"},
	{"DAC AP23", NULL, "CLK_DAC"},
	{"DAC CODEC_TEST", NULL, "CLK_DAC"},
	{"DAC LOOP", NULL, "CLK_DAC"},

	{"Digital DACL Switch", NULL, "DAC"},
	{"Digital DACR Switch", NULL, "DAC"},
	{"Digital DACL Switch", NULL, "DAC Fm"},
	{"Digital DACR Switch", NULL, "DAC Fm"},
	{"Digital DACL Switch", NULL, "DAC Fm_DSP"},
	{"Digital DACR Switch", NULL, "DAC Fm_DSP"},
	{"Digital DACL Switch", NULL, "DAC AP23"},
	{"Digital DACR Switch", NULL, "DAC AP23"},
	{"Digital DACL Switch", NULL, "DAC Voice"},
	{"Digital DACR Switch", NULL, "DAC Voice"},
	{"Digital DACL Switch", NULL, "DAC Offload"},
	{"Digital DACR Switch", NULL, "DAC Offload"},
	{"Digital DACL Switch", NULL, "DAC Fast"},
	{"Digital DACR Switch", NULL, "DAC Fast"},
	{"Digital DACL Switch", NULL, "DAC Voip"},
	{"Digital DACR Switch", NULL, "DAC Voip"},
	{"Digital DACL Switch", NULL, "DAC CODEC_TEST"},
	{"Digital DACR Switch", NULL, "DAC CODEC_TEST"},
	{"Digital DACL Switch", NULL, "DAC LOOP"},
	{"Digital DACR Switch", NULL, "DAC LOOP"},


	{"ADie Digital DACL Switch", NULL, "Digital DACL Switch"},
	{"ADie Digital DACR Switch", NULL, "Digital DACR Switch"},
	{"DAC Gain", NULL, "ADie Digital DACL Switch"},
	{"DAC Gain", NULL, "ADie Digital DACR Switch"},

/* SPK */
	{"DACS Switch", NULL, "DAC Gain"},
	{"SPKL Mixer", "DACLSPKL Switch", "DACS Switch"},
	{"DAS DC Offset", NULL, "SPKL Mixer"},
	{"SPKL Gain", NULL, "DAS DC Offset"},
	{"SPK PA", NULL, "SPKL Gain"},
	{"SPK PA", NULL, "CLK_PA_32K"},
	{"SPK PA", NULL, "DIG_CLK_PA"},
	{"SPK PA", NULL, "DIG_CLK_FGU"},
	{"SPK Pin", NULL, "SPK PA"},

/* HP */
	{"CP AD Cali", NULL, "CLK_CP"},
	{"CP", NULL, "CP AD Cali"},
	{"CP", NULL, "CP_LDO"},
	{"HP DEPOP", NULL, "CP"},
	{"HP DEPOP", NULL, "DIG_CLK_HPDPOP"},
	{"DACL Switch", NULL, "DAC Gain"},
	{"DACR Switch", NULL, "DAC Gain"},
	{"HPL EAR Sel", NULL, "DACL Switch"},
	{"HPL Path", "HPL", "HPL EAR Sel"},
	{"HPL Mixer", "DACLHPL Switch", "HPL Path"},
	{"HPR Mixer", "DACRHPR Switch", "DACR Switch"},
	{"HP DEPOP", NULL, "HPL Mixer"},
	{"HP DEPOP", NULL, "HPR Mixer"},
	{"DALR DC Offset", NULL, "HP DEPOP"},
	{"HP BUF Switch", NULL, "DALR DC Offset"},
	{"HPL EAR Sel2", NULL, "HP BUF Switch"},
	{"HPL Switch", "HPL", "HPL EAR Sel2"},
	{"HPR Switch", NULL, "HP BUF Switch"},
	{"HPL Gain", NULL, "HPL Switch"},
	{"HPR Gain", NULL, "HPR Switch"},
	{"HP Pin", NULL, "HPL Gain"},
	{"HP Pin", NULL, "HPR Gain"},

/* EAR */
	{"RCV DEPOP", NULL, "CP"},
	{"RCV DEPOP", NULL, "DIG_CLK_HPDPOP"},
	{"EAR Path", "EAR", "HPL EAR Sel"},
	{"EAR Mixer", "DACLEAR Switch", "EAR Path"},
	{"RCV DEPOP", NULL, "EAR Mixer"},
	{"DALR DC Offset", NULL, "RCV DEPOP"},
	{"HP BUF Switch", NULL, "DALR DC Offset"},
	{"HPL EAR Sel2", NULL, "HP BUF Switch"},
	{"EAR Switch", "EAR", "HPL EAR Sel2"},
	{"EAR Gain", NULL, "EAR Switch"},
	{"EAR Pin", NULL, "EAR Gain"},

/* AD route */
	{"ADCL Switch", NULL, "ADCL_CLK"},
	{"ADCR Switch", NULL, "ADCR_CLK"},
	{"ADCL Gain", NULL, "ADCL Mixer"},
	{"ADCR Gain", NULL, "ADCR Mixer"},
	{"ADCL Switch", NULL, "ADCL Gain"},
	{"ADCR Switch", NULL, "ADCR Gain"},
	{"ADie Digital ADCL Switch", NULL, "ADCL Switch"},
	{"ADie Digital ADCR Switch", NULL, "ADCR Switch"},
	{"Digital ADC In Sel", "ADC", "ADie Digital ADCL Switch"},
	{"Digital ADC In Sel", "ADC", "ADie Digital ADCR Switch"},
	{"Digital ADC1 In Sel", "ADC", "ADie Digital ADCL Switch"},
	{"Digital ADC1 In Sel", "ADC", "ADie Digital ADCR Switch"},

	{"Digital ADCL Switch", NULL, "Digital ADC In Sel"},
	{"Digital ADCR Switch", NULL, "Digital ADC In Sel"},
	{"Digital ADC1L Switch", NULL, "Digital ADC1 In Sel"},
	{"Digital ADC1R Switch", NULL, "Digital ADC1 In Sel"},

	{"AUD ADC0L", "Switch", "Digital ADCL Switch"},
	{"AUD ADC0R", "Switch", "Digital ADCR Switch"},
	{"AUD ADC1L", "Switch", "Digital ADC1L Switch"},
	{"AUD ADC1R", "Switch", "Digital ADC1R Switch"},

	/* AUD ADC0 */
	{"ADC DSP_C", NULL, "AUD ADC0L"},
	{"ADC DSP_C", NULL, "AUD ADC0R"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC0L"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC0R"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC0L"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC0R"},
	{"ADC Voice", NULL, "AUD ADC0L"},
	{"ADC Voice", NULL, "AUD ADC0R"},
	{"ADC Voip", NULL, "AUD ADC0L"},
	{"ADC Voip", NULL, "AUD ADC0R"},
	{"ADC CODEC_TEST", NULL, "AUD ADC0L"},
	{"ADC CODEC_TEST", NULL, "AUD ADC0R"},
	{"ADC LOOP", NULL, "AUD ADC0L"},
	{"ADC LOOP", NULL, "AUD ADC0R"},
	{"ADC", NULL, "AUD ADC0L"},
	{"ADC", NULL, "AUD ADC0R"},
	{"ADC AP23", NULL, "AUD ADC0L"},
	{"ADC AP23", NULL, "AUD ADC0R"},
	{"ADC1", NULL, "AUD ADC0L"},
	{"ADC1", NULL, "AUD ADC0R"},

	/* AUD ADC1 */
	{"ADC DSP_C", NULL, "AUD ADC1L"},
	{"ADC DSP_C", NULL, "AUD ADC1R"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC1L"},
	{"ADC DSP_C_btsco_test", NULL, "AUD ADC1R"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC1L"},
	{"ADC DSP_C_fm_test", NULL, "AUD ADC1R"},
	{"ADC Voice", NULL, "AUD ADC1L"},
	{"ADC Voice", NULL, "AUD ADC1R"},
	{"ADC Voip", NULL, "AUD ADC1L"},
	{"ADC Voip", NULL, "AUD ADC1R"},
	{"ADC CODEC_TEST", NULL, "AUD ADC1L"},
	{"ADC CODEC_TEST", NULL, "AUD ADC1R"},
	{"ADC LOOP", NULL, "AUD ADC1L"},
	{"ADC LOOP", NULL, "AUD ADC1R"},
	{"ADC", NULL, "AUD ADC1L"},
	{"ADC", NULL, "AUD ADC1R"},
	{"ADC AP23", NULL, "AUD ADC1L"},
	{"ADC AP23", NULL, "AUD ADC1R"},
	{"ADC1", NULL, "AUD ADC1L"},
	{"ADC1", NULL, "AUD ADC1R"},

	{"ADC", NULL, "CLK_ADC"},
	{"ADC1", NULL, "CLK_ADC"},
	{"ADC AP23", NULL, "CLK_ADC"},
	{"ADC DSP_C", NULL, "CLK_ADC"},
	{"ADC DSP_C_btsco_test", NULL, "CLK_ADC"},
	{"ADC DSP_C_fm_test", NULL, "CLK_ADC"},
	{"ADC Voice", NULL, "CLK_ADC"},
	{"ADC Voip", NULL, "CLK_ADC"},
	{"ADC CODEC_TEST", NULL, "CLK_ADC"},
	{"ADC LOOP", NULL, "CLK_ADC"},

/* MIC */
	{"MICBIAS1", NULL, "BG"},
	{"MICBIAS2", NULL, "BG"},
	{"MIC Pin", NULL, "MICBIAS1"},
	{"MIC2 Pin", NULL, "MICBIAS2"},
	{"HPMIC Pin", NULL, "HEADMICBIAS"},
	{"ADCL Mixer", "MainMICADCL Switch", "MIC Pin"},
	{"ADCR Mixer", "AuxMICADCR Switch", "MIC2 Pin"},
	{"ADCL Mixer", "HPMICADCL Switch", "HPMIC Pin"},
	{"ADCR Mixer", "HPMICADCR Switch", "HPMIC Pin"},

/* IVSense */
	{"ISNS Mixer", "SPAISNS Switch", "SPK PA"},
	{"VSNS Mixer", "SPAVSNS Switch", "SPK PA"},
	{"ADCL Mixer", "VSENSEL Switch", "VSNS Mixer"},
	{"ADCR Mixer", "ISENSER Switch", "ISNS Mixer"},
	{"IVSense Virt", "Switch", "ADC"},
	{"IVSense Virt", "Switch", "ADC1"},
	{"IVSense Virt", NULL, "IVSense SRC"},
	{"DAC Fast", NULL, "IVSense Virt"},
	{"DAC Offload", NULL, "IVSense Virt"},

/* ADie loop */
	{"ADC-DAC Adie Loop", "switch", "ADC"},
	{"ADC1-DAC Adie Loop", "switch", "ADC1"},
	{"ADC-DAC Adie Loop post", NULL, "ADC-DAC Adie Loop"},
	{"ADC1-DAC Adie Loop post", NULL, "ADC1-DAC Adie Loop"},
	{"DAC", NULL, "ADC-DAC Adie Loop post"},
	{"DAC", NULL, "ADC1-DAC Adie Loop post"},

/* Ddie loop */
	{"ADC-DAC Digital Loop", "switch", "ADC"},
	{"ADC1-DAC Digital Loop", "switch", "ADC1"},
	{"ADC-DAC Digital Loop post", NULL, "ADC-DAC Digital Loop"},
	{"ADC1-DAC Digital Loop post", NULL, "ADC1-DAC Digital Loop"},
	{"DAC", NULL, "ADC-DAC Digital Loop post"},
	{"DAC", NULL, "ADC1-DAC Digital Loop post"},

	/* DMIC0 */
	{"DMIC Switch", NULL, "DMIC Pin"},
	{"Digital ADC In Sel", "DMIC", "DMIC Switch"},
	/* DMIC1 */
	{"DMIC1 Switch", NULL, "DMIC1 Pin"},
	{"Digital ADC1 In Sel", "DMIC", "DMIC1 Switch"},

	/* virt path */
	{"Virt Output", "Switch", "Digital DACL Switch"},
	{"Virt Output", "Switch", "Digital DACR Switch"},
	{"Virt Output Pin", NULL, "Virt Output"},

#endif
};

static int sprd_codec_info_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	u32 chip_id = 0;
	u32 ver_id = 0;

	chip_id = sci_get_ana_chip_id();
	ver_id = sci_get_ana_chip_ver();

	sp_asoc_pr_info("%s, chip_id = %d, ver_id=%d, AUDIO_CODEC_2730=%d\n",
		__func__, chip_id, ver_id, AUDIO_CODEC_2730);

	ucontrol->value.integer.value[0] = AUDIO_CODEC_2730;
	return 0;
}


static int sprd_codec_inter_pa_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int invert = mc->invert;

	ucontrol->value.integer.value[0] = sprd_codec->inter_pa.value;

	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_inter_pa_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int mask = BIT(fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;

	sp_asoc_pr_info("Config inter PA 0x%lx\n",
			ucontrol->value.integer.value[0]);

	val = ucontrol->value.integer.value[0] & mask;
	if (invert)
		val = max - val;

	sprd_codec->inter_pa.value = val;
	spk_pa_config(codec, sprd_codec->inter_pa.setting);

	return 0;
}

static int sprd_codec_dac_lrclk_sel_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	val = sprd_codec->lrclk_sel[LRCLK_SEL_DAC];
	ucontrol->value.enumerated.item[0] = !!val;

	sp_asoc_pr_dbg("%s, %u\n",
		__func__, ucontrol->value.enumerated.item[0]);

	return 0;
}

static int sprd_codec_dac_lrclk_sel_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s, %d\n",
		__func__, (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_DAC] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(DAC_LR_SEL) : 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL),
		BIT(DAC_LR_SEL), val);
	agdsp_access_disable();

	return 0;
}

static int sprd_codec_adc_lrclk_sel_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	val = sprd_codec->lrclk_sel[LRCLK_SEL_ADC];
	ucontrol->value.enumerated.item[0] = !!val;

	sp_asoc_pr_dbg("%s, %u\n",
		__func__, ucontrol->value.enumerated.item[0]);

	return 0;
}

static int sprd_codec_adc_lrclk_sel_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	int ret;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s, %d\n",
		__func__, (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_ADC] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(ADC_LR_SEL) : 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_I2S_CTL),
		BIT(ADC_LR_SEL), val);
	agdsp_access_disable();

	return 0;
}

static int sprd_codec_adc1_lrclk_sel_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	val = sprd_codec->lrclk_sel[LRCLK_SEL_ADC1];
	ucontrol->value.enumerated.item[0] = !!val;

	sp_asoc_pr_dbg("%s, %u\n",
		__func__, ucontrol->value.enumerated.item[0]);

	return 0;
}

static int sprd_codec_adc1_lrclk_sel_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	int ret;

	if (ucontrol->value.integer.value[0] >= texts->items) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s, %d\n",
		__func__, (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_ADC1] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(ADC1_LR_SEL) : 0;
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s:agdsp_access_enable:error:%d", __func__, ret);
		return ret;
	}
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC1_I2S_CTL),
		BIT(ADC1_LR_SEL), val);
	agdsp_access_disable();

	return 0;
}

static int sprd_codec_fixed_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->fixed_sample_rate[0];

	return 0;
}

static int sprd_codec_fixed_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int i;
	u32 fixed_rate;

	fixed_rate = ucontrol->value.integer.value[0];

	pr_info("%s fixed rate=%u\n", __func__, fixed_rate);
	for (i = 0; i < CODEC_PATH_MAX; i++)
		sprd_codec->fixed_sample_rate[i] = fixed_rate;

	return 0;
}

static int sprd_codec_get_hp_mix_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->hp_mix_mode;

	return 0;
}

#ifdef AUTO_DA_MODE_SWITCH
static int sprd_codec_da_mode_switch(struct snd_soc_codec *codec, bool on)
{
	struct snd_kcontrol *kctrl;
	struct snd_ctl_elem_value ucontrol = {
		.value.integer.value[0] = on,
	};

	kctrl = sprd_codec_find_ctrl(codec, "DA mode Switch");
	sp_asoc_pr_dbg("%s, ctrl=%p, %s\n", __func__,
		kctrl, on ? "on" : "off");
	if (!kctrl)
		return -EPERM;

	return snd_soc_dapm_put_volsw(kctrl, &ucontrol);
}
#endif

static int sprd_codec_set_hp_mix_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->hp_mix_mode = ucontrol->value.integer.value[0];

	return 0;
}

static int sprd_codec_spk_dg_fall_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_codec->spk_fall_dg;

	return 0;
}

static int sprd_codec_spk_dg_fall_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sprd_codec->spk_fall_dg = ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new sprd_codec_snd_controls[] = {

	SOC_ENUM_EXT("Aud Codec Info", codec_info_enum,
		sprd_codec_info_get, NULL),
	SOC_SINGLE_EXT("Inter PA Config", 0, 0, INT_MAX, 0,
		sprd_codec_inter_pa_get, sprd_codec_inter_pa_put),
	SOC_ENUM_EXT("DAC LRCLK Select", lrclk_sel_enum,
		sprd_codec_dac_lrclk_sel_get,
		sprd_codec_dac_lrclk_sel_put),
	SOC_ENUM_EXT("ADC LRCLK Select", lrclk_sel_enum,
		sprd_codec_adc_lrclk_sel_get,
		sprd_codec_adc_lrclk_sel_put),
	SOC_ENUM_EXT("ADC1 LRCLK Select", lrclk_sel_enum,
		sprd_codec_adc1_lrclk_sel_get,
		sprd_codec_adc1_lrclk_sel_put),
	SOC_SINGLE_EXT("HP mix mode", 0, 0, INT_MAX, 0,
		sprd_codec_get_hp_mix_mode, sprd_codec_set_hp_mix_mode),
	SOC_SINGLE_EXT("SPK DG fall", 0, 0, INT_MAX, 0,
		sprd_codec_spk_dg_fall_get, sprd_codec_spk_dg_fall_set),

	SOC_ENUM("DAS Input Mux", das_input_mux_enum),
	SOC_SINGLE_EXT("TEST_FIXED_RATE", 0, 0, INT_MAX, 0,
		sprd_codec_fixed_rate_get, sprd_codec_fixed_rate_put),
	SOC_SINGLE_EXT("Codec Digital Access Disable", SND_SOC_NOPM, 0, 1, 0,
		dig_access_disable_get, dig_access_disable_put),
};

static unsigned int sprd_codec_read(struct snd_soc_codec *codec,
				    unsigned int reg)
{
	int ret = 0;

	/*
	 * Because snd_soc_update_bits reg is 16 bits short type,
	   so muse do following convert
	 */
	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		return arch_audio_codec_read(reg);
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;

		ret = agdsp_access_enable();
		if (ret) {
			pr_err("%s, agdsp_access_enable failed!\n", __func__);
			return ret;
		}
		codec_digital_reg_enable(codec);
		ret = readl_relaxed((void __iomem *)(reg -
			CODEC_DP_BASE + sprd_codec_dp_base));
		codec_digital_reg_disable(codec);
		agdsp_access_disable();

		return ret;
	}

	sp_asoc_pr_dbg("read the register is not codec's reg = 0x%x\n",
		reg);

	return 0;
}

static int sprd_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			    unsigned int val)
{
	int ret = 0;

	if (IS_SPRD_CODEC_AP_RANG(reg | SPRD_CODEC_AP_BASE_HI)) {
		reg |= SPRD_CODEC_AP_BASE_HI;
		sp_asoc_pr_reg("A[0x%04x] R:[0x%08x]\n",
			(reg - CODEC_AP_BASE) & 0xFFFF,
			arch_audio_codec_read(reg));
		ret = arch_audio_codec_write(reg, val);
		sp_asoc_pr_reg("A[0x%04x] W:[0x%08x] R:[0x%08x]\n",
			(reg - CODEC_AP_BASE) & 0xFFFF,
			val, arch_audio_codec_read(reg));
		return ret;
	} else if (IS_SPRD_CODEC_DP_RANG(reg | SPRD_CODEC_DP_BASE_HI)) {
		reg |= SPRD_CODEC_DP_BASE_HI;

		ret = agdsp_access_enable();
		if (ret) {
			pr_err("%s, agdsp_access_enable failed!\n", __func__);
			return ret;
		}
		codec_digital_reg_enable(codec);
		sp_asoc_pr_reg("D[0x%04x] R:[0x%08x]\n",
			(reg - CODEC_DP_BASE) & 0xFFFF,
			readl_relaxed((void __iomem *)(reg -
			    CODEC_DP_BASE + sprd_codec_dp_base)));

		writel_relaxed(val, (void __iomem *)(reg -
			CODEC_DP_BASE + sprd_codec_dp_base));
		sp_asoc_pr_reg("D[0x%04x] W:[0x%08x] R:[0x%08x]\n",
			(reg - CODEC_DP_BASE) & 0xFFFF, val,
			readl_relaxed((void __iomem *)(reg -
			    CODEC_DP_BASE + sprd_codec_dp_base)));
		codec_digital_reg_disable(codec);
		agdsp_access_disable();

		return ret;
	}

	sp_asoc_pr_dbg("write the register is not codec's reg = 0x%x\n",
		reg);

	return ret;
}

static int sprd_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	int mask = 0xf, shift = 0;
	struct snd_soc_codec *codec = dai->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u32 *fixed_rate = sprd_codec->fixed_sample_rate;
	u32 rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sprd_codec->da_sample_val = fixed_rate[CODEC_PATH_DA] ?
			fixed_rate[CODEC_PATH_DA] : rate;
		sprd_codec_set_sample_rate(codec,
			sprd_codec->da_sample_val, mask, shift);
		sp_asoc_pr_info("Playback rate is [%u]\n",
			sprd_codec->da_sample_val);
	} else {
		sprd_codec->ad_sample_val = fixed_rate[CODEC_PATH_AD] ?
			fixed_rate[CODEC_PATH_AD] : rate;
		sprd_codec_set_ad_sample_rate(codec,
			sprd_codec->ad_sample_val, mask, shift);
		sp_asoc_pr_info("Capture rate is [%u]\n",
			sprd_codec->ad_sample_val);

		sprd_codec->ad1_sample_val = fixed_rate[CODEC_PATH_AD1]
			? fixed_rate[CODEC_PATH_AD1] : rate;
		sprd_codec_set_ad_sample_rate(codec,
			sprd_codec->ad1_sample_val,
			ADC1_SRC_N_MASK, ADC1_SRC_N);
		sp_asoc_pr_info("Capture(ADC1) rate is [%u]\n",
			sprd_codec->ad1_sample_val);
	}

	return 0;
}

static int sprd_codec_pcm_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	return 0;
}

static int sprd_codec_pcm_hw_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct sprd_codec_priv *sprd_codec =
		snd_soc_codec_get_drvdata(dai->codec);

	/* notify only once */
	if (sprd_codec->startup_cnt == 0)
		headset_set_audio_state(true);
	sprd_codec->startup_cnt++;
	sp_asoc_pr_dbg("%s, startup_cnt=%d\n",
		__func__, sprd_codec->startup_cnt);
	return 0;
}

static void sprd_codec_pcm_hw_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct sprd_codec_priv *sprd_codec =
		snd_soc_codec_get_drvdata(dai->codec);

	if (sprd_codec->startup_cnt > 0)
		sprd_codec->startup_cnt--;
	if (sprd_codec->startup_cnt == 0)
		headset_set_audio_state(false);
	sp_asoc_pr_dbg("%s, startup_cnt=%d\n",
		__func__, sprd_codec->startup_cnt);
}

static irqreturn_t sprd_codec_dp_irq(int irq, void *dev_id)
{
	int mask;
	struct sprd_codec_priv *sprd_codec = dev_id;
	struct snd_soc_codec *codec = sprd_codec->codec;

	mask = snd_soc_read(codec, AUD_AUD_STS0);
	sp_asoc_pr_dbg("dac mute irq mask = 0x%x\n", mask);
	if (BIT(DAC_MUTE_D_MASK) & mask) {
		mask = BIT(DAC_MUTE_D);
		complete(&sprd_codec->completion_dac_mute);
	}
	if (BIT(DAC_MUTE_U_MASK) & mask)
		mask = BIT(DAC_MUTE_U);
	snd_soc_update_bits(codec, SOC_REG(AUD_INT_EN), mask, 0);

	return IRQ_HANDLED;
}

static struct snd_soc_dai_ops sprd_codec_dai_ops = {
	.hw_params = sprd_codec_pcm_hw_params,
	.hw_free = sprd_codec_pcm_hw_free,
	.startup = sprd_codec_pcm_hw_startup,
	.shutdown = sprd_codec_pcm_hw_shutdown,
};

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
/* procfs interfaces for factory test: check if audio PA P/N is shorted. */
static void pa_short_stat_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;

	snd_iprintf(buffer, "CP short stat=%d\nPA short stat=%d\n",
		sprd_codec->cp_short_stat, sprd_codec->pa_short_stat);
}

static void sprd_codec_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;
	struct snd_soc_codec *codec = sprd_codec->codec;
	int reg, ret;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return;
	}
	snd_iprintf(buffer, "%s digital part\n", codec->component.name);
	for (reg = SPRD_CODEC_DP_BASE; reg < SPRD_CODEC_DP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(unsigned int)(reg - SPRD_CODEC_DP_BASE)
			, snd_soc_read(codec, reg + 0x00)
			, snd_soc_read(codec, reg + 0x04)
			, snd_soc_read(codec, reg + 0x08)
			, snd_soc_read(codec, reg + 0x0C)
		);
	}
	agdsp_access_disable();

	snd_iprintf(buffer, "%s analog part\n",
		codec->component.name);
	for (reg = SPRD_CODEC_AP_BASE;
			reg < SPRD_CODEC_AP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(unsigned int)(reg - SPRD_CODEC_AP_BASE)
			, snd_soc_read(codec, reg + 0x00)
			, snd_soc_read(codec, reg + 0x04)
			, snd_soc_read(codec, reg + 0x08)
			, snd_soc_read(codec, reg + 0x0C)
		);
	}

	/* For AUDIF registers
	 * 0x5c0 = 0x700 - 0x140;
	 */
	snd_iprintf(buffer, "%s audif\n", codec->component.name);
	for (reg = CTL_BASE_AUD_TOPA_RF; reg < 0x30 + CTL_BASE_AUD_TOPA_RF;
			reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(unsigned int)(reg - CTL_BASE_AUD_TOPA_RF)
			, arch_audio_codec_read(reg + 0x00)
			, arch_audio_codec_read(reg + 0x04)
			, arch_audio_codec_read(reg + 0x08)
			, arch_audio_codec_read(reg + 0x0C)
		);
	}
}

#define REG_PAIR_NUM 30
static void aud_glb_reg_read(struct snd_info_entry *entry,
			  struct snd_info_buffer *buffer)
{
	int i = 0, j = 0;
	struct glb_reg_dump *aud_glb_reg = NULL;
	struct glb_reg_dump *reg_p = NULL;

	aud_glb_reg = devm_kzalloc(entry->card->dev,
				   sizeof(struct glb_reg_dump) * REG_PAIR_NUM,
				   GFP_KERNEL);
	if (aud_glb_reg == NULL) {
		sp_asoc_pr_info("%s, error, not enough memory!!!\n", __func__);
		return;
	}
	reg_p = aud_glb_reg;
	AUDIO_GLB_REG_DUMP_LIST(reg_p);

	snd_iprintf(buffer, "audio global register dump\n");
	for (i = 0; i < REG_PAIR_NUM; i++) {
		for (j = 0; j < aud_glb_reg[i].count; j++) {
			if (aud_glb_reg[i].func)
				goto free;
			if (j == 0)
				snd_iprintf(buffer, "%30s: 0x%08lx 0x%08x\n",
					aud_glb_reg[i].reg_name,
					aud_glb_reg[i].reg +
					sizeof(aud_glb_reg[i].reg) *
					j, aud_glb_reg[i].func(
					(void *)(aud_glb_reg[i].reg +
					sizeof(aud_glb_reg[i].reg) * j))
				);
			else
				snd_iprintf(buffer, "%27s+%2d: %#08lx %#08x\n",
					aud_glb_reg[i].reg_name,
					j, aud_glb_reg[i].reg +
					sizeof(aud_glb_reg[i].reg) *
					j, aud_glb_reg[i].func(
					(void *)(aud_glb_reg[i].reg +
					sizeof(aud_glb_reg[i].reg) * j))
				);
		}
	}

free:
	devm_kfree(entry->card->dev, aud_glb_reg);
}

static void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
	struct snd_info_entry *entry;
	struct snd_soc_codec *codec = sprd_codec->codec;
	struct snd_card *card = codec->component.card->snd_card;

	if (!snd_card_proc_new(card, "sprd-codec", &entry))
		snd_info_set_text_ops(entry, sprd_codec, sprd_codec_proc_read);
	if (!snd_card_proc_new(card, "aud-glb", &entry))
		snd_info_set_text_ops(entry, sprd_codec, aud_glb_reg_read);

	if (!snd_card_proc_new(card, "short-check-stat", &entry))
		snd_info_set_text_ops(entry, sprd_codec,
				      pa_short_stat_proc_read);
}
/* !CONFIG_PROC_FS */
#else
static inline void sprd_codec_proc_init(struct sprd_codec_priv *sprd_codec)
{
}
#endif

#define SPRD_CODEC_PCM_RATES \
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_11025 | \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_22050 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_96000)

#define SPRD_CODEC_PCM_AD_RATES \
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_32000 | \
	 UNSUPPORTED_AD_RATE | \
	 SNDRV_PCM_RATE_48000)

#define SPRD_CODEC_PCM_FATMATS (SNDRV_PCM_FMTBIT_S16_LE|SNDRV_PCM_FMTBIT_S24_LE)

/* PCM Playing and Recording default in full duplex mode */
static struct snd_soc_dai_driver sprd_codec_dai[] = {
/* 0: NORMAL_AP01 */
	{
		.name = "sprd-codec-normal-ap01",
		.playback = {
			.stream_name = "Normal-Playback-AP01",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Normal-Capture-AP01",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 1: NORMAL_AP23 */
	{
		.name = "sprd-codec-normal-ap23",
		.playback = {
			.stream_name = "Normal-Playback-AP23",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Normal-Capture-AP23",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 2: CAPTURE_DSP */
	{
		.name = "sprd-codec-capture-dsp",
		.capture = {
			.stream_name = "Capture-DSP",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 3: FAST_P */
	{
		.name = "sprd-codec-fast-playback",
		.playback = {
			.stream_name = "Fast-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 4: OFFLOAD */
	{
		.name = "sprd-codec-offload-playback",
		.playback = {
			.stream_name = "Offload-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 5: VOICE */
	{
		.name = "sprd-codec-voice",
		.playback = {
			.stream_name = "Voice-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Voice-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 6: VOIP */
	{
		.name = "sprd-codec-voip",
		.playback = {
			.stream_name = "Voip-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "Voip-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 7: FM */
	{
		.name = "sprd-codec-fm",
		.playback = {
			.stream_name = "Fm-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 8 loopback(dsp voice) */
	{
		.name = "sprd-codec-loop",
		.playback = {
			.stream_name = "LOOP-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "LOOP-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 9: FM dsp*/
	{
		.name = "sprd-codec-fm-dsp",
		.playback = {
			.stream_name = "Fm-DSP-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 10 */
	{
		.id = SPRD_CODEC_IIS0_ID,
		.name = "sprd-codec-ad1",
		.capture = {
			.stream_name = "Ext-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 11 */
	{
		.id = SPRD_CODEC_IIS0_ID,
		.name = "sprd-codec-ad1-voice",
		.capture = {
			.stream_name = "Ext-Voice-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 12: CAPTURE_DSP_fm test haps, remove it if not haps */
	{
		.name = "test_fm_codec_replace",
		.capture = {
			.stream_name = "Capture-DSP-FM-test",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 13: CAPTURE_DSP_btsco test haps, remove it if not haps */
	{
		.name = "test_btsco_codec_replace",
		.capture = {
			.stream_name = "Capture-DSP-BTSCO-test",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
	/* 14: codec test */
	{
		.name = "sprd-codec-test",
		.playback = {
			.stream_name = "CODEC_TEST-Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.capture = {
			.stream_name = "CODEC_TEST-Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_CODEC_PCM_AD_RATES,
			.formats = SPRD_CODEC_PCM_FATMATS,
		},
		.ops = &sprd_codec_dai_ops,
	},
};

static void codec_reconfig_dai_rate(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct sprd_card_data *mdata = snd_soc_card_get_drvdata(card);
	unsigned int replace_adc_rate =
		mdata ? mdata->codec_replace_adc_rate : 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int *fixed_rates = sprd_codec->fixed_sample_rate;
	unsigned int unsupported_rates = UNSUPPORTED_AD_RATE;
	struct snd_soc_dai *dai;

	if (replace_adc_rate &&
	    (fixed_rates[CODEC_PATH_AD] || fixed_rates[CODEC_PATH_AD1]))
		dev_warn(codec->dev, "%s, both replacingrate and fixed rate are set for adc!\n",
			 __func__);

	/* VBC SRC supported input rate: 32000, 44100, 48000. */
	if (replace_adc_rate == 32000 || replace_adc_rate == 48000) {
		sprd_codec->replace_rate = replace_adc_rate;
		dev_info(codec->dev, "%s, use rate(%u) for the unsupported rate capture.\n",
			 __func__, replace_adc_rate);
		return;
	}

	/*
	 * If fixed_rates is provided, tell user that this codec supports
	 * all kinds of rates.
	 */
	list_for_each_entry(dai, &codec->component.dai_list, list) {
		if (fixed_rates[CODEC_PATH_DA])
			dai->driver->playback.rates = SNDRV_PCM_RATE_CONTINUOUS;
		if ((dai->driver->id == SPRD_CODEC_IIS0_ID &&
		     fixed_rates[CODEC_PATH_AD]) ||
		    (dai->driver->id != SPRD_CODEC_IIS0_ID &&
		     fixed_rates[CODEC_PATH_AD1]))
			dai->driver->capture.rates = SNDRV_PCM_RATE_CONTINUOUS;
		else
			dai->driver->capture.rates &= ~unsupported_rates;
	}
}

static int sprd_codec_soc_probe(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	if (!sprd_codec) {
		pr_err("%s sprd_codec is NULL!\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s\n", __func__);

	codec_reconfig_dai_rate(codec);

	dapm->idle_bias_off = 1;

	sprd_codec->codec = codec;

	sprd_codec_proc_init(sprd_codec);

	snd_soc_dapm_ignore_suspend(dapm, "Offload-Playback");
	snd_soc_dapm_ignore_suspend(dapm, "Fm-Playback");
	snd_soc_dapm_ignore_suspend(dapm, "Voice-Playback");
	snd_soc_dapm_ignore_suspend(dapm, "Voice-Capture");

	/*
	 * Even without headset driver, codec could work well.
	 * So, igore the return status here.
	 */
	ret = sprd_headset_soc_probe(codec);
	if (ret == -EPROBE_DEFER) {
		pr_info("The headset is not ready now\n");
		return ret;
	}

	return 0;
}

/* power down chip */
static int sprd_codec_soc_remove(struct snd_soc_codec *codec)
{
	sprd_headset_remove();

	return 0;
}

static int sprd_codec_power_regulator_init(struct sprd_codec_priv *sprd_codec,
					   struct device *dev)
{
	int ret;

	sprd_codec_power_get(dev, &sprd_codec->main_mic, "MICBIAS");
	sprd_codec_power_get(dev, &sprd_codec->head_mic, "HEADMICBIAS");
	sprd_codec_power_get(dev, &sprd_codec->vb, "VB");
	ret = regulator_enable(sprd_codec->vb);
	if (!ret) {
		regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_STANDBY);
		regulator_disable(sprd_codec->vb);
	}
	return 0;
}

static void sprd_codec_power_regulator_exit(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec_power_put(&sprd_codec->main_mic);
	sprd_codec_power_put(&sprd_codec->head_mic);
	sprd_codec_power_put(&sprd_codec->vb);
}

static struct snd_soc_codec_driver soc_codec_dev_sprd_codec = {
	.probe = sprd_codec_soc_probe,
	.remove = sprd_codec_soc_remove,
	.read = sprd_codec_read,
	.write = sprd_codec_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.component_driver = {
		.dapm_widgets = sprd_codec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(sprd_codec_dapm_widgets),
		.dapm_routes = sprd_codec_intercon,
		.num_dapm_routes = ARRAY_SIZE(sprd_codec_intercon),
		.controls = sprd_codec_snd_controls,
		.num_controls = ARRAY_SIZE(sprd_codec_snd_controls),
	}
};

enum {
	SOC_TYPE_WITH_AGCP,
	SOC_TYPE_WITHOUT_AGCP
};

static int sprd_codec_get_soc_type(struct device_node *np)
{
	int ret;
	const char *cmptbl;

	if (!np) {
		pr_err("%s: np is NULL!\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_string(np, "compatible", &cmptbl);
	if (unlikely(ret)) {
		pr_err("%s: node '%s' has no compatible prop?!\n",
			__func__, np->name);
		return -ENODEV;
	}

	if (strstr(cmptbl, "agcp"))
		return SOC_TYPE_WITH_AGCP;

	return SOC_TYPE_WITHOUT_AGCP;
}

static int sprd_codec_dig_probe(struct platform_device *pdev)
{
	int ret = 0;
	int soc_type;
	u32 val = 0;
	struct resource *res;
	struct regmap *agcp_ahb_gpr;
	struct regmap *aon_apb_gpr;
	struct regmap *anlg_phy_gpr;
	struct device_node *np = pdev->dev.of_node;
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);

	if (!np) {
		pr_err("ERR: [%s] np is NULL!\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!sprd_codec) {
		pr_err("ERR: [%s] sprd_codec_priv is NULL!\n", __func__);
		return -ENOMEM;
	}

	soc_type = sprd_codec_get_soc_type(np);
	if (soc_type == SOC_TYPE_WITH_AGCP) {
		/* Prepare for registers accessing. */
		agcp_ahb_gpr = syscon_regmap_lookup_by_phandle(
			np, "sprd,syscon-agcp-ahb");
		if (IS_ERR(agcp_ahb_gpr)) {
			pr_err("ERR: [%s] Get the codec aon apb syscon failed!(%ld)\n",
				__func__, PTR_ERR(agcp_ahb_gpr));
			agcp_ahb_gpr = NULL;
			return -EPROBE_DEFER;
		}
		arch_audio_set_agcp_ahb_gpr(agcp_ahb_gpr);
	} else if (soc_type == SOC_TYPE_WITHOUT_AGCP) {
		aon_apb_gpr = syscon_regmap_lookup_by_phandle(
			np, "sprd,syscon-aon-apb");
		if (IS_ERR(aon_apb_gpr)) {
			pr_err("ERR: [%s] Get the codec aon apb syscon failed!(%ld)\n",
				__func__, PTR_ERR(aon_apb_gpr));
			aon_apb_gpr = NULL;
			return -EPROBE_DEFER;
		}
		arch_audio_set_aon_apb_gpr(aon_apb_gpr);

		arch_audio_codec_switch2ap();
		val = platform_get_irq(pdev, 0);
		if (val > 0) {
			sprd_codec->dp_irq = val;
			sp_asoc_pr_dbg("Set DP IRQ is %u!\n", val);
		} else {
			pr_err("ERR:Must give me the DP IRQ!\n");
			return -EINVAL;
		}
		ret = devm_request_irq(&pdev->dev, sprd_codec->dp_irq,
			sprd_codec_dp_irq, 0, "sprd_codec_dp", sprd_codec);
		if (ret) {
			pr_err("ERR:Request irq dp failed!\n");
			return ret;
		}
		/* initial value for FM route */
		sprd_codec->da_sample_val = 44100;
		if (!of_property_read_u32(np, "sprd,def_da_fs", &val)) {
			sprd_codec->da_sample_val = val;
			sp_asoc_pr_dbg("Change DA default fs to %u!\n", val);
		}
	} else {
		pr_err("ERR: %s unknown soc type: %d", __func__, soc_type);
		return -EINVAL;
	}
	anlg_phy_gpr = syscon_regmap_lookup_by_phandle(
				np, "sprd,anlg-phy-g-syscon");
	if (IS_ERR(anlg_phy_gpr)) {
		dev_warn(&pdev->dev, "Get the sprd,anlg-phy-g-syscon failed!(%ld)\n",
			PTR_ERR(anlg_phy_gpr));
		anlg_phy_gpr = NULL;
	} else
		arch_audio_set_anlg_phy_g(anlg_phy_gpr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res != NULL) {
		sprd_codec_dp_base = (unsigned long)
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR_VALUE(sprd_codec_dp_base)) {
			pr_err("ERR: cannot create iomap address for codec DP!\n");
			return -EINVAL;
		}
	} else {
		pr_err("ERR:Must give me the codec DP reg address!\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_codec_dt_parse_mach(struct platform_device *pdev)
{
	int ret = 0, i;
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;

	if (!np || !sprd_codec) {
		pr_err("ERR: [%s] np(%p) or sprd_codec(%p) is NULL!\n",
		       __func__, np, sprd_codec);
		return 0;
	}

	/* Parsing codec properties located in machine. */
	ret = of_property_read_u32_array(np, "fixed-sample-rate",
		sprd_codec->fixed_sample_rate, CODEC_PATH_MAX);
	if (ret) {
		if (ret != -EINVAL)
			pr_warn("%s parsing 'fixed-sample-rate' failed!\n",
				__func__);
		for (i = 0; i < CODEC_PATH_MAX; i++)
			sprd_codec->fixed_sample_rate[i] = 0;
	}
	pr_debug("%s fixed sample rate of codec: %u, %u, %u\n", __func__,
		 sprd_codec->fixed_sample_rate[CODEC_PATH_DA],
		 sprd_codec->fixed_sample_rate[CODEC_PATH_AD],
		 sprd_codec->fixed_sample_rate[CODEC_PATH_AD1]);

	return 0;
}

static int sprd_codec_ana_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 val;
	struct device_node *np = pdev->dev.of_node;
	struct regmap *adi_rgmp;
	struct sprd_headset_global_vars glb_vars;
	struct sprd_codec_priv *sprd_codec;

	if (!np) {
		pr_err("ERR: [%s] there must be a analog node!\n", __func__);
		return -EPROBE_DEFER;
	}

	sprd_codec = platform_get_drvdata(pdev);
	if (!sprd_codec) {
		pr_err("ERR: [%s] sprd_codec_priv is NULL!\n", __func__);
		return -ENOMEM;
	}

	/* Prepare for registers accessing. */
	adi_rgmp = dev_get_regmap(pdev->dev.parent, NULL);
	if (!adi_rgmp) {
		pr_err("ERR: [%s] spi device is not ready yet!\n", __func__);
		return -EPROBE_DEFER;
	}
	ret = of_property_read_u32(np, "reg", &val);
	if (ret) {
		pr_err("ERR: %s :no property of 'reg'\n", __func__);
		return -ENXIO;
	}
	pr_err("%s :adi_rgmp = %p, val=0x%x\n", __func__, adi_rgmp, val);
	arch_audio_codec_set_regmap(adi_rgmp);
	arch_audio_codec_set_reg_offset((unsigned long)val);
	/* Set global register accessing vars for headset. */
	glb_vars.regmap = adi_rgmp;
	glb_vars.codec_reg_offset = val;
	/* yintang: marked for compiling */
	sprd_headset_set_global_variables(&glb_vars);

	/* Parsing configurations varying as machine. */
	ret = sprd_codec_dt_parse_mach(pdev);
	if (ret)
		return ret;

	return 0;
}

/* This @pdev is the one corresponding to analog part dt node. */
static int sprd_codec_probe(struct platform_device *pdev)
{
	struct sprd_codec_priv *sprd_codec;
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dig_np = NULL;
	struct platform_device *dig_pdev = NULL;
	u32 ana_chip_id;
	struct regmap *pmu_apb_gpr;

	pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s: Only OF is supported yet!\n", __func__);
		return -EINVAL;
	}

	sprd_codec = devm_kzalloc(&pdev->dev,
		sizeof(struct sprd_codec_priv), GFP_KERNEL);
	if (sprd_codec == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, sprd_codec);
	/* Probe for analog part(in PMIC). */
	ret = sprd_codec_ana_probe(pdev);
	if (ret < 0) {
		pr_err("%s: analog probe failed!\n", __func__);
		return ret;
	}

	/* Probe for digital part(in AP). */
	dig_np = of_parse_phandle(np, "digital-codec", 0);
	if (!dig_np) {
		pr_err("%s: Parse 'digital-codec' failed!\n", __func__);
		return -EINVAL;
	}
	pmu_apb_gpr =
	    syscon_regmap_lookup_by_phandle(np, "sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb_gpr)) {
		pr_err("ERR: %s Get the pmu apb syscon failed!\n", __func__);
		pmu_apb_gpr = NULL;
		return -EPROBE_DEFER;
	}

	mutex_init(&sprd_codec->dig_access_mutex);

	mutex_init(&sprd_codec->digital_enable_mutex);
	arch_audio_set_pmu_apb_gpr(pmu_apb_gpr);
	of_node_put(np);
	dig_pdev = of_find_device_by_node(dig_np);
	if (unlikely(!dig_pdev)) {
		pr_err("%s: this node has no pdev?!\n", __func__);
		return -EPROBE_DEFER;
	}
	platform_set_drvdata(dig_pdev, sprd_codec);

	ret = sprd_codec_read_efuse(pdev, "aud_pabst_vcal_efuse",
		&sprd_codec->aud_pabst_vcal);
	if (ret) {
		pr_err("%s:read pa_bst_vcal failed!\n", __func__);
		return ret;
	}

	ret = sprd_codec_dig_probe(dig_pdev);
	if (ret < 0) {
		pr_err("%s: digital probe failed!\n", __func__);
		return ret;
	}

	ana_chip_id  = sci_get_ana_chip_id() >> 16;
	pr_info("ana_chip_id is 0x%x\n", ana_chip_id);
	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_sprd_codec,
				     sprd_codec_dai,
				     ARRAY_SIZE(sprd_codec_dai));
	if (ret != 0) {
		pr_err("ERR:Failed to register CODEC: %d\n", ret);
		return ret;
	}
	sprd_codec_inter_pa_init(sprd_codec);
	sprd_codec_power_regulator_init(sprd_codec, &pdev->dev);

	cp_short_check(sprd_codec);
	spk_pa_short_check(sprd_codec);
	load_ocp_pfw_cfg(sprd_codec);
	sprd_codec_init_fgu(sprd_codec);

	ret = sprd_codec_read_efuse(pdev, "neg_cp_efuse",
					&sprd_codec->neg_cp_efuse);
	if (ret)
		sprd_codec->neg_cp_efuse = 0xe1;
	ret = sprd_codec_read_efuse(pdev, "fgu_4p2_efuse",
					&sprd_codec->fgu_4p2_efuse);
	if (ret)
		sprd_codec->fgu_4p2_efuse = 0;

	return 0;
}

static int sprd_codec_remove(struct platform_device *pdev)
{
	struct sprd_codec_priv *sprd_codec = platform_get_drvdata(pdev);

	sprd_codec_power_regulator_exit(sprd_codec);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id codec_of_match[] = {
	{.compatible = "sprd,sc2730-audio-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, codec_of_match);
#endif

static struct platform_driver sprd_codec_codec_driver = {
	.driver = {
		.name = "sprd-codec-sc2730",
		.owner = THIS_MODULE,
		.of_match_table = codec_of_match,
	},
	.probe = sprd_codec_probe,
	.remove = sprd_codec_remove,
};

static int __init sprd_codec_driver_init(void)
{
	return platform_driver_register(&sprd_codec_codec_driver);
}

late_initcall(sprd_codec_driver_init);

MODULE_DESCRIPTION("SPRD-CODEC ALSA SoC codec driver");
MODULE_AUTHOR("Jian Chen <jian.chen@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:sprd-codec");
