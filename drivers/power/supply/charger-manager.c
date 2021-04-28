/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This driver enables to monitor battery health and control charger
 * during suspend-to-mem.
 * Charger manager depends on other devices. register this later than
 * the depending devices.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
**/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/power/charger-manager.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/thermal.h>

/*
 * Default termperature threshold for charging.
 * Every temperature units are in tenth of centigrade.
 */
#define CM_DEFAULT_RECHARGE_TEMP_DIFF	50
#define CM_DEFAULT_CHARGE_TEMP_MAX	500
#define CM_CAP_CYCLE_TRACK_TIME		15
#define CM_UVLO_OFFSET			50000
#define CM_FORCE_SET_FUEL_CAP_FULL	100

static const char * const default_event_names[] = {
	[CM_EVENT_UNKNOWN] = "Unknown",
	[CM_EVENT_BATT_FULL] = "Battery Full",
	[CM_EVENT_BATT_IN] = "Battery Inserted",
	[CM_EVENT_BATT_OUT] = "Battery Pulled Out",
	[CM_EVENT_BATT_OVERHEAT] = "Battery Overheat",
	[CM_EVENT_BATT_COLD] = "Battery Cold",
	[CM_EVENT_EXT_PWR_IN_OUT] = "External Power Attach/Detach",
	[CM_EVENT_CHG_START_STOP] = "Charging Start/Stop",
	[CM_EVENT_OTHERS] = "Other battery events"
};

static char *charger_manager_supplied_to[] = {
	"audio-ldo",
};

/*
 * Regard CM_JIFFIES_SMALL jiffies is small enough to ignore for
 * delayed works so that we can run delayed works with CM_JIFFIES_SMALL
 * without any delays.
 */
#define	CM_JIFFIES_SMALL	(2)

/* If y is valid (> 0) and smaller than x, do x = y */
#define CM_MIN_VALID(x, y)	x = (((y > 0) && ((x) > (y))) ? (y) : (x))

/*
 * Regard CM_RTC_SMALL (sec) is small enough to ignore error in invoking
 * rtc alarm. It should be 2 or larger
 */
#define CM_RTC_SMALL		(2)

#define UEVENT_BUF_SIZE		32

static LIST_HEAD(cm_list);
static DEFINE_MUTEX(cm_list_mtx);

/* About in-suspend (suspend-again) monitoring */
static struct alarm *cm_timer;

static bool cm_suspended;
static bool cm_timer_set;
static unsigned long cm_suspend_duration_ms;
static enum cm_event_types cm_event_type;
static char *cm_event_msg;

/* About normal (not suspended) monitoring */
static unsigned long polling_jiffy = ULONG_MAX; /* ULONG_MAX: no polling */
static unsigned long next_polling; /* Next appointed polling time */
static struct workqueue_struct *cm_wq; /* init at driver add */
static struct delayed_work cm_monitor_work; /* init at driver add */

static bool allow_charger_enable;
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg);

static int __init boot_calibration_mode(char *str)
{
	if (!str)
		return 0;

	if (!strncmp(str, "cali", strlen("cali")) ||
	    !strncmp(str, "autotest", strlen("autotest")))
		allow_charger_enable = true;

	return 0;
}
__setup("androidboot.mode=", boot_calibration_mode);

/**
 * is_batt_present - See if the battery presents in place.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_batt_present(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool present = false;
	int i, ret;

	switch (cm->desc->battery_present) {
	case CM_BATTERY_PRESENT:
		present = true;
		break;
	case CM_NO_BATTERY:
		break;
	case CM_FUEL_GAUGE:
		psy = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!psy)
			break;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
				&val);
		if (ret == 0 && val.intval)
			present = true;
		power_supply_put(psy);
		break;
	case CM_CHARGER_STAT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_PRESENT, &val);
			power_supply_put(psy);
			if (ret == 0 && val.intval) {
				present = true;
				break;
			}
		}
		break;
	}

	return present;
}

/**
 * is_ext_pwr_online - See if an external power source is attached to charge
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if at least one of the chargers of the battery has an external
 * power source attached to charge the battery regardless of whether it is
 * actually charging or not.
 */
static bool is_ext_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
				&val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

 /**
  * get_batt_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_batt_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CURRENT_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

 /**
  * get_batt_cur_now - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_batt_cur_now(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

/**
 *
 * get_batt_uV - Get the voltage level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uV = val.intval;
	return 0;
}
/*
 * get_batt_ocv - Get the battery ocv
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_ocv(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/*
 * get_batt_now - Get the battery voltage now
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_vol_now(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/**
 * get_batt_cap - Get the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

/**
 * set_batt_cap - Set the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int set_batt_cap(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "can not find fuel gauge device\n");
		return -ENODEV;
	}

	val.intval = cm->desc->cap;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY,
					&val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to save current battery capacity\n");

	return ret;
}

/**
 * adjust_fuel_cap - Adjust the fuel cap level
 * @cm: the Charger Manager representing the battery.
 * @cap: the adjust fuel cap level.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int adjust_fuel_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge,
					POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to adjust fuel cap\n");

	return ret;
}

/**
 * get_charger_current - Get the charging current from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
						&val);
		power_supply_put(psy);
		if (ret == 0) {
			*cur = val.intval;
			break;
		}
	}

	return ret;
}

/**
 * get_charger_limit_current - Get the charging limit current from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input limit current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_limit_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
						&val);
		power_supply_put(psy);
		if (ret == 0) {
			*cur = val.intval;
			break;
		}
	}

	return ret;
}

/**
 * is_charging - Returns true if the battery is being charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_charging(struct charger_manager *cm)
{
	int i, ret;
	bool charging = false;
	struct power_supply *psy;
	union power_supply_propval val;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	/* If at least one of the charger is charging, return yes */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		/* 1. The charger sholuld not be DISABLED */
		if (cm->emergency_stop)
			continue;
		if (!cm->charger_enabled)
			continue;

		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		/* 2. The charger should be online (ext-power) */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
				&val);
		if (ret) {
			dev_warn(cm->dev, "Cannot read ONLINE value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			power_supply_put(psy);
			continue;
		}
		if (val.intval == 0) {
			power_supply_put(psy);
			continue;
		}

		/*
		 * 3. The charger should not be FULL, DISCHARGING,
		 * or NOT_CHARGING.
		 */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS,
				&val);
		power_supply_put(psy);
		if (ret) {
			dev_warn(cm->dev, "Cannot read STATUS value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			continue;
		}
		if (val.intval == POWER_SUPPLY_STATUS_FULL ||
				val.intval == POWER_SUPPLY_STATUS_DISCHARGING ||
				val.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
			continue;

		/* Then, this is charging. */
		charging = true;
		break;
	}

	return charging;
}

/**
 * is_full_charged - Returns true if the battery is fully charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_full_charged(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	bool is_full = false;
	int ret = 0;
	int uV, uA;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return false;

	if (desc->fullbatt_full_capacity > 0) {
		val.intval = 0;

		/* Not full if capacity of fuel gauge isn't full */
		ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CHARGE_FULL, &val);
		if (!ret && val.intval > desc->fullbatt_full_capacity) {
			is_full = true;
			goto out;
		}
	}

	/* Full, if it's over the fullbatt voltage */
	if (desc->fullbatt_uV > 0 && desc->fullbatt_uA > 0) {
		ret = get_batt_uV(cm, &uV);
		if (ret)
			goto out;

		ret = get_batt_uA(cm, &uA);
		if (ret)
			goto out;

		if (uV >= desc->fullbatt_uV && uA <= desc->fullbatt_uA) {
			if (++desc->trigger_cnt > 1) {
				if (cm->desc->cap >= 100) {
					is_full = true;
				} else {
					is_full = false;
					adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
				}
				cm->desc->force_set_full = true;
			} else {
				is_full = false;
			}
			goto out;
		} else {
			is_full = false;
			desc->trigger_cnt = 0;
			goto out;
		}
	}

	/* Full, if the capacity is more than fullbatt_soc */
	if (desc->fullbatt_soc > 0) {
		val.intval = 0;

		ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		if (!ret && val.intval >= desc->fullbatt_soc) {
			is_full = true;
			goto out;
		}
	}

out:
	power_supply_put(fuel_gauge);
	return is_full;
}

/**
 * is_polling_required - Return true if need to continue polling for this CM.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_polling_required(struct charger_manager *cm)
{
	switch (cm->desc->polling_mode) {
	case CM_POLL_DISABLE:
		return false;
	case CM_POLL_ALWAYS:
		return true;
	case CM_POLL_EXTERNAL_POWER_ONLY:
		return is_ext_pwr_online(cm);
	case CM_POLL_CHARGING_ONLY:
		return is_charging(cm);
	default:
		dev_warn(cm->dev, "Incorrect polling_mode (%d)\n",
			 cm->desc->polling_mode);
	}

	return false;
}

static int try_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS,
						&val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

/**
 * try_charger_enable - Enable/Disable chargers altogether
 * @cm: the Charger Manager representing the battery.
 * @enable: true: enable / false: disable
 *
 * Note that Charger Manager keeps the charger enabled regardless whether
 * the charger is charging or not (because battery is full or no external
 * power source exists) except when CM needs to disable chargers forcibly
 * bacause of emergency causes; when the battery is overheated or too cold.
 */
