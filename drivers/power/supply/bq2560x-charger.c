/*
 * Driver for the TI bq2560x charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
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
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define BQ2560X_REG_0				0x0
#define BQ2560X_REG_1				0x1
#define BQ2560X_REG_2				0x2
#define BQ2560X_REG_3				0x3
#define BQ2560X_REG_4				0x4
#define BQ2560X_REG_5				0x5
#define BQ2560X_REG_6				0x6
#define BQ2560X_REG_B				0xb

#define BQ2560X_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)

#define	BQ2560X_REG_IINLIM_BASE			100

#define BQ2560X_REG_ICHG_LSB			60

#define BQ2560X_REG_ICHG_MASK			GENMASK(5, 0)

#define BQ2560X_REG_CHG_MASK			GENMASK(4, 4)


#define BQ2560X_REG_RESET_MASK			GENMASK(6, 6)

#define BQ2560X_REG_OTG_MASK			GENMASK(5, 5)

#define BQ2560X_REG_WATCHDOG_MASK		GENMASK(6, 6)

#define BQ2560X_REG_TERMINAL_VOLTAGE_MASK	GENMASK(7, 3)
#define BQ2560X_REG_TERMINAL_VOLTAGE_SHIFT	3

#define BQ2560X_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)

#define BQ2560X_REG_VINDPM_VOLTAGE_MASK		GENMASK(3, 0)

#define BQ2560X_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)

#define BQ2560X_DISABLE_PIN_MASK_2730		BIT(0)
#define BQ2560X_DISABLE_PIN_MASK_2721		BIT(15)
#define BQ2560X_DISABLE_PIN_MASK_2720		BIT(0)

#define BQ2560X_OTG_VALID_MS			500
#define BQ2560X_FEED_WATCHDOG_VALID_MS		50
#define BQ2560X_OTG_RETRY_TIMES			10
#define BQ2560X_LIMIT_CURRENT_MAX		3200000

struct bq2560x_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
};

static int
bq2560x_charger_set_limit_current(struct bq2560x_charger_info *info,
				  u32 limit_cur);

static bool bq2560x_charger_is_bat_present(struct bq2560x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(BQ2560X_BATTERY_NAME);
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

static int bq2560x_read(struct bq2560x_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int bq2560x_write(struct bq2560x_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int bq2560x_update_bits(struct bq2560x_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = bq2560x_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return bq2560x_write(info, reg, v);
}

static int
bq2560x_charger_set_vindpm(struct bq2560x_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 3900)
		reg_val = 0x0;
	else if (vol > 5400)
		reg_val = 0x0f;
	else
		reg_val = (vol - 3900) / 100;

	return bq2560x_update_bits(info, BQ2560X_REG_6,
				   BQ2560X_REG_VINDPM_VOLTAGE_MASK, reg_val);
}

static int
bq2560x_charger_set_termina_vol(struct bq2560x_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 3500)
		reg_val = 0x0;
	else if (vol >= 4440)
		reg_val = 0x2e;
	else
		reg_val = (vol - 3856) / 32;

	return bq2560x_update_bits(info, BQ2560X_REG_4,
				   BQ2560X_REG_TERMINAL_VOLTAGE_MASK,
				   reg_val << BQ2560X_REG_TERMINAL_VOLTAGE_SHIFT);
}

static int
bq2560x_charger_set_termina_cur(struct bq2560x_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur <= 60)
		reg_val = 0x0;
	else if (cur >= 480)
		reg_val = 0x8;
	else
		reg_val = (cur - 60) / 60;

	return bq2560x_update_bits(info, BQ2560X_REG_3,
				   BQ2560X_REG_TERMINAL_CUR_MASK,
				   reg_val);
}

static int bq2560x_charger_hw_init(struct bq2560x_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, current_max_ua;
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
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

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		current_max_ua = bat_info.constant_charge_current_max_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		ret = bq2560x_update_bits(info, BQ2560X_REG_B,
					  BQ2560X_REG_RESET_MASK,
					  BQ2560X_REG_RESET_MASK);
		if (ret) {
			dev_err(info->dev, "reset bq2560x failed\n");
			return ret;
		}

		ret = bq2560x_charger_set_vindpm(info, voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set bq2560x vindpm vol failed\n");
			return ret;
		}

		ret = bq2560x_charger_set_termina_vol(info,
						      voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set bq2560x terminal vol failed\n");
			return ret;
		}

		ret = bq2560x_charger_set_termina_cur(info, current_max_ua);
		if (ret) {
			dev_err(info->dev, "set bq2560x terminal cur failed\n");
			return ret;
		}

		ret = bq2560x_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set bq2560x limit current failed\n");
	}

	return ret;
}

static int bq2560x_charger_start_charge(struct bq2560x_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret)
		dev_err(info->dev, "enable bq2560x charge failed\n");

	return ret;
}

static void bq2560x_charger_stop_charge(struct bq2560x_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable bq2560x charge failed\n");
}

static int bq2560x_charger_set_current(struct bq2560x_charger_info *info,
				       u32 cur)
{
	u8 reg_val;

	cur = cur / 1000;
	if (cur > 3000) {
		reg_val = 0x32;
	} else {
		reg_val = cur / BQ2560X_REG_ICHG_LSB;
		reg_val &= BQ2560X_REG_ICHG_MASK;
	}

	return bq2560x_update_bits(info, BQ2560X_REG_2,
				   BQ2560X_REG_ICHG_MASK,
				   reg_val);
}

static int bq2560x_charger_get_current(struct bq2560x_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = bq2560x_read(info, BQ2560X_REG_2, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ2560X_REG_ICHG_MASK;
	*cur = reg_val * BQ2560X_REG_ICHG_LSB * 1000;

	return 0;
}

static int
bq2560x_charger_set_limit_current(struct bq2560x_charger_info *info,
				  u32 limit_cur)
{
	u8 reg_val;
	int ret;

	if (limit_cur >= BQ2560X_LIMIT_CURRENT_MAX)
		limit_cur = BQ2560X_LIMIT_CURRENT_MAX;

	limit_cur = limit_cur / 1000;
	reg_val = limit_cur / BQ2560X_REG_IINLIM_BASE;

	ret = bq2560x_update_bits(info, BQ2560X_REG_0,
				  BQ2560X_REG_LIMIT_CURRENT_MASK,
				  reg_val);
	if (ret)
		dev_err(info->dev, "set bq2560x limit cur failed\n");

	return ret;
}

static u32
bq2560x_charger_get_limit_current(struct bq2560x_charger_info *info,
				  u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = bq2560x_read(info, BQ2560X_REG_0, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ2560X_REG_LIMIT_CURRENT_MASK;
	*limit_cur = reg_val * BQ2560X_REG_IINLIM_BASE * 1000;
	if (*limit_cur >= BQ2560X_LIMIT_CURRENT_MAX)
		*limit_cur = BQ2560X_LIMIT_CURRENT_MAX;

	return 0;
}

static int bq2560x_charger_get_health(struct bq2560x_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int bq2560x_charger_get_online(struct bq2560x_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int bq2560x_charger_feed_watchdog(struct bq2560x_charger_info *info,
					 u32 val)
{
	int ret;

	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_RESET_MASK,
				  BQ2560X_REG_RESET_MASK);
	if (ret)
		dev_err(info->dev, "reset bq2560x failed\n");

	return ret;
}

static int bq2560x_charger_get_status(struct bq2560x_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int bq2560x_charger_set_status(struct bq2560x_charger_info *info,
				      int val)
{
	int ret = 0;

	if (!val && info->charging) {
		bq2560x_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = bq2560x_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void bq2560x_charger_work(struct work_struct *data)
{
	struct bq2560x_charger_info *info =
		container_of(data, struct bq2560x_charger_info, work);
	int limit_cur, cur, ret;
	bool present = bq2560x_charger_is_bat_present(info);

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

		ret = bq2560x_charger_set_limit_current(info, limit_cur);
		if (ret)
			goto out;

		ret = bq2560x_charger_set_current(info, cur);
		if (ret)
			goto out;

		ret = bq2560x_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		bq2560x_charger_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}


static int bq2560x_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct bq2560x_charger_info *info =
		container_of(nb, struct bq2560x_charger_info, usb_notify);

	info->limit = limit;

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int bq2560x_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct bq2560x_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health;
	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = bq2560x_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq2560x_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq2560x_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq2560x_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = bq2560x_charger_get_health(info, &health);
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

static int bq2560x_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct bq2560x_charger_info *info = power_supply_get_drvdata(psy);
	int ret;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq2560x_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq2560x_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = bq2560x_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = bq2560x_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = bq2560x_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq2560x_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type bq2560x_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property bq2560x_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc bq2560x_charger_desc = {
	.name			= "bq2560x_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq2560x_usb_props,
	.num_properties		= ARRAY_SIZE(bq2560x_usb_props),
	.get_property		= bq2560x_charger_usb_get_property,
	.set_property		= bq2560x_charger_usb_set_property,
	.property_is_writeable	= bq2560x_charger_property_is_writeable,
	.usb_types		= bq2560x_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(bq2560x_charger_usb_types),
};

static void bq2560x_charger_detect_status(struct bq2560x_charger_info *info)
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

static void
bq2560x_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq2560x_charger_info *info = container_of(dwork,
							 struct bq2560x_charger_info,
							 wdt_work);
	int ret;

	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_WATCHDOG_MASK,
				  BQ2560X_REG_WATCHDOG_MASK);
	if (ret) {
		dev_err(info->dev, "reset bq2560x failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static void bq2560x_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq2560x_charger_info *info = container_of(dwork,
			struct bq2560x_charger_info, otg_work);
	bool otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		ret = bq2560x_update_bits(info, BQ2560X_REG_1,
					  BQ2560X_REG_OTG_MASK,
					  BQ2560X_REG_OTG_MASK);
		if (ret)
			dev_err(info->dev, "restart bq2560x charger otg failed\n");

		otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	} while (!otg_valid && retry++ < BQ2560X_OTG_RETRY_TIMES);

	if (retry >= BQ2560X_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int bq2560x_charger_enable_otg(struct regulator_dev *dev)
{
	struct bq2560x_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_OTG_MASK,
				  BQ2560X_REG_OTG_MASK);
	if (ret) {
		dev_err(info->dev, "enable bq2560x otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(BQ2560X_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(BQ2560X_OTG_VALID_MS));

	return 0;
}

static int bq2560x_charger_disable_otg(struct regulator_dev *dev)
{
	struct bq2560x_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_OTG_MASK,
				  0);
	if (ret) {
		dev_err(info->dev, "disable bq2560x otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int bq2560x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct bq2560x_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = bq2560x_read(info, BQ2560X_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get bq2560x otg status\n");
		return ret;
	}

	val &= BQ2560X_REG_OTG_MASK;

	return val;
}

static const struct regulator_ops bq2560x_charger_vbus_ops = {
	.enable = bq2560x_charger_enable_otg,
	.disable = bq2560x_charger_disable_otg,
	.is_enabled = bq2560x_charger_vbus_is_enabled,
};

static const struct regulator_desc bq2560x_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq2560x_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
bq2560x_charger_register_vbus_regulator(struct bq2560x_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &bq2560x_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
bq2560x_charger_register_vbus_regulator(struct bq2560x_charger_info *info)
{
	return 0;
}
#endif

static int bq2560x_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct bq2560x_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;
	mutex_init(&info->lock);
	INIT_WORK(&info->work, bq2560x_charger_work);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = bq2560x_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = BQ2560X_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = BQ2560X_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = BQ2560X_DISABLE_PIN_MASK_2720;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		return -EINVAL;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	info->usb_notify.notifier_call = bq2560x_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &bq2560x_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return PTR_ERR(info->psy_usb);
	}

	ret = bq2560x_charger_hw_init(info);
	if (ret) {
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}

	bq2560x_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, bq2560x_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  bq2560x_charger_feed_watchdog_work);

	return 0;
}

static int bq2560x_charger_remove(struct i2c_client *client)
{
	struct bq2560x_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static const struct i2c_device_id bq2560x_i2c_id[] = {
	{"bq2560x_chg", 0},
	{}
};

static const struct of_device_id bq2560x_charger_of_match[] = {
	{ .compatible = "ti,bq2560x_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq2560x_charger_of_match);

static struct i2c_driver bq2560x_charger_driver = {
	.driver = {
		.name = "bq2560x_chg",
		.of_match_table = bq2560x_charger_of_match,
	},
	.probe = bq2560x_charger_probe,
	.remove = bq2560x_charger_remove,
	.id_table = bq2560x_i2c_id,
};

module_i2c_driver(bq2560x_charger_driver);
MODULE_DESCRIPTION("BQ2560X Charger Driver");
MODULE_LICENSE("GPL v2");
