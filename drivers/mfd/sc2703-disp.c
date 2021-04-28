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

#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sc2703/sc2703-disp.h>
#include <linux/module.h>
#include <linux/regmap.h>

static bool cali_mode;

static const struct mfd_cell sc2703_disp_devs[] = {
	{
		.name = "sc2703-lcd-regulator",
		.of_compatible = "sprd,sc2703-lcd-regulator",
	}, {
		.name = "sc2703-backlight",
		.of_compatible = "sprd,sc2703-backlight",
	},
};

static int boot_mode_check(char *str)
{
	if (str != NULL && !strncmp(str, "cali", strlen("cali")))
		cali_mode = true;
	else
		cali_mode = false;
	return 0;
}
__setup("androidboot.mode=", boot_mode_check);

static bool sc2703_disp_volatile_reg(struct device *dev,
			unsigned int reg)
{
	switch (reg) {
	case SC2703_SYSCTRL_EVENT:
	case SC2703_SYSCTRL_STATUS:
	case SC2703_SYSCTRL_DISPLAY_ACTIVE:
	case SC2703_SYSCTRL_DISPLAY_STATUS:
	case SC2703_DISPLAY_EVENT_A:
	case SC2703_DISPLAY_STATUS_A:
	case SC2703_WLED_EVENT:
	case SC2703_WLED_STATUS:
	case SC2703_WLED_CONFIG1:
	case SC2703_WLED_CONFIG3:
	case SC2703_WLED_CONFIG6:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config sc2703_disp_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
	.volatile_reg = sc2703_disp_volatile_reg,
};

static int sc2703_disp_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct regmap *regmap;
	int ret;

	if (cali_mode) {
		dev_info(&client->dev,
			"Calibration Mode! Don't register sc2703 display");
		return 0;
	}

	regmap = devm_regmap_init_i2c(client, &sc2703_disp_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&client->dev, "allocate regmap: %d failed\n", ret);
		return ret;
	}

	ret = mfd_add_devices(&client->dev, PLATFORM_DEVID_AUTO,
			      sc2703_disp_devs, ARRAY_SIZE(sc2703_disp_devs),
			      NULL, 0, NULL);
	if (ret) {
		dev_err(&client->dev, "failed to register devices %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id sc2703_disp_of_match[] = {
	{ .compatible = "sprd,sc2703-disp", },
	{ }
};
MODULE_DEVICE_TABLE(of, sc2703_disp_of_match);

static const struct i2c_device_id sc2703_disp_i2c_id[] = {
	{ "sc2703-disp", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2703_disp_i2c_id);

static struct i2c_driver sc2703_disp_driver = {
	.driver = {
		.name = "sc2703-disp",
		.of_match_table = sc2703_disp_of_match,
	},
	.probe		= sc2703_disp_probe,
	.id_table	= sc2703_disp_i2c_id,
};

static int __init sc2703_disp_i2c_init(void)
{
	return i2c_add_driver(&sc2703_disp_driver);
}
subsys_initcall(sc2703_disp_i2c_init);

static void __exit sc2703_disp_i2c_exit(void)
{
	i2c_del_driver(&sc2703_disp_driver);
}
module_exit(sc2703_disp_i2c_exit);

MODULE_DESCRIPTION("multi-function driver for sc2703 display");
MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_LICENSE("GPL v2");