static int try_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0;

	/* Ignore if it's redundant command */
	if (enable == cm->charger_enabled)
		return 0;

	if (enable) {
		if (cm->emergency_stop)
			return -EAGAIN;

		/*
		 * Enable charge is permitted in calibration mode
		 * even if use fake battery.
		 * So it will not return in calibration mode.
		 */
		if (!is_batt_present(cm) && !allow_charger_enable)
			return 0;
		/*
		 * Save start time of charging to limit
		 * maximum possible charging time.
		 */
		cm->charging_start_time = ktime_to_ms(ktime_get());
		cm->charging_end_time = 0;

		err = try_charger_enable_by_psy(cm, enable);
	} else {
		/*
		 * Save end time of charging to maintain fully charged state
		 * of battery after full-batt.
		 */
		cm->charging_start_time = 0;
		cm->charging_end_time = ktime_to_ms(ktime_get());

		err = try_charger_enable_by_psy(cm, enable);
	}

	if (!err)
		cm->charger_enabled = enable;

	return err;
}

/**
 * try_charger_restart - Restart charging.
 * @cm: the Charger Manager representing the battery.
 *
 * Restart charging by turning off and on the charger.
 */
static int try_charger_restart(struct charger_manager *cm)
{
	int err;

	if (cm->emergency_stop)
		return -EAGAIN;

	err = try_charger_enable(cm, false);
	if (err)
		return err;

	return try_charger_enable(cm, true);
}

/**
 * uevent_notify - Let users know something has changed.
 * @cm: the Charger Manager representing the battery.
 * @event: the event string.
 *
 * If @event is null, it implies that uevent_notify is called
 * by resume function. When called in the resume function, cm_suspended
 * should be already reset to false in order to let uevent_notify
 * notify the recent event during the suspend to users. While
 * suspended, uevent_notify does not notify users, but tracks
 * events so that uevent_notify can notify users later after resumed.
 */
static void uevent_notify(struct charger_manager *cm, const char *event)
{
	static char env_str[UEVENT_BUF_SIZE + 1] = "";
	static char env_str_save[UEVENT_BUF_SIZE + 1] = "";

	if (cm_suspended) {
		/* Nothing in suspended-event buffer */
		if (env_str_save[0] == 0) {
			if (!strncmp(env_str, event, UEVENT_BUF_SIZE))
				return; /* status not changed */
			strncpy(env_str_save, event, UEVENT_BUF_SIZE);
			return;
		}

		if (!strncmp(env_str_save, event, UEVENT_BUF_SIZE))
			return; /* Duplicated. */
		strncpy(env_str_save, event, UEVENT_BUF_SIZE);
		return;
	}

	if (event == NULL) {
		/* No messages pending */
		if (!env_str_save[0])
			return;

		strncpy(env_str, env_str_save, UEVENT_BUF_SIZE);
		kobject_uevent(&cm->dev->kobj, KOBJ_CHANGE);
		env_str_save[0] = 0;

		return;
	}

	/* status not changed */
	if (!strncmp(env_str, event, UEVENT_BUF_SIZE))
		return;

	/* save the status and notify the update */
	strncpy(env_str, event, UEVENT_BUF_SIZE);
	kobject_uevent(&cm->dev->kobj, KOBJ_CHANGE);

	dev_info(cm->dev, "%s\n", event);
}

/**
 * fullbatt_vchk - Check voltage drop some times after "FULL" event.
 * @work: the work_struct appointing the function
 *
 * If a user has designated "fullbatt_vchkdrop_ms/uV" values with
 * charger_desc, Charger Manager checks voltage drop after the battery
 * "FULL" event. It checks whether the voltage has dropped more than
 * fullbatt_vchkdrop_uV by calling this function after fullbatt_vchkrop_ms.
 */
static void fullbatt_vchk(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
			struct charger_manager, fullbatt_vchk_work);
	struct charger_desc *desc = cm->desc;
	int batt_ocv, err, diff;

	/* remove the appointment for fullbatt_vchk */
	cm->fullbatt_vchk_jiffies_at = 0;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		return;

	err = get_batt_ocv(cm, &batt_ocv);
	if (err) {
		dev_err(cm->dev, "%s: get_batt_ocV error(%d)\n", __func__, err);
		return;
	}

	diff = desc->fullbatt_uV - batt_ocv;
	if (diff < 0)
		return;

	dev_info(cm->dev, "VBATT dropped %duV after full-batt\n", diff);

	if (diff >= desc->fullbatt_vchkdrop_uV) {
		try_charger_restart(cm);
		uevent_notify(cm, "Recharging");
	}
}

/**
 * check_charging_duration - Monitor charging/discharging duration
 * @cm: the Charger Manager representing the battery.
 *
 * If whole charging duration exceed 'charging_max_duration_ms',
 * cm stop charging to prevent overcharge/overheat. If discharging
 * duration exceed 'discharging _max_duration_ms', charger cable is
 * attached, after full-batt, cm start charging to maintain fully
 * charged state for battery.
 */
static int check_charging_duration(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u64 curr = ktime_to_ms(ktime_get());
	u64 duration;
	int batt_ocv, diff, ret = false;

	if (!desc->charging_max_duration_ms &&
			!desc->discharging_max_duration_ms)
		return ret;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_DURATION_ABNORMAL))
		return ret;

	ret = get_batt_ocv(cm, &batt_ocv);
	if (ret) {
		dev_err(cm->dev, "failed to get battery OCV\n");
		return ret;
	}
	diff = desc->fullbatt_uV - batt_ocv;

	if (cm->charger_enabled) {
		duration = curr - cm->charging_start_time;

		if (duration > desc->charging_max_duration_ms &&
		    diff < desc->fullbatt_vchkdrop_uV) {
			dev_info(cm->dev, "Charging duration exceed %ums\n",
				 desc->charging_max_duration_ms);
			uevent_notify(cm, "Discharging");
			try_charger_enable(cm, false);
			cm->charging_status |= CM_CHARGE_DURATION_ABNORMAL;
			ret = true;
		}
	} else if (is_ext_pwr_online(cm) && !cm->charger_enabled &&
		(cm->charging_status & CM_CHARGE_DURATION_ABNORMAL)) {
		duration = curr - cm->charging_end_time;

		if (duration > desc->discharging_max_duration_ms &&
				is_ext_pwr_online(cm)) {
			dev_info(cm->dev, "Discharging duration exceed %ums\n",
				 desc->discharging_max_duration_ms);
			uevent_notify(cm, "Recharging");
			try_charger_enable(cm, true);
			cm->charging_status &= ~CM_CHARGE_DURATION_ABNORMAL;
			ret = true;
		}
	}

	if (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL) {
		dev_info(cm->dev, "Charging duration is still exceed\n");
		return true;
	}

	return ret;
}

static int cm_get_battery_temperature_by_psy(struct charger_manager *cm,
					int *temp)
{
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_TEMP,
				(union power_supply_propval *)temp);
	power_supply_put(fuel_gauge);

	return ret;
}

static int cm_get_battery_temperature(struct charger_manager *cm,
					int *temp)
{
	int ret;

	if (!cm->desc->measure_battery_temp)
		return -ENODEV;

#ifdef CONFIG_THERMAL
	if (cm->tzd_batt) {
		ret = thermal_zone_get_temp(cm->tzd_batt, temp);
		if (!ret)
			/* Calibrate temperature unit */
			*temp /= 100;
	} else
#endif
	{
		/* if-else continued from CONFIG_THERMAL */
		ret = cm_get_battery_temperature_by_psy(cm, temp);
	}

	return ret;
}

static int cm_check_thermal_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int temp, upper_limit, lower_limit;
	int ret = 0;

	ret = cm_get_battery_temperature(cm, &temp);
	if (ret) {
		/* FIXME:
		 * No information of battery temperature might
		 * occur hazadous result. We have to handle it
		 * depending on battery type.
		 */
		dev_err(cm->dev, "Failed to get battery temperature\n");
		return 0;
	}

	upper_limit = desc->temp_max;
	lower_limit = desc->temp_min;

	if (cm->emergency_stop) {
		upper_limit -= desc->temp_diff;
		lower_limit += desc->temp_diff;
	}

	if (temp > upper_limit)
		ret = CM_EVENT_BATT_OVERHEAT;
	else if (temp < lower_limit)
		ret = CM_EVENT_BATT_COLD;

	return ret;
}

static int cm_check_charge_voltage(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *fuel_gauge;
	union power_supply_propval val;
	int ret, charge_vol;

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop)
		return -EINVAL;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL))
		return -EINVAL;

	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
				&val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	charge_vol = val.intval;

	if (cm->charger_enabled && charge_vol > desc->charge_voltage_max) {
		dev_info(cm->dev, "Charging voltage is larger than %d\n",
			 desc->charge_voltage_max);
		uevent_notify(cm, "Discharging");
		try_charger_enable(cm, false);
		cm->charging_status |= CM_CHARGE_VOLTAGE_ABNORMAL;
		power_supply_changed(cm->charger_psy);
		return 0;
	} else if (is_ext_pwr_online(cm) && !cm->charger_enabled &&
		   charge_vol <= (desc->charge_voltage_max - desc->charge_voltage_drop) &&
		   (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)) {
		dev_info(cm->dev, "Charging voltage is less than %d, recharging\n",
			 desc->charge_voltage_max - desc->charge_voltage_drop);
		uevent_notify(cm, "Recharging");
		try_charger_enable(cm, true);
		cm->charging_status &= ~CM_CHARGE_VOLTAGE_ABNORMAL;
		power_supply_changed(cm->charger_psy);
		return 0;
	} else if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL) {
		dev_info(cm->dev, "Charging voltage is still abnormal\n");
		return 0;
	}

	return -EINVAL;
}

