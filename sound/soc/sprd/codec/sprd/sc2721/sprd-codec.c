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
#define pr_fmt(fmt) pr_sprd_fmt("SC2721")""fmt

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
#include "sprd-headset-2721.h"

#define SC2721_UNSUPPORTED_AD_RATE SNDRV_PCM_RATE_44100

#define SOC_REG(r) ((unsigned short)(r))
#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define ID_FUN(id, lr) ((unsigned short)(((id) << 1) | (lr)))

#define SPRD_CODEC_AP_BASE_HI (SPRD_CODEC_AP_BASE & 0xFFFF0000)
#define SPRD_CODEC_DP_BASE_HI (SPRD_CODEC_DP_BASE & 0xFFFF0000)

enum PA_SHORT_T {
	PA_SHORT_NONE, /* PA output normal */
	PA_SHORT_VBAT, /* PA output P/N short VBAT */
	PA_SHORT_GND, /* PA output P/N short GND */
	PA_SHORT_CHECK_FAILED, /* error when do the check */
	PA_SHORT_MAX
};

enum {
	SPRD_CODEC_ANA_MIXER_ORDER = 98,
	SPRD_CODEC_PA_ORDER = 99,
	SPRD_CODEC_DC_OS_SWITCH_ORDER = 100,
	SPRD_CODEC_DC_OS_ORDER = 100,
	SPRD_CODEC_DA_EN_ORDER = 101,
	SPRD_CODEC_BUF_SWITCH_ORDER = 102,
	SPRD_CODEC_DEPOP_ORDER = 103,
	SPRD_CODEC_SWITCH_ORDER = 104,
	SPRD_CODEC_MIXER_ORDER = 105,/* Must be the last one */
};

enum {
	SPRD_CODEC_PLAYBACK,
	SPRD_CODEC_CAPTRUE,
	SPRD_CODEC_CAPTRUE1,
	SPRD_CODEC_CHAN_MAX,
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

enum {
	SPRD_CODEC_PGA_START = 0,
	SPRD_CODEC_PGA_SPKL = SPRD_CODEC_PGA_START,
	SPRD_CODEC_PGA_HPL,
	SPRD_CODEC_PGA_HPR,
	SPRD_CODEC_PGA_EAR,
	SPRD_CODEC_PGA_ADCL,
	SPRD_CODEC_PGA_ADCR,
	SPRD_CODEC_PGA_DAC,

	SPRD_CODEC_PGA_END
};

#define GET_PGA_ID(x)   ((x) - SPRD_CODEC_PGA_START)
#define SPRD_CODEC_PGA_MAX  (SPRD_CODEC_PGA_END - SPRD_CODEC_PGA_START)
static const char *sprd_codec_pga_debug_str[SPRD_CODEC_PGA_MAX] = {
	"SPKL",
	"HPL",
	"HPR",
	"EAR",
	"ADCL",
	"ADCR",
	"DAC",
};

typedef int (*sprd_codec_pga_set) (struct snd_soc_codec *codec, int pgaval);

struct sprd_codec_pga {
	sprd_codec_pga_set set;
	int min;
};

struct sprd_codec_pga_op {
	int pgaval;
	sprd_codec_pga_set set;
};

typedef int (*sprd_codec_switch_set) (struct snd_soc_codec *codec, int on);
struct sprd_codec_switch {
	sprd_codec_switch_set set;
	int min;
};

struct sprd_codec_switch_op {
	int on;
	sprd_codec_switch_set set;
};

enum {
	SPRD_CODEC_LEFT = 0,
	SPRD_CODEC_RIGHT = 1,
};

enum {
	SPRD_CODEC_MIXER_START = SPRD_CODEC_PGA_END + 20,
	SPRD_CODEC_MAIN_MIC = SPRD_CODEC_MIXER_START,
	SPRD_CODEC_AUX_MIC,
	SPRD_CODEC_HP_MIC,
	SPRD_CODEC_ADC_MIXER_MAX,
	SPRD_CODEC_HP_DACL = SPRD_CODEC_ADC_MIXER_MAX,
	SPRD_CODEC_HP_DACR,
	SPRD_CODEC_HP_DAC_MIXER_MAX,
	SPRD_CODEC_HP_ADCL = SPRD_CODEC_HP_DAC_MIXER_MAX,
	SPRD_CODEC_HP_ADCR,
	SPRD_CODEC_HP_MIXER_MAX,
	SPRD_CODEC_SPK_DACL = SPRD_CODEC_HP_MIXER_MAX,
	SPRD_CODEC_SPK_DACR,
	SPRD_CODEC_SPK_DAC_MIXER_MAX,
	SPRD_CODEC_SPK_ADCL = SPRD_CODEC_SPK_DAC_MIXER_MAX,
	SPRD_CODEC_SPK_ADCR,
	SPRD_CODEC_SPK_MIXER_MAX,
	SPRD_CODEC_EAR_DACL = SPRD_CODEC_SPK_MIXER_MAX,
	SPRD_CODEC_EAR_MIXER_MAX,

	SPRD_CODEC_MIXER_END
};

#define GET_MIXER_ID(x) ((unsigned short)(x) -\
	ID_FUN(SPRD_CODEC_MIXER_START, SPRD_CODEC_LEFT))
#define SPRD_CODEC_MIXER_MAX \
	(ID_FUN(SPRD_CODEC_MIXER_END, SPRD_CODEC_LEFT) - \
	ID_FUN(SPRD_CODEC_MIXER_START, SPRD_CODEC_LEFT))
static const char *sprd_codec_mixer_debug_str[SPRD_CODEC_MIXER_MAX] = {
	"MAIN MIC->ADCL",
	"MAIN MIC->ADCR",
	"AUX MIC->ADCL",
	"AUX MIC->ADCR",
	"HP MIC->ADCL",
	"HP MIC->ADCR",
	"DACL->HPL",
	"DACL->HPR",
	"DACR->HPL",
	"DACR->HPR",
	"ADCL->HPL",
	"ADCL->HPR",
	"ADCR->HPL",
	"ADCR->HPR",
	"DACL->SPKL",
	"DACL->SPKR",
	"DACR->SPKL",
	"DACR->SPKR",
	"ADCL->SPKL",
	"ADCL->SPKR",
	"ADCR->SPKL",
	"ADCR->SPKR",
	"DACL->EAR",
	"DACR->EAR(bug)"
};

#define IS_SPRD_CODEC_MIXER_RANG(reg) \
	(((reg) >= ID_FUN(SPRD_CODEC_MIXER_START, SPRD_CODEC_LEFT)) && \
	((reg) <= ID_FUN(SPRD_CODEC_MIXER_END, SPRD_CODEC_LEFT)))
#define IS_SPRD_CODEC_PGA_RANG(reg) (((reg) >= SPRD_CODEC_PGA_START) && \
	((reg) <= SPRD_CODEC_PGA_END))
#define IS_SPRD_CODEC_MIC_BIAS_RANG(reg) \
	(((reg) >= SPRD_CODEC_MIC_BIAS_START) && \
	((reg) <= SPRD_CODEC_MIC_BIAS_END))
#define IS_SPRD_CODEC_SWITCH_RANG(reg) \
	(((reg) >= SPRD_CODEC_SWITCH_START) && \
	((reg) <= SPRD_CODEC_SWITCH_END))
#define IS_SPRD_CODEC_ADC_LOOP_RANG(reg) \
	(((reg) >= SPRD_CODEC_ADC_LOOP_START) && \
	((reg) <= SPRD_CODEC_ADC_LOOP_END))

typedef int (*sprd_codec_mixer_set) (struct snd_soc_codec *codec, int on);
struct sprd_codec_mixer {
	int on;
	sprd_codec_mixer_set set;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int xtlbuf1_eb_set(void)
__attribute__ ((weak, alias("__xtlbuf1_eb_set")));

int xtlbuf1_eb_clr(void)
__attribute__ ((weak, alias("__xtlbuf1_eb_clr")));

static int __xtlbuf1_eb_set(void)
{
	pr_debug("%s not defined\n", __func__);

	return 0;
}

static int __xtlbuf1_eb_clr(void)
{
	pr_debug("%s not defined\n", __func__);

	return 0;
}

uint32_t sprd_get_vbat_voltage(void)
	__attribute__ ((weak, alias("__sprd_get_vbat_voltage")));
static uint32_t __sprd_get_vbat_voltage(void)
{
	pr_err("ERR: Can't get vbat!\n");
	return 3800;
}

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

struct sprd_codec_ldo_v_map {
	int ldo_v_level;
	int volt;
};

static const struct sprd_codec_ldo_v_map ldo_v_map[] = {
	{LDO_V_3000, 3500},
	{LDO_V_3000, 3600},
	{LDO_V_3100, 3700},
	{LDO_V_3200, 3800},
	{LDO_V_3300, 3900},
	{LDO_V_3300, 4000},
	{LDO_V_3300, 4100},
	{LDO_V_3300, 4300},
	{LDO_V_3300, 5000},
};

struct sprd_codec_ldo_cfg {
	const struct sprd_codec_ldo_v_map *v_map;
	int v_map_size;
};

struct sprd_codec_inter_pa {
	/* FIXME little endian */
	u32 DTRI_F_sel:3;
	u32 is_DEMI_mode:1;
	u32 is_classD_mode:1;
	u32 EMI_rate:3;
	u32 RESV:25;
};

struct sprd_codec_pa_setting {
	union {
		struct sprd_codec_inter_pa setting;
		u32 value;
	};
	int set;
};

enum {
	SPRD_CODEC_MIC_BIAS_START = ID_FUN(SPRD_CODEC_MIXER_END,
					   SPRD_CODEC_LEFT) + 100,
	SPRD_CODEC_MIC_BIAS = SPRD_CODEC_MIC_BIAS_START,
	SPRD_CODEC_HEADMIC_BIAS,

	SPRD_CODEC_MIC_BIAS_END
};

#define GET_MIC_BIAS_ID(x) ((x) - SPRD_CODEC_MIC_BIAS_START)
#define SPRD_CODEC_MIC_BIAS_MAX \
	(SPRD_CODEC_MIC_BIAS_END - SPRD_CODEC_MIC_BIAS_START)
static const char *mic_bias_name[SPRD_CODEC_MIC_BIAS_MAX] = {
	"Mic Bias",
	"HeadMic Bias",
};

enum {
	SPRD_CODEC_SWITCH_START = SPRD_CODEC_MIC_BIAS_END + 20,
	SPRD_CODEC_DACL = SPRD_CODEC_SWITCH_START,
	SPRD_CODEC_DACR,
	SPRD_CODEC_DACS,

	SPRD_CODEC_SWITCH_END
};

#define GET_SWITCH_ID(x) ((x) - SPRD_CODEC_SWITCH_START)
#define SPRD_CODEC_SWITCH_MAX \
	(SPRD_CODEC_SWITCH_END - SPRD_CODEC_SWITCH_START)
static const char *switch_name[SPRD_CODEC_SWITCH_MAX] = {
	"DACL",
	"DACR",
	"DACS"
};

enum {
	SPRD_CODEC_ADC_LOOP_START = SPRD_CODEC_SWITCH_END + 20,
	SPRD_CODEC_ADC_DAC_ADIE_LOOP = SPRD_CODEC_ADC_LOOP_START,
	SPRD_CODEC_ADC1_DAC_ADIE_LOOP,
	SPRD_CODEC_ADC_DAC_DIGITAL_LOOP,
	SPRD_CODEC_ADC1_DAC_DIGITAL_LOOP,

	SPRD_CODEC_ADC_LOOP_END
};

#define GET_ADC_LOOP_ID(x)   ((x) - SPRD_CODEC_ADC_LOOP_START)
#define SPRD_CODEC_ADC_LOOP_MAX \
	(SPRD_CODEC_ADC_LOOP_END - SPRD_CODEC_ADC_LOOP_START)
static const char *adc_loop_name[SPRD_CODEC_ADC_LOOP_MAX] = {
	"ADC->DAC ADIE LOOP",
	"ADC1->DAC ADIE LOOP",
	"ADC->DAC DIGITAL LOOP",
	"ADC1->DAC DIGITAL LOOP",
};

static unsigned long sprd_codec_dp_base;

/*
 * In whale2, when make a BT call, an iis clock is needed from
 * digital codec, which needs some digital codec regs access.
 * So export virtual address of digital codec regs to VBC.
 */
unsigned long sprd_get_codec_dp_base(void)
{
	return sprd_codec_dp_base;
}
EXPORT_SYMBOL(sprd_get_codec_dp_base);

enum {
	SPRD_CODEC_HP_PA_VER_1,
	SPRD_CODEC_HP_PA_VER_2,
	SPRD_CODEC_HP_PA_VER_MAX,
};

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

/* codec private data */
struct sprd_codec_priv {
	struct snd_soc_codec *codec;
	u32 da_sample_val;
	u32 ad_sample_val;
	u32 ad1_sample_val;
	struct sprd_codec_mixer mixer[SPRD_CODEC_MIXER_MAX];
	struct sprd_codec_pga_op pga[SPRD_CODEC_PGA_MAX];
	struct sprd_codec_switch_op switcher[SPRD_CODEC_SWITCH_MAX];
	int mic_bias[SPRD_CODEC_MIC_BIAS_MAX];

	int ap_irq;
	struct delayed_work ap_irq_delayed_work;

	int dp_irq;
	struct completion completion_dac_mute;
	struct power_supply_desc audio_ldo_desc;
	struct power_supply *audio_ldo;

	struct sprd_codec_pa_setting inter_pa;
	/*mutex for internal pa setting*/
	struct mutex inter_pa_mutex;

	struct regulator *main_mic;
	struct regulator *head_mic;
	struct regulator *vb;
	atomic_t ldo_refcount;

	struct sprd_codec_ldo_cfg s_vcom;

	int ad_da_loop_en[SPRD_CODEC_ADC_LOOP_MAX];

	int sprd_dacspkl_enable;
	int sprd_dacspkl_set;
	/*mutex for dacspeakl setting */
	struct mutex sprd_dacspkl_mutex;
	u32 fixed_sample_rate[CODEC_PATH_MAX];
	u32 lrclk_sel[LRCLK_SEL_MAX];
	unsigned int replace_rate;
	enum PA_SHORT_T pa_short_stat;

	u32 startup_cnt;
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

static int sprd_codec_regulator_set(struct regulator **regu, int on)
{
	int ret = 0;

	if (!*regu)
		return 0;

	if (on) {
		ret = regulator_enable(*regu);
		if (ret) {
			sprd_codec_power_put(regu);
			return ret;
		}
	} else {
		ret = regulator_disable(*regu);
	}

	return ret;
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
void sprd_codec_wait(u32 wait_time)
{
	pr_debug("%s %d ms\n", __func__, wait_time);

	if (wait_time < 0)
		return;
	if (wait_time < 20)
		usleep_range(wait_time * 1000, wait_time * 1000 + 200);
	else
		msleep(wait_time);
}

static inline int _sprd_codec_ldo_cfg_set(
			struct sprd_codec_ldo_cfg *p_v_cfg,
			const struct sprd_codec_ldo_v_map *v_map,
			const int v_map_size)
{
	p_v_cfg->v_map = v_map;
	p_v_cfg->v_map_size = v_map_size;

	return 0;
}

static inline int _sprd_codec_hold(int index, int volt,
				   const struct sprd_codec_ldo_v_map *v_map)
{
	int scope = 40;		/* unit: mv */
	int ret = ((v_map[index].volt - volt) <= scope);

	if (index >= 1)
		return (ret || ((volt - v_map[index - 1].volt) <= scope));

	return ret;
}

static int sprd_codec_auto_ldo_volt(
				    struct snd_soc_codec *codec,
				    void (*set_level)(struct snd_soc_codec *, int),
	const struct sprd_codec_ldo_v_map *v_map,
	const int v_map_size, int init)
{
	int i;
	int volt = sprd_get_vbat_voltage();

	sp_asoc_pr_dbg("Get vbat voltage %d\n", volt);
	for (i = 0; i < v_map_size; i++) {
		if (volt <= v_map[i].volt) {
			sp_asoc_pr_dbg("Hold %d\n",
				       _sprd_codec_hold(i, volt, v_map));
			if (init || !_sprd_codec_hold(i, volt, v_map))
				set_level(codec, v_map[i].ldo_v_level);
			return 0;
		}
	}

	return -EFAULT;
}

static inline void sprd_codec_vcm_v_sel(struct snd_soc_codec *codec, int v_sel)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("VCM Set %d\n", v_sel);
	mask = VB_V_MASK << VB_V;
	val = (v_sel << VB_V) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU1), mask, val);
}

