/*
 *
 * General TouchScreen Function Test
 *
 * Copyright (c) 2013  Hisense Ltd.
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "ts_func_test.h"

#define MOD_TAG "ts_gen_func_test"

#define gen_printk(level, title, format, arg...) \
	printk(level "%s: " format, title, ## arg)
#ifdef DEBUG
#define gen_dbg(format, arg...) \
	gen_printk(KERN_DEBUG, MOD_TAG, format, ## arg)
#else
#define gen_dbg(format, arg...) do { (void)(dev); } while (0)
#endif

#define gen_err(format, arg...) do { \
		gen_printk(KERN_ERR, MOD_TAG, format, ##arg); \
	} while (0)
#define gen_info(dev, format, arg...) do {\
		gen_printk(KERN_INFO, MOD_TAG, format, ##arg); \
	} while (0)
#define gen_warn(dev, format, arg...) do { \
		gen_printk(KERN_WARNING, MOD_TAG, format, ##arg); \
	} while (0)
#define gen_notice(dev, format, arg...) do { \
		gen_printk(KERN_NOTICE, MOD_TAG, format, ##arg); \
	} while (0)

static struct ts_func_test_device *ts_func_test_device[5];
static char input_num;
void register_ts_func_test_device(struct ts_func_test_device *device)
{
	ts_func_test_device[input_num - 1] = device;
	__module_get(THIS_MODULE);
}

EXPORT_SYMBOL(register_ts_func_test_device);

void unregister_ts_func_test_device(struct ts_func_test_device *device)
{
	ts_func_test_device[input_num - 1] = NULL;
	module_put(THIS_MODULE);
}

EXPORT_SYMBOL(unregister_ts_func_test_device);

static int get_ts_func_test_device(struct kobject *obj)
{
	const char *path;
	int path_num = 0, i;
	char path_temp[50] = { "/ctp/ctp_xxx" };

	path = kobject_get_path(obj, GFP_KERNEL);
	printk("**********path:%s.\n", path);
	strcpy(path_temp, path);
	for (i = 0; i < sizeof(path_temp); i++) {
		if (path_temp[i] >= '1' && path_temp[i] <= '9') {
			path_num = path_temp[i] - '0';
			break;
		} else {
			if (sizeof(path_temp) == i) {
				path_num = 0;
				break;
			}
		}
	}
	kfree(path);
	return path_num;
}

/*
 * This function is provided for LCD ASYNC DISPLAY.
 * If TP realize this interface---->
 * int (*ts_async_suspend_for_lcd_use)(void),
 * LCD will make TP enter suspend. It can help INCELL
 * IC avoid gesture mode or power problem.
 */
void ts_suspend_for_lcd_async_use(void)
{
	struct ts_func_test_device *test_device;

	test_device = ts_func_test_device[0];

	if (test_device && test_device->ts_async_suspend_for_lcd_use)
		test_device->ts_async_suspend_for_lcd_use();
}
EXPORT_SYMBOL(ts_suspend_for_lcd_async_use);

/*
 * This function is provided for LCD ASYNC DISPLAY.
 * If TP realize this interface---->
 * int (*ts_async_resume_for_lcd_use)(void),
 * LCD will make TP enter resume. It can help INCELL
 * IC avoid gesture mode or power problem.
 */
void ts_resume_for_lcd_async_use(void)
{
	struct ts_func_test_device *test_device;

	test_device = ts_func_test_device[0];

	if (test_device && test_device->ts_async_resume_for_lcd_use)
		test_device->ts_async_resume_for_lcd_use();
}
EXPORT_SYMBOL(ts_resume_for_lcd_async_use);

/*
 * This function is provided for INCELL IC sequence.
 * If TP realize this interface---->
 * int (*ts_reset_for_lcd_use)(void),
 * LCD will use it to make TP reset first, then the
 * sequence can satisfy the IC's need.
 */
