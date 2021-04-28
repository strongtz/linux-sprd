#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/time64.h>
#include <linux/module.h>

#include "ats_core.h"

/* spinlock used to enable/disable irq */
static DEFINE_SPINLOCK(g_irqlock);

static inline int ts_get_mode(
	struct ts_data *pdata, unsigned int mode)
{
	return test_bit(mode, &pdata->status);
}

static inline void ts_set_mode(
	struct ts_data *pdata, unsigned int mode, bool on)
{
	if (on)
		set_bit(mode, &pdata->status);
	else
		clear_bit(mode, &pdata->status);
}

/*
 * clear all points
 * if use type A, there's no need to clear manually
 */
static void ts_clear_points(struct ts_data *pdata)
{
	int i = 0;
	bool need_sync = false;
	struct ts_point *p = NULL;
	struct device *dev = &pdata->pdev->dev;

	for (i = 0; i < TS_MAX_POINTS; i++) {
		if (pdata->stashed_points[i].pressed) {
			p = &pdata->stashed_points[i];
			input_mt_slot(pdata->input, p->slot);
			input_mt_report_slot_state(pdata->input, MT_TOOL_FINGER, false);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "TS Point[%d] UP: x=%d, y=%d", p->slot, p->x, p->y);
			need_sync = true;
			p->pressed = 0;
		}
	}

	if (need_sync) {
		input_report_key(pdata->input, BTN_TOUCH, 0);
		input_sync(pdata->input);
	}
}

/*
 * report type A, just report what controller tells us
 */
static int ts_report1(struct ts_data *pdata,
		       struct ts_point *points, int counts)
{
	int i;
	struct ts_point *p;
	struct device *dev = &pdata->pdev->dev;

	if (counts == 0) {
		input_report_key(pdata->input, BTN_TOUCH, 0);
		input_mt_sync(pdata->input);
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			dev_dbg(dev, "UP: all points leave");
	} else {
		for (i = 0; i < counts; i++) {
			p = points + i;
			input_report_key(pdata->input, BTN_TOUCH, 1);
			input_report_abs(pdata->input, ABS_MT_POSITION_X, p->x);
			input_report_abs(pdata->input, ABS_MT_POSITION_Y, p->y);
			input_mt_sync(pdata->input);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "DOWN: x%d=%d, y%d=%d", i, p->x, i, p->y);
		}
	}

	input_sync(pdata->input);

	return 0;
}

/* TODO: implemented this
 * just report what hardware reports
 */
static int ts_report2(struct ts_data *pdata,
		       struct ts_point *points, int counts)
{
	return 0;
}

/* if (x, y) is a virtual key then return its keycode, otherwise return 0 */
static inline unsigned int ts_get_keycode(
	struct ts_data *pdata, unsigned short x, unsigned short y)
{
	int i;
	struct ts_virtualkey_info *vkey;
	struct ts_virtualkey_hitbox *hbox;

	if (!ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS)) {
		vkey = pdata->vkey_list;
		hbox = pdata->vkey_hitbox;
		for (i = 0; i < pdata->vkey_count; i++, vkey++, hbox++) {
			if (x >= hbox->left && x <= hbox->right
				&& y >= hbox->top && y <= hbox->bottom)
				return vkey->keycode;
		}
	}

	return 0;
}

static inline void ts_report_abs(struct ts_data *pdata,
				 struct ts_point *point, bool down)
{
	input_mt_slot(pdata->input, point->slot);
	if (down) {
		input_mt_report_slot_state(pdata->input, MT_TOOL_FINGER, true);
		input_report_abs(pdata->input, ABS_MT_POSITION_X, point->x);
		input_report_abs(pdata->input, ABS_MT_POSITION_Y, point->y);
	} else {
		input_mt_report_slot_state(pdata->input, MT_TOOL_FINGER, false);
	}
}

static inline void ts_report_translate_key(struct ts_data *pdata,
		       struct ts_point *cur, struct ts_point *last,
		       bool *sync_abs, bool *sync_key, bool *btn_down)
{
	unsigned int kc, kc_last;
	struct device *dev = &pdata->pdev->dev;

	kc = ts_get_keycode(pdata, cur->x, cur->y);
	kc_last = ts_get_keycode(pdata, last->x, last->y);

	if (cur->pressed && last->pressed) {
		if (cur->x == last->x && cur->y == last->y) {
			if (!kc)
				*btn_down = true;
			return;
		}

		if (kc > 0 && kc_last > 0) {
			/* from one virtual key to another */
			input_report_key(pdata->input, kc_last, 0);
			input_report_key(pdata->input, kc, 1);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA)) {
				dev_dbg(dev, "Key %s UP", ts_get_keyname(kc_last));
				dev_dbg(dev, "Key %s DOWN", ts_get_keyname(kc));
			}
			*sync_key = true;
		} else if (kc > 0) {
			/* from screen to virtual key */
			ts_report_abs(pdata, last, false);
			input_report_key(pdata->input, kc, 1);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA)) {
				dev_dbg(dev, "Point[%d] UP: x=%d, y=%d",
				       last->slot, last->x, last->y);
				dev_dbg(dev, "Key %s DOWN", ts_get_keyname(kc));
			}
			*sync_key = *sync_abs = true;
		} else if (kc_last > 0) {
			/* from virtual key to screen */
			input_report_key(pdata->input, kc_last, 0);
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA)) {
				dev_dbg(dev, "Key %s UP", ts_get_keyname(kc));
				dev_dbg(dev, "Point[%d] DOWN: x=%d, y=%d",
				       last->slot, last->x, last->y);
			}
			*btn_down = true;
			*sync_key = *sync_abs = true;
		} else {
			/* from screen to screen */
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "Point[%d] MOVE TO: x=%d, y=%d",
				       cur->slot, cur->x, cur->y);
			*btn_down = true;
			*sync_abs = true;
		}
	} else if (cur->pressed) {
		if (kc > 0) {
			/* virtual key pressed */
			input_report_key(pdata->input, kc, 1);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "Key %s DOWN", ts_get_keyname(kc));
			*sync_key = true;
		} else {
			/* new point down */
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "Point[%d] DOWN: x=%d, y=%d",
				       cur->slot, cur->x, cur->y);
			*btn_down = true;
			*sync_abs = true;
		}
	} else if (last->pressed) {
		if (kc_last > 0) {
			/* virtual key released */
			input_report_key(pdata->input, kc_last, 0);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "Key %s UP", ts_get_keyname(kc_last));
			*sync_key = true;
		} else {
			/* point up */
			ts_report_abs(pdata, last, false);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "Point[%d] UP: x=%d, y=%d",
				       last->slot, last->x, last->y);
			*sync_abs = true;
		}
	}
}

static inline void ts_report_no_translate(struct ts_data *pdata,
		      struct ts_point *cur, struct ts_point *last,
		      bool *sync_abs, bool *btn_down)
{
	struct device *dev = &pdata->pdev->dev;

	if (cur->pressed && last->pressed) {
		*btn_down = true;
		if (cur->x != last->x || cur->y != last->y) {
			ts_report_abs(pdata, cur, true);
			if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
				dev_dbg(dev, "Point[%d] MOVE TO: x=%d, y=%d",
				       cur->slot, cur->x, cur->y);
			*sync_abs = true;
		}
	} else if (cur->pressed) {
		*btn_down = true;
		ts_report_abs(pdata, cur, true);
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			dev_dbg(dev, "Point[%d] DOWN: x=%d, y=%d",
			       cur->slot, cur->x, cur->y);
		*sync_abs = true;
	} else if (last->pressed) {
		ts_report_abs(pdata, last, false);
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			dev_dbg(dev, "Point[%d] UP: x=%d, y=%d",
			       last->slot, last->x, last->y);
		*sync_abs = true;
	}
}

