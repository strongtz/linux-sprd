/*
 * SPDX-License-Identifier: GPL-2.0
 * Core MFD(Charger, ADC, Flash and GPIO) driver for UCP1301
 *
 * Copyright (c) 2019 Dialog Semiconductor.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("ucp1301")""fmt

#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include "sprd-asoc-common.h"
#include "ucp1301.h"

#define UCP1301_I2C_NAME    "ucp1301"
#define UCP1301_DRIVER_VERSION  "v1.0.0"
#define UCP1301_CHIP_ID 0x1301a000
#define UCP1301_NEW_CHIP_ID 0x1301a001

#define UCP_I2C_RETRIES 5
#define UCP_I2C_RETRY_DELAY 2
#define UCP_READ_CHIPID_RETRIES 5
#define UCP_READ_CHIPID_RETRY_DELAY 2
#define UCP_READ_EFS_STATUS_RETRIES 3
#define UCP_EFS_DATA_REG_COUNTS 4
/* Timeout (us) of polling the status */
#define UCP_READ_EFS_POLL_TIMEOUT	800
#define UCP_READ_EFS_POLL_DELAY_US	200
#define UCP_AGC_GAIN0		0x65
#define UCP_PRODUCT_ID_MASK	0xe
#define UCP_POWER_BASE_DATA	(1383 * 1383ULL)
#define UCP_BASE_DIVISOR	10000000000ULL
/* coefficient of p1, 10^(0.15) = 1.412538 */
#define UCP_P1_RATIO	1412538
/* coefficient of pb, 10^(-0.15) = 0.707946 */
#define UCP_PB_RATIO	707946
#define UCP_P1_PB_DIVISOR	1000000
#define UCP_LIMIT_P2		1000
#define UCP_R_LOAD		8000
#define UCP_AGC_N2		2
#define UCP_AGC_N1		1
#define UCP_AGC_NB		16
#define UCP_TYPE_MAX		3

enum ucp1301_class_mode {
	HW_OFF,
	SPK_AB,
	RCV_AB,
	SPK_D,
	RCV_D,
	MODE_MAX
};

enum ucp1301_ivsense_mode {
	IVS_UCP1301,
	IVS_UCP1300A,
	IVS_UCP1300B,
	IVS_MODE_MAX
};

struct ucp1301_t {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	struct device *dev;
	struct gpio_desc *reset_gpio;
	/* protect paras which can be modified by kcontrol */
	struct mutex ctrl_lock;
	u8 i2c_index;
	bool init_flag;
	bool hw_enabled;
	bool class_ab;/* true class AB, false class D */
	bool bypass;/* true bypass, false boost */
	enum ucp1301_ivsense_mode ivsense_mode;
	u32 vosel;/* value of RG_BST_VOSEL */
	u32 efs_data[4];/* 0 L, 1 M, 2 H, 3 T */
	u32 calib_code;
	enum ucp1301_class_mode class_mode;
	bool agc_en;
	u32 agc_gain0;
	u32 clsd_trim;
	u32 power_p2;
	u32 power_p1;
	u32 power_pb;
	u32 r_load;
	u32 agc_n1;
	u32 agc_n2;
	u32 agc_nb;
	u32 product_id;
};

struct ucp1301_t *ucp1301_g;
static const char * const ucp1301_type[UCP_TYPE_MAX] = {
	"ucp1301-spk",
	"ucp1301-spk2",
	"ucp1301-rcv"
};

static u32 efs_data_reg[4] = {
	REG_EFS_RD_DATA_L,/* low */
	REG_EFS_RD_DATA_M,
	REG_EFS_RD_DATA_H,
	REG_EFS_RD_DATA_T/* high */
};

static const struct regmap_config ucp1301_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
};

static const char *ucp1301_get_event_name(int event)
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

static int ucp1301_widget_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	pr_debug("wname %s %s\n", w->name, ucp1301_get_event_name(event));

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

static void ucp1301_sw_reset(struct ucp1301_t *ucp1301)
{
	switch (ucp1301->product_id) {
	case 2:
		ucp1301->ivsense_mode = IVS_UCP1300A;
		break;
	case 4:
		ucp1301->ivsense_mode = IVS_UCP1300B;
		break;
	default:
		ucp1301->ivsense_mode = IVS_UCP1301;
	}

	ucp1301->init_flag = true;
	ucp1301->class_mode = SPK_D;
	ucp1301->agc_en = false;
	ucp1301->agc_gain0 = UCP_AGC_GAIN0;
	/* set to 1600 KHz in default, corresponding reg value is 0xf */
	ucp1301->clsd_trim = 0xf;
	ucp1301->r_load = UCP_R_LOAD;
	ucp1301->power_p2 = UCP_LIMIT_P2;
	ucp1301->power_p1 =
		ucp1301->power_p2 * UCP_P1_RATIO / UCP_P1_PB_DIVISOR;
	ucp1301->power_pb =
		ucp1301->power_p2 * UCP_PB_RATIO / UCP_P1_PB_DIVISOR;
}

static void ucp1301_write_agc_en(struct ucp1301_t *ucp1301, bool agc_en)
{
	regmap_update_bits(ucp1301->regmap, REG_AGC_EN, BIT_AGC_EN,
			   agc_en);
}

static void ucp1301_set_agc_n2(struct ucp1301_t *ucp1301, u32 agc_n2)
{
	int ret;

	ret = regmap_update_bits(ucp1301->regmap, REG_AGC_N2,
				 BIT_AGC_N2(0xff),
				 agc_n2);
	if (ret)
		dev_warn(ucp1301->dev, "update BIT_AGC_N2 fail, %d\n",
			 ret);
}

static void ucp1301_set_agc_n1(struct ucp1301_t *ucp1301, u32 agc_n1)
{
	int ret;

	ret = regmap_update_bits(ucp1301->regmap, REG_AGC_N1,
				 BIT_AGC_N1(0xff),
				 agc_n1);
	if (ret)
		dev_warn(ucp1301->dev, "update BIT_AGC_N1 fail, %d\n",
			 ret);
}

static void ucp1301_set_agc_nb(struct ucp1301_t *ucp1301, u32 agc_nb)
{
	int ret;

	ret = regmap_update_bits(ucp1301->regmap, REG_AGC_NB,
				 BIT_AGC_NB(0xff),
				 agc_nb);
	if (ret)
		dev_warn(ucp1301->dev, "update BIT_AGC_NB fail, %d\n",
			 ret);
}

static void ucp1301_set_agc_vcut(struct ucp1301_t *ucp1301, u32 agc_vcut)
{
	int ret;

	ret = regmap_update_bits(ucp1301->regmap, REG_AGC_VCUT,
				 BIT_AGC_VCUT(0x3ff), agc_vcut);
	if (ret)
		dev_warn(ucp1301->dev, "update BIT_AGC_VCUT fail, %d\n",
			 ret);
}

static void ucp1301_set_agc_mk2(struct ucp1301_t *ucp1301, u32 agc_mk2)
{
	int ret;

	ret = regmap_update_bits(ucp1301->regmap, REG_AGC_MK2,
				 BIT_AGC_MK2(0xff), agc_mk2);
	if (ret)
		dev_warn(ucp1301->dev, "update BIT_AGC_MK2 fail, %d\n",
			 ret);
}

static void ucp1301_write_agc_gain(struct ucp1301_t *ucp1301, u32 agc_gain0)
{
	int ret;

	ret = regmap_update_bits(ucp1301->regmap, REG_AGC_GAIN0,
				 BIT_AGC_GAIN0(0x7f), agc_gain0);
	if (ret)
		dev_warn(ucp1301->dev, "update BIT_AGC_GAIN0 fail, %d\n",
			 ret);
}

