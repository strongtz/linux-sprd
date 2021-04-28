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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/cpufreq.h>
#include "sprd-hwdvfs-normal.h"

struct cpudvfs_archdata *gpdev;

int target_device_hit(struct dvfs_cluster **clu, struct kobject *kobj)
{
	struct dvfs_cluster *cluster;
	struct sub_device *sdev;
	int ix, jx, ret;
	bool flag = false;

	for (ix = 0; ix < gpdev->total_cluster_num; ++ix) {
		cluster = gpdev->cluster_array[ix];
		for (jx = 0; jx < cluster->device_num; ++jx) {
			sdev = &cluster->subdevs[jx];
			if (!strcmp(kobj->name, sdev->name)) {
				flag = true;
				goto out;
			}
		}
	}

out:
	if (flag) {
		*clu = cluster;
		ret = jx;
	} else {
		*clu = NULL;
		ret = -EINVAL;
	}

	return ret;
}

int target_pwr_hit(struct kobject *kobj)
{
	struct dcdc_pwr *pwr;
	int ret, i;
	bool flag = false;

	for (i = 0; i < gpdev->dcdc_num; ++i) {
		pwr = &gpdev->pwr[i];
		if (!strcmp(kobj->name, pwr->name)) {
			flag = true;
			goto out;
		}
	}
out:
	if (flag)
		ret = i;
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t target_show(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct dvfs_cluster *cluster;
	int id, index;

	id = target_device_hit(&cluster, kobj);
	if (id < 0) {
		pr_err("No device found\n");
		return -EINVAL;
	}

	index = gpdev->phy_ops->get_dvfs_index(gpdev, cluster->id, 1);
	return sprintf(buf, "%d\n", index);
}

static ssize_t target_store(struct kobject *kobj,
			    struct kobj_attribute *attr, const char *buf,
			    size_t count)
{
	struct dvfs_cluster *cluster;
	unsigned int index;
	int id;
	size_t ret;

	id = target_device_hit(&cluster, kobj);
	if (id < 0) {
		pr_err("No device found\n");
		return id;
	}

	ret = kstrtouint(buf, 0, &index);
	if (ret)
		return ret;

	ret = gpdev->phy_ops->set_dvfs_work_index(gpdev, cluster->id, index);
	if (ret)
		return ret;

	return count;
}

static ssize_t sel_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct dvfs_cluster *cluster;
	int sel, jx;

	jx = target_device_hit(&cluster, kobj);
	if (jx < 0) {
		pr_err("No device found\n");
		return -EINVAL;
	}

	sel = gpdev->phy_ops->get_cgm_sel_value(gpdev, cluster->id, jx);

	return sprintf(buf, "%d\n", sel);
}

static ssize_t div_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct dvfs_cluster *cluster;
	int div, jx;

	jx = target_device_hit(&cluster, kobj);
	if (jx < 0) {
		pr_err("No device found\n");
		return -EINVAL;
	}

	div = gpdev->phy_ops->get_cgm_div_value(gpdev, cluster->id, jx);

	return sprintf(buf, "%d\n", div);
}

static ssize_t voted_volt_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	struct dvfs_cluster *cluster;
	int volt, jx;

	jx = target_device_hit(&cluster, kobj);
	if (jx < 0) {
		pr_err("No device found\n");
		return -EINVAL;
	}

	volt = gpdev->phy_ops->get_cgm_voted_volt(gpdev, cluster->id, jx);

	return sprintf(buf, "%d\n", volt);
}

static struct kobj_attribute target_kobj_attr = __ATTR_RW(target);
static struct kobj_attribute sel_kobj_attr = __ATTR_RO(sel);
static struct kobj_attribute div_kobj_attr = __ATTR_RO(div);
static struct kobj_attribute voted_volt_kobj_attr = __ATTR_RO(voted_volt);

static struct attribute *apcpu_attributes[] = {
	&target_kobj_attr.attr,
	&sel_kobj_attr.attr,
	&div_kobj_attr.attr,
	&voted_volt_kobj_attr.attr,
	NULL,
};