static int cm_check_charge_health(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;
	int ret, i;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL))
		return -EINVAL;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH,
						&val);
		power_supply_put(psy);
		if (ret)
			return ret;
		health = val.intval;
	}

	if (health == POWER_SUPPLY_HEALTH_UNKNOWN)
		return -ENODEV;

	if (cm->charger_enabled && health != POWER_SUPPLY_HEALTH_GOOD) {
		dev_info(cm->dev, "Charging health is not good\n");
		uevent_notify(cm, "Discharging");
		try_charger_enable(cm, false);
		cm->charging_status |= CM_CHARGE_HEALTH_ABNORMAL;
		return 0;
	} else if (is_ext_pwr_online(cm) && !cm->charger_enabled &&
		health == POWER_SUPPLY_HEALTH_GOOD &&
		(cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL)) {
		dev_info(cm->dev, "Charging health is recover good\n");
		uevent_notify(cm, "Recharging");
		try_charger_enable(cm, true);
		cm->charging_status &= ~CM_CHARGE_HEALTH_ABNORMAL;
		return 0;
	} else if (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL) {
		dev_info(cm->dev, "Charging health is still abnormal\n");
		return 0;
	}

	return -EINVAL;
}

static int cm_feed_watchdog(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int err, i;

	if (!cm->desc->wdt_interval)
		return 0;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = cm->desc->wdt_interval;
		err = power_supply_set_property(psy,
						POWER_SUPPLY_PROP_FEED_WATCHDOG,
						&val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

enum cm_manager_jeita_status {
	STATUS_BELOW_T0 = 0,
	STATUS_T0_TO_T1,
	STATUS_T1_TO_T2,
	STATUS_T2_TO_T3,
	STATUS_ABOVE_T3,
};

static bool cm_manager_adjust_current(struct charger_manager *cm,
				      int jeita_status)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int term_volt, target_cur, i, ret = -ENODEV;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_TEMP_ABNORMAL))
		return true;

	if (jeita_status > desc->jeita_tab_size)
		jeita_status = desc->jeita_tab_size;

	if (jeita_status == 0 || jeita_status == desc->jeita_tab_size) {
		dev_warn(cm->dev,
			 "stop charging due to battery overheat or cold\n");
		try_charger_enable(cm, false);
		cm->charging_status |= CM_CHARGE_TEMP_ABNORMAL;
		return false;
	}

	term_volt = desc->jeita_tab[jeita_status].term_volt;
	target_cur = desc->jeita_tab[jeita_status].current_ua;

	if (cm->desc->thm_adjust_cur >= 0 &&
	    cm->desc->thm_adjust_cur < target_cur) {
		target_cur = cm->desc->thm_adjust_cur;
		dev_info(cm->dev, "thermel current is less than jeita current\n");
	}

	dev_info(cm->dev, "target terminate voltage = %d, target current = %d\n",
		 term_volt, target_cur);

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = term_volt;
		ret = power_supply_set_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
					&val);
		if (ret) {
			power_supply_put(psy);
			dev_err(cm->dev,
				"failed to set terminate voltage, ret = %d\n",
				ret);
			continue;
		}

		val.intval = target_cur;
		ret = power_supply_set_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
					&val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev,
				"failed to set charge current, ret = %d\n",
				ret);
			continue;
		}
	}

	if (ret)
		return false;

	try_charger_enable(cm, true);
	cm->charging_status &= ~CM_CHARGE_TEMP_ABNORMAL;
	return true;
}

static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp)
{
	struct charger_desc *desc = cm->desc;
	static int jeita_status;
	int i;

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
		if ((cur_temp >= desc->jeita_tab[i].temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].temp && i == 0)) {
			break;
		}
	}

	switch (i) {
	case 3:
		jeita_status = STATUS_ABOVE_T3;
		break;

	case 2:
		if (jeita_status != STATUS_ABOVE_T3 ||
		    cur_temp <= desc->jeita_tab[3].recovery_temp)
			jeita_status = STATUS_T2_TO_T3;
		break;

	case 1:
		if ((jeita_status != STATUS_T2_TO_T3 ||
		     cur_temp <= desc->jeita_tab[2].recovery_temp) &&
		    (jeita_status != STATUS_T0_TO_T1 ||
		     cur_temp >= desc->jeita_tab[1].recovery_temp))
			jeita_status = STATUS_T1_TO_T2;
		break;

	case 0:
		if (jeita_status != STATUS_BELOW_T0 ||
		    cur_temp >= desc->jeita_tab[0].recovery_temp)
			jeita_status = STATUS_T0_TO_T1;
		break;

	default:
		jeita_status = STATUS_BELOW_T0;
		break;
	}

	return jeita_status;
}

static int cm_manager_jeita_current_monitor(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	static int last_jeita_status = -1, temp_up_trigger, temp_down_trigger;
	int cur_jeita_status, cur_temp, ret;
	static bool is_normal = true;

	if (!desc->jeita_tab_size)
		return 0;

	if (!is_ext_pwr_online(cm)) {
		if (last_jeita_status != -1)
			last_jeita_status = -1;

		return 0;
	}

	if (desc->jeita_disabled) {
		if (last_jeita_status != STATUS_T1_TO_T2) {
			dev_info(cm->dev, "Disable jeita and force jeita state to STATUS_T1_TO_T2\n");
			last_jeita_status = STATUS_T1_TO_T2;
			desc->thm_adjust_cur = -EINVAL;
			cm_manager_adjust_current(cm, last_jeita_status);
		}

		return 0;
	}

	ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		return ret;
	}

	cur_jeita_status = cm_manager_get_jeita_status(cm, cur_temp);

	dev_info(cm->dev, "current-last jeita status: %d-%d, current temperature: %d\n",
		 cur_jeita_status, last_jeita_status, cur_temp);

	/*
	 * We should give a initial jeita status with adjusting the charging
	 * current when pluging in the cabel.
	 */
	if (last_jeita_status == -1) {
		is_normal = cm_manager_adjust_current(cm, cur_jeita_status);
		last_jeita_status = cur_jeita_status;
		goto out;
	}

	if (cur_jeita_status > last_jeita_status) {
		temp_down_trigger = 0;

		if (++temp_up_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else if (cur_jeita_status < last_jeita_status) {
		temp_up_trigger = 0;

		if (++temp_down_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else {
		temp_up_trigger = 0;
		temp_down_trigger = 0;
	}

out:
	if (!is_normal)
		return -EAGAIN;

	return 0;
}

/**
 * _cm_monitor - Monitor the temperature and return true for exceptions.
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if there is an event to notify for the battery.
 * (True if the status of "emergency_stop" changes)
 */
static bool _cm_monitor(struct charger_manager *cm)
{
	int temp_alrt, ret;
	int i;

	/* Feed the charger watchdog if necessary */
	ret = cm_feed_watchdog(cm);
	if (ret) {
		dev_warn(cm->dev, "Failed to feed charger watchdog\n");
		return false;
	}

	for (i = 0; i < cm->desc->num_charger_regulators; i++) {
		if (cm->desc->charger_regulators[i].externally_control) {
			dev_info(cm->dev,
				 "Charger has been controlled externally, so no need monitoring\n");
			return false;
		}
	}

	temp_alrt = cm_check_thermal_status(cm);

	/* It has been stopped already */
	if (temp_alrt && cm->emergency_stop) {
		dev_warn(cm->dev,
			 "Emergency stop, temperature alert = %d\n", temp_alrt);
		return false;
	}

	/*
	 * Adjust the charging current according to current battery
	 * temperature jeita table.
	 */
	ret = cm_manager_jeita_current_monitor(cm);
	if (ret) {
		dev_warn(cm->dev,
			 "Errors orrurs when adjusting charging current\n");
		return false;
	}

	/*
	 * Check temperature whether overheat or cold.
	 * If temperature is out of range normal state, stop charging.
	 */
	if (temp_alrt) {
		cm->emergency_stop = temp_alrt;
		dev_info(cm->dev,
			"Temperature is out of range normal state, stop charging\n");
		if (!try_charger_enable(cm, false))
			uevent_notify(cm, default_event_names[temp_alrt]);
	/*
	 * Check if the charge voltage is in the normal range.
	 */
	} else if (!cm->emergency_stop && !cm_check_charge_voltage(cm)) {
		dev_info(cm->dev,
			"Stop charging/Recharging due to charge voltage changes\n");
	/*
	 * Check if the charge health is in the normal mode.
	 */
	} else if (!cm->emergency_stop && !cm_check_charge_health(cm)) {
		dev_info(cm->dev,
			"Stop charging/Recharging due to charge health changes\n");
	/*
	 * Check whole charging duration and discharing duration
	 * after full-batt.
	 */
	} else if (!cm->emergency_stop && check_charging_duration(cm)) {
		dev_info(cm->dev,
			"Charging/Discharging duration is out of range\n");
	/*
	 * Check dropped voltage of battery. If battery voltage is more
	 * dropped than fullbatt_vchkdrop_uV after fully charged state,
	 * charger-manager have to recharge battery.
	 */
	} else if (!cm->emergency_stop && is_ext_pwr_online(cm) &&
			!cm->charger_enabled) {
		dev_info(cm->dev, "Check dropped voltage of battery\n");
		fullbatt_vchk(&cm->fullbatt_vchk_work.work);

	/*
	 * Check whether fully charged state to protect overcharge
	 * if charger-manager is charging for battery.
	 */
	} else if (!cm->emergency_stop && is_full_charged(cm) &&
			cm->charger_enabled) {
		dev_info(cm->dev, "EVENT_HANDLE: Battery Fully Charged\n");
		uevent_notify(cm, default_event_names[CM_EVENT_BATT_FULL]);

		try_charger_enable(cm, false);

		fullbatt_vchk(&cm->fullbatt_vchk_work.work);
	} else {
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		if (is_ext_pwr_online(cm)) {
			dev_info(cm->dev, "No emergency stop, charging\n");
			if (!try_charger_enable(cm, true))
				uevent_notify(cm, "CHARGING");
		}
	}

	return true;
}

/**
 * cm_monitor - Monitor every battery.
 *
 * Returns true if there is an event to notify from any of the batteries.
 * (True if the status of "emergency_stop" changes)
 */
static bool cm_monitor(void)
{
	bool stop = false;
	struct charger_manager *cm;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (_cm_monitor(cm))
			stop = true;
	}

	mutex_unlock(&cm_list_mtx);

	return stop;
}

/**
 * _setup_polling - Setup the next instance of polling.
 * @work: work_struct of the function _setup_polling.
 */
static void _setup_polling(struct work_struct *work)
{
	unsigned long min = ULONG_MAX;
	struct charger_manager *cm;
	bool keep_polling = false;
	unsigned long _next_polling;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
			keep_polling = true;

			if (min > cm->desc->polling_interval_ms)
				min = cm->desc->polling_interval_ms;
		}
	}

	polling_jiffy = msecs_to_jiffies(min);
	if (polling_jiffy <= CM_JIFFIES_SMALL)
		polling_jiffy = CM_JIFFIES_SMALL + 1;

	if (!keep_polling)
		polling_jiffy = ULONG_MAX;
	if (polling_jiffy == ULONG_MAX)
		goto out;

	WARN(cm_wq == NULL, "charger-manager: workqueue not initialized"
			    ". try it later. %s\n", __func__);

	/*
	 * Use mod_delayed_work() iff the next polling interval should
	 * occur before the currently scheduled one.  If @cm_monitor_work
	 * isn't active, the end result is the same, so no need to worry
	 * about stale @next_polling.
	 */
	_next_polling = jiffies + polling_jiffy;

	if (time_before(_next_polling, next_polling)) {
		mod_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy);
		next_polling = _next_polling;
	} else {
		if (queue_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy))
			next_polling = _next_polling;
	}