#define SPK_GAIN_MAX 2
static int sprd_codec_pga_spk_set(
			    struct snd_soc_codec *codec,
			    int pgaval)
{
	int reg, val, mask;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	bool is_class_d = sprd_codec->inter_pa.setting.is_classD_mode;

	reg = ANA_CDC4;
	mask = PA_G_MASK << PA_G;
	/* In class D mode, the gain of class AB takes effect, too.
	 * To make case simple, fix the class AB gain to 0dB when
	 * in class D mode.
	 */
	val = is_class_d ? ((pgaval << PA_D_G) | (PA_AB_G_0DB << PA_G)) :
		(pgaval << PA_G);
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static inline int hp_idx_to_reg_val(int index)
{
	int gain_map[] = {0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xf};

	if (index < 0 || index >= ARRAY_SIZE(gain_map)) {
		pr_err("%s, Invalid index: %d\n", __func__, index);
		return -EINVAL;
	}

	return gain_map[index];
}

static int sprd_codec_pga_hpl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;

	reg = ANA_CDC4;
	mask = HPL_G_MASK << HPL_G;
	pgaval = hp_idx_to_reg_val(pgaval);
	if (pgaval < 0)
		return pgaval;
	val = (pgaval << HPL_G) & mask;

	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_hpr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;

	reg = ANA_CDC4;
	mask = HPR_G_MASK << HPR_G;
	val = (hp_idx_to_reg_val(pgaval) << HPR_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static inline int rcv_idx_to_reg_val(int index)
{
	int gain_map[] = {0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xf};

	if (index < 0 || index >= ARRAY_SIZE(gain_map)) {
		pr_err("%s, Invalid index: %d\n", __func__, index);
		return -EINVAL;
	}

	return gain_map[index];
}

static int sprd_codec_pga_ear_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;

	reg = ANA_CDC4;
	mask = RCV_G_MASK << RCV_G;
	pgaval = rcv_idx_to_reg_val(pgaval);
	if (pgaval < 0)
		return pgaval;
	val = (pgaval << RCV_G) & mask;

	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_adcl_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;

	reg = ANA_CDC1;
	mask = ADPGAL_G_MASK << ADPGAL_G;
	val = (pgaval << ADPGAL_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_adcr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;

	reg = ANA_CDC1;
	mask = ADPGAR_G_MASK << ADPGAR_G;
	val = (pgaval << ADPGAR_G) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

static int sprd_codec_pga_daclr_set(struct snd_soc_codec *codec, int pgaval)
{
	int reg, val, mask;

	reg = ANA_PMU2;
	mask = DA_IG_MASK << DA_IG;
	val = (pgaval << DA_IG) & mask;
	return snd_soc_update_bits(codec, SOC_REG(reg), mask, val);
}

/* das dc offset setting */
/* TODO: add dc offset to AudioTestor. */
static int sprd_das_dc_os_en(struct snd_soc_codec *codec, int on)
{
	int mask, val;

	mask = BIT(DAS_OS_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);

	return 0;
}

static void sprd_das_dc_os_set(struct snd_soc_codec *codec, int offset)
{
	int mask, val;

	mask = DAS_OS_D_MASK << DAS_OS_D;
	val = offset << DAS_OS_D;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC1), mask, val);
}

static int das_dc_os_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (on)
		sprd_das_dc_os_set(codec, DAS_OS_D_2);
	sprd_das_dc_os_en(codec, on);

	return 0;
}

static int sprd_codec_switch_dacs_set(struct snd_soc_codec *codec, int on)
{
	int val, mask;

	mask = BIT(DAS_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask, val);

	/*dc offset*/
	/* sprd_das_dc_os_set(codec, on); */

	return 0;
}

static int sprd_codec_switch_dacl_set(struct snd_soc_codec *codec, int on)
{
	int val, mask;

	mask = BIT(DAL_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask, val);

	sprd_codec_wait(1);

	return 0;
}

static int sprd_codec_switch_dacr_set(struct snd_soc_codec *codec, int on)
{
	int val, mask;

	mask = BIT(DAR_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask, val);

	sprd_codec_wait(1);

	return 0;
}

static struct sprd_codec_pga sprd_codec_pga_cfg[SPRD_CODEC_PGA_MAX] = {
	{sprd_codec_pga_spk_set, 0},
	{sprd_codec_pga_hpl_set, 6},
	{sprd_codec_pga_hpr_set, 6},
	{sprd_codec_pga_ear_set, 8},

	{sprd_codec_pga_adcl_set, 0},
	{sprd_codec_pga_adcr_set, 0},

	{sprd_codec_pga_daclr_set, 0},
};

static struct sprd_codec_switch sprd_codec_switch_cfg[SPRD_CODEC_SWITCH_MAX] = {
	{sprd_codec_switch_dacl_set, 0},
	{sprd_codec_switch_dacr_set, 0},
	{sprd_codec_switch_dacs_set, 0},
};

/* adc mixer */
static int mainmicadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), BIT(SMIC1PGAL),
		on << SMIC1PGAL);
}

static int mainmicadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("[%s] Main mic can't connect with ADCR anymore.\n",
		       __func__);
	return 0;
}

static int auxmicadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d, Aux-mic can't connect with ADCL.\n",
		       __func__, on);
	return 0;
}

static int auxmicadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC3),
		BIT(SMIC2PGAR), on << SMIC2PGAR);
}

static int hpmicadcl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC3),
		BIT(SHMICPGAL), on << SHMICPGAL);
}

static int hpmicadcr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC3),
		BIT(SHMICPGAR), on << SHMICPGAR);
}

/* hp mixer */
static int daclhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);

	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC3),
		BIT(SDALHPL), on << SDALHPL);
}

static int daclhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. DACL can't be connected to HPR.\n",
		       __func__, on);
	return 0;
}

static int dacrhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. DACL can't be connected to HPR.\n",
		       __func__, on);
	return 0;
}

static int dacrhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC3),
		BIT(SDARHPR), on << SDARHPR);
}

static int adclhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return 0;
}

static int adclhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. LINL can't be connected to HPR.\n",
		       __func__, on);
	return 0;
}

static int adcrhpl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. LINR can't be connected to HPL.\n",
		       __func__, on);
	return 0;
}

static int adcrhpr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return 0;
}

/* spkl mixer */
static int daclspkl_set(struct snd_soc_codec *codec, int on)
{
	int ret = 0;
	int enable;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	mutex_lock(&sprd_codec->sprd_dacspkl_mutex);
	if (on)
		sprd_codec->sprd_dacspkl_set |= BIT(SPRD_CODEC_LEFT);
	else
		sprd_codec->sprd_dacspkl_set &= ~BIT(SPRD_CODEC_LEFT);
	enable = sprd_codec->sprd_dacspkl_enable & BIT(SPRD_CODEC_LEFT);
	sp_asoc_pr_dbg("%s %d,enalbe :%d\n", __func__, on, enable);

	if (enable) {
		if (on) {
			/* wait 10ms for speaker pa switching on. */
			sprd_codec_wait(10);
		}
		ret = snd_soc_update_bits(codec,
					  SOC_REG(ANA_CDC3), BIT(SDAPA), on << SDAPA);
		if (!on) {
			/* wait 10ms, then speaker pa switches off. */
			sprd_codec_wait(10);
		}
	}
	mutex_unlock(&sprd_codec->sprd_dacspkl_mutex);

	return ret;
}

static int dacrspkl_set(struct snd_soc_codec *codec, int on)
{
	return 0;
}

static int adclspkl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return 0;
}

static int adcrspkl_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return 0;
}

/* spkr mixer */
static int daclspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. No right speaker anymore.\n", __func__, on);
	return 0;
}

static int dacrspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. No right speaker anymore.\n", __func__, on);
	return 0;
}

static int adclspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. No right speaker.\n", __func__, on);
	return 0;
}

static int adcrspkr_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d. No right speaker.\n", __func__, on);
	return 0;
}

/* ear mixer */

static int daclear_set(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, on);
	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), BIT(SDALRCV),
				   on << SDALRCV);
}

static sprd_codec_mixer_set mixer_set_func[SPRD_CODEC_MIXER_MAX] = {
	/* adc mixer */
	mainmicadcl_set, mainmicadcr_set,
	auxmicadcl_set, auxmicadcr_set,
	hpmicadcl_set, hpmicadcr_set,
	/* hp mixer */
	daclhpl_set, daclhpr_set,
	dacrhpl_set, dacrhpr_set,
	adclhpl_set, adclhpr_set,
	adcrhpl_set, adcrhpr_set,
	/* spk mixer */
	daclspkl_set, daclspkr_set,
	dacrspkl_set, dacrspkr_set,
	adclspkl_set, adclspkr_set,
	adcrspkl_set, adcrspkr_set,
	/* ear mixer */
	daclear_set, 0,
};

/* inter PA */
static inline void sprd_codec_pa_d_en(struct snd_soc_codec *codec, int on)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("%s set %d\n", __func__, on);
	mask = BIT(PA_D_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU5), mask, val);
}

static inline void sprd_codec_pa_emi_rate(struct snd_soc_codec *codec, int rate)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("%s set %d\n", __func__, rate);

	mask = PA_EMI_L_MASK << PA_EMI_L;
	val = (rate << PA_EMI_L) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU4), mask, val);
}

static inline void sprd_codec_pa_ovp_v_sel(struct snd_soc_codec *codec,
					   int v_sel)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("%s set %d\n", __func__, v_sel);
	mask = PA_OVP_V_MASK << PA_OVP_V;
	val = (v_sel << PA_OVP_V) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU3), mask, val);
}

static inline void sprd_codec_pa_dtri_f_sel(struct snd_soc_codec *codec,
					    int f_sel)
{
	int mask;
	int val;

	sp_asoc_pr_dbg("%s set %d\n", __func__, f_sel);
	mask = PA_DTRI_FC_MASK << PA_DTRI_FC;
	val = (f_sel << PA_DTRI_FC) & mask;
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU5), mask, val);
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

	mask = BIT(PA_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask, val);
}

static inline void sprd_codec_inter_pa_init(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec->inter_pa.setting.DTRI_F_sel = 0x2;
}

static inline void sprd_codec_dacspkl_enable_init(
	struct sprd_codec_priv *sprd_codec)
{
	sprd_codec->sprd_dacspkl_enable =
		BIT(SPRD_CODEC_RIGHT) | BIT(SPRD_CODEC_LEFT);
	sprd_codec->sprd_dacspkl_set = 0;

	mutex_init(&sprd_codec->sprd_dacspkl_mutex);
}

static inline void sprd_codec_switch_init(struct sprd_codec_priv *sprd_codec)
{
	int i = 0;

	for (i = 0; i < SPRD_CODEC_SWITCH_MAX; i++) {
		sprd_codec->switcher[i].set = NULL;
		sprd_codec->switcher[i].on = 1;
	}
}

/* NOTE: VB, BG & BIAS must be enabled before calling this function. */
static enum PA_SHORT_T codec_pa_short_check(struct snd_soc_codec *codec)
{
	int ret = 0;
	unsigned int val;

	ret = snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), BIT(PA_EN), 0);
	if (ret < 0)
		goto CHECK_FAILED;

	/* VBAT short check  */
	ret = snd_soc_update_bits(codec, SOC_REG(ANA_PMU5),
				  BIT(PA_SH_DET_EN), BIT(PA_SH_DET_EN));
	if (ret < 0)
		goto CHECK_FAILED;
	sprd_codec_wait(1);
	val = snd_soc_read(codec, SOC_REG(ANA_STS7));
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU5), BIT(PA_SH_DET_EN), 0);
	if (val == -1)
		goto CHECK_FAILED;
	if (val & BIT(PA_SH_FLAG)) {
		pr_err("ERR: Spk PA output short to VBAT! STS7: %#x\n", val);
		return PA_SHORT_VBAT;
	}

	/* GND short check  */
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU5),
			    BIT(PA_SL_DET_EN), BIT(PA_SL_DET_EN));
	if (ret < 0)
		goto CHECK_FAILED;
	sprd_codec_wait(1);
	val = snd_soc_read(codec, SOC_REG(ANA_STS7));
	snd_soc_update_bits(codec, SOC_REG(ANA_PMU5), BIT(PA_SL_DET_EN), 0);
	if (val == -1)
		goto CHECK_FAILED;
	if (val & BIT(PA_SL_FLAG)) {
		pr_err("ERR: Speaker PA output short to GND! STS7: %#x\n", val);
		return PA_SHORT_GND;
	}

	return PA_SHORT_NONE;

CHECK_FAILED:
	pr_err("ERR: pa short check failed! ret: %d\n", ret);
	return PA_SHORT_CHECK_FAILED;
}

static int pa_short_check_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	static bool first_check = true;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	if (!first_check)
		return 0;

	sprd_codec->pa_short_stat = codec_pa_short_check(codec);
	pr_debug("%s, pa_short_stat: %d\n", __func__,
		 sprd_codec->pa_short_stat);
	/* Check it every time.
	 * first_check = 0;
	 */

	return 0;
}

static int sprd_inter_speaker_pa(struct snd_soc_codec *codec, int on)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct sprd_codec_inter_pa *p_setting = &sprd_codec->inter_pa.setting;

	sp_asoc_pr_info("inter PA Switch %s\n", STR_ON_OFF(on));
	mutex_lock(&sprd_codec->inter_pa_mutex);
	if (on) {
		sprd_codec_pa_ovp_v_sel(codec, PA_OVP_V_5P8);

		sprd_codec_pa_d_en(codec, p_setting->is_classD_mode);
		if (p_setting->is_DEMI_mode)
			sprd_codec_pa_emi_rate(codec, p_setting->EMI_rate);

		sprd_codec_pa_dtri_f_sel(codec, p_setting->DTRI_F_sel);
		/* delay 20ms as weifeng's suggestion to avoid
		 * pop noise from spk
		 */
		sprd_codec_wait(20);
		sprd_codec_pa_en(codec, 1);
		sprd_codec->inter_pa.set = 1;
	} else {
		sprd_codec->inter_pa.set = 0;
		sprd_codec_pa_en(codec, 0);
	}
	mutex_unlock(&sprd_codec->inter_pa_mutex);

	return 0;
}

static int spk_pa_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	return sprd_inter_speaker_pa(codec, on);
}

/*dalr dc offset setting*/
/*TODO: add dc offset to AudioTestor.*/
static int sprd_dalr_dc_os_en(struct snd_soc_codec *codec, int on)
{
	int mask, val;

	mask = BIT(DALR_OS_EN);
	val = on ? mask : 0;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC3), mask, val);

	return 0;
}

static void sprd_dalr_dc_os_set(struct snd_soc_codec *codec, int offset)
{
	int mask, val;

	mask = DALR_OS_D_MASK << DALR_OS_D;
	val = offset << DALR_OS_D;
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC1), mask, val);
}

static int dalr_dc_os_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (on)
		sprd_dalr_dc_os_set(codec, DALR_OS_D_2);
	sprd_dalr_dc_os_en(codec, on);

	return 0;
}

static int sprd_codec_set_sample_rate(struct snd_soc_codec *codec,
				      u32 rate,
				      int mask, int shift)
{
	int i = 0;
	unsigned int val = 0;
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

	agdsp_access_enable();

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
	return (snd_pcm_rate_to_rate_bit(rate) & SC2721_UNSUPPORTED_AD_RATE);
}

static int sprd_codec_set_ad_sample_rate(struct snd_soc_codec *codec,
					 u32 rate,
					 int mask, int shift)
{
	int set;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int replace_rate = sprd_codec->replace_rate;

	agdsp_access_enable();

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
	sp_asoc_pr_dbg("%s AD %u DA %u AD1 %u\n", __func__,
		       sprd_codec->ad_sample_val, sprd_codec->da_sample_val,
		       sprd_codec->ad1_sample_val);
	agdsp_access_enable();

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

static inline int sprd_codec_vcom_ldo_auto(struct snd_soc_codec *codec,
					   int init)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	return sprd_codec_auto_ldo_volt(codec, sprd_codec_vcm_v_sel,
					sprd_codec->s_vcom.v_map,
					sprd_codec->s_vcom.v_map_size, init);
}

