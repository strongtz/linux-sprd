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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "sprd-hwdvfs-archdata.h"

static int default_dcdc_volt_update(struct regmap *map, struct reg_info *regs,
				    void *data, unsigned long u_volt, int index,
				    int count)
{
	u32 reg, off, msk, val;
	struct pmic_data *pm = (struct pmic_data *)data;

	if (index < 0 || index > count) {
		pr_err("Incorrcet voltage gear table index\n");
		return -EINVAL;
	}

	if (!pm) {
		pr_err("The pmic needed to update volt gear value is NULL\n");
		return -ENODEV;
	}

	reg = regs[index].reg;
	off = regs[index].off;
	msk = regs[index].msk;

	if ((u_volt - pm->volt_base) % pm->per_step)
		u_volt += pm->per_step;

	val = (u_volt - pm->volt_base) / pm->per_step;

	return regmap_update_bits(map, reg, msk << off, val << off);
}

static u32 default_cycle_calculate(u32 max_val_uV, u32 slew_rate,
				   u32 module_clk_hz, u32 margin_us)
{
	pr_debug("max_val_uV = %d, slew_rate = %d, module_clk_hz = %d margin = %d\n",
		 max_val_uV, slew_rate, module_clk_hz, margin_us);

	return (max_val_uV / slew_rate + margin_us) * module_clk_hz / 1000;
}

static struct pmic_data pmic_array[MAX_PMIC_TYPE_NUM] = {
	[PMIC_SC2730] = {
		.volt_base = 0,
		.per_step = 3125,
		.margin_us = 20,
		.update = default_dcdc_volt_update,
		.up_cycle_calculate = default_cycle_calculate,
		.down_cycle_calculate = default_cycle_calculate,
	},
	[PMIC_SC2703] = {
		.volt_base = 300000,
		.per_step = 10000,
		.margin_us = 20,
		.update = default_dcdc_volt_update,
		.up_cycle_calculate = default_cycle_calculate,
		.down_cycle_calculate = default_cycle_calculate,
	},
	[PMIC_FAN53555] = {
		.volt_base = 600000,
		.per_step = 10000,
		.margin_us = 20,
		.update = default_dcdc_volt_update,
		.up_cycle_calculate = default_cycle_calculate,
		.down_cycle_calculate = default_cycle_calculate,
	},
	{
	},
};

/* UMS312 Private data */
static struct volt_grades_table ums312_volt_grades_tbl[] = {
	[DCDC_CPU0] = {
		.regs_array = {
			GENREGSET(0xf4, 0, 0x1ff),
			GENREGSET(0xf4, 9, 0x1ff),
			GENREGSET(0xf4, 18, 0x1ff),
			GENREGSET(0xf8, 0, 0x1ff),
			GENREGSET(0xf8, 9, 0x1ff),
			GENREGSET(0xf8, 18, 0x1ff),
			GENREGSET(0xfc, 0, 0x1ff),
			GENREGSET(0xfc, 9, 0x1ff),
		},
		.grade_count = 8,
	},
	[DCDC_CPU1] = {
		.regs_array = {
			GENREGSET(0x100, 0, 0x1ff),
			GENREGSET(0x100, 9, 0x1ff),
			GENREGSET(0x100, 18, 0x1ff),
			GENREGSET(0x104, 0, 0x1ff),
			GENREGSET(0x104, 9, 0x1ff),
			GENREGSET(0x104, 18, 0x1ff),
		},
		.grade_count = 6,
	},
	[DCDC_CPU1_I2C] = {
		.regs_array = {
			GENREGSET(0x12c, 0, 0x7f),
			GENREGSET(0x12c, 7, 0x7f),
			GENREGSET(0x12c, 14, 0x7f),
			GENREGSET(0x12c, 21, 0x7f),
			GENREGSET(0x130, 0, 0x7f),
			GENREGSET(0x130, 7, 0x7f),
		},
		.grade_count = 6,
	},
};

