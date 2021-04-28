/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dphy.h"

struct pike2_dphy_glb_context {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	struct regmap *regmap;
};

struct pike2_dphy_glb_context enable;
struct pike2_dphy_glb_context power_s;
struct pike2_dphy_glb_context power_l;
struct pike2_dphy_glb_context power_shutdown;
struct pike2_dphy_glb_context power_iso;

static int dphy_glb_parse_dt(struct dphy_context *ctx,
			 struct device_node *np)
{
	unsigned int syscon_args[2];
	int ret;

	enable.regmap = syscon_regmap_lookup_by_name(np, "enable");
	if (IS_ERR(enable.regmap)) {
		pr_warn("failed to map dphy glb reg: enable\n");
		return PTR_ERR(enable.regmap);
	}

	ret = syscon_get_args_by_name(np, "enable", 2, syscon_args);
	if (ret == 2) {
		enable.ctrl_reg = syscon_args[0];
		enable.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: enable\n");

	power_s.regmap = syscon_regmap_lookup_by_name(np, "power_small");
	if (IS_ERR(power_s.regmap))
		pr_warn("failed to get syscon-name: power_small\n");

	ret = syscon_get_args_by_name(np, "power_small", 2, syscon_args);

	if (ret == 2) {
		power_s.ctrl_reg = syscon_args[0];
		power_s.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: power_small");

	power_l.regmap = syscon_regmap_lookup_by_name(np, "power_large");
	if (IS_ERR(power_l.regmap))
		pr_warn("failed to get syscon-name: power_large\n");

	ret = syscon_get_args_by_name(np, "power_large", 2, syscon_args);
	if (ret == 2) {
		power_l.ctrl_reg = syscon_args[0];
		power_l.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: power_large");

	power_shutdown.regmap = syscon_regmap_lookup_by_name(np,
			"power_shutdown");
	if (IS_ERR(power_shutdown.regmap))
		pr_warn("failed to get syscon-name: power_shutdown\n");

	ret = syscon_get_args_by_name(np, "power_shutdown", 2, syscon_args);
	if (ret == 2) {
		power_shutdown.ctrl_reg = syscon_args[0];
		power_shutdown.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: power_shutdown");

	power_iso.regmap = syscon_regmap_lookup_by_name(np, "power_iso");
	if (IS_ERR(power_iso.regmap))
		pr_warn("failed to get syscon-name: power_iso\n");

	ret = syscon_get_args_by_name(np, "power_iso", 2, syscon_args);
	if (ret == 2) {
		power_iso.ctrl_reg = syscon_args[0];
		power_iso.ctrl_mask = syscon_args[1];
	} else
		pr_warn("failed to parse dphy glb reg: power_iso");

	return 0;
}

static void dphy_glb_enable(struct dphy_context *ctx)
{
	regmap_update_bits(enable.regmap,
		enable.ctrl_reg,
		enable.ctrl_mask,
		enable.ctrl_mask);
}

static void dphy_glb_disable(struct dphy_context *ctx)
{
	regmap_update_bits(enable.regmap,
		enable.ctrl_reg,
		enable.ctrl_mask,
		(unsigned int)(~enable.ctrl_mask));
}

static void dphy_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(power_s.regmap,
			power_s.ctrl_reg,
			power_s.ctrl_mask,
			(unsigned int)(~power_s.ctrl_mask));

		/* Dphy has a random wakeup failed after poweron,
		 * this will caused testclr reset failed and
		 * writing pll configuration parameter failed.
		 * Delay 100us after dphy poweron, waiting for pll is stable.
		 */
		udelay(100);

		regmap_update_bits(power_l.regmap,
			power_l.ctrl_reg,
			power_l.ctrl_mask,
			(unsigned int)(~power_l.ctrl_mask));
		regmap_update_bits(power_shutdown.regmap,
			power_shutdown.ctrl_reg,
			power_shutdown.ctrl_mask,
			power_shutdown.ctrl_mask);
		regmap_update_bits(power_iso.regmap,
			power_iso.ctrl_reg,
			power_iso.ctrl_mask,
			(unsigned int)(~power_iso.ctrl_mask));
	} else {
		regmap_update_bits(power_iso.regmap,
			power_iso.ctrl_reg,
			power_iso.ctrl_mask,
			power_iso.ctrl_mask);
		regmap_update_bits(power_shutdown.regmap,
			power_shutdown.ctrl_reg,
			power_shutdown.ctrl_mask,
			(unsigned int)(~power_shutdown.ctrl_mask));
		regmap_update_bits(power_s.regmap,
			power_s.ctrl_reg,
			power_s.ctrl_mask,
			power_s.ctrl_mask);

		/* Dphy has a random wakeup failed after poweron,
		 * this will caused testclr reset failed and
		 * writing pll configuration parameter failed.
		 * Delay 100us after dphy poweron, waiting for pll is stable.
		 */
		udelay(100);

		regmap_update_bits(power_l.regmap,
			power_l.ctrl_reg,
			power_l.ctrl_mask,
			power_l.ctrl_mask);
	}
}

static struct dphy_glb_ops dphy_glb_ops = {
	.parse_dt = dphy_glb_parse_dt,
	.enable = dphy_glb_enable,
	.disable = dphy_glb_disable,
	.power = dphy_power_domain,
};

static struct ops_entry entry = {
	.ver = "pike2",
	.ops = &dphy_glb_ops,
};

static int __init dphy_glb_register(void)
{
	return dphy_glb_ops_register(&entry);
}

subsys_initcall(dphy_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("albert.zhang@unisoc.com");
MODULE_DESCRIPTION("sprd pike2 dphy global AHB&APB regs low-level config");