static inline int sprd_codec_vcom_ldo_cfg(struct sprd_codec_priv *sprd_codec,
					  const struct sprd_codec_ldo_v_map *v_map, const int v_map_size)
{
	return _sprd_codec_ldo_cfg_set(&sprd_codec->s_vcom, v_map, v_map_size);
}

static DEFINE_MUTEX(ldo_mutex);
static int sprd_codec_ldo_on(struct sprd_codec_priv *sprd_codec)
{
	struct regulator **regu = &sprd_codec->head_mic;

	sp_asoc_pr_dbg("%s\n", __func__);

	mutex_lock(&ldo_mutex);
	atomic_inc(&sprd_codec->ldo_refcount);
	if (atomic_read(&sprd_codec->ldo_refcount) == 1) {
		sp_asoc_pr_dbg("LDO ON!\n");
		/* peng: why do we disable hmic_bias sleep here? */
		if (*regu)
			regulator_set_mode(*regu, REGULATOR_MODE_NORMAL);
		arch_audio_codec_analog_enable();
		sprd_codec_vcom_ldo_auto(sprd_codec->codec, 1);
	}
	mutex_unlock(&ldo_mutex);

	return 0;
}

static int sprd_codec_ldo_off(struct sprd_codec_priv *sprd_codec)
{
	struct regulator **regu = &sprd_codec->head_mic;

	sp_asoc_pr_dbg("%s\n", __func__);

	mutex_lock(&ldo_mutex);
	if (atomic_dec_and_test(&sprd_codec->ldo_refcount)) {
		if (*regu && regulator_is_enabled(*regu))
			regulator_set_mode(*regu, REGULATOR_MODE_STANDBY);
		arch_audio_codec_analog_disable();
		sp_asoc_pr_dbg("LDO OFF!\n");
	}
	mutex_unlock(&ldo_mutex);

	return 0;
}

static void sprd_codec_analog_clk_on(struct snd_soc_codec *codec, int on)
{
	u32 mask = 0, val = 0;

	sp_asoc_pr_dbg("%s, on: %d\n", __func__, on);

	mask = BIT(DIG_CLK_6P5M_EN) | BIT(ANA_CLK_EN);
	val = on ? mask : 0;

	snd_soc_update_bits(codec, SOC_REG(ANA_CLK0), mask, val);
}

static int sprd_codec_analog_open(struct snd_soc_codec *codec)
{
	sp_asoc_pr_dbg("%s\n", __func__);

	return 0;
}

static int sprd_codec_digital_open(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s\n", __func__);

	/* FIXME : Some Clock SYNC bug will cause MUTE */
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_CTL), BIT(DAC_MUTE_EN), 0);

	sprd_codec_sample_rate_setting(sprd_codec);

	/*Bug 362021*/
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_L), 0xFFFF, 0X9999);
	snd_soc_update_bits(codec, SOC_REG(AUD_DAC_SDM_H), 0xFF, 0x1);

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

	return 0;
}

static void sprd_codec_cfga_clk_enable(struct snd_soc_codec *codec, int en)
{
	int msk, val;

	msk = BIT(CLK_AUD_6P5M_EN) | BIT(CLK_AUD_1K_EN) | BIT(CLK_AUD_32K_EN);
	val = en ? msk : 0;
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN), msk, val);
	msk = BIT(DAC_POST_SOFT_RST) | BIT(DIG_6P5M_SOFT_RST)
		| BIT(AUD_32K_SOFT_RST) | BIT(AUD_1K_SOFT_RST);
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_SOFT_RST), msk, msk);
	udelay(10);
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_SOFT_RST), msk, 0);
}

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

static int enable_oxp(struct snd_soc_codec *codec)
{
	unsigned int val, mask;

	sprd_codec_irq_oxp_enable(codec);

	/* Enable OVP and OCP. */
	mask = BIT(PA_OVP_PD) | BIT(PA_OVP_THD)
		| BIT(PA_OCP_PD) | BIT(DRV_OCP_PD)
		| (DRV_OCP_MODE_MASK << DRV_OCP_MODE);
	val = BIT(PA_OVP_THD) | (DRV_OCP_MODE_156MA << DRV_OCP_MODE);
	return snd_soc_update_bits(codec, SOC_REG(ANA_PMU3), mask, val);
}

static int adc_low_power_setting(struct snd_soc_codec *codec)
{
	int ret = 0;
	unsigned int mask, val;

	/* PGA + ADC enter low power mode */
	mask = ADPGA_IBIAS_SEL_MASK << ADPGA_IBIAS_SEL;
	/* PGA: 7.5uA, Modulator: 3.75uA */
	val = ADPGA_IBIAS_SEL_PGA_7P5_ADC_3P75 << ADPGA_IBIAS_SEL;
	ret |= snd_soc_update_bits(codec, SOC_REG(ANA_PMU2), mask, val);

	/* Decrease the frequence of ADC clock. */
	mask = AD_CLK_F_MASK << AD_CLK_F;
	val = AD_CLK_F_HALF << AD_CLK_F;
	ret |= snd_soc_update_bits(codec, SOC_REG(ANA_CLK0), mask, val);

	return ret;
}

static void sprd_codec_power_enable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int ret;

	sp_asoc_pr_dbg("%s\n", __func__);

	/* VB will be kept on by headset driver for headset pop noise issue.
	 * Just change the mode of VB in codec driver.
	 */
	ret = regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_NORMAL);
	if (ret < 0)
		pr_err("ERR: %s set VB to normal mode failed!\n", __func__);

	sprd_codec_cfga_clk_enable(codec, 1);
	ret = sprd_codec_ldo_on(sprd_codec);
	if (ret != 0)
		pr_err("ERR:SPRD-CODEC open ldo error %d\n", ret);

	sprd_codec_analog_open(codec);
	sprd_codec_analog_clk_on(codec, 1);

	enable_oxp(codec);

	if (adc_low_power_setting(codec) < 0)
		pr_err("set ADC low power failed!\n");
}

static void sprd_codec_power_disable(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_dbg("%s\n", __func__);

	sprd_codec_analog_clk_on(codec, 0);
	sprd_codec_ldo_off(sprd_codec);
	sprd_codec_cfga_clk_enable(codec, 0);
	/* Set VB to standy mode to save power consumption. */
	regulator_set_mode(sprd_codec->vb, REGULATOR_MODE_STANDBY);
}

static void sprd_codec_adc_clock_input_en(struct snd_soc_codec *codec, int on)
{
	sp_asoc_pr_dbg("%s, on:%d\n", __func__, on);

	if (on) {
		snd_soc_update_bits(codec, SOC_REG(ANA_CLK0),
				    BIT(AD_CLK_EN), BIT(AD_CLK_EN));
		snd_soc_update_bits(codec, SOC_REG(ANA_CLK0),
				    BIT(AD_CLK_RST), BIT(AD_CLK_RST));
		snd_soc_update_bits(codec,
				    SOC_REG(ANA_CLK0), BIT(AD_CLK_RST), 0);
	} else {
		snd_soc_update_bits(codec,
				    SOC_REG(ANA_CLK0), BIT(AD_CLK_EN), 0);
		snd_soc_update_bits(codec, SOC_REG(ANA_CLK0),
				    BIT(AD_CLK_RST), BIT(AD_CLK_RST));
	}
}

static DEFINE_SPINLOCK(aud_en_lock);
static unsigned int aud_eb_set_cnt;

static int codec_aud_en_set(void)
{
	int ret = 0;

	spin_lock(&aud_en_lock);
	if (aud_eb_set_cnt == 0) {
		ret = aud_aon_bit_raw_set(REG_AON_APB_APB_EB0,
					  BIT_AON_APB_AUD_EB);
		if (ret) {
			pr_err("%s failed!\n", __func__);
			spin_unlock(&aud_en_lock);
			return -1;
		}
		xtlbuf1_eb_set();
	}
	aud_eb_set_cnt++;
	spin_unlock(&aud_en_lock);

	sp_asoc_pr_dbg("%s aud_eb_set_cnt=%d\n", __func__, aud_eb_set_cnt);

	return ret;
}

static int codec_aud_en_clr(void)
{
	int ret = 0;

	spin_lock(&aud_en_lock);
	if (aud_eb_set_cnt == 1) {
		ret = aud_aon_bit_raw_clear(REG_AON_APB_APB_EB0,
					    BIT_AON_APB_AUD_EB);
		if (ret) {
			pr_err("%s failed!\n", __func__);
			spin_unlock(&aud_en_lock);
			return -1;
		}
		xtlbuf1_eb_clr();
	}
	if (aud_eb_set_cnt == 0) {
		pr_warn("%s, not enable yet!\n", __func__);
		spin_unlock(&aud_en_lock);
		return 0;
	}
	aud_eb_set_cnt--;
	spin_unlock(&aud_en_lock);

	sp_asoc_pr_dbg("%s aud_eb_set_cnt=%d\n", __func__, aud_eb_set_cnt);

	return ret;
}

static int digital_power_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = codec_aud_en_set();
		if (ret < 0) {
			pr_err("%s, enable audio top failed!\n", __func__);
			return -1;
		}
		arch_audio_codec_audif_enable(0);
		arch_audio_codec_digital_enable();
		arch_audio_codec_digital_reset();
		sprd_codec_digital_open(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * maybe ADC module use it, so we cann't close it
		 * arch_audio_codec_digital_disable();
		 */
		arch_audio_codec_audif_disable();
		codec_aud_en_clr();
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

static int audio_drv_clk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	unsigned int msk;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	msk = BIT(DCL_EN) | BIT(DRV_SOFT_EN);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL0), msk, msk);
		msk = BIT(DCL_RST);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL0), msk, msk);
		udelay(10);
		snd_soc_update_bits(codec, SOC_REG(ANA_DCL0), msk, 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int audio_adc_clock_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sprd_codec_adc_clock_input_en(codec, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sprd_codec_adc_clock_input_en(codec, 0);
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

static int mixer_set_mixer(struct snd_soc_codec *codec, int id, int lr,
			   int try_on, int need_set)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int reg = GET_MIXER_ID(ID_FUN(id, lr));
	struct sprd_codec_mixer *mixer = &sprd_codec->mixer[reg];

	if (try_on) {
		mixer->set = mixer_set_func[reg];
		if (need_set)
			return mixer->set(codec, mixer->on);
	} else {
		mixer_set_func[reg] (codec, 0);
		mixer->set = 0;
	}

	return 0;
}

static inline int mixer_setting(struct snd_soc_codec *codec, int start,
				int end, int lr, int try_on, int need_set)
{
	int id;

	for (id = start; id < end; id++)
		mixer_set_mixer(codec, id, lr, try_on, need_set);

	return 0;
}

static inline int mixer_setting_one(struct snd_soc_codec *codec, int id,
				    int try_on, int need_set)
{
	int lr = id & 0x1;

	id = (id >> 1) + SPRD_CODEC_MIXER_START;
	return mixer_setting(codec, id, id + 1, lr, try_on, need_set);
}

static inline void codec_print_oxp_stat(struct snd_soc_codec *codec)
{
	unsigned int val;

	val = snd_soc_read(codec, ANA_STS7);
	pr_debug("ANA_STS7: %#x\n", val);
	if (val & BIT(PA_OVP_FLAG))
		pr_err("ERR: speaker PA OVP! ANA_STS7: %#x\n", val);
	if (val & BIT(PA_OTP_FLAG))
		pr_err("ERR: speaker PA OTP! ANA_STS7: %#x\n", val);
	if ((val >> DRV_OCP_FLAG_SPK) & DRV_OCP_FLAG_MASK)
		pr_err("ERR: speaker PA OCP! ANA_STS7: %#x\n", val);
	if ((val >> DRV_OCP_FLAG_HPRCV) & DRV_OCP_FLAG_MASK)
		pr_err("ERR: headphone or receiver OCP! ANA_STS7: %#x\n", val);
}

#define to_sprd_codec(x) \
	container_of((x), struct sprd_codec_priv, ap_irq_delayed_work)
#define to_delay_worker(x) container_of((x), struct delayed_work, work)
static void codec_ap_irq_delay_worker(struct work_struct *work)
{
	struct sprd_codec_priv *sprd_codec =
		to_sprd_codec(to_delay_worker(work));
	struct snd_soc_codec *codec = sprd_codec->codec;
	unsigned int mask, val;

	mask = BIT(PA_OVP_FLAG) | BIT(PA_OTP_FLAG) |
		(DRV_OCP_FLAG_MASK << DRV_OCP_FLAG_SPK) |
		(DRV_OCP_FLAG_MASK << DRV_OCP_FLAG_HPRCV);
	val = snd_soc_read(codec, ANA_STS7);
	pr_debug("%s, ANA_STS7: %#x\n", __func__, val);
	if (!(val & mask)) {
		sprd_codec_irq_oxp_enable(codec);
	} else {
		/* When OVP or OCP occurred, the chip will process
		 * the case by itself. So here just print some information.
		 */
		codec_print_oxp_stat(codec);
		schedule_delayed_work(&sprd_codec->ap_irq_delayed_work,
				      msecs_to_jiffies(2000));
	}
}

static irqreturn_t sprd_codec_ap_irq(int irq, void *dev_id)
{
	unsigned int mask, val = 0;
	struct sprd_codec_priv *sprd_codec = dev_id;
	struct snd_soc_codec *codec = sprd_codec->codec;

	mask = (snd_soc_read(codec, SOC_REG(AUD_CFGA_RD_STS)) >> AUD_IRQ_MSK);
	val = snd_soc_read(codec, SOC_REG(ANA_STS7));
	pr_info("IRQ Mask = %#x, ANA_STS7: %#x\n", mask, val);

	/* When OVP or OCP occurred, the chip will process the case by itself.
	 * So here just print some information.
	 */
	val = 0;
	if (BIT(OVP_IRQ) & mask) {
		pr_warn("%s, PA OVP!\n", __func__);
		val |= BIT(OVP_IRQ);
	}
	if (BIT(OTP_IRQ) & mask) {
		pr_warn("%s, PA OTP!\n", __func__);
		val |= BIT(OTP_IRQ);
	}
	if (BIT(PA_OCP_IRQ) & mask) {
		pr_warn("%s, PA OCP!\n", __func__);
		val |= BIT(PA_OCP_IRQ);
	}
	if (BIT(HP_EAR_OCP_IRQ) & mask) {
		pr_warn("%s, HP or EAR OCP!\n", __func__);
		val |= BIT(HP_EAR_OCP_IRQ);
	}

	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_INT_MODULE_CTRL),
			    val << AUD_A_INT_EN, 0);

	schedule_delayed_work(&sprd_codec->ap_irq_delayed_work,
			      msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

#define WAIT_CNT_CHG_CAP 30
static int sprd_codec_charge_ext_cap(struct snd_soc_codec *codec)
{
	int val;
	int ret = 0;
	int cnt = 20;

	/* Check the status of DC-CAL*/
	do {
		val = snd_soc_read(codec, SOC_REG(ANA_STS2));
		pr_debug("ANA_STS2: %#x\n", val);
		if (val & BIT(HP_DPOP_DVLD))
			break;
		sprd_codec_wait(5);
	} while (--cnt);
	if (!cnt) {
		pr_err("%s, waiting for DCCAL finish timeout!\n", __func__);
		return -1;
	}
	pr_debug("Check DC-CAL status wait cnt: %d\n", cnt);

	val = BIT(DEPOP_CHG_EN) | BIT(PLUGIN) | BIT(DEPOP_EN);
	snd_soc_update_bits(codec, SOC_REG(ANA_STS2), val, val);
	sprd_codec_wait(2);

	/* Start charging */
	snd_soc_update_bits(codec, SOC_REG(ANA_STS2), BIT(DEPOP_CHG_START),
			    BIT(DEPOP_CHG_START));

	if (headset_get_plug_state() != 1)
		return 0;

	/* Waiting for charging finish. only headset plug in need to waiting */
	cnt = WAIT_CNT_CHG_CAP;
	do {
		val = snd_soc_read(codec, SOC_REG(ANA_STS2));
		pr_debug("Check charging status, ANA_STS2: %#x\n", val);
		if ((val & BIT(DEPOP_CHG_STS)) && (val & BIT(RCV_DPOP_DVLD)))
			break;
		sprd_codec_wait(20);
	} while (--cnt);
	if (!cnt) {
		ret = -1;
		pr_err("%s, waiting for charging finish timeout!\n", __func__);
	}
	pr_debug("Charging ext cap takes about %dms. cnt: %d\n",
		 (WAIT_CNT_CHG_CAP - cnt) * 20, cnt);

	return ret;
}

static int hp_depop_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	if (on)
		return sprd_codec_charge_ext_cap(codec);

	/* When off, clear CHG_START. */
	return snd_soc_update_bits(codec, SOC_REG(ANA_STS2),
				   BIT(DEPOP_CHG_START) | BIT(DEPOP_CHG_EN), 0);
}