static int ucp1301_read_agc_gain(struct ucp1301_t *ucp1301, u32 *agc_gain0)
{
	u32 val_temp;
	int ret;

	ret = regmap_read(ucp1301->regmap, REG_AGC_GAIN0, &val_temp);
	if (ret < 0) {
		dev_err(ucp1301->dev, "read REG_AGC_GAIN0 reg fail, %d\n", ret);
		return ret;
	}

	*agc_gain0 = val_temp;
	return ret;
}

static int ucp1301_write_clsd_trim(struct ucp1301_t *ucp1301, u32 clsd_trim)
{
	u32 val;
	int new_trim, ret;

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set clsd trim, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;

	/*
	 * formula:
	 * para convert to frequence(unit: KHz): x = (clsd_trim + 1) * 100
	 * new_trim = ucp1301->calib_code + x / 100 - 16
	 *               = ucp1301->calib_code + clsd_trim + 1 - 16
	 *               = ucp1301->calib_code + clsd_trim - 15
	 */
	new_trim = ucp1301->calib_code + clsd_trim - 15;
	if (new_trim < 0)
		new_trim = 0;
	else
		new_trim = new_trim & BIT_RG_PMU_OSC1P6M_CLSD_TRIM(0x1f);

	dev_dbg(ucp1301->dev, "new_trim 0x%x, calib_code 0x%x, clsd_trim 0x%x\n",
		new_trim, ucp1301->calib_code, ucp1301->clsd_trim);

	regmap_update_bits(ucp1301->regmap, REG_PMU_REG1,
			   BIT_PMU_OSC1P6M_CLSD_SW_SEL,
			   BIT_PMU_OSC1P6M_CLSD_SW_SEL);
	regmap_update_bits(ucp1301->regmap, REG_PMU_REG1,
			   BIT_RG_PMU_OSC1P6M_CLSD_TRIM(0x1f), new_trim);

	return 0;
}

static void ucp1301_calcu_power(struct ucp1301_t *ucp1301, u32 r_load,
				u32 power_p2, u32 power_p1, u32 power_pb)
{
	u64 temp_val;
	u32 value;

	/* calculate limit_p2 */
	temp_val = UCP_POWER_BASE_DATA * power_p2 * r_load * ucp1301->agc_n2;
	temp_val /= UCP_BASE_DIVISOR;
	dev_dbg(ucp1301->dev, "power_p2 %u, r_load %u, agc_n2 %u, agc_n1 %u, agc_nb %u, temp_val 0x%llx\n",
		power_p2, r_load, ucp1301->agc_n2, ucp1301->agc_n1,
		ucp1301->agc_nb, temp_val);
	value = temp_val & BIT_AGC_P2_L(0xffff);
	regmap_update_bits(ucp1301->regmap, REG_AGC_P2_L, BIT_AGC_P2_L(0xffff),
			   value);

	value = (temp_val >> 16) & BIT_AGC_P2_H(0x7f);
	regmap_update_bits(ucp1301->regmap, REG_AGC_P2_H, BIT_AGC_P2_H(0x7f),
			   value);

	/* calculate p1, p1 is max output power */
	temp_val = UCP_POWER_BASE_DATA * power_p1 * r_load * ucp1301->agc_n1;
	temp_val /= UCP_BASE_DIVISOR;
	dev_dbg(ucp1301->dev, "power_p1 %u, temp_val 0x%llx\n",
		power_p1, temp_val);
	value = temp_val & BIT_AGC_P1_L(0xffff);
	regmap_update_bits(ucp1301->regmap, REG_AGC_P1_L, BIT_AGC_P1_L(0xffff),
			   value);

	value = (temp_val >> 16) & BIT_AGC_P1_H(0x7f);
	regmap_update_bits(ucp1301->regmap, REG_AGC_P1_H, BIT_AGC_P1_H(0x7f),
			   value);

	/* calculate pb, pb is min output power */
	temp_val = UCP_POWER_BASE_DATA * power_pb * r_load * ucp1301->agc_nb;
	temp_val /= UCP_BASE_DIVISOR;
	dev_dbg(ucp1301->dev, "power_pb %u, temp_val 0x%llx\n",
		power_pb, temp_val);
	value = temp_val & BIT_AGC_PB_L(0xffff);
	regmap_update_bits(ucp1301->regmap, REG_AGC_PB_L, BIT_AGC_PB_L(0xffff),
			   value);

	value = (temp_val >> 16) & BIT_AGC_PB_H(0x7f);
	regmap_update_bits(ucp1301->regmap, REG_AGC_PB_H, BIT_AGC_PB_H(0x7f),
			   value);
}

/* class AB depop boost on mode(spk + bst mode) */
static void ucp1301_depop_ab_boost_on(struct ucp1301_t *ucp1301, bool enable)
{
	if (enable) {
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, BIT_RG_CLSD_MODE_SEL);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_BST_ACTIVE, BIT_BST_ACTIVE);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, 0);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG1,
				   BIT_RG_BST_VOSEL(0xf), BIT_RG_BST_VOSEL(1));
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, BIT_CHIP_EN);
		/* dc calibration time */
		sprd_msleep(40);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4),
				   BIT_RG_RESERVED1(0x4));
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2),
				   BIT_RG_RESERVED1(0x2));

		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, BIT_RG_CLSAB_MODE_EN);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2), 0);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4), 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
		/* pcc time */
		sprd_msleep(30);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		/* pcc time */
		sprd_msleep(30);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, 0);
	}
}

/*
 * class AB depop boost on mode(rcv + bypass mode),
 * only BIT_RG_CLSD_MODE_SEL and BIT_RG_BST_BYPASS
 * bits is different, compare to boost_on mode
 */
static void ucp1301_depop_ab_boost_bypass(struct ucp1301_t *ucp1301,
					  bool enable)
{
	int ret;

	if (enable) {
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_BST_ACTIVE, BIT_BST_ACTIVE);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, BIT_RG_BST_BYPASS);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG1,
				   BIT_RG_BST_VOSEL(0xf), BIT_RG_BST_VOSEL(1));
		ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
					 BIT_CHIP_EN,
					 BIT_CHIP_EN);
		/* dc calibration time */
		sprd_msleep(40);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4),
				   BIT_RG_RESERVED1(0x4));
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2),
				   BIT_RG_RESERVED1(0x2));

		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, BIT_RG_CLSAB_MODE_EN);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2), 0);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4), 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, 0);
		ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
					 BIT_CHIP_EN, 0);
	}
	if (ret)
		dev_err(ucp1301->dev, "boost_bypass, set BIT_CHIP_EN to %d fail, %d\n",
			enable, ret);
	/* pcc time */
	sprd_msleep(30);
}

/* spk + boost mode */
static void ucp1301_d_boost_on(struct ucp1301_t *ucp1301, bool on)
{
	int ret;
	u32 temp;

	if (on) {
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, BIT_RG_CLSD_MODE_SEL);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
	}

	temp = on ? BIT_CHIP_EN : 0;
	ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				 BIT_CHIP_EN, temp);
	if (ret) {
		dev_err(ucp1301->dev, "boost_on, set BIT_CHIP_EN to %d fail, %d\n",
			on, ret);
		return;
	}

	/* wait circuit to response */
	sprd_msleep(30);
}

/* rcv + bypass mode */
static void ucp1301_d_bypass(struct ucp1301_t *ucp1301, bool on)
{
	int ret;
	u32 temp;

	if (on) {
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, BIT_RG_BST_BYPASS);
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
	}

	temp = on ? BIT_CHIP_EN : 0;
	ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				 BIT_CHIP_EN, temp);
	if (ret) {
		dev_err(ucp1301->dev, "d_bypass, set BIT_CHIP_EN to %d fail, %d\n",
			on, ret);
		return;
	}

	/* wait circuit to response */
	sprd_msleep(30);
}

static void ucp1301_d_ab_switch(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {/* class D switch to class AB */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, BIT_RG_CLSAB_MODE_EN);
	} else {/* class AB switch to class D */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, 0);
	}
}

