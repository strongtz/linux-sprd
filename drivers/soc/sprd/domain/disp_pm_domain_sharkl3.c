/*
 * Spreadtrum Generic power domain support.
 *
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

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(__fmt) "[disp-pm-domain][drm][%20s] "__fmt, __func__
#endif

struct disp_pm_domain {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;

	struct generic_pm_domain pd;
	struct regmap *regmap;
};

static int disp_power_on(struct generic_pm_domain *domain)
{
	struct disp_pm_domain *pd;

	pd = container_of(domain, struct disp_pm_domain, pd);

	regmap_update_bits(pd->regmap,
		    pd->ctrl_reg,
		    pd->ctrl_mask,
		    (unsigned int)~pd->ctrl_mask);

	mdelay(10);

	pr_info("disp power domain on\n");
	return 0;
}

static int disp_power_off(struct generic_pm_domain *domain)
{
	struct disp_pm_domain *pd;

	pd = container_of(domain, struct disp_pm_domain, pd);

	mdelay(10);

	regmap_update_bits(pd->regmap,
		    pd->ctrl_reg,
		    pd->ctrl_mask,
		    pd->ctrl_mask);

	pr_info("disp power domain off\n");
	return 0;
}

static __init int disp_pm_domain_init(void)
{
	struct disp_pm_domain *pd;
	struct device_node *np;
	unsigned int syscon_args[2];
	int ret;

	np = of_find_compatible_node(NULL, NULL, "sprd,sharkl3-disp-domain");
	if  (!np) {
		pr_err("Error: sprd,sharkl3-disp-domain not found\n");
		return -ENODEV;
	}

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd) {
		of_node_put(np);
		return -ENOMEM;
	}

	pd->pd.name = kstrdup(np->name, GFP_KERNEL);
	pd->pd.power_off = disp_power_off;
	pd->pd.power_on = disp_power_on;

	pd->regmap = syscon_regmap_lookup_by_name(np, "power");
	if (IS_ERR(pd->regmap)) {
		pr_err("failed to map glb reg\n");
		goto err;
	}

	ret = syscon_get_args_by_name(np, "power", 2, syscon_args);
	if (ret == 2) {
		pd->ctrl_reg = syscon_args[0];
		pd->ctrl_mask = syscon_args[1];
	} else {
		pr_err("failed to parse glb reg\n");
		goto err;
	}

	pm_genpd_init(&pd->pd, NULL, true);
	of_genpd_add_provider_simple(np, &pd->pd);

	pr_info("display power domain init ok!\n");

	return 0;
err:
	kfree(pd);
	return -EINVAL;
}

core_initcall(disp_pm_domain_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("leon.he@spreadtrum.com");
MODULE_DESCRIPTION("sprd sharkl3 display pm generic domain");