static int hp_drv_path_switch_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	unsigned int val;
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	int cnt = 10;
	int bit = (w->shift == HPL_EN) ? BIT(HPL_RDAC_STS) : BIT(HPR_RDAC_STS);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	mixer_setting(codec, SPRD_CODEC_HP_DACL,
		      SPRD_CODEC_HP_DAC_MIXER_MAX, SPRD_CODEC_LEFT,
		       on, 1);
	mixer_setting(codec, SPRD_CODEC_HP_DACL,
		      SPRD_CODEC_HP_DAC_MIXER_MAX, SPRD_CODEC_RIGHT,
		       on, 1);

	mixer_setting(codec, SPRD_CODEC_HP_ADCL,
		      SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_LEFT,
		       on, 0);
	mixer_setting(codec, SPRD_CODEC_HP_ADCL,
		      SPRD_CODEC_HP_MIXER_MAX, SPRD_CODEC_RIGHT,
		       on, 0);

	/* Wait for RDAC status  only headset plug in need to waiting */
	if (headset_get_plug_state() != 1)
		return 0;

	while (--cnt) {
		val = snd_soc_read(codec, SOC_REG(ANA_DCL5));
		pr_debug("ANA_DCL5: %#x\n", val);
		if (on && (val & bit))
			break;
		else if (!on && !(val & bit))
			break;
		sprd_codec_wait(20);
	}
	if (cnt == 0) {
		pr_err("%s Waiting for RDAC status timeout!\n", __func__);
		ret = -1;
	}

	return ret;
}

static int hp_switch_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	unsigned int val, mask, mask_drv;
	bool left = (w->shift == HPL_EN);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	mask_drv = left ? (0x3 << HPL_FLOOP_END) : (0x3 << HPR_FLOOP_END);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mask = (HP_IBCUR3_MASK << HP_IBCUR3) |
			(DRV_PM_SEL_MASK << DRV_PM_SEL);
		val = (HP_IBCUR3_10UA << HP_IBCUR3) |
			(DRV_PM_SEL_3 << DRV_PM_SEL);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU2), mask, val);

		/* Hp driver dummy on */
		val = left ? BIT(HPL_FLOOPEN) : BIT(HPR_FLOOPEN);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask_drv, val);
		sprd_codec_wait(1);
		/* Hp driver on */
		val = left ? BIT(HPL_FLOOP_END) : BIT(HPR_FLOOP_END);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask_drv, val);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		val = left ? BIT(HPL_FLOOPEN) : BIT(HPR_FLOOPEN);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask_drv, val);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask_drv, 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int spk_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mixer_setting(codec, SPRD_CODEC_SPK_DACL,
			      SPRD_CODEC_SPK_DAC_MIXER_MAX, SPRD_CODEC_LEFT,
			   1, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mixer_setting(codec, SPRD_CODEC_SPK_DACL,
			      SPRD_CODEC_SPK_DAC_MIXER_MAX, SPRD_CODEC_LEFT,
			   0, 1);
		break;
	default:
		break;
	}

	return 0;
}

static int ear_drv_path_switch_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mixer_setting(codec, SPRD_CODEC_EAR_DACL,
			      SPRD_CODEC_EAR_MIXER_MAX, SPRD_CODEC_LEFT,
			   1, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mixer_setting(codec, SPRD_CODEC_EAR_DACL,
			      SPRD_CODEC_EAR_MIXER_MAX, SPRD_CODEC_LEFT,
			   0, 1);
		sprd_codec_wait(1);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int ear_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	unsigned int mask, val;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mask = (HP_IBCUR3_MASK << HP_IBCUR3) |
			(DRV_PM_SEL_MASK << DRV_PM_SEL);
		val = (HP_IBCUR3_5UA << HP_IBCUR3) |
			(DRV_PM_SEL_1 << DRV_PM_SEL);
		snd_soc_update_bits(codec, SOC_REG(ANA_PMU2), mask, val);

		/* Ear driver dummy on */
		mask = BIT(RCV_FLOOPEN) | BIT(RCV_FLOOP_END);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2),
				    mask, BIT(RCV_FLOOPEN));
		sprd_codec_wait(1);
		/* Ear driver on */
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2),
				    mask, BIT(RCV_FLOOP_END));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mask = BIT(RCV_FLOOPEN) | BIT(RCV_FLOOP_END);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2),
				    mask, BIT(RCV_FLOOPEN));
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask, 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int adcpgal_set(struct snd_soc_codec *codec, int on)
{
	int value = on ? BIT(ADPGAL_EN) : 0;

	return snd_soc_update_bits(codec,
		SOC_REG(ANA_CDC0), BIT(ADPGAL_EN), value);
}

static int adcpgar_set(struct snd_soc_codec *codec, int on)
{
	int value = on ? BIT(ADPGAR_EN) : 0;

	return snd_soc_update_bits(codec,
		SOC_REG(ANA_CDC0), BIT(ADPGAR_EN), value);
}

static int adc_switch_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int is_right = (w->shift == SPRD_CODEC_RIGHT);
	int on = (!!SND_SOC_DAPM_EVENT_ON(event));

	sp_asoc_pr_dbg("%s Event is %s, is_right: %d\n",
		       __func__, get_event_name(event), is_right);

	if (is_right) {
		adcpgar_set(codec, on);
		mixer_setting(codec, SPRD_CODEC_MIXER_START,
			      SPRD_CODEC_ADC_MIXER_MAX,
			      SPRD_CODEC_RIGHT, on, 1);
	} else {
		adcpgal_set(codec, on);
		mixer_setting(codec, SPRD_CODEC_MIXER_START,
			      SPRD_CODEC_ADC_MIXER_MAX,
			      SPRD_CODEC_LEFT, on, 1);
	}

	return 0;
}

static int pga_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = GET_PGA_ID(FUN_REG(w->reg));
	struct sprd_codec_pga_op *pga = &sprd_codec->pga[id];
	int ret = 0;
	int min = sprd_codec_pga_cfg[id].min;

	sp_asoc_pr_dbg("%s Set %s(%d) Event is %s\n", __func__,
		       sprd_codec_pga_debug_str[id], pga->pgaval,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		pga->set = sprd_codec_pga_cfg[id].set;
		ret = pga->set(codec, pga->pgaval);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pga->set = 0;
		if (min >= 0)
			ret = sprd_codec_pga_cfg[id].set(codec, min);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int switch_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = GET_SWITCH_ID(FUN_REG(w->reg));
	int ret = 0;
	int min = 0;
	struct sprd_codec_switch_op *switcher = &sprd_codec->switcher[id];

	sp_asoc_pr_info("%s Switch %s(%s) Event is %s\n", __func__,
			switch_name[id], switcher->on ? "on" : "off",
		    get_event_name(event));
	min = sprd_codec_switch_cfg[id].min;
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		switcher->set = sprd_codec_switch_cfg[id].set;
		ret = switcher->set(codec, switcher->on);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		switcher->set = 0;
		if (min >= 0)
			ret = sprd_codec_switch_cfg[id].set(codec, min);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int adcpgar_byp_set(struct snd_soc_codec *codec, int value)
{
	int mask = ADPGAR_BYP_MASK << ADPGAR_BYP;

	return snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), mask,
				    value << ADPGAR_BYP);
}

static int adcl_rst_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	int mask;

	sp_asoc_pr_dbg("%s  Event is %s\n", __func__,  get_event_name(event));

	mask = BIT(ADL_RST);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), mask, mask);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), mask, 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int adcr_rst_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;
	int mask;

	sp_asoc_pr_dbg("%s  Event is %s\n", __func__,  get_event_name(event));

	mask = BIT(ADR_RST);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), mask, mask);
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC0), mask, 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int adcpgar_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s  Event is %s\n", __func__,  get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		adcpgar_byp_set(codec, ADPGAR_BYP_HDST2ADC);
		sprd_codec_wait(100);
		adcpgar_byp_set(codec, ADPGAR_BYP_NORMAL);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int ana_loop_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int id = GET_MIXER_ID(FUN_REG(w->reg));
	struct sprd_codec_mixer *mixer;
	int ret = 0;
	static int s_need_wait = 1;

	sp_asoc_pr_dbg("Entering %s event is %s\n", __func__,
		       get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/*
		 * NOTES: reduce linein pop noise must open
		 * ADCL/R ->HP/SPK MIXER AFTER delay 250ms for
		 * both ADCL/ADCR switch complete.
		 */
		if (s_need_wait == 1) {
			/*
			 * NOTES: reduce linein pop noise must delay 250ms
			 * after linein mixer switch on.
			 * actually this function perform after
			 * adc_switch_event function for
			 * both ADCL/ADCR switch complete.
			 */
			sprd_codec_wait(250);
			s_need_wait++;
			sp_asoc_pr_dbg("ADC Switch ON delay\n");
		}
		mixer = &sprd_codec->mixer[id];
		if (mixer->set)
			mixer->set(codec, mixer->on);
		break;
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_POST_PMD:
		s_need_wait = 1;
		mixer = &sprd_codec->mixer[id];
		if (mixer->set)
			mixer->set(codec, mixer->on);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static int mic_bias_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int reg = FUN_REG(w->reg);
	int ret = 0;
	int on = !!SND_SOC_DAPM_EVENT_ON(event);
	struct regulator **regu;

	sp_asoc_pr_dbg("%s %s Event is %s\n", __func__,
		       mic_bias_name[GET_MIC_BIAS_ID(reg)],
		       get_event_name(event));

	switch (reg) {
	case SPRD_CODEC_MIC_BIAS:
		regu = &sprd_codec->main_mic;
		break;
	case SPRD_CODEC_HEADMIC_BIAS:
		regu = &sprd_codec->head_mic;
		break;
	default:
		WARN_ON(1);
		return -EINVAL;
	}

	ret = sprd_codec_regulator_set(regu, on);

	return ret;
}

static int mixer_get(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct sprd_codec_priv *sprd_codec =
		snd_soc_component_get_drvdata(dapm->component);
	int id = GET_MIXER_ID(FUN_REG(mc->reg));

	ucontrol->value.integer.value[0] = sprd_codec->mixer[id].on;

	return 0;
}

static int mixer_need_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol, bool need_set)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct sprd_codec_priv *sprd_codec =
		snd_soc_component_get_drvdata(dapm->component);
	struct snd_soc_codec *codec = sprd_codec->codec;
	int id = GET_MIXER_ID(FUN_REG(mc->reg));
	struct sprd_codec_mixer *mixer = &sprd_codec->mixer[id];
	int ret = 0;

	if (mixer->on == ucontrol->value.integer.value[0])
		return 0;

	sp_asoc_pr_info("Set %s Switch %s\n", sprd_codec_mixer_debug_str[id],
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	/* notice the sequence */
	snd_soc_dapm_put_volsw(kcontrol, ucontrol);

	/*
	 * update reg: must be set after
	 * snd_soc_dapm_put_enum_double->change =
	 * snd_soc_test_bits(widget->codec, e->reg, mask, val);
	 */
	if (mixer->set && need_set)
		ret = mixer->set(codec, mixer->on);

	return ret;
}

static int mixer_set_mem(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	return mixer_need_set(kcontrol, ucontrol, 0);
}

static int mixer_set(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	return mixer_need_set(kcontrol, ucontrol, 1);
}

static inline int adie_audio_codec_adie_loop_clk_en(
		struct snd_soc_codec *codec, int on)
{
	int ret = 0;

	if (on)
		ret = snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN),
					  BIT(CLK_AUD_LOOP_EN), BIT(CLK_AUD_LOOP_EN));
	else
		ret = snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN),
					  BIT(CLK_AUD_LOOP_EN), 0);

	return ret;
}

static int adie_loop_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	unsigned int id = GET_ADC_LOOP_ID(FUN_REG(w->reg));
	int ret = 0;

	sp_asoc_pr_info("%s, %s, Event is %s\n",
			__func__, adc_loop_name[id], get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* enable loop clock */
		ret = adie_audio_codec_adie_loop_clk_en(codec, 1);
		snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
				    BIT(AUDIO_ADIE_LOOP_EN), 1 << AUDIO_ADIE_LOOP_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* disable loop clock */
		ret = adie_audio_codec_adie_loop_clk_en(codec, 0);
		snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
				    BIT(AUDIO_ADIE_LOOP_EN), 0 << AUDIO_ADIE_LOOP_EN);
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
	unsigned int id = GET_ADC_LOOP_ID(FUN_REG(w->reg));
	int ret = 0;

	sp_asoc_pr_info("%s, %s, Event is %s\n",
			__func__, adc_loop_name[id], get_event_name(event));

	if (FUN_REG(w->reg) == SPRD_CODEC_ADC_DAC_DIGITAL_LOOP) {
		sprd_codec_set_ad_sample_rate(codec, 48000, 0x0F, 0);
	} else if (FUN_REG(w->reg) ==
		SPRD_CODEC_ADC1_DAC_DIGITAL_LOOP) {
		sprd_codec_set_ad_sample_rate(codec, 48000, 0xF0, 4);
	} else {
		sp_asoc_pr_info("%s, wrong id is %d\n", __func__, id);
		return -EINVAL;
	}
	sprd_codec_set_sample_rate(codec, 48000, 0x0F, 0);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
				    BIT(LOOP_ADC_PATH_SEL),
			((id - 2) << LOOP_ADC_PATH_SEL));
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
				    BIT(AUD_LOOP_TEST), 1 << AUD_LOOP_TEST);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(AUD_LOOP_CTL),
				    BIT(AUD_LOOP_TEST), 0 << AUD_LOOP_TEST);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_codec_adie_loop_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct sprd_codec_priv *sprd_codec =
		snd_soc_component_get_drvdata(dapm->component);
	unsigned int id = GET_ADC_LOOP_ID(FUN_REG(mc->reg));
	int ret = 0;

	ret = !!(ucontrol->value.integer.value[0]);
	if (ret == sprd_codec->ad_da_loop_en[id])
		return ret;
	sp_asoc_pr_info("%s, %s, set %d\n", __func__,
			adc_loop_name[id], (int)ucontrol->value.integer.value[0]);
	/* notice the sequence */
	snd_soc_dapm_put_volsw(kcontrol, ucontrol);
	sprd_codec->ad_da_loop_en[id] = ret;

	return ret;
}

static int sprd_codec_adie_loop_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct sprd_codec_priv *sprd_codec =
		snd_soc_component_get_drvdata(dapm->component);
	unsigned int id = GET_ADC_LOOP_ID(FUN_REG(mc->reg));

	ucontrol->value.integer.value[0] = !!sprd_codec->ad_da_loop_en[id];

	return 0;
}

static int sprd_codec_digital_loop_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct sprd_codec_priv *sprd_codec =
		snd_soc_component_get_drvdata(dapm->component);
	unsigned int id = GET_ADC_LOOP_ID(FUN_REG(mc->reg));
	int ret = 0;

	ret = !!(ucontrol->value.integer.value[0]);
	if (ret == sprd_codec->ad_da_loop_en[id])
		return ret;
	sp_asoc_pr_info("%s, %s, set %d\n", __func__,
			adc_loop_name[id], (int)ucontrol->value.integer.value[0]);

	/* notice the sequence */
	snd_soc_dapm_put_volsw(kcontrol, ucontrol);
	sprd_codec->ad_da_loop_en[id] = ret;

	return ret;
}

static int sprd_codec_digital_loop_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct sprd_codec_priv *sprd_codec =
		snd_soc_component_get_drvdata(dapm->component);
	unsigned int id = GET_ADC_LOOP_ID(FUN_REG(mc->reg));

	ucontrol->value.integer.value[0] = !!sprd_codec->ad_da_loop_en[id];

	return 0;
}

#define SPRD_CODEC_MIXER(xname, xreg)\
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, mixer_get, mixer_set)

