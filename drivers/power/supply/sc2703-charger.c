/* SPDX-License-Identifier: GPL-2.0
 *
 * Charger device driver for SC2703
 *
 * Copyright (c) 2018 Dialog Semiconductor.
 */
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/mfd/sc2703_regs.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define SC2703_BATTERY_NAME		"sc27xx-fgu"
#define SC2703_EVENT_CLR_MASK		GENMASK(7, 0)
#define SC2703_VBAT_CHG_DEFAULT		0x14
#define SC2703_VBAT_CHG_MAX		0x32
#define SC2703_IBAT_TERM_DEFAULT	0x00
#define SC2703_IBAT_TERM_MAX		0x7
#define SC2703_S_CHG_STAT_FULL		0x5
#define SC2703_S_CHG_STAT_FAULT_L1	0x6
#define SC2703_S_CHG_STAT_FAULT_L2	0x7
#define SC2703_E_VBAT_OV_SHIFT		6
#define SC2703_E_VBAT_OV_MASK		BIT(6)
#define BIT_DP_DM_BC_ENB		BIT(0)

#define SC2703_CHG_B_IMIN		500000
#define SC2703_CHG_B_ISTEP		50000
#define SC2703_CHG_B_IMAX		3500000

#define SC2703_CHG_B_VMIN		3800000
#define SC2703_CHG_B_VSTEP		20000
#define SC2703_CHG_B_VMAX		4800000
#define SC2703_OTG_RETRY_TIMES		10
#define SC2703_OTG_VALID_MS		100
#define SC2703_OTG_NORMAL_VALID_MS	1500

struct sc2703_charger_info {
	struct device *dev;
	struct regmap *regmap;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct regmap *pmic;
	u32 charger_detect;
	bool long_key_detect;
};

/* sc2703 input limit current, Milliamp */
static int sc2703_limit_current[] = {
	100000,
	150000,
	500000,
	900000,
	1500000,
	2000000,
	2500000,
	3000000,
};

static const char * const sc2703_fast_charger_supply_name[] = {
	"sc2730_fast_charger",
};

static bool need_disable_dcdc;

static int __init boot_mode(char *str)
{
	if (!str)
		return 0;

	if (!strncmp(str, "cali", strlen("cali")))
		need_disable_dcdc = true;
	else if (!strncmp(str, "autotest", strlen("autotest")))
		need_disable_dcdc = true;
	else if (!strncmp(str, "factorytest", strlen("factorytest")))
		need_disable_dcdc = true;

	return 0;
}
__setup("androidboot.mode=", boot_mode);