static inline void ts_fix_UP_if_needed(struct ts_data *pdata,
			struct ts_point *p, enum ts_stashed_status status,
			bool *sync_key, bool *sync_abs)
{
	unsigned int kc;
	struct device *dev = &pdata->pdev->dev;

	if (status == TSSTASH_NEW && p->pressed) {
		if (!ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS)) {
			kc = ts_get_keycode(pdata, p->x, p->y);
			if (kc) {
				input_report_key(pdata->input, kc, 0);
				*sync_key = true;
				if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
					dev_dbg(dev, "Key %s UP",
						ts_get_keyname(kc));
				return;
			}
		}
		ts_report_abs(pdata, p, false);
		*sync_abs = true;
		if (ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA))
			dev_dbg(dev, "Point[%d] UP: x=%d, y=%d",
				p->slot, p->x, p->y);
	}
}

static int ts_report3(struct ts_data *pdata,
		       struct ts_point *points, int counts)
{
	struct ts_point *cur, *last;
	int i;
	bool sync_abs = false, btn_down = false, sync_key = false;
	struct device *dev = &pdata->pdev->dev;

	for (i = 0; i < counts; i++) {
		cur = points + i;
		if (cur->slot >= TS_MAX_POINTS) {
			dev_err(dev, "invalid current slot number: %u", cur->slot);
			continue;
		}

		last = &pdata->stashed_points[cur->slot];
		pdata->stashed_status[cur->slot] = TSSTASH_CONSUMED;
		if (last->slot >= TS_MAX_POINTS) {
			dev_err(dev, "invalid last slot number: %u", last->slot);
			continue;
		}

		if (!ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS))
			ts_report_translate_key(pdata, cur, last, &sync_abs,
					       &sync_key, &btn_down);
		else
			ts_report_no_translate(pdata, cur, last,
					      &sync_abs, &btn_down);
	}

	/* check for disappeared UP events */
	for (i = 0; i < TS_MAX_POINTS; i++)
		ts_fix_UP_if_needed(pdata,
			&pdata->stashed_points[i], pdata->stashed_status[i],
			&sync_key, &sync_abs);

	/* record current point's status */
	memset(pdata->stashed_status, 0, sizeof(pdata->stashed_status));
	for (i = 0; i < counts; i++)
		pdata->stashed_status[points[i].slot] = TSSTASH_NEW;

	if (sync_key || sync_abs) {
		if (sync_abs)
			input_report_key(pdata->input, BTN_TOUCH, btn_down);
		input_sync(pdata->input);

		/* record current point */
		for (i = 0; i < counts; i++)
			memcpy(&pdata->stashed_points[points[i].slot],
				&points[i], sizeof(struct ts_point));
	}

	return 0;
}

static int ts_report(struct ts_data *pdata, struct ts_point *points, int counts)
{
	unsigned int type = pdata->controller->config;

	type &= TSCONF_REPORT_TYPE_MASK;
	if (type == TSCONF_REPORT_TYPE_1)
		return ts_report1(pdata, points, counts);
	else if (type == TSCONF_REPORT_TYPE_2)
		return ts_report2(pdata, points, counts);
	else if (type == TSCONF_REPORT_TYPE_3)
		return ts_report3(pdata, points, counts);

	return 0;
}

static int ts_request_gpio(struct ts_data *pdata)
{
	int retval;
	struct ts_board *board = pdata->board;
	struct device *dev = &pdata->pdev->dev;

	if (!board->int_gpio) {
		dev_warn(dev, "no int on our board!");
	} else {
		retval = devm_gpio_request(&pdata->pdev->dev,
			board->int_gpio, ATS_INT_LABEL);
		if (retval < 0) {
			dev_err(dev, "failed to request int gpio: %d, retval: %d!",
				board->int_gpio, retval);
			return retval;
		}
		dev_dbg(dev, "request int gpio \"%d\"", board->int_gpio);
	}

	if (!board->rst_gpio) {
		dev_dbg(dev, "no rst on our board!");
	} else {
		retval = devm_gpio_request(&pdata->pdev->dev,
			board->rst_gpio, ATS_RST_LABEL);
		if (retval < 0) {
			dev_err(dev, "failed to request rst gpio: %d, retval: %d!",
				board->rst_gpio, retval);
			return retval;
		}
		dev_dbg(dev, "request rst gpio \"%d\"", board->rst_gpio);
	}

	return 0;
}

static int ts_export_gpio(struct ts_data *pdata)
{
	int retval;
	struct ts_board *board = pdata->board;
	struct device *dev = &pdata->pdev->dev;

	if (board->int_gpio) {
		retval = gpio_export(board->int_gpio, true);
		if (retval < 0) {
			dev_warn(dev, "failed to export int gpio: %d, retval: %d!",
				board->int_gpio, retval);
			return retval;
		}
		dev_dbg(dev, "exported int gpio: %d", board->rst_gpio);
	}

	if (board->rst_gpio) {
		retval = gpio_export(board->rst_gpio, true);
		if (retval < 0) {
			dev_warn(dev, "failed to export rst gpio: %d, retval: %d!",
				board->rst_gpio, retval);
			return retval;
		}
		dev_dbg(dev, "exported rst gpio: %d", board->rst_gpio);
	}

	return 0;
}

struct ts_firmware_upgrade_param {
	struct ts_data *pdata;
	bool force_upgrade;
};

/*
 * firmware upgrading worker, asynchronous callback from firmware subsystem
 */
static void ts_firmware_upgrade_worker(const struct firmware *fw, void *context)
{
	struct ts_firmware_upgrade_param *param = context;
	struct ts_data *pdata = param->pdata;
	enum ts_result ret;
	struct device *dev = &pdata->pdev->dev;

	if (unlikely(fw == NULL)) {
		dev_warn(dev, "upgrading cancel: cannot find such a firmware file");
		return;
	}

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		dev_err(dev, "controller not ready!");
		return;
	}

	if (!pdata->controller->upgrade_firmware) {
		dev_err(dev, "controller \"%s\" does not support firmware upgrade",
			pdata->controller->name);
		/* if controller doesn't support, don't hold firmware */
		release_firmware(fw);
		return;
	}

	ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, false);
	dev_dbg(dev, ">>> Upgrade Firmware Begin <<<");
	__pm_stay_awake(&pdata->upgrade_lock);
	ret = pdata->controller->upgrade_firmware(pdata->controller,
		fw->data, fw->size, param->force_upgrade);
	__pm_relax(&pdata->upgrade_lock);
	dev_dbg(dev, ">>> Upgrade Firmware End <<<");
	if (ret == TSRESULT_UPGRADE_FINISHED)
		dev_info(dev, ">>> Upgrade Result: Success <<<");
	else if (ret == TSRESULT_INVALID_BINARY)
		dev_err(dev, ">>> Upgrade Result: bad firmware file <<<");
	else if (ret == TSRESULT_OLDER_VERSION)
		dev_warn(dev, ">>> Upgrade Result: older version, no need to upgrade <<<");
	else
		dev_err(dev, ">>> Upgrade Result: other error <<<");
	ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, true);

	release_firmware(fw);
}

/*
 * firmware upgrade entry
 */
