/*
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/sipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/skbuff.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "sipa_phy_v0/sipa_sys_phy.h"
#include "sipa_sys.h"

#define DRV_NAME "sprd-sipa-sys"

struct sipa_sys_cfg_tag *sipa_sys_cfg;

static const struct of_device_id sipa_sys_drv_match[] = {
	{.compatible = "sprd,roc1-sipa-sys",},
	{}
};

static int sipa_sys_parse_dts_configuration(struct platform_device *pdev,
					    struct sipa_sys_cfg_tag *cfg)
{
	int ret;
	u32 reg_info[2];

	/*get pmu apb register information*/
	cfg->pmu_regmap = syscon_regmap_lookup_by_name(pdev->dev.of_node,
						       "ipa-sys-forcewakeup");
	if (IS_ERR(cfg->pmu_regmap))
		pr_err("%s :get pmu apb regmap fail!\n", __func__);

	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-forcewakeup", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get wakeup  register info fail!\n", __func__);
	} else {
		cfg->forcewakeup_reg = reg_info[0];
		cfg->forcewakeup_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-forceshutdown", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get forceshutdown register info fail!\n",
			__func__);
	} else {
		cfg->forceshutdown_reg = reg_info[0];
		cfg->forceshutdown_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-autoshutdown", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get autoshutdown register info fail!\n", __func__);
	} else {
		cfg->autoshutdown_reg = reg_info[0];
		cfg->autoshutdown_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-forcedslp", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get forcedslp register info fail!\n", __func__);
	} else {
		cfg->forcedslp_reg = reg_info[0];
		cfg->forcedslp_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-dslpen", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get dslpen  register info fail!\n", __func__);
	} else {
		cfg->dslpeb_reg = reg_info[0];
		cfg->dslpeb_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-forcelslp", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get forcelslp register info fail!\n", __func__);
	} else {
		cfg->forcelslp_reg = reg_info[0];
		cfg->forcelslp_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-lslpen", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get lslpen  register info fail!\n", __func__);
	} else {
		cfg->lslpeb_reg = reg_info[0];
		cfg->lslpeb_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-smartlslpen", 2, reg_info);
	if (ret < 0 || ret != 2) {
		pr_warn("%s :get smartlslpen register info fail!\n", __func__);
	} else {
		cfg->smartlslp_reg = reg_info[0];
		cfg->smartlslp_mask = reg_info[1];
	}

	/* get sys ahb  register information */
	cfg->sys_regmap = syscon_regmap_lookup_by_name(pdev->dev.of_node,
						       "ipa-sys-ipaeb");
	if (IS_ERR(cfg->sys_regmap))
		pr_err("%s :get sys ahb regmap fail!\n", __func__);

	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-ipaeb", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("%s :get ipaeb register info fail!\n", __func__);
	} else {
		cfg->ipaeb_reg = reg_info[0];
		cfg->ipaeb_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-cm4eb", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("%s :get  ipa cm4eb  register info fail!\n", __func__);
	} else {
		cfg->cm4eb_reg = reg_info[0];
		cfg->cm4eb_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-autogaten", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("%s :get autogat en register info fail!\n", __func__);
	} else {
		cfg->autogateb_reg = reg_info[0];
		cfg->autogateb_mask = reg_info[1];
	}
	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-s5-autogaten", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("%s :get s5 autogate  register info fail!\n", __func__);
	} else {
		cfg->s5autogateb_reg = reg_info[0];
		cfg->s5autogateb_mask = reg_info[1];
	}

	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-pciepllhsel", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("get pciepll_p_sel register info fail!\n");
	} else {
		cfg->pciepllhsel_reg = reg_info[0];
		cfg->pciepllhsel_mask = reg_info[1];
	}

	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-pciepllvsel", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("get s5 pciepll_v_sel register info fail!\n");
	} else {
		cfg->pciepllvsel_reg = reg_info[0];
		cfg->pciepllvsel_mask = reg_info[1];
	}

	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-xtlbufpciehsel", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("get s5 xtlbuf_pcieh_sel register info fail!\n");
	} else {
		cfg->xtlbufpciehsel_reg = reg_info[0];
		cfg->xtlbufpciehsel_mask = reg_info[1];
	}

	ret = syscon_get_args_by_name(pdev->dev.of_node,
				      "ipa-sys-xtlbufpcievsel", 2, reg_info);

	if (ret < 0 || ret != 2) {
		pr_warn("get s5 xtlbuf_pciev_sel register info fail!\n");
	} else {
		cfg->xtlbufpcievsel_reg = reg_info[0];
		cfg->xtlbufpcievsel_mask = reg_info[1];
	}

	return 0;
}

static int sipa_sys_drv_probe(struct platform_device *pdev_p)
{

	struct sipa_sys_cfg_tag *cfg;

	cfg = devm_kzalloc(&pdev_p->dev, sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	sipa_sys_cfg = cfg;

	sipa_sys_parse_dts_configuration(pdev_p, cfg);

	sipa_sys_proc_init(cfg);

	return 0;
}

static struct platform_driver sipa_sys_drv = {
	.probe = sipa_sys_drv_probe,
	.driver = {
		   .name = DRV_NAME,
		   .of_match_table = sipa_sys_drv_match,
	},
};

static int __init sipa_sys_module_init(void)
{
	pr_debug("sipa sys module init\n");

	/* Register as a platform device driver */
	return platform_driver_register(&sipa_sys_drv);
}

static void __exit sipa_sys_module_exit(void)
{
	pr_debug("sipa sys module exit\n");

	platform_driver_unregister(&sipa_sys_drv);
}

module_init(sipa_sys_module_init);
module_exit(sipa_sys_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum sipa sys hw device driver");