static void ucp1301_bst_bypass_switch(struct ucp1301_t *ucp1301, bool on)
{
	int ret;

	if (on) {/* boost switch to bypass */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, 0);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, BIT_RG_BST_BYPASS);
	} else {/* bypass switch to bootst */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, 0);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, 0);
	}
	ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN, BIT_CHIP_EN,
				 BIT_CHIP_EN);
	if (ret)
		dev_err(ucp1301->dev, "bypass_switch, set BIT_CHIP_EN to 1 fail, %d\n",
			ret);
}

static int ucp1301_read_efs_data(struct ucp1301_t *ucp1301)
{
	int ret, i = 0;

	do {
		ret = regmap_read(ucp1301->regmap, efs_data_reg[i],
				  &ucp1301->efs_data[i]);
		if (ret < 0) {
			dev_err(ucp1301->dev, "read efs data, read reg 0x%x fail, %d\n",
				efs_data_reg[i], ret);
			return ret;
		}
		dev_dbg(ucp1301->dev, "efs[%d] 0x%x\n",
			i, ucp1301->efs_data[i]);
	} while (++i < UCP_EFS_DATA_REG_COUNTS);

	return 0;
}

static int ucp1301_read_efuse(struct ucp1301_t *ucp1301)
{
	u32 val, product_id;
	int ret;

	ret = regmap_read_poll_timeout(ucp1301->regmap, REG_EFS_STATUS, val,
				       (val & BIT_EFS_IDLE),
				       UCP_READ_EFS_POLL_DELAY_US,
				       UCP_READ_EFS_POLL_TIMEOUT);
	if (ret) {
		dev_err(ucp1301->dev, "check bit BIT_EFS_IDLE fail, %d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(ucp1301->regmap, REG_EFS_MODE_CTRL,
				 BIT_EFS_NORMAL_READ_START,
				 BIT_EFS_NORMAL_READ_START);
	if (ret) {
		dev_err(ucp1301->dev, "update BIT_EFS_NORMAL_READ_START fail, %d\n",
			ret);
		return ret;
	}
	ret = regmap_read_poll_timeout(ucp1301->regmap, REG_EFS_STATUS, val,
		((val & BIT_EFS_IDLE) && (val & BIT_EFS_NORMAL_RD_DONE_FLAG)),
				       UCP_READ_EFS_POLL_DELAY_US,
				       UCP_READ_EFS_POLL_TIMEOUT);
	if (ret) {
		dev_err(ucp1301->dev, "check bits BIT_EFS_IDLE, BIT_EFS_NORMAL_RD_DONE_FLAG fail, %d\n",
			ret);
		return ret;
	}

	ret = ucp1301_read_efs_data(ucp1301);
	if (ret) {
		dev_err(ucp1301->dev, "read efs data fail, %d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(ucp1301->regmap, REG_EFS_MODE_CTRL,
				 BIT_EFS_NORMAL_READ_DONE_FLAG_CLR,
				 BIT_EFS_NORMAL_READ_DONE_FLAG_CLR);
	if (ret) {
		dev_err(ucp1301->dev, "update BIT_EFS_NORMAL_READ_DONE_FLAG_CLR fail, %d\n",
			ret);
		return ret;
	}

	if ((ucp1301->efs_data[0] & BIT_EFS_RD_DATA_L_PRO) > 0) {
		ucp1301->calib_code =
			(ucp1301->efs_data[0] &
			 BIT_EFS_RD_DATA_L_OSC1P6M_CLSD_TRIM(0x1f)) >> 9;
		product_id = ucp1301->efs_data[0] & UCP_PRODUCT_ID_MASK;
		ucp1301->product_id = product_id >> 1;
	} else {
		ucp1301->calib_code = 0xf;/* set default value if  */
		ucp1301->product_id = 1;/* treat as ucp1301 in default */
		pr_warn("chip not calibrated, set default value 0xf for calib_code\n");
	}

	pr_info("read efuse, calib_code 0x%x\n", ucp1301->calib_code);

	return 0;
}

static void ucp1301_power_param_init(struct ucp1301_t *ucp1301)
{
	ucp1301_set_agc_n2(ucp1301, UCP_AGC_N2);
	ucp1301->agc_n2 = UCP_AGC_N2;

	ucp1301_set_agc_n1(ucp1301, UCP_AGC_N1);
	ucp1301->agc_n1 = UCP_AGC_N1;

	ucp1301_set_agc_nb(ucp1301, UCP_AGC_NB);
	ucp1301->agc_nb = UCP_AGC_NB;
}

static void ucp1301_reg_init(struct ucp1301_t *ucp1301)
{
	ucp1301_set_agc_vcut(ucp1301, 0x1c9);
	ucp1301_set_agc_mk2(ucp1301, 0x1);
}

static int ucp1301_hw_on(struct ucp1301_t *ucp1301, bool on)
{
	int ret;

	if (!ucp1301->reset_gpio) {
		dev_err(ucp1301->dev, "hw_on failed, reset_gpio error\n");
		return -EINVAL;
	}

	dev_dbg(ucp1301->dev, "hw_on, on %d\n", on);
	if (on) {
		gpiod_set_value_cansleep(ucp1301->reset_gpio, true);
		usleep_range(2000, 2050);
		ucp1301->hw_enabled = true;
		ucp1301_reg_init(ucp1301);
		/*
		 * following is reruning kcontrol settings, because some
		 * kcontrols are called before power on, it is invalid,
		 * so rerun them here.
		 */
		mutex_lock(&ucp1301->ctrl_lock);
		ucp1301_write_clsd_trim(ucp1301, ucp1301->clsd_trim);
		ucp1301_power_param_init(ucp1301);
		ucp1301_calcu_power(ucp1301, ucp1301->r_load, ucp1301->power_p2,
				    ucp1301->power_p1, ucp1301->power_pb);
		ucp1301_write_agc_en(ucp1301, ucp1301->agc_en);
		ucp1301_write_agc_gain(ucp1301, ucp1301->agc_gain0);
		mutex_unlock(&ucp1301->ctrl_lock);
	} else {
		ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
					 BIT_CHIP_EN, 0);
		if (ret)
			dev_warn(ucp1301->dev, "hw_on, set BIT_CHIP_EN to 0 fail, %d\n",
				 ret);
		gpiod_set_value_cansleep(ucp1301->reset_gpio, false);
		usleep_range(2000, 2050);
		ucp1301->hw_enabled = false;
	}

	return 0;
}

static int ucp1301_audio_receiver(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_depop_ab_boost_bypass(ucp1301, true);
	} else {
		ucp1301_depop_ab_boost_bypass(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}

	return 0;
}

static int ucp1301_audio_speaker(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_depop_ab_boost_on(ucp1301, true);
	} else {
		ucp1301_depop_ab_boost_on(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}

	return 0;
}

static int ucp1301_audio_receiver_d(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_d_bypass(ucp1301, true);
	} else {
		ucp1301_d_bypass(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}
	return 0;
}

static int ucp1301_audio_speaker_d(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_d_boost_on(ucp1301, true);
	} else {
		ucp1301_d_boost_on(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}

	return 0;
}

static ssize_t ucp1301_get_reg(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 reg, reg_val[5];
	int ret;
	ssize_t len = 0;
	u8 i;

	for (reg = REG_CHIP_ID_LOW; reg < UCP1301_REG_MAX; reg += 5) {
		for (i = 0; i < 5; i++) {
			ret = regmap_read(ucp1301->regmap, reg + i,
					  &reg_val[i]);
			if (ret < 0) {
				dev_err(ucp1301->dev, "get reg, read reg 0x%x fail, ret %d\n",
					reg + i - CTL_BASE_ANA_APB_IF, ret);
				return ret;
			}
		}
		len += sprintf(buf + len,
			       "0x%02x | %04x %04x %04x %04x %04x\n",
			       reg, reg_val[0], reg_val[1],
			       reg_val[2], reg_val[3], reg_val[4]);
	}
	/* read the last(max reg) reg, 0x32 */
	if (reg >= UCP1301_REG_MAX) {
		ret = regmap_read(ucp1301->regmap, reg, &reg_val[0]);
		if (ret < 0) {
			dev_err(ucp1301->dev, "get reg, read reg 0x%x fail, ret %d\n",
				reg - CTL_BASE_ANA_APB_IF, ret);
			return ret;
		}
	}
	len += sprintf(buf + len, "0x%02x | %04x\n", reg,
		       reg_val[0]);

	return len;
}

static ssize_t ucp1301_set_reg(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t len)
{
	u32 databuf[2];
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) != 2) {
		dev_err(ucp1301->dev, "set reg, parse data fail\n");
		return -EINVAL;
	}

	dev_dbg(ucp1301->dev, "set reg, reg 0x%x --> val 0x%x\n",
		databuf[0], databuf[1]);
	regmap_write(ucp1301->regmap, databuf[0], databuf[1]);

	return len;
}

