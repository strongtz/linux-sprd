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

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "sprd_dphy.h"
#include "sysfs/sysfs_display.h"

LIST_HEAD(dphy_pll_head);
LIST_HEAD(dphy_ppi_head);
LIST_HEAD(dphy_glb_head);

static int regmap_tst_io_write(void *context, u32 reg, u32 val)
{
	struct sprd_dphy *dphy = context;

	if (val > 0xff || reg > 0xff)
		return -EINVAL;

	pr_debug("reg = 0x%02x, val = 0x%02x\n", reg, val);

	sprd_dphy_test_write(dphy, reg, val);

	return 0;
}

static int regmap_tst_io_read(void *context, u32 reg, u32 *val)
{
	struct sprd_dphy *dphy = context;
	int ret;

	if (reg > 0xff)
		return -EINVAL;

	ret = sprd_dphy_test_read(dphy, reg);
	if (ret < 0)
		return ret;

	*val = ret;

	pr_debug("reg = 0x%02x, val = 0x%02x\n", reg, *val);
	return 0;
}

static struct regmap_bus regmap_tst_io = {
	.reg_write = regmap_tst_io_write,
	.reg_read = regmap_tst_io_read,
};

static const struct regmap_config byte_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct regmap_config word_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int sprd_dphy_regmap_init(struct sprd_dphy *dphy)
{
	struct dphy_context *ctx = &dphy->ctx;
	struct regmap *regmap;

	if (ctx->apbbase)
		regmap = devm_regmap_init_mmio(&dphy->dev,
			(void __iomem *)ctx->apbbase, &word_config);
	else
		regmap = devm_regmap_init(&dphy->dev, &regmap_tst_io,
					  dphy, &byte_config);

	if (IS_ERR(regmap)) {
		DRM_ERROR("dphy regmap init failed\n");
		return PTR_ERR(regmap);
	}

	ctx->regmap = regmap;

	return 0;
}

void sprd_dphy_ulps_enter(struct sprd_dphy *dphy)
{
	DRM_INFO("dphy ulps enter\n");
	sprd_dphy_hs_clk_en(dphy, false);
	sprd_dphy_data_ulps_enter(dphy);
	sprd_dphy_clk_ulps_enter(dphy);
}

void sprd_dphy_ulps_exit(struct sprd_dphy *dphy)
{
	DRM_INFO("dphy ulps exit\n");
	sprd_dphy_force_pll(dphy, true);
	sprd_dphy_clk_ulps_exit(dphy);
	sprd_dphy_data_ulps_exit(dphy);
	sprd_dphy_force_pll(dphy, false);
}

int sprd_dphy_resume(struct sprd_dphy *dphy)
{
	int ret;

	mutex_lock(&dphy->ctx.lock);
	if (dphy->glb && dphy->glb->power)
		dphy->glb->power(&dphy->ctx, true);
	if (dphy->glb && dphy->glb->enable)
		dphy->glb->enable(&dphy->ctx);

	ret = sprd_dphy_configure(dphy);
	if (ret) {
		mutex_unlock(&dphy->ctx.lock);
		DRM_ERROR("sprd dphy init failed\n");
		return -EINVAL;
	}

	dphy->ctx.is_enabled = true;
	mutex_unlock(&dphy->ctx.lock);

	DRM_INFO("dphy resume OK\n");
	return ret;
}

int sprd_dphy_suspend(struct sprd_dphy *dphy)
{
	int ret;

	mutex_lock(&dphy->ctx.lock);
	ret = sprd_dphy_close(dphy);
	if (ret)
		DRM_ERROR("sprd dphy close failed\n");

	if (dphy->glb && dphy->glb->disable)
		dphy->glb->disable(&dphy->ctx);
	if (dphy->glb && dphy->glb->power)
		dphy->glb->power(&dphy->ctx, false);

	dphy->ctx.is_enabled = false;
	mutex_unlock(&dphy->ctx.lock);

	DRM_INFO("dphy suspend OK\n");
	return ret;
}

static int sprd_dphy_device_create(struct sprd_dphy *dphy,
				   struct device *parent)
{
	int ret;