static int ts_request_firmware_upgrade(struct ts_data *pdata,
	const char *fw_name, bool force_upgrade)
{
	char *buf, *name;
	struct ts_firmware_upgrade_param *param;
	struct device *dev = &pdata->pdev->dev;

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		dev_err(dev, "controller not exist!");
		return -EBUSY;
	}

	param = devm_kmalloc(&pdata->pdev->dev,
		sizeof(struct ts_firmware_upgrade_param), GFP_KERNEL);
	if (IS_ERR(param)) {
		dev_err(dev, "fail to allocate firmware upgrade param");
		return -ENOMEM;
	}
	param->force_upgrade = force_upgrade;
	param->pdata = pdata;

	if (!fw_name) {
		buf = devm_kmalloc(&pdata->pdev->dev, 32, GFP_KERNEL);
		if (IS_ERR(buf)) {
			dev_err(dev, "fail to allocate buffer for firmware name");
			return -ENOMEM;
		}
		sprintf(buf, "%s-%s.bin", pdata->controller->vendor, pdata->controller->name);
		name = buf;
	} else {
		name = (char *)fw_name;
	}

	dev_dbg(dev, "requesting firmware \"%s\"", name);
	if (request_firmware_nowait(THIS_MODULE, true, name, &pdata->pdev->dev,
		GFP_KERNEL, param, ts_firmware_upgrade_worker) < 0) {
		dev_err(dev, "request firmware failed");
		return -ENOENT;
	}

	return 0;
}

static int ts_register_input_dev(struct ts_data *pdata)
{
	int retval;
	unsigned int i;
	struct input_dev *input;
	struct device *dev = &pdata->pdev->dev;

	input = devm_input_allocate_device(&pdata->pdev->dev);
	if (IS_ERR(input)) {
		dev_err(dev, "Failed to allocate input device.");
		return -ENOMEM;
	}

	input_set_capability(input, EV_KEY, BTN_TOUCH);
	if (!ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) && pdata->vkey_list) {
		for (i = 0; i < pdata->vkey_count; i++)
			input_set_capability(input, EV_KEY, pdata->vkey_list[i].keycode);
	}
	input_set_capability(input, EV_ABS, ABS_MT_POSITION_X);
	input_set_capability(input, EV_ABS, ABS_MT_POSITION_Y);
	/* TODO report pressure */
	/* input_set_capability(input, EV_ABS, ABS_MT_PRESSURE); */

	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ((pdata->controller->config & TSCONF_REPORT_TYPE_MASK)
		!= TSCONF_REPORT_TYPE_1))
		input_mt_init_slots(input, TS_MAX_POINTS, INPUT_MT_DIRECT);

	input_set_abs_params(input, ABS_MT_POSITION_X, 0, pdata->width, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, pdata->height, 0, 0);
	/* input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0); */
	input->name = ATS_INPUT_DEV;
	input->dev.parent = &pdata->pdev->dev;
	input->id.bustype = BUS_HOST;

	retval = input_register_device(input);
	if (retval < 0) {
		dev_err(dev, "Failed to register input device.");
		input_free_device(input);
		return retval;
	}

	dev_dbg(dev, "Succeed to register input device.");
	pdata->input = input;

	return 0;
}

/*
 * reset controller
 */
static int ts_reset_controller(struct ts_data *pdata, bool hw_reset)
{
	int level;
	struct device *dev = &pdata->pdev->dev;

	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		if (hw_reset) {
			if ((pdata->controller->config & TSCONF_RESET_LEVEL_MASK)
				== TSCONF_RESET_LEVEL_LOW)
				level = 0;
			else
				level = 1;

			dev_info(dev, "hardware reset now...");
			ts_gpio_output(TSGPIO_RST, level);
			msleep(pdata->controller->reset_keep_ms);
			ts_gpio_output(TSGPIO_RST, !level);
			msleep(pdata->controller->reset_delay_ms);
		} else {
			/* TODO: add software reset */
		}
	}
	return 0;
}

/*
 * configure parameters from controller definition and board specification
 * values from board specification always overwrite controller definition
 * if they are found in dts file
 *
 * did not check value of pdata->controller
 */
static void ts_configure(struct ts_data *pdata)
{
	struct ts_controller *c = pdata->controller;
	struct ts_board *b = pdata->board;
	struct ts_data *p = pdata;
	const struct ts_virtualkey_info *key_source;
	int key_count = 0;
	struct device *dev = &pdata->pdev->dev;

	/* configure touch panel size */
	if (b->panel_width) {
		/* have touch panel size configured in .dts, just use these values */
		p->width = b->panel_width;
		p->height = b->panel_height;
	} else {
		if (b->surface_width && (b->surface_width < b->lcd_width)) {
			/*
			 * got surface in .dts file, and its size is smaller than lcd size
			 * we're simulating low resolution now!!
			 */
			p->width = c->panel_width * b->surface_width / b->lcd_width;
			p->height = c->panel_height * b->surface_height / b->lcd_height;
			dev_info(dev, "low resolution simulation, surface=%dx%d, lcd=%dx%d",
				b->surface_width, b->surface_height, b->lcd_width, b->lcd_height);
		} else {
			/*
			 * nothing special, we just report real size of controller and
			 * let framework to do scaling
			 */
			p->width = c->panel_width;
			p->height = c->panel_height;
		}
	}

	/* configure virtualkeys */
	if (b->virtualkey_count > 0) {
		/* .dts values come first */
		key_count = b->virtualkey_count;
		key_source = b->virtualkeys;
	} else if (c->virtualkey_count > 0) {
		/* check if controller values exist */
		key_count = c->virtualkey_count;
		key_source = c->virtualkeys;
	}

	if (key_count) {
		p->vkey_count = key_count;
		p->vkey_list = devm_kzalloc(&p->pdev->dev,
			sizeof(p->vkey_list[0]) * p->vkey_count, GFP_KERNEL);
		p->vkey_hitbox = devm_kzalloc(&p->pdev->dev,
			sizeof(p->vkey_hitbox[0]) * p->vkey_count, GFP_KERNEL);
		if (IS_ERR_OR_NULL(p->vkey_list) || IS_ERR_OR_NULL(p->vkey_hitbox)) {
			dev_err(dev, "No memory for virtualkeys!");
			p->vkey_count = 0;
			p->vkey_list = NULL;
			p->vkey_hitbox = NULL;
			return;
		}
		memcpy(p->vkey_list, key_source, sizeof(key_source[0]) * key_count);
		for (key_count = 0; key_count < p->vkey_count; key_count++) {
			p->vkey_hitbox[key_count].top = key_source[key_count].y
				- key_source[key_count].height / 2;
			p->vkey_hitbox[key_count].bottom = key_source[key_count].y
				+ key_source[key_count].height / 2;
			p->vkey_hitbox[key_count].left = key_source[key_count].x
				- key_source[key_count].width / 2;
			p->vkey_hitbox[key_count].right = key_source[key_count].x
				+ key_source[key_count].width / 2;
		}
	}

	/* configure reg_width to ensure communication*/
	if ((c->config & TSCONF_ADDR_WIDTH_MASK) == TSCONF_ADDR_WIDTH_WORD)
		b->bus->reg_width = 2;
	else
		b->bus->reg_width = 1;

	/* configure virtual key reporting */
	ts_set_mode(pdata, TSMODE_VKEY_REPORT_ABS, pdata->board->vkey_report_abs);

	/* configure firmware upgrading */
	ts_set_mode(pdata, TSMODE_AUTO_UPGRADE_FW, pdata->board->auto_upgrade_fw);
}

/*
 * open controller
 */
