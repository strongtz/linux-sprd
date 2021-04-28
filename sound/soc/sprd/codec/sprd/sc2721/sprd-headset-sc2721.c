/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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
#define pr_fmt(fmt) pr_sprd_fmt("HDST2721")""fmt

#include <asm/div64.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/iio/consumer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/slab.h>
//#include <linux/sprd_otp.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/nvmem-consumer.h>

#include "sprd-asoc-common.h"
#include "sprd-codec.h"
#include "sprd-headset-2721.h"
#include "sprd-asoc-card-utils.h"

#define ENTER pr_debug("func: %s  line: %04d\n", __func__, __LINE__)

/* In kernel 3.18, the processing of the interrupt handler of
 * pmic eic changes to thread way, which cause the gap between
 * irq triggered and  eic level trigger settings must be less than
 * about 50ms. So the trigger settings must be located
 * in headset_irq_xx_handler().
 */
#define FOR_EIC_CHANGE_DET /* peng.lee debug for eic change */
#define FOR_EIC_CHANGE_BUTTON /* peng.lee debug for eic change */

#define EIC_AUD_HEAD_INST2 312
#define MAX_BUTTON_NUM 6
#define SPRD_HEADSET_JACK_MASK (SND_JACK_HEADSET)
#define SPRD_BUTTON_JACK_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
	SND_JACK_BTN_2 | SND_JACK_BTN_3 | SND_JACK_BTN_4)
#define PLUG_CONFIRM_COUNT (1)
#define NO_MIC_RETRY_COUNT (0)
/*
 * to confirm irq to the first time to read adc value then
 * we can set ADC_READ_COUNT to 2
 */
#define ADC_READ_COUNT (2)
#define ADC_READ_LOOP (2)
#define CHIP_ID_2720 0x2720
/*
 * acoording asic(chen.si)
 * asci has made 2 average adc sample for 2721,
 * so we can change adc sapmle from 20 to 10.
 *
 */
#define SCI_ADC_GET_VALUE_COUNT (10)

#define ABS(x) (((x) < (0)) ? (-(x)) : (x))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define headset_reg_read(reg, val) \
	sci_adi_read(CODEC_REG((reg)), val)

#define headset_reg_write(reg, val, mask) \
	sci_adi_write(CODEC_REG((reg)), (val), (mask))

#define headset_reg_clr_bits(reg, bits) \
	sci_adi_clr(CODEC_REG((reg)), (bits))

#define headset_reg_set_bits(reg, bits) \
	sci_adi_set(CODEC_REG((reg)), (bits))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int dsp_fm_mute_by_set_dg(void)
	__attribute__ ((weak, alias("__dsp_fm_mute_by_set_dg")));

static int __dsp_fm_mute_by_set_dg(void)
{
	pr_err("ERR: dsp_fm_mute_by_set_dg is not defined!\n");
	return -1;
}
#pragma GCC diagnostic pop

static inline int headset_reg_get_bits(unsigned int reg, int bits)
{
	unsigned int temp;
	int ret;

	ret = sci_adi_read(CODEC_REG(reg), &temp);
	if (ret) {
		pr_err("%s: read reg#%#x failed!\n", __func__, reg);
		return ret;
	}
	temp = temp & bits;

	return temp;
}

enum sprd_headset_type {
	HEADSET_4POLE_NORMAL,
	HEADSET_NO_MIC,
	HEADSET_4POLE_NOT_NORMAL,
	HEADSET_APPLE,
	HEADSET_TYPE_ERR = -1,
};

struct sprd_headset_auxadc_cal_l {
	u32 A;
	u32 B;
	u32 E1;
	u32 E2;
	u32 cal_type;
};

#define SPRD_HEADSET_AUXADC_CAL_NO 0
#define SPRD_HEADSET_AUXADC_CAL_DO 1

/* summer for calculation usage */
static struct sprd_headset_auxadc_cal_l adc_cal_headset = {
	0, 0, 0, 0, SPRD_HEADSET_AUXADC_CAL_NO,
};


static struct sprd_headset *sprd_hdst;
static bool fast_charge_finished;

/* ========================  audio codec  ======================== */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int vbc_close_fm_dggain(bool mute)
	__attribute__ ((weak, alias("__vbc_close_fm_dggain")));
static int __vbc_close_fm_dggain(bool mute)
{
	pr_err("ERR: vbc_close_fm_dggain is not defined!\n");
	return -1;
}
#pragma GCC diagnostic pop

/* When remove headphone, disconnect the headphone
 * dapm DA path in codec driver.
 */
static int dapm_jack_switch_control(struct snd_soc_codec *codec, bool on)
{
	struct snd_kcontrol *kctrl;
	struct snd_soc_card *card = codec ? codec->component.card : NULL;
	struct snd_ctl_elem_id id = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Virt HP Jack Switch",
	};
	struct snd_ctl_elem_value ucontrol = {
		.value.integer.value[0] = on,
	};

	if (!card) {
		pr_err("%s card is NULL!\n", __func__);
		return -1;
	}

	pr_info("%s, %s\n", __func__, on ? "on" : "off");
	kctrl = snd_ctl_find_id(card->snd_card, &id);
	if (!kctrl) {
		pr_err("%s can't find kctrl '%s'\n", __func__, id.name);
		return -1;
	}

	return snd_soc_dapm_put_volsw(kctrl, &ucontrol);
}

static void headset_jack_report(struct sprd_headset *hdst,
	struct snd_soc_jack *jack, int status, int mask)
{
	struct sprd_headset_platform_data *pdata = &hdst->pdata;

	if (mask & SND_JACK_HEADPHONE && (!pdata->hpr_spk))
		dapm_jack_switch_control(hdst->codec, !!status);

	snd_soc_jack_report(jack, status, mask);
}

static enum snd_jack_types headset_jack_type_get(int index)
{
	enum snd_jack_types jack_type_map[MAX_BUTTON_NUM] = {
		SND_JACK_BTN_0, SND_JACK_BTN_1, SND_JACK_BTN_2,
		SND_JACK_BTN_3, SND_JACK_BTN_4, SND_JACK_BTN_5
	};

	return jack_type_map[index];
}

static int sprd_headset_power_get(struct device *dev,
				  struct regulator **regu, const char *id)
{
	struct regulator *regu_ret;

	if (!*regu) {
		*regu = regulator_get(dev, id);
		if (IS_ERR(*regu)) {
			pr_err("ERR:Failed to request %ld: %s\n",
				PTR_ERR(*regu), id);
			regu_ret = *regu;
			*regu = 0;
			return PTR_ERR(regu_ret);
		}
	}

	return 0;
}

/* summer need to update from the sprd-audio */
static int sprd_headset_power_init(struct sprd_headset *hdst)
{
	int ret = 0;
	struct platform_device *pdev = hdst->pdev;
	struct device *dev = NULL;
	struct sprd_headset_power *power = &hdst->power;

	if (!pdev) {
		pr_err("%s: codec is null!\n", __func__);
		return -1;
	}
	dev = &pdev->dev;
	ret = sprd_headset_power_get(dev, &power->head_mic, "HEADMICBIAS");
	if (ret || (power->head_mic == NULL)) {
		power->head_mic = 0;
		return ret;
	}
	regulator_set_voltage(power->head_mic, 950000, 950000);

	ret = sprd_headset_power_get(dev, &power->bg, "BG");
	if (ret) {
		power->bg = 0;
		goto __err1;
	}

	ret = sprd_headset_power_get(dev, &power->bias, "BIAS");
	if (ret) {
		power->bias = 0;
		goto __err2;
	}

	goto __ok;

__err2:
	regulator_put(power->bg);
__err1:
	regulator_put(power->head_mic);
__ok:
	return ret;
}

static int sprd_headset_power_regulator_init(struct sprd_headset *hdst)
{
	int ret = 0;
	struct platform_device *pdev = hdst->pdev;
	struct device *dev = NULL;
	struct sprd_headset_power *power = &hdst->power;

	if (!pdev) {
		pr_err("%s: codec is null!\n", __func__);
		return -1;
	}
	dev = &pdev->dev;
	ret = sprd_headset_power_get(dev, &power->vb, "VB");
	if (ret || (power->vb == NULL)) {
		power->vb = 0;
		return ret;
	}
	return sprd_headset_power_init(hdst);
}

/* summer need to update from the sprd-audio */
void sprd_headset_power_deinit(void)
{
	struct sprd_headset_power *power = &sprd_hdst->power;

	regulator_put(power->head_mic);
	regulator_put(power->bg);
	regulator_put(power->bias);
}
/* summer need to update from the sprd-audio */
static int sprd_headset_audio_block_is_running(struct sprd_headset *hdst)
{
	struct sprd_headset_power *power = &hdst->power;

	return (regulator_is_enabled(power->bg) &&
		regulator_is_enabled(power->bias));
}

static int sprd_headset_audio_headmic_sleep_disable(
	struct sprd_headset *hdst, int on)
{
	int ret = 0;
	struct sprd_headset_power *power = &hdst->power;

	if (!power->head_mic)
		return -1;

	if (on) {
		ret = regulator_set_mode(
			power->head_mic, REGULATOR_MODE_NORMAL);
	} else {
		if (sprd_headset_audio_block_is_running(hdst))
			ret = regulator_set_mode(
				power->head_mic, REGULATOR_MODE_NORMAL);
		else
			ret = regulator_set_mode(
				power->head_mic, REGULATOR_MODE_STANDBY);
	}

	return ret;
}

static int sprd_headset_headmic_bias_control(
	struct sprd_headset *hdst, int on)
{
	int ret = 0;
	struct sprd_headset_power *power = &hdst->power;

	if (!power->head_mic)
		return -1;

	if (on)
		ret = regulator_enable(power->head_mic);
	else
		ret = regulator_disable(power->head_mic);
	if (!ret) {
		/* Set HEADMIC_SLEEP when audio block closed */
		if (sprd_headset_audio_block_is_running(hdst))
			ret = regulator_set_mode(
				power->head_mic, REGULATOR_MODE_NORMAL);
		else
			ret = regulator_set_mode(
				power->head_mic, REGULATOR_MODE_STANDBY);
	}

	return ret;
}

static int sprd_headset_bias_control(
	struct sprd_headset *hdst, int on)
{
	int ret = 0;
	struct sprd_headset_power *power = &hdst->power;

	if (!power->head_mic)
		return -1;

	pr_info("%s bias set %d\n", __func__, on);
	if (on)
		ret = regulator_enable(power->bias);
	else
		ret = regulator_disable(power->bias);

	return ret;
}
static int sprd_headset_vb_control(
	struct sprd_headset *hdst, int on)
{
	int ret = 0;
	static int state;
	struct sprd_headset_power *power = &hdst->power;

	if (!power->vb)
		return -1;

	pr_info("%s bias set %d\n", __func__, on);
	if (on) {
		if (state == 0) {
			ret = regulator_enable(power->vb);
			state = 1;
		}
	} else
		ret = regulator_disable(power->vb);

	return ret;
}


static BLOCKING_NOTIFIER_HEAD(hp_chain_list);
int headset_register_notifier(struct notifier_block *nb)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return 0;
	}

	if (pdata->jack_type == JACK_TYPE_NO) {
		nb = NULL;
		return 0;
	}

	return blocking_notifier_chain_register(&hp_chain_list, nb);
}
EXPORT_SYMBOL(headset_register_notifier);

int headset_unregister_notifier(struct notifier_block *nb)
{
	if (nb == NULL)
		return -1;

	return blocking_notifier_chain_unregister(&hp_chain_list, nb);
}
EXPORT_SYMBOL(headset_unregister_notifier);

#if 0
static int hp_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&hp_chain_list, val, NULL)
		== NOTIFY_BAD) ? -EINVAL : 0;
}
#endif

