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

#ifndef __HEADSET_SPRD__2730_H__
#define __HEADSET_SPRD__2730_H__
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#if defined(CONFIG_SND_SOC_SPRD_CODEC_SC2730)
#include <linux/pm_wakeup.h>
#define HDST_REGULATOR_COUNT 11
#else
#include <linux/wakelock.h>
#endif
#include <sound/jack.h>
#include <sound/soc.h>

/**********************************************
 * The micro ADPGAR_BYP_SELECT is used for avoiding the Bug#298417,
 * please refer to Bug#298417 to confirm your configuration.
 **********************************************/
/* #define ADPGAR_BYP_SELECT */
#define TO_STRING(e) #e
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
	HDST_GPIO_AUD_DET_INT_ALL = 0,
	HDST_GPIO_AUD_MAX
};

/* define this according to ANA_INT8 */
enum hdst_eic_type {
	HDST_INSERT_ALL_EIC = 10,
	HDST_MDET_EIC,
	HDST_LDETL_EIC,
	HDST_LDETH_EIC,
	HDST_GDET_EIC,
	HDST_BDET_EIC,
	HDST_ALL_EIC
};

/* define this according to ANA_STS0 */
enum hdst_insert_signal {
	HDST_INSERT_MDET = 10,
	HDST_INSERT_LDETL,
	HDST_INSERT_LDETH,
	HDST_INSERT_GDET,
	HDST_INSERT_BDET,
	HDST_INSERT_ALL,
	HDST_INSERT_MAX
};

/* define 15 channels interrupt controller according to ANA_INT33 */
enum intc_type {
	INTC_FGU_HIGH = 0,
	INTC_FGU_LOW,
	INTC_IMPD_BIST_DONE,
	INTC_IMPD_CHARGE,
	INTC_IMPD_DISCHARGE,
	INTC_HPL_DPOP,
	INTC_HPR_DPOP = 6,
	INTC_REV_DPOP,
	INTC_CFGA_EAR,
	INTC_CFGA_PA,
	INTC_CFGA_HPL,
	INTC_CFGA_HPR,
	INTC_PACAL_DONE,
	INTC_PADPOP_DONE,
	INTC_ALL_ANALOG = 14,
	INTC_MAX
};

enum {
	JACK_TYPE_NO = 0, /* Normal-open jack */
	JACK_TYPE_NC, /* Normal-close jack */
	JACK_TYPE_MAX
};

struct sprd_headset_platform_data {
	u32 gpio_switch;
	u32 jack_type;
#if defined(CONFIG_SND_SOC_SPRD_CODEC_SC2730)
	u32 gpios[HDST_GPIO_AUD_MAX];
	u32 dbnc_times[HDST_GPIO_AUD_MAX]; /* debounce times */
	u32 irq_trigger_levels[HDST_GPIO_AUD_MAX];
	u32 eu_us_switch;
	u32 threshold_3pole;/* adc threshold value of 3pole headset  */
#else
	u32 gpios[HDST_GPIO_MAX];
	u32 dbnc_times[HDST_GPIO_MAX]; /* debounce times */
	u32 irq_trigger_levels[HDST_GPIO_MAX];
#endif
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
	bool support_typec_hdst;/* support typec analog headset */
	struct gpio_desc *typec_mic_gpio;
	struct gpio_desc *typec_lr_gpio;
	struct regulator *switch_regu;
	u32 switch_vol;
};

struct sprd_headset_power {
	struct regulator *head_mic;
	struct regulator *vcom_buf;
	struct regulator *vbo;
	struct regulator *bg;
	struct regulator *bias;
	struct regulator *vb;
};

enum headset_hw_status {
	HW_LDETL_PLUG_OUT,
	HW_INSERT_ALL_PLUG_OUT,
	HW_LDETL_PLUG_IN,
	HW_INSERT_ALL_PLUG_IN,
	HW_MDET,
	HW_BTN_PRESS,
	HW_BTN_RELEASE
};