static int ts_open_controller(struct ts_data *pdata)
{
	struct ts_controller *c;
	int result;
	struct device *dev = &pdata->pdev->dev;

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST))
		return -ENODEV;
	c = pdata->controller;

	/* open board specific regulator */
	if (pdata->board->avdd_supply) {
		if (IS_ERR_OR_NULL(pdata->power)) {
			pdata->power = devm_regulator_get(&pdata->pdev->dev,
				pdata->board->avdd_supply);
			if (IS_ERR_OR_NULL(pdata->power)) {
				dev_err(dev, "Cannot get regulator \"%s\"",
					pdata->board->avdd_supply);
				return -ENODEV;
			}
			dev_dbg(dev, "get regulator \"%s\"", pdata->board->avdd_supply);
		}
		if (regulator_enable(pdata->power)) {
			dev_err(dev, "Cannot enable regulator \"%s\"", pdata->board->avdd_supply);
			devm_regulator_put(pdata->power);
			pdata->power = NULL;
		}
		dev_dbg(dev, "enable regulator \"%s\"", pdata->board->avdd_supply);
	}

	/* set gpio directions */
	ts_gpio_input(TSGPIO_INT);
	if ((c->config & TSCONF_RESET_LEVEL_MASK) == TSCONF_RESET_LEVEL_LOW)
		ts_gpio_output(TSGPIO_RST, 1);
	else
		ts_gpio_output(TSGPIO_RST, 0);

	if ((c->config & TSCONF_POWER_ON_RESET_MASK) == TSCONF_POWER_ON_RESET)
		ts_reset_controller(pdata, true);

	if (c->handle_event) {
		result = c->handle_event(c, TSEVENT_POWER_ON, pdata->board->priv);
		return result == TSRESULT_EVENT_HANDLED ? 0 : -ENODEV;
	}

	return 0;
}

/*
 * close controller
 */
static void ts_close_controller(struct ts_data *pdata)
{
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		/* TODO power off controller */

		if (pdata->board->avdd_supply && !IS_ERR_OR_NULL(pdata->power))
			regulator_disable(pdata->power);
	}
}

/*
 * enable/disable irq
 */
static void ts_enable_irq(struct ts_data *pdata, bool enable)
{
	unsigned long irqflags = 0;
	struct device *dev = &pdata->pdev->dev;

	spin_lock_irqsave(&g_irqlock, irqflags);
	if (enable && !ts_get_mode(pdata, TSMODE_IRQ_STATUS)) {
		enable_irq(pdata->irq);
		ts_set_mode(pdata, TSMODE_IRQ_STATUS, true);
		dev_dbg(dev, "enable irq");
	} else if (!enable && ts_get_mode(pdata, TSMODE_IRQ_STATUS)) {
		ts_set_mode(pdata, TSMODE_IRQ_STATUS, false);
		disable_irq_nosync(pdata->irq);
		dev_dbg(dev, "disable irq");
	} else {
		dev_warn(dev, "Irq status and enable(%s) mis-match!", enable ? "true" : "false");
	}
	spin_unlock_irqrestore(&g_irqlock, irqflags);
}

/*
 * enable/disable polling
 */
static void ts_enable_polling(struct ts_data *pdata, bool enable)
{
	struct device *dev = &pdata->pdev->dev;

	ts_set_mode(pdata, TSMODE_POLLING_STATUS, enable ? true : false);
	dev_dbg(dev, "%s polling", enable ? "enable" : "disable");
}

/*
 * poll worker
 */
static void ts_poll_handler(unsigned long _data)
{
	struct ts_data *pdata = (struct ts_data *)_data;
	struct ts_point points[TS_MAX_POINTS];
	int counts;
	struct device *dev = &pdata->pdev->dev;

	memset(points, 0, sizeof(struct ts_point) * TS_MAX_POINTS);

	if (ts_get_mode(pdata, TSMODE_POLLING_STATUS)) {
		if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
			dev_err(dev, "controller not exist");
		} else if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)) {
			dev_warn(dev, "controller is busy...");
			/* wait for upgrading completion */
			msleep(100);
		} else {
			counts = pdata->controller->fetch_points(pdata->controller, points);
			if (counts >= 0)
				ts_report(pdata, points, counts);
		}

		mod_timer(&pdata->poll_timer, pdata->poll_interval);
	}
}

/*
 * enable/disable polling mode
 */
static void ts_poll_control(struct ts_data *pdata, bool enable)
{
	struct device *dev = &pdata->pdev->dev;

	if (enable) {
		if (ts_get_mode(pdata, TSMODE_POLLING_MODE)) {
			dev_warn(dev, "polling mode already enabled");
			return;
		}

		ts_enable_polling(pdata, true);
		mod_timer(&pdata->poll_timer, jiffies + pdata->poll_interval);
		ts_set_mode(pdata, TSMODE_POLLING_MODE, true);
		dev_dbg(dev, "succeed to enable polling mode");
	} else {
		if (!ts_get_mode(pdata, TSMODE_POLLING_MODE)) {
			dev_warn(dev, "polling mode already disabled");
			return;
		}

		ts_enable_polling(pdata, false);
		ts_set_mode(pdata, TSMODE_POLLING_MODE, false);
		dev_dbg(dev, "succeed to disable polling mode");
	}
}

/*
 * isr top
 */
static irqreturn_t ts_interrupt_handler(int irq, void *data)
{
	struct ts_data *pdata = data;
	static struct timespec64 last;
	struct timespec64 cur;
	struct device *dev = &pdata->pdev->dev;

	/* TODO: change to async for performance reason */
	if (ts_get_mode(pdata, TSMODE_DEBUG_IRQTIME)) {
		getnstimeofday64(&cur);
		dev_dbg(dev, "time elapsed in two interrupts: %ld ns",
			timespec64_sub(cur, last).tv_nsec);
		memcpy(&last, &cur, sizeof(struct timespec64));
	}

	return IRQ_WAKE_THREAD;
}

/*
 * isr bottom
 */
static irqreturn_t ts_interrupt_worker(int irq, void *data)
{
	struct ts_data *pdata = data;
	struct ts_point points[TS_MAX_POINTS];
	int counts;

	memset(points, 0, sizeof(struct ts_point) * TS_MAX_POINTS);

	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		counts = pdata->controller->fetch_points(pdata->controller, points);
		if (counts >= 0)
			ts_report(pdata, points, counts);
	}

	return IRQ_HANDLED;
}

/*
 * (un)register isr
 */
static int ts_isr_control(struct ts_data *pdata, bool _register)
{
	int retval = 0;
	unsigned long flags = IRQF_TRIGGER_FALLING;
	struct device *dev = &pdata->pdev->dev;

	if (_register) {
		if (ts_get_mode(pdata, TSMODE_IRQ_MODE)) {
			dev_warn(dev, "interrupt handler already registered");
			return 0;
		}

		switch (pdata->controller->config & TSCONF_IRQ_TRIG_MASK) {
		case TSCONF_IRQ_TRIG_EDGE_FALLING:
			flags = IRQF_TRIGGER_FALLING;
			break;
		case TSCONF_IRQ_TRIG_EDGE_RISING:
			flags = IRQF_TRIGGER_RISING;
			break;
		case TSCONF_IRQ_TRIG_LEVEL_HIGH:
			flags = IRQF_TRIGGER_HIGH;
			break;
		case TSCONF_IRQ_TRIG_LEVEL_LOW:
			flags = IRQF_TRIGGER_LOW;
			break;
		}

		flags |= IRQF_ONESHOT;

		retval = devm_request_threaded_irq(&pdata->pdev->dev, pdata->irq,
			ts_interrupt_handler, ts_interrupt_worker, flags, ATS_IRQ_HANDLER, pdata);
		if (retval < 0) {
			dev_err(dev, "register interrupt handler fail, retval=%d", retval);
			return retval;
		}

		ts_set_mode(pdata, TSMODE_IRQ_MODE, true);
		ts_set_mode(pdata, TSMODE_IRQ_STATUS, true);
		dev_dbg(dev, "succeed to register interrupt handler, irq=%d", pdata->irq);
	} else {
		if (!ts_get_mode(pdata, TSMODE_IRQ_MODE)) {
			dev_warn(dev, "interrupt handler already unregistered");
			return 0;
		}

		ts_enable_irq(pdata, false);
		devm_free_irq(&pdata->pdev->dev, pdata->irq, pdata);
		ts_set_mode(pdata, TSMODE_IRQ_MODE, false);
		dev_dbg(dev, "succeed to unregister interrupt handler");
	}

	return retval;
}

