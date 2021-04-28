// SPDX-License-Identifier: GPL-2.0
//
// Spreadtrum multiplexer clock driver
//
// Copyright (C) 2017 Spreadtrum, Inc.
// Author: Chunyan Zhang <chunyan.zhang@spreadtrum.com>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include "mux.h"

u8 sprd_mux_helper_get_parent(const struct sprd_clk_common *common,
			      const struct sprd_mux_ssel *mux)
{
	unsigned int reg;
	u8 parent;
	int num_parents;
	int i;

	regmap_read(common->regmap, common->reg, &reg);
	parent = reg >> mux->shift;
	parent &= (1 << mux->width) - 1;

	if (!mux->table)
		return parent;

	num_parents = clk_hw_get_num_parents(&common->hw);

	for (i = 0; i < num_parents - 1; i++)
		if (parent >= mux->table[i] && parent < mux->table[i + 1])
			return i;

	return num_parents - 1;
}
EXPORT_SYMBOL_GPL(sprd_mux_helper_get_parent);

static u8 sprd_mux_get_parent(struct clk_hw *hw)
{
	struct sprd_mux *cm = hw_to_sprd_mux(hw);

	return sprd_mux_helper_get_parent(&cm->common, &cm->mux);
}

int sprd_mux_helper_set_parent(const struct sprd_clk_common *common,
			       const struct sprd_mux_ssel *mux,
			       u8 index)
{
	unsigned int reg;

	if (mux->table)
		index = mux->table[index];

	regmap_read(common->regmap, common->reg, &reg);
	reg &= ~GENMASK(mux->width + mux->shift - 1, mux->shift);
	regmap_write(common->regmap, common->reg,
			  reg | (index << mux->shift));

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_mux_helper_set_parent);

static int sprd_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct sprd_mux *cm = hw_to_sprd_mux(hw);

	return sprd_mux_helper_set_parent(&cm->common, &cm->mux, index);
}

const struct clk_ops sprd_mux_ops = {
	.get_parent = sprd_mux_get_parent,
	.set_parent = sprd_mux_set_parent,
	.determine_rate = __clk_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(sprd_mux_ops);

u8 sprd_mux_helper_get_parent_sec(const struct sprd_clk_common *common,
				  const struct sprd_mux_ssel *mux)
{
	unsigned int reg;
	u8 parent;
	int num_parents;
	int i;

	common->svc_handle->perf_ops.get_parent(common->id, &reg);
	parent = reg >> mux->shift;
	parent &= (1 << mux->width) - 1;

	if (!mux->table)
		return parent;

	num_parents = clk_hw_get_num_parents(&common->hw);

	for (i = 0; i < num_parents - 1; i++)
		if (parent >= mux->table[i] && parent < mux->table[i + 1])
			return i;

	return num_parents - 1;
}
EXPORT_SYMBOL_GPL(sprd_mux_helper_get_parent_sec);

static u8 sprd_mux_get_parent_sec(struct clk_hw *hw)
{
	struct sprd_mux *cm = hw_to_sprd_mux(hw);

	return sprd_mux_helper_get_parent_sec(&cm->common, &cm->mux);
}

int sprd_mux_helper_set_parent_sec(const struct sprd_clk_common *common,
				   const struct sprd_mux_ssel *mux,
				   u8 index)
{
	unsigned int reg;

	if (mux->table)
		index = mux->table[index];

	common->svc_handle->perf_ops.get_parent(common->id, &reg);
	reg &= ~GENMASK(mux->width + mux->shift - 1, mux->shift);
	common->svc_handle->perf_ops.set_parent(common->id, reg | (index << mux->shift));

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_mux_helper_set_parent_sec);

static int sprd_mux_set_parent_sec(struct clk_hw *hw, u8 index)
{
	struct sprd_mux *cm = hw_to_sprd_mux(hw);

	return sprd_mux_helper_set_parent_sec(&cm->common, &cm->mux, index);
}

const struct clk_ops sprd_mux_ops_sec = {
	.get_parent = sprd_mux_get_parent_sec,
	.set_parent = sprd_mux_set_parent_sec,
	.determine_rate = __clk_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(sprd_mux_ops_sec);