void ts_reset_for_lcd_use(void)
{
	struct ts_func_test_device *test_device;

	test_device = ts_func_test_device[0];

	if (test_device && test_device->ts_reset_for_lcd_use)
		test_device->ts_reset_for_lcd_use();
}
EXPORT_SYMBOL(ts_reset_for_lcd_use);

/*
 * This function is provided for INCELL IC sequence.
 * If TP realize this interface---->
 * int (*ts_suspend_need_lcd_reset_high)(void),
 * LCD will use it to make TP reset keep high when suspend,
 * then the sequence can satisfy the IC's need.
 */
bool ts_suspend_need_lcd_reset_high(void)
{
	struct ts_func_test_device *test_device;

	test_device = ts_func_test_device[0];

	if (test_device && test_device->ts_suspend_need_lcd_reset_high)
		return test_device->ts_suspend_need_lcd_reset_high();
	else
		return false;
}
EXPORT_SYMBOL(ts_suspend_need_lcd_reset_high);

/*
 * This function is provided for INCELL IC gesture sequence.
 * If TP realize this interface---->
 * int (*ts_suspend_need_lcd_reset_high)(void),
 * LCD will use it to keep LCD power and reset high when suspend,
 * then the gesture can work.
 */
int ts_supend_need_power_reset_high(void)
{
	struct ts_func_test_device *test_device;

	test_device = ts_func_test_device[0];

	if (test_device && test_device->ts_suspend_need_lcd_power_reset_high)
		return test_device->ts_suspend_need_lcd_power_reset_high();
	else
		return false;
}
EXPORT_SYMBOL(ts_supend_need_power_reset_high);

static ssize_t fw_state_show(struct kobject *obj, struct kobj_attribute *attr,
			     char *buf)
{
	struct ts_func_test_device *test_device;
	int update_need = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->check_fw_update_need) {
		update_need =
		    test_device->check_fw_update_need(test_device->dev);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", update_need);
}

static ssize_t fw_update_show(struct kobject *obj, struct kobj_attribute *attr,
			      char *buf)
{
	struct ts_func_test_device *test_device;
	int update_progress = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_fw_update_progress) {
		update_progress =
		    test_device->get_fw_update_progress(test_device->dev);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", update_progress);
}

static ssize_t fw_update_store(struct kobject *obj, struct kobj_attribute *attr,
			       const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (strncmp(buf, "UPDATE", 6))
		return -EINVAL;

	if (test_device && test_device->proc_fw_update) {
		test_device->proc_fw_update(test_device->dev, false);
	}
	return size;
}

static ssize_t fw_path_show(struct kobject *obj, struct kobj_attribute *attr,
			    char *buf)
{
	struct ts_func_test_device *test_device;
	char fw_path[64] = { 0 };
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_fw_path) {
		test_device->get_fw_path(test_device->dev, fw_path,
					 sizeof(fw_path));
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", fw_path);
}

static ssize_t fw_path_store(struct kobject *obj, struct kobj_attribute *attr,
			     const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_fw_path)
		test_device->set_fw_path(test_device->dev, buf);

	return size;
}

static ssize_t fw_update_with_appointed_file_show(struct kobject *obj,
						  struct kobj_attribute *attr,
						  char *buf)
{
	struct ts_func_test_device *test_device;
	int update_progress = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_fw_update_progress)
		update_progress =
		    test_device->get_fw_update_progress(test_device->dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", update_progress);
}

static ssize_t fw_update_with_appointed_file_store(struct kobject *obj,
						   struct kobj_attribute *attr,
						   const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_fw_path) {
		test_device->set_fw_path(test_device->dev, buf);

		if (test_device && test_device->proc_fw_update) {
			test_device->proc_fw_update(test_device->dev, true);
		}
	} else if (test_device && test_device->proc_fw_update_with_given_file) {
		test_device->proc_fw_update_with_given_file(test_device->dev,
							    buf);
	}
	return size;
}