/*
 * suspend and turn off controller
 */
static int ts_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ts_data *pdata = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	dev_info(dev, "now we're going to sleep...");

	if (ts_get_mode(pdata, TSMODE_SUSPEND_STATUS))
		return 0;

	ts_set_mode(pdata, TSMODE_SUSPEND_STATUS, true);

	/* turn off polling if it's on */
	if (ts_get_mode(pdata, TSMODE_POLLING_MODE)
		&& ts_get_mode(pdata, TSMODE_POLLING_STATUS))
		ts_enable_polling(pdata, false);

	/* disable irq if work in irq mode */
	if (ts_get_mode(pdata, TSMODE_IRQ_MODE)
		&& ts_get_mode(pdata, TSMODE_IRQ_STATUS))
		ts_enable_irq(pdata, false);

	/* notify controller if it has requested */
	/* TODO: pending suspend request if we're busy upgrading firmware */
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)
		&& pdata->controller->handle_event)
		pdata->controller->handle_event(pdata->controller,
		TSEVENT_SUSPEND, NULL);

	/* clear report data */
	ts_clear_points(pdata);

	pdata->suspend = true;

	return 0;
}

/*
 * resume and turn on controller
 * TODO change to async way
 */
static int ts_resume(struct platform_device *pdev)
{
	struct ts_data *pdata = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	dev_info(dev, "now we're going to wake up...");

	if (!ts_get_mode(pdata, TSMODE_SUSPEND_STATUS))
		return 0;

	ts_set_mode(pdata, TSMODE_SUSPEND_STATUS, false);

	/* notify controller if it has requested */
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)
		&& pdata->controller->handle_event)
		pdata->controller->handle_event(pdata->controller,
		TSEVENT_RESUME, NULL);

	/* turn on polling if needed */
	if (ts_get_mode(pdata, TSMODE_POLLING_MODE)
		&& !ts_get_mode(pdata, TSMODE_POLLING_STATUS))
		ts_enable_polling(pdata, true);

	/* enable irq if needed */
	if (ts_get_mode(pdata, TSMODE_IRQ_MODE)
		&& !ts_get_mode(pdata, TSMODE_IRQ_STATUS))
		ts_enable_irq(pdata, true);

	pdata->suspend = false;

	return 0;
}

static void ts_resume_worker(struct work_struct *work)
{
	struct ts_data *pdata = container_of(work, struct ts_data, resume_work);
	ts_resume(pdata->pdev);
}

static void ts_notifier_worker(struct work_struct *work)
{
	struct ts_data *pdata = container_of(work,
						struct ts_data, notifier_work);
	/* notify controller if it has requested */
	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		&& ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)
		&& pdata->controller->handle_event) {
		if (ts_get_mode(pdata, TSMODE_NOISE_STATUS)) {
			ts_set_mode(pdata, TSMODE_NOISE_STATUS, false);
			pr_info("trying to open hardware anti-noise algorithm...");
			pdata->controller->handle_event(pdata->controller,
			TSEVENT_NOISE_HIGH, NULL);
		} else {
			pr_info("closing hardware anti-noise algorithm...");
			pdata->controller->handle_event(pdata->controller,
			TSEVENT_NOISE_NORMAL, NULL);
		}
	}
}

/*
 * handle external events
 */
static void ts_ext_event_handler(struct ts_data *pdata, enum ts_event event, void *data)
{
	struct device *dev = &pdata->pdev->dev;

	switch (event) {
	case TSEVENT_SUSPEND:
		ts_suspend(pdata->pdev, PMSG_SUSPEND);
		break;
	case TSEVENT_RESUME:
		if (ts_get_mode(pdata, TSMODE_WORKQUEUE_STATUS))
			queue_work(pdata->workqueue, &pdata->resume_work);
		else
			ts_resume(pdata->pdev);
		break;
	case TSEVENT_NOISE_HIGH:
		ts_set_mode(pdata, TSMODE_NOISE_STATUS, true);
		break;
	case TSEVENT_NOISE_NORMAL:
		queue_work(pdata->notifier_workqueue, &pdata->notifier_work);
		break;
	default:
		dev_info(dev, "ignore unknown event: %d", event);
		break;
	}
}

static ssize_t ts_hardware_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));

	ts_reset_controller(pdata, true);

	return count;
}

static struct device_attribute dev_attr_hardware_reset = {
	.attr = {
		.name = "hardware_reset",
		.mode = S_IWUSR,
	},
	.show = NULL,
	.store = ts_hardware_reset_store,
};

static ssize_t ts_firmware_upgrade_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	char name[128] = { 0 };

	count--;

	if (count > 1) {
		memcpy(name, buf, count);
		/* upgrading with designated firmware file */
		ts_request_firmware_upgrade(pdata, name, true);
	} else if (count == 1) {
		ts_request_firmware_upgrade(pdata, NULL, buf[0] == 'f');
	}

	return count + 1;
}

static struct device_attribute dev_attr_firmware_upgrade = {
	.attr = {
		.name = "firmware_upgrade",
		.mode = S_IWUSR,
	},
	.show = NULL,
	.store = ts_firmware_upgrade_store,
};

/*
 * read a register value from an address designated in store() func
 */
static ssize_t ts_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int size = 0, ret = 0;
	unsigned char data[1];

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)
		|| !ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS)) {
		size += sprintf(buf, "No controller exist or controller busy!\n");
	} else if (pdata->stashed_reg > 0xFFFF) {
		size += sprintf(buf, "Invalid register address: %d\n", pdata->stashed_reg);
	} else if (pdata->stashed_reg > 0xFF
		&& ((pdata->controller->config & TSCONF_ADDR_WIDTH_MASK) == TSCONF_ADDR_WIDTH_BYTE)) {
		size += sprintf(buf, "Register address out of range: 0x%04X\n", pdata->stashed_reg);
	} else {
		ret = ts_reg_read(pdata->stashed_reg, data, 1);
		if (ret != 1) {
			size += sprintf(buf, "Read error!\n");
		} else {
			if ((pdata->controller->config & TSCONF_ADDR_WIDTH_MASK) == TSCONF_ADDR_WIDTH_BYTE)
				size += sprintf(buf, "Address=0x%02X, Val=0x%02X\n",
					pdata->stashed_reg, data[0]);
			else
				size += sprintf(buf, "Address=0x%04X, Val=0x%02X\n",
					pdata->stashed_reg, data[0]);
		}
	}

	return size;
}

/*
 * store user input value for read next time when show() called
 * checks are done in show() func
 */
static ssize_t ts_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int value = 0;

	if (kstrtoint(buf, 16, &value) < 0) {
		pr_warn("fail to convert \"%s\" to integer", buf);
	} else {
		pr_info("receive register address: %d", value);
		pdata->stashed_reg = value;
	}

	return count;
}