static ssize_t ucp1301_get_hw_state(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);

	return sprintf(buf, "hwenable: %d\n", ucp1301->hw_enabled);
}

static ssize_t ucp1301_set_hw_state(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set hw state, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	if (databuf == 0)
		ucp1301_hw_on(ucp1301, false);
	else
		ucp1301_hw_on(ucp1301, true);

	return len;
}

static ssize_t ucp1301_get_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	ssize_t len;

	len = sprintf(buf, "current mode %d\n", ucp1301->class_mode);
	len += sprintf(buf + len,
		       "mode: 0 hwoff, 1 spk ab, 2 rcv ab, 3 spk d, 4 rev d\n");

	return len;
}

static void ucp1301_save_mode(struct ucp1301_t *ucp1301, u32 mode_type)
{
	switch (mode_type) {
	case 0:
		ucp1301->class_mode = HW_OFF;
		break;
	case 1:
		ucp1301->class_mode = SPK_AB;
		break;
	case 2:
		ucp1301->class_mode = RCV_AB;
		break;
	case 3:
		ucp1301->class_mode = SPK_D;
		break;
	case 4:
		ucp1301->class_mode = RCV_D;
		break;
	default:
		dev_err(ucp1301->dev, "unknown mode type %d, set to default\n",
			mode_type);
		ucp1301->class_mode = SPK_D;
	}
}

static ssize_t ucp1301_set_mode(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 mode_type;
	int ret;

	ret = kstrtouint(buf, 10, &mode_type);
	if (ret) {
		dev_err(ucp1301->dev, "set mode,fail %d, buf %s\n", ret, buf);
		return ret;
	}

	pr_info("set mode, mode_type %d, buf %s\n", mode_type, buf);
	ucp1301_save_mode(ucp1301, mode_type);

	return len;
}

/* return current AB/D mode, 1 class AB, 0 class D */
static ssize_t ucp1301_get_abd(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 val;
	int ret;

	ret = regmap_read(ucp1301->regmap, REG_CLSAB_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get abd, read reg 0x%x fail, %d\n",
			REG_CLSAB_REG0, ret);
		return ret;
	}
	ucp1301->class_ab = (val & BIT_RG_CLSAB_MODE_EN) > 0 ? true : false;

	return sprintf(buf, "current class_ab %d\n", ucp1301->class_ab);
}

static ssize_t ucp1301_set_abd(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 databuf, val;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set abd, fail %d, buf %s\n", ret, buf);
		return ret;
	}

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_CLSAB_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set abd, read reg 0x%x fail, %d\n",
			REG_CLSAB_REG0, ret);
		return ret;
	}
	ucp1301->class_ab = (val & BIT_RG_CLSAB_MODE_EN) > 0 ? true : false;
	pr_info("set abd, databuf %d, class_ab %d, buf %s\n",
		databuf, ucp1301->class_ab, buf);

	if (databuf == ucp1301->class_ab) {
		dev_err(ucp1301->dev, "class_ab was %d already, needn't to set\n",
			ucp1301->class_ab);
		return -EINVAL;
	}
	ucp1301_d_ab_switch(ucp1301, ucp1301->class_ab);

	return len;
}

/* return current Boost/bypass mode, 1 bypass, 0 boost */
static ssize_t ucp1301_get_bypass(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 val;
	int ret;

	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get bypass, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;

	return sprintf(buf, "current bypass %d\n", ucp1301->bypass);
}

static ssize_t ucp1301_set_bypass(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf, val;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set bypass, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set bypass, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;
	pr_info("set bypass, databuf %d, bypass %d, buf %s\n",
		databuf, ucp1301->bypass, buf);

	if (databuf == ucp1301->bypass) {
		dev_err(ucp1301->dev, "set bypass was %d already, needn't to set\n",
			ucp1301->bypass);
		return len;
	}
	ucp1301_bst_bypass_switch(ucp1301, ucp1301->bypass);

	return len;
}

/* return current VOSEL value */
static ssize_t ucp1301_get_vosel(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 val, val2;
	int ret;

	/* read vosel */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG1, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get vosel, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->vosel = (val & BIT_RG_BST_VOSEL(0xf)) >> 7;

	/* read bypass state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val2);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get vosel, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val2 & BIT_RG_BST_BYPASS) > 0 ? true : false;

	pr_info("get vosel, vosel 0x%x, bypass %d, REG1 0x%x\n",
		ucp1301->vosel, ucp1301->bypass, val);
	return sprintf(buf, "current vosel 0x%x, bypass %d\n",
		       ucp1301->vosel, ucp1301->bypass);
}

static ssize_t ucp1301_set_vosel(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf, val;
	int ret;

	ret = kstrtouint(buf, 16, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set vosel, fail %d, buf %s\n", ret, buf);
		return ret;
	}

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set vosel, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;
	dev_dbg(ucp1301->dev, "set vosel, databuf 0x%x, bypass %d, buf %s\n",
		databuf, ucp1301->bypass, buf);

	if (ucp1301->bypass) {
		dev_err(ucp1301->dev, "you can't set vosel when bypass mode is on\n");
		return len;
	}
	regmap_update_bits(ucp1301->regmap, REG_BST_REG1, BIT_RG_BST_VOSEL(0xf),
			   BIT_RG_BST_VOSEL(databuf));

	return len;
}

static int ucp1301_read_clsd_trim(struct ucp1301_t *ucp1301, u32 *clsd_trim)
{
	u32 val;
	int ret;

	/* read clsd_trim */
	ret = regmap_read(ucp1301->regmap, REG_PMU_REG1, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get clsd trim, read reg 0x%x fail, %d\n",
			REG_PMU_REG1, ret);
		return ret;
	}
	*clsd_trim = val & BIT_RG_PMU_OSC1P6M_CLSD_TRIM(0x1f);

	/* read bypass state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get clsd trim, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;

	dev_dbg(ucp1301->dev, "get clsd trim 0x%x, ori clsd_trim 0x%x, calib_code 0x%x, bypass %d\n",
		*clsd_trim, ucp1301->clsd_trim, ucp1301->calib_code,
		ucp1301->bypass);

	return 0;
}

/*
 * return current /ori clsd_trim value, ori clsd_trim value is
 * 0xf, corresponding clock output frequency is 1.6 MHz
 */
static ssize_t ucp1301_get_clsd_trim(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 clsd_trim;
	int ret;

	ret = ucp1301_read_clsd_trim(ucp1301, &clsd_trim);
	if (ret)
		dev_warn(ucp1301->dev, "get clsd trim fail, %d\n", ret);

	return sprintf(buf,
		       "calib_code 0x%x, clsd_trim 0x%x, bypass %d\n",
		       ucp1301->calib_code, clsd_trim, ucp1301->bypass);
}

/* the unit is KHz, if databuf[0] = 100, it means 100 KHz */
static ssize_t ucp1301_set_clsd_trim(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set clsd trim, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	ucp1301->clsd_trim = databuf;
	ret = ucp1301_write_clsd_trim(ucp1301, ucp1301->clsd_trim);
	if (ret)
		return ret;

	return len;
}