static ssize_t rawdata_show(struct kobject *obj, struct kobj_attribute *attr,
			    char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_rawdata) {
		ret = test_device->get_rawdata(test_device->dev, buf);
	} else {
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
	}
	return ret;
}

static ssize_t diff_show(struct kobject *obj, struct kobj_attribute *attr,
			 char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_diff) {
		ret = test_device->get_diff(test_device->dev, buf);
	} else {
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
	}
	return ret;
}

static ssize_t selftest_show(struct kobject *obj, struct kobj_attribute *attr,
			     char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_selftest) {
		ret = test_device->get_selftest(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", ret);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t rawdata_info_show(struct kobject *obj,
				 struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_rawdata_info) {
		ret = test_device->get_rawdata_info(test_device->dev, buf);
		return ret;
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t hibernate_test_show(struct kobject *obj,
				   struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->proc_hibernate_test) {
		ret = test_device->proc_hibernate_test(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", ret);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t fw_ic_version_show(struct kobject *obj,
				  struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_ic_fw_version)
		ret = test_device->get_ic_fw_version(test_device->dev, buf);

	return ret;
}

static ssize_t fw_fs_version_show(struct kobject *obj,
				  struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_fs_fw_version)
		ret = test_device->get_fs_fw_version(test_device->dev, buf);

	return ret;
}

static ssize_t fw_fs_settinginfo_show(struct kobject *obj,
				      struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_tp_settings_info)
		ret = test_device->get_tp_settings_info(test_device->dev, buf);

	return ret;
}

static ssize_t fw_set_erase_flash_test(struct kobject *obj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (strncmp(buf, "ERASE", 5))
		return -EINVAL;

	if (test_device && test_device->set_erase_flash_test)
		test_device->set_erase_flash_test(test_device->dev, true);

	return size;
}

static ssize_t module_id_show(struct kobject *obj, struct kobj_attribute *attr,
			      char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_module_id)
		ret = test_device->get_module_id(test_device->dev, buf);

	return ret;
}

static ssize_t chip_detect_show(struct kobject *obj,
				struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_chip_type)
		ret = test_device->get_chip_type(test_device->dev, buf);

	return ret;
}