static struct device_attribute dev_attr_register = {
	.attr = {
		.name = "register",
		.mode = S_IWUSR | S_IRUGO,
	},
	.show = ts_register_show,
	.store = ts_register_store,
};

static ssize_t ts_input_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", ATS_INPUT_DEV);
}

static struct device_attribute dev_attr_input_name = {
	.attr = {
		.name = "input_name",
		.mode = S_IRUGO,
	},
	.show = ts_input_name_show,
	.store = NULL,
};

static ssize_t ts_ui_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int size = 0, i;

	size += sprintf(buf + size, "\n======Current Setting======\n");
	size += sprintf(buf + size, "Report area: %u x %u\n",
		pdata->width, pdata->height);
	if (ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS)) {
		size += sprintf(buf + size, "Virtual key reported as KEY event");
	} else {
		for (i = 0; i < pdata->vkey_count; i++) {
			size += sprintf(buf + size,
				"Key: %s(%u) --- (%u, %u), width = %u, height = %u\n",
				ts_get_keyname(pdata->vkey_list[i].keycode),
				pdata->vkey_list[i].keycode,
				pdata->vkey_list[i].x, pdata->vkey_list[i].y,
				pdata->vkey_list[i].width, pdata->vkey_list[i].height);
		}
	}
	size += sprintf(buf + size, "======Current Setting======\n");

	size += sprintf(buf + size, "\n======From DTS======\n");
	size += sprintf(buf + size, "LCD size: %u x %u\n",
		pdata->board->lcd_width, pdata->board->lcd_height);
	size += sprintf(buf + size, "Surface area: %u x %u\n",
		pdata->board->surface_width, pdata->board->surface_height);
	size += sprintf(buf + size, "Report area: %u x %u\n",
		pdata->board->panel_width, pdata->board->panel_height);
	for (i = 0; i < pdata->board->virtualkey_count; i++) {
		size += sprintf(buf + size,
			"Key: %s(%u) --- (%u, %u), width = %u, height = %u\n",
			ts_get_keyname(pdata->board->virtualkeys[i].keycode),
			pdata->board->virtualkeys[i].keycode,
			pdata->board->virtualkeys[i].x,
			pdata->board->virtualkeys[i].y,
			pdata->board->virtualkeys[i].width,
			pdata->board->virtualkeys[i].height);
	}
	size += sprintf(buf + size, "======From DTS======\n");

	return size;
}

static struct device_attribute dev_attr_ui_info = {
	.attr = {
		.name = "ui_info",
		.mode = S_IRUGO,
	},
	.show = ts_ui_info_show,
	.store = NULL,
};

/* show detailed controller info, including those declared by itself */
static ssize_t ts_controller_detail_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	struct ts_controller *c = NULL;
	int size = 0, i;
	char value[1] = { 0 };

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST))
		return sprintf(buf, "Controller doesn't exist!\n");

	c = pdata->controller;
	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS))
		return sprintf(buf, "Controller is busy!\n");

	size += sprintf(buf + size, "\nBacis Properties:\n");
	size += sprintf(buf + size, ">  name: %s\n", c->name);
	size += sprintf(buf + size, ">  vendor: %s\n", c->vendor);

	size += sprintf(buf + size, "\nUI Properties:\n");
	size += sprintf(buf + size, ">  resolution: %u x %u\n", c->panel_width, c->panel_height);
	for (i = 0; i < c->virtualkey_count; i++)
		size += sprintf(buf + size, ">  virtualkey[%d]: (%u, %u) --- %s (0x%X)\n",
			i + 1, c->virtualkeys[i].x, c->virtualkeys[i].y,
			ts_get_keyname(c->virtualkeys[i].keycode), c->virtualkeys[i].keycode);

	size += sprintf(buf + size, "\nBehaviors:\n");
	if ((c->config & TSCONF_REPORT_TYPE_MASK) == TSCONF_REPORT_TYPE_1)
		size += sprintf(buf + size, ">  report_type: 1(Type-A)\n");
	else if ((c->config & TSCONF_REPORT_TYPE_MASK) == TSCONF_REPORT_TYPE_2)
		size += sprintf(buf + size, ">  report_type: 2(Type-B)\n");
	else
		size += sprintf(buf + size, ">  report_type: 3\n");

	if ((c->config & TSCONF_REPORT_MODE_MASK) == TSCONF_REPORT_MODE_IRQ)
		size += sprintf(buf + size, ">  irq_support: true\n");
	else
		size += sprintf(buf + size, ">  irq_support: false\n");

	if ((c->config & TSCONF_POWER_ON_RESET_MASK) == TSCONF_POWER_ON_RESET)
		size += sprintf(buf + size, ">  power_on_reset: true\n");
	else
		size += sprintf(buf + size, ">  power_on_reset: false\n");

	size += sprintf(buf + size, "\nRegister Values:\n");
	for (i = 0; i < c->register_count; i++) {
		if (1 == ts_reg_read(c->registers[i].reg, value, 1))
			size += sprintf(buf + size, ">  %s: 0x%02X\n",
				c->registers[i].name, value[0]);
		else
			size += sprintf(buf + size, ">  %s: read error!\n",
				c->registers[i].name);
	}

	return size;
}

static struct device_attribute dev_attr_controller_info = {
	.attr = {
		.name = "chip_info",
		.mode = S_IRUGO,
	},
	.show = ts_controller_detail_info_show,
	.store = NULL,
};

static ssize_t ts_controller_basic_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	struct ts_controller *c = NULL;
	int i;
	char value_buf[2];

	if (!ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST))
		return sprintf(buf, "Controller doesn't exist!\n");

	c = pdata->controller;

#define __CHECK_ATTR_NAME(s) \
	!strncmp(attr->attr.name, s, min(strlen(s), strlen(attr->attr.name)))
#define __CHECK_DESC(s) \
	!strncmp(c->registers[i].name, s, min(strlen(s), strlen(c->registers[i].name)))

	if (__CHECK_ATTR_NAME("chip_id")) {
		if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS))
			return sprintf(buf, "Controller is busy!\n");

		for (i = 0; i < c->register_count; i++) {
			if (__CHECK_DESC(TSREG_CHIP_ID)) {
				if (1 == ts_reg_read(c->registers[i].reg, value_buf, 1))
					return sprintf(buf, "chip id: 0x%02X\n", value_buf[0]);
				else
					return sprintf(buf, "Read error!\n");
			}
		}

		return sprintf(buf, "chip id: (null)\n");
	} else if (__CHECK_ATTR_NAME("firmware_version")) {
		if (!ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS))
			return sprintf(buf, "Controller is busy!\n");

		for (i = 0; i < c->register_count; i++) {
			if (__CHECK_DESC(TSREG_FW_VER)) {
				if (1 == ts_reg_read(c->registers[i].reg, value_buf, 1))
					return sprintf(buf, "firmware version: 0x%02X\n", value_buf[0]);
				else
					return sprintf(buf, "Read error!\n");
			}
		}

		return sprintf(buf, "firmware version: (null)\n");

	} else if (__CHECK_ATTR_NAME("chip_name")) {
		return sprintf(buf, "chip name: %s\n", c->name);
	} else if (__CHECK_ATTR_NAME("vendor_name")) {
		return sprintf(buf, "vendor name: %s\n", c->vendor);
	}
	return 0;
}

