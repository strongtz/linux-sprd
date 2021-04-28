/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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

#include <asm/div64.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "sprd_dphy.h"

struct dphy_pll {
	u32 phy_clk;
	u32 div5b;
	u32 div2or3;
	u32 div5bbyass;
	u32 i_fbdivratio;
	u32 i_fracdiv2;
	u32 i_fracdiv1;
	u32 i_fracdiv0;
	u32 i_feedfwrdgain;
	u32 i_fracnen_h;
	u32 i_sscstepsize0;
	u32 i_sscstepsize1;
	u32 i_sscsteplength0;
	u32 i_sscsteplength1;
	u32 i_sscstepnum;
	u32 i_ssctype;
	u32 i_sscen_h;
	u32 i_prop_coeff;
	u32 i_int_coeff;
	u32 i_gainctrl;
	u32 i_tdctargetcnt0;
	u32 i_tdctargetcnt1;
	u32 i_lockthreshsel;
	u32 i_pllwait_cntr;
};

static struct dphy_pll pll_array[] = {
	{
		.phy_clk = 450*1000,
		.div5b = 1,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x68,
		.i_fracdiv2 = 0x07,
		.i_fracdiv1 = 0xe0,
		.i_fracdiv0 = 0x7e,
		.i_feedfwrdgain = 0x3,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 460*1000,
		.div5b = 1,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x6a,
		.i_fracdiv2 = 0x15,
		.i_fracdiv1 = 0xa9,
		.i_fracdiv0 = 0x5a,
		.i_feedfwrdgain = 0x3,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 500*1000,
		.div5b = 1,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x75,
		.i_fracdiv2 = 0x1a,
		.i_fracdiv1 = 0x95,
		.i_fracdiv0 = 0xa9,
		.i_feedfwrdgain = 0x2,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 530*1000,
		.div5b = 1,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x7c,
		.i_fracdiv2 = 0x03,
		.i_fracdiv1 = 0xf0,
		.i_fracdiv0 = 0x3f,
		.i_feedfwrdgain = 0x2,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 600*1000,
		.div5b = 1,
		.div2or3 = 0x0,
		.div5bbyass = 0,
		.i_fbdivratio = 0x5d,
		.i_fracdiv2 = 0x02,
		.i_fracdiv1 = 0xf4,
		.i_fracdiv0 = 0x2f,
		.i_feedfwrdgain = 0x3,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 790*1000,
		.div5b = 1,
		.div2or3 = 0x0,
		.div5bbyass = 0,
		.i_fbdivratio = 0x7a,
		.i_fracdiv2 = 0x25,
		.i_fracdiv1 = 0x6a,
		.i_fracdiv0 = 0x56,
		.i_feedfwrdgain = 0x2,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x5,
		.i_int_coeff = 0x9,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 800*1000,
		.div5b = 1,
		.div2or3 = 0,
		.div5bbyass = 0,
		.i_fbdivratio = 0x7c,
		.i_fracdiv2 = 0x3,
		.i_fracdiv1 = 0xf0,
		.i_fracdiv0 = 0x3f,
		.i_feedfwrdgain = 0x2,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x5,
		.i_int_coeff = 0x9,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 862*1000,
		.div5b = 0,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x63,
		.i_fracdiv2 = 0x1d,
		.i_fracdiv1 = 0x89,
		.i_fracdiv0 = 0xd8,
		.i_feedfwrdgain = 0x3,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 880*1000,
		.div5b = 0x0,
		.div2or3 = 0x1,
		.div5bbyass = 0x0,
		.i_fbdivratio = 0x65,
		.i_fracdiv2 = 0x3a,
		.i_fracdiv1 = 0x17,
		.i_fracdiv0 = 0xa1,
		.i_feedfwrdgain = 0x03,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0x0,
		.i_sscstepsize1 = 0x0,
		.i_sscsteplength0 = 0x0,
		.i_sscsteplength1 = 0x0,
		.i_sscstepnum = 0x0,
		.i_ssctype = 0x0,
		.i_sscen_h = 0x0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0x0,
		.i_lockthreshsel = 0x0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 900*1000,
		.div5b = 0,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x68,
		.i_fracdiv2 = 0x7,
		.i_fracdiv1 = 0xe0,
		.i_fracdiv0 = 0x7e,
		.i_feedfwrdgain = 0x3,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 930*1000,
		.div5b = 0,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x6b,
		.i_fracdiv2 = 0x1c,
		.i_fracdiv1 = 0x8d,
		.i_fracdiv0 = 0xc8,
		.i_feedfwrdgain = 0x3,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 960*1000,
		.div5b = 0x0,
		.div2or3 = 0x1,
		.div5bbyass = 0x0,
		.i_fbdivratio = 0x6e,
		.i_fracdiv2 = 0x31,
		.i_fracdiv1 = 0x3b,
		.i_fracdiv0 = 0x13,
		.i_feedfwrdgain = 0x02,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0x0,
		.i_sscstepsize1 = 0x0,
		.i_sscsteplength0 = 0x0,
		.i_sscsteplength1 = 0x0,
		.i_sscstepnum = 0x0,
		.i_ssctype = 0x0,
		.i_sscen_h = 0x0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0x0,
		.i_lockthreshsel = 0x0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 964600,
		.div5b = 0x0,
		.div2or3 = 0x1,
		.div5bbyass = 0x0,
		.i_fbdivratio = 0x6f,
		.i_fracdiv2 = 0x13,
		.i_fracdiv1 = 0x33,
		.i_fracdiv0 = 0x33,
		.i_feedfwrdgain = 0x2,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 979800,
		.div5b = 0,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x71,
		.i_fracdiv2 = 0x3,
		.i_fracdiv1 = 0x72,
		.i_fracdiv0 = 0x37,
		.i_feedfwrdgain = 0x2,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x4,
		.i_int_coeff = 0x8,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
	{
		.phy_clk = 1000*1000,
		.div5b = 0,
		.div2or3 = 0x1,
		.div5bbyass = 0,
		.i_fbdivratio = 0x74,
		.i_fracdiv2 = 0x13,
		.i_fracdiv1 = 0xb1,
		.i_fracdiv0 = 0x3b,
		.i_feedfwrdgain = 0x2,
		.i_fracnen_h = 0x1,
		.i_sscstepsize0 = 0,
		.i_sscstepsize1 = 0,
		.i_sscsteplength0 = 0,
		.i_sscsteplength1 = 0,
		.i_sscstepnum = 0,
		.i_ssctype = 0,
		.i_sscen_h = 0,
		.i_prop_coeff = 0x5,
		.i_int_coeff = 0x9,
		.i_gainctrl = 0x1,
		.i_tdctargetcnt0 = 0x22,
		.i_tdctargetcnt1 = 0,
		.i_lockthreshsel = 0,
		.i_pllwait_cntr = 0x2,
	},
};

static inline u32 phy_readl(unsigned long base, u32 addr)
{
	return readl((void __iomem *)(base + addr));
}

static inline void phy_writel(unsigned long base, u32 val, u32 addr)
{
	writel(val, (void __iomem *)(base + addr));
}

static inline void writel_nbits(unsigned long base, u32 val,
				u8 nbits, u8 offset, u32 addr)
{
	u32 tmp;

	tmp = phy_readl(base, addr);
	tmp &= ~(((1 << nbits) - 1) << offset);
	tmp |= val << offset;
	phy_writel(base, tmp, addr);
}

#if 0
static void dphy_bios_programming(unsigned long base)
{
	writel_nbits(base, 0x60, 8, 0, 0x300);
	writel_nbits(base, 0x7a, 8, 0, 0x308);
	writel_nbits(base, 0xa1, 8, 8, 0x308);
	writel_nbits(base, 0x17, 6, 16, 0x308);
	writel_nbits(base, 0x3, 4, 0, 0x30c);
	writel_nbits(base, 0x0, 8, 0, 0x310);
	writel_nbits(base, 0x0, 2, 8, 0x310);
	writel_nbits(base, 0x0, 8, 16, 0x310);
	writel_nbits(base, 0x0, 2, 24, 0x310);
	writel_nbits(base, 0x0, 3, 0, 0x314);
	writel_nbits(base, 0x0, 1, 16, 0x314);
	writel_nbits(base, 0x4, 4, 0, 0x318);
	writel_nbits(base, 0x8, 5, 8, 0x318);
	writel_nbits(base, 0x1, 3, 16, 0x318);
	writel_nbits(base, 0x22, 8, 0, 0x320);
	writel_nbits(base, 0x0, 2, 8, 0x320);
	writel_nbits(base, 0x0, 1, 0, 0x324);
	writel_nbits(base, 0x2, 3, 16, 0x324);
	writel_nbits(base, 0x1, 1, 16, 0x30c);
	writel_nbits(base, 0x1, 1, 5, 0x04);
	writel_nbits(base, 0x0, 1, 4, 0x04);
	writel_nbits(base, 0x4, 3, 1, 0x04);
	writel_nbits(base, 0xf, 8, 0, 0xe0);
	writel_nbits(base, 0xd, 8, 0, 0xe4);
	writel_nbits(base, 0x22, 8, 0, 0xe8);
	writel_nbits(base, 0x13, 8, 0, 0xec);
	writel_nbits(base, 0x13, 8, 0, 0xfc);
	writel_nbits(base, 0x20, 8, 0, 0xf0);
	writel_nbits(base, 0x20, 8, 0, 0x100);
	writel_nbits(base, 0xf, 8, 0, 0x104);
	writel_nbits(base, 0x53, 8, 0, 0x10c);
	writel_nbits(base, 0xb, 8, 0, 0x108);
	writel_nbits(base, 0x1a, 8, 0, 0x114);
	writel_nbits(base, 0x186, 12, 0, 0x110);
	writel_nbits(base, 0x9c, 8, 12, 0x110);
	writel_nbits(base, 0x7, 5, 11, 0x18);
	writel_nbits(base, 0x4, 3, 28, 0x58);
}
#endif

static int dphy_set_pll_reg(unsigned long base, u32 freq)
{
	struct dphy_pll *pll;
	u8 i;

	for (i = 0; i < ARRAY_SIZE(pll_array); i++)
		if (pll_array[i].phy_clk == freq)
			break;

	if (i == ARRAY_SIZE(pll_array)) {
		pr_err("the target freq %u is not supported\n", freq);
		return -EINVAL;
	}

	pll = &pll_array[i];

	writel_nbits(base, pll->div5b, 3, 1, 0x04);
	writel_nbits(base, pll->div2or3, 1, 4, 0x04);
	writel_nbits(base, pll->div5bbyass, 1, 5, 0x04);
	writel_nbits(base, pll->i_fbdivratio, 8, 0, 0x300);
	writel_nbits(base, pll->i_fracdiv2, 6, 16, 0x308);
	writel_nbits(base, pll->i_fracdiv1, 8, 8, 0x308);
	writel_nbits(base, pll->i_fracdiv0, 8, 0, 0x308);
	writel_nbits(base, pll->i_feedfwrdgain, 4, 0, 0x30c);
	writel_nbits(base, pll->i_fracnen_h, 1, 16, 0x30c);
	writel_nbits(base, pll->i_sscstepsize0, 8, 0, 0x310);
	writel_nbits(base, pll->i_sscstepsize1, 2, 8, 0x310);
	writel_nbits(base, pll->i_sscsteplength0, 8, 16, 0x310);
	writel_nbits(base, pll->i_sscsteplength1, 2, 24, 0x310);
	writel_nbits(base, pll->i_sscstepnum, 3, 0, 0x314);
	writel_nbits(base, pll->i_ssctype, 2, 8, 0x314);
	writel_nbits(base, pll->i_sscen_h, 1, 16, 0x314);
	writel_nbits(base, pll->i_prop_coeff, 4, 0, 0x318);
	writel_nbits(base, pll->i_int_coeff, 5, 8, 0x318);
	writel_nbits(base, pll->i_gainctrl, 3, 16, 0x318);
	writel_nbits(base, pll->i_tdctargetcnt0, 8, 0, 0x320);
	writel_nbits(base, pll->i_tdctargetcnt1, 2, 8, 0x320);
	writel_nbits(base, pll->i_lockthreshsel, 1, 0, 0x324);
	writel_nbits(base, pll->i_pllwait_cntr, 3, 16, 0x324);

	return 0;
}

static int dphy_pll_config(struct dphy_context *ctx)
{
	struct sprd_dphy *dphy = container_of(ctx, struct sprd_dphy, ctx);
	unsigned long base = ctx->apbbase;

	/***********intel dphy enable dsi**************/
	phy_writel(base, 0x00, 0x14);
	phy_writel(base, 0x00, 0x00);
	phy_writel(base, 0x0f, 0x18);
	phy_writel(base, 0x00, 0x58);
	phy_writel(base, 0x3810, 0x44);

	writel_nbits(base, 1, 1, 4, 0x120);

	dphy_set_pll_reg(base, ctx->freq);

	writel_nbits(base, 1, 1, 0, 0x120);

	dphy->ppi->force_pll(ctx, 1);

	return 0;
}

static int dphy_timing_config(struct dphy_context *ctx)
{
	unsigned long base = ctx->apbbase;

	/************set hs prepare time*************/
	phy_writel(base, 0x06, 0xe4);
	phy_writel(base, 0x1c, 0xe8);
	phy_writel(base, 0x09, 0xec);

	return 0;
}

static struct dphy_pll_ops intel_ip733mdsitop_ops = {
	.pll_config = dphy_pll_config,
	.timing_config = dphy_timing_config,
	.hop_config = NULL,
	.ssc_en = NULL,
	.force_pll = NULL,
};

static struct ops_entry entry = {
	.ver = "intel,ip733mdsitop",
	.ops = &intel_ip733mdsitop_ops,
};

static int __init sprd_dphy_pll_register(void)
{
	return dphy_pll_ops_register(&entry);
}

subsys_initcall(sprd_dphy_pll_register);