static struct attribute_group apcpu_attr_group = {
	.attrs = apcpu_attributes,
};

struct sysfs_attr {
	const char *name;
	struct attribute_group *group;
};

const char *apcpu_dvfs_state[] = {
	"IDLE",
	"V_UP",
	"V_REQ",
	"V_DOWN",
	"NULL",
	"V_COMPLETE",
	"V_WAIT",
	"V_UPDATE",
	"Incorrect state"
};

static ssize_t apcpu_dvfs_state_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	u32 state;
	int i;

	i = target_pwr_hit(kobj);
	if (i < 0) {
		pr_err("No device found\n");
		return -EINVAL;
	}

	state = gpdev->phy_ops->get_sys_dcdc_dvfs_state(gpdev, i);

	if (state < 0 || state >= ARRAY_SIZE(apcpu_dvfs_state))
		state = ARRAY_SIZE(apcpu_dvfs_state) - 1;

	return sprintf(buf, "%s\n", apcpu_dvfs_state[state]);
}

const char *top_dvfs_state_adi[] = {
	"IDLE",
	"V_UP",
	"V_UP_PREPARE",
	"V_UP_REQ",
	"V_UP_HDSK",
	"V_UP_WAIT",
	"V_UP_UPD",
	"V_DOWN",
	"V_DOWN_PREPARE",
	"V_DOWN_REQ",
	"V_DOWN_HDSK",
	"V_DOWN_WAIT",
	"V_DOWN_UPD",
	"V_DOWN_COMPLETE",
	"Incorrect state"
};

const char *top_dvfs_state_i2c[] = {
	"IDLE",
	"V_UP",
	"V_UP_PREPARE",
	"V_UP_REQ_0",
	"V_UP_HDSK_0",
	"V_UP_WAIT",
	"V_UP_UPD",
	"V_DOWN",
	"V_DOWN_PREPARE",
	"V_DOWN_REQ_0",
	"V_DOWN_HDSK_0",
	"V_DOWN_WAIT",
	"V_DOWN_UPD",
	"V_DOWN_REQ_1",
	"V_UP_REQ_1",
	"V_UP_HDSK_1",
	"V_COMPLETE",
	"Incorrect state"
};

static ssize_t top_dvfs_state_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	u32 value;
	bool use_i2c;
	int i;
	const char *state_name;

	i = target_pwr_hit(kobj);
	if (i < 0) {
		pr_err("No device found\n");
		return -EINVAL;
	}

	use_i2c = gpdev->pwr[i].i2c_used;

	value = gpdev->phy_ops->get_top_dcdc_dvfs_state(gpdev, i);

	if (use_i2c) {
		if (value < 0 || value >= ARRAY_SIZE(top_dvfs_state_i2c))
			value = ARRAY_SIZE(top_dvfs_state_i2c) - 1;
		state_name = top_dvfs_state_i2c[value];
	} else {
		if (value < 0 || value >= ARRAY_SIZE(top_dvfs_state_adi))
			value = ARRAY_SIZE(top_dvfs_state_adi) - 1;
		state_name = top_dvfs_state_adi[value];
	}

	return sprintf(buf, "%s\n", state_name);
}

static struct kobj_attribute top_dvfs_state_kobj_attr =
						__ATTR_RO(top_dvfs_state);
static struct kobj_attribute apcpu_dvfs_state_kobj_attr =
						__ATTR_RO(apcpu_dvfs_state);

static struct attribute *dcdc_dvfs_state_attributes[] = {
	&top_dvfs_state_kobj_attr.attr,
	&apcpu_dvfs_state_kobj_attr.attr,
	NULL,
};

static struct attribute_group dcdc_attr_group = {
	.attrs = dcdc_dvfs_state_attributes,
};

struct dcdc_attr {
	char name[10];
	struct attribute_group *group;
};