int headset_get_plug_state(void)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return 0;
	}

	return !!hdst->plug_stat_last;
}
EXPORT_SYMBOL(headset_get_plug_state);
/* ========================  audio codec  ======================== */

static int headset_wrap_sci_adc_get(struct iio_channel *chan)
{
	int count = 0;
	int average = 0;
	int val, ret = 0;
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return 0;
	}

	if (!chan) {
		pr_err("%s: iio_channel is NULL\n", __func__);
		return 0;
	}

	#if 0
	if (hdst->pdata.jack_type == JACK_TYPE_NO) {
		headset_reg_set_bits(ANA_HDT0, BIT(BUT_DET_PD));
		usleep_range(2000, 4000);
	}
	#endif
	while (count < SCI_ADC_GET_VALUE_COUNT) {
		ret = iio_read_channel_raw(chan, &val);
		if (ret < 0) {
			pr_err("%s: read adc raw value failed!\n", __func__);
			return 0;
		}
		average += val;
		count++;
	}

	#if 0
	if (hdst->pdata.jack_type == JACK_TYPE_NO)
		headset_reg_clr_bits(ANA_HDT0, BIT(BUT_DET_PD));
	#endif
	average /= SCI_ADC_GET_VALUE_COUNT;

	pr_debug("%s: adc: %d\n", __func__, average);

	return average;
}

#if 0
/*  on = 0: open headmic detect circuit */
static void headset_detect_circuit(unsigned int on)
{
	#if 0
	if (on) {
		headset_reg_clr_bits(ANA_HDT0, BIT(HEAD_INS_PD));
		headset_reg_clr_bits(ANA_HDT0, BIT(BUT_DET_PD));
	} else {
		headset_reg_set_bits(ANA_HDT0, BIT(HEAD_INS_PD));
		headset_reg_set_bits(ANA_HDT0, BIT(BUT_DET_PD));
	}
	#endif
}
#endif
static void headset_detect_clk_en(void)
{
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN);
	headset_reg_set_bits(AUD_CFGA_CLK_EN, BIT(CLK_AUD_HID_EN));
}

static void headset_detect_reg_init(void)
{
	unsigned int msk, val;

	headset_detect_clk_en();

	/* default ANA_HDT0*/

	/* must set in initial phase */
	headset_reg_set_bits(ANA_HDT0, BIT(HEDET_VREF_EN));
	headset_reg_set_bits(ANA_HDT0, BIT(HEDET_V2AD_SCALE));

	/* headset_reg_set_bits(ANA_HDT0, BIT(HEDET_BUF_CHOP)); */

	msk = HEDET_V21_SEL_MASK << HEDET_V21_SEL;
	val = HEDET_V21_SEL_1U << HEDET_V21_SEL;
	headset_reg_write(ANA_HDT0, val, msk);
	headset_reg_read(ANA_HDT0, &val);
	pr_info("%s: ANA_HDT0: %#x\n", __func__, val);

	/* default ANA_HDT1 = 0xce05 */
	/* bit15-13 = 110 */
	msk = HEDET_MICDET_REF_SEL_MASK << HEDET_MICDET_REF_SEL;
	val = HEDET_MICDET_REF_SEL_2P6 << HEDET_MICDET_REF_SEL;
	headset_reg_write(ANA_HDT1, val, msk);

	/* bit12-11 = 01 */
	msk = HEDET_MICDET_HYS_SEL_MASK << HEDET_MICDET_HYS_SEL;
	val = HEDET_MICDET_HYS_SEL_20MV << HEDET_MICDET_HYS_SEL;
	headset_reg_write(ANA_HDT1, val, msk);

	/* bit10-8 = 110 */
	msk = HEDET_LDET_REFL_SEL_MASK << HEDET_LDET_REFL_SEL;
	val = HEDET_LDET_REFL_SEL_300MV << HEDET_LDET_REFL_SEL;
	headset_reg_write(ANA_HDT1, val, msk);

	/* bit7-6 = 0 */
	msk = HEDET_LDET_REFH_SEL_MASK << HEDET_LDET_REFH_SEL;
	val = HEDET_LDET_REFH_SEL_1P9 << HEDET_LDET_REFH_SEL;
	headset_reg_write(ANA_HDT1, val, msk);

	/* bit5-4 = 0 must set in initial phase */
	msk = HEDET_LDET_PU_PD_MASK << HEDET_LDET_PU_PD;
	val = HEDET_LDET_PU_PD_PU << HEDET_LDET_PU_PD;
	headset_reg_write(ANA_HDT1, val, msk);

	/* bit3-2 = 01 */
	msk = HEDET_LDET_L_HYS_SEL_MASK << HEDET_LDET_L_HYS_SEL;
	val = HEDET_LDET_L_HYS_SEL_20MV << HEDET_LDET_L_HYS_SEL;
	headset_reg_write(ANA_HDT1, val, msk);

	/* bit1-0 = 01 */
	msk = HEDET_LDET_H_HYS_SEL_MASK << HEDET_LDET_H_HYS_SEL;
	val = HEDET_LDET_H_HYS_SEL_20MV << HEDET_LDET_H_HYS_SEL;
	headset_reg_write(ANA_HDT1, val, msk);
	headset_reg_read(ANA_HDT1, &val);
	pr_info("%s: ANA_HDT1: %#x\n", __func__, val);

	/* default ANA_HDT2 = 0x0030 */
	val = HEDET_BDET_HYS_SEL_20MV << HEDET_BDET_HYS_SEL;
	msk = HEDET_BDET_HYS_SEL_MASK << HEDET_BDET_HYS_SEL;
	headset_reg_write(ANA_HDT2, val, msk);

	headset_reg_set_bits(ANA_HDT2, BIT(PLGPD_EN));
	headset_reg_set_bits(ANA_HDT2, BIT(HPL_EN_D2HDT_EN));
	headset_reg_read(ANA_HDT2, &val);
	pr_info("%s: ANA_HDT2: %#x\n", __func__, val);

	/*ANA_DCL0 = 0x46*/
	headset_reg_write(ANA_DCL0, 0x46, 0xFFFF);
	headset_reg_read(ANA_DCL0, &val);
	pr_info("%s: ANA_DCL0: %#x\n", __func__, val);

	/*ANA_PMU0 init config*/
	headset_reg_set_bits(ANA_PMU0, BIT(VBG_SEL));
	headset_reg_clr_bits(ANA_PMU0, BIT(MICBIAS_PLGB));

	val = VBG_TEMP_TUNE_TC_REDUCE << VBG_TEMP_TUNE;
	msk = VBG_TEMP_TUNE_MASK << VBG_TEMP_TUNE;
	headset_reg_write(ANA_PMU0, val, msk);

	headset_reg_set_bits(ANA_PMU0, BIT(HMIC_COMP_MODE1_EN));
	headset_reg_read(ANA_PMU0, &val);
	pr_info("%s: ANA_PMU0: %#x\n", __func__, val);

}

static void headset_detect_init(void)
{
	int val;

	headset_reg_clr_bits(ANA_PMU0, BIT(MICBIAS_PLGB));

	headset_reg_set_bits(ANA_PMU0, BIT(HMIC_COMP_MODE1_EN));
	headset_reg_read(ANA_PMU0, &val);
	pr_info("%s: ANA_PMU0: %#x\n", __func__, val);

}

static void headmic_sleep_disable(struct sprd_headset *hdst, int on);

static void headset_scale_set(int large_scale)
{
	if (large_scale)
		headset_reg_set_bits(ANA_HDT0, BIT(HEDET_V2AD_SCALE));
	else
		headset_reg_clr_bits(ANA_HDT0, BIT(HEDET_V2AD_SCALE));
}
static void headset_adc_en(int en)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	if (en) {
		headset_reg_set_bits(ANA_HDT0, BIT(HEDET_BUF_EN));
		headmic_sleep_disable(hdst, 1);
	} else {
		headset_reg_clr_bits(ANA_HDT0, BIT(HEDET_BUF_EN));
		headmic_sleep_disable(hdst, 0);
	}
}


/* is_set = 1, headset_mic to AUXADC */
static void headset_set_adc_to_headmic(unsigned int is_set)
{
	unsigned long msk, val;

	headset_adc_en(1);
	msk = HEDET_MUX2ADC_SEL_MASK << HEDET_MUX2ADC_SEL;
	if (is_set)
		val = HEDET_MUX2ADC_SEL_HEADMIC_IN_DETECT << HEDET_MUX2ADC_SEL;
	else
		val = HEDET_MUX2ADC_SEL_HEADSET_L_INT << HEDET_MUX2ADC_SEL;
	headset_reg_write(ANA_HDT0, val, msk);
}
static void headset_button_irq_threshold(int enable)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int audio_head_sbut = 0;
	unsigned long msk, val;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	audio_head_sbut = pdata->irq_threshold_button;
	msk = HEDET_BDET_REF_SEL_MASK << HEDET_BDET_REF_SEL;
	val = enable ? audio_head_sbut << HEDET_BDET_REF_SEL : 0xf;
	headset_reg_write(ANA_HDT2, val, msk);
	if (enable)
		headset_reg_set_bits(ANA_HDT0, BIT(HEDET_BDET_EN));
	else
		headset_reg_clr_bits(ANA_HDT0, BIT(HEDET_BDET_EN));
}

static void headset_irq_button_enable(int enable, unsigned int irq)
{
	static int current_irq_state = 1;
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	mutex_lock(&hdst->irq_btn_lock);
	if (enable == 1) {
		if (current_irq_state == 0) {
			enable_irq(irq);
			current_irq_state = 1;
		}
	} else {
		if (current_irq_state == 1) {
			disable_irq_nosync(irq);
			current_irq_state = 0;
		}
	}
	mutex_unlock(&hdst->irq_btn_lock);
}

static void headset_irq_detect_all_enable(int enable, unsigned int irq)
{
	/* irq is enabled after request_irq() */
	static int current_irq_state = 1;
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	mutex_lock(&hdst->irq_det_all_lock);
	if (enable == 1) {
		if (current_irq_state == 0) {
			enable_irq(irq);
			current_irq_state = 1;
		}
	} else {
		if (current_irq_state == 1) {
			disable_irq_nosync(irq);
			current_irq_state = 0;
		}
	}
	mutex_unlock(&hdst->irq_det_all_lock);
}

static void headset_irq_detect_mic_enable(int enable, unsigned int irq)
{
	static int current_irq_state = 1;
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	mutex_lock(&hdst->irq_det_mic_lock);
	if (enable == 1) {
		if (current_irq_state == 0) {
			enable_irq(irq);
			current_irq_state = 1;
		}
	} else {
		if (current_irq_state == 1) {
			disable_irq_nosync(irq);
			current_irq_state = 0;
		}
	}
	mutex_unlock(&hdst->irq_det_mic_lock);
}

static void headmic_sleep_disable(struct sprd_headset *hdst, int on)
{
	static int current_power_state;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	if (on == 1) {
		if (current_power_state == 0) {
			sprd_headset_audio_headmic_sleep_disable(hdst, 1);
			current_power_state = 1;
		}
	} else {
		if (current_power_state == 1) {
			sprd_headset_audio_headmic_sleep_disable(hdst, 0);
			current_power_state = 0;
		}
	}
}

static void headmicbias_power_on(struct sprd_headset *hdst, int on)
{
	static int current_power_state;
	struct sprd_headset_platform_data *pdata;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	pdata = &hdst->pdata;

	if (on == 1) {
		if (current_power_state == 0) {
			if (pdata->external_headmicbias_power_on != NULL)
				pdata->external_headmicbias_power_on(1);
			sprd_headset_headmic_bias_control(hdst, 1);
			current_power_state = 1;
		}
	} else {
		if (current_power_state == 1) {
			if (pdata->external_headmicbias_power_on != NULL)
				pdata->external_headmicbias_power_on(0);
			sprd_headset_headmic_bias_control(hdst, 0);
			current_power_state = 0;
		}
	}
}

