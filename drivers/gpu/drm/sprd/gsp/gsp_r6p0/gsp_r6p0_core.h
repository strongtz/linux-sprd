/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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
#ifndef _GSP_R6P0_CORE_H
#define _GSP_R6P0_CORE_H

#include <linux/device.h>
#include <linux/list.h>
#include <drm/gsp_cfg.h>
#include "gsp_core.h"
#include "gsp_debug.h"

#define R2P0_DPU_CLOCK_NAME			 ("clk_dpu_core")
#define R2P0_DPU_CLOCK_PARENT		   ("clk_dpu_core_src")

#define MIN_POOL_SIZE				   (6 * 1024)
#define R6P0_GSP_COEF_CACHE_MAX		 32

struct gsp_r6p0_core {
	struct gsp_core common;
	struct list_head coef_list;
	struct COEF_ENTRY_T coef_cache[R6P0_GSP_COEF_CACHE_MAX];

	ulong gsp_coef_force_calc;
	uint32_t cache_coef_init_flag;
	char coef_buf_pool[MIN_POOL_SIZE];

	struct clk *dpu_clk;
	struct clk *dpu_clk_parent;
	/* module ctl reg base, virtual	0x63000000 */
	void __iomem *gsp_ctl_reg_base;
};

#define MEM_OPS_ADDR_ALIGN_MASK (0x7UL)

int gsp_r6p0_core_parse_dt(struct gsp_core *core);

int gsp_r6p0_core_copy_cfg(struct gsp_kcfg *kcfg, void *arg, int index);

int gsp_r6p0_core_init(struct gsp_core *core);

int gsp_r6p0_core_alloc(struct gsp_core **core, struct device_node *node);

int gsp_r6p0_core_trigger(struct gsp_core *core);

int gsp_r6p0_core_enable(struct gsp_core *core);

void gsp_r6p0_core_disable(struct gsp_core *core);

int gsp_r6p0_core_release(struct gsp_core *core);

int __user *gsp_r6p0_core_intercept(void __user *arg, int index);
void gsp_r6p0_core_reset(struct gsp_core *core);
void gsp_r6p0_core_dump(struct gsp_core *core);

#endif