out:
	mutex_unlock(&cm_list_mtx);
}
static DECLARE_WORK(setup_polling, _setup_polling);

/**
 * cm_monitor_poller - The Monitor / Poller.
 * @work: work_struct of the function cm_monitor_poller
 *
 * During non-suspended state, cm_monitor_poller is used to poll and monitor
 * the batteries.
 */
static void cm_monitor_poller(struct work_struct *work)
{
	cm_monitor();
	schedule_work(&setup_polling);
}

/**
 * fullbatt_handler - Event handler for CM_EVENT_BATT_FULL
 * @cm: the Charger Manager representing the battery.
 */
static void fullbatt_handler(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		goto out;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	mod_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
			 msecs_to_jiffies(desc->fullbatt_vchkdrop_ms));
	cm->fullbatt_vchk_jiffies_at = jiffies + msecs_to_jiffies(
				       desc->fullbatt_vchkdrop_ms);

	if (cm->fullbatt_vchk_jiffies_at == 0)
		cm->fullbatt_vchk_jiffies_at = 1;

out:
	dev_info(cm->dev, "EVENT_HANDLE: Battery Fully Charged\n");
	uevent_notify(cm, default_event_names[CM_EVENT_BATT_FULL]);
}

/**
 * battout_handler - Event handler for CM_EVENT_BATT_OUT
 * @cm: the Charger Manager representing the battery.
 */
static void battout_handler(struct charger_manager *cm)
{
	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (!is_batt_present(cm)) {
		dev_emerg(cm->dev, "Battery Pulled Out!\n");
		try_charger_enable(cm, false);
		uevent_notify(cm, default_event_names[CM_EVENT_BATT_OUT]);
	} else {
		dev_emerg(cm->dev, "Battery Pulled in!\n");
		try_charger_enable(cm, true);
		uevent_notify(cm, default_event_names[CM_EVENT_BATT_IN]);
	}
}

/**
 * misc_event_handler - Handler for other evnets
 * @cm: the Charger Manager representing the battery.
 * @type: the Charger Manager representing the battery.
 */
static void misc_event_handler(struct charger_manager *cm,
			enum cm_event_types type)
{
	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (is_ext_pwr_online(cm))
		try_charger_enable(cm, true);
	else
		try_charger_enable(cm, false);

	if (cm->desc->force_set_full)
		cm->desc->force_set_full = false;

	if (cm->charging_status)
		cm->charging_status = 0;

	if (is_polling_required(cm) && cm->desc->polling_interval_ms)
		schedule_work(&setup_polling);
	uevent_notify(cm, default_event_names[type]);
}

static int charger_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	struct power_supply *fuel_gauge = NULL;
	int ret = 0;
	int i;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (is_charging(cm)) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else if (is_ext_pwr_online(cm)) {
			if (is_full_charged(cm) || cm->desc->force_set_full)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (cm->emergency_stop > 0)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (cm->emergency_stop < 0)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (is_batt_present(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = get_batt_uV(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}
		ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CURRENT_AVG, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = get_batt_vol_now(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = get_batt_cur_now(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		return cm_get_battery_temperature_by_psy(cm, &val->intval);
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		return cm_get_battery_temperature(cm, &val->intval);
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!is_batt_present(cm)) {
			/* There is no battery. Assume 100% */
			val->intval = 100;
			break;
		}
		val->intval = cm->desc->cap;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (is_ext_pwr_online(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		fuel_gauge = power_supply_get_by_name(
					cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}

		ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
						val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		if (is_charging(cm)) {
			fuel_gauge = power_supply_get_by_name(
					cm->desc->psy_fuel_gauge);
			if (!fuel_gauge) {
				ret = -ENODEV;
				break;
			}

			ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_CHARGE_NOW,
						val);
			if (ret) {
				val->intval = 1;
				ret = 0;
			} else {
				/* If CHARGE_NOW is supplied, use it */
				val->intval = (val->intval > 0) ?
						val->intval : 1;
			}
		} else {
			val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "set charge current failed\n");
				continue;
			}
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "set charge limit current failed\n");
				continue;
			}
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		fuel_gauge = power_supply_get_by_name(
					cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}

		ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
						val);
		if (ret) {
			val->intval = 1;
			ret = 0;
		} else {
			/* If CHARGE_COUNTER is supplied, use it */
			val->intval = val->intval > 0 ? val->intval : 1;
			val->intval = (cm->desc->cap * val->intval) / 100;
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
							val);
			if (!ret) {
				power_supply_put(psy);
				break;
			}

			ret = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
							val);
			if (!ret) {
				power_supply_put(psy);
				break;
			}
		}
		break;

	default:
		return -EINVAL;
	}
	if (fuel_gauge)
		power_supply_put(fuel_gauge);
	return ret;
}

static int
charger_set_property(struct power_supply *psy,
		     enum power_supply_property psp,
		     const union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	union power_supply_propval thermal_val;
	int cur_temp, cur_jeita_status;
	int ret = 0;
	int i;

	if (!is_ext_pwr_online(cm))
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "set charge current failed\n");
				continue;
			}
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "set charge limit current failed\n");
				continue;
			}
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		cm->desc->thm_adjust_cur = val->intval;
		ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
		if (ret) {
			dev_err(cm->dev, "failed to get battery temperature\n");
			return ret;
		}

		thermal_val.intval = val->intval;

		if (cm->desc->jeita_tab_size) {
			cur_jeita_status = cm_manager_get_jeita_status(cm, cur_temp);
			if (val->intval > cm->desc->jeita_tab[cur_jeita_status].current_ua)
				thermal_val.intval = cm->desc->jeita_tab[cur_jeita_status].current_ua;
		}

		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_set_property(psy,
							POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
							(const union power_supply_propval *)&thermal_val);
			if (!ret) {
				power_supply_put(psy);
				break;
			}

			ret = power_supply_set_property(psy,
							POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
							(const union power_supply_propval *)&thermal_val);
			if (!ret) {
				power_supply_put(psy);
				break;
			}
		}
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int charger_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}
#define NUM_CHARGER_PSY_OPTIONAL	(4)
static enum power_supply_property default_charger_props[] = {
	/* Guaranteed to provide */
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	/*
	 * Optional properties are:
	 * POWER_SUPPLY_PROP_CHARGE_NOW,
	 * POWER_SUPPLY_PROP_CURRENT_NOW,
	 * POWER_SUPPLY_PROP_TEMP, and
	 * POWER_SUPPLY_PROP_TEMP_AMBIENT,
	 */
};

static const struct power_supply_desc psy_default = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = default_charger_props,
	.num_properties = ARRAY_SIZE(default_charger_props),
	.get_property = charger_get_property,
	.set_property = charger_set_property,
	.property_is_writeable	= charger_property_is_writeable,
	.no_thermal = true,
};

/**
 * cm_setup_timer - For in-suspend monitoring setup wakeup alarm
 *		    for suspend_again.
 *
 * Returns true if the alarm is set for Charger Manager to use.
 * Returns false if
 *	cm_setup_timer fails to set an alarm,
 *	cm_setup_timer does not need to set an alarm for Charger Manager,
 *	or an alarm previously configured is to be used.
 */