static int headset_sts0_confirm(u32 jack_type)
{
	if (headset_reg_get_bits(ANA_STS7, BIT(HEAD_INSERT_ALL)) == 0) {
		if (jack_type == JACK_TYPE_NC)
			return 0;
		else
			return 1;
	} else {
		return 1;
	}
}

static int headset_adc_compare(int x1, int x2)
{
	int delta = 0;
	int max = 0;

	if ((x1 < 300) && (x2 < 300))
		return 1;

	x1 = ((x1 == 0) ? 1 : (x1*100));
	x2 = ((x2 == 0) ? 1 : (x2*100));

	delta = ABS(x1-x2);
	max = MAX(x1, x2);

	if (delta < ((max*10)/100))
		return 1;
	else
		return 0;
}

static int headset_accumulate_adc(int *adc, u32 jack_type,
				  struct iio_channel *chan,
				  int gpio_num, int gpio_value)
{
	int k;
	int j;
	int success = 1;

	for (j = 0; j < ADC_READ_COUNT/2; j++) {
		if (jack_type != JACK_TYPE_NO &&
		    gpio_get_value(gpio_num) != gpio_value) {
			pr_warn("gpio value changed!!! the adc read operation aborted (step3)\n");
			return -1;
		}

		if (headset_sts0_confirm(jack_type) == 0) {
			pr_warn("headset_sts0_confirm failed!!! the adc read operation aborted (step4)\n");
			return -1;
		}
		headset_reg_clr_bits(ANA_HDT0, BIT(HEDET_BUF_CHOP));
		adc[2*j] = headset_wrap_sci_adc_get(chan);
		headset_reg_set_bits(ANA_HDT0, BIT(HEDET_BUF_CHOP));
		adc[2*j+1] = headset_wrap_sci_adc_get(chan);
		if (headset_adc_compare(adc[2*j], adc[2*j+1]) == 0) {
			success = 0;
			for (k = 0; k <= j+1; k++)
				pr_debug("adc[%d] = %d\n", k, adc[k]);
			break;
		}
	}

	return success;
}

static int headset_cal_adc_average(int *adc, u32 jack_type,
				   int gpio_num, int gpio_value)
{
	int i;
	int adc_average = 0;

	if (gpio_get_value(gpio_num) != gpio_value) {
		pr_warn("gpio value changed!!! the adc read operation aborted (step5)\n");
		return -1;
	}

	if (headset_sts0_confirm(jack_type) == 0) {
		pr_warn("headset_sts0_confirm failed!!! the adc read operation aborted (step6)\n");
		return -1;
	}

	for (i = 0; i < ADC_READ_COUNT; i++) {
		adc_average += adc[i];
		pr_debug("adc[%d] = %d\n", i, adc[i]);
	}
	adc_average = adc_average / ADC_READ_COUNT;
	pr_debug("%s success, adc_average = %d\n", __func__, adc_average);

	return adc_average;
}

static int headset_get_adc_average(struct iio_channel *chan,
				   int gpio_num, int gpio_value)
{
	int i = 0;
	int success = 1;
	int adc[ADC_READ_COUNT] = {0};
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	u32 jack_type;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	jack_type = pdata->jack_type;

	/* ================= debug ===================== */
	if (hdst->debug_level >= 3) {
		int count = 0;
		int adc_val = 0;
		int gpio_button = 0;
		int ana_pmu0 = 0;
		int ana_hdt0 = 0;
		int ana_sts7 = 0;

		while (count < 20) {
			count++;
			adc_val = headset_wrap_sci_adc_get(chan);
			gpio_button = gpio_get_value(
						pdata->gpios[HDST_GPIO_BUTTON]);
			headset_reg_read(ANA_PMU0, &ana_pmu0);
			headset_reg_read(ANA_HDT0, &ana_hdt0);
			headset_reg_read(ANA_STS7, &ana_sts7);

			pr_info("%2d:  gpio_button=%d  adc=%4d	ana_pmu0=0x%08X    ana_hdt0=0x%08X	ana_sts77=0x%08X\n",
				count, gpio_button, adc_val, ana_pmu0,
				ana_hdt0, ana_sts7);
			usleep_range(800, 1200);
		}
	}
	/* ================= debug ===================== */

	/*
	 * we can should confirm 2-4ms delay before read adc(from si.chen),
	 * and we can only compare one time based on this delay
	 */
	usleep_range(2000, 4000);
	for (i = 0; i < ADC_READ_LOOP; i++) {
		if (gpio_get_value(gpio_num) != gpio_value) {
			pr_warn("gpio value changed!!! the adc read operation aborted (step1)\n");
			return -1;
		}
		if (headset_sts0_confirm(jack_type) == 0) {
			pr_warn("headset_sts0_confirm failed!!! the adc read operation aborted (step2)\n");
			return -1;
		}

		success = headset_accumulate_adc(adc, jack_type, chan, gpio_num,
						 gpio_value);
		if (success < 0)
			return success;

		if (success == 1) {
			usleep_range(2800, 3600);
			return headset_cal_adc_average(adc, jack_type, gpio_num,
						       gpio_value);
		} else if (i+1 < ADC_READ_LOOP) {
			pr_info("%s failed, retrying count = %d\n",
				__func__, i+1);
			usleep_range(800, 1200);
		}
	}
	pr_info("%s out\n", __func__);

	return -1;
}

static int headset_irq_set_irq_type(unsigned int irq, unsigned int type)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct irq_desc *irq_desc = NULL;
	unsigned int irq_flags = 0;
	int ret = -1;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	ret = irq_set_irq_type(irq, type);
	irq_desc = irq_to_desc(irq);
	irq_flags = irq_desc->action->flags;

	if (irq == hdst->irq_button) {
		if (type == IRQF_TRIGGER_HIGH) {
			pr_debug("IRQF_TRIGGER_HIGH is set for irq_button(%d). irq_flags = 0x%08X, ret = %d\n",
				  hdst->irq_button, irq_flags, ret);
		} else if (type == IRQF_TRIGGER_LOW) {
			pr_debug("IRQF_TRIGGER_LOW is set for irq_button(%d). irq_flags = 0x%08X, ret = %d\n",
				  hdst->irq_button, irq_flags, ret);
		}
	} else if (irq == hdst->irq_detect_all) {
		if (type == IRQF_TRIGGER_HIGH) {
			pr_debug("IRQF_TRIGGER_HIGH is set for irq_detect(%d). irq_flags = 0x%08X, ret = %d\n",
				  hdst->irq_detect_all, irq_flags, ret);
		} else if (type == IRQF_TRIGGER_LOW) {
			pr_debug("IRQF_TRIGGER_LOW is set for irq_detect(%d). irq_flags = 0x%08X, ret = %d\n",
				  hdst->irq_detect_all, irq_flags, ret);
		}
	}

	return 0;
}

static int headset_button_valid(int gpio_detect_value_current)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int button_is_valid = 0;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	if (pdata->irq_trigger_levels[HDST_GPIO_DET_ALL] == 1) {
		if (gpio_detect_value_current == 1)
			button_is_valid = 1;
		else
			button_is_valid = 0;
	} else {
		if (gpio_detect_value_current == 0)
			button_is_valid = 1;
		else
			button_is_valid = 0;
	}

	return button_is_valid;
}

static int headset_gpio_2_button_state(int gpio_button_value_current)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int button_state_current = 0;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	if (pdata->irq_trigger_levels[HDST_GPIO_BUTTON] == 1) {
		if (gpio_button_value_current == 1)
			button_state_current = 1;
		else
			button_state_current = 0;
	} else {
		if (gpio_button_value_current == 0)
			button_state_current = 1;
		else
			button_state_current = 0;
	}

	return button_state_current; /* 0==released, 1==pressed */
}

static int headset_adc_get_ideal(u32 adc_mic, u32 coefficient, bool big_scale);
/* summer: softflow  limit the read adc time */

#if 0
static int headset_plug_confirm_by_adc(struct iio_channel *chan,
	int last_gpio_detect_value)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int adc_last = 0;
	int adc_current = 0;
	int count = 0;
	int adc_read_interval = 10;

	if (!hdst || !chan) {
		pr_err("%s: sprd_hdset(%p) or chan(%p) is NULL!\n",
			__func__, hdst, chan);
		return -1;
	}

	adc_last = headset_get_adc_average(chan,
					   pdata->gpios[HDST_GPIO_DET_ALL],
					   last_gpio_detect_value);
	if (-1 == adc_last) {
		pr_info("%s failed!!!\n", __func__);
		return -1;
	}

	while (count < PLUG_CONFIRM_COUNT) {
		msleep(adc_read_interval);
		adc_current = headset_get_adc_average(
			chan, pdata->gpios[HDST_GPIO_DET_ALL],
			last_gpio_detect_value);
		if (-1 == adc_current) {
			pr_info("%s failed!!!\n", __func__);
			return -1;
		}
		if (headset_adc_compare(adc_last, adc_current) == 0) {
			pr_info("%s failed!!!\n", __func__);
			return -1;
		}
		adc_last = adc_current;
		count++;
	}
	pr_info("%s success!!!\n", __func__);

	return adc_current;
}

static int headset_read_adc_repeatable(struct iio_channel *chan,
	int last_gpio_detect_value)
{
	int retrytimes = 0;
	int adc_value = 0;

	do {
		adc_value = headset_plug_confirm_by_adc(chan,
							last_gpio_detect_value);
		retrytimes++;
	} while ((-1 == adc_value) && (retrytimes < 10));
	pr_info("%s : adc_value  %d retrytimes is %d\n",
		__func__, adc_value, retrytimes);

	return adc_value;
}
#endif

static int headset_get_adc_value(struct iio_channel *chan)
{
	int adc_value = 0;

	headset_reg_clr_bits(ANA_HDT0, BIT(HEDET_BUF_CHOP));
	adc_value = headset_wrap_sci_adc_get(chan);
	headset_reg_set_bits(ANA_HDT0, BIT(HEDET_BUF_CHOP));
	adc_value = headset_wrap_sci_adc_get(chan) + adc_value;
	pr_info("adc_value is %d\n", adc_value/2);

	return adc_value/2;
}
static enum sprd_headset_type
headset_type_detect_all(int last_gpio_detect_value)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int adc_mic_average = 0;
	int adc_left_average = 0;
	int adc_mic_ideal = 0;
	int retry_times = 1;
	unsigned int msk, val;
	struct iio_channel *adc_chan;
	int ret;

	ENTER;

	if (!hdst) {
		pr_err("%s: sprd_hdset(%p) is NULL!\n",
			__func__, hdst);
		return HEADSET_TYPE_ERR;
	}

	adc_chan = hdst->adc_chan;
	if (!adc_chan) {
		pr_err("%s adc_chan is NULL!\n", __func__);
		return HEADSET_TYPE_ERR;
	}

	if (pdata->gpio_switch != 0)
		gpio_direction_output(pdata->gpio_switch, 0);
	else
		pr_info("automatic type switch is unsupported\n");

	headset_detect_clk_en();