	dphy->dev.class = display_class;
	dphy->dev.parent = parent;
	dphy->dev.of_node = parent->of_node;
	dev_set_name(&dphy->dev, "dphy%d", dphy->ctx.id);
	dev_set_drvdata(&dphy->dev, dphy);

	ret = device_register(&dphy->dev);
	if (ret)
		DRM_ERROR("dphy device register failed\n");

	return ret;
}

static int sprd_dphy_context_init(struct sprd_dphy *dphy,
				  struct device_node *np)
{
	struct resource r;
	u32 tmp;

	dphy->ctx.is_enabled = true;
	dphy->ctx.chip_id = 0xffffffff;

	if (dphy->glb && dphy->glb->parse_dt)
		dphy->glb->parse_dt(&dphy->ctx, np);

	if (!of_address_to_resource(np, 0, &r)) {
		dphy->ctx.ctrlbase = (unsigned long)
		    ioremap_nocache(r.start, resource_size(&r));
		if (dphy->ctx.ctrlbase == 0) {
			DRM_ERROR("dphy ctrlbase ioremap failed\n");
			return -EFAULT;
		}
	} else {
		DRM_ERROR("parse dphy ctrl reg base failed\n");
		return -EINVAL;
	}

	if (!of_address_to_resource(np, 1, &r)) {
		DRM_INFO("this dphy has apb reg base\n");
		dphy->ctx.apbbase = (unsigned long)
		    ioremap_nocache(r.start, resource_size(&r));
		if (dphy->ctx.apbbase == 0) {
			DRM_ERROR("dphy apbbase ioremap failed\n");
			return -EFAULT;
		}
	}

	if (!of_property_read_u32(np, "dev-id", &tmp))
		dphy->ctx.id = tmp;

	if (!of_property_read_u32(np, "sprd,mipi-drive-capability", &tmp))
		dphy->ctx.capability = tmp;

	if (of_property_read_bool(np, "sprd,ulps-disabled"))
		dphy->ctx.ulps_enable = false;
	else
		dphy->ctx.ulps_enable = true;

	mutex_init(&dphy->ctx.lock);

	return 0;
}

static int sprd_dphy_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_dphy *dphy;
	struct device *dsi_dev;
	const char *str;
	int ret;

	dphy = devm_kzalloc(&pdev->dev, sizeof(*dphy), GFP_KERNEL);
	if (!dphy)
		return -ENOMEM;

	dsi_dev = sprd_disp_pipe_get_input(&pdev->dev);
	if (!dsi_dev)
		return -ENODEV;

	if (!of_property_read_string(dsi_dev->of_node, "sprd,ip", &str))
		dphy->ppi = dphy_ppi_ops_attach(str);
	else
		DRM_WARN("dphy ppi ops parse failed\n");

	if (!of_property_read_string(np, "sprd,ip", &str))
		dphy->pll = dphy_pll_ops_attach(str);
	else
		DRM_WARN("dphy pll ops parse failed\n");

	if (!of_property_read_string(np, "sprd,soc", &str))
		dphy->glb = dphy_glb_ops_attach(str);
	else
		DRM_WARN("dphy glb ops parse failed\n");

	ret = sprd_dphy_context_init(dphy, pdev->dev.of_node);
	if (ret)
		return ret;

	sprd_dphy_device_create(dphy, &pdev->dev);
	sprd_dphy_sysfs_init(&dphy->dev);
	sprd_dphy_regmap_init(dphy);
	platform_set_drvdata(pdev, dphy);

	DRM_INFO("dphy driver probe success\n");

	return 0;
}

static const struct of_device_id dt_ids[] = {
	{ .compatible = "sprd,dsi-phy", },
	{},
};

static struct platform_driver sprd_dphy_driver = {
	.probe	= sprd_dphy_probe,
	.driver = {
		.name  = "sprd-dphy-drv",
		.of_match_table	= of_match_ptr(dt_ids),
	}
};

module_platform_driver(sprd_dphy_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("infi.chen <infi.chen@spreadtrum.com>");
MODULE_AUTHOR("Leon He <leon.he@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SoC MIPI DSI PHY driver");