static bool cm_setup_timer(void)
{
	struct charger_manager *cm;
	unsigned int wakeup_ms = UINT_MAX;
	int timer_req = 0;

	if (time_after(next_polling, jiffies))
		CM_MIN_VALID(wakeup_ms,
			jiffies_to_msecs(next_polling - jiffies));

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		unsigned int fbchk_ms = 0;

		/* fullbatt_vchk is required. setup timer for that */
		if (cm->fullbatt_vchk_jiffies_at) {
			fbchk_ms = jiffies_to_msecs(cm->fullbatt_vchk_jiffies_at
						    - jiffies);
			if (time_is_before_eq_jiffies(
				cm->fullbatt_vchk_jiffies_at) ||
				msecs_to_jiffies(fbchk_ms) < CM_JIFFIES_SMALL) {
				fullbatt_vchk(&cm->fullbatt_vchk_work.work);
				fbchk_ms = 0;
			}
		}
		CM_MIN_VALID(wakeup_ms, fbchk_ms);

		/* Skip if polling is not required for this CM */
		if (!is_polling_required(cm) && !cm->emergency_stop)
			continue;
		timer_req++;
		if (cm->desc->polling_interval_ms == 0)
			continue;
		CM_MIN_VALID(wakeup_ms, cm->desc->polling_interval_ms);
	}
	mutex_unlock(&cm_list_mtx);

	if (timer_req && cm_timer) {
		ktime_t now, add;

		/*
		 * Set alarm with the polling interval (wakeup_ms)
		 * The alarm time should be NOW + CM_RTC_SMALL or later.
		 */
		if (wakeup_ms == UINT_MAX ||
			wakeup_ms < CM_RTC_SMALL * MSEC_PER_SEC)
			wakeup_ms = 2 * CM_RTC_SMALL * MSEC_PER_SEC;

		pr_info("Charger Manager wakeup timer: %u ms\n", wakeup_ms);

		now = ktime_get_boottime();
		add = ktime_set(wakeup_ms / MSEC_PER_SEC,
				(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
		alarm_start(cm_timer, ktime_add(now, add));

		cm_suspend_duration_ms = wakeup_ms;

		return true;
	}
	return false;
}

/**
 * charger_extcon_notifier - receive the state of charger cable
 *			when registered cable is attached or detached.
 *
 * @self: the notifier block of the charger_extcon_notifier.
 * @event: the cable state.
 * @ptr: the data pointer of notifier block.
 */
static int charger_extcon_notifier(struct notifier_block *self,
			unsigned long event, void *ptr)
{
	struct charger_cable *cable =
		container_of(self, struct charger_cable, nb);

	/*
	 * The newly state of charger cable.
	 * If cable is attached, cable->attached is true.
	 */
	cable->attached = event;

	/*
	 * Setup monitoring to check battery state
	 * when charger cable is attached.
	 */
	if (cable->attached && is_polling_required(cable->cm)) {
		cancel_work_sync(&setup_polling);
		schedule_work(&setup_polling);
	}

	return NOTIFY_DONE;
}

/**
 * charger_extcon_init - register external connector to use it
 *			as the charger cable
 *
 * @cm: the Charger Manager representing the battery.
 * @cable: the Charger cable representing the external connector.
 */
static int charger_extcon_init(struct charger_manager *cm,
		struct charger_cable *cable)
{
	int ret;

	/*
	 * Charger manager use Extcon framework to identify
	 * the charger cable among various external connector
	 * cable (e.g., TA, USB, MHL, Dock).
	 */
	cable->nb.notifier_call = charger_extcon_notifier;
	ret = devm_extcon_register_notifier(cm->dev, cable->extcon_dev,
					    EXTCON_USB, &cable->nb);
	if (ret < 0)
		dev_err(cm->dev, "Cannot register extcon_dev for (cable: %s)\n",
			cable->name);

	return ret;
}

/**
 * charger_manager_register_extcon - Register extcon device to recevie state
 *				     of charger cable.
 * @cm: the Charger Manager representing the battery.
 *
 * This function support EXTCON(External Connector) subsystem to detect the
 * state of charger cables for enabling or disabling charger(regulator) and
 * select the charger cable for charging among a number of external cable
 * according to policy of H/W board.
 */
static int charger_manager_register_extcon(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	int ret;
	int i;
	int j;

	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		charger->consumer = regulator_get(cm->dev,
					charger->regulator_name);
		if (IS_ERR(charger->consumer)) {
			dev_err(cm->dev, "Cannot find charger(%s)\n",
				charger->regulator_name);
			return PTR_ERR(charger->consumer);
		}
		charger->cm = cm;

		for (j = 0; j < charger->num_cables; j++) {
			struct charger_cable *cable = &charger->cables[j];

			ret = charger_extcon_init(cm, cable);
			if (ret < 0) {
				dev_err(cm->dev, "Cannot initialize charger(%s)\n",
					charger->regulator_name);
				return ret;
			}
			cable->charger = charger;
			cable->cm = cm;
		}
	}

	return 0;
}

/* help function of sysfs node to control charger(regulator) */
static ssize_t charger_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator, attr_name);

	return sprintf(buf, "%s\n", charger->regulator_name);
}

static ssize_t charger_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator, attr_state);
	int state = 0;

	if (!charger->externally_control)
		state = regulator_is_enabled(charger->consumer);

	return sprintf(buf, "%s\n", state ? "enabled" : "disabled");
}

static ssize_t jeita_control_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_jeita_control);
	struct charger_desc *desc = charger->cm->desc;

	return sprintf(buf, "%d\n", !desc->jeita_disabled);
}

static ssize_t jeita_control_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_jeita_control);
	struct charger_desc *desc = charger->cm->desc;
	bool enabled;

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	desc->jeita_disabled = !enabled;

	return count;
}

static ssize_t charger_stop_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
			       attr_stop_charge);
	bool stop_charge;

	stop_charge = is_charging(charger->cm);

	return sprintf(buf, "%d\n", !stop_charge);
}

static ssize_t charger_stop_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
					attr_stop_charge);
	struct charger_manager *cm = charger->cm;
	int stop_charge, ret;

	ret = sscanf(buf, "%d", &stop_charge);
	if (!ret)
		return -EINVAL;

	if (!is_ext_pwr_online(cm))
		return -EINVAL;

	if (!stop_charge) {
		ret = try_charger_enable(cm, true);
		if (ret) {
			dev_err(cm->dev, "failed to start charger.\n");
			return ret;
		}
		charger->externally_control = false;
	} else {
		ret = try_charger_enable(cm, false);
		if (ret) {
			dev_err(cm->dev, "failed to stop charger.\n");
			return ret;
		}
		charger->externally_control = true;
	}

	power_supply_changed(cm->charger_psy);

	return count;
}

static ssize_t charger_externally_control_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger = container_of(attr,
			struct charger_regulator, attr_externally_control);

	return sprintf(buf, "%d\n", charger->externally_control);
}

static ssize_t charger_externally_control_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
					attr_externally_control);
	struct charger_manager *cm = charger->cm;
	struct charger_desc *desc = cm->desc;
	int i;
	int ret;
	int externally_control;
	int chargers_externally_control = 1;

	ret = sscanf(buf, "%d", &externally_control);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	if (!externally_control) {
		charger->externally_control = 0;
		return count;
	}

	for (i = 0; i < desc->num_charger_regulators; i++) {
		if (&desc->charger_regulators[i] != charger &&
			!desc->charger_regulators[i].externally_control) {
			/*
			 * At least, one charger is controlled by
			 * charger-manager
			 */
			chargers_externally_control = 0;
			break;
		}
	}

	if (!chargers_externally_control) {
		if (cm->charger_enabled) {
			try_charger_enable(charger->cm, false);
			charger->externally_control = externally_control;
			try_charger_enable(charger->cm, true);
		} else {
			charger->externally_control = externally_control;
		}
	} else {
		dev_warn(cm->dev,
			 "'%s' regulator should be controlled in charger-manager because charger-manager must need at least one charger for charging\n",
			 charger->regulator_name);
	}

	return count;
}

/**
 * charger_manager_register_sysfs - Register sysfs entry for each charger
 * @cm: the Charger Manager representing the battery.
 *
 * This function add sysfs entry for charger(regulator) to control charger from
 * user-space. If some development board use one more chargers for charging
 * but only need one charger on specific case which is dependent on user
 * scenario or hardware restrictions, the user enter 1 or 0(zero) to '/sys/
 * class/power_supply/battery/charger.[index]/externally_control'. For example,
 * if user enter 1 to 'sys/class/power_supply/battery/charger.[index]/
 * externally_control, this charger isn't controlled from charger-manager and
 * always stay off state of regulator.
 */
static int charger_manager_register_sysfs(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	int chargers_externally_control = 1;
	char buf[11];
	char *str;
	int ret;
	int i;

	/* Create sysfs entry to control charger(regulator) */
	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		snprintf(buf, 10, "charger.%d", i);
		str = devm_kzalloc(cm->dev,
				sizeof(char) * (strlen(buf) + 1), GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		strcpy(str, buf);

		charger->attrs[0] = &charger->attr_name.attr;
		charger->attrs[1] = &charger->attr_state.attr;
		charger->attrs[2] = &charger->attr_externally_control.attr;
		charger->attrs[3] = &charger->attr_stop_charge.attr;
		charger->attrs[4] = &charger->attr_jeita_control.attr;
		charger->attrs[5] = NULL;
		charger->attr_g.name = str;
		charger->attr_g.attrs = charger->attrs;

		sysfs_attr_init(&charger->attr_name.attr);
		charger->attr_name.attr.name = "name";
		charger->attr_name.attr.mode = 0444;
		charger->attr_name.show = charger_name_show;

		sysfs_attr_init(&charger->attr_state.attr);
		charger->attr_state.attr.name = "state";
		charger->attr_state.attr.mode = 0444;
		charger->attr_state.show = charger_state_show;

		sysfs_attr_init(&charger->attr_stop_charge.attr);
		charger->attr_stop_charge.attr.name = "stop_charge";
		charger->attr_stop_charge.attr.mode = 0644;
		charger->attr_stop_charge.show = charger_stop_show;
		charger->attr_stop_charge.store = charger_stop_store;

		sysfs_attr_init(&charger->attr_jeita_control.attr);
		charger->attr_jeita_control.attr.name = "jeita_control";
		charger->attr_jeita_control.attr.mode = 0644;
		charger->attr_jeita_control.show = jeita_control_show;
		charger->attr_jeita_control.store = jeita_control_store;

		sysfs_attr_init(&charger->attr_externally_control.attr);
		charger->attr_externally_control.attr.name
				= "externally_control";
		charger->attr_externally_control.attr.mode = 0644;
		charger->attr_externally_control.show
				= charger_externally_control_show;
		charger->attr_externally_control.store
				= charger_externally_control_store;

		if (!desc->charger_regulators[i].externally_control ||
				!chargers_externally_control)
			chargers_externally_control = 0;

		dev_info(cm->dev, "'%s' regulator's externally_control is %d\n",
			 charger->regulator_name, charger->externally_control);

		ret = sysfs_create_group(&cm->charger_psy->dev.kobj,
					&charger->attr_g);
		if (ret < 0) {
			dev_err(cm->dev, "Cannot create sysfs entry of %s regulator\n",
				charger->regulator_name);
			return ret;
		}
	}

	if (chargers_externally_control) {
		dev_err(cm->dev, "Cannot register regulator because charger-manager must need at least one charger for charging battery\n");
		return -EINVAL;
	}

	return 0;
}