static struct udelay_tbl ums312_up_udelay_tbl[] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x58, 0, 0x1ffff),
			GENREGSET(0x58, 16, 0x1ffff),
			GENREGSET(0x54, 0, 0x1ffff),
			GENREGSET(0x54, 16, 0x1ffff),
			GENREGSET(0x50, 0, 0x1ffff),
			GENREGSET(0x50, 16, 0x1ffff),
			GENREGSET(0x110, 0, 0x1ffff),
		},
	},
	[DCDC_CPU1] = {
		.tbl = {
			GENREGSET(0x84, 0, 0xffff),
			GENREGSET(0x84, 16, 0xffff),
			GENREGSET(0x80, 0, 0xffff),
			GENREGSET(0x80, 16, 0xffff),
			GENREGSET(0x7c, 0, 0xffff),
			GENREGSET(0x7c, 16, 0xffff),
			GENREGSET(0x118, 0, 0xffff),
		},
	},
};

static struct udelay_tbl ums312_down_udelay_tbl[] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x64, 0, 0xffff),
			GENREGSET(0x64, 16, 0xffff),
			GENREGSET(0x60, 0, 0xffff),
			GENREGSET(0x60, 16, 0xffff),
			GENREGSET(0x5c, 0, 0xffff),
			GENREGSET(0x5c, 16, 0xffff),
			GENREGSET(0x114, 0, 0xffff),
		},
	},
	[DCDC_CPU1] = {
		.tbl = {
			GENREGSET(0x90, 0, 0xffff),
			GENREGSET(0x90, 16, 0xffff),
			GENREGSET(0x8c, 0, 0xffff),
			GENREGSET(0x8c, 16, 0xffff),
			GENREGSET(0x88, 0, 0xffff),
			GENREGSET(0x88, 16, 0xffff),
			GENREGSET(0x11c, 0, 0xffff),
		},
	},
};

static struct reg_info ums312_volt_misc_cfg_array[] = {
	GENREGVALSET(0, 0, 0, 0),
};

static struct reg_info ums312_freq_misc_cfg_array[] = {
	/* Set default work index 7 for lit core */
	GENREGVALSET(0x214, 0, 0xf, 7),
	/* Set default work index 3 for big core */
	GENREGVALSET(0x224, 0, 0xf, 3),
};

static struct mpll_index_tbl ums312_mpll_index_tbl[MAX_MPLL] = {
	[MPLL1] = {
		.entry = {
			{
				.output = {
					GENREGVALSET(0x30, 0, 0x7, 5),
					GENREGVALSET(0x30, 3, 0x1, 0),
					GENREGVALSET(0x30, 4, 0x7ff, 0x4d),
				},
			},
		},
	},
};

static struct topdvfs_volt_manager ums312_volt_manager = {
	.grade_tbl = ums312_volt_grades_tbl,
	.up_udelay_tbl =  ums312_up_udelay_tbl,
	.down_udelay_tbl = ums312_down_udelay_tbl,
	.misc_cfg_array = ums312_volt_misc_cfg_array,
};

static struct cpudvfs_freq_manager ums312_freq_manager = {
	.misc_cfg_array = ums312_freq_misc_cfg_array,
};

static struct mpll_freq_manager ums312_mpll_manager = {
	.mpll_tbl = ums312_mpll_index_tbl,
};

const struct dvfs_private_data ums312_dvfs_private_data = {
	.module_clk_khz = 128000,
	.pmic = pmic_array,
	.volt_manager = &ums312_volt_manager,
	.freq_manager = &ums312_freq_manager,
	.mpll_manager = &ums312_mpll_manager,
};

static struct reg_info ums512_volt_misc_cfg_array[] = {
	GENREGVALSET(0, 0, 0, 0),
};

