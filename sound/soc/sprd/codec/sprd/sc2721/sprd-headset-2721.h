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

#ifndef __HEADSET_SPRD_2721_H__
#define __HEADSET_SPRD_2721_H__
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_wakeup.h>
#include <sound/jack.h>
#include <sound/soc.h>

/**********************************************
 * The micro ADPGAR_BYP_SELECT is used for avoiding the Bug#298417,
 * please refer to Bug#298417 to confirm your configuration.
 **********************************************/
#define ADPGAR_BYP_SELECT

enum {
	BIT_HEADSET_OUT = 0,
	BIT_HEADSET_MIC = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};

struct headset_buttons {
	u32 adc_min;
	u32 adc_max;
	u32 code;
};

enum {
	HDST_GPIO_DET_L = 0,
	HDST_GPIO_DET_H,
	HDST_GPIO_DET_MIC,
	HDST_GPIO_DET_ALL,
	HDST_GPIO_BUTTON,
	HDST_GPIO_MAX
};

enum {
	JACK_TYPE_NO = 0, /* Normal-open jack */
	JACK_TYPE_NC, /* Normal-close jack */
	JACK_TYPE_MAX
};

struct sprd_headset_platform_data {
	u32 gpio_switch;
	u32 jack_type;
	u32 gpios[HDST_GPIO_MAX];
	u32 dbnc_times[HDST_GPIO_MAX]; /* debounce times */
	u32 irq_trigger_levels[HDST_GPIO_MAX];
	u32 adc_threshold_3pole_detect;
	u32 irq_threshold_button;
	u32 voltage_headmicbias;
	u32 sprd_adc_gnd;
	u32 sprd_half_adc_gnd;
	u32 sprd_one_half_adc_gnd;
	u32 sprd_stable_value;
	u32 coefficient;
	struct headset_buttons *headset_buttons;
	u32 nbuttons;
	int (*external_headmicbias_power_on)(int);
	bool do_fm_mute;
	bool hpr_spk;
};

struct sprd_headset_power {
	struct regulator *head_mic;
	struct regulator *vcom_buf;
	struct regulator *vbo;
	struct regulator *bg;
	struct regulator *bias;
	struct regulator *vb;
};

struct sprd_headset {
	int headphone;
	int irq_detect;
	int irq_button;
	int irq_detect_l;
	int irq_detect_h;
	int irq_detect_mic;
	int irq_detect_all;
	struct platform_device *pdev;
	struct sprd_headset_platform_data pdata;
	struct delayed_work det_work;
	struct workqueue_struct *det_work_q;
	struct delayed_work det_all_work;
	struct workqueue_struct *det_all_work_q;
	struct delayed_work btn_work;
	struct workqueue_struct *btn_work_q;
	struct snd_soc_codec *codec;
	struct sprd_headset_power power;
	struct semaphore sem;
	struct snd_soc_jack hdst_jack;
	struct snd_soc_jack btn_jack;
	enum snd_jack_types hdst_status;
	enum snd_jack_types btns_pressed;
	struct iio_channel *adc_chan;
	struct mutex irq_btn_lock;
	struct mutex irq_det_lock;
	struct mutex irq_det_all_lock;
	struct mutex irq_det_mic_lock;
	struct delayed_work reg_dump_work;
	struct workqueue_struct *reg_dump_work_q;
	struct delayed_work fc_work; /* for fast charge */
#ifdef ADPGAR_BYP_SELECT
	/* used for adpgar bypass selecting. */
	struct delayed_work adpgar_work;
	struct workqueue_struct *adpgar_work_q;
#endif
	int debug_level;
	int det_err_cnt; /* detecting error count */
	int gpio_det_val_last; /* detecting gpio last value */
	int gpio_btn_val_last; /* button detecting gpio last value */

	int btn_stat_last; /* 0==released, 1==pressed */
	/* if the hardware detected a headset is
	 * plugged in, set plug_state_last = 1
	 */
	int plug_stat_last;
	int report;
	bool re_detect;
	struct wakeup_source det_wakelock;
	struct wakeup_source det_all_wakelock;
	struct wakeup_source btn_wakelock;
};

struct sprd_headset_global_vars {
	struct regmap *regmap;
	unsigned long codec_reg_offset;
};

void sprd_headset_set_global_variables(struct sprd_headset_global_vars *glb);
int sprd_headset_soc_probe(struct snd_soc_codec *codec);
int headset_register_notifier(struct notifier_block *nb);
int headset_unregister_notifier(struct notifier_block *nb);
int headset_get_plug_state(void);
void sprd_headset_power_deinit(void);

#if defined(CONFIG_SND_SOC_SPRD_CODEC_SC2721)
int headset_fast_charge_finished(void);
#else
static inline int headset_fast_charge_finished(void)
{
	return 1;
}
#endif

#endif
