//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>

struct pmic_glb {
	u32 reg;
	u32 base;
	struct regmap *regmap;
	struct device *dev;
};

static ssize_t pmic_reg_show(struct device *dev,
			struct device_attribute *attr,  char *buf)
{
	struct pmic_glb *sc27xx_glb = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x", sc27xx_glb->reg);
}

static ssize_t pmic_reg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct pmic_glb *sc27xx_glb = dev_get_drvdata(dev);

	ret = sscanf(buf, "%x", &sc27xx_glb->reg);
	if (ret != 1) {
		dev_err(dev->parent, "error input\n");
		return -EINVAL;
	}

	return strnlen(buf, count);
}
static DEVICE_ATTR_RW(pmic_reg);

static ssize_t pmic_value_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int ret;
	u32 value;
	struct pmic_glb *sc27xx_glb = dev_get_drvdata(dev);

	if (sc27xx_glb->reg < sc27xx_glb->base) {
		dev_err(dev->parent, "out of reg range\n");
		return -EINVAL;
	}

	ret = regmap_read(sc27xx_glb->regmap, sc27xx_glb->reg, &value);
	if (ret) {
		dev_err(dev->parent, "unable to get glb\n");
		return ret;
	}

	return sprintf(buf, "%x", value);
}

static ssize_t pmic_value_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	u32 value;
	struct pmic_glb *sc27xx_glb = dev_get_drvdata(dev);

	ret = sscanf(buf, "%x", &value);
	if (ret != 1) {
		dev_err(dev->parent, "error input\n");
		return -EINVAL;
	}

	if (sc27xx_glb->reg < sc27xx_glb->base) {
		dev_err(dev->parent, "out of reg range\n");
		return -EINVAL;
	}

	ret = regmap_write(sc27xx_glb->regmap, sc27xx_glb->reg, value);
	if (ret) {
		dev_err(dev->parent, "unable to write glb\n");
		return ret;
	}

	return count;
}
static DEVICE_ATTR_RW(pmic_value);

static struct attribute *pmic_syscon_attrs[] = {
	&dev_attr_pmic_reg.attr,
	&dev_attr_pmic_value.attr,
	NULL
};
ATTRIBUTE_GROUPS(pmic_syscon);

static int sprd_pmic_glb_probe(struct platform_device *pdev)
{
	int ret;
	struct pmic_glb *sc27xx_glb;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	sc27xx_glb = devm_kzalloc(dev, sizeof(struct pmic_glb),
						GFP_KERNEL);
	if (!sc27xx_glb)
		return -ENOMEM;

	sc27xx_glb->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sc27xx_glb->regmap) {
		dev_err(dev, "get regmap fail\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(np, "reg", 0, &sc27xx_glb->base);
	if (ret) {
		dev_err(dev, "get base register failed\n");
		return -EINVAL;
	}

	sc27xx_glb->dev = &pdev->dev;

	ret = sysfs_create_groups(&sc27xx_glb->dev->kobj,
				  pmic_syscon_groups);
	if (ret)
		dev_warn(dev, "failed to create pmic_syscon attributes\n");

	dev_set_drvdata(dev, sc27xx_glb);

	return 0;
}

static int sprd_pmic_glb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pmic_glb *sc27xx_glb = dev_get_drvdata(dev);

	sysfs_remove_groups(&sc27xx_glb->dev->kobj, pmic_syscon_groups);

	return 0;
}
static const struct of_device_id sprd_pmic_glb_match[] = {
	{ .compatible = "sprd,sc27xx-syscon"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pmic_glb_match);

static struct platform_driver sprd_pmic_glb_driver = {
	.probe = sprd_pmic_glb_probe,
	.remove = sprd_pmic_glb_remove,
	.driver = {
		.name = "sprd-pmic-glb",
		.of_match_table = sprd_pmic_glb_match,
	},
};

module_platform_driver(sprd_pmic_glb_driver);

MODULE_AUTHOR("Erick Chen <erick.chen@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum pmic glob driver");
MODULE_LICENSE("GPL v2");