static int cm_init_thermal_data(struct charger_manager *cm,
		struct power_supply *fuel_gauge)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	int ret;

	/* Verify whether fuel gauge provides battery temperature */
	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_TEMP, &val);

	if (!ret) {
		cm->charger_psy_desc.properties[cm->charger_psy_desc.num_properties] =
				POWER_SUPPLY_PROP_TEMP;
		cm->charger_psy_desc.num_properties++;
		cm->desc->measure_battery_temp = true;
	}
#ifdef CONFIG_THERMAL
	if (desc->thermal_zone) {
		cm->tzd_batt =
			thermal_zone_get_zone_by_name(desc->thermal_zone);
		if (IS_ERR(cm->tzd_batt))
			return PTR_ERR(cm->tzd_batt);

		/* Use external thermometer */
		cm->charger_psy_desc.properties[cm->charger_psy_desc.num_properties] =
				POWER_SUPPLY_PROP_TEMP_AMBIENT;
		cm->charger_psy_desc.num_properties++;
		cm->desc->measure_battery_temp = true;
		ret = 0;
	}
#endif
	if (cm->desc->measure_battery_temp) {
		/* NOTICE : Default allowable minimum charge temperature is 0 */
		if (!desc->temp_max)
			desc->temp_max = CM_DEFAULT_CHARGE_TEMP_MAX;
		if (!desc->temp_diff)
			desc->temp_diff = CM_DEFAULT_RECHARGE_TEMP_DIFF;
	}

	return ret;
}

static int cm_init_jeita_table(struct charger_desc *desc, struct device *dev)
{
	struct device_node *np = dev->of_node;
	const __be32 *list;
	int i, size;

	list = of_get_property(np, "cm-jeita-temp-table", &size);
	if (!list || !size)
		return 0;

	desc->jeita_tab_size = size / (4 * sizeof(__be32));
	desc->jeita_tab = devm_kzalloc(dev, sizeof(struct charger_jeita_table) *
				       (desc->jeita_tab_size + 1), GFP_KERNEL);
	if (!desc->jeita_tab)
		return -ENOMEM;

	for (i = 0; i < desc->jeita_tab_size; i++) {
		desc->jeita_tab[i].temp = be32_to_cpu(*list++) - 1000;
		desc->jeita_tab[i].recovery_temp = be32_to_cpu(*list++) - 1000;
		desc->jeita_tab[i].current_ua = be32_to_cpu(*list++);
		desc->jeita_tab[i].term_volt = be32_to_cpu(*list++);
	}

	return 0;
}

static const struct of_device_id charger_manager_match[] = {
	{
		.compatible = "charger-manager",
	},
	{},
};

static struct charger_desc *of_cm_parse_desc(struct device *dev)
{
	struct charger_desc *desc;
	struct device_node *np = dev->of_node;
	u32 poll_mode = CM_POLL_DISABLE;
	u32 battery_stat = CM_NO_BATTERY;
	int num_chgs = 0, ret;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "cm-name", &desc->psy_name);

	of_property_read_u32(np, "cm-poll-mode", &poll_mode);
	desc->polling_mode = poll_mode;

	of_property_read_u32(np, "cm-poll-interval",
				&desc->polling_interval_ms);

	of_property_read_u32(np, "cm-fullbatt-vchkdrop-ms",
					&desc->fullbatt_vchkdrop_ms);
	of_property_read_u32(np, "cm-fullbatt-vchkdrop-volt",
					&desc->fullbatt_vchkdrop_uV);
	of_property_read_u32(np, "cm-fullbatt-voltage", &desc->fullbatt_uV);
	of_property_read_u32(np, "cm-fullbatt-current", &desc->fullbatt_uA);
	of_property_read_u32(np, "cm-fullbatt-soc", &desc->fullbatt_soc);
	of_property_read_u32(np, "cm-fullbatt-capacity",
					&desc->fullbatt_full_capacity);
	of_property_read_u32(np, "cm-shutdown-voltage", &desc->shutdown_voltage);
	of_property_read_u32(np, "cm-tickle-time-out", &desc->trickle_time_out);
	of_property_read_u32(np, "cm-one-cap-time", &desc->cap_one_time);
	of_property_read_u32(np, "cm-wdt-interval", &desc->wdt_interval);

	of_property_read_u32(np, "cm-battery-stat", &battery_stat);
	desc->battery_present = battery_stat;

	/* chargers */
	of_property_read_u32(np, "cm-num-chargers", &num_chgs);
	if (num_chgs) {
		/* Allocate empty bin at the tail of array */
		desc->psy_charger_stat = devm_kzalloc(dev, sizeof(char *)
						* (num_chgs + 1), GFP_KERNEL);
		if (desc->psy_charger_stat) {
			int i;
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-chargers",
						i, &desc->psy_charger_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	of_property_read_string(np, "cm-fuel-gauge", &desc->psy_fuel_gauge);

	of_property_read_string(np, "cm-thermal-zone", &desc->thermal_zone);

	of_property_read_u32(np, "cm-battery-cold", &desc->temp_min);
	if (of_get_property(np, "cm-battery-cold-in-minus", NULL))
		desc->temp_min *= -1;
	of_property_read_u32(np, "cm-battery-hot", &desc->temp_max);
	of_property_read_u32(np, "cm-battery-temp-diff", &desc->temp_diff);

	of_property_read_u32(np, "cm-charging-max",
				&desc->charging_max_duration_ms);
	of_property_read_u32(np, "cm-discharging-max",
				&desc->discharging_max_duration_ms);
	of_property_read_u32(np, "cm-charge-voltage-max",
			     &desc->charge_voltage_max);
	of_property_read_u32(np, "cm-charge-voltage-drop",
			     &desc->charge_voltage_drop);

	/* Initialize the jeita temperature table. */
	ret = cm_init_jeita_table(desc, dev);
	if (ret)
		return ERR_PTR(ret);

	/* battery charger regualtors */
	desc->num_charger_regulators = of_get_child_count(np);
	if (desc->num_charger_regulators) {
		struct charger_regulator *chg_regs;
		struct device_node *child;

		chg_regs = devm_kzalloc(dev, sizeof(*chg_regs)
					* desc->num_charger_regulators,
					GFP_KERNEL);
		if (!chg_regs)
			return ERR_PTR(-ENOMEM);

		desc->charger_regulators = chg_regs;

		for_each_child_of_node(np, child) {
			struct charger_cable *cables;
			struct device_node *_child;

			of_property_read_string(child, "cm-regulator-name",
					&chg_regs->regulator_name);

			/* charger cables */
			chg_regs->num_cables = of_get_child_count(child);
			if (chg_regs->num_cables) {
				cables = devm_kzalloc(dev, sizeof(*cables)
						* chg_regs->num_cables,
						GFP_KERNEL);
				if (!cables) {
					of_node_put(child);
					return ERR_PTR(-ENOMEM);
				}

				chg_regs->cables = cables;

				for_each_child_of_node(child, _child) {
					of_property_read_string(_child,
					"cm-cable-name", &cables->name);
					of_property_read_u32(_child,
					"cm-cable-min",
					&cables->min_uA);
					of_property_read_u32(_child,
					"cm-cable-max",
					&cables->max_uA);

					if (of_property_read_bool(_child, "extcon")) {
						struct device_node *np;

						np = of_parse_phandle(_child, "extcon", 0);
						if (!np)
							return ERR_PTR(-ENODEV);

						cables->extcon_dev = extcon_find_edev_by_node(np);
						of_node_put(np);
						if (IS_ERR(cables->extcon_dev))
							return ERR_PTR(PTR_ERR(cables->extcon_dev));
					}
					cables++;
				}
			}
			chg_regs++;
		}
	}
	return desc;
}

static inline struct charger_desc *cm_get_drv_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		return of_cm_parse_desc(&pdev->dev);
	return dev_get_platdata(&pdev->dev);
}

