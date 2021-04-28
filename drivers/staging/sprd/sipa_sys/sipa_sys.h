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

#ifndef _SIPA_SYS_H_
#define _SIPA_SYS_H_

#include <linux/sipa.h>

struct sipa_sys_cfg_tag {
	const char *name;

	dev_t dev_num;
	struct class *class;
	struct device *dev;

	struct regmap *sys_regmap;
	u32 ipaeb_reg;
	u32 ipaeb_mask;
	u32 cm4eb_reg;
	u32 cm4eb_mask;
	u32 autogateb_reg;
	u32 autogateb_mask;
	u32 s5autogateb_reg;
	u32 s5autogateb_mask;
	struct regmap *pmu_regmap;
	u32 forcewakeup_reg;
	u32 forcewakeup_mask;
	u32 forceshutdown_reg;
	u32 forceshutdown_mask;
	u32 autoshutdown_reg;
	u32 autoshutdown_mask;
	u32 forcedslp_reg;
	u32 forcedslp_mask;
	u32 dslpeb_reg;
	u32 dslpeb_mask;
	u32 forcelslp_reg;
	u32 forcelslp_mask;
	u32 lslpeb_reg;
	u32 lslpeb_mask;
	u32 smartlslp_reg;
	u32 smartlslp_mask;
	struct regmap *aon_sec_regmap;
	u32 ipaseceb_reg;
	u32 ipaseceb_mask;

	u32 pciepllhsel_reg;
	u32 pciepllhsel_mask;
	u32 pciepllvsel_reg;
	u32 pciepllvsel_mask;
	u32 xtlbufpciehsel_reg;
	u32 xtlbufpciehsel_mask;
	u32 xtlbufpcievsel_reg;
	u32 xtlbufpcievsel_mask;
};

#endif
