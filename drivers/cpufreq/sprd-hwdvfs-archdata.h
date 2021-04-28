/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define MAX_VOLT_GRADE_NUM	16
#define MAX_MPLL_INDEX_NUM	8
#define MAX_DCDC_CPU_ADI_NUM	4
#define MAX_APCPU_DVFS_MISC_CFG_ENTRY	16
#define MAX_TOP_DVFS_MISC_CFG_ENTRY	16

#define GENREGSET(r, o, m)		{.reg = r, .off = o, .msk = m, .val = 0}
#define GENREGVALSET(r, o, m, v)	{.reg = r, .off = o, .msk = m, .val = v}
#define SPECREGVALSET(x, r, o, m, v) \
	{.x.reg = r, .x.off = o, .x.msk = m, .x.val = v}

enum sprd_cpudvfs_pmic {
	PMIC_SC2730,
	PMIC_SC2703,
	PMIC_FAN53555,
	MAX_PMIC_TYPE_NUM = 8,
};

enum dcdc_name {
	DCDC_CPU0,
	DCDC_CPU1,
	/* Add DCDC_CPUx here */
	DCDC_CPU0_I2C = MAX_DCDC_CPU_ADI_NUM,
	DCDC_CPU1_I2C,
};

enum mpll_name {
	MPLL0,
	MPLL1,
	MPLL2,
	MAX_MPLL,
};

enum mpll_output_val {
	ICP,
	POSTDIV,
	N,
};

struct reg_info {
	u32 reg;
	u32 off;
	u32 msk;
	u32 val;
};

struct mpll_index_entry {
	struct reg_info output[3];
};

struct mpll_index_tbl {
	struct mpll_index_entry entry[MAX_MPLL_INDEX_NUM];
};

struct volt_grades_table {
	struct reg_info regs_array[MAX_VOLT_GRADE_NUM];
	int grade_count;
};

struct udelay_tbl {
	struct reg_info tbl[MAX_VOLT_GRADE_NUM];
};

struct  topdvfs_volt_manager {
	struct volt_grades_table *grade_tbl;
	struct udelay_tbl *up_udelay_tbl, *down_udelay_tbl;
	struct reg_info *misc_cfg_array;
};

struct cpudvfs_freq_manager {
	struct reg_info *misc_cfg_array;
};

struct mpll_freq_manager {
	struct mpll_index_tbl *mpll_tbl;
};

struct pmic_data {
	u32 volt_base;	/*uV*/
	u32 per_step;	/*uV*/
	u32 margin_us;
	int (*update)(struct regmap *map, struct reg_info *regs, void *pm,
		      unsigned long u_volt, int index, int count);
	u32 (*up_cycle_calculate)(u32 max_val_uV, u32 slew_rate,
				  u32 module_clk_khz, u32 margin_us);
	u32 (*down_cycle_calculate)(u32 max_val_uV, u32 slew_rate,
				    u32 module_clk_hz, u32 margin_us);
};

struct dvfs_private_data {
	u32 module_clk_khz;
	struct pmic_data *pmic;
	struct topdvfs_volt_manager *volt_manager;
	struct cpudvfs_freq_manager *freq_manager;
	struct mpll_freq_manager *mpll_manager;
};

extern const struct dvfs_private_data ums512_dvfs_private_data;