static void ucp1301_ivsense_remote(struct ucp1301_t *ucp1301)
{
	u32 val, mask;

	mask = BIT_RG_AUD_PA_SVSNSAD | BIT_RG_AUD_PA_SISNSAD;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   mask);

	mask = BIT_RG_AUD_AD_CLK_RST_V | BIT_RG_AUD_AD_CLK_RST_I |
		BIT_RG_AUD_ADC_V_RST | BIT_RG_AUD_ADC_I_RST |
		BIT_RG_AUD_AD_D_GATE_V | BIT_RG_AUD_AD_D_GATE_I;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_ADC_REG0, mask, 0);

	mask = BIT_RG_AUD_PA_VSNS_EN | BIT_RG_AUD_PA_ISNS_EN;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   mask);

	mask = BIT_RG_AUD_ADPGA_IBIAS_EN | BIT_RG_AUD_VCM_VREF_BUF_EN |
		BIT_RG_AUD_AD_CLK_EN_V | BIT_RG_AUD_AD_CLK_EN_I |
		BIT_RG_AUD_ADC_V_EN | BIT_RG_AUD_ADC_I_EN |
		BIT_RG_AUD_AD_DATA_INVERSE_V;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_ADC_REG0, mask, mask);

	mask = BIT_RG_AUD_PA_VS_G(0x3) | BIT_RG_AUD_PA_IS_G(0x3);
	val = BIT_RG_AUD_PA_VS_G(0) | BIT_RG_AUD_PA_IS_G(0x3);
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   val);
}

static void ucp1301_ivsense_local(struct ucp1301_t *ucp1301)
{
	u32 mask;

	ucp1301_ivsense_remote(ucp1301);
	mask = BIT_RG_AUD_PA_ISNS_EN | BIT_RG_AUD_PA_SISNSAD;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   0);

	mask = BIT_RG_AUD_ADC_I_EN;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_ADC_REG0, mask, 0);
}

static void ucp1301_ivsense_set(struct ucp1301_t *ucp1301)
{
	enum ucp1301_ivsense_mode ivsense_mode;

	mutex_lock(&ucp1301->ctrl_lock);
	ivsense_mode = ucp1301->ivsense_mode;
	mutex_unlock(&ucp1301->ctrl_lock);

	switch (ivsense_mode) {
	case IVS_UCP1300A:
		ucp1301_ivsense_local(ucp1301);
		break;
	case IVS_UCP1301:
		ucp1301_ivsense_remote(ucp1301);
		break;
	default:
		dev_info(ucp1301->dev, "ivsense_set, this is ucp1300b or others %d\n",
			 ivsense_mode);
	}
}

static ssize_t ucp1301_get_ivsense_mode(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	ssize_t len;

	len = sprintf(buf,
		      "ivsense_mode %d\n", ucp1301->ivsense_mode);
	len += sprintf(buf + len, "mode: 0 remote, 1 local, 2 no\n");

	return len;
}

static ssize_t ucp1301_set_ivsense_mode(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 ivsense_mode;
	int ret;

	ret = kstrtouint(buf, 10, &ivsense_mode);
	if (ret) {
		dev_err(ucp1301->dev, "set ivsense, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	if (ivsense_mode > IVS_UCP1300B) {
		dev_err(ucp1301->dev, "set ivsense, out of range %d\n",
			ivsense_mode);
		return -EINVAL;
	}

	ucp1301->ivsense_mode = ivsense_mode;
	dev_dbg(ucp1301->dev, "set ivsense, ivsense_mode %d\n",
		ucp1301->ivsense_mode);

	return len;
}

static DEVICE_ATTR(regs, 0660, ucp1301_get_reg, ucp1301_set_reg);
static DEVICE_ATTR(hwenable, 0660, ucp1301_get_hw_state, ucp1301_set_hw_state);
static DEVICE_ATTR(mode, 0660, ucp1301_get_mode, ucp1301_set_mode);
static DEVICE_ATTR(abd_switch, 0660, ucp1301_get_abd, ucp1301_set_abd);
static DEVICE_ATTR(bypass, 0660, ucp1301_get_bypass, ucp1301_set_bypass);
static DEVICE_ATTR(vosel, 0660, ucp1301_get_vosel, ucp1301_set_vosel);
static DEVICE_ATTR(clsd_trim, 0660, ucp1301_get_clsd_trim,
		   ucp1301_set_clsd_trim);
static DEVICE_ATTR(ivsense_mode, 0660, ucp1301_get_ivsense_mode,
		   ucp1301_set_ivsense_mode);

static struct attribute *ucp1301_attributes[] = {
	&dev_attr_regs.attr,
	&dev_attr_hwenable.attr,
	&dev_attr_mode.attr,
	&dev_attr_abd_switch.attr,
	&dev_attr_bypass.attr,
	&dev_attr_vosel.attr,
	&dev_attr_clsd_trim.attr,
	&dev_attr_ivsense_mode.attr,
	NULL
};

static struct attribute_group ucp1301_attribute_group = {
	.attrs = ucp1301_attributes
};

static int ucp1301_get_agc_en(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->agc_en;

	return 0;
}

static int ucp1301_set_agc_en(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] > 1)
		return -EINVAL;

	mutex_lock(&ucp1301->ctrl_lock);
	ucp1301->agc_en = ucontrol->value.integer.value[0];
	ucp1301_write_agc_en(ucp1301, ucp1301->agc_en);
	mutex_unlock(&ucp1301->ctrl_lock);

	dev_dbg(ucp1301->dev, "set_agc_en %d\n", ucp1301->agc_en);

	return 0;
}

static int ucp1301_get_agc_gain(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);
	u32 val_temp;
	int ret;

	ret = ucp1301_read_agc_gain(ucp1301, &val_temp);
	if (ret < 0) {
		ucontrol->value.integer.value[0] = ucp1301->agc_gain0;
		return 0;
	}
	ucontrol->value.integer.value[0] = val_temp & BIT_AGC_GAIN0(0x7f);

	return 0;
}

static int ucp1301_set_agc_gain(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] > BIT_AGC_GAIN0(0x7f))
		return -EINVAL;

	mutex_lock(&ucp1301->ctrl_lock);
	ucp1301->agc_gain0 = ucontrol->value.integer.value[0];
	ucp1301_write_agc_gain(ucp1301, ucp1301->agc_gain0);
	mutex_unlock(&ucp1301->ctrl_lock);

	dev_dbg(ucp1301->dev, "set_agc_gain %d\n", ucp1301->agc_gain0);

	return 0;
}

static int ucp1301_get_clasd_trim(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);
	u32 clsd_trim;
	int ret;

	ret = ucp1301_read_clsd_trim(ucp1301, &clsd_trim);
	if (ret) {
		dev_warn(ucp1301->dev, "get_clasd_trim fail %d\n", ret);
		ucontrol->value.integer.value[0] = ucp1301->clsd_trim;
		return 0;
	}
	ucontrol->value.integer.value[0] = clsd_trim;

	return 0;
}

static int ucp1301_set_clasd_trim(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&ucp1301->ctrl_lock);
	ucp1301->clsd_trim = ucontrol->value.integer.value[0];
	ucp1301_write_clsd_trim(ucp1301, ucp1301->clsd_trim);
	mutex_unlock(&ucp1301->ctrl_lock);

	return 0;
}

static int ucp1301_get_r_load(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->r_load;

	return 0;
}

static int ucp1301_set_r_load(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&ucp1301->ctrl_lock);
	ucp1301->r_load = ucontrol->value.integer.value[0];
	ucp1301_calcu_power(ucp1301, ucp1301->r_load, ucp1301->power_p2,
			    ucp1301->power_p1, ucp1301->power_pb);
	mutex_unlock(&ucp1301->ctrl_lock);

	dev_dbg(ucp1301->dev, "set_r_load %d\n", ucp1301->r_load);

	return 0;
}

static int ucp1301_get_limit_p2(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->power_p2;

	return 0;
}

