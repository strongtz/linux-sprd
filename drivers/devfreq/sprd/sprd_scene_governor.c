/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#include <linux/errno.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "../governor.h"
#include <linux/ctype.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/sprd_dfs_drv.h>
#include <trace/events/power.h>

#include "sprd_dfs_trace.h"

#define FTRACE_DDR_NAME    "unisoc-ddr"

static int backdoor_status;
static int trace_poll_time;
static void trace_poll_callback(struct work_struct *work);
static DECLARE_DELAYED_WORK(trace_poll_work, trace_poll_callback);


static void poll_all_status(void)
{
	unsigned int cur_freq;
	unsigned int ap_freq;
	unsigned int cp_freq;
	unsigned int force_freq;
	unsigned int on_off;
	unsigned int auto_on_off;

	get_cur_freq(&cur_freq);
	get_ap_freq(&ap_freq);
	get_cp_freq(&cp_freq);
	get_force_freq(&force_freq);
	get_dfs_status(&on_off);
	get_dfs_auto_status(&auto_on_off);

	trace_sprd_dfs_poll(cur_freq, ap_freq, cp_freq,
			force_freq, on_off, auto_on_off);
	trace_clock_set_rate(FTRACE_DDR_NAME, cur_freq, raw_smp_processor_id());
}

static void trace_poll_callback(struct work_struct *work)
{
	poll_all_status();
	if (trace_poll_time != 0)
		queue_delayed_work(system_power_efficient_wq,
			&trace_poll_work, msecs_to_jiffies(trace_poll_time));
}

/*****************userspace start********************/

static ssize_t scaling_force_ddr_freq_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	int err;

	err = get_force_freq(&data);
	if (err < 0)
		data = 0;
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t scaling_force_ddr_freq_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int force_freq;
	int err;

	err = sscanf(buf, "%u\n", &force_freq);
	if (err < 1) {
		pr_err("%s: force freq para err: %d", __func__, err);
		return count;
	}
	err = force_freq_request(force_freq);
	if (err)
		pr_err("%s: force freq fail: %d", __func__, err);
	trace_sprd_dfs_sysfs(__func__, force_freq);
	return count;
}

static ssize_t scaling_overflow_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int freq_num;
	unsigned int data;
	int i, err;

	err = get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = get_overflow(&data, i);
		if (err < 0)
			data = 0;
		count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");
	return count;
}

static ssize_t scaling_overflow_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int sel;
	unsigned int overflow;
	int err;

	err = sscanf(buf, "%u %u\n", &sel, &overflow);
	if (err < 2) {
		pr_err("%s: overflow para err: %d", __func__, err);
		return count;
	}
	err = set_overflow(overflow, sel);
	if (err)
		pr_err("%s: set overflow fail: %d", __func__, err);
	return count;
}

static ssize_t scaling_underflow_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int freq_num;
	unsigned int data;
	int i, err;

	err = get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = get_underflow(&data, i);
		if (err < 0)
			data = 0;
		count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");
	return count;
}

static ssize_t scaling_underflow_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int sel;
	unsigned int underflow;
	int err;

	err = sscanf(buf, "%u %u\n", &sel, &underflow);
	if (err < 2) {
		pr_err("%s: overflow para err: %d", __func__, err);
		return count;
	}
	err = set_underflow(underflow, sel);
	if (err)
		pr_err("%s: set underflow fail: %d", __func__, err);
	return count;
}

static ssize_t dfs_on_off_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	int err;

	err = get_dfs_status(&data);
	if (err < 0)
		data = 0;
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t dfs_on_off_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int enable;
	int err;

	err = sscanf(buf, "%u\n", &enable);
	if (err < 1) {
		pr_err("%s: enable para err: %d", __func__, err);
		return count;
	}
	if (enable == 1)
		err = dfs_enable();
	else if (enable == 0)
		err = dfs_disable();
	else
		err = -EINVAL;
	if (err)
		pr_err("%s: enable fail: %d", __func__, err);
	trace_sprd_dfs_sysfs(__func__, enable);
	return count;
}


static ssize_t auto_dfs_on_off_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	int err;

	err = get_dfs_auto_status(&data);
	if (err < 0)
		data = 0;
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t auto_dfs_on_off_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int enable;
	int err;

	err = sscanf(buf, "%u\n", &enable);
	if (err < 1) {
		pr_err("%s: enable para err: %d", __func__, err);
		return count;
	}
	if (enable == 1)
		err = dfs_auto_enable();
	else if (enable == 0)
		err = dfs_auto_disable();
	else
		err = -EINVAL;
	if (err)
		pr_err("%s: enable fail: %d", __func__, err);
	trace_sprd_dfs_sysfs(__func__, enable);
	return count;
}