static bool sc2703_charger_is_bat_present(struct sc2703_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SC2703_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static u32 sc2703_charger_of_prop_range(struct device *dev, u32 val, u32 min,
				       u32 max, u32 step, u32 default_val,
				       const char *name)
{
	if (val < min || val > max) {
		dev_warn(dev, "Invalid %s value\n", name);
		return default_val;
	} else {
		return (val - min) / step;
	}
}

static int sc2703_charger_hw_init(struct sc2703_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	u32 cur_val, vol_val;
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		cur_val = 0x0;
		vol_val = 0x14;
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		cur_val = sc2703_charger_of_prop_range(info->dev,
					bat_info.charge_term_current_ua,
					100000, 450000, 50000,
					SC2703_IBAT_TERM_DEFAULT,
					"ibat-term");

		/* Set charge termination voltage */
		vol_val = sc2703_charger_of_prop_range(info->dev,
					bat_info.constant_charge_voltage_max_uv,
					SC2703_CHG_B_VMIN,
					SC2703_CHG_B_VMAX,
					SC2703_CHG_B_VSTEP,
					SC2703_VBAT_CHG_DEFAULT,
					"vbat-chg");

		power_supply_put_battery_info(info->psy_usb, &bat_info);
	}

	/* Set charge termination current */
	if (cur_val > SC2703_IBAT_TERM_MAX)
		cur_val = SC2703_IBAT_TERM_MAX;

	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_D,
				 SC2703_IBAT_TERM_MASK,
				 cur_val << SC2703_IBAT_TERM_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set terminal current:%d\n", ret);
		return ret;
	}

	/* Set charge termination voltage */
	if (vol_val > SC2703_VBAT_CHG_MAX)
		vol_val = SC2703_VBAT_CHG_MAX;

	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_A,
				 SC2703_VBAT_CHG_MASK, vol_val);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set terminal voltage:%d\n", ret);
		return ret;
	}

	/* Set vindrop threshold 4300mv */
	ret = regmap_update_bits(info->regmap, SC2703_VIN_CTRL_A,
					 SC2703_VIN_DROP_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set vindrop threshold:%d\n", ret);
		return ret;
	}

	/* Set pre-charging current 100ma */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_D,
				 SC2703_IBAT_PRE_MASK, 0x01);
	if (ret) {
		dev_err(info->dev,
			 "Failed to write pre-charging current:%d\n", ret);
		return ret;
	}

	/* Set end of charge detection time 60min */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_D,
				 SC2703_T_EOC_MASK, 0x07 << SC2703_T_EOC_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to set detection time:%d\n", ret);
		return ret;
	}

	/* Set battery voltage re-charge threshold 100ma */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_A,
				 SC2703_VBAT_RECHG_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage re-charge threshold:%d\n", ret);
		return ret;
	}

	/* Set dc-dc peak current limit 6000ma */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_DCDC_PEAK_ILIM_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set dc-dc peak current limit:%d\n", ret);
		return ret;
	}

	/* Set vbat ovp 4600mv */
	ret = regmap_update_bits(info->regmap, SC2703_VBAT_CTRL_A,
				 SC2703_VBAT_OV_MASK,
				 0x06 << SC2703_VBAT_OV_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage ovp value:%d\n", ret);
		return ret;
	}

	/* Set vbat uvp 2800mv */
	ret = regmap_update_bits(info->regmap, SC2703_VBAT_CTRL_A,
				 SC2703_VBAT_UV_MASK, 0x0c);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage uvp value:%d\n", ret);
		return ret;
	}

	/* Set minimum system voltage 3420mv */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_C,
				 SC2703_VSYS_MIN_MASK, 0x07);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set minimum system voltage:%d\n", ret);
		return ret;
	}

	/* Set pre-charge timeout 900s */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_TIMER_CTRL_A,
				 SC2703_TIMEOUT_PRE_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set pre-charge timeout:%d\n", ret);
		return ret;
	}

	/* Set constant current/constant voltage charging timeout 10hour */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_TIMER_CTRL_A,
				 SC2703_TIMEOUT_CCCV_MASK, 0x04);
	if (ret) {
		dev_err(info->dev, "Failed to set cc timeout:%d\n", ret);
		return ret;
	}

	/* enable the watchdog timer */
	ret = regmap_update_bits(info->regmap, SC2703_CONF_A,
				 SC2703_WD_EN_MASK, SC2703_WD_EN_MASK);
	if (ret) {
		dev_err(info->dev,
			 "Failed to enable the watchdog timer:%d\n", ret);
		return ret;
	}

	/* Set dc-dc output current limit in reverse boost mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_IIN_REV_LIM_MASK,
				 0x05 << SC2703_IIN_REV_LIM_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set dc-dc output current limit:%d\n", ret);
		return ret;
	}

	/* Set dc-dc peak current limit in reverse boost mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_DCDC_REV_PEAK_ILIM_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set dc-dc peak current limit:%d\n", ret);
		return ret;
	}

	/* Selects the sources for battery detection */
	ret = regmap_update_bits(info->regmap, SC2703_CONF_A,
				 SC2703_BAT_DET_SRC_MASK,
				 0x01 << SC2703_BAT_DET_SRC_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to selects the sources detection:%d\n", ret);
		return ret;
	}

	/* Set T3 temperature threshold */
	ret = regmap_update_bits(info->regmap, SC2703_TBAT_CTRL_A,
				 SC2703_TBAT_T3_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set T3 temperature threshold:%d\n", ret);
		return ret;
	}

	/* Set T4 temperature threshold */
	ret = regmap_update_bits(info->regmap, SC2703_TBAT_CTRL_A,
				 SC2703_TBAT_T4_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set T4 temperature threshold:%d\n", ret);
		return ret;
	}

	/* Set charge voltage reduction in TBAT_T1-TBAT_T2 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_VBAT_COLD_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage reduction in T1-T2:%d\n", ret);
		return ret;
	}

	/* Set charge current reduction in TBAT_T1-TBAT_T2 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_IBAT_COLD_MASK, SC2703_IBAT_COLD_MASK);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set current reduction in T1-T2:%d\n", ret);
		return ret;
	}

	/* Set charge voltage reduction in TBAT_T3-TBAT_T4 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_VBAT_WARM_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage reduction in T3-T4:%d\n", ret);
		return ret;
	}

	/* Set charge current reduction in TBAT_T3-TBAT_T4 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_IBAT_WARM_MASK, SC2703_IBAT_WARM_MASK);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set current reduction in T3-T4:%d\n", ret);
		return ret;
	}

	if (info->long_key_detect) {
		/* Enable the detection of long key presses */
		ret = regmap_update_bits(info->regmap, SC2703_CONF_A,
					 SC2703_ONKEY_DET_EN_MASK,
					 SC2703_ONKEY_DET_EN_MASK);
		if (ret) {
			dev_err(info->dev,
				 "Failed to enable the long key:%d\n", ret);
			return ret;
		}
	}

	/* Set flash voltage output 4800mv */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_F,
				 SC2703_DCDC_REV_VOUT_MASK, 0);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set flash voltage output 4800mv:%d\n", ret);
		return ret;
	}

	/* Unlock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_REG_UNLOCK_VAL);
	if (ret) {
		dev_err(info->dev,
			 "Failed to unlock 2703 test mode:%d\n", ret);
		return ret;
	}

	/* Disable vsys_uv */
	ret = regmap_update_bits(info->regmap, SC2703_VSYS_UV_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_VSYS_UV_SWITCH_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to disable vsys_uv:%d\n", ret);
		return ret;
	}

	/* Enable charge buck */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to enable charge buck:%d\n", ret);
		return ret;
	}

	/* Lock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to lock 2703 test mode:%d\n", ret);
		return ret;
	}

	return 0;
}

static void sc2703_clear_event(struct sc2703_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_A,
				 SC2703_EVENT_CLR_MASK, SC2703_EVENT_CLR_MASK);
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_a:%d\n", ret);

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_B,
				 SC2703_EVENT_CLR_MASK, SC2703_EVENT_CLR_MASK);
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_b:%d\n", ret);

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_C,
				 SC2703_EVENT_CLR_MASK,
				 SC2703_EVENT_CLR_MASK &
				 (~SC2703_E_ADC_DONE_MASK));
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_c:%d\n", ret);

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_D,
				 SC2703_EVENT_CLR_MASK, SC2703_EVENT_CLR_MASK);
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_d:%d\n", ret);
}

static int sc2703_charger_start_charge(struct sc2703_charger_info *info)
{
	int ret;

	sc2703_clear_event(info);
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_CHG_EN_MASK | SC2703_DCDC_EN_MASK,
				 SC2703_CHG_EN_MASK | SC2703_DCDC_EN_MASK);
	if (ret) {
		dev_err(info->dev, "Failed to enable sc2703 charge:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_REG_UNLOCK_VAL);
	if (ret) {
		dev_err(info->dev,
			"Failed to unlock sc2703 test mode:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_VSYS_UV_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_VSYS_UV_SWITCH_VAL);
	if (ret) {
		dev_err(info->dev,
			"Failed to disable sc2703 vys_uv mode:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev,
			"Failed to enable sc2703 charge buck:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to lock sc2703 test mode:%d\n", ret);
		return ret;
	}

	return 0;
}

static void sc2703_charger_stop_charge(struct sc2703_charger_info *info,
				       bool present)
{
	int mask, ret;

	if (need_disable_dcdc)
		mask = SC2703_CHG_EN_MASK | SC2703_DCDC_EN_MASK;
	else
		mask = SC2703_CHG_EN_MASK;

	if (present) {
		ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
					 mask, 0);
		if (ret)
			dev_err(info->dev,
				"Failed to set chg_en and dcdc_en:%d\n", ret);
	} else {
		ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
					 SC2703_CHG_EN_MASK, 0);
		if (ret) {
			dev_err(info->dev, "Failed to set chg_en:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_REG_UNLOCK_VAL);
		if (ret) {
			dev_err(info->dev,
				"Failed to unlock sc2703 test mode:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_VSYS_UV_SWITCH,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_VSYS_UV_SWITCH_VAL);
		if (ret) {
			dev_err(info->dev,
				"Failed to disable sc2703 vys_uv:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_CHG_BUCK_SWITCH_VAL);
		if (ret) {
			dev_err(info->dev,
				"Failed to enable  charge buck:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_CHG_REG_DISABLE_VAL);
		if (ret)
			dev_err(info->dev,
				"Failed to lock sc2703 test mode:%d\n", ret);
	}
}

static int
sc2703_charger_set_current(struct sc2703_charger_info *info, u32 val)
{
	u32 reg;

	if (val < SC2703_CHG_B_IMIN)
		val = SC2703_CHG_B_IMIN;
	else if (val > SC2703_CHG_B_IMAX)
		val = SC2703_CHG_B_IMAX;

	reg = (val - SC2703_CHG_B_IMIN) / SC2703_CHG_B_ISTEP;
	return regmap_update_bits(info->regmap, SC2703_CHG_CTRL_B,
			   SC2703_IBAT_CHG_MASK,
			   reg << SC2703_IBAT_CHG_SHIFT);
}

static u32
sc2703_charger_get_current(struct sc2703_charger_info *info, u32 *cur)
{
	u32 val;
	int ret;

	ret = regmap_read(info->regmap, SC2703_CHG_CTRL_B, &val);
	if (ret)
		return ret;

	/* The value of the SC2703_CHG_CTRL_B register is converted
	 * to the charge current value.
	 * Calculation formula:(x * 50) + 500 ma
	 */
	 *cur = ((val & SC2703_IBAT_CHG_MASK) * 50) + 500;

	return 0;
}