static int ucp1301_set_limit_p2(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&ucp1301->ctrl_lock);
	ucp1301->power_p2 = ucontrol->value.integer.value[0];
	ucp1301->power_p1 =
		ucp1301->power_p2 * UCP_P1_RATIO / UCP_P1_PB_DIVISOR;
	ucp1301->power_pb =
		ucp1301->power_p2 * UCP_PB_RATIO / UCP_P1_PB_DIVISOR;
	ucp1301_calcu_power(ucp1301, ucp1301->r_load, ucp1301->power_p2,
			    ucp1301->power_p1, ucp1301->power_pb);
	mutex_unlock(&ucp1301->ctrl_lock);

	dev_dbg(ucp1301->dev, "set_limit_p2 %d\n", ucp1301->power_p2);

	return 0;
}

static int ucp1301_get_class_mode(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->class_mode;

	return 0;
}

static int ucp1301_set_class_mode(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);
	u32 mode;

	mode = ucontrol->value.integer.value[0];
	mutex_lock(&ucp1301->ctrl_lock);
	ucp1301_save_mode(ucp1301, mode);
	mutex_unlock(&ucp1301->ctrl_lock);

	dev_dbg(ucp1301->dev, "set_class_mode %d\n", mode);
	return 0;
}

static int ucp1301_get_product_id(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->product_id;

	return 0;
}

static int ucp1301_get_pdn(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->hw_enabled;

	return 0;
}

static int ucp1301_set_pdn(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);
	bool reset_state = ucontrol->value.integer.value[0];

	dev_dbg(ucp1301->dev, "set_pdn to %d\n", reset_state);
	ucp1301_hw_on(ucp1301, reset_state);

	return 0;
}

static int ucp1301_get_regs(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	return -EINVAL;
}

static int ucp1301_set_regs(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);
	u32 reg_addr = ucontrol->value.integer.value[0];
	u32 reg_value = ucontrol->value.integer.value[1];

	regmap_update_bits(ucp1301->regmap, reg_addr, 0xff, reg_value);
	dev_dbg(ucp1301->dev, "set_regs, reg_addr 0x%x, reg_value 0x%x\n",
		reg_addr, reg_value);

	return 0;
}

static int ucp1301_ivsense_get_mode(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->ivsense_mode;

	return 0;
}