static ssize_t ddrinfo_cur_freq_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	int err;

	err = get_cur_freq(&data);
	if (err < 0)
		data = 0;
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t ddrinfo_ap_freq_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	int err;

	err = get_ap_freq(&data);
	if (err < 0)
		data = 0;
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t ddrinfo_cp_freq_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int data;
	int err;

	err = get_cp_freq(&data);
	if (err < 0)
		data = 0;
	count = sprintf(buf, "%u\n", data);
	return count;
}

static ssize_t ddrinfo_freq_table_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int freq_num;
	unsigned int data;
	int i, err;

	err = get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = get_freq_table(&data, i);
		if (err < 0)
			data = 0;
		count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");
	return count;
}

static ssize_t scenario_dfs_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	int name_len;
	char *arg;

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg-buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;
	memcpy(arg, buf, name_len);
	err = scene_dfs_request(arg);
	if (err)
		pr_err("%s: scene enter fail: %d", __func__, err);
	kfree(arg);
	return count;
}

static ssize_t exit_scenario_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	int name_len;
	char *arg;

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg-buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;
	memcpy(arg, buf, name_len);
	err = scene_exit(arg);
	if (err)
		pr_err("%s: scene exit fail: %d", __func__, err);
	kfree(arg);
	return count;
}

static ssize_t scene_freq_set_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	int name_len;
	char *arg;
	unsigned int freq;

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg-buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;
	memcpy(arg, buf, name_len);

	err = sscanf(&buf[name_len], "%u\n", &freq);
	if (err < 1) {
		pr_err("%s: enable para err: %d", __func__, err);
		kfree(arg);
		return count;
	}

	err = change_scene_freq(arg, freq);
	if (err)
		pr_err("%s: scene freq set fail: %d", __func__, err);
	kfree(arg);
	return count;
}

static ssize_t scene_boost_dfs_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int enable;
	unsigned int freq;
	int err;

	err = sscanf(buf, "%u %u\n", &enable, &freq);
	if (err < 2) {
		pr_err("%s: overflow para err: %d", __func__, err);
		return count;
	}
	if (enable == 1)
		err = scene_dfs_request("boost");
	else if (enable == 0)
		err = scene_exit("boost");
	else
		err = -EINVAL;
	if (err)
		pr_err("%s: scene exit fail: %d", __func__, err);
	return count;
}

static ssize_t scene_dfs_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int scene_num;
	char *name;
	unsigned int freq;
	unsigned int num;
	unsigned int magic;

	int i, err;

	err = get_scene_num(&scene_num);
	if (err < 0)
		scene_num = 0;
	for (i = 0; i < scene_num; i++) {
		err = get_scene_info(&name, &freq, &num, &magic, i);
		if (err == 0)
			count += sprintf(&buf[count],
				"%s freq %u  magic 0x%x count %u\n",
				name, freq, magic, num);
	}
	return count;
}

static ssize_t backdoor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", backdoor_status);
}

static ssize_t backdoor_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	int backdoor;

	err = sscanf(buf, "%d\n", &backdoor);
	if (err < 1) {
		dev_err(dev, "set backdoor err: %d", err);
		return count;
	}

	if (backdoor_status == backdoor)
		return count;

	if (backdoor == 1)
		err = set_backdoor();
	else if (backdoor == 0)
		err = reset_backdoor();
	else
		err = -EINVAL;

	if (err)
		pr_err("%s: set backdoor fail: %d", __func__, err);
	else
		backdoor_status  = backdoor;

	return count;
}

static ssize_t trace_poll_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", trace_poll_time);
}

static ssize_t trace_poll_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int err;
	unsigned int time;

	err = sscanf(buf, "%u\n", &time);
	if (err < 1) {
		dev_err(dev, "poll dfs store err: %d", err);
		return count;
	}
	if (time > 5000)
		time = 5000;

	if (trace_poll_time == 0 && time != 0)
		queue_delayed_work(system_power_efficient_wq,
			&trace_poll_work, msecs_to_jiffies(trace_poll_time));
	else if (trace_poll_time != 0 && time == 0)
		cancel_delayed_work_sync(&trace_poll_work);

	trace_poll_time = time;
	return count;
}

