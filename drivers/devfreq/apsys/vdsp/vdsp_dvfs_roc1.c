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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include "sprd_dvfs_apsys.h"
#include "sprd_dvfs_vdsp.h"
#include "apsys_dvfs_roc1.h"

static struct vdsp_dvfs_map_cfg map_table[] = {
	{0, VOLT70, VDSP_CLK_INDEX_192M, VDSP_CLK192M, EDAP_DIV_1, M0_DIV_3},
	{1, VOLT70, VDSP_CLK_INDEX_307M2, VDSP_CLK307M2, EDAP_DIV_1, M0_DIV_3},
	{2, VOLT75, VDSP_CLK_INDEX_468M, VDSP_CLK468M, EDAP_DIV_1, M0_DIV_3},
	{3, VOLT80, VDSP_CLK_INDEX_614M4, VDSP_CLK614M4, EDAP_DIV_1, M0_DIV_3},
	{4, VOLT80, VDSP_CLK_INDEX_702M, VDSP_CLK702M, EDAP_DIV_1, M0_DIV_3},
	{5, VOLT80, VDSP_CLK_INDEX_768M, VDSP_CLK768M, EDAP_DIV_1, M0_DIV_3},
};

static void vdsp_hw_dfs_en(bool dfs_en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (dfs_en)
		reg->ap_dfs_en_ctrl |= BIT(2);
	else
		reg->ap_dfs_en_ctrl &= ~BIT(2);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void vdsp_dvfs_map_cfg(void)
{
#if 1
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	reg->vdsp_index0_map = map_table[0].clk_level |
		map_table[0].edap_div << 3 |
		map_table[0].m0_div << 5 |
		map_table[0].volt_level << 7;
	reg->vdsp_index1_map = map_table[1].clk_level |
		map_table[1].edap_div << 3 |
		map_table[1].m0_div << 5 |
		map_table[1].volt_level << 7;
	reg->vdsp_index2_map = map_table[2].clk_level |
		map_table[2].edap_div << 3 |
		map_table[2].m0_div << 5 |
		map_table[2].volt_level << 7;
	reg->vdsp_index3_map = map_table[3].clk_level |
		map_table[3].edap_div << 3 |
		map_table[3].m0_div << 5 |
		map_table[3].volt_level << 7;
	reg->vdsp_index4_map = map_table[4].clk_level |
		map_table[4].edap_div << 3 |
		map_table[4].m0_div << 5 |
		map_table[4].volt_level << 7;
	reg->vdsp_index5_map = map_table[5].clk_level |
		map_table[5].edap_div << 3 |
		map_table[5].m0_div << 5 |
		map_table[5].volt_level << 7;
#endif
}

static void set_vdsp_work_freq(u32 freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->vdsp_dvfs_index_cfg = i;
			break;
		}
	}
}

static u32 get_vdsp_work_freq(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->vdsp_dvfs_index_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_vdsp_idle_freq(u32 freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->vdsp_dvfs_index_idle_cfg = i;
			break;
		}
	}
}

static u32 get_vdsp_idle_freq(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	u32 freq = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->vdsp_dvfs_index_idle_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_vdsp_work_index(int index)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->vdsp_dvfs_index_cfg = index;
}

static int get_vdsp_work_index(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	return reg->vdsp_dvfs_index_cfg;
}

static void set_vdsp_idle_index(int index)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->vdsp_dvfs_index_idle_cfg = index;
}

static int get_vdsp_idle_index(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	return reg->vdsp_dvfs_index_idle_cfg;
}

static void set_vdsp_gfree_wait_delay(u32 para)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	u32 temp;

	mutex_lock(&apsys_glb_reg_lock);
	temp = reg->ap_gfree_wait_delay_cfg;
	temp &= GENMASK(19, 0);
	reg->ap_gfree_wait_delay_cfg = para << 20 | temp;
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_vdsp_freq_upd_en_byp(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_freq_update_bypass |= BIT(2);
	else
		reg->ap_freq_update_bypass &= ~BIT(2);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_vdsp_freq_upd_delay_en(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_freq_upd_type_cfg |= BIT(1);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(1);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_vdsp_freq_upd_hdsk_en(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_freq_upd_type_cfg |= BIT(0);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(0);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_vdsp_dvfs_swtrig_en(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_sw_trig_ctrl |= BIT(0);
	else
		reg->ap_sw_trig_ctrl &= ~BIT(0);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_vdsp_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	memcpy(map_table, dvfs_table, sizeof(struct
		ip_dvfs_map_cfg));
}

static int get_vdsp_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		dvfs_table[i].map_index = map_table[i].map_index;
		dvfs_table[i].volt_level = map_table[i].volt_level;
		dvfs_table[i].clk_level = map_table[i].clk_level;
		dvfs_table[i].clk_rate = map_table[i].clk_rate;
		dvfs_table[i].volt_val = NULL;
	}

	return i;
}

static void get_vdsp_dvfs_status(struct ip_dvfs_status *dvfs_status)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	dvfs_status->apsys_cur_volt =
		roc1_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 12 & 0x7);
	dvfs_status->vsp_vote_volt =
		roc1_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 6 & 0x7);
	dvfs_status->dpu_vote_volt =
		roc1_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 3 & 0x7);
	dvfs_status->vdsp_vote_volt =
		roc1_apsys_val_to_volt(reg->ap_dvfs_voltage_dbg >> 9 & 0x7);
	dvfs_status->vsp_cur_freq =
		roc1_vsp_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg >> 3 & 0x3);
	dvfs_status->dpu_cur_freq =
		roc1_dpu_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg & 0x7);
	dvfs_status->vdsp_cur_freq =
		roc1_vdsp_val_to_freq(reg->ap_dvfs_cgm_cfg_dbg >> 5 & 0x7);
	dvfs_status->vdsp_edap_div = reg->ap_dvfs_cgm_cfg_dbg >> 8 & 0x3;
	dvfs_status->vdsp_m0_div = reg->ap_dvfs_cgm_cfg_dbg >> 10 & 0x3;
	mutex_unlock(&apsys_glb_reg_lock);
}