enum headset_eic_type {
	LDETL_PLUGIN = 0x1,
	INSERT_ALL_PLUGOUT = 0x2,
	MDET_EIC = 0x4,
	BDET_EIC = 0x8,
	TYPE_RE_DETECT = 0x10,
	BTN_PRESS = 0x20,
	BTN_RELEASE = 0x40,
	INSERT_ALL_PLUGIN = 0x80,
	LDETL_PLUGOUT = 0x100,
	EIC_TYPE_MAX
};

enum headset_retrun_val {
	RET_PLUGERR = -1,
	RET_PLUGIN = 1,
	RET_PLUGOUT = 0,
	RET_DEBOUN_ERR = -2,
	RET_NOERROR = 0,
	RET_MIS_ERR = -3,
	RET_NO_HDST_IRQ = -4,
	RET_BTN_PRESS = 5,
	RET_BTN_RELEASE = 6,
	RET_BTN_INVALID = -7,
	RET_LDETL_INVALID = -8,
	RET_OTHER_ERR = -9
};

struct headset_power {
	char *name;
	u32 index;
	bool en;
	struct regulator *hdst_regu;
};

struct headset_power_manager {
	struct headset_power power[HDST_REGULATOR_COUNT];
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
	struct completion wait_insert_all;
	struct completion wait_mdet;
	struct completion wait_ldetl;
	bool current_polling_state;
	struct  headset_power_manager power_manager;
	struct delayed_work btn_work;
	enum headset_hw_status hdst_hw_status;
	enum snd_jack_types hdst_type_status;
	struct wakeup_source hdst_detect_wakelock;
	enum headset_eic_type eic_type;
	bool audio_on;
	bool btn_detecting;
	/*
	 * Add mutex to protect member audio_on and btn_detecting,
	 * because both codec and headset driver will write or access
	 * them. Mode of hmicbias and polling can be enabled only when
	 * audio is off and button is not in detecting.
	 */
	struct mutex audio_on_lock;
	struct mutex btn_detecting_lock;
	int det_3pole_cnt;/* re-check times after recognize a 3 pole headset */
	bool mdet_tried;/* tried to wait mdet eic */
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
#ifdef ADPGAR_BYP_SELECT
	/* used for adpgar bypass selecting. */
	struct delayed_work adpgar_work;
	struct workqueue_struct *adpgar_work_q;
#endif
	int debug_level;
	int det_err_cnt; /* re-check times for a error type headset */
	int gpio_det_val_last; /* detecting gpio last value */
	int gpio_btn_val_last; /* button detecting gpio last value */

	int btn_stat_last; /* 0==released, 1==pressed */
	/*
	 * if the hardware detected a headset is
	 * plugged in, set plug_state_last = 1
	 */
	int plug_stat_last;
	int report;/* headset type has reported by input event */
	bool re_detect;
	int irq_detect_int_all;
	struct delayed_work det_mic_work;
	struct workqueue_struct *det_mic_work_q;
	struct delayed_work ldetl_work;
	struct workqueue_struct *ldetl_work_q;
	struct mutex irq_det_ldetl_lock;
	struct mutex hmicbias_polling_lock;
	int ldetl_trig_val_last; /* ldetect low detecting gpio last value */
	int insert_all_val_last; /* detecting all gpio last value */
	int bdet_val_last; /* button detecting gpio last value */
	int mdet_val_last;
	int ldetl_plug_in;
	int btn_state_last; /* 0==released, 1==pressed */
	/*
	 * if the hardware detected a headset is
	 * plugged in, set plug_state_last = 1
	 */
	/* the plug in/out state of last time irq handled */
	int plug_state_last;
	bool adc_big_scale;
	bool typec_attached;
	struct notifier_block typec_plug_nb;
	struct extcon_dev *edev;
	unsigned int codec_intc;
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
void sprd_headset_remove(void);
void sprd_codec_intc_irq(struct snd_soc_codec *codec, u32 int_shadow);

#if defined(CONFIG_SND_SOC_SPRD_CODEC_SC2730)
void headset_set_audio_state(bool enable);
#endif
void sprd_codec_intc_enable(bool enable, u32 irq_bit);

#endif