/*
 * Just for LINE IN path, mixer_set not really set mixer
 * (ADCL/R -> HP/SPK L/R) here but setting in
 * ana_loop_event, just remeber state here
 */
#define SPRD_CODEC_MIXER_NOSET(xname, xreg) SOC_SINGLE_EXT( \
	xname, FUN_REG(xreg), 0, 1, 0, mixer_get, mixer_set_mem)

/* ADCL Mixer */
static const struct snd_kcontrol_new adcl_mixer_controls[] = {
	SPRD_CODEC_MIXER("MainMICADCL Switch",
			 ID_FUN(SPRD_CODEC_MAIN_MIC, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("AuxMICADCL Switch",
			 ID_FUN(SPRD_CODEC_AUX_MIC, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("HPMICADCL Switch",
			 ID_FUN(SPRD_CODEC_HP_MIC, SPRD_CODEC_LEFT)),
};

/* ADCR Mixer */
static const struct snd_kcontrol_new adcr_mixer_controls[] = {
	SPRD_CODEC_MIXER("MainMICADCR Switch",
			 ID_FUN(SPRD_CODEC_MAIN_MIC, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("AuxMICADCR Switch",
			 ID_FUN(SPRD_CODEC_AUX_MIC, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("HPMICADCR Switch",
			 ID_FUN(SPRD_CODEC_HP_MIC, SPRD_CODEC_RIGHT)),
};

/* HPL Mixer */
static const struct snd_kcontrol_new hpl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_DACL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("DACRHPL Switch",
			 ID_FUN(SPRD_CODEC_HP_DACR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCLHPL Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCRHPL Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_LEFT)),
};

/* HPR Mixer */
static const struct snd_kcontrol_new hpr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_DACL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("DACRHPR Switch",
			 ID_FUN(SPRD_CODEC_HP_DACR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCLHPR Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCRHPR Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_RIGHT)),
};

/* SPKL Mixer */
/* TODO: adjust speaker mixer. */
static const struct snd_kcontrol_new spkl_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER("DACRSPKL Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCLSPKL Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_MIXER_NOSET("ADCRSPKL Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_LEFT)),
};

/* SPKR Mixer */
/* TODO: should be removed. */
static const struct snd_kcontrol_new spkr_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER("DACRSPKR Switch",
			 ID_FUN(SPRD_CODEC_SPK_DACR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCLSPKR Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_MIXER_NOSET("ADCRSPKR Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_RIGHT)),
};

static const struct snd_kcontrol_new ear_mixer_controls[] = {
	SPRD_CODEC_MIXER("DACLEAR Switch",
			 ID_FUN(SPRD_CODEC_EAR_DACL, SPRD_CODEC_LEFT)),
};

static const struct snd_kcontrol_new sprd_codec_loop_controls[] = {
	SOC_SINGLE_EXT("switch", FUN_REG(SPRD_CODEC_ADC_DAC_ADIE_LOOP), 0,
		       1, 0, sprd_codec_adie_loop_get, sprd_codec_adie_loop_put),
	SOC_SINGLE_EXT("switch", FUN_REG(SPRD_CODEC_ADC1_DAC_ADIE_LOOP), 0,
		       1, 0, sprd_codec_adie_loop_get, sprd_codec_adie_loop_put),
	SOC_SINGLE_EXT("switch", FUN_REG(SPRD_CODEC_ADC_DAC_DIGITAL_LOOP), 0,
		       1, 0, sprd_codec_digital_loop_get, sprd_codec_digital_loop_put),
	SOC_SINGLE_EXT("switch", FUN_REG(SPRD_CODEC_ADC1_DAC_DIGITAL_LOOP), 0,
		       1, 0, sprd_codec_digital_loop_get, sprd_codec_digital_loop_put),
};

/* ANA LOOP SWITCH */
#define SPRD_CODEC_LOOP_SWITCH(xname, xreg)\
	SND_SOC_DAPM_PGA_S(xname, SPRD_CODEC_ANA_MIXER_ORDER, FUN_REG(xreg), \
		0, 0, ana_loop_event,\
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)

#ifndef SND_SOC_DAPM_MICBIAS_E
#define SND_SOC_DAPM_MICBIAS_E(wname, wreg, wshift, winvert, wevent, wflags) \
{	.id = snd_soc_dapm_micbias, .name = wname, \
	SND_SOC_DAPM_INIT_REG_VAL(wreg, wshift, winvert), \
	.kcontrol_news = NULL, .num_kcontrols = 0, \
	.event = wevent, .event_flags = wflags}
#endif

static const struct snd_kcontrol_new hp_jack_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new virt_output_switch =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static DEFINE_MUTEX(dac_adc_lock);
static int dac_adc_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	static short ref_cnt[4];
	int index;
	unsigned int mask;

	switch (w->shift) {
	case DAC_EN_L:
		index = 0;
		mask = BIT(DAC_EN_L);
		break;
	case DAC_EN_R:
		index = 1;
		mask = BIT(DAC_EN_R);
		break;
	case ADC_EN_L:
		index = 2;
		mask = BIT(ADC_EN_L);
		break;
	case ADC_EN_R:
		index = 3;
		mask = BIT(ADC_EN_R);
		break;
	default:
		pr_warn("%s '%s', invalid shift(%u).\n", __func__, w->name,
			w->shift);
		return 0;
	}

	sp_asoc_pr_dbg("%s widget: %s, event is %s, ref count[%d]: %d\n",
		       __func__, w->name, get_event_name(event),
		       index, ref_cnt[index]);

	mutex_lock(&dac_adc_lock);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SOC_REG(AUD_TOP_CTL), mask, mask);
		ref_cnt[index]++;
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (ref_cnt[index]-- == 1)
			snd_soc_update_bits(codec, SOC_REG(AUD_TOP_CTL),
					    mask, 0);
		break;
	default:
		pr_warn("%s event(%#x) hasn't been processed!\n", __func__,
			event);
		break;
	}
	mutex_unlock(&dac_adc_lock);

	return 0;
}

#define WAIT_CNT_DCCAL 20
static int codec_hp_dc_cal(struct snd_soc_codec *codec)
{
	int ret = 0;
	unsigned int mask, val;
	int cnt = WAIT_CNT_DCCAL;
	unsigned int adie_chip_id;

	mask = DEPOP_BIAS_SEL_MASK << DEPOP_BIAS_SEL;
	val = DEPOP_BIAS_SEL_2P5UA << DEPOP_BIAS_SEL;
	snd_soc_update_bits(codec, SOC_REG(ANA_STS3), mask, val);

	snd_soc_update_bits(codec, SOC_REG(ANA_DCL0), BIT(DPOP_AUTO_RST), 0);

	if (headset_get_plug_state() == 1) {
		cnt = 10;
		while (!headset_fast_charge_finished() && cnt--)
			sprd_codec_wait(5);
	}
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2),
			    BIT(HPBUF_EN), BIT(HPBUF_EN));
	mask = BIT(CALDC_ENO) | BIT(CALDC_EN);
	snd_soc_write(codec, SOC_REG(ANA_STS2), mask);
	snd_soc_write(codec, SOC_REG(ANA_DCL4), 0xFFFF);
	adie_chip_id = sci_get_ana_chip_id();
	adie_chip_id = (adie_chip_id >> 16) & 0xffff;
	if (adie_chip_id == CHIP_ID_2720)
		snd_soc_write(codec, SOC_REG(ANA_DCL5), 0xF000);
	else
		snd_soc_write(codec, SOC_REG(ANA_DCL5), 0xFA10);
	snd_soc_write(codec, SOC_REG(ANA_DCL6), 0x4CD8);
	snd_soc_write(codec, SOC_REG(ANA_DCL7), 0x2E6C);
	snd_soc_write(codec, SOC_REG(ANA_STS0), 0xA820);
	sprd_codec_wait(2);

	/* Start calibrating */
	snd_soc_update_bits(codec, SOC_REG(ANA_STS2),
			    BIT(CALDC_START), BIT(CALDC_START));

	/* Waiting for DCCAL process finish.
	 * Twice read for anti-glitch.
	 */
	if (headset_get_plug_state() == 1) {
		cnt = WAIT_CNT_DCCAL;
		do {
			val = snd_soc_read(codec, SOC_REG(ANA_STS2));
			pr_debug("1st ANA_STS2: %#x\n", val);
			if ((val & BIT(DCCAL_STS)) && (val & BIT(HP_DPOP_DVLD))) {
				sprd_codec_wait(5);
				val = snd_soc_read(codec, SOC_REG(ANA_STS2));
				pr_debug("2nd ANA_STS2: %#x\n", val);
				if ((val & BIT(DCCAL_STS)) && (val & BIT(HP_DPOP_DVLD)))
					break;
			}
			sprd_codec_wait(15);
		} while (--cnt);
		if (!cnt) {
			ret = -1;
			pr_err("%s, waiting for DCCAL finish timeout!\n", __func__);
		}
		pr_debug("DC-CAL takes about %dms. cnt: %d\n",
		 (WAIT_CNT_DCCAL - cnt) * 15 + (cnt ? 5 : 0), cnt);
	}

	mask = ~0u & ~(BIT(DAS_EN) | BIT(PA_EN));
	snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), mask, 0);

	return ret;
}

static int dc_cal_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		codec_hp_dc_cal(codec);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SOC_REG(ANA_STS2),
				    BIT(CALDC_START), 0);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static const char * const hpl_rcv_mux_texts[] = {
	"EAR", "HPL"
};

static const SOC_ENUM_SINGLE_VIRT_DECL(hpl_rcv_enum, hpl_rcv_mux_texts);
static const struct snd_kcontrol_new hpl_rcv_mux =
	SOC_DAPM_ENUM("HPL EAR Sel", hpl_rcv_enum);