static DEVICE_ATTR(scaling_force_ddr_freq, 0664,
	scaling_force_ddr_freq_show, scaling_force_ddr_freq_store);
static DEVICE_ATTR(scaling_overflow, 0664,
	scaling_overflow_show, scaling_overflow_store);
static DEVICE_ATTR(scaling_underflow, 0664,
	scaling_underflow_show, scaling_underflow_store);
static DEVICE_ATTR(dfs_on_off, 0664,
	dfs_on_off_show, dfs_on_off_store);
static DEVICE_ATTR(auto_dfs_on_off, 0664,
	auto_dfs_on_off_show, auto_dfs_on_off_store);
static DEVICE_ATTR(ddrinfo_cur_freq, 0444,
	ddrinfo_cur_freq_show, NULL);
static DEVICE_ATTR(ddrinfo_ap_freq, 0444,
	ddrinfo_ap_freq_show, NULL);
static DEVICE_ATTR(ddrinfo_cp_freq, 0444,
	ddrinfo_cp_freq_show, NULL);
static DEVICE_ATTR(ddrinfo_freq_table, 0444,
	ddrinfo_freq_table_show, NULL);
static DEVICE_ATTR(scenario_dfs, 0220,
	NULL, scenario_dfs_store);
static DEVICE_ATTR(exit_scene, 0220,
	NULL, exit_scenario_store);
static DEVICE_ATTR(scene_freq_set, 0220,
	NULL, scene_freq_set_store);
static DEVICE_ATTR(scene_boost_dfs, 0220,
	NULL, scene_boost_dfs_store);
static DEVICE_ATTR(scene_dfs_list, 0444,
	scene_dfs_status_show, NULL);
static DEVICE_ATTR(backdoor, 0664,
	backdoor_show, backdoor_store);
static DEVICE_ATTR(trace_poll, 0664,
	trace_poll_show, trace_poll_store);

static struct attribute *dev_entries[] = {
	&dev_attr_scaling_force_ddr_freq.attr,
	&dev_attr_scaling_overflow.attr,
	&dev_attr_scaling_underflow.attr,
	&dev_attr_dfs_on_off.attr,
	&dev_attr_auto_dfs_on_off.attr,
	&dev_attr_ddrinfo_cur_freq.attr,
	&dev_attr_ddrinfo_ap_freq.attr,
	&dev_attr_ddrinfo_cp_freq.attr,
	&dev_attr_ddrinfo_freq_table.attr,
	&dev_attr_scenario_dfs.attr,
	&dev_attr_exit_scene.attr,
	&dev_attr_scene_freq_set.attr,
	&dev_attr_scene_boost_dfs.attr,
	&dev_attr_scene_dfs_list.attr,
	&dev_attr_backdoor.attr,
	&dev_attr_trace_poll.attr,
	NULL,
};
/*****************userspace end********************/

static struct attribute_group dev_attr_group = {
	.name	= "sprd_governor",
	.attrs	= dev_entries,
};

static int devfreq_sprd_gov_start(struct devfreq *devfreq)
{
	int err = 0;

	err = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);
	if (err)
		pr_err("%s:sysfs create fail: %d", __func__, err);
	return err;
}


static int devfreq_sprd_gov_stop(struct devfreq *devfreq)
{
	return 0;
}


static int devfreq_sprd_gov_func(struct devfreq *df,
					unsigned long *freq)
{
	*freq = 933;
	return 0;
}


static int devfreq_sprd_gov_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_sprd_gov_start(devfreq);
		break;
	case DEVFREQ_GOV_STOP:
		devfreq_sprd_gov_stop(devfreq);
		break;
	case DEVFREQ_GOV_INTERVAL:
		break;
	case DEVFREQ_GOV_SUSPEND:
		break;
	case DEVFREQ_GOV_RESUME:
		break;
	default:
		break;
	}

	return 0;
}

struct devfreq_governor devfreq_sprd_gov = {
	.name = "sprd_governor",
	.get_target_freq = devfreq_sprd_gov_func,
	.event_handler = devfreq_sprd_gov_handler,
};

static int __init devfreq_sprd_gov_init(void)
{
	trace_poll_time = 0;
	backdoor_status = 0;
	return devfreq_add_governor(&devfreq_sprd_gov);
}
subsys_initcall(devfreq_sprd_gov_init);

static void __exit devfreq_sprd_gov_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_sprd_gov);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);
}

module_exit(devfreq_sprd_gov_exit);
MODULE_LICENSE("GPL");