retry_again:
	pr_info("now get adc value of headmic in big scale\n");
	/* set large scale */
	headset_scale_set(1);
	ret = iio_write_channel_attribute(adc_chan, 1, 0, IIO_CHAN_INFO_SCALE);
	if (!ret)
		pr_err("%s set channel attribute big failed!\n", __func__);

	headset_set_adc_to_headmic(1);
	sprd_msleep(10);
	adc_mic_average = headset_get_adc_value(adc_chan);
	adc_mic_average = headset_adc_get_ideal(adc_mic_average,
						pdata->coefficient, true);
	if ((adc_mic_average > pdata->sprd_stable_value) ||
					(adc_mic_average == -1)) {
		if (retry_times < 10) {
			retry_times++;
			sprd_msleep(100);
			pr_info("retry time is %d adc_mic_average %d\n",
				retry_times, adc_mic_average);
			goto  retry_again;
		}
	}
	pr_info("adc_mic_average = %d\n", adc_mic_average);

	pr_info("now get adc value  in little scale\n");
	/* change to little scale */
	headset_scale_set(0);

	ret = iio_write_channel_attribute(adc_chan, 0, 0, IIO_CHAN_INFO_SCALE);
	if (!ret)
		pr_err("%s set channel attribute little failed!\n", __func__);

	/* get adc value of left */
	headset_set_adc_to_headmic(0);
	msleep(20);
	adc_left_average = headset_get_adc_value(adc_chan);
	pr_info("adc_left_average = %d\n", adc_left_average);
	if (-1 == adc_left_average)
		return HEADSET_TYPE_ERR;

	if (retry_times >= 10) {
		if (adc_left_average < pdata->sprd_one_half_adc_gnd)
			return HEADSET_NO_MIC;
		if (adc_left_average >= pdata->sprd_one_half_adc_gnd)
			return HEADSET_TYPE_ERR;
	}

	/* Get adc value of headmic in. */
	headset_set_adc_to_headmic(1);
	adc_mic_average = headset_get_adc_value(adc_chan);
	pr_info("adc_mic_average = %d\n", adc_mic_average);

	adc_mic_ideal = headset_adc_get_ideal(adc_mic_average,
						pdata->coefficient, false);
	if (adc_mic_ideal >= 0)
		adc_mic_average = adc_mic_ideal;

	if ((gpio_get_value(pdata->gpios[HDST_GPIO_DET_ALL])) !=
	    last_gpio_detect_value) {
		pr_info("software debance (gpio check)!!!(headset_type_detect)\n");
		return HEADSET_TYPE_ERR;
	}

	if (adc_left_average < pdata->sprd_one_half_adc_gnd) {
		if (adc_mic_average < pdata->adc_threshold_3pole_detect)
			return HEADSET_NO_MIC;
		if (adc_left_average < pdata->sprd_half_adc_gnd) {
			/* bit10-8 change from 110 to 001 */
			msk = HEDET_LDET_REFL_SEL_MASK << HEDET_LDET_REFL_SEL;
			val = HEDET_LDET_REFL_SEL_50MV << HEDET_LDET_REFL_SEL;
			headset_reg_write(ANA_HDT1, val, msk);
			headset_reg_read(ANA_HDT1, &val);
			pr_info("ANA_HDT1 0x%04x, %#x\n", val, val);
		}
		return HEADSET_4POLE_NORMAL;
	} else if  (ABS(adc_mic_average - adc_left_average) <
						pdata->sprd_adc_gnd)
		return HEADSET_4POLE_NOT_NORMAL;
	else
		return HEADSET_TYPE_ERR;

	return HEADSET_TYPE_ERR;
}

static void headset_button_release_verify(void)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	if (hdst->btn_stat_last == 1) {
		headset_jack_report(hdst, &hdst->btn_jack,
			0, hdst->btns_pressed);
		hdst->btn_stat_last = 0;
		pr_info("headset button released by force!!! current button: %#x\n",
			hdst->btns_pressed);
		hdst->btns_pressed &= ~SPRD_BUTTON_JACK_MASK;
		if (hdst->pdata.irq_trigger_levels[HDST_GPIO_BUTTON] == 1)
			headset_irq_set_irq_type(
				hdst->irq_button, IRQF_TRIGGER_HIGH);
		else
			headset_irq_set_irq_type(
				hdst->irq_button, IRQF_TRIGGER_LOW);
	}
}

static enum snd_jack_types headset_adc_to_button(int adc_mic)
{
	int i;
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	struct headset_buttons *hdst_btns =
		(pdata ? pdata->headset_buttons : NULL);
	int nb = (pdata ? pdata->nbuttons : 0);
	enum snd_jack_types j_type = KEY_RESERVED;

	if (!hdst || !hdst_btns) {
		pr_err("%s: sprd_hdst(%p) or hdst_btns(%p) is NULL!\n",
			__func__, sprd_hdst, hdst_btns);
		return KEY_RESERVED;
	}

	for (i = 0; i < nb; i++) {
		if (adc_mic >= hdst_btns[i].adc_min &&
			adc_mic < hdst_btns[i].adc_max) {
			j_type = headset_jack_type_get(i);
			break;
		}
	}

	return j_type;
}

static void headset_button_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int gpio_button_value_current = 0;
	int button_state_current = 0;
	int adc_mic_average = 0;
	int adc_ideal = 0;
	struct iio_channel *chan;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}
	chan = hdst->adc_chan;

	down(&hdst->sem);
	headset_set_adc_to_headmic(1);
	ENTER;

	gpio_button_value_current =
		gpio_get_value(pdata->gpios[HDST_GPIO_BUTTON]);
	if (gpio_button_value_current != hdst->gpio_btn_val_last) {
		pr_info("software debance (step 1: gpio check)\n");
		goto out;
	}

	button_state_current =
		headset_gpio_2_button_state(gpio_button_value_current);

	if (button_state_current == 1) {/* pressed! */
		if (pdata->nbuttons > 0) {
			adc_mic_average = headset_get_adc_average(chan,
				pdata->gpios[HDST_GPIO_BUTTON],
				hdst->gpio_btn_val_last);
			if (-1 == adc_mic_average) {
				pr_info("software debance (step 3: adc check)!!!(%s)(pressed)\n",
						__func__);
				goto out;
			}
			adc_ideal = headset_adc_get_ideal(adc_mic_average,
						pdata->coefficient, false);
			pr_info("adc_mic_average=%d, adc_ideal=%d\n",
				adc_mic_average, adc_ideal);
			if (adc_ideal >= 0)
				adc_mic_average = adc_ideal;
			pr_info("adc_mic_average = %d\n", adc_mic_average);
			hdst->btns_pressed |=
				headset_adc_to_button(adc_mic_average);
		}

		if (hdst->btn_stat_last == 0) {
			headset_jack_report(hdst, &hdst->btn_jack,
				hdst->btns_pressed, hdst->btns_pressed);
			hdst->btn_stat_last = 1;
			pr_info("Reporting headset button press. button: %#x\n",
				hdst->btns_pressed);
		} else {
			pr_err("Headset button has been reported already. button: %#x\n",
				hdst->btns_pressed);
		}
	} else { /* released! */
		if (hdst->btn_stat_last == 1) {
			headset_jack_report(hdst, &hdst->btn_jack,
				0, hdst->btns_pressed);
			hdst->btn_stat_last = 0;
			pr_info("Reporting headset button release. button: %#x\n",
				hdst->btns_pressed);
		} else
			pr_err("Headset button has been released already. button: %#x\n",
				hdst->btns_pressed);

		hdst->btns_pressed &= ~SPRD_BUTTON_JACK_MASK;
	}
out:
	headset_adc_en(0);
	headset_irq_button_enable(1, hdst->irq_button);
	/* wake_unlock(&hdst->btn_wakelock); */
	up(&hdst->sem);
}

static void headset_process_for_4pole(enum sprd_headset_type headset_type)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (!pdata) {
		pr_err("%s: pdata is NULL!\n", __func__);
		return;
	}

	if ((headset_type == HEADSET_4POLE_NOT_NORMAL)
	    && (pdata->gpio_switch == 0)) {
		headset_irq_button_enable(0, hdst->irq_button);
		pr_info("micbias power off for 4 pole abnormal\n");
		headmicbias_power_on(hdst, 0);
		pr_info("HEADSET_4POLE_NOT_NORMAL is not supported %s\n",
			"by your hardware! so disable the button irq!");
	} else {
		if (pdata->irq_trigger_levels[HDST_GPIO_BUTTON])
			headset_irq_set_irq_type(
				hdst->irq_button,
				IRQF_TRIGGER_HIGH);
		else
			headset_irq_set_irq_type(
				hdst->irq_button,
				IRQF_TRIGGER_LOW);

		headset_button_irq_threshold(1);
		headset_irq_button_enable(1, hdst->irq_button);
		/* add this for future use */
		headset_reg_set_bits(ANA_HDT0, BIT(HEDET_MICDET_EN));
	}

	hdst->hdst_status = SND_JACK_HEADSET;
	if (hdst->report == 0) {
		pr_debug("report for 4p\n");
		headset_jack_report(hdst, &hdst->hdst_jack,
			hdst->hdst_status, SND_JACK_HEADSET);
	}
	if (hdst->re_detect == true) {
		pr_debug("report for 4p re_detect\n");
		headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);

		headset_jack_report(hdst, &hdst->hdst_jack,
			hdst->hdst_status, SND_JACK_HEADSET);
	}
	hdst->report = 1;
	pr_info("headset plug in (headset_detect_work_func)\n");
}

int headset_fast_charge_finished(void)
{
	return fast_charge_finished;
}

static void headset_fast_charge(struct sprd_headset *hdst)
{
	struct sprd_headset_platform_data *pdata = &hdst->pdata;
	unsigned int mask = ~0u & ~(BIT(DAS_EN) | BIT(PA_EN));

	if (!pdata->hpr_spk)
		headset_reg_clr_bits(ANA_CDC2, mask);
	headset_reg_set_bits(ANA_STS2, BIT(CALDC_ENO));
	headset_reg_set_bits(ANA_STS0, BIT(DC_CALI_RDACI_ADJ));
	usleep_range(1000, 1100); /* Wait for 1mS */
	headset_reg_clr_bits(ANA_STS0, BIT(DC_CALI_RDACI_ADJ));

	queue_delayed_work(hdst->reg_dump_work_q, &hdst->fc_work,
		msecs_to_jiffies(0));
}

static void headset_fc_work_func(struct work_struct *work)
{
	int cnt = 0;
	struct sprd_headset *hdst = sprd_hdst;

	pr_debug("Start waiting for fast charging.\n");
	/* Wait at least 50mS before DC-CAL. */
	while ((++cnt <= 3) && hdst->plug_stat_last)
		sprd_codec_wait(20);

	if (hdst->plug_stat_last) {
		pr_info("Headphone fast charging completed. (%d ms)\n",
			 (cnt - 1) * 20);
		fast_charge_finished = true;
	} else
		pr_info("Wait for headphone fast charging aborted.\n");
}