static int vdsp_dvfs_parse_dt(struct vdsp_dvfs *vdsp,
				struct device_node *np)
{
	int ret;

	pr_info("%s()\n", __func__);

	ret = of_property_read_u32(np, "sprd,gfree-wait-delay",
			&vdsp->dvfs_coffe.gfree_wait_delay);
	if (ret)
		vdsp->dvfs_coffe.gfree_wait_delay = 0x100;

	ret = of_property_read_u32(np, "sprd,freq-upd-hdsk-en",
			&vdsp->dvfs_coffe.freq_upd_hdsk_en);
	if (ret)
		vdsp->dvfs_coffe.freq_upd_hdsk_en = 1;

	ret = of_property_read_u32(np, "sprd,freq-upd-delay-en",
			&vdsp->dvfs_coffe.freq_upd_delay_en);
	if (ret)
		vdsp->dvfs_coffe.freq_upd_delay_en = 1;

	ret = of_property_read_u32(np, "sprd,freq-upd-en-byp",
			&vdsp->dvfs_coffe.freq_upd_en_byp);
	if (ret)
		vdsp->dvfs_coffe.freq_upd_en_byp = 0;

	ret = of_property_read_u32(np, "sprd,sw-trig-en",
			&vdsp->dvfs_coffe.sw_trig_en);
	if (ret)
		vdsp->dvfs_coffe.sw_trig_en = 0;

	return ret;
}

static int vdsp_dvfs_init(struct vdsp_dvfs *vdsp)
{
	pr_info("%s()\n", __func__);

	vdsp_dvfs_map_cfg();
	set_vdsp_gfree_wait_delay(vdsp->dvfs_coffe.gfree_wait_delay);
	set_vdsp_freq_upd_hdsk_en(vdsp->dvfs_coffe.freq_upd_hdsk_en);
	set_vdsp_freq_upd_delay_en(vdsp->dvfs_coffe.freq_upd_delay_en);
	set_vdsp_freq_upd_en_byp(vdsp->dvfs_coffe.freq_upd_en_byp);
	set_vdsp_work_index(vdsp->dvfs_coffe.work_index_def);
	set_vdsp_idle_index(vdsp->dvfs_coffe.idle_index_def);
	vdsp_hw_dfs_en(vdsp->dvfs_coffe.hw_dfs_en);

	return 0;
}

static struct vdsp_dvfs_ops vdsp_dvfs_ops = {
	.parse_dt = vdsp_dvfs_parse_dt,
	.dvfs_init = vdsp_dvfs_init,
	.hw_dfs_en = vdsp_hw_dfs_en,
	.set_work_freq = set_vdsp_work_freq,
	.get_work_freq = get_vdsp_work_freq,
	.set_idle_freq = set_vdsp_idle_freq,
	.get_idle_freq = get_vdsp_idle_freq,
	.set_work_index = set_vdsp_work_index,
	.get_work_index = get_vdsp_work_index,
	.set_idle_index = set_vdsp_idle_index,
	.get_idle_index = get_vdsp_idle_index,
	.set_dvfs_table = set_vdsp_dvfs_table,
	.get_dvfs_table = get_vdsp_dvfs_table,
	.get_dvfs_status = get_vdsp_dvfs_status,

	.set_gfree_wait_delay = set_vdsp_gfree_wait_delay,
	.set_freq_upd_en_byp = set_vdsp_freq_upd_en_byp,
	.set_freq_upd_delay_en = set_vdsp_freq_upd_delay_en,
	.set_freq_upd_hdsk_en = set_vdsp_freq_upd_hdsk_en,
	.set_dvfs_swtrig_en = set_vdsp_dvfs_swtrig_en,
};

static struct dvfs_ops_entry vdsp_dvfs_entry = {
	.ver = "roc1",
	.ops = &vdsp_dvfs_ops,
};

static int __init vdsp_dvfs_register(void)
{
	return vdsp_dvfs_ops_register(&vdsp_dvfs_entry);
}

subsys_initcall(vdsp_dvfs_register);