static struct udelay_tbl ums512_down_udelay_tbl[] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x64, 0, 0xffff),
			GENREGSET(0x64, 16, 0xffff),
			GENREGSET(0x60, 0, 0xffff),
			GENREGSET(0x60, 16, 0xffff),
			GENREGSET(0x5c, 0, 0xffff),
			GENREGSET(0x5c, 16, 0xffff),
			GENREGSET(0x114, 0, 0xffff),
		},
	},
	[DCDC_CPU1] = {
		.tbl = {
			GENREGSET(0x90, 0, 0xffff),
			GENREGSET(0x90, 16, 0xffff),
			GENREGSET(0x8c, 0, 0xffff),
			GENREGSET(0x8c, 16, 0xffff),
			GENREGSET(0x88, 0, 0xffff),
			GENREGSET(0x88, 16, 0xffff),
			GENREGSET(0x11c, 0, 0xffff),
		},
	},
};

static struct udelay_tbl ums512_up_udelay_tbl[] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x58, 0, 0xffff),
			GENREGSET(0x58, 16, 0xffff),
			GENREGSET(0x54, 0, 0xffff),
			GENREGSET(0x54, 16, 0xffff),
			GENREGSET(0x50, 0, 0xffff),
			GENREGSET(0x50, 16, 0xffff),
			GENREGSET(0x110, 0, 0xffff),
		},
	},
	[DCDC_CPU1] = {
		.tbl = {
			GENREGSET(0x84, 0, 0xffff),
			GENREGSET(0x84, 16, 0xffff),
			GENREGSET(0x80, 0, 0xffff),
			GENREGSET(0x80, 16, 0xffff),
			GENREGSET(0x7c, 0, 0xffff),
			GENREGSET(0x7c, 16, 0xffff),
			GENREGSET(0x118, 0, 0xffff),
		},
	},
};

static struct volt_grades_table ums512_volt_grades_tbl[] = {
	[DCDC_CPU0] = {
		.regs_array = {
			GENREGSET(0xf4, 0, 0x1ff),
			GENREGSET(0xf4, 9, 0x1ff),
			GENREGSET(0xf4, 18, 0x1ff),
			GENREGSET(0xf8, 0, 0x1ff),
			GENREGSET(0xf8, 9, 0x1ff),
			GENREGSET(0xf8, 18, 0x1ff),
			GENREGSET(0xfc, 0, 0x1ff),
		},
		.grade_count = 7,
	},
	[DCDC_CPU1_I2C] = {
		.regs_array = {
			GENREGSET(0x12c, 0, 0x7f),
			GENREGSET(0x12c, 7, 0x7f),
			GENREGSET(0x12c, 14, 0x7f),
			GENREGSET(0x12c, 21, 0x7f),
			GENREGSET(0x130, 0, 0x7f),
			GENREGSET(0x130, 7, 0x7f),
		},
		.grade_count = 6,
	},
};

static struct reg_info ums512_freq_misc_cfg_array[] = {
	/* Set default work index 7 for lit core */
	GENREGVALSET(0x214, 0, 0xf, 7),
	/* Set default work index 3 for big core */
	GENREGVALSET(0x224, 0, 0xf, 3),
	/* Set default work index 3 to twppll for scu */
	GENREGVALSET(0x22c, 0, 0xf, 3),
	/* The end of misc configurations */
	GENREGVALSET(0, 0, 0, 0),
};

static struct mpll_index_tbl ums512_mpll_index_tbl[MAX_MPLL] = {
	[MPLL0] = {
		.entry = {
			{
				.output = {
					GENREGVALSET(0x8c, 0, 0x7, 5),
					GENREGVALSET(0x8c, 3, 0x1, 0),
					GENREGVALSET(0x8c, 4, 0x7ff, 0x4d),
				},
			},

			{
				.output = {
					GENREGVALSET(0x90, 0, 0x7, 5),
					GENREGVALSET(0x90, 3, 0x1, 0),
					GENREGVALSET(0x90, 4, 0x7ff, 0x4d),
				},
			},

			{
				.output = {
					GENREGVALSET(0x94, 0, 0x7, 5),
					GENREGVALSET(0x94, 3, 0x1, 0),
					GENREGVALSET(0x94, 4, 0x7ff, 0x4d),
				},
			},
		},
	},

