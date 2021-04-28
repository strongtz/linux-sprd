// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 * Copyright (C) 2018 Linaro Ltd.
 */

#include <linux/cpu.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/syscore_ops.h>

#define SC2720_PWR_PD_HW	0xc20
#define SC2720_SLP_CTRL		0xd68
#define SC2721_PWR_PD_HW	0xc20
#define SC2721_SLP_CTRL		0xd98
#define SC2730_PWR_PD_HW	0x1820
#define SC2730_SLP_CTRL		0x1a48
#define SC2731_PWR_PD_HW	0xc2c
#define SC2731_SLP_CTRL		0xdf0
#define SC2720_LDO_XTL_EN	BIT(2)
#define SC2721_LDO_XTL_EN	BIT(2)
#define SC2730_LDO_XTL_EN	BIT(2)
#define SC2731_LDO_XTL_EN	BIT(3)
#define SC27XX_PWR_OFF_EN	BIT(0)

struct sc27xx_poweroff_data {
	u32 sc27xx_poweroff_reg;
	u32 sc27xx_slp_ctrl_reg;
	u32 sc27xx_ldo_xtl_en;
};

static struct regmap *regmap;
const struct sc27xx_poweroff_data *pdata;
/*
 * On Spreadtrum platform, we need power off system through external SC27xx
 * series PMICs, and it is one similar SPI bus mapped by regmap to access PMIC,
 * which is not fast io access.
 *
 * So before stopping other cores, we need release other cores' resource by
 * taking cpus down to avoid racing regmap or spi mutex lock when poweroff
 * system through PMIC.
 */
void sc27xx_poweroff_shutdown(void)
{
#ifdef CONFIG_PM_SLEEP_SMP
	int cpu = smp_processor_id();

	freeze_secondary_cpus(cpu);
#endif
}

static struct syscore_ops poweroff_syscore_ops = {
	.shutdown = sc27xx_poweroff_shutdown,
};

static void sc27xx_poweroff_do_poweroff(void)
{
	regmap_write(regmap, pdata->sc27xx_slp_ctrl_reg, pdata->sc27xx_ldo_xtl_en);
	regmap_write(regmap, pdata->sc27xx_poweroff_reg, SC27XX_PWR_OFF_EN);
}

static const struct sc27xx_poweroff_data sc2720_data = {
	.sc27xx_poweroff_reg = SC2720_PWR_PD_HW,
	.sc27xx_slp_ctrl_reg = SC2720_SLP_CTRL,
	.sc27xx_ldo_xtl_en = SC2720_LDO_XTL_EN,
};

static const struct sc27xx_poweroff_data sc2721_data = {
	.sc27xx_poweroff_reg = SC2721_PWR_PD_HW,
	.sc27xx_slp_ctrl_reg = SC2721_SLP_CTRL,
	.sc27xx_ldo_xtl_en = SC2721_LDO_XTL_EN,
};

static const struct sc27xx_poweroff_data sc2730_data = {
	.sc27xx_poweroff_reg = SC2730_PWR_PD_HW,
	.sc27xx_slp_ctrl_reg = SC2730_SLP_CTRL,
	.sc27xx_ldo_xtl_en = SC2730_LDO_XTL_EN,
};

static const struct sc27xx_poweroff_data sc2731_data = {
	.sc27xx_poweroff_reg = SC2731_PWR_PD_HW,
	.sc27xx_slp_ctrl_reg = SC2731_SLP_CTRL,
	.sc27xx_ldo_xtl_en = SC2731_LDO_XTL_EN,
};

static int sc27xx_poweroff_probe(struct platform_device *pdev)
{
	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	if (regmap)
		return -EINVAL;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap)
		return -ENODEV;

	pm_power_off = sc27xx_poweroff_do_poweroff;
	register_syscore_ops(&poweroff_syscore_ops);
	return 0;
}

static const struct of_device_id sc27xx_poweroff_of_match[] = {
	{ .compatible = "sprd,sc2720-poweroff", .data = &sc2720_data},
	{ .compatible = "sprd,sc2721-poweroff", .data = &sc2721_data},
	{ .compatible = "sprd,sc2730-poweroff", .data = &sc2730_data},
	{ .compatible = "sprd,sc2731-poweroff", .data = &sc2731_data},
	{ }
};

static struct platform_driver sc27xx_poweroff_driver = {
	.probe = sc27xx_poweroff_probe,
	.driver = {
		.name = "sc27xx-poweroff",
		.of_match_table = sc27xx_poweroff_of_match,
	},
};
builtin_platform_driver(sc27xx_poweroff_driver);
