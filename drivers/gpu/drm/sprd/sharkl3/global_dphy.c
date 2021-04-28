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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dphy.h"

struct glb_ctrl {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

static struct glb_ctrl enable;
static struct glb_ctrl power_s;
static struct glb_ctrl power_l;
static struct glb_ctrl power_iso;

static int dphy_glb_parse_dt(struct dphy_context *ctx,
				struct device_node *np)
{
	int ret;
	u32 args[2];

	enable.regmap = syscon_regmap_lookup_by_name(np, "enable");
	if (IS_ERR(enable.regmap))
		pr_warn("failed to get syscon-name: enable\n");

	ret = syscon_get_args_by_name(np, "enable", 2, args);
	if (ret == 2) {
		enable.reg = args[0];
		enable.mask = args[1];
	} else
		pr_warn("failed to get args for syscon-name enable\n");

	power_s.regmap = syscon_regmap_lookup_by_name(np, "power_small");
	if (IS_ERR(power_s.regmap))
		pr_warn("failed to get syscon-name: power_small\n");

	ret = syscon_get_args_by_name(np, "power_small", 2, args);
	if (ret == 2) {
		power_s.reg = args[0];
		power_s.mask = args[1];
	} else
		pr_warn("failed to get args for syscon-name power_small\n");

	power_l.regmap = syscon_regmap_lookup_by_name(np, "power_large");
	if (IS_ERR(power_l.regmap))
		pr_warn("failed to get syscon-name: power_large\n");

	ret = syscon_get_args_by_name(np, "power_large", 2, args);
	if (ret == 2) {
		power_l.reg = args[0];
		power_l.mask = args[1];
	} else
		pr_warn("failed to get args for syscon-name power_large\n");

	power_iso.regmap = syscon_regmap_lookup_by_name(np, "power_iso");
	if (IS_ERR(power_iso.regmap))
		pr_warn("failed to get syscon-name: power_iso\n");

	ret = syscon_get_args_by_name(np, "power_iso", 2, args);
	if (ret == 2) {
		power_iso.reg = args[0];
		power_iso.mask = args[1];
	} else
		pr_warn("failed to get args for syscon-name power_iso\n");

	regmap_read(enable.regmap, 0x00F8, &ctx->chip_id);

	return 0;
}

static void dphy_glb_enable(struct dphy_context *ctx)
{
	regmap_update_bits(enable.regmap, enable.reg,
			enable.mask, enable.mask);
}

static void dphy_glb_disable(struct dphy_context *ctx)
{
	regmap_update_bits(enable.regmap, enable.reg,
			enable.mask, (u32)~enable.mask);
}

static void dphy_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(power_s.regmap, power_s.reg,
				power_s.mask, (u32)~power_s.mask);
		udelay(10);
		regmap_update_bits(power_l.regmap, power_l.reg,
				power_l.mask, (u32)~power_l.mask);
		regmap_update_bits(power_iso.regmap, power_iso.reg,
				power_iso.mask, (u32)~power_iso.mask);
	} else {
		regmap_update_bits(power_iso.regmap, power_iso.reg,
				power_iso.mask, power_iso.mask);
		regmap_update_bits(power_s.regmap, power_s.reg,
				power_s.mask, power_s.mask);
		udelay(10);
		regmap_update_bits(power_l.regmap, power_l.reg,
				power_l.mask, power_l.mask);
	}
}

static struct dphy_glb_ops dphy_glb_ops = {
	.parse_dt = dphy_glb_parse_dt,
	.enable = dphy_glb_enable,
	.disable = dphy_glb_disable,
	.power = dphy_power_domain,
};

static struct ops_entry entry = {
	.ver = "sharkl3",
	.ops = &dphy_glb_ops,
};

static int __init dphy_glb_register(void)
{
	return dphy_glb_ops_register(&entry);
}

subsys_initcall(dphy_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("leon.he@spreadtrum.com");
MODULE_DESCRIPTION("sprd sharkl3 dphy global AHB regs low-level config");