static void headset_detect_all_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	struct sprd_headset_power *power = (hdst ? &hdst->power : NULL);
	enum sprd_headset_type headset_type;
	int plug_state_current = 0;
	int gpio_detect_value_current = 0;
	int gpio_detect_value = 0;
	unsigned int val, msk;
	static int times, times_1;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	down(&hdst->sem);
	ENTER;

	pr_info("%s enter into\n", __func__);
	if ((power->head_mic == NULL) || (power->bias == NULL)) {
		pr_info("sprd_headset_power_init fail 0\n");
		goto out;
	}

	if (hdst->plug_stat_last == 0) {
		msleep(20);
		sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN);
		headset_reg_set_bits(ANA_HDT0, BIT(HEDET_LDET_L_FILTER));
		headset_reg_read(ANA_HDT0, &val);
		pr_info("filter detect_l ANA_HDT0 0x%04x\n", val);

		pr_info("micbias power on\n");
		sprd_headset_bias_control(hdst, 1);
		headmicbias_power_on(hdst, 1);
		headset_detect_init();
		msleep(20);
	}

	if (hdst->plug_stat_last == 0) {
		gpio_detect_value_current =
			gpio_get_value(pdata->gpios[HDST_GPIO_DET_ALL]);
			pr_info("gpio_detect_value_current = %d, gpio_detect_value_last = %d, plug_state_last = %d\n",
				gpio_detect_value_current,
				hdst->gpio_det_val_last,
				hdst->plug_stat_last);

		if (gpio_detect_value_current != hdst->gpio_det_val_last) {
			pr_info("software debance (step 1)!!!(headset_detect_work_func)\n");
			headmicbias_power_on(hdst, 0);
			pr_info("micbias power off for debance error\n");
			goto out;
		}

		if (pdata->irq_trigger_levels[HDST_GPIO_DET_ALL] == 1) {
			if (gpio_detect_value_current == 1)
				plug_state_current = 1;
			else
				plug_state_current = 0;
		} else {
			if (gpio_detect_value_current == 0)
				plug_state_current = 1;
			else
				plug_state_current = 0;
		}
	} else
		plug_state_current = 0;/* no debounce for plug out!!! */

	if (hdst->re_detect == true) {
		gpio_detect_value =
			gpio_get_value(pdata->gpios[HDST_GPIO_DET_ALL]);
	}

	if ((1 == plug_state_current && 0 == hdst->plug_stat_last) ||
	  (hdst->re_detect == true && 1 == gpio_detect_value)) {
		headset_type = headset_type_detect_all(hdst->gpio_det_val_last);
		headset_adc_en(0);
		switch (headset_type) {
		case HEADSET_TYPE_ERR:
			hdst->det_err_cnt++;
			pr_info("headset_type = %d detect_err_count = %d(HEADSET_TYPE_ERR) times %d\n",
				headset_type, hdst->det_err_cnt, times);
			if (times < 10)
				queue_delayed_work(hdst->det_all_work_q,
				&hdst->det_all_work, msecs_to_jiffies(2000));
			times++;
			goto out;
		case HEADSET_4POLE_NORMAL:
			pr_info("headset_type = %d (HEADSET_4POLE_NORMAL)\n",
				headset_type);
			if (pdata->gpio_switch != 0)
				gpio_direction_output(pdata->gpio_switch, 0);
			break;
		case HEADSET_4POLE_NOT_NORMAL:
			pr_info("headset_type = %d (HEADSET_4POLE_NOT_NORMAL)\n",
				headset_type);
			if (pdata->gpio_switch != 0)
				gpio_direction_output(pdata->gpio_switch, 1);
			/* Repeated detection 5 times when 3P is detected */
		case HEADSET_NO_MIC:
			pr_info("headset_type = %d (HEADSET_NO_MIC)\n",
				headset_type);
			if (times_1 < 5) {
				queue_delayed_work(hdst->det_all_work_q,
				  &hdst->det_all_work, msecs_to_jiffies(1000));
				times_1++;
				hdst->re_detect = true;
			} else {
				hdst->re_detect = false;
			}
			if (pdata->gpio_switch != 0)
				gpio_direction_output(pdata->gpio_switch, 0);
			break;
		case HEADSET_APPLE:
			pr_info("headset_type = %d (HEADSET_APPLE)\n",
				headset_type);
			pr_info("we have not yet implemented this in the code\n");
			break;
		default:
			pr_info("headset_type = %d (HEADSET_UNKNOWN)\n",
				headset_type);
			break;
		}

		times = 0;
		hdst->det_err_cnt = 0;
		if (headset_type == HEADSET_NO_MIC ||
				headset_type == HEADSET_4POLE_NOT_NORMAL)
			hdst->headphone = 1;
		else
			hdst->headphone = 0;

		hdst->plug_stat_last = 1;
		if (fast_charge_finished == false)
			headset_fast_charge(hdst);

		if (hdst->headphone) {
			headset_button_irq_threshold(0);
			headset_irq_button_enable(0, hdst->irq_button);

			hdst->hdst_status = SND_JACK_HEADPHONE;
			if (hdst->report == 0) {
				pr_debug("report for 3p\n");
				headset_jack_report(hdst, &hdst->hdst_jack,
					hdst->hdst_status, SND_JACK_HEADPHONE);
			}

			hdst->report = 1;

			pr_info("micbias power off for 3pole\n");
			if (hdst->re_detect == false)
				headmicbias_power_on(hdst, 0);
			pr_info("headphone plug in (headset_detect_work_func)\n");
		} else
			headset_process_for_4pole(headset_type);

		if (pdata->do_fm_mute)
			vbc_close_fm_dggain(false);
	} else if (0 == plug_state_current && 1 == hdst->plug_stat_last) {
		headmicbias_power_on(hdst, 0);
		times = 0;
		times_1 = 0;
		pr_info("micbias power off for plug out  times %d\n", times);

		headset_irq_button_enable(0, hdst->irq_button);
		headset_button_release_verify();

		hdst->hdst_status &= ~SPRD_HEADSET_JACK_MASK;
		headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);
		hdst->plug_stat_last = 0;
		hdst->report = 0;
		hdst->re_detect = false;

		if (hdst->headphone)
			pr_info("headphone plug out (%s)\n", __func__);
		else
			pr_info("headset plug out (%s)\n", __func__);

		/*delay 10ms*/
		sprd_msleep(10);
		/* bit10-8 change from 001 to 110 */
		msk = HEDET_LDET_REFL_SEL_MASK << HEDET_LDET_REFL_SEL;
		val = HEDET_LDET_REFL_SEL_300MV << HEDET_LDET_REFL_SEL;
		headset_reg_write(ANA_HDT1, val, msk);
		headset_reg_read(ANA_HDT1, &val);
		pr_info("ANA_HDT1 0x%04x\n", val);

		headset_reg_clr_bits(ANA_HDT0, BIT(HEDET_MICDET_EN));
		/* Close the fm in advance because of the noise when playing fm
		 * in speaker mode plugging out headset.
		 */
		if (pdata->do_fm_mute)
			vbc_close_fm_dggain(true);

		cancel_delayed_work_sync(&hdst->fc_work);
		fast_charge_finished = false;

		msleep(20);
	} else {
		times = 0;
		times_1 = 0;
		hdst->re_detect = false;
		hdst->report = 0;
		pr_info("times %d irq_detect must be enabled anyway!!\n",
			times);
		headmicbias_power_on(hdst, 0);
		pr_info("micbias power off for irq_error\n");
		goto out;
	}
out:
	headset_reg_clr_bits(ANA_HDT0, BIT(HEDET_LDET_L_FILTER));
	headset_reg_read(ANA_HDT0, &val);
	pr_info("ANA_HDT0 0x%04x\n", val);
	if (hdst->plug_stat_last == 0) {
		headset_scale_set(0);
		headmicbias_power_on(hdst, 0);
		pr_info("micbias power off end\n");
		sprd_headset_bias_control(hdst, 0);
		sprd_headset_vb_control(hdst, 1);
		headset_button_irq_threshold(0);
	}
	pr_info("%s out\n", __func__);
	up(&hdst->sem);
}

static void headset_reg_dump_func(struct work_struct *work)
{
	int adc_mic = 0;

	int gpio_detect_l = 0;
	int gpio_detect_h = 0;
	int gpio_detect_all = 0;
	int gpio_button = 0;
	int gpio_detect_mic = 0;

	unsigned int ana_sts7 = 0;
	unsigned int ana_pmu0 = 0;
	unsigned int ana_hdt0 = 0;
	unsigned int ana_hdt1 = 0;
	unsigned int ana_hdt2 = 0;
	unsigned int ana_dcl0 = 0;

	unsigned int ana_cdc2 = 0;
	unsigned int ana_cdc3 = 0;
	unsigned int ana_sts2 = 0;

	unsigned int arm_module_en = 0;
	unsigned int arm_clk_en = 0;

	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	adc_mic = headset_wrap_sci_adc_get(hdst->adc_chan);

	gpio_detect_l = gpio_get_value(pdata->gpios[HDST_GPIO_DET_L]);
	gpio_detect_h = gpio_get_value(pdata->gpios[HDST_GPIO_DET_H]);
	gpio_detect_all = gpio_get_value(pdata->gpios[HDST_GPIO_DET_ALL]);
	gpio_detect_mic = gpio_get_value(pdata->gpios[HDST_GPIO_DET_MIC]);
	gpio_button = gpio_get_value(pdata->gpios[HDST_GPIO_BUTTON]);

	if (pdata->jack_type == JACK_TYPE_NC)
		gpio_detect_mic = gpio_get_value(
			pdata->gpios[HDST_GPIO_DET_MIC]);

	sci_adi_write(ANA_REG_GLB_ARM_MODULE_EN,
		BIT_ANA_AUD_EN, BIT_ANA_AUD_EN);
	headset_reg_read(ANA_PMU0, &ana_pmu0);
	headset_reg_read(ANA_HDT0, &ana_hdt0);
	headset_reg_read(ANA_HDT1, &ana_hdt1);
	headset_reg_read(ANA_HDT2, &ana_hdt2);
	headset_reg_read(ANA_STS7, &ana_sts7);
	headset_reg_read(ANA_DCL0, &ana_dcl0);
	headset_reg_read(ANA_CDC2, &ana_cdc2);
	headset_reg_read(ANA_CDC3, &ana_cdc3);
	headset_reg_read(ANA_STS2, &ana_sts2);

	sci_adi_read(ANA_REG_GLB_ARM_MODULE_EN, &arm_module_en);
	sci_adi_read(ANA_REG_GLB_ARM_CLK_EN, &arm_clk_en);

	pr_info("GPIO_%03d(det_l)=%d GPIO_%03d(det_h)=%d GPIO_%03d(det_mic)=%d GPIO_%03d(det_all)=%d GPIO_%03d(but)=%d adc_mic=%d\n",
		pdata->gpios[HDST_GPIO_DET_L], gpio_detect_l,
		pdata->gpios[HDST_GPIO_DET_H], gpio_detect_h,
		pdata->gpios[HDST_GPIO_DET_MIC], gpio_detect_mic,
		pdata->gpios[HDST_GPIO_DET_ALL], gpio_detect_all,
		pdata->gpios[HDST_GPIO_BUTTON], gpio_button,
		adc_mic);

	pr_info("ana_pmu0  | ana_hdt0 | ana_hdt1 | ana_hdt2 | ana_sts7 | ana_dcl0\n");
	pr_info("0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X\n",
		 ana_pmu0, ana_hdt0, ana_hdt1,
		ana_hdt2, ana_sts7, ana_dcl0);

	pr_info("ana_cdc2 | ana_cdc3 | ana_sts2\n");
	pr_info("0x%08X|0x%08X|0x%08X\n", ana_cdc2, ana_cdc3, ana_sts2);

	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));
}