static struct device_attribute dev_attr_chip_id = {
	.attr = {
		.name = "chip_id",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static struct device_attribute dev_attr_firmware_version = {
	.attr = {
		.name = "firmware_version",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static struct device_attribute dev_attr_chip_name = {
	.attr = {
		.name = "chip_name",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static struct device_attribute dev_attr_vendor_name = {
	.attr = {
		.name = "vendor_name",
		.mode = S_IRUGO,
	},
	.show = ts_controller_basic_info_show,
	.store = NULL,
};

static ssize_t ts_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int size = 0;

	size += sprintf(buf, "Status code: 0x%lX\n\n", pdata->status);

	size += sprintf(buf + size, "controller            : %s\n",
		ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST) ? "exist" : "not found");

	size += sprintf(buf + size, "polling               : %s\n",
		ts_get_mode(pdata, TSMODE_POLLING_MODE) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "polling status        : %s\n",
		ts_get_mode(pdata, TSMODE_POLLING_STATUS) ? "working" : "stopped");

	size += sprintf(buf + size, "interrupt             : %s\n",
		ts_get_mode(pdata, TSMODE_IRQ_MODE) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "interrupt status      : %s\n",
		ts_get_mode(pdata, TSMODE_IRQ_STATUS) ? "working" : "stopped");

	size += sprintf(buf + size, "suspend status        : %s\n",
		ts_get_mode(pdata, TSMODE_SUSPEND_STATUS) ? "suspend" : "no suspend");

	size += sprintf(buf + size, "controller status     : %s\n",
		ts_get_mode(pdata, TSMODE_CONTROLLER_STATUS) ? "available" : "busy");

	size += sprintf(buf + size, "virtual key report    : %s\n",
		ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) ? "abs" : "key");

	size += sprintf(buf + size, "firmware auto upgrade : %s\n",
		ts_get_mode(pdata, TSMODE_AUTO_UPGRADE_FW) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "workqueue status      : %s\n",
		ts_get_mode(pdata, TSMODE_WORKQUEUE_STATUS) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "noise status      : %s\n",
		ts_get_mode(pdata, TSMODE_NOISE_STATUS) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "print UP & DOWN       : %s\n",
		ts_get_mode(pdata, TSMODE_DEBUG_UPDOWN) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "print raw data        : %s\n",
		ts_get_mode(pdata, TSMODE_DEBUG_RAW_DATA) ? "enabled" : "not enabled");

	size += sprintf(buf + size, "print irq time        : %s\n",
		ts_get_mode(pdata, TSMODE_DEBUG_IRQTIME) ? "enabled" : "not enabled");

	return size;
}

static ssize_t ts_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));
	int cmd_length = 0, cmd = 0;
	char *c;

	count--; /* ignore '\n' in the end of line */

	c = strchr(buf, ' ');
	cmd_length = c ? c - buf : count;

	if (!strncmp(buf, "irq_enable", min(cmd_length, 10))) {
		if (count  == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			pr_info("irq_enable, cmd = %d", cmd);
			ts_enable_irq(pdata, !!cmd);
		}
	} else if (!strncmp(buf, "raw_data", min(cmd_length, 8))) {
		if (count == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			pr_info("raw_data, cmd = %d", cmd);
			ts_set_mode(pdata, TSMODE_DEBUG_RAW_DATA, !!cmd);
		}
	} else if (!strncmp(buf, "up_down", min(cmd_length, 7))) {
		if (count == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			pr_info("up_down, cmd = %d", cmd);
			ts_set_mode(pdata, TSMODE_DEBUG_UPDOWN, !!cmd);
		}
	} else if (!strncmp(buf, "irq_time", min(cmd_length, 8))) {
		if (count == cmd_length + 2) {
			cmd = buf[count - 1] - '0';
			pr_info("irq_time, cmd = %d", cmd);
			ts_set_mode(pdata, TSMODE_DEBUG_IRQTIME, !!cmd);
		}
	} else {
		pr_info("unrecognized cmd");
	}

	return count + 1;
}

static struct device_attribute dev_attr_mode = {
	.attr = {
		.name = "mode",
		.mode = S_IWUSR | S_IRUGO,
	},
	.show = ts_mode_show,
	.store = ts_mode_store,
};

static ssize_t ts_debug_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ts_dbg_level);
}

static ssize_t ts_debug_level_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (count == 2 && buf[0] >= '0' && buf[0] <= '1')
		ts_dbg_level = buf[0] - '0';

	return count;
}

static struct device_attribute dev_attr_debug_level = {
	.attr = {
		.name = "debug_level",
		.mode = S_IWUSR | S_IRUGO,
	},
	.show = ts_debug_level_show,
	.store = ts_debug_level_store,
};

static ssize_t ts_suspend_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%s\n",
		pdata->suspend ? "true" : "false");
}

static ssize_t ts_suspend_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ts_data *pdata = platform_get_drvdata(to_platform_device(dev));

	if ((buf[0] == '1') && !pdata->suspend)
		ts_suspend(to_platform_device(dev), PMSG_SUSPEND);
	else if ((buf[0] == '0') && pdata->suspend)
		ts_resume(to_platform_device(dev));

	return count;
}

static DEVICE_ATTR_RW(ts_suspend);

static struct attribute *ts_debug_attrs[] = {
	&dev_attr_debug_level.attr,
	&dev_attr_mode.attr,
	&dev_attr_register.attr,
	&dev_attr_firmware_upgrade.attr,
	&dev_attr_hardware_reset.attr,
	&dev_attr_input_name.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_firmware_version.attr,
	&dev_attr_chip_name.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_controller_info.attr,
	&dev_attr_ui_info.attr,
	&dev_attr_ts_suspend.attr,
	NULL,
};

static struct attribute_group ts_debug_attr_group = {
	.attrs = ts_debug_attrs,
};

static ssize_t ts_virtualkey_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct ts_data *pdata = container_of(attr, struct ts_data, vkey_attr);
	ssize_t size = 0;
	unsigned int i;

	if (pdata->board->lcd_width > 0) {
		for (i = 0; i < pdata->vkey_count; i++) {
			size += sprintf(buf + size, "%s:%u:%u:%u:%u:%u:",
				__stringify(ANDROID_KEYMAP_VERSION),
				pdata->vkey_list[i].keycode,
				pdata->vkey_list[i].x * pdata->board->lcd_width / pdata->width,
				pdata->vkey_list[i].y * pdata->board->lcd_height / pdata->height,
				pdata->vkey_list[i].width * pdata->board->lcd_width / pdata->width,
				pdata->vkey_list[i].height * pdata->board->lcd_height / pdata->height);
		}
	} else {
		for (i = 0; i < pdata->vkey_count; i++) {
			size += sprintf(buf + size, "%s:%u:%u:%u:%u:%u:",
				__stringify(ANDROID_KEYMAP_VERSION),
				pdata->vkey_list[i].keycode,
				pdata->vkey_list[i].x,
				pdata->vkey_list[i].y,
				pdata->vkey_list[i].width,
				pdata->vkey_list[i].height);
		}
	}

	if (size > 0)
		buf[size - 1] = '\n';

	return size;
}

static struct attribute *ts_virtualkey_attrs[] = {
	NULL,
	NULL,
};

static struct attribute_group ts_virtualkey_attr_group = {
	.attrs = ts_virtualkey_attrs,
};

