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
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>

#include "disp_lib.h"
#include "sprd_dphy.h"
#include "sysfs_display.h"

static int hop_freq;
static int ssc_en;
static int ulps_en;
static u32 input_param[64];
static u32 read_buf[64];

static ssize_t reg_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int i;
	u32 reg;
	u8 reg_stride;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	struct regmap *regmap = ctx->regmap;

	if (!regmap)
		return -ENODEV;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.is_enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	str_to_u32_array(buf, 0, input_param);

	reg_stride = regmap_get_reg_stride(regmap);

	for (i = 0; i < (input_param[1] ? : 1); i++) {
		reg = input_param[0] + i * reg_stride;
		regmap_read(regmap, reg, &read_buf[i]);
	}
	mutex_unlock(&dphy->ctx.lock);

	return count;
}

static ssize_t reg_read_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	int i;
	int ret = 0;
	u8 val_width;
	u8 reg_stride;
	const char *fmt = NULL;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	struct regmap *regmap = ctx->regmap;

	if (!regmap)
		return -ENODEV;

	val_width = regmap_get_val_bytes(regmap);
	reg_stride = regmap_get_reg_stride(regmap);

	if (val_width == 1) {
		ret += snprintf(buf + ret, PAGE_SIZE,
				" ADDR | VALUE\n");
		ret += snprintf(buf + ret, PAGE_SIZE,
				"------+------\n");
		fmt = " 0x%02x | 0x%02x\n";
	} else if (val_width == 4) {
		ret += snprintf(buf + ret, PAGE_SIZE,
				"  ADDRESS  |   VALUE\n");
		ret += snprintf(buf + ret, PAGE_SIZE,
				"-----------+-----------\n");
		fmt = "0x%08x | 0x%08x\n";
	} else
		return -ENODEV;

	for (i = 0; i < (input_param[1] ? : 1); i++)
		ret += snprintf(buf + ret, PAGE_SIZE, fmt,
				input_param[0] + i * reg_stride,
				read_buf[i]);

	return ret;
}
static DEVICE_ATTR_RW(reg_read);

static ssize_t reg_write_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int i, len;
	u8 reg_stride;
	u32 reg, val;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;
	struct regmap *regmap = ctx->regmap;

	if (!regmap)
		return -ENODEV;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.is_enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	len = str_to_u32_array(buf, 16, input_param);

	reg_stride = regmap_get_reg_stride(regmap);

	for (i = 0; i < len - 1; i++) {
		reg = input_param[0] + i * reg_stride;
		val = input_param[1 + i];
		regmap_write(regmap, reg, val);
	}
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_WO(reg_write);

static ssize_t ssc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", ssc_en);

	return ret;
}

static ssize_t ssc_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.is_enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	ret = kstrtoint(buf, 10, &ssc_en);
	if (ret) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("Invalid input value\n");
		return -EINVAL;
	}

	sprd_dphy_ssc_en(dphy, ssc_en);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_RW(ssc);

static ssize_t hop_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", hop_freq);

	return ret;
}

static ssize_t hop_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int delta;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.is_enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	ret = kstrtoint(buf, 10, &hop_freq);
	if (ret) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("Invalid input freq\n");
		return -EINVAL;
	}

	/*
	 * because of double edge trigger,
	 * the rule is actual freq * 10 / 2,
	 * Eg: Required freq is 500M
	 * Equation: 2500*2*1000/10=500*1000=2500*200=500M
	 */
	if (hop_freq == 0)
		hop_freq = ctx->freq;
	else
		hop_freq *= 200;
	pr_info("input freq is %d\n", hop_freq);

	delta = hop_freq - ctx->freq;
	sprd_dphy_hop_config(dphy, delta, 200);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_RW(hop);

static ssize_t ulps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", ulps_en);

	return ret;
}

static ssize_t ulps_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.is_enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	ret = kstrtoint(buf, 10, &ulps_en);
	if (ret) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("Invalid input freq\n");
		return -EINVAL;
	}

	if (ulps_en)
		sprd_dphy_ulps_enter(dphy);
	else
		sprd_dphy_ulps_exit(dphy);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_RW(ulps);

static ssize_t freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", ctx->freq);

	return ret;
}

static ssize_t freq_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int freq;
	struct sprd_dphy *dphy = dev_get_drvdata(dev);
	struct dphy_context *ctx = &dphy->ctx;

	ret = kstrtoint(buf, 10, &freq);
	if (ret) {
		pr_err("Invalid input freq\n");
		return -EINVAL;
	}

	if (freq == ctx->freq) {
		pr_info("input freq is the same as old\n");
		return count;
	}

	pr_info("input freq is %d\n", freq);

	ctx->freq = freq;

	return count;
}
static DEVICE_ATTR_RW(freq);

static ssize_t suspend_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	sprd_dphy_suspend(dphy);

	return count;
}
static DEVICE_ATTR_WO(suspend);

static ssize_t resume_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	sprd_dphy_resume(dphy);

	return count;
}
static DEVICE_ATTR_WO(resume);

static ssize_t reset_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.is_enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	sprd_dphy_reset(dphy);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_WO(reset);

static ssize_t shutdown_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_dphy *dphy = dev_get_drvdata(dev);

	mutex_lock(&dphy->ctx.lock);
	if (!dphy->ctx.is_enabled) {
		mutex_unlock(&dphy->ctx.lock);
		pr_err("dphy is not initialized\n");
		return -ENXIO;
	}

	sprd_dphy_shutdown(dphy);
	mutex_unlock(&dphy->ctx.lock);

	return count;
}
static DEVICE_ATTR_WO(shutdown);

static struct attribute *dphy_attrs[] = {
	&dev_attr_reg_read.attr,
	&dev_attr_reg_write.attr,
	&dev_attr_ssc.attr,
	&dev_attr_hop.attr,
	&dev_attr_ulps.attr,
	&dev_attr_freq.attr,
	&dev_attr_suspend.attr,
	&dev_attr_resume.attr,
	&dev_attr_reset.attr,
	&dev_attr_shutdown.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dphy);

int sprd_dphy_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&dev->kobj, dphy_groups);
	if (rc)
		pr_err("create dphy attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_dphy_sysfs_init);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Add dphy attribute nodes for userspace");
MODULE_LICENSE("GPL v2");