	[MPLL1] = {
		.entry = {
			{
				.output = {
					GENREGVALSET(0x2c, 0, 0x7, 4),
					GENREGVALSET(0x2c, 3, 0x1, 0),
					GENREGVALSET(0x2c, 4, 0x7ff, 0x46),
				},
			},

			{
				.output = {
					GENREGVALSET(0x30, 0, 0x7, 4),
					GENREGVALSET(0x30, 3, 0x1, 0),
					GENREGVALSET(0x30, 4, 0x7ff, 0x48),
				},
			},

			{
				.output = {
					GENREGVALSET(0x34, 0, 0x7, 5),
					GENREGVALSET(0x34, 3, 0x1, 0),
					GENREGVALSET(0x34, 4, 0x7ff, 0x4d),
				},
			},

			{
				.output = {
					GENREGVALSET(0x38, 0, 0x7, 5),
					GENREGVALSET(0x38, 3, 0x1, 0),
					GENREGVALSET(0x38, 4, 0x7ff, 0x4d),
				},
			},

			{
				.output = {
					GENREGVALSET(0x3c, 0, 0x7, 5),
					GENREGVALSET(0x3c, 3, 0x1, 0),
					GENREGVALSET(0x3c, 4, 0x7ff, 0x4d),
				},
			},

			{
				.output = {
					GENREGVALSET(0x40, 0, 0x7, 5),
					GENREGVALSET(0x40, 3, 0x1, 0),
					GENREGVALSET(0x40, 4, 0x7ff, 0x4d),
				},
			},

		},
	},

	[MPLL2] = {
		.entry = {
			{
				.output = {
					GENREGVALSET(0xc8, 0, 0x7, 0),
					GENREGVALSET(0xc8, 3, 0x1, 0),
					GENREGVALSET(0xc8, 4, 0x7ff, 0x27),
				},
			},

			{
				.output = {
					GENREGVALSET(0xcc, 0, 0x7, 0),
					GENREGVALSET(0xcc, 3, 0x1, 0),
					GENREGVALSET(0xcc, 4, 0x7ff, 0x2b),
				},
			},

			{
				.output = {
					GENREGVALSET(0xd0, 0, 0x7, 1),
					GENREGVALSET(0xd0, 3, 0x1, 0),
					GENREGVALSET(0xd0, 4, 0x7ff, 0x2f),
				},
			},

			{
				.output = {
					GENREGVALSET(0xd4, 0, 0x7, 1),
					GENREGVALSET(0xd4, 3, 0x1, 0),
					GENREGVALSET(0xd4, 4, 0x7ff, 0x33),
				},
			},

			{
				.output = {
					GENREGVALSET(0xd8, 0, 0x7, 2),
					GENREGVALSET(0xd8, 3, 0x1, 0),
					GENREGVALSET(0xd8, 4, 0x7ff, 0x36),
				},
			},
		},
	},
};

static struct topdvfs_volt_manager ums512_volt_manager = {
	.grade_tbl = ums512_volt_grades_tbl,
	.up_udelay_tbl =  ums512_up_udelay_tbl,
	.down_udelay_tbl = ums512_down_udelay_tbl,
	.misc_cfg_array = ums512_volt_misc_cfg_array,
};

static struct cpudvfs_freq_manager ums512_freq_manager = {
	.misc_cfg_array = ums512_freq_misc_cfg_array,
};

static struct mpll_freq_manager ums512_mpll_manager = {
	.mpll_tbl = ums512_mpll_index_tbl,
};

const struct dvfs_private_data ums512_dvfs_private_data = {
	.module_clk_khz = 128000,
	.pmic = pmic_array,
	.volt_manager = &ums512_volt_manager,
	.freq_manager = &ums512_freq_manager,
	.mpll_manager = &ums512_mpll_manager,
};