static void cm_batt_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, cap_update_work);
	struct timespec64 cur_time;
	int batt_uV, batt_ocV, bat_uA, fuel_cap, chg_sts, ret;
	int period_time, flush_time;
	int chg_cur = 0, chg_limit_cur = 0;

	ret = get_batt_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_batt_uV error.\n");
		return;
	}

	ret = get_batt_ocv(cm, &batt_ocV);
	if (ret) {
		dev_err(cm->dev, "get_batt_ocV error.\n");
		return;
	}

	ret = get_batt_uA(cm, &bat_uA);
	if (ret) {
		dev_err(cm->dev, "get bat_uA error.\n");
		return;
	}

	ret = get_batt_cap(cm, &fuel_cap);
	if (ret) {
		dev_err(cm->dev, "get fuel_cap error.\n");
		return;
	}

	ret = get_charger_current(cm, &chg_cur);
	if (ret) {
		dev_err(cm->dev, "get chg_cur error.\n");
		return;
	}

	ret = get_charger_limit_current(cm, &chg_limit_cur);
	if (ret) {
		dev_err(cm->dev, "get chg_limit_cur error.\n");
		return;
	}

	if (fuel_cap > 100)
		fuel_cap = 100;
	else if (fuel_cap < 0)
		fuel_cap = 0;

	cur_time = ktime_to_timespec64(ktime_get_boottime());

	if (is_full_charged(cm))
		chg_sts = POWER_SUPPLY_STATUS_FULL;
	else if (is_charging(cm))
		chg_sts = POWER_SUPPLY_STATUS_CHARGING;
	else if (is_ext_pwr_online(cm))
		chg_sts = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		chg_sts = POWER_SUPPLY_STATUS_DISCHARGING;

	/*
	 * Record the charging time when battery
	 * capacity is larger than 99%.
	 */
	if (chg_sts == POWER_SUPPLY_STATUS_CHARGING) {
		if (cm->desc->cap >= 99) {
			cm->desc->trickle_time =
				cur_time.tv_sec - cm->desc->trickle_start_time;
		} else {
			cm->desc->trickle_start_time = cur_time.tv_sec;
			cm->desc->trickle_time = 0;
		}
	} else {
		cm->desc->trickle_start_time = cur_time.tv_sec;
		cm->desc->trickle_time = cm->desc->trickle_time_out +
				cm->desc->cap_one_time;
	}

	flush_time = cur_time.tv_sec - cm->desc->update_capacity_time;
	period_time = cur_time.tv_sec - cm->desc->last_query_time;
	cm->desc->last_query_time = cur_time.tv_sec;

	if (cm->desc->force_set_full && is_ext_pwr_online(cm))
		cm->desc->charger_status = POWER_SUPPLY_STATUS_FULL;
	else
		cm->desc->charger_status = chg_sts;

	dev_info(cm->dev, "battery voltage = %d, OCV = %d, current = %d, "
		 "capacity = %d, charger status = %d, force set full = %d, "
		 "charging current = %d, charging limit current = %d\n",
		 batt_uV, batt_ocV, bat_uA, fuel_cap, cm->desc->charger_status,
		 cm->desc->force_set_full, chg_cur, chg_limit_cur);

	switch (cm->desc->charger_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (fuel_cap < cm->desc->cap) {
			if (bat_uA >= 0) {
				fuel_cap = cm->desc->cap;
			} else {
				/*
				 * The percentage of electricity is not
				 * allowed to change by 1% in cm->desc->cap_one_time.
				 */
				if (period_time < cm->desc->cap_one_time)
					fuel_cap = cm->desc->cap - 1;
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((cm->desc->cap - fuel_cap) >=
				    (flush_time / cm->desc->cap_one_time))
					fuel_cap = cm->desc->cap -
						flush_time / cm->desc->cap_one_time;
			}
		} else if (fuel_cap > cm->desc->cap) {
			if (period_time < cm->desc->cap_one_time)
				fuel_cap = cm->desc->cap + 1;
			if ((fuel_cap - cm->desc->cap) >=
			    (flush_time / cm->desc->cap_one_time))
				fuel_cap = cm->desc->cap +
					flush_time / cm->desc->cap_one_time;
		}

		if (cm->desc->cap != 100 && fuel_cap >= 100)
			fuel_cap = 99;
		/*
		 * Record 99% of the charging time.
		 * if it is greater than 1500s,
		 * it will be mandatory to display 100%,
		 * but the background is still charging.
		 */
		if (cm->desc->cap >= 99 &&
		    cm->desc->trickle_time >= cm->desc->trickle_time_out &&
		    cm->desc->trickle_time_out > 0 &&
		    bat_uA > 0)
			cm->desc->force_set_full = true;

		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		/*
		 * In not charging status,
		 * the cap is not allowed to increase.
		 */
		if (fuel_cap >= cm->desc->cap) {
			fuel_cap = cm->desc->cap;
		} else {
			if (period_time < cm->desc->cap_one_time)
				fuel_cap = cm->desc->cap - 1;
			/*
			 * If wake up from long sleep mode,
			 * will make a percentage compensation based on time.
			 */
			if ((cm->desc->cap - fuel_cap) >=
			    (flush_time / cm->desc->cap_one_time))
				fuel_cap = cm->desc->cap -
					flush_time / cm->desc->cap_one_time;
		}
		break;

	case POWER_SUPPLY_STATUS_FULL:
		cm->desc->update_capacity_time = cur_time.tv_sec;
		if ((batt_ocV < (cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV - 50000))
		    && (bat_uA < 0))
			cm->desc->force_set_full = false;
		if (is_ext_pwr_online(cm)) {
			if (fuel_cap != 100)
				fuel_cap = 100;

			if (fuel_cap > cm->desc->cap)
				fuel_cap = cm->desc->cap + 1;
		}

		break;
	default:
		break;
	}

	if (batt_uV <= cm->desc->shutdown_voltage - CM_UVLO_OFFSET) {
		dev_err(cm->dev, "WARN: batt_uV less than uvlo, will shutdown\n");
		orderly_poweroff(true);
	}

	dev_info(cm->dev, "battery cap = %d, charger manager cap = %d\n",
		 fuel_cap, cm->desc->cap);

	if (fuel_cap != cm->desc->cap) {
		cm->desc->cap = fuel_cap;
		cm->desc->update_capacity_time = cur_time.tv_sec;
		set_batt_cap(cm);
		power_supply_changed(cm->charger_psy);
	}

	queue_delayed_work(system_power_efficient_wq,
			   &cm->cap_update_work,
			   CM_CAP_CYCLE_TRACK_TIME * HZ);
}

static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_desc *desc = cm_get_drv_data(pdev);
	struct charger_manager *cm;
	int ret, i = 0;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	struct power_supply_config psy_cfg = {};
	struct timespec64 cur_time;

	if (IS_ERR(desc)) {
		dev_err(&pdev->dev, "No platform data (desc) found\n");
		return PTR_ERR(desc);
	}

	cm = devm_kzalloc(&pdev->dev, sizeof(*cm), GFP_KERNEL);
	if (!cm)
		return -ENOMEM;

	/* Basic Values. Unspecified are Null or 0 */
	cm->dev = &pdev->dev;
	cm->desc = desc;
	psy_cfg.drv_data = cm;

	/* Initialize alarm timer */
	if (alarmtimer_get_rtcdev()) {
		cm_timer = devm_kzalloc(cm->dev, sizeof(*cm_timer), GFP_KERNEL);
		if (!cm_timer)
			return -ENOMEM;
		alarm_init(cm_timer, ALARM_BOOTTIME, NULL);
	}

	/*
	 * Some of the following do not need to be errors.
	 * Users may intentionally ignore those features.
	 */
	if (desc->fullbatt_uV == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery voltage threshold as it is not supplied\n");
	}

	if (desc->fullbatt_uA == 0)
		dev_info(&pdev->dev, "Ignoring full-battery current threshold as it is not supplied\n");

	if (!desc->fullbatt_vchkdrop_ms || !desc->fullbatt_vchkdrop_uV) {
		dev_info(&pdev->dev, "Disabling full-battery voltage drop checking mechanism as it is not supplied\n");
		desc->fullbatt_vchkdrop_ms = 0;
		desc->fullbatt_vchkdrop_uV = 0;
	}
	if (desc->fullbatt_soc == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery soc(state of charge) threshold as it is not supplied\n");
	}
	if (desc->fullbatt_full_capacity == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery full capacity threshold as it is not supplied\n");
	}

	if (!desc->charger_regulators || desc->num_charger_regulators < 1) {
		dev_err(&pdev->dev, "charger_regulators undefined\n");
		return -EINVAL;
	}

	if (!desc->psy_charger_stat || !desc->psy_charger_stat[0]) {
		dev_err(&pdev->dev, "No power supply defined\n");
		return -EINVAL;
	}

	if (!desc->psy_fuel_gauge) {
		dev_err(&pdev->dev, "No fuel gauge power supply defined\n");
		return -EINVAL;
	}

	/* Check if charger's supplies are present at probe */
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		struct power_supply *psy;

		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -EPROBE_DEFER;
		}
		power_supply_put(psy);
	}

	if (desc->polling_interval_ms == 0 ||
	    msecs_to_jiffies(desc->polling_interval_ms) <= CM_JIFFIES_SMALL) {
		dev_err(&pdev->dev, "polling_interval_ms is too small\n");
		return -EINVAL;
	}

	if (!desc->charging_max_duration_ms ||
			!desc->discharging_max_duration_ms) {
		dev_info(&pdev->dev, "Cannot limit charging duration checking mechanism to prevent overcharge/overheat and control discharging duration\n");
		desc->charging_max_duration_ms = 0;
		desc->discharging_max_duration_ms = 0;
	}

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop) {
		dev_info(&pdev->dev, "Cannot validate charge voltage\n");
		desc->charge_voltage_max = 0;
		desc->charge_voltage_drop = 0;
	}

	platform_set_drvdata(pdev, cm);

	memcpy(&cm->charger_psy_desc, &psy_default, sizeof(psy_default));

	if (!desc->psy_name)
		strncpy(cm->psy_name_buf, psy_default.name, PSY_NAME_MAX);
	else
		strncpy(cm->psy_name_buf, desc->psy_name, PSY_NAME_MAX);
	cm->charger_psy_desc.name = cm->psy_name_buf;

	/* Allocate for psy properties because they may vary */
	cm->charger_psy_desc.properties = devm_kzalloc(&pdev->dev,
				sizeof(enum power_supply_property)
				* (ARRAY_SIZE(default_charger_props) +
				NUM_CHARGER_PSY_OPTIONAL), GFP_KERNEL);
	if (!cm->charger_psy_desc.properties)
		return -ENOMEM;

	memcpy(cm->charger_psy_desc.properties, default_charger_props,
		sizeof(enum power_supply_property) *
		ARRAY_SIZE(default_charger_props));
	cm->charger_psy_desc.num_properties = psy_default.num_properties;

	/* Find which optional psy-properties are available */
	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fuel_gauge);
		return -ENODEV;
	}
	if (!power_supply_get_property(fuel_gauge,
					  POWER_SUPPLY_PROP_CHARGE_NOW, &val)) {
		cm->charger_psy_desc.properties[cm->charger_psy_desc.num_properties] =
				POWER_SUPPLY_PROP_CHARGE_NOW;
		cm->charger_psy_desc.num_properties++;
	}
	if (!power_supply_get_property(fuel_gauge,
					  POWER_SUPPLY_PROP_CURRENT_NOW,
					  &val)) {
		cm->charger_psy_desc.properties[cm->charger_psy_desc.num_properties] =
				POWER_SUPPLY_PROP_CURRENT_NOW;
		cm->charger_psy_desc.num_properties++;
	}

	ret = get_batt_cap(cm, &cm->desc->cap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get initial battery capacity\n");
		return ret;
	}

	cm->desc->thm_adjust_cur = -EINVAL;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cm->desc->update_capacity_time = cur_time.tv_sec;
	cm->desc->last_query_time = cur_time.tv_sec;

	ret = cm_init_thermal_data(cm, fuel_gauge);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize thermal data\n");
		cm->desc->measure_battery_temp = false;
	}
	power_supply_put(fuel_gauge);

	INIT_DELAYED_WORK(&cm->fullbatt_vchk_work, fullbatt_vchk);
	INIT_DELAYED_WORK(&cm->cap_update_work, cm_batt_works);

	cm->charger_psy = power_supply_register(&pdev->dev,
						&cm->charger_psy_desc,
						&psy_cfg);
	if (IS_ERR(cm->charger_psy)) {
		dev_err(&pdev->dev, "Cannot register charger-manager with name \"%s\"\n",
			cm->charger_psy_desc.name);
		return PTR_ERR(cm->charger_psy);
	}
	cm->charger_psy->supplied_to = charger_manager_supplied_to;
	cm->charger_psy->num_supplicants =
		ARRAY_SIZE(charger_manager_supplied_to);

	/* Register extcon device for charger cable */
	ret = charger_manager_register_extcon(cm);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot initialize extcon device\n");
		goto err_reg_extcon;
	}

	/* Register sysfs entry for charger(regulator) */
	ret = charger_manager_register_sysfs(cm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Cannot initialize sysfs entry of regulator\n");
		goto err_reg_sysfs;
	}

	/* Add to the list */
	mutex_lock(&cm_list_mtx);
	list_add(&cm->entry, &cm_list);
	mutex_unlock(&cm_list_mtx);

	/*
	 * Charger-manager is capable of waking up the systme from sleep
	 * when event is happened through cm_notify_event()
	 */
	device_init_wakeup(&pdev->dev, true);
	device_set_wakeup_capable(&pdev->dev, false);

	/*
	 * Charger-manager have to check the charging state right after
	 * tialization of charger-manager and then update current charging
	 * state.
	 */
	cm_monitor();

	schedule_work(&setup_polling);
	queue_delayed_work(system_power_efficient_wq, &cm->cap_update_work, CM_CAP_CYCLE_TRACK_TIME * HZ);

	if (cm_event_type)
		cm_notify_type_handle(cm, cm_event_type, cm_event_msg);

	return 0;

