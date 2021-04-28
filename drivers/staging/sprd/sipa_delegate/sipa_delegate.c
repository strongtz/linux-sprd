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

#define pr_fmt(fmt) "sipa_dele: %s " fmt, __func__

#include <linux/sipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include "../sipa/sipa_hal_priv.h"
#include "sipa_delegate.h"
#include "sipa_dele_priv.h"

#define DRV_NAME "sipa_delegate"

static struct sipa_delegate_plat_drv_cfg s_sipa_dele_cfg;

static int sipa_dele_parse_dts_cfg(
	struct platform_device *pdev,
	struct sipa_delegate_plat_drv_cfg *cfg)
{
	int ret;
	struct resource *resource;

	/* get modem IPA global register base  address */
	resource = platform_get_resource_byname(pdev,
						IORESOURCE_MEM,
						"mem-base");
	if (!resource) {
		dev_err(&pdev->dev, "get resource failed for remote-base!\n");
		return -ENODEV;
	}
	cfg->mem_base = resource->start;
	cfg->mem_end = resource->end;

	/* get mapped modem IPA global register base  address */
	resource = platform_get_resource_byname(pdev,
						IORESOURCE_MEM,
						"reg-base");
	if (!resource) {
		dev_err(&pdev->dev, "get resource failed for mapped-base!\n");
		return -ENODEV;
	}
	cfg->reg_base = resource->start;
	cfg->reg_end = resource->end;

	/* get ul fifo depth */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,ul-fifo-depth",
				   &cfg->ul_fifo_depth);
	if (ret) {
		dev_err(&pdev->dev, "get resource failed for ul_fifo_depth\n");
		return ret;
	} else
		pr_debug("%s : using ul_fifo_depth = %d", __func__,
			 cfg->ul_fifo_depth);

	/* get dl fifo depth */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,dl-fifo-depth",
				   &cfg->dl_fifo_depth);
	if (ret) {
		dev_err(&pdev->dev, "get resource failed for dl_fifo_depth\n");
		return ret;
	} else
		dev_err(&pdev->dev, "using dl_fifo_depth = %d", cfg->dl_fifo_depth);

	return 0;
}

static int sipa_dele_plat_drv_probe(struct platform_device *pdev_p)
{
	int ret;
	struct device *dev = &pdev_p->dev;
	struct sipa_delegate_plat_drv_cfg *cfg = &s_sipa_dele_cfg;
	struct sipa_delegator_create_params create_params;

	if (!sipa_rm_is_initialized())
		return -EPROBE_DEFER;

	memset(cfg, 0, sizeof(*cfg));

	ret = sipa_dele_parse_dts_cfg(pdev_p, cfg);
	if (ret) {
		dev_err(dev, "dts parsing failed\n");
		return ret;
	}

	create_params.pdev = dev;
	create_params.cfg = cfg;
	create_params.chan = SMSG_CH_COMM_SIPA;

#ifdef CONFIG_SPRD_SIPA_DELE_MINIAP_IN_AP
	create_params.prod_id = SIPA_RM_RES_PROD_MINI_AP;
	create_params.cons_prod = SIPA_RM_RES_CONS_WWAN_UL;
	create_params.cons_user = SIPA_RM_RES_CONS_WWAN_DL;
	create_params.dst = SIPC_ID_MINIAP;

	ret = miniap_delegator_init(&create_params);
	if (ret) {
		dev_err(dev, "miniap_delegator_init failed: %d\n", ret);
		return ret;
	}
	pr_debug("miniap_delegator_init!\n");
#endif /* CONFIG_SPRD_SIPA_DELE_MINIAP_IN_AP */

#ifdef CONFIG_SPRD_SIPA_DELE_AP_IN_MINIAP
	create_params.prod_id = SIPA_RM_RES_PROD_AP;
	create_params.cons_prod = SIPA_RM_RES_CONS_WWAN_DL;
	create_params.cons_user = SIPA_RM_RES_CONS_WWAN_UL;
	create_params.dst = SIPC_ID_AP;

	ret = ap_delegator_init(&create_params);
	if (ret) {
		dev_err(dev, "ap_delegator_init failed: %d\n", ret);
		return ret;
	}
	pr_debug("ap_delegator_init!\n");
#endif /* CONFIG_SPRD_SIPA_DELE_AP_IN_MINIAP */

#ifdef CONFIG_SPRD_SIPA_DELE_CP_IN_MINIAP
	create_params.prod_id = SIPA_RM_RES_PROD_CP;
	create_params.cons_prod = SIPA_RM_RES_CONS_WWAN_UL;
	create_params.cons_user = SIPA_RM_RES_CONS_WWAN_DL;
	create_params.dst = SIPC_ID_PSCP;

	ret = cp_delegator_init(&create_params);
	if (ret) {
		dev_err(dev, "cp_delegator_init failed: %d\n", ret);
		return ret;
	}
	pr_debug("cp_delegator_init!\n");
#endif /* CONFIG_SPRD_SIPA_DELE_AP_IN_MINIAP */

	return ret;
}

static struct of_device_id sipa_dele_plat_drv_match[] = {
	{ .compatible = "sprd,roc1-sipa-delegate", },
	{ .compatible = "sprd,orca-sipa-delegate", },
	{}
};

/**
 * sipa_dele_ap_suspend() - suspend callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP suspend
 * operation is invoked.
 *
 * Returns -EAGAIN to runtime_pm framework in case IPA is in use by AP.
 * This will postpone the suspend operation until IPA is no longer used by AP.
 */
static int sipa_dele_ap_suspend(struct device *dev)
{
	return 0;
}

/**
 * sipa_dele_ap_resume() - resume callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP resume
 * operation is invoked.
 *
 * Always returns 0 since resume should always succeed.
 */
static int sipa_dele_ap_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sipa_dele_pm_ops = {
	.suspend_noirq = sipa_dele_ap_suspend,
	.resume_noirq = sipa_dele_ap_resume,
};

static struct platform_driver sipa_dele_plat_drv = {
	.probe = sipa_dele_plat_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &sipa_dele_pm_ops,
		.of_match_table = sipa_dele_plat_drv_match,
	},
};

static int __init sipa_dele_module_init(void)
{
	/* Register as a platform device driver */
	return platform_driver_register(&sipa_dele_plat_drv);
}

module_init(sipa_dele_module_init);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum IPA Delegate device driver");