static irqreturn_t headset_button_irq_handler(int irq, void *dev)
{
	struct sprd_headset *hdst = dev;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int button_state_current = 0;
	int gpio_button_value_current = 0;
	unsigned int val;

	pr_info("%s in\n", __func__);
	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return IRQ_HANDLED;
	}

	if (headset_button_valid(
	    gpio_get_value(pdata->gpios[HDST_GPIO_DET_ALL])) == 0) {
		headset_reg_read(ANA_STS0, &val);
		pr_info("%s: button is invalid!!! IRQ_%d(GPIO_%d) = %d, ANA_STS0 = 0x%08X\n",
			__func__, hdst->irq_button,
			pdata->gpios[HDST_GPIO_BUTTON],
			hdst->gpio_btn_val_last, val);

		if (hdst->plug_stat_last == 0)
			headset_irq_button_enable(0, hdst->irq_button);

		return IRQ_HANDLED;
	}

	gpio_button_value_current =
		gpio_get_value(pdata->gpios[HDST_GPIO_BUTTON]);

	if (gpio_button_value_current == 1) {
		if (pdata->irq_trigger_levels[HDST_GPIO_BUTTON] == 1)
			headset_irq_set_irq_type(
				hdst->irq_button, IRQF_TRIGGER_LOW);
		else
			headset_irq_set_irq_type(
				hdst->irq_button, IRQF_TRIGGER_HIGH);
	} else {
		if (pdata->irq_trigger_levels[HDST_GPIO_BUTTON] == 1)
			headset_irq_set_irq_type(
				hdst->irq_button, IRQF_TRIGGER_HIGH);
		else
			headset_irq_set_irq_type(
				hdst->irq_button, IRQF_TRIGGER_LOW);
	}

	button_state_current =
		headset_gpio_2_button_state(gpio_button_value_current);
	if (button_state_current == hdst->btn_stat_last) {
		pr_info("button state check failed!!! maybe the release is too quick. button_state_current=%d, hdst->btn_stat_last=%d\n",
			button_state_current, hdst->btn_stat_last);
		return IRQ_HANDLED;
	}

	headset_irq_button_enable(0, hdst->irq_button);
	__pm_wakeup_event(&hdst->btn_wakelock, msecs_to_jiffies(2000));

	hdst->gpio_btn_val_last = gpio_button_value_current;
	headset_reg_read(ANA_STS7, &val);
	pr_info("%s: IRQ_%d(GPIO_%d) = %d, ANA_STS7 = 0x%08X\n",
		__func__, hdst->irq_button, pdata->gpios[HDST_GPIO_BUTTON],
		hdst->gpio_btn_val_last, val);
	pr_info("%s out\n", __func__);
	queue_delayed_work(hdst->btn_work_q, &hdst->btn_work,
		msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static irqreturn_t headset_detect_all_irq_handler(int irq, void *dev)
{
	struct sprd_headset *hdst = dev;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	unsigned int val;
	bool ret = false;

	pr_info("%s in\n", __func__);
	if (!hdst) {
		pr_err("%s hdst is NULL!\n", __func__);
		return false;
	}
	headset_irq_button_enable(0, hdst->irq_button);
	headset_irq_detect_all_enable(0, hdst->irq_detect_all);
	__pm_wakeup_event(&hdst->det_all_wakelock, msecs_to_jiffies(2000));
	hdst->gpio_det_val_last =
		gpio_get_value(pdata->gpios[HDST_GPIO_DET_ALL]);
	headset_reg_read(ANA_STS7, &val);
	pr_info("%s: IRQ_%d(GPIO_%d)(insert all) = %d, ANA_STS7 = 0x%08X\n",
		__func__,
		hdst->irq_detect_all, pdata->gpios[HDST_GPIO_DET_ALL],
		hdst->gpio_det_val_last, val);

	/*follow ASIC's device when plug out/in the headset
	 *  we need to set the ANA_STS2 to 0 for decreasing
	 *  pop noise
	 */
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN);
	headset_reg_write(ANA_STS2, 0x0, 0xffff);

	headset_reg_read(ANA_STS2, &val);
	pr_info("ANA_STS2 %#x\n", val);
#ifdef FOR_EIC_CHANGE_DET /* peng.lee debug eic change */
	if (pdata->jack_type != JACK_TYPE_NC) {
		if (hdst->gpio_det_val_last == 1) {
			if (pdata->irq_trigger_levels[HDST_GPIO_DET_ALL] == 1)
				headset_irq_set_irq_type(
					hdst->irq_detect_all, IRQF_TRIGGER_LOW);
			else
				headset_irq_set_irq_type(hdst->irq_detect_all,
							 IRQF_TRIGGER_HIGH);
		} else {
			if (pdata->irq_trigger_levels[HDST_GPIO_DET_ALL] == 1)
				headset_irq_set_irq_type(hdst->irq_detect_all,
							 IRQF_TRIGGER_HIGH);
			else
				headset_irq_set_irq_type(
					hdst->irq_detect_all, IRQF_TRIGGER_LOW);
		}
	}
#endif
	headset_irq_detect_all_enable(1, hdst->irq_detect_all);
	ret = cancel_delayed_work(&hdst->det_all_work);
	queue_delayed_work(hdst->det_all_work_q,
		&hdst->det_all_work, msecs_to_jiffies(5));
	pr_info("%s out ret %d\n", __func__, ret);
	return IRQ_HANDLED;
}

static irqreturn_t headset_detect_mic_irq_handler(int irq, void *dev)
{
	struct sprd_headset *hdst = dev;

	pr_info("%s in\n", __func__);
	if (hdst->plug_stat_last != 1) {
		headset_irq_detect_mic_enable(0, hdst->irq_detect_mic);
		pr_info("%s out0\n", __func__);
		return IRQ_HANDLED;
	}

	headset_irq_detect_mic_enable(0, hdst->irq_detect_mic);
	headset_irq_detect_all_enable(0, hdst->irq_detect_all);
	__pm_wakeup_event(&hdst->det_all_wakelock, msecs_to_jiffies(2000));
	queue_delayed_work(hdst->det_work_q,
		&hdst->det_work, msecs_to_jiffies(0));
	pr_info("%s out1\n", __func__);

	return IRQ_HANDLED;
}

/* ================= create sys fs for debug =================== */
 /* Used for getting headset type in sysfs. */
static ssize_t headset_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buff)
{
	struct sprd_headset *hdst = sprd_hdst;
	int type = 0;

	switch (hdst->hdst_status) {
	case SND_JACK_HEADSET:
		type = 1;
		break;
	case SND_JACK_HEADPHONE:
		type = 2;
		break;
	default:
		type = 0;
		break;
	}

	pr_debug("%s status: %#x, headset_state = %d\n",
		__func__, hdst->hdst_status, type);

	return sprintf(buff, "%d\n", type);
}

static ssize_t headset_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buff, size_t len)
{
	return len;
}
/* ============= /sys/kernel/headset/debug_level =============== */

static ssize_t headset_debug_level_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buff)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	pr_info("debug_level = %d\n", hdst->debug_level);

	return sprintf(buff, "%d\n", hdst->debug_level);
}

static ssize_t headset_debug_level_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buff, size_t len)
{
	struct sprd_headset *hdst = sprd_hdst;
	unsigned long level;
	int ret;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	ret = kstrtoul(buff, 10, &level);
	if (ret) {
		pr_err("%s kstrtoul failed!(%d)\n", __func__, ret);
		return len;
	}
	hdst->debug_level = level;
	pr_info("debug_level = %d\n", hdst->debug_level);
	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));

	return len;
}

static int headset_debug_sysfs_init(void)
{
	int ret = -1;
	int i;
	static struct kobject *headset_debug_kobj;
	static struct kobj_attribute headset_debug_attr[] = {
		__ATTR(debug_level, 0644,
		headset_debug_level_show,
		headset_debug_level_store),
		__ATTR(state, 0644,
		headset_state_show,
		headset_state_store),
	};

	headset_debug_kobj = kobject_create_and_add("headset", kernel_kobj);
	if (headset_debug_kobj == NULL) {
		ret = -ENOMEM;
		pr_err("register sysfs failed. ret = %d\n", ret);
		return ret;
	}

	for (i = 0; i < sizeof(headset_debug_attr) /
	     sizeof(headset_debug_attr[0]); i++) {
		ret = sysfs_create_file(headset_debug_kobj,
					&headset_debug_attr[i].attr);
		if (ret) {
			pr_err("create sysfs '%s' failed. ret = %d\n",
			       headset_debug_attr[i].attr.name, ret);
			return ret;
		}
	}

	pr_info("%s success\n", __func__);

	return ret;
}
/* ================= create sys fs for debug =================== */

void sprd_headset_set_global_variables(
	struct sprd_headset_global_vars *glb)
{
	arch_audio_codec_set_regmap(glb->regmap);
	arch_audio_codec_set_reg_offset(glb->codec_reg_offset);
}

static int headset_adc_cal_from_efuse(struct platform_device *pdev);
static int sprd_headset_probe(struct platform_device *pdev);

static struct device_node *sprd_audio_codec_get_card0_node(void)
{
	int i;
	struct device_node *np;
	const char * const comp[] = {
		"sprd,vbc-r1p0v3-codec-sc2721",
		"sprd,vbc-r1p0v3-codec-sc2731",
		"sprd,vbc-r3p0-codec-sc2731",
	};

	for (i = 0; i < ARRAY_SIZE(comp); i++) {
		np = of_find_compatible_node(
			NULL, NULL, comp[i]);
		if (np)
			return np;
	}

	return NULL;
}