static const struct snd_soc_dapm_widget sprd_codec_dapm_widgets[] = {
	SND_SOC_DAPM_REGULATOR_SUPPLY("BG", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("BIAS", 0, 0),
	SND_SOC_DAPM_SUPPLY_S("Digital Power", 1, SND_SOC_NOPM,
			      0, 0, digital_power_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("Analog Power", 2, SND_SOC_NOPM,
			      0, 0, analog_power_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("DA Clk", 3, SOC_REG(ANA_CLK0),
			      DA_CLK_EN, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DRV Clk", 4, SOC_REG(ANA_CLK0),
			      DRV_CLK_EN, 0,
		audio_drv_clk_event,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("AD IBIAS", 3, SOC_REG(ANA_CDC0),
			      ADPGA_IBIAS_EN, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AD IBUF", 3, SOC_REG(ANA_CDC0),
			      ADPGA_IBUF_EN, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AD Clk", 3, SND_SOC_NOPM, 0, 0,
			      audio_adc_clock_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("Digital DACL Switch", 4, SND_SOC_NOPM,
			   DAC_EN_L, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Digital DACR Switch", 4, SND_SOC_NOPM,
			   DAC_EN_R, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Virt Digital DACL Switch", 4, SND_SOC_NOPM,
			   DAC_EN_L, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Virt Digital DACR Switch", 4, SND_SOC_NOPM,
			   DAC_EN_R, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADie Digital DACL Switch", 5,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		AUDIFA_DACL_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADie Digital DACR Switch", 5,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		AUDIFA_DACR_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DACS Switch", SPRD_CODEC_DA_EN_ORDER,
			   FUN_REG(SPRD_CODEC_DACS), 0, 0,
		switch_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_DEMUX("HPL EAR Sel", SND_SOC_NOPM, 0, 0, &hpl_rcv_mux),
	SND_SOC_DAPM_DEMUX("HPL EAR Sel2", SND_SOC_NOPM, 0, 0, &hpl_rcv_mux),
	SND_SOC_DAPM_PGA("HPL Path", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("EAR Path", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA_S("DACL Switch", SPRD_CODEC_DA_EN_ORDER,
			   FUN_REG(SPRD_CODEC_DACL), 0, 0,
		switch_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("DACR Switch", SPRD_CODEC_DA_EN_ORDER,
			   FUN_REG(SPRD_CODEC_DACR), 0, 0,
		switch_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("DAC Gain", SPRD_CODEC_DA_EN_ORDER,
			   FUN_REG(SPRD_CODEC_PGA_DAC), 0, 0,
		pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_DAC_E("DAC", "Normal-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0,
			   0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Voice", "Voice-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0,
			   0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Offload", "Offload-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0,
			   0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fast", "Fast-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0,
			   0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Voip", "Voip-Playback",
			   FUN_REG(SPRD_CODEC_PLAYBACK), 0,
			   0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC Fm", "Fm-Playback",
			   SND_SOC_NOPM, 0, 0,
		dfm_out_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HPL Drv Path Switch", SPRD_CODEC_MIXER_ORDER,
			   0, HPL_EN, 0,
		hp_drv_path_switch_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HPR Drv Path Switch", SPRD_CODEC_MIXER_ORDER,
			   0, HPR_EN, 0,
		hp_drv_path_switch_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HP DEPOP", SPRD_CODEC_DEPOP_ORDER,
			   SND_SOC_NOPM,
		0, 0, hp_depop_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("DALR DC Offset", SPRD_CODEC_DC_OS_ORDER,
			   SND_SOC_NOPM, 0, 0,
		dalr_dc_os_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HP BUF Switch", SPRD_CODEC_BUF_SWITCH_ORDER,
			   SOC_REG(ANA_CDC2), HPBUF_EN, 0,
		NULL,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HPL Switch", SPRD_CODEC_SWITCH_ORDER,
			   SOC_REG(ANA_CDC2), HPL_EN, 0,
		hp_switch_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR Switch", SPRD_CODEC_SWITCH_ORDER,
			   SOC_REG(ANA_CDC2), HPR_EN, 0,
		hp_switch_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPL Gain", 6, FUN_REG(SPRD_CODEC_PGA_HPL),
			   0, 0, pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("HPR Gain", 6, FUN_REG(SPRD_CODEC_PGA_HPR),
			   0, 0, pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("HPL Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_mixer_controls[0],
		ARRAY_SIZE(hpl_mixer_controls)),
	SND_SOC_DAPM_MIXER("HPR Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_mixer_controls[0],
		ARRAY_SIZE(hpr_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPKL Mixer", SND_SOC_NOPM, 0, 0,
			   &spkl_mixer_controls[0],
		ARRAY_SIZE(spkl_mixer_controls)),
	SND_SOC_DAPM_MIXER("SPKR Mixer", SND_SOC_NOPM, 0, 0,
			   &spkr_mixer_controls[0],
		ARRAY_SIZE(spkr_mixer_controls)),
	SPRD_CODEC_LOOP_SWITCH("ADCLHPL Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRHPL Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCLHPR Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRHPR Loop Switch",
			       ID_FUN(SPRD_CODEC_HP_ADCR, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_LOOP_SWITCH("ADCLSPKL Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRSPKL Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_LEFT)),
	SPRD_CODEC_LOOP_SWITCH("ADCLSPKR Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCL, SPRD_CODEC_RIGHT)),
	SPRD_CODEC_LOOP_SWITCH("ADCRSPKR Loop Switch",
			       ID_FUN(SPRD_CODEC_SPK_ADCR, SPRD_CODEC_RIGHT)),
	SND_SOC_DAPM_PGA_S("DAS DC Offset", SPRD_CODEC_DC_OS_ORDER,
			   SND_SOC_NOPM, 0, 0,
		das_dc_os_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SPKL Switch", SPRD_CODEC_MIXER_ORDER,
			   SND_SOC_NOPM, 0, 0, spk_switch_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("SPKR Switch", 5, SND_SOC_NOPM, 0, 0,
			   spk_switch_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("SPKL Gain", 6, FUN_REG(SPRD_CODEC_PGA_SPKL),
			   0, 0, pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("EAR Mixer", SND_SOC_NOPM, 0, 0,
			   &ear_mixer_controls[0],
		ARRAY_SIZE(ear_mixer_controls)),
	SND_SOC_DAPM_PGA_S("EAR Drv Path Switch", SPRD_CODEC_MIXER_ORDER,
			   SND_SOC_NOPM, 0, 0, ear_drv_path_switch_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("EAR Switch", SPRD_CODEC_SWITCH_ORDER,
			   SOC_REG(ANA_CDC2), RCV_EN, 0,
		ear_switch_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("EAR Gain", 6, FUN_REG(SPRD_CODEC_PGA_EAR),
			   0, 0, pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("ADCL Mixer", SND_SOC_NOPM, 0, 0,
			   &adcl_mixer_controls[0],
		ARRAY_SIZE(adcl_mixer_controls)),
	SND_SOC_DAPM_MIXER("ADCR Mixer", SND_SOC_NOPM, 0, 0,
			   &adcr_mixer_controls[0],
		ARRAY_SIZE(adcr_mixer_controls)),

	SND_SOC_DAPM_PGA_S("Digital ADCL Switch", 4, SND_SOC_NOPM,
			   ADC_EN_L, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Digital ADCR Switch", 4, SND_SOC_NOPM,
			   ADC_EN_R, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Virt Digital ADCL Switch", 4, SND_SOC_NOPM,
			   ADC_EN_L, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Virt Digital ADCR Switch", 4, SND_SOC_NOPM,
			   ADC_EN_R, 0, dac_adc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("ADie Digital ADCL Switch", 2,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		AUDIFA_ADCL_EN, 0,
		adcl_rst_event,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("ADie Digital ADCR Switch", 2,
			   SOC_REG(AUD_CFGA_LP_MODULE_CTRL),
		AUDIFA_ADCR_EN, 0,
		adcr_rst_event,
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("ADCL Switch", 1, SOC_REG(ANA_CDC0),
			   ADL_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADCR Switch", 1, SOC_REG(ANA_CDC0), ADR_EN, 0,
			   adcpgar_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("ADCL PGA", SND_SOC_NOPM, SPRD_CODEC_LEFT,
			   0, NULL,  0, adc_switch_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ADCR PGA", SND_SOC_NOPM, SPRD_CODEC_RIGHT,
			   0, NULL, 0, adc_switch_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADCL Mute", 3, FUN_REG(SPRD_CODEC_PGA_ADCL),
			   0, 0, pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("ADCR Mute", 3, FUN_REG(SPRD_CODEC_PGA_ADCR),
			   0, 0, pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "Normal-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
		0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC Voip", "Voip-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC Voice", "Voice-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE),
			   0, 0,
			   chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E("Mic Bias", FUN_REG(SPRD_CODEC_MIC_BIAS),
			       0, 0, mic_bias_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MICBIAS_E("HeadMic Bias", FUN_REG(SPRD_CODEC_HEADMIC_BIAS),
			       0, 0, mic_bias_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("Digital ADC1L Switch", 5, SOC_REG(AUD_TOP_CTL),
			   ADC1_EN_L, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Digital ADC1R Switch", 5, SOC_REG(AUD_TOP_CTL),
			   ADC1_EN_R, 0, NULL, 0),
	SND_SOC_DAPM_ADC_E("ADC1", "Ext-Voice-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE1), 0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1 Ext", "Ext-Capture",
			   FUN_REG(SPRD_CODEC_CAPTRUE1), 0, 0, chan_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	/* add DMIC */
	SND_SOC_DAPM_PGA_S("DMIC Switch", 3, SOC_REG(AUD_DMIC_CTL),
			   ADC_DMIC_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DMIC1 Switch", 3, SOC_REG(AUD_DMIC_CTL),
			   ADC1_DMIC1_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Spk PA Switch",
			   SPRD_CODEC_PA_ORDER, SND_SOC_NOPM,
		0, 0, spk_pa_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("ADC-DAC Adie Loop",
			    FUN_REG(SPRD_CODEC_ADC_DAC_ADIE_LOOP),
		0, 0, &sprd_codec_loop_controls[0]),
	SND_SOC_DAPM_SWITCH("ADC1-DAC Adie Loop",
			    FUN_REG(SPRD_CODEC_ADC1_DAC_ADIE_LOOP),
		0, 0, &sprd_codec_loop_controls[1]),
	SND_SOC_DAPM_SWITCH("ADC-DAC Digital Loop",
			    FUN_REG(SPRD_CODEC_ADC_DAC_DIGITAL_LOOP),
		0, 0, &sprd_codec_loop_controls[2]),
	SND_SOC_DAPM_SWITCH("ADC1-DAC Digital Loop",
			    FUN_REG(SPRD_CODEC_ADC1_DAC_DIGITAL_LOOP),
		0, 0, &sprd_codec_loop_controls[3]),

	SND_SOC_DAPM_PGA_S("ADC-DAC Adie Loop post",
			   SPRD_CODEC_ANA_MIXER_ORDER,
		FUN_REG(SPRD_CODEC_ADC_DAC_ADIE_LOOP),
		0, 0, adie_loop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADC1-DAC Adie Loop post",
			   SPRD_CODEC_ANA_MIXER_ORDER,
		FUN_REG(SPRD_CODEC_ADC1_DAC_ADIE_LOOP),
		0, 0, adie_loop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADC-DAC Digital Loop post",
			   SPRD_CODEC_ANA_MIXER_ORDER,
		FUN_REG(SPRD_CODEC_ADC_DAC_DIGITAL_LOOP),
		0, 0, digital_loop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("ADC1-DAC Digital Loop post",
			   SPRD_CODEC_ANA_MIXER_ORDER,
		FUN_REG(SPRD_CODEC_ADC1_DAC_DIGITAL_LOOP),
		0, 0, digital_loop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("DMIC"),
	SND_SOC_DAPM_INPUT("DMIC1"),
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_OUTPUT("HP PA"),
	SND_SOC_DAPM_OUTPUT("Spk PA"),
	SND_SOC_DAPM_OUTPUT("Spk2 PA"),

	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_INPUT("AUXMIC"),
	SND_SOC_DAPM_INPUT("HPMIC"),

	/* In sc2721, no such outputs. However, define them here for mute
	 * function, like "Speaker Mute" and "HeadPhone Mute" in the
	 * machine driver. When "Speaker Mute" set to 1, widget
	 * "Speaker Function" will be set to 0, then the part of the codec path
	 *	"ADie Digital DACL Switch" -> ... -> "inter Spk PA"
	 * will be shut down. So speaker PA will be switched off, in this way,
	 * 1. speaker muted; 2. save power(because speaker PA is off).
	 * Nevertheless, I don't like it.
	 */
	SND_SOC_DAPM_OUTPUT("Virt Output Pin"),
	SND_SOC_DAPM_SWITCH("Virt Output", SND_SOC_NOPM,
			    0, 0, &virt_output_switch),

	/* Virtual widgets for headphone dc-calibration */
	SND_SOC_DAPM_SUPPLY_S("DCCAL", 5, SND_SOC_NOPM,
			      0, 0, dc_cal_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Virtual widget for headphone DA path control. If headphone
	 * is removed, switch off it, else switch on it.
	 */
	SND_SOC_DAPM_SWITCH("Virt HP Jack", SND_SOC_NOPM,
			    0, 0, &hp_jack_switch),

	/* Virtual widgets for speaker PA short status check. */
	SND_SOC_DAPM_SUPPLY_S("PA Short Check", 3, SND_SOC_NOPM,
			      0, 0, pa_short_check_event,
		SND_SOC_DAPM_PRE_PMU),
};

/* sprd_codec supported interconnection */
static const struct snd_soc_dapm_route sprd_codec_intercon[] = {
	/* Power */
	{"Analog Power", NULL, "BG"},
	{"Analog Power", NULL, "BIAS"},
	{"DA Clk", NULL, "Analog Power"},
	{"DA Clk", NULL, "Digital Power"},
	{"DAC", NULL, "DA Clk"},
	{"DAC Voice", NULL, "DA Clk"},
	{"DAC Offload", NULL, "DA Clk"},
	{"DAC Fast", NULL, "DA Clk"},
	{"DAC Fm", NULL, "DA Clk"},
	{"DAC Voip", NULL, "DA Clk"},

	{"AD IBIAS", NULL, "Analog Power"},
	{"AD IBUF", NULL, "Analog Power"},
	{"AD Clk", NULL, "Digital Power"},
	{"AD Clk", NULL, "AD IBIAS"},
	{"AD Clk", NULL, "AD IBUF"},
	{"ADC", NULL, "AD Clk"},
	{"ADC Voice", NULL, "AD Clk"},
	{"ADC Voip", NULL, "AD Clk"},

	{"ADC1", NULL, "AD Clk"},
	{"ADC1 Ext", NULL, "AD Clk"},

	{"ADCL PGA", NULL, "AD IBUF"},
	{"ADCR PGA", NULL, "AD IBUF"},

	{"HPL Switch", NULL, "DRV Clk"},
	{"HPR Switch", NULL, "DRV Clk"},
	{"SPKL Switch", NULL, "DRV Clk"},
	{"SPKR Switch", NULL, "DRV Clk"},
	{"EAR Switch", NULL, "DRV Clk"},

	/* Playback */
	{"Digital DACL Switch", NULL, "DAC Fm"},
	{"Digital DACR Switch", NULL, "DAC Fm"},
	{"Digital DACL Switch", NULL, "DAC"},
	{"Digital DACR Switch", NULL, "DAC"},
	{"Digital DACL Switch", NULL, "DAC Voice"},
	{"Digital DACR Switch", NULL, "DAC Voice"},
	{"Digital DACL Switch", NULL, "DAC Offload"},
	{"Digital DACR Switch", NULL, "DAC Offload"},
	{"Digital DACL Switch", NULL, "DAC Fast"},
	{"Digital DACR Switch", NULL, "DAC Fast"},
	{"Digital DACL Switch", NULL, "DAC Voip"},
	{"Digital DACR Switch", NULL, "DAC Voip"},
	{"ADie Digital DACL Switch", NULL, "Digital DACL Switch"},
	{"ADie Digital DACR Switch", NULL, "Digital DACR Switch"},
	{"Virt Output", "Switch", "Digital DACL Switch"},
	{"Virt Output", "Switch", "Digital DACR Switch"},
	{"Virt Output Pin", NULL, "Virt Output"},
	{"DAC Gain", NULL, "ADie Digital DACL Switch"},
	{"DAC Gain", NULL, "ADie Digital DACR Switch"},
	/*
	 * TODO: add DACS mixer to mix "ADie Digital DACL Switch" and
	 *"ADie Digital DACR Switch"
	 */
	{"DAC Gain", NULL, "ADie Digital DACL Switch"},
	{"DAC Gain", NULL, "ADie Digital DACR Switch"},
	{"DACS Switch", NULL, "DAC Gain"},
	{"DACL Switch", NULL, "DAC Gain"},
	{"DACR Switch", NULL, "DAC Gain"},
	{"HPL EAR Sel", NULL, "DACL Switch"},
	{"HPL Path", "HPL", "HPL EAR Sel"},
	{"EAR Path", "EAR", "HPL EAR Sel"},

	/* Output */
	{"HPL Mixer", "DACLHPL Switch", "HPL Path"},
	{"HPR Mixer", "DACRHPR Switch", "DACR Switch"},

	{"ADCLHPL Loop Switch", NULL, "ADCL PGA"},
	{"ADCLHPR Loop Switch", NULL, "ADCL PGA"},
	{"ADCRHPL Loop Switch", NULL, "ADCR PGA"},
	{"ADCRHPR Loop Switch", NULL, "ADCR PGA"},

	{"HPL Drv Path Switch", NULL, "HPL Mixer"},
	{"HPL Drv Path Switch", NULL, "HPR Mixer"},
	{"HP DEPOP", NULL, "HPL Drv Path Switch"},
	{"HP DEPOP", NULL, "HPR Drv Path Switch"},
	{"DALR DC Offset", NULL, "HP DEPOP"},
	{"HP BUF Switch", NULL, "DALR DC Offset"},
	{"HPL EAR Sel2", NULL, "HP BUF Switch"},
	{"HPL Switch", "HPL", "HPL EAR Sel2"},
	{"HPR Switch", NULL, "HP BUF Switch"},
	{"HPL Gain", NULL, "HPL Switch"},
	{"HPR Gain", NULL, "HPR Switch"},
	{"Virt HP Jack", "Switch", "HPL Gain"},
	{"Virt HP Jack", "Switch", "HPR Gain"},
	{"HP PA", NULL, "Virt HP Jack"},

	{"SPKL Mixer", "DACLSPKL Switch", "DACS Switch"},

	{"ADCLSPKL Loop Switch", NULL, "ADCL PGA"},
	{"ADCLSPKR Loop Switch", NULL, "ADCL PGA"},
	{"ADCRSPKL Loop Switch", NULL, "ADCR PGA"},
	{"ADCRSPKR Loop Switch", NULL, "ADCR PGA"},

	{"SPKL Mixer", "ADCLSPKL Switch", "ADCLSPKL Loop Switch"},
	{"SPKL Mixer", "ADCRSPKL Switch", "ADCRSPKL Loop Switch"},
	{"SPKR Mixer", "ADCLSPKR Switch", "ADCLSPKR Loop Switch"},
	{"SPKR Mixer", "ADCRSPKR Switch", "ADCRSPKR Loop Switch"},

	{"DAS DC Offset", NULL, "SPKL Mixer"},
	{"SPKL Switch", NULL, "DAS DC Offset"},
	{"SPKR Switch", NULL, "SPKR Mixer"},
	{"SPKL Gain", NULL, "SPKL Switch"},

	{"Spk PA", NULL, "Spk PA Switch"},
	{"Spk PA Switch", NULL, "SPKL Gain"},
	{"Spk PA Switch", NULL, "PA Short Check"},

	{"EAR Mixer", "DACLEAR Switch", "EAR Path"},
	{"EAR Drv Path Switch", NULL, "EAR Mixer"},
	{"DALR DC Offset", NULL, "EAR Drv Path Switch"},
	{"EAR Switch", "EAR", "HPL EAR Sel2"},
	{"EAR Gain", NULL, "EAR Switch"},
	{"EAR", NULL, "EAR Gain"},

	{"ADCL Mute", NULL, "ADCL Mixer"},
	{"ADCR Mute", NULL, "ADCR Mixer"},
	{"ADCL PGA", NULL, "ADCL Mute"},
	{"ADCR PGA", NULL, "ADCR Mute"},
	/* Input */
	{"ADCL Mixer", "MainMICADCL Switch", "Mic Bias"},
	{"ADCR Mixer", "MainMICADCR Switch", "Mic Bias"},
	{"ADCL Mixer", "AuxMICADCL Switch", "Mic Bias"},
	{"ADCR Mixer", "AuxMICADCR Switch", "Mic Bias"},
	{"ADCL Mixer", "HPMICADCL Switch", "HeadMic Bias"},
	{"ADCR Mixer", "HPMICADCR Switch", "HeadMic Bias"},
	/* Captrue */
	{"ADCL Switch", NULL, "ADCL PGA"},
	{"ADCR Switch", NULL, "ADCR PGA"},
	{"ADie Digital ADCL Switch", NULL, "ADCL Switch"},
	{"ADie Digital ADCR Switch", NULL, "ADCR Switch"},
	{"Digital ADCL Switch", NULL, "ADie Digital ADCL Switch"},
	{"Digital ADCR Switch", NULL, "ADie Digital ADCR Switch"},
	{"ADC", NULL, "Digital ADCL Switch"},
	{"ADC", NULL, "Digital ADCR Switch"},
	{"ADC Voice", NULL, "Digital ADCL Switch"},
	{"ADC Voice", NULL, "Digital ADCR Switch"},
	{"ADC Voip", NULL, "Digital ADCL Switch"},
	{"ADC Voip", NULL, "Digital ADCR Switch"},

	{"Digital ADC1L Switch", NULL, "ADie Digital ADCL Switch"},
	{"Digital ADC1R Switch", NULL, "ADie Digital ADCR Switch"},
	{"ADC1", NULL, "Digital ADC1L Switch"},
	{"ADC1", NULL, "Digital ADC1R Switch"},
	{"ADC1 Ext", NULL, "Digital ADC1L Switch"},
	{"ADC1 Ext", NULL, "Digital ADC1R Switch"},

	{"Mic Bias", NULL, "MIC"},
	{"Mic Bias", NULL, "AUXMIC"},
	{"HeadMic Bias", NULL, "HPMIC"},

	/* DMIC0 */
	{"DMIC Switch", NULL, "DMIC"},
	{"Digital ADCL Switch", NULL, "DMIC Switch"},
	{"Digital ADCR Switch", NULL, "DMIC Switch"},
	/* DMIC1 */
	{"DMIC1 Switch", NULL, "DMIC1"},
	{"Digital ADC1L Switch", NULL, "DMIC1 Switch"},
	{"Digital ADC1R Switch", NULL, "DMIC1 Switch"},

	/* Bias independent */
	{"Mic Bias", NULL, "Analog Power"},
	{"HeadMic Bias", NULL, "Analog Power"},
	{"ADC-DAC Adie Loop", "switch", "ADC"},
	{"ADC1-DAC Adie Loop", "switch", "ADC1"},
	{"ADC-DAC Adie Loop post", NULL, "ADC-DAC Adie Loop"},
	{"ADC1-DAC Adie Loop post", NULL, "ADC1-DAC Adie Loop"},
	{"DAC", NULL, "ADC-DAC Adie Loop post"},
	{"DAC", NULL, "ADC1-DAC Adie Loop post"},

	{"ADC-DAC Digital Loop", "switch", "ADC"},
	{"ADC1-DAC Digital Loop", "switch", "ADC1"},
	{"ADC-DAC Digital Loop post", NULL, "ADC-DAC Digital Loop"},
	{"ADC1-DAC Digital Loop post", NULL, "ADC1-DAC Digital Loop"},
	{"DAC", NULL, "ADC-DAC Digital Loop post"},
	{"DAC", NULL, "ADC1-DAC Digital Loop post"},

	/* A virtual path for headphone dc-calibration */
	{"DCCAL", NULL, "BG"},
	{"DCCAL", NULL, "BIAS"},
	{"HP DEPOP", NULL, "DCCAL"},
};

static int sprd_codec_vol_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int id = GET_PGA_ID(FUN_REG(mc->reg));
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	struct sprd_codec_pga_op *pga = &sprd_codec->pga[id];
	int ret = 0;

	sp_asoc_pr_info("Set PGA[%s] to %ld\n", sprd_codec_pga_debug_str[id],
			ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	pga->pgaval = val;
	if (pga->set)
		ret = pga->set(codec, pga->pgaval);

	return ret;
}

static int sprd_codec_vol_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int id = GET_PGA_ID(FUN_REG(mc->reg));
	int max = mc->max;
	unsigned int invert = mc->invert;
	struct sprd_codec_pga_op *pga = &sprd_codec->pga[id];

	ucontrol->value.integer.value[0] = pga->pgaval;
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_switch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int id = GET_SWITCH_ID(FUN_REG(mc->reg));
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	struct sprd_codec_switch_op *switcher = &sprd_codec->switcher[id];
	int ret = 0;

	sp_asoc_pr_info("Switch [%s] %s\n", switch_name[id],
			ucontrol->value.integer.value[0] ? "on" : "off");

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	switcher->on = val;
	if (switcher->set)
		ret = switcher->set(codec, switcher->on);

	return ret;
}

static int sprd_codec_switch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int id = GET_SWITCH_ID(FUN_REG(mc->reg));
	int max = mc->max;
	unsigned int invert = mc->invert;
	struct sprd_codec_switch_op *switcher = &sprd_codec->switcher[id];

	ucontrol->value.integer.value[0] = switcher->on;
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
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;

	sp_asoc_pr_info("Config inter PA 0x%lx\n",
			ucontrol->value.integer.value[0]);

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;

	mutex_lock(&sprd_codec->inter_pa_mutex);

	sprd_codec->inter_pa.value = (u32)val;

	if (sprd_codec->inter_pa.set) {
		mutex_unlock(&sprd_codec->inter_pa_mutex);
		sprd_inter_speaker_pa(codec, 1);
	} else {
		mutex_unlock(&sprd_codec->inter_pa_mutex);
	}

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

	mutex_lock(&sprd_codec->inter_pa_mutex);
	ucontrol->value.integer.value[0] = sprd_codec->inter_pa.value;
	mutex_unlock(&sprd_codec->inter_pa_mutex);
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_info_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	int ret = 0;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_info("%s, %d\n",
			__func__, (int)ucontrol->value.integer.value[0]);

	val = ucontrol->value.integer.value[0];

	if (val != SPRD_CODEC_INFO)
		sp_asoc_pr_info("%s, wrong codec info (%d)\n", __func__, val);

	return ret;
}

static int sprd_codec_info_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	u32 chip_id = 0;
	int ver_id = 0;

	ucontrol->value.integer.value[0] = AUDIO_CODEC_2721;

	chip_id = sci_get_ana_chip_id() >> 16;
	ver_id = sci_get_ana_chip_ver();

	if (chip_id == 0x2731) {
		ucontrol->value.integer.value[0] = AUDIO_CODEC_2731;
	} else if (chip_id == 0x2723) {
		if (ver_id == AUDIO_2723_VER_E)
			ucontrol->value.integer.value[0] = AUDIO_CODEC_2723E;
		else if (ver_id == AUDIO_2723_VER_T)
			ucontrol->value.integer.value[0] = AUDIO_CODEC_2723T;
	}

	sp_asoc_pr_info("%s, codec info = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

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
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s, %d\n",
		       __func__, (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_DAC] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(DAC_LR_SEL) : 0;
	agdsp_access_enable();
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

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s, %d\n",
		       __func__, (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_ADC] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(ADC_LR_SEL) : 0;
	agdsp_access_enable();
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

	if (ucontrol->value.integer.value[0] >= texts->items ||
	    ucontrol->value.integer.value[0] < 0) {
		pr_err("ERR: %s,index outof bounds error\n", __func__);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s, %d\n",
		       __func__, (int)ucontrol->value.integer.value[0]);

	sprd_codec->lrclk_sel[LRCLK_SEL_ADC1] =
		!!ucontrol->value.enumerated.item[0];
	val = !!ucontrol->value.enumerated.item[0] ? BIT(ADC1_LR_SEL) : 0;
	agdsp_access_enable();
	snd_soc_update_bits(codec, SOC_REG(AUD_ADC1_I2S_CTL),
			    BIT(ADC1_LR_SEL), val);
	agdsp_access_disable();

	return 0;
}

static void _dacspkl_switch_nolock(struct snd_soc_codec *codec, int val)
{
	int dacrspkl = val & BIT(SPRD_CODEC_RIGHT);
	int daclspkl = val & BIT(SPRD_CODEC_LEFT);
	int r_on = dacrspkl ? 1 : 0;
	int l_on = daclspkl ? 1 : 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	sp_asoc_pr_info("%s,l:%d,r:%d\n", __func__, l_on, r_on);
	if (sprd_codec->sprd_dacspkl_set & BIT(SPRD_CODEC_RIGHT))
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC3),
				    BIT(SDAPA), r_on << SDAPA);
	if (sprd_codec->sprd_dacspkl_set & BIT(SPRD_CODEC_LEFT))
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC3),
				    BIT(SDAPA), l_on << SDAPA);
}

static int dacspkl_enable_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	unsigned int shift = mc->shift;

	sp_asoc_pr_info("%s, %s(%s)\n", __func__, shift ? "dacrspkl" : "daclspkl",
			(int)ucontrol->value.integer.value[0] ? "on" : "off");

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mutex_lock(&sprd_codec->sprd_dacspkl_mutex);
	if (val)
		sprd_codec->sprd_dacspkl_enable |= (1 << shift);
	else
		sprd_codec->sprd_dacspkl_enable &= ~(1 << shift);
	sp_asoc_pr_info("%s,0x%x,0x%x\n", __func__,
			sprd_codec->sprd_dacspkl_enable, sprd_codec->sprd_dacspkl_set);
	if (sprd_codec->sprd_dacspkl_set)
		_dacspkl_switch_nolock(codec, sprd_codec->sprd_dacspkl_enable);
	mutex_unlock(&sprd_codec->sprd_dacspkl_mutex);

	return 0;
}

static int dacspkl_enable_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	int max = mc->max;
	int shift = mc->shift;
	unsigned int invert = mc->invert;
	int mask = 1 << shift;

	sp_asoc_pr_info("%s, 0x%08x,%s,shift:%d\n",
			__func__, mask, mask & 0x2 ? "dacrspkl" : "daclspkl", shift);
	mutex_lock(&sprd_codec->sprd_dacspkl_mutex);
	ucontrol->value.integer.value[0] =
		(sprd_codec->sprd_dacspkl_enable & mask) >> shift;
	mutex_unlock(&sprd_codec->sprd_dacspkl_mutex);

	if (invert) {
		ucontrol->value.integer.value[0] =
			max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static int sprd_codec_mic_bias_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	int ret = 0;
	int id = GET_MIC_BIAS_ID(reg);

	val = (ucontrol->value.integer.value[0] & mask);

	if (invert)
		val = max - val;
	if (sprd_codec->mic_bias[id] == val)
		return 0;

	sp_asoc_pr_info("%s Switch %s\n", mic_bias_name[id], STR_ON_OFF(val));

	sprd_codec->mic_bias[id] = val;
	if (val) {
		ret = snd_soc_dapm_force_enable_pin(
			&codec->component.card->dapm, mic_bias_name[id]);
	} else {
		ret = snd_soc_dapm_disable_pin(
			&codec->component.card->dapm, mic_bias_name[id]);
	}

	/* signal a DAPM event */
	snd_soc_dapm_sync(&codec->component.card->dapm);

	return ret;
}

static int sprd_codec_mic_bias_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg = FUN_REG(mc->reg);
	int max = mc->max;
	unsigned int invert = mc->invert;
	int id = GET_MIC_BIAS_ID(reg);

	ucontrol->value.integer.value[0] = sprd_codec->mic_bias[id];
	if (invert) {
		ucontrol->value.integer.value[0] =
		    max - ucontrol->value.integer.value[0];
	}

	return 0;
}

static const char * const das_input_mux_texts[] = {
	"L+R", "L*2", "R*2", "ZERO"
};

static const SOC_ENUM_SINGLE_DECL(
	das_input_mux_enum, AUD_CFGA_ANA_ET2,
	AUD_DAS_MIX_SEL, das_input_mux_texts);

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

#define SPRD_CODEC_PGA(xname, xreg, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, FUN_REG(xreg), 0, 15, 1, \
		sprd_codec_vol_get, sprd_codec_vol_put, tlv_array)

#define SPRD_CODEC_PGA_M(xname, xreg, max, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, FUN_REG(xreg), 0, max, 0, \
		sprd_codec_vol_get, sprd_codec_vol_put, tlv_array)

#define SPRD_CODEC_PGA_MAX_INVERT(xname, xreg, max, tlv_array) \
	SOC_SINGLE_EXT_TLV(xname, FUN_REG(xreg), 0, max, 1, \
		sprd_codec_vol_get, sprd_codec_vol_put, tlv_array)

#define SPRD_CODEC_MIC_BIAS(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, \
		sprd_codec_mic_bias_get, sprd_codec_mic_bias_put)

#define SPRD_CODEC_SWITCH(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, 1, 0, \
		sprd_codec_switch_get, sprd_codec_switch_put)

static const struct snd_kcontrol_new sprd_codec_snd_controls[] = {
	SPRD_CODEC_PGA_M("SPKL Playback Volume",
			 SPRD_CODEC_PGA_SPKL, 2, spk_tlv),
	SPRD_CODEC_PGA_MAX_INVERT("HPL Playback Volume",
				  SPRD_CODEC_PGA_HPL, 7, hp_tlv),
	SPRD_CODEC_PGA_MAX_INVERT("HPR Playback Volume",
				  SPRD_CODEC_PGA_HPR, 7, hp_tlv),

	SPRD_CODEC_PGA_MAX_INVERT("EAR Playback Volume",
				  SPRD_CODEC_PGA_EAR, 9, ear_tlv),

	SPRD_CODEC_PGA_M("ADCL Capture Volume", SPRD_CODEC_PGA_ADCL, 7,
			 adc_tlv),
	SPRD_CODEC_PGA_M("ADCR Capture Volume", SPRD_CODEC_PGA_ADCR, 7,
			 adc_tlv),

	SPRD_CODEC_PGA_MAX_INVERT("DAC Playback Volume",
				  SPRD_CODEC_PGA_DAC, 2, dac_tlv),
	SOC_SINGLE_EXT("Inter PA Config", 0, 0, INT_MAX, 0,
		       sprd_codec_inter_pa_get, sprd_codec_inter_pa_put),

	SPRD_CODEC_SWITCH("DACL Switch", SPRD_CODEC_DACL),

	SPRD_CODEC_SWITCH("DACR Switch", SPRD_CODEC_DACR),

	SPRD_CODEC_MIC_BIAS("MIC Bias Switch", SPRD_CODEC_MIC_BIAS),

	SPRD_CODEC_MIC_BIAS("HEADMIC Bias Switch", SPRD_CODEC_HEADMIC_BIAS),

	SOC_SINGLE_EXT("DACLSPKL Enable", 0, SPRD_CODEC_LEFT, 1, 0,
		       dacspkl_enable_get, dacspkl_enable_put),

	SOC_SINGLE_EXT("DACRSPKL Enable", 0, SPRD_CODEC_RIGHT, 1, 0,
		       dacspkl_enable_get, dacspkl_enable_put),

	SOC_ENUM_EXT("Aud Codec Info", codec_info_enum,
		     sprd_codec_info_get, sprd_codec_info_put),

	SOC_ENUM_EXT("DAC LRCLK Select", lrclk_sel_enum,
		     sprd_codec_dac_lrclk_sel_get,
		sprd_codec_dac_lrclk_sel_put),
	SOC_ENUM_EXT("ADC LRCLK Select", lrclk_sel_enum,
		     sprd_codec_adc_lrclk_sel_get,
		sprd_codec_adc_lrclk_sel_put),
	SOC_ENUM_EXT("ADC1 LRCLK Select", lrclk_sel_enum,
		     sprd_codec_adc1_lrclk_sel_get,
		sprd_codec_adc1_lrclk_sel_put),

	SOC_ENUM("DAS Input Mux", das_input_mux_enum),
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

		if (codec_aud_en_set() < 0)
			return -1;
		ret = readl_relaxed((void __iomem *)(reg -
			CODEC_DP_BASE + sprd_codec_dp_base));
		codec_aud_en_clr();

		return ret;
	} else if (IS_SPRD_CODEC_MIXER_RANG(FUN_REG(reg))) {
		struct sprd_codec_priv *sprd_codec =
		    snd_soc_codec_get_drvdata(codec);
		int id = GET_MIXER_ID(FUN_REG(reg));
		struct sprd_codec_mixer *mixer = &sprd_codec->mixer[id];

		return mixer->on;
	} else if (IS_SPRD_CODEC_ADC_LOOP_RANG(FUN_REG(reg))) {
		struct sprd_codec_priv *sprd_codec =
			snd_soc_codec_get_drvdata(codec);
		int id = GET_ADC_LOOP_ID(FUN_REG(reg));

		return sprd_codec->ad_da_loop_en[id];
	} else if (IS_SPRD_CODEC_PGA_RANG(FUN_REG(reg))) {
		/* do nothing */
	} else if (IS_SPRD_CODEC_MIC_BIAS_RANG(FUN_REG(reg))) {
		/* do nothing */
	} else if (IS_SPRD_CODEC_SWITCH_RANG(FUN_REG(reg))) {
		/* do nothing */
	} else {
		sp_asoc_pr_dbg("read the register is not codec's reg = 0x%x\n",
			       reg);
	}
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

		if (codec_aud_en_set() < 0)
			return -1;
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
		codec_aud_en_clr();

		return ret;
	} else if (IS_SPRD_CODEC_MIXER_RANG(FUN_REG(reg))) {
		struct sprd_codec_priv *sprd_codec =
			snd_soc_codec_get_drvdata(codec);
		int id = GET_MIXER_ID(FUN_REG(reg));
		struct sprd_codec_mixer *mixer = &sprd_codec->mixer[id];

		mixer->on = val ? 1 : 0;
	} else if (IS_SPRD_CODEC_SWITCH_RANG(FUN_REG(reg))) {
		struct sprd_codec_priv *sprd_codec =
			snd_soc_codec_get_drvdata(codec);
		int id = GET_SWITCH_ID(FUN_REG(reg));
		struct sprd_codec_switch_op *switcher =
			&sprd_codec->switcher[id];

		switcher->on = val ? 1 : 0;
	} else if (IS_SPRD_CODEC_PGA_RANG(FUN_REG(reg))) {
		/* do nothing */
	} else if (IS_SPRD_CODEC_MIC_BIAS_RANG(FUN_REG(reg))) {
		/* do nothing */
	} else {
		sp_asoc_pr_dbg("write the register is not codec's reg = 0x%x\n",
			       reg);
	}
	return ret;
}

static int sprd_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	u32 *fixed_rate = sprd_codec->fixed_sample_rate;
	int mask = 0x0F;
	int shift = 0;
	u32 rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		sprd_codec->da_sample_val = fixed_rate[CODEC_PATH_DA] ?
			fixed_rate[CODEC_PATH_DA] : rate;
		sp_asoc_pr_info("Playback rate is [%u]\n",
				sprd_codec->da_sample_val);
		sprd_codec_set_sample_rate(codec,
					   sprd_codec->da_sample_val, mask, shift);
	} else {
		if (dai->id != SPRD_CODEC_IIS1_ID) {
			sprd_codec->ad_sample_val = fixed_rate[CODEC_PATH_AD] ?
				fixed_rate[CODEC_PATH_AD] : rate;
			sprd_codec_set_ad_sample_rate(codec,
						      sprd_codec->ad_sample_val, mask, shift);
			sp_asoc_pr_info("Capture rate is [%u]\n",
					sprd_codec->ad_sample_val);
		} else {
			sprd_codec->ad1_sample_val = fixed_rate[CODEC_PATH_AD1]
				? fixed_rate[CODEC_PATH_AD1] : rate;
			sprd_codec_set_ad_sample_rate(codec,
						      sprd_codec->ad1_sample_val,
				ADC1_SRC_N_MASK, ADC1_SRC_N);
			sp_asoc_pr_info("Capture(ADC1) rate is [%u]\n",
					sprd_codec->ad1_sample_val);
		}
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

#ifdef CONFIG_PM
static int sprd_codec_soc_suspend(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct regulator *regu = sprd_codec->head_mic;
	int ret = 0;

	sp_asoc_pr_info("%s, startup_cnt=%d\n",
			__func__, sprd_codec->startup_cnt);

	if (regu && regulator_is_enabled(regu) &&
	    sprd_codec->startup_cnt == 0) {
		ret = regulator_set_mode(regu, REGULATOR_MODE_STANDBY);
		if (ret < 0) {
			sp_asoc_pr_info("%s, set mode ret=%d", __func__, ret);
			return ret;
		}
		ret = snd_soc_update_bits(codec, ANA_PMU0,
					  BIT(BG_EN) | BIT(HMICBIAS_VREF_SEL), 0);

		if (ret < 0) {
			sp_asoc_pr_info("%s, clear BG failed, ANA_PMU0=0x%x",
					__func__, snd_soc_read(codec, ANA_PMU0));
			return ret;
		}
	} else {
		sp_asoc_pr_info("%s ignored\n", __func__);
	}
	return 0;
}

static int sprd_codec_soc_resume(struct snd_soc_codec *codec)
{
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	struct regulator *regu = sprd_codec->head_mic;
	int ret = 0;

	sp_asoc_pr_info("%s, startup_cnt=%d\n",
			__func__, sprd_codec->startup_cnt);

	if (regu && regulator_is_enabled(regu)) {
		ret =
			snd_soc_update_bits(codec,
			    ANA_PMU0, BIT(BG_EN) | BIT(HMICBIAS_VREF_SEL),
			    BIT(BG_EN) | BIT(HMICBIAS_VREF_SEL));
		if (ret < 0) {
			sp_asoc_pr_info("%s, set BG failed, ANA_PMU0=0x%x\n",
					__func__, snd_soc_read(codec, ANA_PMU0));
			return ret;
		}

		ret = regulator_set_mode(regu, REGULATOR_MODE_NORMAL);
		if (ret < 0) {
			sp_asoc_pr_info("%s, set mode ret=%d", __func__, ret);
			return ret;
		}
	}
	return 0;
}
#else
#define sprd_codec_soc_suspend NULL
#define sprd_codec_soc_resume  NULL
#endif

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
#define REGU_CNT 2

/* procfs interfaces for factory test: check if audio PA P/N is shorted. */
static void pa_short_stat_proc_read(struct snd_info_entry *entry,
				    struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;
	struct snd_soc_codec *codec = sprd_codec ? sprd_codec->codec : NULL;
	struct regulator *regu[REGU_CNT];
	unsigned int regu_mode[REGU_CNT];
	const char *regu_id[REGU_CNT] = {"BG", "BIAS"};
	struct device *dev = codec ? codec->dev : NULL;
	const char *short_stat[] = {"NONE", "VBAT", "GND", "CHECK_FAILED"};
	enum PA_SHORT_T pa_short_stat = PA_SHORT_CHECK_FAILED;
	unsigned int pa_en;
	int ret = 0;
	int i;

	if (!codec) {
		pr_err("%s, codec is NULL!", __func__);
		goto CHECK_FAILED;
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

	/* Store the pa status before pa short check. */
	pa_en = snd_soc_read(codec, SOC_REG(ANA_CDC2)) & BIT(PA_EN);
	/* Do the short check. */
	pa_short_stat = codec_pa_short_check(codec);
	pr_debug("%s, pa_short_stat: %d, pa_en: %d\n", __func__, pa_short_stat,
		 !!pa_en);
	/* Restore the pa status after pa short check. */
	if (pa_en)
		snd_soc_update_bits(codec, SOC_REG(ANA_CDC2), pa_en, pa_en);

	/* Disable VB, BG & BIAS */
REGULATOR_CLEAR:
	for (i -= 1; i >= 0; i--) {
		regulator_set_mode(regu[i], regu_mode[i]);
		regulator_disable(regu[i]);
		regulator_put(regu[i]);
	}

CHECK_FAILED:
	snd_iprintf(buffer, "short_stat=%s\n", short_stat[pa_short_stat]);
}

static void sprd_codec_proc_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct sprd_codec_priv *sprd_codec = entry->private_data;
	struct snd_soc_codec *codec = sprd_codec->codec;
	int reg;

	agdsp_access_enable();
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

	snd_iprintf(buffer, "%s analog part(0x403C8700)\n",
		    codec->component.name);
	for (reg = SPRD_CODEC_AP_BASE;
	       reg < SPRD_CODEC_AP_ANA_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - SPRD_CODEC_AP_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		    );
	}
	snd_iprintf(buffer, "%s analog part(0x403C8800)\n",
		    codec->component.name);
	for (reg = AUD_CFGA_REG_BASE; reg < SPRD_CODEC_AP_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - AUD_CFGA_REG_BASE)
			    , snd_soc_read(codec, reg + 0x00)
			    , snd_soc_read(codec, reg + 0x04)
			    , snd_soc_read(codec, reg + 0x08)
			    , snd_soc_read(codec, reg + 0x0C)
		    );
	}

	/* For AUDIF registers
	 * 0x5c0 = 0x700 - 0x140;
	 */
	snd_iprintf(buffer, "%s audif(0x403C8140)\n", codec->component.name);
	for (reg = SPRD_CODEC_AP_BASE; reg < 0x20 + SPRD_CODEC_AP_BASE;
	     reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (unsigned int)(reg - SPRD_CODEC_AP_BASE)
			    , arch_audio_codec_read(reg + 0x00 - 0x5c0)
			    , arch_audio_codec_read(reg + 0x04 - 0x5c0)
			    , arch_audio_codec_read(reg + 0x08 - 0x5c0)
			    , arch_audio_codec_read(reg + 0x0C - 0x5c0)
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
	if (!aud_glb_reg) {
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

	if (!snd_card_proc_new(card, "pa-short-stat", &entry))
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
	 SC2721_UNSUPPORTED_AD_RATE | \
	 SNDRV_PCM_RATE_48000)

#define SPRD_CODEC_PCM_FATMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

/* PCM Playing and Recording default in full duplex mode */
static struct snd_soc_dai_driver sprd_codec_dai[] = {
	{/* 0 */
	 .name = "sprd-codec-normal",
	 .playback = {
		      .stream_name = "Normal-Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SPRD_CODEC_PCM_RATES,
		      .formats = SPRD_CODEC_PCM_FATMATS,
		      },
	 .capture = {
		     .stream_name = "Normal-Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SPRD_CODEC_PCM_AD_RATES,
		     .formats = SPRD_CODEC_PCM_FATMATS,
		     },
	 .ops = &sprd_codec_dai_ops,
	 },
	{/* 1 */
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
	{/* 2 */
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
	/* 3 */
	{
	 .id = SPRD_CODEC_IIS1_ID,
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
	/* 4 */
	{
	 .id = SPRD_CODEC_IIS1_ID,
	 .name = "sprd-codec-ad1-voice",
	 .capture = {
		     .stream_name = "Ext-Vioce-Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SPRD_CODEC_PCM_AD_RATES,
		     .formats = SPRD_CODEC_PCM_FATMATS,
		     },
	 .ops = &sprd_codec_dai_ops,
	 },
	{/* 5 */
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
	{/* 6 */
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
	{/* 7 */
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
};

static void sprd_codec_power_changed(struct power_supply *psy)
{
	struct sprd_codec_priv *sprd_codec = psy->drv_data;

	dev_info(&psy->dev, "%s %s\n", __func__, psy->desc->name);

	if (atomic_read(&sprd_codec->ldo_refcount) >= 1)
		sprd_codec_vcom_ldo_auto(sprd_codec->codec, 1);
}

static int sprd_codec_power_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	return -EINVAL;
}

static int sprd_codec_audio_ldo(struct sprd_codec_priv *sprd_codec)
{
	struct snd_soc_codec *codec = sprd_codec->codec;
	struct power_supply *spy;
	struct power_supply_config cfg = {
		.drv_data = sprd_codec,
	};

	/*
	 * "audio-ldo" is added in @battery_supply_list in sprd-battery.c.
	 * Or a property "power-supplies = <&battery>;" must be added
	 * in analog codec node in dts.
	 */
	sprd_codec->audio_ldo_desc.name = "audio-ldo";
	sprd_codec->audio_ldo_desc.get_property = sprd_codec_power_get_property;
	sprd_codec->audio_ldo_desc.external_power_changed =
		sprd_codec_power_changed;
	spy = power_supply_register(codec->dev, &sprd_codec->audio_ldo_desc,
				    &cfg);
	if (IS_ERR(spy)) {
		dev_err(codec->dev, "ERR:Register power supply error!\n");
		return PTR_ERR(spy);
	}
	sprd_codec->audio_ldo = spy;

	return 0;
}

static void codec_reconfig_dai_rate(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct sprd_card_data *mdata = snd_soc_card_get_drvdata(card);
	unsigned int replace_adc_rate =
		mdata ? mdata->codec_replace_adc_rate : 0;
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int *fixed_rates = sprd_codec->fixed_sample_rate;
	unsigned int unsupported_rates = SC2721_UNSUPPORTED_AD_RATE;
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
		if ((dai->driver->id == SPRD_CODEC_IIS1_ID &&
		     fixed_rates[CODEC_PATH_AD1]) ||
		    (dai->driver->id != SPRD_CODEC_IIS1_ID &&
		     fixed_rates[CODEC_PATH_AD]))
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

	sprd_codec_audio_ldo(sprd_codec);

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
	struct sprd_codec_priv *sprd_codec = snd_soc_codec_get_drvdata(codec);

	if (sprd_codec->audio_ldo)
		power_supply_unregister(sprd_codec->audio_ldo);

	sprd_headset_power_deinit();

	return 0;
}

static int sprd_codec_power_regulator_init(struct sprd_codec_priv *sprd_codec,
					   struct device *dev)
{
	sprd_codec_power_get(dev, &sprd_codec->main_mic, "MICBIAS");
	sprd_codec_power_get(dev, &sprd_codec->head_mic, "HEADMICBIAS");
	sprd_codec_power_get(dev, &sprd_codec->vb, "VB");

	return 0;
}

static void sprd_codec_power_regulator_exit(struct sprd_codec_priv *sprd_codec)
{
	sprd_codec_power_put(&sprd_codec->main_mic);
	sprd_codec_power_put(&sprd_codec->head_mic);
}

static struct snd_soc_codec_driver soc_codec_dev_sprd_codec = {
	.probe = sprd_codec_soc_probe,
	.remove = sprd_codec_soc_remove,
	.suspend = sprd_codec_soc_suspend,
	.resume = sprd_codec_soc_resume,
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
	SOC_TYPE_NA = 0,
	SOC_TYPE_SHARK,
	SOC_TYPE_WHALE2,
	SOC_TYPE_MAX
};

static int sprd_codec_get_soc_type(struct device_node *np)
{
	int ret, i;
	const char *cmptbl;
	struct cmp_str_type {
		const char *str;
		int type;
	} cmp_str[] = {
		{"whale", SOC_TYPE_WHALE2},
		{"sc9850", SOC_TYPE_WHALE2},
		{"sc9860", SOC_TYPE_WHALE2},
		{"shark", SOC_TYPE_SHARK},
	};

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

	for (i = 0; i < ARRAY_SIZE(cmp_str); i++) {
		if (strstr(cmptbl, cmp_str[i].str))
			return cmp_str[i].type;
	}

	return SOC_TYPE_NA;
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
		return -ENODEV;
	}

	if (!sprd_codec) {
		pr_err("ERR: [%s] sprd_codec_priv is NULL!\n", __func__);
		return -ENOMEM;
	}

	soc_type = sprd_codec_get_soc_type(np);
	if (soc_type == SOC_TYPE_WHALE2) {
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
	} else if (soc_type == SOC_TYPE_SHARK) {
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
		ret = devm_request_irq(&pdev->dev,
				       sprd_codec->dp_irq,
			    sprd_codec_dp_irq, 0,
			    "sprd_codec_dp", sprd_codec);
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
	} else {
		arch_audio_set_anlg_phy_g(anlg_phy_gpr);
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
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
					 sprd_codec->fixed_sample_rate,
				    CODEC_PATH_MAX);
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
		return -ENODEV;
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
	arch_audio_codec_set_regmap(adi_rgmp);
	arch_audio_codec_set_reg_offset((unsigned long)val);
	/* Set global register accessing vars for headset. */
	glb_vars.regmap = adi_rgmp;
	glb_vars.codec_reg_offset = val;
	sprd_headset_set_global_variables(&glb_vars);

	/* Parsing configurations varying as machine. */
	ret = sprd_codec_dt_parse_mach(pdev);
	if (ret)
		return ret;

	val = platform_get_irq(pdev, 0);
	if (val > 0) {
		sprd_codec->ap_irq = val;
		sp_asoc_pr_dbg("Set AP IRQ is %d!\n", val);
	} else {
		pr_err("ERR:Must give me the AP IRQ!\n");
		return -EINVAL;
	}
	ret = devm_request_threaded_irq(&pdev->dev,
					sprd_codec->ap_irq, NULL,
			    sprd_codec_ap_irq, IRQF_ONESHOT,
			    "sprd_codec_ap", sprd_codec);
	if (ret) {
		pr_err("ERR:Request ap irq(%d) failed(ret)!\n",
		       sprd_codec->ap_irq);
		return ret;
	}
	INIT_DELAYED_WORK(&sprd_codec->ap_irq_delayed_work,
			  codec_ap_irq_delay_worker);

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

	sp_asoc_pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s: Only OF is supported yet!\n", __func__);
		return -EINVAL;
	}

	sprd_codec = devm_kzalloc(&pdev->dev,
				  sizeof(struct sprd_codec_priv), GFP_KERNEL);
	if (!sprd_codec)
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
		return -ENODEV;
	}
	arch_audio_set_pmu_apb_gpr(pmu_apb_gpr);
	of_node_put(np);
	dig_pdev = of_find_device_by_node(dig_np);
	if (unlikely(!dig_pdev)) {
		pr_err("%s: this node has no pdev?!\n", __func__);
		return -ENODEV;
	}
	platform_set_drvdata(dig_pdev, sprd_codec);
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
	sprd_codec_switch_init(sprd_codec);
	sprd_codec_dacspkl_enable_init(sprd_codec);
	sprd_codec_power_regulator_init(sprd_codec, &pdev->dev);

	mutex_init(&sprd_codec->inter_pa_mutex);

	sprd_codec_vcom_ldo_cfg(sprd_codec, ldo_v_map, ARRAY_SIZE(ldo_v_map));

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
	{.compatible = "sprd,sc2721-audio-codec",},
	{.compatible = "sprd,sc2720-audio-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, codec_of_match);
#endif

static struct platform_driver sprd_codec_codec_driver = {
	.driver = {
		   .name = "sprd-codec-sc2721",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(codec_of_match),
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
