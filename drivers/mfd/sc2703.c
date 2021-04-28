/*
 * SPDX-License-Identifier: GPL-2.0
 * Core MFD(Charger, ADC, Flash and GPIO) driver for SC2703
 *
 * Copyright (c) 2018 Dialog Semiconductor.
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sc2703_regs.h>

struct sc2703_pdata {
	int irq_base;
};

struct sc2703_data {
	struct device *dev;
	struct regmap *regmap;
	int irq_base;
};

enum sc2703_irq_defs {
	SC2703_IRQ_ADC_DONE = 0,
	SC2703_IRQ_GPI0,
	SC2703_IRQ_GPI1,
	SC2703_IRQ_GPI2,
	SC2703_IRQ_GPI3,
};

static bool sc2703_volatile_reg(struct device *dev, u32 reg)
{
	switch (reg) {
	case SC2703_STATUS_A:
	case SC2703_STATUS_B:
	case SC2703_STATUS_C:
	case SC2703_STATUS_D:
	case SC2703_STATUS_E:
	case SC2703_STATUS_F:
	case SC2703_STATUS_G:
	case SC2703_STATUS_H:
	case SC2703_STATUS_I:
	case SC2703_EVENT_A:
	case SC2703_EVENT_B:
	case SC2703_EVENT_C:
	case SC2703_EVENT_D:
	case SC2703_EVENT_E:
	case SC2703_EVENT_F:
	case SC2703_DCDC_CTRL_A:
	case SC2703_ADC_CTRL_A:
	case SC2703_ADC_RES_0:
	case SC2703_ADC_RES_1:
	case SC2703_ADC_RES_2:
	case SC2703_ADC_RES_3:
	case SC2703_ADC_RES_4:
	case SC2703_ADC_RES_5:
	case SC2703_CHG_TIMER_CTRL_B:
	case SC2703_CHG_TIMER_CTRL_C:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config sc2703_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SC2703_MAX_REG,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = sc2703_volatile_reg,
};

/* Helper macro to automatically populate resource name */
#define SC2703_RES_IRQ_NAMED(_name)	\
	DEFINE_RES_IRQ_NAMED(SC2703_IRQ_##_name, #_name)

/* ADC IRQs */
static struct resource sc2703_adc_resources[] = {
	SC2703_RES_IRQ_NAMED(ADC_DONE),
};


static struct resource sc2703_gpio_resources[] = {
	SC2703_RES_IRQ_NAMED(GPI0),
	SC2703_RES_IRQ_NAMED(GPI1),
	SC2703_RES_IRQ_NAMED(GPI2),
	SC2703_RES_IRQ_NAMED(GPI3),
};

enum sc2703_dev_idx {
	SC2703_ADC_IDX = 0,
	SC2703_CHARGER_IDX,
	SC2703_GPIO_IDX,
};

static struct mfd_cell sc2703_devs[] = {
	[SC2703_ADC_IDX] = {
		.name = "sc2703-adc",
		.of_compatible = "sprd,sc2703-adc",
		.resources = sc2703_adc_resources,
		.num_resources = ARRAY_SIZE(sc2703_adc_resources),
	},

	[SC2703_CHARGER_IDX] = {
		.name = "sc2703-charger",
		.of_compatible = "sprd,sc2703-charger",
	},

	[SC2703_GPIO_IDX] = {
		.name = "sc2703-gpio",
		.of_compatible = "sprd,sc2703-gpio",
		.resources = sc2703_gpio_resources,
		.num_resources = ARRAY_SIZE(sc2703_gpio_resources),
	},
};

static int sc2703_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc2703_data *sc2703;
	struct sc2703_pdata *pdata = dev_get_platdata(&client->dev);
	struct regmap *regmap;
	u32 event_b, event_d;
	int ret;

	sc2703 = devm_kzalloc(dev, sizeof(*sc2703), GFP_KERNEL);
	if (!sc2703)
		return -ENOMEM;

	sc2703->dev = dev;
	i2c_set_clientdata(client, sc2703);

	if (pdata)
		sc2703->irq_base = pdata->irq_base;

	regmap = devm_regmap_init_i2c(client, &sc2703_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}
	sc2703->regmap = regmap;

	/* Report and clear POR related fault events before enabling IRQs */
	ret = regmap_read(regmap, SC2703_EVENT_B, &event_b);
	if (ret)
		return ret;

	if (event_b & SC2703_E_TJUNC_POR_MASK) {
		dev_info(dev, "Reset due to TJUNC POR event\n");
		ret = regmap_update_bits(regmap, SC2703_EVENT_B,
					 SC2703_E_TJUNC_POR_MASK,
					 SC2703_E_TJUNC_POR_MASK);
		if (ret)
			return ret;
	}

	ret = regmap_read(regmap, SC2703_EVENT_D, &event_d);
	if (ret)
		return ret;

	if (event_d & SC2703_E_VSYS_POR_MASK) {
		dev_info(dev, "Reset due to VSYS POR event\n");
		ret = regmap_update_bits(regmap, SC2703_EVENT_D,
					 SC2703_E_VSYS_POR_MASK,
					 SC2703_E_VSYS_POR_MASK);
		if (ret)
			return ret;
	}

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_NONE, sc2703_devs,
			      ARRAY_SIZE(sc2703_devs), NULL,
			      sc2703->irq_base, NULL);
	if (ret) {
		dev_err(dev, "Failed to add child devices: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id sc2703_of_match[] = {
	{ .compatible = "sprd,sc2703", },
	{ }
};

MODULE_DEVICE_TABLE(of, sc2703_of_match);

static const struct i2c_device_id sc2703_i2c_id[] = {
	{ "sc2703", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2703_i2c_id);

static struct i2c_driver sc2703_driver = {
	.driver	= {
		.name	= "sc2703",
		.of_match_table = sc2703_of_match,
	},
	.probe		= sc2703_probe,
	.id_table	= sc2703_i2c_id,
};

static int __init sc2703_init(void)
{
	return i2c_add_driver(&sc2703_driver);
}
module_init(sc2703_init);

static void __exit sc2703_exit(void)
{
	i2c_del_driver(&sc2703_driver);
}
module_exit(sc2703_exit);

MODULE_DESCRIPTION("MFD Core Driver for SC2703");
MODULE_AUTHOR("Roy Im <Roy.Im.Opensource@diasemi.com>");
MODULE_LICENSE("GPL v2");
