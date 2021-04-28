// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

/*
 * SC2730 7s reset mask
 */
#define BIT_KEY2_7S_RST_EN		BIT(9)
#define BIT_PBINT_7S_RST_SWMODE		BIT(8)
#define BITS_PBINT_7S_RST_THRESHOLD(x)	GENMASK(7, 4)
#define BIT_PBINT_7S_RST_DISABLE	BIT(1)
#define BIT_PBINT_7S_RST_MODE		BIT(0)

struct sprd_7sreset {
	struct device *dev;
	struct regmap *reg_map;
	u32 reg_reset_ctrl;
	u32 reg_7s_ctrl;
	unsigned long chip_ver;
};

static const struct of_device_id sprd_7sreset_of_match[] = {
	{.compatible = "sprd,sc2720-7sreset", .data = (void *)0x2720},
	{.compatible = "sprd,sc2721-7sreset", .data = (void *)0x2721},
	{.compatible = "sprd,sc2730-7sreset", .data = (void *)0x2730},
	{.compatible = "sprd,sc2731-7sreset", .data = (void *)0x2731},
	{}
};
MODULE_DEVICE_TABLE(of, sprd_7sreset_of_match);

static int sprd_7sreset_disable(struct device *dev, int disable)
{
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	if (disable)
		return regmap_update_bits(sprd_7sreset_dev->reg_map,
					  sprd_7sreset_dev->reg_7s_ctrl,
					  BIT_PBINT_7S_RST_DISABLE,
					  BIT_PBINT_7S_RST_DISABLE);

	return regmap_update_bits(sprd_7sreset_dev->reg_map,
				  sprd_7sreset_dev->reg_7s_ctrl,
				  BIT_PBINT_7S_RST_DISABLE, 0);
}

static int sprd_7sreset_is_disable(struct device *dev)
{
	int ret;
	u32 r_val;
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	ret = regmap_read(sprd_7sreset_dev->reg_map,
			  sprd_7sreset_dev->reg_7s_ctrl, &r_val);
	if (ret)
		return ret;

	return !!(r_val & BIT_PBINT_7S_RST_DISABLE);
}

static int sprd_7sreset_set_keymode(struct device *dev, int mode)
{
	u32 reg;
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	if (sprd_7sreset_dev->chip_ver == 0x2731)
		reg = sprd_7sreset_dev->reg_reset_ctrl;
	else
		reg = sprd_7sreset_dev->reg_7s_ctrl;

	if (!mode)
		return regmap_update_bits(sprd_7sreset_dev->reg_map, reg,
					  BIT_KEY2_7S_RST_EN,
					  BIT_KEY2_7S_RST_EN);

	return regmap_update_bits(sprd_7sreset_dev->reg_map, reg,
				  BIT_KEY2_7S_RST_EN, 0);
}

static int sprd_7sreset_get_keymode(struct device *dev)
{
	int ret;
	u32 reg, r_val;
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	if (sprd_7sreset_dev->chip_ver == 0x2731)
		reg = sprd_7sreset_dev->reg_reset_ctrl;
	else
		reg = sprd_7sreset_dev->reg_7s_ctrl;

	ret = regmap_read(sprd_7sreset_dev->reg_map, reg, &r_val);
	if (ret)
		return ret;

	return !(r_val & BIT_KEY2_7S_RST_EN);
}

static int sprd_7sreset_set_resetmode(struct device *dev, int mode)
{
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	if (mode)
		return regmap_update_bits(sprd_7sreset_dev->reg_map,
					  sprd_7sreset_dev->reg_7s_ctrl,
					  BIT_PBINT_7S_RST_MODE,
					  BIT_PBINT_7S_RST_MODE);

	return regmap_update_bits(sprd_7sreset_dev->reg_map,
				  sprd_7sreset_dev->reg_7s_ctrl,
				  BIT_PBINT_7S_RST_MODE, 0);
}

static int sprd_7sreset_get_resetmode(struct device *dev)
{
	int ret;
	u32 r_val;
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	ret = regmap_read(sprd_7sreset_dev->reg_map,
			  sprd_7sreset_dev->reg_7s_ctrl, &r_val);
	if (ret)
		return ret;

	return !!(r_val & BIT_PBINT_7S_RST_MODE);
}

static int sprd_7sreset_set_shortmode(struct device *dev, int mode)
{
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	if (mode)
		return regmap_update_bits(sprd_7sreset_dev->reg_map,
					  sprd_7sreset_dev->reg_7s_ctrl,
					  BIT_PBINT_7S_RST_SWMODE,
					  BIT_PBINT_7S_RST_SWMODE);

	return regmap_update_bits(sprd_7sreset_dev->reg_map,
				  sprd_7sreset_dev->reg_7s_ctrl,
				  BIT_PBINT_7S_RST_SWMODE, 0);
}

static int sprd_7sreset_get_shortmode(struct device *dev)
{
	int ret;
	u32 r_val;
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	ret = regmap_read(sprd_7sreset_dev->reg_map,
			  sprd_7sreset_dev->reg_7s_ctrl, &r_val);
	if (ret)
		return ret;

	return !!(r_val & BIT_PBINT_7S_RST_SWMODE);
}