static int ucp1301_ivsense_set_mode(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] > IVS_UCP1300B ||
	    ucontrol->value.integer.value[0] < IVS_UCP1301) {
		dev_err(ucp1301->dev, "ivsense_set_mode, out of range %ld\n",
			ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	mutex_lock(&ucp1301->ctrl_lock);
	ucp1301->ivsense_mode = ucontrol->value.integer.value[0];
	mutex_unlock(&ucp1301->ctrl_lock);

	dev_dbg(ucp1301->dev, "set ivsense, ivsense_mode %d\n",
		ucp1301->ivsense_mode);

	return 0;
}

static int ucp1301_get_i2c_index(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ucp1301->i2c_index;

	return 0;
}

static const struct snd_kcontrol_new ucp1301_snd_controls[] = {
	SOC_SINGLE_EXT("UCP1301 AGC Enable", 0, 0, 1, 0,
		       ucp1301_get_agc_en, ucp1301_set_agc_en),
	SOC_SINGLE_EXT("UCP1301 AGC Gain", 0, 0, 0x7F, 0,
		       ucp1301_get_agc_gain, ucp1301_set_agc_gain),
	SOC_SINGLE_EXT("UCP1301 CLSD Trim", 0, 0, 0x1f, 0,
		       ucp1301_get_clasd_trim, ucp1301_set_clasd_trim),
	SOC_SINGLE_EXT("UCP1301 R Load", 0, 0, INT_MAX, 0,
		       ucp1301_get_r_load, ucp1301_set_r_load),
	SOC_SINGLE_EXT("UCP1301 Power Limit P2", 0, 0, INT_MAX, 0,
		       ucp1301_get_limit_p2, ucp1301_set_limit_p2),
	SOC_SINGLE_EXT("UCP1301 Class Mode", 0, 0, MODE_MAX, 0,
		       ucp1301_get_class_mode, ucp1301_set_class_mode),
	SOC_SINGLE_EXT("UCP1301 Product ID", 0, 0, 0xf, 0,
		       ucp1301_get_product_id, NULL),
	SOC_SINGLE_EXT("UCP1301 HW Enable", 0, 0, 1, 0,
		       ucp1301_get_pdn, ucp1301_set_pdn),
	SOC_DOUBLE_EXT("UCP1301 Regs Control", 0, 0, 0, INT_MAX, 0,
		       ucp1301_get_regs, ucp1301_set_regs),
	SOC_SINGLE_EXT("UCP1301 IVsense", 0, 0, 3, 0,
		       ucp1301_ivsense_get_mode, ucp1301_ivsense_set_mode),
	SOC_SINGLE_EXT("UCP1301 I2C Index", 0, 0, 0xf, 0,
		       ucp1301_get_i2c_index, NULL),
};

static const struct snd_kcontrol_new ucp1301_snd_controls_spk2[] = {
	SOC_SINGLE_EXT("UCP1301 AGC Enable SPK2", 0, 0, 1, 0,
		       ucp1301_get_agc_en, ucp1301_set_agc_en),
	SOC_SINGLE_EXT("UCP1301 AGC Gain SPK2", 0, 0, 0x7F, 0,
		       ucp1301_get_agc_gain, ucp1301_set_agc_gain),
	SOC_SINGLE_EXT("UCP1301 CLSD Trim SPK2", 0, 0, 0x1f, 0,
		       ucp1301_get_clasd_trim, ucp1301_set_clasd_trim),
	SOC_SINGLE_EXT("UCP1301 R Load SPK2", 0, 0, INT_MAX, 0,
		       ucp1301_get_r_load, ucp1301_set_r_load),
	SOC_SINGLE_EXT("UCP1301 Power Limit P2 SPK2", 0, 0, INT_MAX, 0,
		       ucp1301_get_limit_p2, ucp1301_set_limit_p2),
	SOC_SINGLE_EXT("UCP1301 Class Mode SPK2", 0, 0, MODE_MAX, 0,
		       ucp1301_get_class_mode, ucp1301_set_class_mode),
	SOC_SINGLE_EXT("UCP1301 Product ID SPK2", 0, 0, 0xf, 0,
		       ucp1301_get_product_id, NULL),
	SOC_SINGLE_EXT("UCP1301 HW Enable SPK2", 0, 0, 1, 0,
		       ucp1301_get_pdn, ucp1301_set_pdn),
	SOC_DOUBLE_EXT("UCP1301 Regs Control SPK2", 0, 0, 0, INT_MAX, 0,
		       ucp1301_get_regs, ucp1301_set_regs),
	SOC_SINGLE_EXT("UCP1301 IVsense SPK2", 0, 0, 3, 0,
		       ucp1301_ivsense_get_mode, ucp1301_ivsense_set_mode),
	SOC_SINGLE_EXT("UCP1301 I2C Index SPK2", 0, 0, 0xf, 0,
		       ucp1301_get_i2c_index, NULL),
};

static const struct snd_kcontrol_new ucp1301_snd_controls_rcv[] = {
	SOC_SINGLE_EXT("UCP1301 AGC Enable RCV", 0, 0, 1, 0,
		       ucp1301_get_agc_en, ucp1301_set_agc_en),
	SOC_SINGLE_EXT("UCP1301 AGC Gain RCV", 0, 0, 0x7F, 0,
		       ucp1301_get_agc_gain, ucp1301_set_agc_gain),
	SOC_SINGLE_EXT("UCP1301 CLSD Trim RCV", 0, 0, 0x1f, 0,
		       ucp1301_get_clasd_trim, ucp1301_set_clasd_trim),
	SOC_SINGLE_EXT("UCP1301 R Load RCV", 0, 0, INT_MAX, 0,
		       ucp1301_get_r_load, ucp1301_set_r_load),
	SOC_SINGLE_EXT("UCP1301 Power Limit P2 RCV", 0, 0, INT_MAX, 0,
		       ucp1301_get_limit_p2, ucp1301_set_limit_p2),
	SOC_SINGLE_EXT("UCP1301 Class Mode RCV", 0, 0, MODE_MAX, 0,
		       ucp1301_get_class_mode, ucp1301_set_class_mode),
	SOC_SINGLE_EXT("UCP1301 Product ID RCV", 0, 0, 0xf, 0,
		       ucp1301_get_product_id, NULL),
	SOC_SINGLE_EXT("UCP1301 HW Enable RCV", 0, 0, 1, 0,
		       ucp1301_get_pdn, ucp1301_set_pdn),
	SOC_DOUBLE_EXT("UCP1301 Regs Control RCV", 0, 0, 0, INT_MAX, 0,
		       ucp1301_get_regs, ucp1301_set_regs),
	SOC_SINGLE_EXT("UCP1301 IVsense RCV", 0, 0, 3, 0,
		       ucp1301_ivsense_get_mode, ucp1301_ivsense_set_mode),
	SOC_SINGLE_EXT("UCP1301 I2C Index RCV", 0, 0, 0xf, 0,
		       ucp1301_get_i2c_index, NULL),
};

static void ucp1301_audio_on(struct ucp1301_t *ucp1301, bool on_off)
{
	enum ucp1301_class_mode class_mode;

	dev_dbg(ucp1301->dev, "audio_on, on_off %d, class_mode %d\n",
		on_off, ucp1301->class_mode);

	mutex_lock(&ucp1301->ctrl_lock);
	class_mode = ucp1301->class_mode;
	mutex_unlock(&ucp1301->ctrl_lock);

	switch (class_mode) {
	case HW_OFF:
		ucp1301_hw_on(ucp1301, on_off);
		break;
	case SPK_AB:
		ucp1301_audio_speaker(ucp1301, on_off);
		break;
	case RCV_AB:
		ucp1301_audio_receiver(ucp1301, on_off);
		break;
	case SPK_D:
		ucp1301_audio_speaker_d(ucp1301, on_off);
		break;
	case RCV_D:
		ucp1301_audio_receiver_d(ucp1301, on_off);
		break;
	default:
		dev_err(ucp1301->dev, "unknown mode type error, replace it with SPK_D, %d\n",
			class_mode);
		ucp1301_audio_speaker_d(ucp1301, on_off);
	}

	if (on_off)
		ucp1301_ivsense_set(ucp1301);
}

static int ucp1301_power_on(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(ucp1301->dev, "wname %s, %s, class_mode %d\n",
		w->name, ucp1301_get_event_name(event), ucp1301->class_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		ucp1301_audio_on(ucp1301, true);
		break;
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_POST_PMD:
		ucp1301_audio_on(ucp1301, false);
		break;
	default:
		dev_err(ucp1301->dev, "unknown widget event type %d\n", event);
	}

	return 0;
}

static const struct snd_soc_dapm_widget ucp1301_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN_E("UCP1301 PLAY", "Playback_SPK", 0, SND_SOC_NOPM,
			      0, 0, ucp1301_widget_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("UCP1301 SPK ON", SND_SOC_NOPM, 0, 0, NULL, 0,
			   ucp1301_power_on,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("UCP1301 SPK"),
};

static const struct snd_soc_dapm_widget ucp1301_dapm_widgets_spk2[] = {
	SND_SOC_DAPM_AIF_IN("UCP1301 PLAY SPK2", "Playback_SPK2",
			    0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA_E("UCP1301 SPK2 ON", SND_SOC_NOPM, 0, 0, NULL, 0,
			   ucp1301_power_on,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("UCP1301 SPK2"),
};

static const struct snd_soc_dapm_widget ucp1301_dapm_widgets_rcv[] = {
	SND_SOC_DAPM_AIF_IN("UCP1301 PLAY RCV", "Playback_RCV",
			    0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA_E("UCP1301 RCV ON", SND_SOC_NOPM, 0, 0, NULL, 0,
			   ucp1301_power_on,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("UCP1301 RCV"),
};

static const struct snd_soc_dapm_route ucp1301_intercon[] = {
	{"UCP1301 SPK", NULL, "UCP1301 PLAY"},
	{"UCP1301 SPK", NULL, "UCP1301 SPK ON"},
};

static const struct snd_soc_dapm_route ucp1301_intercon_spk2[] = {
	{"UCP1301 SPK2", NULL, "UCP1301 PLAY SPK2"},
	{"UCP1301 SPK2", NULL, "UCP1301 SPK2 ON"},
};

static const struct snd_soc_dapm_route ucp1301_intercon_rcv[] = {
	{"UCP1301 RCV", NULL, "UCP1301 PLAY RCV"},
	{"UCP1301 RCV", NULL, "UCP1301 RCV ON"},
};

static int ucp1301_soc_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	if (!dapm) {
		dev_err(ucp1301->dev, "spk dapm %p, ucp1301 %p, NULL error\n",
			dapm, ucp1301);
		return -EINVAL;
	}

	snd_soc_dapm_ignore_suspend(dapm, "Playback_SPK");
	snd_soc_dapm_ignore_suspend(dapm, "UCP1301 SPK");

	return 0;
}

static int ucp1301_spk2_soc_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	if (!dapm) {
		dev_err(ucp1301->dev, "spk2 dapm %p, ucp1301 %p, NULL error\n",
			dapm, ucp1301);
		return -EINVAL;
	}

	snd_soc_dapm_ignore_suspend(dapm, "Playback_SPK2");
	snd_soc_dapm_ignore_suspend(dapm, "UCP1301 SPK2");

	return 0;
}

static int ucp1301_rcv_soc_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct ucp1301_t *ucp1301 = snd_soc_codec_get_drvdata(codec);

	if (!dapm) {
		dev_err(ucp1301->dev, "rcv dapm %p, ucp1301 %p, NULL error\n",
			dapm, ucp1301);
		return -EINVAL;
	}

	snd_soc_dapm_ignore_suspend(dapm, "Playback_RCV");
	snd_soc_dapm_ignore_suspend(dapm, "UCP1301 RCV");

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_ucp1301 = {
	.probe = ucp1301_soc_probe,
	.idle_bias_off = true,
	.component_driver = {
		.controls = ucp1301_snd_controls,
		.num_controls = ARRAY_SIZE(ucp1301_snd_controls),
		.dapm_widgets = ucp1301_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ucp1301_dapm_widgets),
		.dapm_routes = ucp1301_intercon,
		.num_dapm_routes = ARRAY_SIZE(ucp1301_intercon),
	}
};

static struct snd_soc_codec_driver soc_codec_dev_ucp1301_spk2 = {
	.probe = ucp1301_spk2_soc_probe,
	.idle_bias_off = true,
	.component_driver = {
		.controls = ucp1301_snd_controls_spk2,
		.num_controls = ARRAY_SIZE(ucp1301_snd_controls_spk2),
		.dapm_widgets = ucp1301_dapm_widgets_spk2,
		.num_dapm_widgets = ARRAY_SIZE(ucp1301_dapm_widgets_spk2),
		.dapm_routes = ucp1301_intercon_spk2,
		.num_dapm_routes = ARRAY_SIZE(ucp1301_intercon_spk2),
	}
};

static struct snd_soc_codec_driver soc_codec_dev_ucp1301_rcv = {
	.probe = ucp1301_rcv_soc_probe,
	.idle_bias_off = true,
	.component_driver = {
		.controls = ucp1301_snd_controls_rcv,
		.num_controls = ARRAY_SIZE(ucp1301_snd_controls_rcv),
		.dapm_widgets = ucp1301_dapm_widgets_rcv,
		.num_dapm_widgets = ARRAY_SIZE(ucp1301_dapm_widgets_rcv),
		.dapm_routes = ucp1301_intercon_rcv,
		.num_dapm_routes = ARRAY_SIZE(ucp1301_intercon_rcv),
	}
};

static int ucp1301_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	return 0;
}

static int ucp1301_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				  unsigned int freq, int dir)
{
	return 0;
}

static int ucp1301_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

static int ucp1301_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	return 0;
}