static int sprd_headset_parse(struct snd_soc_card *card)
{
	struct platform_device *pdev, *h_pdev;
	struct device_node *hdst_np;
	enum of_gpio_flags flags;
	struct sprd_card_data *priv;
	struct device_node *node;
	int ret = 0;

	priv = snd_soc_card_get_drvdata(card);
	node = sprd_audio_codec_get_card0_node();
	if (!node) {
		pr_err("error, there must be a card0 node!\n");
		return -ENODEV;
	}
	pdev = of_find_device_by_node(node);
	if (unlikely(!pdev)) {
		pr_err("card0 node has no pdev?\n");
		ret = -EPROBE_DEFER;
		of_node_put(node);
		return ret;
	}

	priv->gpio_hp_det = of_get_named_gpio_flags(node,
						    "sprd-audio-card,hp-det-gpio",
						    0, &flags);
	priv->gpio_hp_det_invert = !!(flags & OF_GPIO_ACTIVE_LOW);
	if (priv->gpio_hp_det == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	priv->gpio_mic_det = of_get_named_gpio_flags(node,
						     "sprd-audio-card,mic-det-gpio",
						     0, &flags);
	priv->gpio_mic_det_invert = !!(flags & OF_GPIO_ACTIVE_LOW);
	if (priv->gpio_mic_det == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if (priv->gpio_hp_det >= 0)
		return 0;

	/* Sprd headset */
	hdst_np = of_parse_phandle(node, "sprd-audio-card,headset", 0);
	if (hdst_np) {
		h_pdev = of_find_device_by_node(hdst_np);
		if (unlikely(!h_pdev)) {
			pr_err("headset node has no pdev?\n");
			return -EPROBE_DEFER;
		}

		ret = sprd_headset_probe(h_pdev);
		if (ret < 0) {
			if (ret == -EPROBE_DEFER)
				return ret;
			pr_err("sprd_headset_probe failed, ret %d\n", ret);
		}
	} else {
		pr_err("parse 'sprd-audio-card,headset' failed!\n");
	}

	return ret;
}

int sprd_headset_soc_probe(struct snd_soc_codec *codec)
{
	int ret, i;
	struct sprd_headset *hdst;
	struct platform_device *pdev;
	struct sprd_headset_platform_data *pdata;
	struct device *dev; /* digiatal part device */
	unsigned int adie_chip_id = 0;
	unsigned long irqflags = 0;
	struct snd_soc_card *card;

	if (!codec) {
		pr_err("%s codec NULL\n", __func__);
		return -EINVAL;
	}
	if (!codec->dev) {
		pr_err("%s codec->dev NULL\n", __func__);
		return -EINVAL;
	}
	if (!codec->component.card) {
		pr_err("%s codec->component.card NULL\n", __func__);
		return -EINVAL;
	}
	dev = codec->dev;
	card = codec->component.card;

	ret = sprd_headset_parse(card);
	if (ret) {
		pr_err("sprd_headset_parse fail %d\n", ret);
		return ret;
	}
	hdst = sprd_hdst;
	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -EINVAL;
	}
	pdev = hdst->pdev;
	pdata = &hdst->pdata;
	if (!pdev || !pdata) {
		pr_err("%s pdev %p, pdata %p\n", __func__, pdev, pdata);
		return -EINVAL;
	}

	hdst->codec = codec;

	adie_chip_id = sci_get_ana_chip_id();
	pr_info("adie chip is 0x%x, 0x%x\n", (adie_chip_id >> 16) & 0xFFFF,
	adie_chip_id & 0xFFFF);

	headset_detect_reg_init();

	ret = sprd_headset_power_init(hdst);
	if (ret) {
		pr_err("sprd_headset_power_init failed\n");
		return ret;
	}

	ret = snd_soc_card_jack_new(card, "Headset Jack",
		SPRD_HEADSET_JACK_MASK, &hdst->hdst_jack, NULL, 0);
	if (ret) {
		pr_err("Failed to create headset jack\n");
		return ret;
	}

	ret = snd_soc_card_jack_new(card, "Headset Keyboard",
		SPRD_BUTTON_JACK_MASK, &hdst->btn_jack, NULL, 0);
	if (ret) {
		pr_err("Failed to create button jack\n");
		return ret;
	}

	if (pdata->nbuttons > MAX_BUTTON_NUM) {
		pr_warn("button number in dts is more than %d!\n",
			MAX_BUTTON_NUM);
		pdata->nbuttons = MAX_BUTTON_NUM;
	}
	for (i = 0; i < pdata->nbuttons; i++) {
		struct headset_buttons *buttons =
			&pdata->headset_buttons[i];

		ret = snd_jack_set_key(hdst->btn_jack.jack,
			headset_jack_type_get(i), buttons->code);
		if (ret) {
			pr_err("%s: Failed to set code for btn-%d\n",
				__func__, i);
			return ret;
		}
	}

	if (pdata->gpio_switch != 0)
		gpio_direction_output(pdata->gpio_switch, 0);
	gpio_direction_input(pdata->gpios[HDST_GPIO_DET_L]);
	gpio_direction_input(pdata->gpios[HDST_GPIO_DET_H]);
	gpio_direction_input(pdata->gpios[HDST_GPIO_DET_MIC]);
	gpio_direction_input(pdata->gpios[HDST_GPIO_DET_ALL]);
	gpio_direction_input(pdata->gpios[HDST_GPIO_BUTTON]);

	hdst->irq_detect_l = gpio_to_irq(pdata->gpios[HDST_GPIO_DET_L]);
	hdst->irq_detect_h = gpio_to_irq(pdata->gpios[HDST_GPIO_DET_H]);
	hdst->irq_detect_mic = gpio_to_irq(pdata->gpios[HDST_GPIO_DET_MIC]);
	hdst->irq_detect_all = gpio_to_irq(pdata->gpios[HDST_GPIO_DET_ALL]);
	hdst->irq_button = gpio_to_irq(pdata->gpios[HDST_GPIO_BUTTON]);
	#if 0
	if (pdata->jack_type == JACK_TYPE_NC) {
		gpio_direction_input(pdata->gpios[HDST_GPIO_DET_MIC]);
		hdst->irq_detect_mic =
			gpio_to_irq(pdata->gpios[HDST_GPIO_DET_MIC]);
	}
	#endif
	sema_init(&hdst->sem, 1);

	INIT_DELAYED_WORK(&hdst->btn_work, headset_button_work_func);
	hdst->btn_work_q = create_singlethread_workqueue("headset_button");
	if (hdst->btn_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_button failed!\n");
		goto failed_to_micbias_power_off;
	}

	INIT_DELAYED_WORK(&hdst->det_all_work, headset_detect_all_work_func);
	hdst->det_all_work_q =
		create_singlethread_workqueue("headset_detect_all");
	if (hdst->det_all_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_detect failed!\n");
		goto failed_to_headset_detect;
	}

	INIT_DELAYED_WORK(&hdst->reg_dump_work, headset_reg_dump_func);
	hdst->reg_dump_work_q =
		create_singlethread_workqueue("headset_reg_dump");
	if (hdst->reg_dump_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_reg_dump failed!\n");
		goto failed_to_headset_reg_dump;
	}
	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));

	INIT_DELAYED_WORK(&hdst->fc_work, headset_fc_work_func);

	wakeup_source_init(&hdst->det_all_wakelock, "headset_detect_wakelock");
	wakeup_source_init(&hdst->btn_wakelock, "headset_button_wakelock");

	mutex_init(&hdst->irq_btn_lock);
	mutex_init(&hdst->irq_det_all_lock);
	mutex_init(&hdst->irq_det_mic_lock);

	for (i = 0; i < HDST_GPIO_MAX; i++) {
		#if 0
		if (pdata->jack_type == JACK_TYPE_NC) {
			if (i == HDST_GPIO_DET_MIC)
				continue;
		}
		#endif
		gpio_set_debounce(pdata->gpios[i], pdata->dbnc_times[i] * 1000);
	}

	irqflags = pdata->irq_trigger_levels[HDST_GPIO_BUTTON] ?
		IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
	ret = devm_request_threaded_irq(
		dev, hdst->irq_button, NULL, headset_button_irq_handler,
		irqflags | IRQF_NO_SUSPEND | IRQF_ONESHOT, "headset_button", hdst);
	if (ret) {
		pr_err("failed to request IRQ_%d(GPIO_%d)\n",
			hdst->irq_button, pdata->gpios[HDST_GPIO_BUTTON]);
		goto failed_to_request_irq;
	}
	/* Disable button irq before headset detected. */
	headset_irq_button_enable(0, hdst->irq_button);

	irq_set_status_flags(hdst->irq_detect_all, IRQ_DISABLE_UNLAZY);

	irqflags = pdata->irq_trigger_levels[HDST_GPIO_DET_ALL] ?
		IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
	ret = devm_request_threaded_irq(
		dev, hdst->irq_detect_all, NULL, headset_detect_all_irq_handler,
		irqflags | IRQF_NO_SUSPEND | IRQF_ONESHOT, "headset_detect", hdst);
	if (ret < 0) {
		pr_err("failed to request IRQ_%d(GPIO_%d)\n",
			hdst->irq_detect_all, pdata->gpios[HDST_GPIO_DET_ALL]);
		goto failed_to_request_detect_irq;
	}

	if (pdata->jack_type == JACK_TYPE_NC) {
		irqflags = pdata->irq_trigger_levels[HDST_GPIO_DET_MIC] ?
			IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
		ret = devm_request_threaded_irq(dev, hdst->irq_detect_mic, NULL,
			headset_detect_mic_irq_handler,
			irqflags | IRQF_NO_SUSPEND | IRQF_ONESHOT, "headset_detect_mic", hdst);
		if (ret < 0) {
			pr_err("failed to request IRQ_%d(GPIO_%d)\n",
				hdst->irq_detect_mic,
				pdata->gpios[HDST_GPIO_DET_MIC]);
			goto failed_to_request_detect_mic_irq;
		}
	}
	headset_debug_sysfs_init();
	headset_adc_cal_from_efuse(hdst->pdev);
	sprd_headset_power_regulator_init(hdst);

	return 0;

failed_to_request_detect_mic_irq:
	devm_free_irq(dev, hdst->irq_detect_all, hdst);
failed_to_request_detect_irq:
	devm_free_irq(dev, hdst->irq_button, hdst);
failed_to_request_irq:
	cancel_delayed_work_sync(&hdst->reg_dump_work);
	destroy_workqueue(hdst->reg_dump_work_q);
failed_to_headset_reg_dump:
	destroy_workqueue(hdst->det_work_q);
failed_to_headset_detect:
	destroy_workqueue(hdst->btn_work_q);
failed_to_micbias_power_off:
	headmicbias_power_on(hdst, 0);

	return ret;
}

static struct gpio_map {
	int type;
	const char *name;
} gpio_map[] = {
	{HDST_GPIO_DET_L, "detect_l"},
	{HDST_GPIO_DET_H, "detect_h"},
	{HDST_GPIO_DET_MIC, "detect_mic"},
	{HDST_GPIO_DET_ALL, "detect_all"},
	{HDST_GPIO_BUTTON, "button"},
	{0, NULL},
};