err_reg_sysfs:
	for (i = 0; i < desc->num_charger_regulators; i++) {
		struct charger_regulator *charger;

		charger = &desc->charger_regulators[i];
		sysfs_remove_group(&cm->charger_psy->dev.kobj,
				&charger->attr_g);
	}
err_reg_extcon:
	for (i = 0; i < desc->num_charger_regulators; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);

	return ret;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);
	struct charger_desc *desc = cm->desc;
	int i = 0;

	/* Remove from the list */
	mutex_lock(&cm_list_mtx);
	list_del(&cm->entry);
	mutex_unlock(&cm_list_mtx);

	cancel_work_sync(&setup_polling);
	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->cap_update_work);

	for (i = 0 ; i < desc->num_charger_regulators ; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);

	try_charger_enable(cm, false);

	return 0;
}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);

	set_batt_cap(cm);
}

static const struct platform_device_id charger_manager_id[] = {
	{ "charger-manager", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, charger_manager_id);

static int cm_suspend_noirq(struct device *dev)
{
	if (device_may_wakeup(dev)) {
		device_set_wakeup_capable(dev, false);
		return -EAGAIN;
	}

	return 0;
}

static int cm_suspend_prepare(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (!cm_suspended)
		cm_suspended = true;

	/*
	 * In some situation, the system is not sleep between
	 * the charger polling interval - 15s, it maybe occur
	 * that charger manager will feed watchdog, but the
	 * system has no work to do to suspend, and charger
	 * manager also suspend. In this function, it will
	 * cancel cm_monito_work, it cause that this time can't
	 * feed watchdog until the next polling time, this means
	 * that charger manager feed watchdog per 15s usually,
	 * but this time need 30s, and the charger IC(fan54015)
	 * watchdog timeout to reset.
	 */
	if (is_ext_pwr_online(cm))
		cm_feed_watchdog(cm);
	cm_timer_set = cm_setup_timer();

	if (cm_timer_set) {
		cancel_work_sync(&setup_polling);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work(&cm->fullbatt_vchk_work);
		cancel_delayed_work_sync(&cm->cap_update_work);
	}

	return 0;
}

static void cm_suspend_complete(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (cm_suspended)
		cm_suspended = false;

	if (cm_timer_set) {
		ktime_t remain;

		alarm_cancel(cm_timer);
		cm_timer_set = false;
		remain = alarm_expires_remaining(cm_timer);
		if (remain > 0)
			cm_suspend_duration_ms -= ktime_to_ms(remain);
		schedule_work(&setup_polling);
	}

	_cm_monitor(cm);
	cm_batt_works(&cm->cap_update_work.work);

	/* Re-enqueue delayed work (fullbatt_vchk_work) */
	if (cm->fullbatt_vchk_jiffies_at) {
		unsigned long delay = 0;
		unsigned long now = jiffies + CM_JIFFIES_SMALL;

		if (time_after_eq(now, cm->fullbatt_vchk_jiffies_at)) {
			delay = (unsigned long)((long)now
				- (long)(cm->fullbatt_vchk_jiffies_at));
			delay = jiffies_to_msecs(delay);
		} else {
			delay = 0;
		}

		/*
		 * Account for cm_suspend_duration_ms with assuming that
		 * timer stops in suspend.
		 */
		if (delay > cm_suspend_duration_ms)
			delay -= cm_suspend_duration_ms;
		else
			delay = 0;

		queue_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
				   msecs_to_jiffies(delay));
	}
	device_set_wakeup_capable(cm->dev, false);
}

static const struct dev_pm_ops charger_manager_pm = {
	.prepare	= cm_suspend_prepare,
	.suspend_noirq	= cm_suspend_noirq,
	.complete	= cm_suspend_complete,
};

static struct platform_driver charger_manager_driver = {
	.driver = {
		.name = "charger-manager",
		.pm = &charger_manager_pm,
		.of_match_table = charger_manager_match,
	},
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.id_table = charger_manager_id,
};

static int __init charger_manager_init(void)
{
	cm_wq = create_freezable_workqueue("charger_manager");
	INIT_DELAYED_WORK(&cm_monitor_work, cm_monitor_poller);

	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_cleanup(void)
{
	destroy_workqueue(cm_wq);
	cm_wq = NULL;

	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_cleanup);

/**
 * cm_notify_type_handle - charger driver handle charger event
 * @cm: the Charger Manager representing the battery
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg)
{
	switch (type) {
	case CM_EVENT_BATT_FULL:
		fullbatt_handler(cm);
		break;
	case CM_EVENT_BATT_IN:
	case CM_EVENT_BATT_OUT:
		battout_handler(cm);
		break;
	case CM_EVENT_EXT_PWR_IN_OUT ... CM_EVENT_CHG_START_STOP:
		misc_event_handler(cm, type);
		break;
	case CM_EVENT_UNKNOWN:
	case CM_EVENT_OTHERS:
		uevent_notify(cm, msg ? msg : default_event_names[type]);
		break;
	default:
		dev_err(cm->dev, "%s: type not specified\n", __func__);
		break;
	}

	power_supply_changed(cm->charger_psy);
}

/**
 * cm_notify_event - charger driver notify Charger Manager of charger event
 * @psy: pointer to instance of charger's power_supply
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
void cm_notify_event(struct power_supply *psy, enum cm_event_types type,
		     char *msg)
{
	struct charger_manager *cm;
	bool found_power_supply = false;

	if (psy == NULL)
		return;

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		if (match_string(cm->desc->psy_charger_stat, -1,
				 psy->desc->name) >= 0 ||
				 match_string(&cm->desc->psy_fuel_gauge,
					      -1, psy->desc->name) >= 0) {
			found_power_supply = true;
			break;
		}
	}
	mutex_unlock(&cm_list_mtx);

	if (!found_power_supply) {
		cm_event_msg = msg;
		cm_event_type = type;
		return;
	}

	cm_notify_type_handle(cm, type, msg);
}
EXPORT_SYMBOL_GPL(cm_notify_event);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("Charger Manager");
MODULE_LICENSE("GPL");