static int ucp1301_set_dai_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

#define UCP1301_RATES	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |\
			SNDRV_PCM_RATE_192000)

#define UCP1301_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops ucp1301_dai_ops = {
	.hw_params = ucp1301_hw_params,
	.set_sysclk = ucp1301_set_dai_sysclk,
	.set_fmt = ucp1301_set_dai_fmt,
	.trigger = ucp1301_trigger,
	.digital_mute = ucp1301_set_dai_mute,
};

static struct snd_soc_dai_driver ucp1301_dai[] = {
	{
		.name = "ucp1301-SPK",
		.playback = {
			.stream_name = "Playback_SPK",
			.channels_min = 1,
			.channels_max = 2,
			.rates = UCP1301_RATES,
			.formats = UCP1301_FORMATS,
		},
		.ops = &ucp1301_dai_ops,
	},
};

static struct snd_soc_dai_driver ucp1301_dai_spk2[] = {
	{
		.name = "ucp1301-SPK2",
		.playback = {
			.stream_name = "Playback_SPK2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = UCP1301_RATES,
			.formats = UCP1301_FORMATS,
		},
		.ops = &ucp1301_dai_ops,
	},
};

static struct snd_soc_dai_driver ucp1301_dai_rcv[] = {
	{
		.name = "ucp1301-RCV",
		.playback = {
			.stream_name = "Playback_RCV",
			.channels_min = 1,
			.channels_max = 2,
			.rates = UCP1301_RATES,
			.formats = UCP1301_FORMATS,
		},
		.ops = &ucp1301_dai_ops,
	},
};

static int ucp1301_debug_sysfs_init(struct ucp1301_t *ucp1301)
{
	int ret;

	ret = sysfs_create_group(&ucp1301->dev->kobj, &ucp1301_attribute_group);
	if (ret < 0)
		dev_err(ucp1301->dev, "fail to create sysfs attr files\n");

	return ret;
}

static int ucp1301_parse_dt(struct ucp1301_t *ucp1301, struct device_node *np)
{
	ucp1301->reset_gpio = devm_gpiod_get_index(ucp1301->dev, "reset", 0,
						   GPIOD_ASIS);
	if (IS_ERR(ucp1301->reset_gpio)) {
		dev_err(ucp1301->dev, "parse 'reset-gpios' fail\n");
		return PTR_ERR(ucp1301->reset_gpio);
	}
	gpiod_direction_output(ucp1301->reset_gpio, 0);

	return 0;
}

static int ucp1301_read_chipid(struct ucp1301_t *ucp1301)
{
	u32 cnt = 0, chip_id = 0, val_temp;
	int ret;

	while (cnt < UCP_READ_CHIPID_RETRIES) {
		ret = regmap_read(ucp1301->regmap, REG_CHIP_ID_HIGH, &val_temp);
		if (ret < 0) {
			dev_err(ucp1301->dev, "read chip id high fail %d\n",
				ret);
			return ret;
		}
		chip_id = val_temp << 16;
		ret = regmap_read(ucp1301->regmap, REG_CHIP_ID_LOW, &val_temp);
		if (ret < 0) {
			dev_err(ucp1301->dev, "read chip id low fail %d\n",
				ret);
			return ret;
		}
		chip_id |= val_temp;

		if (chip_id == UCP1301_CHIP_ID ||
		    chip_id == UCP1301_NEW_CHIP_ID) {
			pr_info("read chipid successful 0x%x\n", chip_id);
			return 0;
		}
		dev_err(ucp1301->dev,
			"read chipid fail, try again, 0x%x, cnt %d\n",
			chip_id, cnt);

		cnt++;
		msleep(UCP_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

static int ucp1301_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct device *dev = &client->dev;
	struct ucp1301_t *ucp1301;
	struct regmap *regmap;
	int ret;

	ucp1301 = devm_kzalloc(&client->dev, sizeof(struct ucp1301_t),
			       GFP_KERNEL);
	if (!ucp1301)
		return -ENOMEM;

	ucp1301->dev = dev;
	ucp1301->i2c_client = client;
	ucp1301->i2c_index = client->adapter->nr;
	i2c_set_clientdata(client, ucp1301);
	ucp1301_g = ucp1301;

	regmap = devm_regmap_init_i2c(client, &ucp1301_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}
	ucp1301->regmap = regmap;

	ret = ucp1301_parse_dt(ucp1301, np);
	if (ret) {
		dev_err(&client->dev, "failed to parse device tree node\n");
		return ret;
	}
	mutex_init(&ucp1301->ctrl_lock);

	ucp1301_hw_on(ucp1301, true);
	ret = ucp1301_read_chipid(ucp1301);
	if (ret < 0) {
		dev_err(&client->dev, "ucp1301_read_chipid failed ret=%d\n",
			ret);
		return ret;
	}
	ucp1301_read_efuse(ucp1301);
	ucp1301_debug_sysfs_init(ucp1301);
	ucp1301_hw_on(ucp1301, false);
	ucp1301_sw_reset(ucp1301);

	if (strcmp(client->name, ucp1301_type[0]) == 0) {
		ret = snd_soc_register_codec(dev, &soc_codec_dev_ucp1301,
					     &ucp1301_dai[0],
					     ARRAY_SIZE(ucp1301_dai));
	} else if (strcmp(client->name, ucp1301_type[1]) == 0) {
		ret = snd_soc_register_codec(dev, &soc_codec_dev_ucp1301_spk2,
					     &ucp1301_dai_spk2[0],
					     ARRAY_SIZE(ucp1301_dai_spk2));
	} else if (strcmp(client->name, ucp1301_type[2]) == 0) {
		ret = snd_soc_register_codec(dev, &soc_codec_dev_ucp1301_rcv,
					     &ucp1301_dai_rcv[0],
					     ARRAY_SIZE(ucp1301_dai_rcv));
	} else {
		dev_warn(&client->dev, "iic client name error, %s\n",
			 client->name);
	}
	if (ret)
		dev_err(&client->dev, " %s register codec fail, %d\n",
			client->name, ret);

	return ret;
}

static int ucp1301_i2c_remove(struct i2c_client *client)
{
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);

	snd_soc_unregister_codec(&client->dev);
	sysfs_remove_group(&ucp1301->dev->kobj, &ucp1301_attribute_group);

	return 0;
}

static const struct i2c_device_id ucp1301_i2c_id[] = {
	{ UCP1301_I2C_NAME, 0 },
	{ }
};

static const struct of_device_id extpa_of_match[] = {
	{ .compatible = "sprd,ucp1301-spk" },
	{ .compatible = "sprd,ucp1301-spk2" },
	{ .compatible = "sprd,ucp1301-rcv" },
	{},
};

static struct i2c_driver ucp1301_i2c_driver = {
	.driver = {
		.name = UCP1301_I2C_NAME,
		.of_match_table = extpa_of_match,
	},
	.probe = ucp1301_i2c_probe,
	.remove = ucp1301_i2c_remove,
	.id_table    = ucp1301_i2c_id,
};

static int __init ucp1301_init(void)
{
	int ret;

	ret = i2c_add_driver(&ucp1301_i2c_driver);
	if (ret) {
		pr_err("ucp1301 init, Unable to register driver (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit ucp1301_exit(void)
{
	i2c_del_driver(&ucp1301_i2c_driver);
}

late_initcall(ucp1301_init);
module_exit(ucp1301_exit);

MODULE_DESCRIPTION("SPRD SMART PA UCP1301 driver");
MODULE_AUTHOR("Harvey Yin <harvey.yin@unisoc.com>");
