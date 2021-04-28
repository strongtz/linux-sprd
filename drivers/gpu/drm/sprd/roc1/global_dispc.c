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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dpu.h"

static LIST_HEAD(dpu_clk_list);
static LIST_HEAD(dpi_clk_list);

static struct clk *clk_dpu_core;
static struct clk *clk_dpu_dpi;
static struct clk *clk_ap_ahb_disp_eb;

static struct qos_thres {
	u8 awqos_thres;
	u8 arqos_thres;
} qos_cfg;

struct dpu_clk_context {
	struct list_head head;
	const char *name;
	struct clk *source;
	unsigned int rate;
};

/* Must be sorted in ascending order */
static char *dpu_clk_src[] = {
	"clk_src_153m6",
	"clk_src_192m",
	"clk_src_256m",
	"clk_src_307m2",
	"clk_src_384m",
};

/* Must be sorted in ascending order */
static char *dpi_clk_src[] = {
	"clk_src_128m",
	"clk_src_153m6",
	"clk_src_192m",
	"clk_src_307m2",
};

static struct dpu_glb_context {
	unsigned int enable_reg;
	unsigned int mask_bit;

	struct regmap *regmap;
} ctx_reset, ctx_qos;

static int dpu_clk_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct dpu_clk_context *clk_ctx = NULL;
	int index;

	for (index = 0; index < ARRAY_SIZE(dpu_clk_src); index++) {
		clk_ctx = kzalloc(sizeof(*clk_ctx), GFP_KERNEL);
		if (!clk_ctx)
			return -ENOMEM;

		clk_ctx->source =
			of_clk_get_by_name(np, dpu_clk_src[index]);

		if (IS_ERR(clk_ctx->source)) {
			pr_warn("read dpu %s failed\n", dpu_clk_src[index]);
			clk_ctx->source = NULL;
		} else {
			clk_ctx->name = dpu_clk_src[index];
			clk_ctx->rate = clk_get_rate(clk_ctx->source);
			pr_info("get dpu_clk_src \"%s\" value %d\n", clk_ctx->name, clk_ctx->rate);
		}

		list_add_tail(&clk_ctx->head, &dpu_clk_list);
	}

	for (index = 0; index < ARRAY_SIZE(dpi_clk_src); index++) {
		clk_ctx = kzalloc(sizeof(*clk_ctx), GFP_KERNEL);
		if (!clk_ctx)
			return -ENOMEM;

		clk_ctx->source =
			of_clk_get_by_name(np, dpi_clk_src[index]);

		if (IS_ERR(clk_ctx->source)) {
			pr_warn("read dpi %s failed\n", dpi_clk_src[index]);
			clk_ctx->source = NULL;
		} else {
			clk_ctx->name = dpi_clk_src[index];
			clk_ctx->rate = clk_get_rate(clk_ctx->source);
			pr_info("get dpi_clk_src \"%s\" value %d\n", clk_ctx->name, clk_ctx->rate);
		}

		list_add_tail(&clk_ctx->head, &dpi_clk_list);
	}


	clk_dpu_core =
		of_clk_get_by_name(np, "clk_dpu_core");
	clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");
	clk_ap_ahb_disp_eb =
		of_clk_get_by_name(np, "clk_ap_ahb_disp_eb");

	if (IS_ERR(clk_dpu_core)) {
		clk_dpu_core = NULL;
		pr_warn("read clk_dpu_core failed\n");
	}

	if (IS_ERR(clk_dpu_dpi)) {
		clk_dpu_dpi = NULL;
		pr_warn("read clk_dpu_dpi failed\n");
	}

	if (IS_ERR(clk_ap_ahb_disp_eb)) {
		clk_ap_ahb_disp_eb = NULL;
		pr_warn("read clk_ap_ahb_disp_eb failed\n");
	}

	return 0;
}

static int dpu_clk_init(struct dpu_context *ctx)
{
	struct dpu_clk_context *clk_ctx = NULL;
	u32 dpu_core_rate = 0;
	u32 dpi_src_rate = 0;
	int ret = 0;

	list_for_each_entry(clk_ctx, &dpi_clk_list, head) {
		pr_info("clk_ctx->rate , ctx->vm.pixelclock = (%d, %ld)\n",
			clk_ctx->rate, ctx->vm.pixelclock);
		if ((clk_ctx->rate % ctx->vm.pixelclock) == 0) {
			dpi_src_rate = clk_ctx->rate;
			ret = clk_set_parent(clk_dpu_dpi, clk_ctx->source);
			if (ret)
				pr_warn("set dpi clk source failed\n");
			break;
		}
	}

	list_for_each_entry(clk_ctx, &dpu_clk_list, head) {
		/* Dpu clk must be greater than dpi clk */
		if (dpi_src_rate * 2 <= clk_ctx->rate) {
			dpu_core_rate = clk_ctx->rate;
			ret = clk_set_parent(clk_dpu_core, clk_ctx->source);
			if (ret)
				pr_warn("set dpu core clk source failed\n");
			break;
		}
	}

	pr_info("DPU_CORE_CLK = %u, DPI_CLK_SRC = %u\n",
		dpu_core_rate, dpi_src_rate);
	pr_info("dpi clock is %lu\n", ctx->vm.pixelclock);

	return ret;
}