static int ts_filesys_create(struct ts_data *pdata)
{
	int retval;
	struct kobj_attribute *attr;
	struct device *dev = &pdata->pdev->dev;

	/* create sysfs virtualkey files */
	if (ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) && pdata->vkey_list) {
		attr = &pdata->vkey_attr;
		attr->attr.name = "virtualkeys.adaptive_ts";
		attr->attr.mode = S_IRUGO;
		attr->show = ts_virtualkey_show;
		/* init attr->key to static to prevent kernel warning */
		sysfs_attr_init(&attr->attr);
		ts_virtualkey_attrs[0] = &attr->attr;

		pdata->vkey_obj = kobject_create_and_add("board_properties", NULL);
		if (IS_ERR_OR_NULL(pdata->vkey_obj)) {
			dev_err(dev, "Fail to create kobject!");
			return -ENOMEM;
		}
		retval = sysfs_create_group(pdata->vkey_obj, &ts_virtualkey_attr_group);
		if (retval < 0) {
			dev_err(dev, "Fail to create virtualkey files!");
			kobject_put(pdata->vkey_obj);
			return -ENOMEM;
		}
		dev_dbg(dev, "virtualkey sysfiles created");
	}

	/* create sysfs debug files	*/
	retval = sysfs_create_group(&pdata->pdev->dev.kobj,
		&ts_debug_attr_group);
	if (retval < 0) {
		dev_err(dev, "Fail to create debug files!");
		return -ENOMEM;
	}

	/* convenient access to sysfs node */
	retval = sysfs_create_link(NULL, &pdata->pdev->dev.kobj, "touchscreen");
	if (retval < 0) {
		dev_err(dev, "Failed to create link!");
		return -ENOMEM;
	}

	return 0;
}

static void ts_filesys_remove(struct ts_data *pdata)
{
	if (ts_get_mode(pdata, TSMODE_VKEY_REPORT_ABS) && pdata->vkey_list) {
		sysfs_remove_group(pdata->vkey_obj, &ts_virtualkey_attr_group);
		kobject_put(pdata->vkey_obj);
	}

	sysfs_remove_link(NULL, "touchscreen");
	sysfs_remove_group(&pdata->pdev->dev.kobj, &ts_debug_attr_group);
}

static int ts_probe(struct platform_device *pdev)
{
	int retval;
	struct ts_data *pdata;
	struct device *dev = &pdev->dev;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct ts_data), GFP_KERNEL);
	if (IS_ERR(pdata)) {
		dev_err(dev, "Failed to allocate platform data!");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, pdata);
	pdata->pdev = pdev;
	pdata->board = pdev->dev.platform_data;

	/* GPIO request first */
	retval = ts_request_gpio(pdata);
	if (retval) {
		dev_err(dev, "Failed to request gpios!");
		return retval;
	}

	/* export GPIO for debug use */
	retval = ts_export_gpio(pdata);
	if (retval) {
		dev_err(dev, "failed to export gpio");
		return retval;
	}

	/* then we find which controller to use */
	pdata->controller = ts_match_controller(pdata->board->controller);
	if (pdata->controller) {
		ts_set_mode(pdata, TSMODE_CONTROLLER_EXIST, true);
		dev_info(dev, "selecting controller \"%s-%s\"", pdata->controller->vendor,
			pdata->controller->name);

		ts_configure(pdata);
		retval = ts_open_controller(pdata);
		if (retval < 0) {
			dev_warn(dev, "fail to open controller, maybe firmware is corrupted");
			if (ts_get_mode(pdata, TSMODE_AUTO_UPGRADE_FW))
				ts_request_firmware_upgrade(pdata, NULL, true);
			ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, true);
		} else if (ts_get_mode(pdata, TSMODE_AUTO_UPGRADE_FW)) {
			ts_request_firmware_upgrade(pdata, NULL, false);
		} else {
			ts_set_mode(pdata, TSMODE_CONTROLLER_STATUS, true);
		}
		if (pdata->board->suspend_on_init)
			ts_suspend(pdata->pdev, PMSG_SUSPEND);
	} else {
		dev_warn(dev, "no matched controller found!");
	}

	/* next we create debug and virtualkey files */
	retval = ts_filesys_create(pdata);
	if (retval) {
		dev_err(dev, "Failed to create sys files.");
		return retval;
	}

	/* also we need to register input device */
	retval = ts_register_input_dev(pdata);
	if (retval) {
		dev_err(dev, "Failed to register input device.");
		return retval;
	}

	/* finally we're gonna report data */
	setup_timer(&pdata->poll_timer, ts_poll_handler, (unsigned long)pdata);
	pdata->poll_interval = TS_POLL_INTERVAL;

	if (ts_get_mode(pdata, TSMODE_CONTROLLER_EXIST)) {
		/* prefer to use irq if supported */
		if ((pdata->controller->config & TSCONF_REPORT_MODE_MASK)
			== TSCONF_REPORT_MODE_IRQ) {
			pdata->irq = gpio_to_irq(pdata->board->int_gpio);
			if (likely(pdata->irq > 0) && !ts_isr_control(pdata, true))
				dev_dbg(dev, "works in interrupt mode, irq=%d", pdata->irq);
		}

		/* if not supported, fallback to polling mode */
		if (!ts_get_mode(pdata, TSMODE_IRQ_MODE)) {
			ts_poll_control(pdata, true);
			dev_dbg(dev, "works in polling mode");
		}
	}

	/* use workqueue to resume device async */
	INIT_WORK(&pdata->resume_work, ts_resume_worker);
	pdata->workqueue = create_singlethread_workqueue(ATS_WORKQUEUE);
	if (IS_ERR(pdata->workqueue)) {
		dev_warn(dev, "failed to create workqueue!");
		ts_set_mode(pdata, TSMODE_WORKQUEUE_STATUS, false);
	} else {
		ts_set_mode(pdata, TSMODE_WORKQUEUE_STATUS, true);
	}

	/* use notifier_workqueue to usb notifier device async */
	INIT_WORK(&pdata->notifier_work, ts_notifier_worker);
	pdata->notifier_workqueue = create_singlethread_workqueue(
		ATS_NOTIFIER_WORKQUEUE);
	if (IS_ERR(pdata->notifier_workqueue)) {
		retval = -ESRCH;
		dev_err(dev, "failed to create notifier_workqueue!");
		return retval;
	}

	/* register external events */
	retval = ts_register_ext_event_handler(pdata, ts_ext_event_handler);
	if (retval < 0)
		dev_err(dev, "error in register external event!");

	dev_info(dev, "ts platform device probe OK");
	wakeup_source_init(&pdata->upgrade_lock, "adaptive_ts upgrade");
	return 0;
}

static int ts_remove(struct platform_device *pdev)
{
	struct ts_data *pdata = platform_get_drvdata(pdev);

	ts_unregister_ext_event_handler();

	if (ts_get_mode(pdata, TSMODE_WORKQUEUE_STATUS)) {
		cancel_work_sync(&pdata->resume_work);
		destroy_workqueue(pdata->workqueue);
	}

	cancel_work_sync(&pdata->notifier_work);
	destroy_workqueue(pdata->notifier_workqueue);

	ts_filesys_remove(pdata);

	ts_close_controller(pdata);

	pdata->status = 0;

	wakeup_source_trash(&pdata->upgrade_lock);

	return 0;
}

static struct platform_driver ats_driver = {
	.driver = {
		.name = ATS_PLATFORM_DEV,
		.owner = THIS_MODULE,
	},
	.probe = ts_probe,
	.remove = ts_remove,
#if TS_USE_LEGACY_SUSPEND
	.suspend = ts_suspend,
	.resume = ts_resume,
#endif
};

static int __init ts_init(void)
{
	int retval;

	retval = ts_board_init();
	if (retval) {
		pr_err("board init failed!");
		return retval;
	}

	return platform_driver_register(&ats_driver);
}

static void __exit ts_exit(void)
{
	platform_driver_unregister(&ats_driver);
	ts_board_exit();
}

late_initcall(ts_init);
module_exit(ts_exit);

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen core module");
MODULE_LICENSE("GPL");