static int attr_group_array_init(struct cpudvfs_archdata *pdev,
				 struct sysfs_attr  **pattr)
{
	struct sysfs_attr *attrgp;
	struct dvfs_cluster *cluster;
	u32 size, ix, id = 0;
	u32 device_num = 0, clu;

	gpdev = pdev;

	for (clu = 0; clu < pdev->total_cluster_num; ++clu)
		device_num += pdev->cluster_array[clu]->device_num;

	size = device_num * sizeof(struct sysfs_attr);

	attrgp = kzalloc(size, GFP_KERNEL);
	if (!attrgp)
		return -ENOMEM;

	for (clu = 0; clu < pdev->total_cluster_num; ++clu) {
		cluster = pdev->cluster_array[clu];
		for (ix = 0; ix < cluster->device_num; ++ix) {
			attrgp[id].name = cluster->subdevs[ix].name;
			attrgp[id].group = &apcpu_attr_group;
			id++;
		}
	}

	*pattr = attrgp;

	return device_num;
}

static int dcdc_group_array_init(struct cpudvfs_archdata *pdev,
				 struct dcdc_attr **pattr)
{
	struct dcdc_attr *attrgp;
	u32 id, size;

	size = pdev->dcdc_num * sizeof(struct dcdc_attr);

	attrgp = kzalloc(size, GFP_KERNEL);
	if (!attrgp)
		return -ENOMEM;

	for (id = 0; id < pdev->dcdc_num; id++) {
		strcpy(attrgp[id].name, pdev->pwr[id].name);
		attrgp[id].group = &dcdc_attr_group;
	}
	*pattr = attrgp;

	return pdev->dcdc_num;
}

int cpudvfs_sysfs_create(struct cpudvfs_archdata *pdev)
{
	struct kobject *cpudvfs_kobj, *dcdc_kobj;
	struct dcdc_attr *dcdc_group_array;
	struct sysfs_attr *group_array;
	struct kobject *dev_kobj;
	struct kobject *tmp_kobj;
	int size, ret, ix;
	const char *name;

	size = attr_group_array_init(pdev, &group_array);
	if (size < 0)
		return size;

	cpudvfs_kobj = kobject_create_and_add("cpudvfs",
					      cpufreq_global_kobject);
	if (!cpudvfs_kobj) {
		ret = -ENOMEM;
		pr_err("failed to add 'cpudvfs' under cpufreq\n");
		goto cpudvfs_kobj_error;
	} else {
		dev_kobj = cpudvfs_kobj;
		for (ix = 0; ix < size; ++ix) {
			name = group_array[ix].name;
			tmp_kobj = kobject_create_and_add(name, dev_kobj);
			if (!(tmp_kobj &&
			      !sysfs_create_group(tmp_kobj,
						  group_array[ix].group))) {
				ret = -ENOMEM;
				pr_err("failed to add '%s' node\n", name);
				goto sysfs_error;
			}
		}
	}

	size = dcdc_group_array_init(pdev, &dcdc_group_array);
	if (size < 0) {
		ret = size;
		goto sysfs_error;
	}

	dcdc_kobj = kobject_create_and_add("dcdc", cpudvfs_kobj);
	if (!dcdc_kobj) {
		ret = -EPERM;
		pr_err("failed to add 'dcdc' sub node\n");
		goto sysfs_error;
	} else {
		for (ix = 0; ix < size; ++ix) {
			name = dcdc_group_array[ix].name;
			tmp_kobj = kobject_create_and_add(name, dcdc_kobj);
			if (!(tmp_kobj &&
			      !sysfs_create_group(tmp_kobj,
			      dcdc_group_array[ix].group))) {
				ret = -ENOMEM;
				pr_err("failed to add '%s' node", name);
				goto dcdc_error;
			}
		}
	}

	return 0;

dcdc_error:
	kobject_put(dcdc_kobj);
	kfree(dcdc_group_array);
sysfs_error:
	kobject_put(cpudvfs_kobj);
cpudvfs_kobj_error:
	kobject_put(cpufreq_global_kobject);
	kfree(group_array);

	return ret;
}