static int dpu_clk_enable(struct dpu_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_dpu_core);
	if (ret) {
		pr_err("enable clk_dpu_core error\n");
		return ret;
	}

	ret = clk_prepare_enable(clk_dpu_dpi);
	if (ret) {
		pr_err("enable clk_dpu_dpi error\n");
		clk_disable_unprepare(clk_dpu_core);
		return ret;
	}

	return 0;
}

static int dpu_clk_disable(struct dpu_context *ctx)
{
	struct dpu_clk_context *clk_ctx = NULL;

	clk_disable_unprepare(clk_dpu_dpi);
	clk_disable_unprepare(clk_dpu_core);

	list_for_each_entry(clk_ctx, &dpi_clk_list, head) {
		if (clk_ctx->name && !strcmp(clk_ctx->name, "clk_src_128m"))
			clk_set_parent(clk_dpu_dpi, clk_ctx->source);
	}

	list_for_each_entry(clk_ctx, &dpu_clk_list, head) {
		if (clk_ctx->name && !strcmp(clk_ctx->name, "clk_src_153m6"))
			clk_set_parent(clk_dpu_core, clk_ctx->source);
	}

	return 0;
}

static int dpu_glb_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	unsigned int syscon_args[2];
	struct device_node *qos_np = NULL;
	int ret;

	ctx_reset.regmap = syscon_regmap_lookup_by_name(np, "reset");
	if (IS_ERR(ctx_reset.regmap)) {
		pr_warn("failed to map dpu glb reg: reset\n");
		return PTR_ERR(ctx_reset.regmap);
	}

	ret = syscon_get_args_by_name(np, "reset", 2, syscon_args);
	if (ret == 2) {
		ctx_reset.enable_reg = syscon_args[0];
		ctx_reset.mask_bit = syscon_args[1];
	} else {
		pr_warn("failed to parse dpu glb reg: reset\n");
	}

	ctx_qos.regmap = syscon_regmap_lookup_by_name(np, "qos");
	if (IS_ERR(ctx_qos.regmap)) {
		pr_warn("failed to map dpu glb reg: qos\n");
		return PTR_ERR(ctx_qos.regmap);
	}

	ret = syscon_get_args_by_name(np, "qos", 2, syscon_args);
	if (ret == 2) {
		ctx_qos.enable_reg = syscon_args[0];
		ctx_qos.mask_bit = syscon_args[1];
	} else {
		pr_warn("failed to parse dpu glb reg: qos\n");
	}

	qos_np = of_parse_phandle(np, "sprd,qos", 0);
	if (!qos_np)
		pr_warn("can't find dpu qos cfg node\n");

	ret = of_property_read_u8(qos_np, "awqos-threshold",
					&qos_cfg.awqos_thres);
	if (ret)
		pr_warn("read awqos-threshold failed, use default\n");

	ret = of_property_read_u8(qos_np, "arqos-threshold",
					&qos_cfg.arqos_thres);
	if (ret)
		pr_warn("read arqos-threshold failed, use default\n");

	return 0;
}

static void dpu_glb_enable(struct dpu_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_ap_ahb_disp_eb);
	if (ret) {
		pr_err("enable clk_aon_apb_disp_eb failed!\n");
		return;
	}
}

static void dpu_glb_disable(struct dpu_context *ctx)
{
	clk_disable_unprepare(clk_ap_ahb_disp_eb);
}

static void dpu_reset(struct dpu_context *ctx)
{
	regmap_update_bits(ctx_reset.regmap,
		    ctx_reset.enable_reg,
		    ctx_reset.mask_bit,
		    ctx_reset.mask_bit);
	udelay(10);
	regmap_update_bits(ctx_reset.regmap,
		    ctx_reset.enable_reg,
		    ctx_reset.mask_bit,
		    (unsigned int)(~ctx_reset.mask_bit));
}

static void dpu_power_domain(struct dpu_context *ctx, int enable)
{
	if (enable)
		regmap_update_bits(ctx_qos.regmap,
			    ctx_qos.enable_reg,
			    ctx_qos.mask_bit,
			    qos_cfg.awqos_thres |
			    qos_cfg.arqos_thres << 4);
}

static struct dpu_clk_ops dpu_clk_ops = {
	.parse_dt = dpu_clk_parse_dt,
	.init = dpu_clk_init,
	.enable = dpu_clk_enable,
	.disable = dpu_clk_disable,
};

static struct dpu_glb_ops dpu_glb_ops = {
	.parse_dt = dpu_glb_parse_dt,
	.reset = dpu_reset,
	.enable = dpu_glb_enable,
	.disable = dpu_glb_disable,
	.power = dpu_power_domain,
};

static struct ops_entry clk_entry = {
	.ver = "roc1",
	.ops = &dpu_clk_ops,
};

static struct ops_entry glb_entry = {
	.ver = "roc1",
	.ops = &dpu_glb_ops,
};

static int __init dpu_glb_register(void)
{
	dpu_clk_ops_register(&clk_entry);
	dpu_glb_ops_register(&glb_entry);
	return 0;
}

subsys_initcall(dpu_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("infi.chen@unisoc.com");
MODULE_AUTHOR("kevin.tang@unisoc.com");
MODULE_DESCRIPTION("sprd roc1 dpu global and clk regs config");