static int sprd_7sreset_set_threshold(struct device *dev, u32 th)
{
	int shft = __ffs(BITS_PBINT_7S_RST_THRESHOLD(~0U));
	u32 max = BITS_PBINT_7S_RST_THRESHOLD(~0U) >> shft;
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	if (th > max)
		th = max;

	return regmap_update_bits(sprd_7sreset_dev->reg_map,
				  sprd_7sreset_dev->reg_7s_ctrl,
				  BITS_PBINT_7S_RST_THRESHOLD(~0U),
				  BITS_PBINT_7S_RST_THRESHOLD(th));
}

static int sprd_7sreset_get_threshold(struct device *dev)
{
	u32 r_val;
	int ret, shft = __ffs(BITS_PBINT_7S_RST_THRESHOLD(~0U));
	struct sprd_7sreset *sprd_7sreset_dev = dev_get_drvdata(dev);

	ret = regmap_read(sprd_7sreset_dev->reg_map,
			  sprd_7sreset_dev->reg_7s_ctrl, &r_val);
	if (ret)
		return ret;

	return ((r_val & BITS_PBINT_7S_RST_THRESHOLD(~0U)) >> shft);
}

static ssize_t module_disable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret, set_value;

	ret = kstrtoint(buf, 10, &set_value);
	if (ret < 0)
		return ret;

	ret = sprd_7sreset_disable(dev, set_value);
	if (ret)
		return ret;

	return count;
}

static ssize_t module_disable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", sprd_7sreset_is_disable(dev));
}
static DEVICE_ATTR_RW(module_disable);

static ssize_t hard_mode_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret, set_value;

	ret = kstrtoint(buf, 10, &set_value);
	if (ret < 0)
		return ret;

	ret = sprd_7sreset_set_resetmode(dev, set_value);
	if (ret)
		return ret;

	return count;
}

static ssize_t hard_mode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 sprd_7sreset_get_resetmode(dev));
}
static DEVICE_ATTR_RW(hard_mode);

static ssize_t short_mode_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret, set_value;

	ret = kstrtoint(buf, 10, &set_value);
	if (ret < 0)
		return ret;

	ret = sprd_7sreset_set_shortmode(dev, set_value);
	if (ret)
		return ret;

	return count;
}

static ssize_t short_mode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 sprd_7sreset_get_shortmode(dev));
}
static DEVICE_ATTR_RW(short_mode);

static ssize_t keypad_mode_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret, set_value;

	ret = kstrtoint(buf, 10, &set_value);
	if (ret < 0)
		return ret;

	ret = sprd_7sreset_set_keymode(dev, set_value);
	if (ret)
		return ret;

	return count;
}

static ssize_t keypad_mode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", sprd_7sreset_get_keymode(dev));
}
static DEVICE_ATTR_RW(keypad_mode);

static ssize_t threshold_time_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret, set_value;

	ret = kstrtoint(buf, 10, &set_value);
	if (ret < 0)
		return ret;

	ret = sprd_7sreset_set_threshold(dev, set_value);
	if (ret)
		return ret;

	return count;
}

static ssize_t threshold_time_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 sprd_7sreset_get_threshold(dev));
}
static DEVICE_ATTR_RW(threshold_time);

static struct attribute *sprd7sreset_attrs[] = {
	&dev_attr_module_disable.attr,
	&dev_attr_hard_mode.attr,
	&dev_attr_short_mode.attr,
	&dev_attr_keypad_mode.attr,
	&dev_attr_threshold_time.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sprd7sreset);

static int sprd_7sreset_probe(struct platform_device *pdev)
{
	int ret;
	u32 val;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct sprd_7sreset *sprd_7sreset_dev;

	sprd_7sreset_dev = devm_kzalloc(&pdev->dev, sizeof(*sprd_7sreset_dev),
					GFP_KERNEL);
	if (!sprd_7sreset_dev)
		return -ENOMEM;

	sprd_7sreset_dev->reg_map = dev_get_regmap(dev->parent, NULL);
	if (!sprd_7sreset_dev->reg_map) {
		dev_err(&pdev->dev, "NULL regmap for pmic\n");
		return -EINVAL;
	}

	sprd_7sreset_dev->dev = dev;

	sprd_7sreset_dev->chip_ver =
		(unsigned long)of_device_get_match_data(dev);

	ret = of_property_read_u32_index(np, "reg", 0, &val);
	if (ret)
		return ret;

	sprd_7sreset_dev->reg_reset_ctrl = val;
	ret = of_property_read_u32_index(np, "reg", 1, &val);
	if (ret)
		return ret;

	sprd_7sreset_dev->reg_7s_ctrl = val;

	ret = sysfs_create_groups(&dev->kobj, sprd7sreset_groups);
	if (ret) {
		dev_err(&pdev->dev, "failed to create device attributes.\n");
		return ret;
	}

	platform_set_drvdata(pdev, sprd_7sreset_dev);

	return 0;
}

static int sprd_7sreset_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_groups(&dev->kobj, sprd7sreset_groups);

	return 0;
}

static struct platform_driver sprd_7sreset_drv = {
	.driver = {
		   .name = "sc27xx-7sreset",
		   .of_match_table = sprd_7sreset_of_match,
	},
	.probe = sprd_7sreset_probe,
	.remove = sprd_7sreset_remove
};

module_platform_driver(sprd_7sreset_drv);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Unisoc sc27xx 7SReset Driver");
MODULE_AUTHOR("Erick Chen <erick.chen@unisoc.com>");