#ifdef CONFIG_OF
static int sprd_headset_parse_dt(struct sprd_headset *hdst)
{
	int ret = 0;
	struct sprd_headset_platform_data *pdata;
	struct device_node *np, *buttons_np = NULL;
	struct headset_buttons *buttons_data;
	u32 val = 0;
	int index;
	int i = 0;

	if (!hdst) {
		pr_err("%s sprd_hdst is NULL!\n", __func__);
		return -EINVAL;
	}

	np = hdst->pdev->dev.of_node;
	if (!np) {
		pr_err("%s No device node for headset!\n", __func__);
		return -ENODEV;
	}

	/* Parse configs for headset & button detecting. */
	pdata = &hdst->pdata;
	ret = of_property_read_u32(np, "jack-type", &val);
	if (ret < 0) {
		pr_err("%s: parse 'jack-type' failed!\n", __func__);
		pdata->jack_type = JACK_TYPE_NO;
	}
	pdata->jack_type = val ? JACK_TYPE_NC : JACK_TYPE_NO;

	/* Parse configs for whether speaker use headset path. */
	pdata->hpr_spk = of_property_read_bool(np, "sprd,spk-route-hp");

	/* Parse gpios. */
	/* Parse for the gpio of EU/US jack type switch. */
	index = of_property_match_string(np, "gpio-names", "switch");
	if (index < 0) {
		pr_info("%s :no match found for switch gpio.\n", __func__);
		pdata->gpio_switch = 0;
	} else {
		ret = of_get_gpio_flags(np, index, NULL);
		if (ret < 0) {
			pr_err("%s :get gpio for 'switch' failed!\n", __func__);
			return -ENXIO;
		}
		pdata->gpio_switch = (u32)ret;
	}

	/* Parse for detecting gpios. */
	for (i = 0; gpio_map[i].name; i++) {
		const char *name = gpio_map[i].name;
		int type = gpio_map[i].type;

		index = of_property_match_string(np, "gpio-names", name);
		if (index < 0) {
			pr_err("%s :no match found for '%s' gpio\n",
			       __func__, name);
			return -ENXIO;
		}

		ret = of_get_gpio_flags(np, index, NULL);
		if (ret < 0) {
			pr_err("%s :get gpio for '%s' failed!\n",
			       __func__, name);
			return -ENXIO;
		}
		pdata->gpios[type] = (u32)ret;

		ret = of_property_read_u32_index(
			np, "gpio-trigger-levels", index, &val);
		if (ret < 0) {
			pr_err("%s :get trigger level for '%s' failed!\n",
				__func__, name);
			return ret;
		}
		pdata->irq_trigger_levels[type] = val;

		ret = of_property_read_u32_index(
			np, "gpio-dbnc-intervals", index, &val);
		if (ret < 0) {
			pr_err("%s :get debounce inteval for '%s' failed!\n",
				__func__, name);
			return ret;
		}
		pdata->dbnc_times[type] = val;

		pr_info("use GPIO_%u for '%s', trigr level: %u, debounce: %u\n",
			pdata->gpios[type],
			name,
			pdata->irq_trigger_levels[type],
			pdata->dbnc_times[type]);
	}

	/* Parse for insert detecting gpio. */
	#if 0
	if (pdata->jack_type == JACK_TYPE_NC) {
		index = of_property_match_string(
			np, "gpio-names", "detect_mic");
		if (index < 0) {
			pr_err("%s :no match found for detect_mic gpio\n",
				__func__);
			return -ENXIO;
		}
		ret = of_get_gpio_flags(np, index, NULL);
		if (ret < 0) {
			pr_err("%s :get gpio for 'detect_mic' failed!\n",
				__func__);
			return -ENXIO;
		}
		pdata->gpios[HDST_GPIO_DET_MIC] = (u32)ret;
		ret = of_property_read_u32_index(
			np, "gpio-trigger-levels", index, &val);
		if (ret < 0) {
			pr_err("%s :get trigger level for 'detect_mic' failed!\n",
				__func__);
			return ret;
		}
		pdata->irq_trigger_levels[HDST_GPIO_DET_MIC] = val;
		ret = of_property_read_u32_index(
			np, "gpio-dbnc-intervals", index, &val);
		if (ret < 0) {
			pr_err("%s :get debounce inteval for 'detect_mic' failed!\n",
				__func__);
			return ret;
		}
		pdata->dbnc_times[HDST_GPIO_DET_MIC] = val;
		pr_debug("use GPIO_%u for mic detecting, trig level: %u, debounce: %u\n",
			pdata->gpios[HDST_GPIO_DET_MIC],
			pdata->irq_trigger_levels[HDST_GPIO_DET_MIC],
			pdata->dbnc_times[HDST_GPIO_DET_MIC]);
	}
	#endif
	ret = of_property_read_u32(np, "adc-threshold-3pole-detect",
		&pdata->adc_threshold_3pole_detect);
	if (ret) {
		pr_err("%s: fail to get adc-threshold-3pole-detect\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(np, "sprd,adc-gnd",
		&pdata->sprd_adc_gnd);
	if (ret) {
		pr_err("%s: fail to get sprd-adc-gnd\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(np, "sprd,stable-value",
		&pdata->sprd_stable_value);
	if (ret) {
		pr_err("%s: fail to get stable-value\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(np, "sprd,coefficient",
		&pdata->coefficient);
	if (ret) {
		pr_err("%s: fail to get sprd-coefficient\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(
		np, "irq-threshold-button", &pdata->irq_threshold_button);
	if (ret) {
		pr_err("%s: fail to get irq-threshold-button\n", __func__);
		return -ENXIO;
	}

	pdata->sprd_half_adc_gnd = pdata->sprd_adc_gnd >> 1;
	pdata->sprd_one_half_adc_gnd = pdata->sprd_adc_gnd +
					pdata->sprd_half_adc_gnd;
	pr_info("half_adc_gnd=%u,one_half_adc_gnd=%u",
		pdata->sprd_half_adc_gnd, pdata->sprd_one_half_adc_gnd);

	pdata->do_fm_mute = !of_property_read_bool(np, "sprd,no-fm-mute");

	/* Parse for buttons */
	pdata->nbuttons = of_get_child_count(np);
	buttons_data = devm_kzalloc(&hdst->pdev->dev,
		pdata->nbuttons*sizeof(*buttons_data), GFP_KERNEL);
	if (!buttons_data)
		return -ENOMEM;
	pdata->headset_buttons = buttons_data;

	for_each_child_of_node(np, buttons_np) {
		ret = of_property_read_u32(
			buttons_np, "adc-min", &buttons_data->adc_min);
		if (ret) {
			pr_err("%s: fail to get adc-min\n", __func__);
			return ret;
		}
		ret = of_property_read_u32(
			buttons_np, "adc-max", &buttons_data->adc_max);
		if (ret) {
			pr_err("%s: fail to get adc-min\n", __func__);
			return ret;
		}
		ret = of_property_read_u32(
			buttons_np, "code", &buttons_data->code);
		if (ret) {
			pr_err("%s: fail to get code\n", __func__);
			return ret;
		}
		pr_info("device tree data: adc_min = %d adc_max = %d code = %d\n",
			buttons_data->adc_min,
			buttons_data->adc_max, buttons_data->code);
		buttons_data++;
	}

	return 0;
}
#endif

/* Note: @pdev is the platform_device of headset node in dts. */
static int sprd_headset_probe(struct platform_device *pdev)
{
	struct sprd_headset *hdst = NULL;
	struct sprd_headset_platform_data *pdata;
	int ret = -1;
	struct device *dev = (pdev ? &pdev->dev : NULL);
	int i;

#ifndef CONFIG_OF
	pr_err("%s: Only OF configurations are supported yet!\n", __func__);
	return -1;
#endif
	if (!pdev) {
		pr_err("%s: platform device is NULL!\n", __func__);
		return -1;
	}

	hdst = devm_kzalloc(dev, sizeof(*hdst), GFP_KERNEL);
	if (!hdst)
		return -ENOMEM;

	hdst->pdev = pdev;
	ret = sprd_headset_parse_dt(hdst);
	if (ret < 0) {
		pr_err("Failed to parse dt for headset.\n");
		return ret;
	}
	pdata = &hdst->pdata;

	if (pdata->gpio_switch != 0) {
		ret = devm_gpio_request(dev,
			pdata->gpio_switch, "headset_switch");
		if (ret < 0) {
			pr_err("failed to request GPIO_%d(headset_switch)\n",
				pdata->gpio_switch);
			return ret;
		}
	} else
		pr_info("automatic EU/US type switch is unsupported\n");

	for (i = 0; gpio_map[i].name; i++) {
		const char *name = gpio_map[i].name;
		int type = gpio_map[i].type;

		ret = devm_gpio_request(dev, pdata->gpios[type], name);
		if (ret < 0) {
			pr_err("failed to request GPIO_%d(%s)\n",
				pdata->gpios[type], name);
			return ret;
		}
	}

	#if 0
	if (pdata->jack_type == JACK_TYPE_NC) {
		ret = devm_gpio_request(dev,
			pdata->gpios[HDST_GPIO_DET_MIC], "headset_detect_mic");
		if (ret < 0) {
			pr_err("failed to request GPIO_%d(headset_detect_mic)\n",
				pdata->gpios[HDST_GPIO_DET_MIC]);
			return ret;
		}
	}
	#endif

	/* Get the adc channels of headset. */
	hdst->adc_chan = iio_channel_get(dev, "headmic_in_little");
	if (IS_ERR(hdst->adc_chan)) {
		pr_err("failed to get headmic in adc channel!\n");
		return PTR_ERR(hdst->adc_chan);
	}
	hdst->report = 0;
	sprd_hdst = hdst;

	pr_info("headset_detect_probe success\n");

	return 0;
}

static int headset_adc_get_ideal(u32 adc_mic, u32 coefficient, bool big_scale)
{
	u64 numerator = 0;
	u64 denominator = 0;
	u32 adc_ideal = 0;
	u32 a, b, e1, e2;
	int64_t exp1, exp2, exp3, exp4;
	u64 dividend;
	u32 divisor;

	if (adc_cal_headset.cal_type != SPRD_HEADSET_AUXADC_CAL_DO) {
		pr_warn("efuse A,B,E hasn't been calculated!\n");
		return adc_mic;
	}

	a = adc_cal_headset.A;
	b = adc_cal_headset.B;
	e1 = adc_cal_headset.E1;
	e2 = adc_cal_headset.E2;

	if (9*adc_mic + b < 10*a)
		return adc_mic;

	/*1.22v new calibration need*/
	exp1 =  ((int64_t)e1 - (int64_t)e2);
	exp2 = (9 * (int64_t)adc_mic - 10 * (int64_t)a + (int64_t)b);
	exp3 = 24 * (int64_t)e1 * ((int64_t)b - (int64_t)a);
	exp4 = ((int64_t)e1 - (int64_t)e2 - 1200) * exp2;

	pr_debug("exp1=%lld, exp2=%lld, exp3=%lld, exp4=%lld\n",
		exp1, exp2, exp3, exp4);
	if (big_scale)
		denominator = exp3 + 4 * exp4;
	else
		denominator = exp3 + exp4;
	numerator = coefficient * (exp1 + 1200) * exp2;
	pr_debug("denominator=%lld, numerator=%lld\n",
			denominator, numerator);
	do_div(numerator, 100);
	pr_debug("denominator=%lld, numerator=%lld\n",
			denominator, numerator);
	/*enable the denominator * 0.01
	 *  numerator * 0.01 at the same time
	 *  for do_div() argument divisor is u32
	 *  and dividend is u64 but divsor is over 32bit
	 */
	do_div(denominator, 100);
	do_div(numerator, 100);
	pr_debug("denominator=%lld, numerator=%lld\n",
			denominator, numerator);

	divisor = (u32)(denominator);
	dividend = numerator;
	pr_info("divisor=%u, dividend=%llu\n", divisor, dividend);

	do_div(dividend, divisor);
	adc_ideal = (u32)dividend;
	pr_info("adc_mic=%d, adc_ideal=%d\n", adc_mic, adc_ideal);

	return adc_ideal;
}

#define DELTA1_BLOCK20 20
#define DELTA2_BLOCK22 22
#define BITCOUNT 16
#define BLK_WIDTH 16
#define PROTECT_BIT (0)

static int sprd_pmic_efuse_bits_read(struct platform_device *pdev,
					const char *cell_name, u32 *data)
{
	struct nvmem_cell *cell;
	u32 calib_data;
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

static int headset_adc_cal_from_efuse(struct platform_device *pdev)
{
	u8 delta[4] = {0};
	u32 block0_bit7 = 128;
	u32 test[2] = {0};
	u32 data;
	int ret;
	unsigned int adie_chip_id;

	pr_info("to get calibration data from efuse ...\n");
	if (adc_cal_headset.cal_type != SPRD_HEADSET_AUXADC_CAL_NO) {
		pr_info("efuse A,B,E has been calculated already!\n");
		return -EINVAL;
	}

	ret = sprd_pmic_efuse_bits_read(pdev, "auxadc", &data);
	if (ret)
		goto adc_cali_error;
	test[0] = data;
	ret = sprd_pmic_efuse_bits_read(pdev, "headmic", &data);
	if (ret)
		goto adc_cali_error;
	test[1] = data;

	delta[0] = test[0] & 0xFF;
	delta[1] = (test[0] & 0xFF00) >> 8;
	delta[2] = test[1] & 0xFF;
	delta[3] = (test[1] & 0xFF00) >> 8;

	pr_info("test[0] 0x%x %d, test[1] 0x%x %d\n",
		test[0], test[0], test[1], test[1]);

	pr_info("d[0] %#x %d d[1] %#x %d d[2] %#x %d d[3] %#x %d\n",
			delta[0], delta[0], delta[1], delta[1],
			delta[2], delta[2],  delta[3], delta[3]);

	ret = sprd_pmic_efuse_bits_read(pdev, "protectbit", &data);
	if (ret)
		goto adc_cali_error;
	block0_bit7 = data;
	pr_info("block_7 0x%08x\n", block0_bit7);
	if (!(block0_bit7&(1<<PROTECT_BIT))) {
		pr_info("block 0 bit 7 set 1 no efuse data\n");
		return -EINVAL;
	}

	adc_cal_headset.cal_type = SPRD_HEADSET_AUXADC_CAL_DO;
	pr_info("block 0 bit 7 set 0 have efuse data\n");
	adie_chip_id = sci_get_ana_chip_id();
	adie_chip_id = (adie_chip_id >> 16) & 0xffff;
	if (adie_chip_id == CHIP_ID_2720) {
		adc_cal_headset.A = (delta[0] - 128 + 80) * 4;
		adc_cal_headset.B =  (delta[1] - 128 + 833) * 4;
		adc_cal_headset.E1 = delta[2] * 2 + 2500;
		adc_cal_headset.E2 = delta[3] * 4 + 1300;
	} else {
		adc_cal_headset.A = (delta[0] - 128) * 4 + 336;
		adc_cal_headset.B =  (delta[1] - 128) * 4 + 3357;
		adc_cal_headset.E1 = delta[2] * 2 + 2500;
		adc_cal_headset.E2 = delta[3] * 4 + 1300;
	}
	pr_info("A %d, B %d E1 %d E2 %d\n",
		adc_cal_headset.A, adc_cal_headset.B,
		adc_cal_headset.E1, adc_cal_headset.E2);

	return 0;
adc_cali_error:
	adc_cal_headset.cal_type = SPRD_HEADSET_AUXADC_CAL_NO;
	pr_err("%s, error: headset adc calibration fail %d\n", __func__, ret);
	return ret;

}

MODULE_DESCRIPTION("headset & button detect driver v2");
MODULE_AUTHOR("Yaochuan Li <yaochuan.li@spreadtrum.com>");
MODULE_LICENSE("GPL");
