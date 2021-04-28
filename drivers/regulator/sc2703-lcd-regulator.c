/*
 * LCD regulator driver for SC2703.
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

#include <linux/mfd/sc2703/sc2703-disp.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

static int lcd_regulator_enable(struct regulator_dev *rdev)
{
	pr_info("sc2703-lcd-regulator: enable\n");
	return regulator_enable_regmap(rdev);
}

static int lcd_regulator_disable(struct regulator_dev *rdev)
{
	pr_info("sc2703-lcd-regulator: disable\n");
	return regulator_disable_regmap(rdev);
}

static struct regulator_ops lcd_regulator_ops = {
	.enable = lcd_regulator_enable,
	.disable = lcd_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
};

static const struct regulator_desc lcd_regulator_desc = {
	.name = "lcd-regulator",
	.supply_name = "vdden",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &lcd_regulator_ops,
	.enable_reg = SC2703_SYSCTRL_DISPLAY_ACTIVE,
	.enable_mask = SC2703_DISPLAY_EN_MASK,
};

static int sc2703_lcd_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct regmap *regmap;
	int ret;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "get i2c regmap failed\n");
		return -ENODEV;
	}

	config.dev = &pdev->dev;
	config.regmap = regmap;
	config.of_node = pdev->dev.of_node;
	config.init_data = of_get_regulator_init_data(&pdev->dev,
				pdev->dev.of_node, &lcd_regulator_desc);

	rdev = devm_regulator_register(&pdev->dev,
				       &lcd_regulator_desc, &config);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(&pdev->dev,
			"failed to register regulator: (%d)\n", ret);
		return ret;
	}

	/*
	 * The regulator is auto enabled, if the panel driver does't
	 * call regulator_enable() after probe, this regulator will
	 * be automatic disabled. So increase the use_count to avoid
	 * auto disable.
	 */
	rdev->use_count++;

	/* select display power control by i2c */
	ret = regmap_update_bits(regmap, SC2703_SYSCTRL_SEQ_MODE_CONTROL1,
				SC2703_DISPLAY_POWER_CTRL_SELECT_MASK, 0);
	if (ret) {
		dev_err(&pdev->dev, "set display power control failed\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id sc2703_regulator_of_match[] = {
	{ .compatible = "sprd,sc2703-lcd-regulator", },
	{ }
};

static struct platform_driver sc2703_lcd_regulator_driver = {
	.driver = {
		.name		= "sc2703-lcd-regulator",
		.of_match_table	= sc2703_regulator_of_match,
	},
	.probe = sc2703_lcd_regulator_probe,
};

static int __init sc2703_lcd_regulator_driver_init(void)
{
	return platform_driver_register(&sc2703_lcd_regulator_driver);
}

subsys_initcall_sync(sc2703_lcd_regulator_driver_init);

MODULE_DESCRIPTION("LCD regulator driver for SC2703");
MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_LICENSE("GPL v2");