static ssize_t calibration_show(struct kobject *obj,
				struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_calibration_ret) {
		ret = test_device->get_calibration_ret(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", ret);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t need_test_config_show(struct kobject *obj,
				     struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->need_test_config) {
		enable = test_device->need_test_config(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t test_config_path_store(struct kobject *obj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_test_config_path) {
		test_device->set_test_config_path(test_device->dev, buf);
		return size;
	}
	return -EINVAL;
}

static ssize_t short_show(struct kobject *obj, struct kobj_attribute *attr,
			  char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_short_test) {
		enable = test_device->get_short_test(test_device->dev, buf);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t open_show(struct kobject *obj, struct kobj_attribute *attr,
			 char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_open_test) {
		enable = test_device->get_open_test(test_device->dev, buf);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t factory_info_show(struct kobject *obj,
				 struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_factory_info)
		ret = test_device->get_factory_info(test_device->dev, buf);
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");

	return ret;
}

static ssize_t factory_cfg_info_show(struct kobject *obj,
				     struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_cfg_info)
		ret = test_device->get_cfg_info(test_device->dev, buf);
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");

	return ret;
}

static ssize_t gesture_show(struct kobject *obj, struct kobj_attribute *attr,
			    char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_gesture_switch) {
		enable = test_device->get_gesture_switch(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t gesture_store(struct kobject *obj, struct kobj_attribute *attr,
			     const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_gesture_switch) {
		test_device->set_gesture_switch(test_device->dev, buf);
		return size;
	}
	return -EINVAL;
}

static ssize_t tp_enable_show(struct kobject *obj, struct kobj_attribute *attr,
			      char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_tp_enable_switch) {
		enable = test_device->get_tp_enable_switch(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t tp_enable_store(struct kobject *obj, struct kobj_attribute *attr,
			       const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_tp_enable_switch) {
		if (strncmp(buf, "0", 1))
			test_device->set_tp_enable_switch(test_device->dev,
							  true);
		else
			test_device->set_tp_enable_switch(test_device->dev,
							  false);
		return size;
	}

	return -EINVAL;
}

static ssize_t gesture_pos_show(struct kobject *obj,
				struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_gesture_pos)
		ret = test_device->get_gesture_pos(test_device->dev, buf);

	return ret;
}

static ssize_t gesture_fingers_show(struct kobject *obj,
				    struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int ret = 0;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_gesture_fingers) {
		ret = test_device->get_gesture_fingers(test_device->dev, buf);
		if (-EINVAL == ret)
			return snprintf(buf, PAGE_SIZE, "%s\n", "MODE ERROR");
		else
			return snprintf(buf, PAGE_SIZE, "0x%02X\n", ret);
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t hall_show(struct kobject *obj, struct kobj_attribute *attr,
			 char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_hall_switch) {
		enable = test_device->get_hall_switch(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t hall_store(struct kobject *obj, struct kobj_attribute *attr,
			  const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_hall_switch) {
		test_device->set_hall_switch(test_device->dev, buf);
		return size;
	}
	return -EINVAL;
}

static ssize_t glove_show(struct kobject *obj, struct kobj_attribute *attr,
			  char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_glove_switch) {
		enable = test_device->get_glove_switch(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t glove_store(struct kobject *obj, struct kobj_attribute *attr,
			   const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_glove_switch) {
		if (strncmp(buf, "0", 1))
			test_device->set_glove_switch(test_device->dev, true);
		else
			test_device->set_glove_switch(test_device->dev, false);

		return size;
	}
	return -EINVAL;
}

static ssize_t irq_awake_show(struct kobject *obj, struct kobj_attribute *attr,
			      char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_tp_irq_awake_switch) {
		enable = test_device->get_tp_irq_awake_switch(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t irq_awake_store(struct kobject *obj, struct kobj_attribute *attr,
			       const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_tp_irq_awake_switch) {
		if (strncmp(buf, "0", 1))
			test_device->set_tp_irq_awake_switch(test_device->dev,
							     true);
		else
			test_device->set_tp_irq_awake_switch(test_device->dev,
							     false);

		return size;
	}
	return -EINVAL;
}

static ssize_t tptype_show(struct kobject *obj, struct kobj_attribute *attr,
			   char *buf)
{
	struct ts_func_test_device *test_device;
	int enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_tptype_switch) {
		enable = test_device->get_tptype_switch(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n",
				enable > 1 ? 2 : enable > 0 ? 1 : 0);
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t tptype_store(struct kobject *obj, struct kobj_attribute *attr,
			    const char *buf, size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_tptype_switch) {
		if (strncmp(buf, "0", 1)) {
			if (strncmp(buf, "1", 1))
				test_device->set_tptype_switch(test_device->dev,
							       2);
			else
				test_device->set_tptype_switch(test_device->dev,
							       1);

		} else
			test_device->set_tptype_switch(test_device->dev, 0);
		return size;
	}
	return -EINVAL;
}

#if defined(CONFIG_TP_MATCH_HW)
static ssize_t check_gpio_state_show(struct kobject *obj,
				     struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	bool state = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->check_gpio_state) {
		state = test_device->check_gpio_state(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", state ? 1 : 0);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t tp_match_hw_show(struct kobject *obj,
				struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	bool enable = false;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->tp_match_hw) {
		enable = test_device->tp_match_hw(test_device->dev);
		return snprintf(buf, PAGE_SIZE, "%d\n", enable ? 1 : 0);
	} else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}
#endif

static ssize_t tp_work_mode_show(struct kobject *obj,
				 struct kobj_attribute *attr, char *buf)
{
	struct ts_func_test_device *test_device;
	int path_num;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->get_tp_work_mode)
		return test_device->get_tp_work_mode(test_device->dev, buf);
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "NOT SUPPORTED");
}

static ssize_t tp_work_mode_store(struct kobject *obj,
				  struct kobj_attribute *attr, const char *buf,
				  size_t size)
{
	struct ts_func_test_device *test_device;
	int path_num;
	int ret = 0;

	path_num = get_ts_func_test_device(obj);
	test_device = ts_func_test_device[path_num];

	if (test_device && test_device->set_tp_work_mode)
		ret = test_device->set_tp_work_mode(test_device->dev, buf);

	return size;
}

static struct kobj_attribute fw_state_attr = {
	.attr = {
		 .name = "fwstate",
		 .mode = 0444,
		 },
	.show = fw_state_show,
};

static struct kobj_attribute fw_update_attr = {
	.attr = {
		 .name = "fwupdate",
		 .mode = 0644,
		 },
	.show = fw_update_show,
	.store = fw_update_store,
};

static struct kobj_attribute fw_path_attr = {
	.attr = {
		 .name = "fwpath",
		 .mode = 0644,
		 },
	.show = fw_path_show,
	.store = fw_path_store,
};

static struct kobj_attribute fw_update_with_appointed_file_attr = {
	.attr = {
		 .name = "fwbinupdate",
		 .mode = 0644,
		 },
	.show = fw_update_with_appointed_file_show,
	.store = fw_update_with_appointed_file_store,
};

static struct kobj_attribute rawdata_attr = {
	.attr = {
		 .name = "rawdatashow",
		 .mode = 0444,
		 },
	.show = rawdata_show,
};

static struct kobj_attribute rawdata_info_attr = {
	.attr = {
		 .name = "rawdatainfo",
		 .mode = 0444,
		 },
	.show = rawdata_info_show,
};

static struct kobj_attribute diff_attr = {
	.attr = {
		 .name = "diffshow",
		 .mode = 0444,
		 },
	.show = diff_show,
};

static struct kobj_attribute selftest_attr = {
	.attr = {
		 .name = "selftestshow",
		 .mode = 0444,
		 },
	.show = selftest_show,
};

static struct kobj_attribute hibernate_test_attr = {
	.attr = {
		 .name = "resettest",
		 .mode = 0444,
		 },
	.show = hibernate_test_show,
};

static struct kobj_attribute fw_tp_settings_attr = {
	.attr = {
		 .name = "settinginfo",
		 .mode = 0444,
		 },
	.show = fw_fs_settinginfo_show,
};

static struct kobj_attribute fw_erase_flash_test_attr = {
	.attr = {
		 .name = "eraseflash",
		 .mode = 0644,
		 },
	.store = fw_set_erase_flash_test,
};

static struct kobj_attribute fw_ic_version_attr = {
	.attr = {
		 .name = "fwversion",
		 .mode = 0444,
		 },
	.show = fw_ic_version_show,
};

static struct kobj_attribute fw_fs_version_attr = {
	.attr = {
		 .name = "fwhostversion",
		 .mode = 0444,
		 },
	.show = fw_fs_version_show,
};

static struct kobj_attribute module_id_attr = {
	.attr = {
		 .name = "fwmoduleid",
		 .mode = 0444,
		 },
	.show = module_id_show,
};

static struct kobj_attribute chip_detect_attr = {
	.attr = {
		 .name = "chiptest",
		 .mode = 0444,
		 },
	.show = chip_detect_show,
};

static struct kobj_attribute calibration_attr = {
	.attr = {
		 .name = "caltest",
		 .mode = 0444,
		 },
	.show = calibration_show,
};

static struct kobj_attribute test_config_attr = {
	.attr = {
		 .name = "testconfig",
		 .mode = 0644,
		 },
	.show = need_test_config_show,
	.store = test_config_path_store,
};

static struct kobj_attribute short_attr = {
	.attr = {
		 .name = "shorttest",
		 .mode = 0444,
		 },
	.show = short_show,
};

static struct kobj_attribute open_attr = {
	.attr = {
		 .name = "opentest",
		 .mode = 0444,
		 },
	.show = open_show,
};

static struct kobj_attribute factory_info_attr = {
	.attr = {
		 .name = "factoryinfo",
		 .mode = 0444,
		 },
	.show = factory_info_show,
};

static struct kobj_attribute factory_cfg_info_attr = {
	.attr = {
		 .name = "cfginfo",
		 .mode = 0444,
		 },
	.show = factory_cfg_info_show,
};

static struct kobj_attribute gesture_attr = {
	.attr = {
		 .name = "gesture",
		 .mode = 0644,
		 },
	.show = gesture_show,
	.store = gesture_store,
};

static struct kobj_attribute gesture_pos_attr = {
	.attr = {
		 .name = "gesturepos",
		 .mode = 0644,
		 },
	.show = gesture_pos_show,
};

static struct kobj_attribute gesture_fingers_attr = {
	.attr = {
		 .name = "gesturefingers",
		 .mode = 0644,
		 },
	.show = gesture_fingers_show,
};

static struct kobj_attribute hall_attr = {
	.attr = {
		 .name = "hall",
		 .mode = 0644,
		 },
	.show = hall_show,
	.store = hall_store,
};

static struct kobj_attribute glove_attr = {
	.attr = {
		 .name = "glove",
		 .mode = 0644,
		 },
	.show = glove_show,
	.store = glove_store,
};

static struct kobj_attribute irqawake_attr = {
	.attr = {
		 .name = "irqawake",
		 .mode = 0644,
		 },
	.show = irq_awake_show,
	.store = irq_awake_store,
};

static struct kobj_attribute tptypeswitch_attr = {
	.attr = {
		 .name = "tptypeswitch",
		 .mode = 0644,
		 },
	.show = tptype_show,
	.store = tptype_store,
};

static struct kobj_attribute tp_enable_attr = {
	.attr = {
		 .name = "tpenable",
		 .mode = 0644,
		 },
	.show = tp_enable_show,
	.store = tp_enable_store,
};

#if defined(CONFIG_TP_MATCH_HW)
static struct kobj_attribute check_gpio_state_attr = {
	.attr = {
		 .name = "check_gpio_state",
		 .mode = 0444,
		 },
	.show = check_gpio_state_show,
};

static struct kobj_attribute tp_match_hw_attr = {
	.attr = {
		 .name = "tp_match_hw",
		 .mode = 0444,
		 },
	.show = tp_match_hw_show,
};
#endif

static struct kobj_attribute tp_work_mode_attr = {
	.attr = {
		 .name = "tp_work_mode",
		 .mode = 0644,
		 },
	.show = tp_work_mode_show,
	.store = tp_work_mode_store,
};

static struct attribute *ts_func_ctp_func_attr[] = {
	&gesture_attr.attr,
	&gesture_pos_attr.attr,
	&gesture_fingers_attr.attr,
	&hall_attr.attr,
	&glove_attr.attr,
	&tptypeswitch_attr.attr,
	&tp_enable_attr.attr,
	&irqawake_attr.attr,
	&tp_work_mode_attr.attr,
	NULL,
};

static struct attribute *ts_func_ctp_update_attr[] = {
	&fw_state_attr.attr,
	&fw_update_attr.attr,
	&fw_path_attr.attr,
	&fw_update_with_appointed_file_attr.attr,
	&fw_erase_flash_test_attr.attr,
	NULL
};

static struct attribute *ts_func_ctp_test_attr[] = {
	&rawdata_attr.attr,
	&rawdata_info_attr.attr,
	&diff_attr.attr,
	&selftest_attr.attr,
	&hibernate_test_attr.attr,
	&fw_ic_version_attr.attr,
	&fw_fs_version_attr.attr,
	&module_id_attr.attr,
	&calibration_attr.attr,
	&open_attr.attr,
	&short_attr.attr,
	&test_config_attr.attr,
	&chip_detect_attr.attr,
	&factory_info_attr.attr,
	&fw_tp_settings_attr.attr,
	&factory_cfg_info_attr.attr,
#if defined(CONFIG_TP_MATCH_HW)
	&check_gpio_state_attr.attr,
	&tp_match_hw_attr.attr,
#endif
	NULL,
};

static struct attribute_group ts_func_ctp_update_grp = {
	.attrs = ts_func_ctp_update_attr,
};

static struct attribute_group ts_func_ctp_test_grp = {
	.attrs = ts_func_ctp_test_attr,
};

static struct attribute_group ts_func_ctp_func_grp = {
	.attrs = ts_func_ctp_func_attr,
};

static struct kobject *ts_func_test_obj;
static struct kobject *ts_func_ctp_test_obj;
static struct kobject *ts_func_ctp_update_obj;
static struct kobject *ts_func_ctp_func_obj;

void ts_gen_func_test_init(void)
{
	int ret = 0;
	char buf[6], tmp[3] = { 0 };

	strlcpy(buf, "ctp", sizeof(buf));
	if (input_num != 0) {
		snprintf(tmp, ARRAY_SIZE(tmp), "%d", input_num);
		strlcat(buf, tmp, sizeof(buf));
	}
	input_num++;
	ts_func_test_obj = kobject_create_and_add(buf, NULL);
	if (!ts_func_test_obj) {
		gen_err("unable to create kobject\n");
		return;
	}

	ts_func_ctp_test_obj =
	    kobject_create_and_add("ctp_test", ts_func_test_obj);
	if (!ts_func_ctp_test_obj) {
		gen_err("unable to create kobject-ts_func_test_obj\n");
		goto destroy_test_obj;
	}

	ts_func_ctp_update_obj =
	    kobject_create_and_add("ctp_update", ts_func_test_obj);
	if (!ts_func_ctp_update_obj) {
		gen_err("unable to create kobject-ts_func_test_obj\n");
		goto destroy_ctp_test_obj;
	}

	ts_func_ctp_func_obj =
	    kobject_create_and_add("ctp_func", ts_func_test_obj);
	if (!ts_func_ctp_func_obj) {
		gen_err("unable to create kobject-ts_func_ctp_func_obj\n");
		goto destroy_ctp_func_obj;
	}

	ret = sysfs_create_group(ts_func_ctp_test_obj, &ts_func_ctp_test_grp);
	if (ret) {
		gen_err("failed to create attributes- ts_func_ctp_test_grp\n");
		goto destroy_ctp_update_obj;
	}

	ret =
	    sysfs_create_group(ts_func_ctp_update_obj, &ts_func_ctp_update_grp);
	if (ret) {
		gen_err
		    ("failed to create attributes- ts_func_ctp_update_grp\n");
		goto remove_ctp_test_grp;
	}

	ret = sysfs_create_group(ts_func_ctp_func_obj, &ts_func_ctp_func_grp);
	if (ret) {
		gen_err("failed to create attributes- ts_func_ctp_func_grp\n");
		goto remove_ctp_func_grp;
	}

	return;

remove_ctp_func_grp:
	sysfs_remove_group(ts_func_test_obj, &ts_func_ctp_func_grp);
remove_ctp_test_grp:
	sysfs_remove_group(ts_func_test_obj, &ts_func_ctp_test_grp);
destroy_ctp_update_obj:
	kobject_put(ts_func_ctp_update_obj);
destroy_ctp_func_obj:
	kobject_put(ts_func_ctp_func_obj);
destroy_ctp_test_obj:
	kobject_put(ts_func_ctp_test_obj);
destroy_test_obj:
	kobject_put(ts_func_test_obj);
	return;
}
EXPORT_SYMBOL(ts_gen_func_test_init);