static int
sc2703_charger_set_limit_current(struct sc2703_charger_info *info, u32 val)
{
	u32 limit_index;
	int i;

	for (i = 1; i < ARRAY_SIZE(sc2703_limit_current); ++i)
		if (val < sc2703_limit_current[i])
			break;

	limit_index = i - 1;
	regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_D,
			   SC2703_IIN_LIM0_MASK | SC2703_IIN_LIM1_MASK,
			   limit_index |
			   (limit_index << SC2703_IIN_LIM1_SHIFT));

	return 0;
}

static int
sc2703_charger_get_limit_current(struct sc2703_charger_info *info, u32 *cur)
{

	u32 val;
	int ret;

	ret = regmap_read(info->regmap, SC2703_DCDC_CTRL_D, &val);
	if (ret)
		return ret;

	*cur = sc2703_limit_current[val & SC2703_IIN_LIM0_MASK];

	return 0;
}

static int sc2703_charger_get_status(struct sc2703_charger_info *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int sc2703_charger_get_health(struct sc2703_charger_info *info,
				     u32 *health)
{
	struct regmap *regmap = info->regmap;
	u8 status[4];
	int ret;

	ret = regmap_bulk_read(regmap, SC2703_STATUS_A, status,
			       ARRAY_SIZE(status));
	if (ret)
		return ret;

	switch (status[3] & SC2703_S_CHG_STAT_MASK) {
	case SC2703_S_CHG_STAT_FAULT_L2:
		if (status[0] & SC2703_S_VIN_UV_MASK)
			*health = POWER_SUPPLY_HEALTH_DEAD;
		else if (status[0] & SC2703_S_VIN_OV_MASK)
			*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else if (status[1] & SC2703_S_TJUNC_CRIT_MASK)
			*health = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (status[3] & SC2703_S_VSYS_OV_MASK)
			*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			*health = POWER_SUPPLY_HEALTH_GOOD;

		break;
	default:
		*health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	}

	return 0;
}

static int sc2703_charger_get_online(struct sc2703_charger_info *info,
				     u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int sc2703_charger_set_status(struct sc2703_charger_info *info, u32 val,
				     bool present)
{
	int ret = 0;

	if (!val && info->charging) {
		sc2703_charger_stop_charge(info, present);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = sc2703_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static int sc2703_charger_feed_watchdog(struct sc2703_charger_info *info,
					u32 val)
{
	int ret;

	ret = regmap_write(info->regmap, SC2703_CHG_TIMER_CTRL_B, val);
	if (ret)
		dev_err(info->dev, "feed watchdog failed\n");

	return ret;
}

static int sc2703_charger_set_terminate_volt(struct sc2703_charger_info *info,
					     u32 val)
{
	u32 vol_val;

	vol_val = sc2703_charger_of_prop_range(info->dev, val,
					       SC2703_CHG_B_VMIN,
					       SC2703_CHG_B_VMAX,
					       SC2703_CHG_B_VSTEP,
					       SC2703_VBAT_CHG_DEFAULT,
					       "vbat-chg");

	/* Set charge termination voltage */
	if (vol_val > SC2703_VBAT_CHG_MAX)
		vol_val = SC2703_VBAT_CHG_MAX;

	return regmap_update_bits(info->regmap, SC2703_CHG_CTRL_A,
				  SC2703_VBAT_CHG_MASK, vol_val);
}

static bool sc2703_charger_is_support_fchg(struct sc2703_charger_info *info)
{
	union power_supply_propval val;
	struct power_supply *psy;
	u32 ic_version;
	int charger_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN, ret, i;

	ret = regmap_read(info->regmap, SC2703_IC_VERSION_INFO, &ic_version);
	if (ret) {
		dev_err(info->dev, "read ic version info failed\n");
		return false;
	}

	if (ic_version != SC2703_SUPPORT_FCHG)
		return false;

	for (i = 0; i < ARRAY_SIZE(sc2703_fast_charger_supply_name); i++) {
		psy = power_supply_get_by_name(sc2703_fast_charger_supply_name[i]);
		if (!psy)
			continue;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
						&val);
		power_supply_put(psy);
		if (ret)
			return false;
		charger_type = val.intval;
	}

	return charger_type == POWER_SUPPLY_CHARGE_TYPE_FAST;
}

static void sc2703_charger_work(struct work_struct *data)
{
	struct sc2703_charger_info *info =
		container_of(data, struct sc2703_charger_info, work);
	int limit_cur, cur, ret;
	bool present = sc2703_charger_is_bat_present(info);

	mutex_lock(&info->lock);

	if (info->limit > 0 && !info->charging && present) {
		/* set current limitation and start to charge */
		switch (info->usb_phy->chg_type) {
		case SDP_TYPE:
			limit_cur = info->cur.sdp_limit;
			cur = info->cur.sdp_cur;
			break;
		case DCP_TYPE:
			limit_cur = info->cur.dcp_limit;
			cur = info->cur.dcp_cur;
			break;
		case CDP_TYPE:
			limit_cur = info->cur.cdp_limit;
			cur = info->cur.cdp_cur;
			break;
		default:
			limit_cur = info->cur.unknown_limit;
			cur = info->cur.unknown_cur;
		}

		if (sc2703_charger_is_support_fchg(info)) {
			limit_cur = info->cur.fchg_limit;
			cur = info->cur.fchg_cur;
		}

		ret = sc2703_charger_set_limit_current(info, limit_cur);
		if (ret)
			goto out;

		ret = sc2703_charger_set_current(info, cur);
		if (ret)
			goto out;

		ret = sc2703_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		sc2703_charger_stop_charge(info, present);
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static int sc2703_charger_usb_change(struct notifier_block *nb,
				     unsigned long limit, void *data)
{
	struct sc2703_charger_info *info =
		container_of(nb, struct sc2703_charger_info, usb_notify);

	info->limit = limit;

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int sc2703_charger_usb_get_property(struct power_supply *psy,
					   enum power_supply_property psp,
					   union power_supply_propval *val)
{
	struct sc2703_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 cur, online, health;
	enum usb_charger_type type;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = sc2703_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sc2703_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur * 1000;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sc2703_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = sc2703_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sc2703_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}

		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int
sc2703_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sc2703_charger_info *info = power_supply_get_drvdata(psy);
	bool present = sc2703_charger_is_bat_present(info);
	int ret;

	mutex_lock(&info->lock);

	if (!info->charging && psp == POWER_SUPPLY_PROP_FEED_WATCHDOG) {
		mutex_unlock(&info->lock);
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc2703_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc2703_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = sc2703_charger_set_status(info, val->intval, present);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = sc2703_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sc2703_charger_set_terminate_volt(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc2703_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type sc2703_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property sc2703_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_FEED_WATCHDOG,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc sc2703_charger_desc = {
	.name			= "sc2703_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sc2703_usb_props,
	.num_properties		= ARRAY_SIZE(sc2703_usb_props),
	.get_property		= sc2703_charger_usb_get_property,
	.set_property		= sc2703_charger_usb_set_property,
	.property_is_writeable	= sc2703_charger_property_is_writeable,
	.usb_types		= sc2703_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(sc2703_charger_usb_types),
};

static void sc2703_charger_detect_status(struct sc2703_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);

	info->limit = min;
	schedule_work(&info->work);
}

#ifdef CONFIG_REGULATOR
static int sc2703_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sc2703_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u32 val;

	ret = regmap_read(info->regmap, SC2703_DCDC_CTRL_A, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sc2703 otg status.\n");
		return ret;
	}
	val &= SC2703_OTG_EN_MASK;

	return val;
}

static int sc2703_charger_enable_otg(struct regulator_dev *dev)
{
	struct sc2703_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev,
			"failed to disable bc1.2 detect function.\n");
		return ret;
	}

	/* Unlock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_REG_UNLOCK_VAL);
	if (ret) {
		dev_err(info->dev,
			"failed to unlock sc2703 test mode.\n");
		return ret;
	}

	/* Enable charge buck */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev,
			"failed to disable sc2703 charge buck.\n");
		return ret;
	}

	/* Lock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev,
			"failed to lock sc2703 test mode.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_F,
				 SC2703_DCDC_REV_VOUT_MASK,
				 SC2703_CHG_DCDC_REV_VOUT_5000MV);
	if (ret) {
		dev_err(info->dev,
			"failed to set flash voltage output 5000mv.\n");
		return ret;
	}

	/* Set dc-dc output limit cur 500ma in reverse boost mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_IIN_REV_LIM_MASK, 0);
	if (ret) {
		dev_err(info->dev,
			"Failed to set dc-dc output current limit:%d\n", ret);
		return ret;
	}
	/* Enable 2703 otg mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_OTG_EN_MASK,
				 SC2703_OTG_EN_MASK);
	if (ret) {
		dev_err(info->dev,
			"failed to enable sc2703 otg.\n");
		return ret;
	}

	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SC2703_OTG_VALID_MS));

	return ret;
}

static int sc2703_charger_disable_otg(struct regulator_dev *dev)
{
	struct sc2703_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	cancel_delayed_work_sync(&info->otg_work);

	/* Disable 2703 otg mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_OTG_EN_MASK, 0);
	if (ret) {
		dev_err(info->dev,
			"Failed to disable sc2703 otg ret.\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, 0);
	if (ret) {
		dev_err(info->dev,
			"failed to enable bc1.2 detect function.\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_F,
				 SC2703_DCDC_REV_VOUT_MASK, 0);
	if (ret) {
		dev_err(info->dev, "failed to set flash voltage output 4800mv.\n");
		return ret;
	}

	return 0;
}

static bool sc2703_charger_otg_is_valid(struct sc2703_charger_info *info)
{
	u32 otg_state, otg_event;
	int ret;

	ret = regmap_read(info->regmap, SC2703_DCDC_CTRL_A, &otg_state);
	if (ret)
		return false;
	ret = regmap_read(info->regmap, SC2703_EVENT_C, &otg_event);
	if (ret)
		return false;

	otg_state = otg_state & SC2703_OTG_EN_MASK;
	otg_event = otg_event & (SC2703_E_LOWBAT_MASK |
		SC2703_E_IIN_REV_LIM_MAX_MASK | SC2703_E_IIN_REV_LIM_MASK |
		SC2703_E_VIN_REV_SHORT_MASK);
	if (!otg_state || otg_event) {
		dev_err(info->dev,
			"otg_state=%x, raw_st=%x\n", otg_state, otg_event);
		return false;
	}

	return true;
}

static void sc2703_charger_otg_restart(struct sc2703_charger_info *info)
{
	int ret;

	/* Disable 2703 otg */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
		SC2703_OTG_EN_MASK, 0);
	if (ret) {
		dev_err(info->dev, "Failed to disable otg:%d\n", ret);
		return;
	}

	/* Clear 2703_event_c */
	ret = regmap_update_bits(info->regmap, SC2703_EVENT_C,
		SC2703_E_LOWBAT_MASK | SC2703_E_IIN_REV_LIM_MAX_MASK
		| SC2703_E_IIN_REV_LIM_MASK | SC2703_E_VIN_REV_SHORT_MASK,
		SC2703_E_LOWBAT_MASK | SC2703_E_IIN_REV_LIM_MAX_MASK |
		SC2703_E_IIN_REV_LIM_MASK | SC2703_E_VIN_REV_SHORT_MASK);
	if (ret) {
		dev_err(info->dev, "Failed to clear 2703_event_c:%d\n", ret);
		return;
	}

	/* Set dc-dc output limit cur 500ma in reverse boost mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_IIN_REV_LIM_MASK, 0);
	if (ret) {
		dev_err(info->dev,
			"Failed to set dc-dc output current limit:%d\n", ret);
		return;
	}

	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_OTG_EN_MASK, SC2703_OTG_EN_MASK);
	if (ret)
		dev_err(info->dev, "Failed to enable otg:%d\n", ret);
}

static void sc2703_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc2703_charger_info *info = container_of(dwork,
			struct sc2703_charger_info, otg_work);
	bool otg_valid = sc2703_charger_otg_is_valid(info);
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		sc2703_charger_otg_restart(info);
		/*
		 * Enable 2703 otg function must be separated by
		 * 100ms, otherwise OTG restart will fail.
		 */
		msleep(100);
		otg_valid = sc2703_charger_otg_is_valid(info);
	} while (!otg_valid && retry++ < SC2703_OTG_RETRY_TIMES);

	if (retry >= SC2703_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	/* Set dc-dc output limit cur 2000ma in reverse boost mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_IIN_REV_LIM_MASK,
				 0x04 << SC2703_IIN_REV_LIM_SHIFT);
	if (ret) {
		dev_err(info->dev,
			"Failed to set dc-dc 2000ma limit cur:%d\n", ret);
		return;
	}
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SC2703_OTG_NORMAL_VALID_MS));
}

static const struct regulator_ops sc2703_charger_vbus_ops = {
	.enable = sc2703_charger_enable_otg,
	.disable = sc2703_charger_disable_otg,
	.is_enabled = sc2703_charger_vbus_is_enabled,
};

static const struct regulator_desc sc2703_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sc2703_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
sc2703_charger_register_vbus_regulator(struct sc2703_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sc2703_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}
#else
static int
sc2703_charger_register_vbus_regulator(struct sc2703_charger_info *info)
{
	return 0;
}
#endif

static int sc2703_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *regmap_np;
	struct sc2703_charger_info *info;
	struct power_supply_config charger_cfg = { };
	struct platform_device *regmap_pdev;
	int ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->dev = &pdev->dev;
	INIT_WORK(&info->work, sc2703_charger_work);

	info->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!info->regmap) {
		dev_err(&pdev->dev, "failed to get charger regmap\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(&pdev->dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(&pdev->dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	ret = sc2703_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(&pdev->dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1, &info->charger_detect);
	if (ret) {
		dev_err(&pdev->dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(&pdev->dev, "unable to get syscon platform device\n");
		return -ENODEV;
	}

	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(&pdev->dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	info->long_key_detect =
		device_property_read_bool(&pdev->dev, "sprd,long-key-detection");

	info->usb_notify.notifier_call = sc2703_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(&pdev->dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = np;
	info->psy_usb = devm_power_supply_register(&pdev->dev,
						   &sc2703_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(&pdev->dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return PTR_ERR(info->psy_usb);
	}

	ret = sc2703_charger_hw_init(info);
	if (ret)
		return ret;

	sc2703_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, sc2703_charger_otg_work);

	return 0;
}

static void sc2703_shutdown(struct platform_device *pdev)
{
	struct sc2703_charger_info *info = platform_get_drvdata(pdev);
	int ret;

	cancel_delayed_work_sync(&info->otg_work);

	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_OTG_EN_MASK, 0);
	if (ret)
		dev_warn(&pdev->dev, "failed to disable otg_en:%d\n", ret);

	/* Unlock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_REG_UNLOCK_VAL);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to unlock sc2703 test mode:%d\n", ret);

	/* Enable charge buck */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to disable sc2703 charge buck:%d\n", ret);

	/* Lock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to lock sc2703 test mode:%d\n", ret);

	/* Set target battery voltage 3800mv */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_A,
				 SC2703_VBAT_CHG_MASK, 0x00);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to set sc2703 target vbat:%d\n", ret);
}

static int sc2703_charger_remove(struct platform_device *pdev)
{
	struct sc2703_charger_info *info = platform_get_drvdata(pdev);
	int ret;

	/* Disable charging/boost */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_CHG_EN_MASK |
				 SC2703_OTG_EN_MASK, 0);
	if (ret) {
		dev_err(info->dev,
			 "Failed to disable charging/boost=%d\n", ret);
	}

	return ret;
}

static const struct of_device_id sc2703_charger_of_match[] = {
	{ .compatible = "sprd,sc2703-charger", },
	{ }
};

MODULE_DEVICE_TABLE(of, sc2703_charger_of_match);

static struct platform_driver sc2703_charger_driver = {
	.driver = {
		.name = "sc2703-charger",
		.of_match_table = sc2703_charger_of_match,
	},
	.probe = sc2703_charger_probe,
	.shutdown = sc2703_shutdown,
	.remove = sc2703_charger_remove,
};

module_platform_driver(sc2703_charger_driver);
MODULE_DESCRIPTION("Dialog SC2703 Charger Driver");
MODULE_LICENSE("GPL v2");
