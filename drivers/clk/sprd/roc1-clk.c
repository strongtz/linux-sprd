// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum Roc1 clock driver
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Xiaolong Zhang <xiaolong.zhang@unisoc.com>

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,roc1-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

/* pll gate clock */
static SPRD_PLL_SC_GATE_CLK(isppll_gate, "isppll-gate", "ext-26m", 0x8c,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(dpll0_gate,  "dpll0-gate",  "ext-26m", 0x98,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(dpll1_gate,  "dpll1-gate",  "ext-26m", 0x9c,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(lpll_gate,   "lpll-gate",   "ext-26m", 0xa0,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(twpll_gate,  "twpll-gate",  "ext-26m", 0xa4,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(gpll_gate,   "gpll-gate",   "ext-26m", 0xa8,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(rpll_gate,   "rpll-gate",   "ext-26m", 0xac,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll0_gate,  "mpll0-gate",  "ext-26m", 0x190,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll1_gate,  "mpll1-gate",  "ext-26m", 0x194,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll2_gate,  "mpll2-gate",  "ext-26m", 0x198,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(apll_gate,  "apll-gate",  "ext-26m", 0x1ec,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *roc1_pmu_gate_clks[] = {
	/* address base is 0x32280000 */
	&isppll_gate.common,
	&dpll0_gate.common,
	&dpll1_gate.common,
	&lpll_gate.common,
	&twpll_gate.common,
	&gpll_gate.common,
	&rpll_gate.common,
	&mpll0_gate.common,
	&mpll1_gate.common,
	&mpll2_gate.common,
	&apll_gate.common,
};

static struct clk_hw_onecell_data roc1_pmu_gate_hws = {
	.hws	= {
		[CLK_ISPPLL_GATE] = &isppll_gate.common.hw,
		[CLK_DPLL0_GATE]  = &dpll0_gate.common.hw,
		[CLK_DPLL1_GATE]  = &dpll1_gate.common.hw,
		[CLK_LPLL_GATE]   = &lpll_gate.common.hw,
		[CLK_TWPLL_GATE]  = &twpll_gate.common.hw,
		[CLK_GPLL_GATE]   = &gpll_gate.common.hw,
		[CLK_RPLL_GATE]   = &rpll_gate.common.hw,
		[CLK_MPLL0_GATE]  = &mpll0_gate.common.hw,
		[CLK_MPLL1_GATE]  = &mpll1_gate.common.hw,
		[CLK_MPLL2_GATE]  = &mpll2_gate.common.hw,
		[CLK_APLL_GATE]   = &apll_gate.common.hw,
	},
	.num = CLK_PMU_GATE_NUM,
};

static struct sprd_clk_desc roc1_pmu_gate_desc = {
	.clk_clks	= roc1_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_pmu_gate_clks),
	.hw_clks        = &roc1_pmu_gate_hws,
};

/* pll clock at g0 */
static struct freq_table dpll0_ftable[5] = {
	{ .ibias = 2, .max_freq = 1173000000ULL },
	{ .ibias = 3, .max_freq = 1475000000ULL },
	{ .ibias = 4, .max_freq = 1855000000ULL },
	{ .ibias = 5, .max_freq = 1866000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_dpll0[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 73,	.width = 1 },	/* div_s	*/
	{ .shift = 74,	.width = 1 },	/* mod_en	*/
	{ .shift = 83,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 2 },	/* icp		*/
	{ .shift = 3,	.width = 13 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(dpll0_clk, "dpll0", "dpll0-gate", 0x10,
				   3, dpll0_ftable, f_dpll0, 240);

static struct clk_bit_field f_dpll1[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 72,	.width = 1 },	/* div_s	*/
	{ .shift = 73,	.width = 1 },	/* mod_en	*/
	{ .shift = 82,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 2 },	/* icp		*/
	{ .shift = 3,	.width = 13 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(dpll1_clk, "dpll1", "dpll1-gate", 0x24,
				   3, dpll0_ftable, f_dpll1, 240);

static struct sprd_clk_common *roc1_g0_pll_clks[] = {
	/* address base is 0x32390000 */
	&dpll0_clk.common,
	&dpll1_clk.common,
};

static struct clk_hw_onecell_data roc1_g0_pll_hws = {
	.hws	= {
		[CLK_DPLL0]	= &dpll0_clk.common.hw,
		[CLK_DPLL1]	= &dpll1_clk.common.hw,
	},
	.num	= CLK_ANLG_PHY_G0_NUM,
};

static struct sprd_clk_desc roc1_g0_pll_desc = {
	.clk_clks	= roc1_g0_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_g0_pll_clks),
	.hw_clks	= &roc1_g0_pll_hws,
};

/* pll clock at g4 */
static struct freq_table mpll_ftable[7] = {
	{ .ibias = 1, .max_freq = 1400000000ULL },
	{ .ibias = 2, .max_freq = 1600000000ULL },
	{ .ibias = 3, .max_freq = 1800000000ULL },
	{ .ibias = 4, .max_freq = 2000000000ULL },
	{ .ibias = 5, .max_freq = 2200000000ULL },
	{ .ibias = 6, .max_freq = 2500000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_mpll1[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 112,	.width = 1 },	/* div_s	*/
	{ .shift = 113,	.width = 1 },	/* mod_en	*/
	{ .shift = 122,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 5,	.width = 7 },	/* icp		*/
	{ .shift = 8,	.width = 18 },	/* n		*/
	{ .shift = 87,	.width = 7 },	/* nint		*/
	{ .shift = 64,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 144,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll1_clk, "mpll1", "mpll1-gate", 0x0,
				   5, mpll_ftable, f_mpll1, 240,
				   1000, 1000, 1, 1200000000ULL);

static struct sprd_clk_common *roc1_g4_pll_clks[] = {
	/* address base is 0x323c0000 */
	&mpll1_clk.common,
};

static struct clk_hw_onecell_data roc1_g4_pll_hws = {
	.hws	= {
		[CLK_MPLL1]	= &mpll1_clk.common.hw,
	},
	.num	= CLK_ANLG_PHY_G4_NUM,
};

static struct sprd_clk_desc roc1_g4_pll_desc = {
	.clk_clks	= roc1_g4_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_g4_pll_clks),
	.hw_clks	= &roc1_g4_pll_hws,
};

/* pll at g5 */
static struct freq_table ftable[6] = {
	{ .ibias = 2, .max_freq = 900000000ULL },
	{ .ibias = 3, .max_freq = 1100000000ULL },
	{ .ibias = 4, .max_freq = 1300000000ULL },
	{ .ibias = 5, .max_freq = 1500000000ULL },
	{ .ibias = 6, .max_freq = 1600000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_rpll[PLL_FACT_MAX] = {
	{ .shift = 96,	.width = 1 },	/* lock_done	*/
	{ .shift = 35,	.width = 1 },	/* div_s	*/
	{ .shift = 36,	.width = 1 },	/* mod_en	*/
	{ .shift = 37,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 0 },	/* icp		*/
	{ .shift = 43,	.width = 21 },	/* n		*/
	{ .shift = 87,	.width = 7 },	/* nint		*/
	{ .shift = 64,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 25,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(rpll_clk, "rpll", "ext-26m", 0x8,
				   4, ftable, f_rpll, 240,
				   1000, 1000, 1, 750000000ULL);

static SPRD_SC_GATE_CLK(audio_gate,	"audio-gate",	"ext-26m", 0xc,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_g5_pll_clks[] = {
	/* address base is 0x323d0000 */
	&rpll_clk.common,
	&audio_gate.common,
};

static struct clk_hw_onecell_data roc1_g5_pll_hws = {
	.hws	= {
		[CLK_RPLL]		= &rpll_clk.common.hw,
		[CLK_AUDIO_GATE]	= &audio_gate.common.hw,
	},
	.num	= CLK_ANLG_PHY_G5_NUM,
};

static struct sprd_clk_desc roc1_g5_pll_desc = {
	.clk_clks	= roc1_g5_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_g5_pll_clks),
	.hw_clks	= &roc1_g5_pll_hws,
};

/* pll at g9 */
static struct freq_table mpll2_ftable[6] = {
	{ .ibias = 0, .max_freq = 1200000000ULL },
	{ .ibias = 1, .max_freq = 1400000000ULL },
	{ .ibias = 2, .max_freq = 1600000000ULL },
	{ .ibias = 3, .max_freq = 1800000000ULL },
	{ .ibias = 4, .max_freq = 2000000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_mpll2[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 112,	.width = 1 },	/* div_s	*/
	{ .shift = 113,	.width = 1 },	/* mod_en	*/
	{ .shift = 122,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 5,	.width = 3 },	/* icp		*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 87,	.width = 7 },	/* nint		*/
	{ .shift = 64,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 144,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll2_clk, "mpll2", "mpll2-gate", 0x9c,
				   5, mpll2_ftable, f_mpll2, 240,
				   1000, 1000, 1, 1000000000ULL);

static struct sprd_clk_common *roc1_g9_pll_clks[] = {
	/* address base is 0x32410000 */
	&mpll2_clk.common,
};

static struct clk_hw_onecell_data roc1_g9_pll_hws = {
	.hws	= {
		[CLK_MPLL2]		= &mpll2_clk.common.hw,
	},
	.num	= CLK_ANLG_PHY_G9_NUM,
};

static struct sprd_clk_desc roc1_g9_pll_desc = {
	.clk_clks	= roc1_g9_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_g9_pll_clks),
	.hw_clks	= &roc1_g9_pll_hws,
};

/* pll at g12 */
static struct clk_bit_field f_twpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 2,	.width = 1 },	/* div_s	*/
	{ .shift = 3,	.width = 1 },	/* mod_en	*/
	{ .shift = 4,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 0 },	/* icp		*/
	{ .shift = 10,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(twpll_clk, "twpll", "ext-26m", 0x14,
				   2, ftable, f_twpll, 240);
static CLK_FIXED_FACTOR(twpll_768m, "twpll-768m", "twpll", 2, 1, 0);
static CLK_FIXED_FACTOR(twpll_384m, "twpll-384m", "twpll", 4, 1, 0);
static CLK_FIXED_FACTOR(twpll_192m, "twpll-192m", "twpll", 8, 1, 0);
static CLK_FIXED_FACTOR(twpll_96m, "twpll-96m", "twpll", 16, 1, 0);
static CLK_FIXED_FACTOR(twpll_48m, "twpll-48m", "twpll", 32, 1, 0);
static CLK_FIXED_FACTOR(twpll_24m, "twpll-24m", "twpll", 64, 1, 0);
static CLK_FIXED_FACTOR(twpll_12m, "twpll-12m", "twpll", 128, 1, 0);
static CLK_FIXED_FACTOR(twpll_512m, "twpll-512m", "twpll", 3, 1, 0);
static CLK_FIXED_FACTOR(twpll_256m, "twpll-256m", "twpll", 6, 1, 0);
static CLK_FIXED_FACTOR(twpll_128m, "twpll-128m", "twpll", 12, 1, 0);
static CLK_FIXED_FACTOR(twpll_64m, "twpll-64m", "twpll", 24, 1, 0);
static CLK_FIXED_FACTOR(twpll_307m2, "twpll-307m2", "twpll", 5, 1, 0);
static CLK_FIXED_FACTOR(twpll_219m4, "twpll-219m4", "twpll", 7, 1, 0);
static CLK_FIXED_FACTOR(twpll_170m6, "twpll-170m6", "twpll", 9, 1, 0);
static CLK_FIXED_FACTOR(twpll_153m6, "twpll-153m6", "twpll", 10, 1, 0);
static CLK_FIXED_FACTOR(twpll_76m8, "twpll-76m8", "twpll", 20, 1, 0);
static CLK_FIXED_FACTOR(twpll_51m2, "twpll-51m2", "twpll", 30, 1, 0);
static CLK_FIXED_FACTOR(twpll_38m4, "twpll-38m4", "twpll", 40, 1, 0);
static CLK_FIXED_FACTOR(twpll_19m2, "twpll-19m2", "twpll", 80, 1, 0);

static SPRD_PLL_WITH_ITABLE_1K(lpll_clk, "lpll", "lpll-gate", 0x20,
				   2, ftable, f_twpll, 240);
static CLK_FIXED_FACTOR(lpll_614m4, "lpll-614m4", "lpll", 2, 1, 0);
static CLK_FIXED_FACTOR(lpll_409m6, "lpll-409m6", "lpll", 3, 1, 0);
static CLK_FIXED_FACTOR(lpll_245m76, "lpll-245m76", "lpll", 5, 1, 0);

static struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 1,	.width = 1 },	/* div_s	*/
	{ .shift = 2,	.width = 1 },	/* mod_en	*/
	{ .shift = 3,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 0 },	/* icp		*/
	{ .shift = 9,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 66,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(gpll_clk, "gpll", "gpll-gate", 0x2c,
				   3, ftable, f_gpll, 240,
				   1000, 1000, 1, 750000000ULL);

static struct clk_bit_field f_isppll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 75,	.width = 1 },	/* div_s	*/
	{ .shift = 64,	.width = 1 },	/* mod_en	*/
	{ .shift = 65,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 0 },	/* icp		*/
	{ .shift = 5,	.width = 11 },	/* n		*/
	{ .shift = 57,	.width = 7 },	/* nint		*/
	{ .shift = 34,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 96,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(isppll_clk, "isppll", "isppll-gate", 0x40,
				   3, ftable, f_isppll, 240,
				   1000, 1000, 1, 750000000ULL);
static CLK_FIXED_FACTOR(isppll_702m, "isppll-702m", "isppll", 2, 1, 0);
static CLK_FIXED_FACTOR(isppll_468m, "isppll-468m", "isppll", 3, 1, 0);

static struct clk_bit_field f_apll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 34,	.width = 1 },	/* div_s	*/
	{ .shift = 51,	.width = 1 },	/* mod_en	*/
	{ .shift = 82,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 36,	.width = 3 },	/* icp		*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 23,	.width = 7 },	/* nint		*/
	{ .shift = 0,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 39,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(apll_clk, "apll", "apll-gate", 0x78,
				   3, ftable, f_apll, 240,
				   1000, 1000, 1, 750000000ULL);

static struct sprd_clk_common *roc1_g12_pll_clks[] = {
	/* address base is 0x323e0000 */
	&twpll_clk.common,
	&lpll_clk.common,
	&gpll_clk.common,
	&isppll_clk.common,
	&apll_clk.common,
};

static struct clk_hw_onecell_data roc1_g12_pll_hws = {
	.hws	= {
		[CLK_TWPLL]		= &twpll_clk.common.hw,
		[CLK_TWPLL_768M]	= &twpll_768m.hw,
		[CLK_TWPLL_384M]	= &twpll_384m.hw,
		[CLK_TWPLL_192M]	= &twpll_192m.hw,
		[CLK_TWPLL_96M]		= &twpll_96m.hw,
		[CLK_TWPLL_48M]		= &twpll_48m.hw,
		[CLK_TWPLL_24M]		= &twpll_24m.hw,
		[CLK_TWPLL_12M]		= &twpll_12m.hw,
		[CLK_TWPLL_512M]	= &twpll_512m.hw,
		[CLK_TWPLL_256M]	= &twpll_256m.hw,
		[CLK_TWPLL_128M]	= &twpll_128m.hw,
		[CLK_TWPLL_64M]		= &twpll_64m.hw,
		[CLK_TWPLL_307M2]	= &twpll_307m2.hw,
		[CLK_TWPLL_219M4]	= &twpll_219m4.hw,
		[CLK_TWPLL_170M6]	= &twpll_170m6.hw,
		[CLK_TWPLL_153M6]	= &twpll_153m6.hw,
		[CLK_TWPLL_76M8]	= &twpll_76m8.hw,
		[CLK_TWPLL_51M2]	= &twpll_51m2.hw,
		[CLK_TWPLL_38M4]	= &twpll_38m4.hw,
		[CLK_TWPLL_19M2]	= &twpll_19m2.hw,
		[CLK_LPLL]		= &lpll_clk.common.hw,
		[CLK_LPLL_614M4]	= &lpll_614m4.hw,
		[CLK_LPLL_409M6]	= &lpll_409m6.hw,
		[CLK_LPLL_245M76]	= &lpll_245m76.hw,
		[CLK_GPLL]		= &gpll_clk.common.hw,
		[CLK_ISPPLL]		= &isppll_clk.common.hw,
		[CLK_ISPPLL_702M]	= &isppll_702m.hw,
		[CLK_ISPPLL_468M]	= &isppll_468m.hw,
		[CLK_APLL]		= &apll_clk.common.hw,
	},
	.num	= CLK_ANLG_PHY_G12_NUM,
};

static struct sprd_clk_desc roc1_g12_pll_desc = {
	.clk_clks	= roc1_g12_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_g12_pll_clks),
	.hw_clks	= &roc1_g12_pll_hws,
};

/* pll at G17 */
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll0_clk, "mpll0", "mpll0-gate", 0x0,
				   5, mpll_ftable, f_mpll1, 240,
				   1000, 1000, 1, 1200000000ULL);
static struct sprd_clk_common *roc1_g17_pll_clks[] = {
	/* address base is 0x32418000 */
	&mpll0_clk.common,
};

static struct clk_hw_onecell_data roc1_g17_pll_hws = {
	.hws	= {
		[CLK_MPLL0]	= &mpll0_clk.common.hw,
	},
	.num	= CLK_ANLG_PHY_G17_NUM,
};

static struct sprd_clk_desc roc1_g17_pll_desc = {
	.clk_clks	= roc1_g17_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_g17_pll_clks),
	.hw_clks	= &roc1_g17_pll_hws,
};

/* ap clocks */
#define ROC1_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

static const char * const ap_apb_parents[] = { "ext-26m", "twpll-64m",
					       "twpll-96m", "twpll-128m" };
static SPRD_MUX_CLK(ap_apb_clk, "ap-apb-clk", ap_apb_parents, 0x20,
			0, 2, ROC1_MUX_FLAG);

static SPRD_MUX_CLK(ap_icu_clk, "ap-icu-clk", ap_apb_parents, 0x24,
			0, 2, ROC1_MUX_FLAG);

static const char * const ap_uart_parents[] = { "ext-26m", "twpll-48m",
					"twpll-51m2", "twpll-96m" };
static SPRD_COMP_CLK(ap_uart0_clk,	"ap-uart0-clk",	ap_uart_parents, 0x28,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_uart1_clk,	"ap-uart1-clk",	ap_uart_parents, 0x2c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_uart2_clk,	"ap-uart2-clk",	ap_uart_parents, 0x30,
		     0, 2, 8, 3, 0);

static const char * const i2c_parents[] = { "ext-26m", "twpll-48m",
					    "twpll-51m2", "twpll-153m6" };
static SPRD_COMP_CLK(ap_i2c0_clk, "ap-i2c0-clk", i2c_parents, 0x34,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c1_clk, "ap-i2c1-clk", i2c_parents, 0x38,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c2_clk, "ap-i2c2-clk", i2c_parents, 0x3c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c3_clk, "ap-i2c3-clk", i2c_parents, 0x40,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c4_clk, "ap-i2c4-clk", i2c_parents, 0x44,
		     0, 2, 8, 3, 0);

static const char * const spi_parents[] = { "ext-26m", "twpll-128m",
					"twpll-153m6", "twpll-192m" };
static SPRD_COMP_CLK(ap_spi0_clk, "ap-spi0-clk", spi_parents, 0x48,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi1_clk, "ap-spi1-clk", spi_parents, 0x4c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi2_clk, "ap-spi2-clk", spi_parents, 0x50,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi3_clk, "ap-spi3-clk", spi_parents, 0x54,
		     0, 2, 8, 3, 0);

static const char * const iis_parents[] = { "ext-26m",
					    "twpll-128m",
					    "twpll-153m6" };
static SPRD_COMP_CLK(ap_iis0_clk, "ap-iis0-clk", iis_parents, 0x58,
		     0, 2, 8, 6, 0);
static SPRD_COMP_CLK(ap_iis1_clk, "ap-iis1-clk", iis_parents, 0x5c,
		     0, 2, 8, 6, 0);
static SPRD_COMP_CLK(ap_iis2_clk, "ap-iis2-clk", iis_parents, 0x60,
		     0, 2, 8, 6, 0);

static const char * const sim_parents[] = { "ext-26m", "twpll-51m2",
					"twpll-64m", "twpll-96m",
					"twpll-128m"};
static SPRD_COMP_CLK(ap_sim_clk, "ap-sim-clk", sim_parents, 0x64,
		     0, 3, 8, 3, 0);

static const char * const ap_ce_parents[] = { "ext-26m", "twpll-96m",
					"twpll-192m", "twpll-256m"};
static SPRD_MUX_CLK(ap_ce_clk, "ap-ce-clk", ap_ce_parents, 0x68,
			0, 2, ROC1_MUX_FLAG);

static const char * const sdio_parents[] = { "ext-1m", "ext-26m",
					     "twpll-307m2", "twpll-384m",
					     "rpll", "lpll-409m6" };
static SPRD_MUX_CLK(sdio0_2x_clk, "sdio0-2x", sdio_parents, 0x70,
			0, 3, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(sdio1_2x_clk, "sdio1-2x", sdio_parents, 0x78,
			0, 3, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(emmc_2x_clk, "emmc-2x", sdio_parents, 0x80,
			0, 3, ROC1_MUX_FLAG);

static SPRD_DIV_CLK(ufs_clk, "ufs-clk", "ext-26m", 0x94,
		    8, 6, 0);

static const char * const vsp_parents[] = { "twpll-256m", "twpll-307m2",
					"twpll-384m", "twpll-512m" };
static SPRD_MUX_CLK(vsp_clk, "vsp-clk", vsp_parents, 0x9c,
			0, 2, ROC1_MUX_FLAG);

static const char * const dispc0_parents[] = { "twpll-153m6", "twpll-192m",
					"twpll-256m", "twpll-307m2",
					"twpll-384m", "isppll-468m" };
static SPRD_MUX_CLK(dispc0_clk, "dispc0-clk", dispc0_parents, 0xa0,
			0, 3, ROC1_MUX_FLAG);

static const char * const dispc0_dpi_parents[] = { "twpll-128m",
					"twpll-153m6", "twpll-192m",
					"twpll-307m2" };
static SPRD_COMP_CLK(dispc0_dpi_clk, "dispc0-dpi-clk", dispc0_dpi_parents,
		     0xa4, 0, 2, 8, 4, 0);

static SPRD_GATE_CLK(dsi_rxesc, "dsi-rxesc", "ap-apb-clk", 0xa8,
		     BIT(16), 0, 0);

static SPRD_GATE_CLK(dsi_lanebyte, "dsi-lanebyte", "ap-apb-clk", 0xac,
		     BIT(16), 0, 0);

static const char * const vdsp_parents[] = { "twpll-192m", "twpll-307m2",
					"isppll-468m", "lpll-614m4",
					"isppll-702m", "twpll-768m" };
static SPRD_MUX_CLK(vdsp_clk, "vdsp-clk", vdsp_parents, 0xb0,
			0, 3, ROC1_MUX_FLAG);
static SPRD_DIV_CLK(vdsp_edap, "vdsp-edap", "vdsp-clk", 0xb4,
		    8, 2, 0);
static SPRD_DIV_CLK(vdsp_m0, "vdsp-m0", "vdsp-clk", 0xb8,
		    8, 2, 0);

static struct sprd_clk_common *roc1_ap_clks[] = {
	/* address base is 0x20200000 */
	&ap_apb_clk.common,
	&ap_icu_clk.common,
	&ap_uart0_clk.common,
	&ap_uart1_clk.common,
	&ap_uart2_clk.common,
	&ap_i2c0_clk.common,
	&ap_i2c1_clk.common,
	&ap_i2c2_clk.common,
	&ap_i2c3_clk.common,
	&ap_i2c4_clk.common,
	&ap_spi0_clk.common,
	&ap_spi1_clk.common,
	&ap_spi2_clk.common,
	&ap_spi3_clk.common,
	&ap_iis0_clk.common,
	&ap_iis1_clk.common,
	&ap_iis2_clk.common,
	&ap_sim_clk.common,
	&ap_ce_clk.common,
	&sdio0_2x_clk.common,
	&sdio1_2x_clk.common,
	&emmc_2x_clk.common,
	&ufs_clk.common,
	&vsp_clk.common,
	&dispc0_clk.common,
	&dispc0_dpi_clk.common,
	&dsi_rxesc.common,
	&dsi_lanebyte.common,
	&vdsp_clk.common,
	&vdsp_edap.common,
	&vdsp_m0.common,
};

static struct clk_hw_onecell_data roc1_ap_clk_hws = {
	.hws	= {
		[CLK_AP_APB] = &ap_apb_clk.common.hw,
		[CLK_ICU] = &ap_icu_clk.common.hw,
		[CLK_AP_UART0] = &ap_uart0_clk.common.hw,
		[CLK_AP_UART1] = &ap_uart1_clk.common.hw,
		[CLK_AP_UART2] = &ap_uart2_clk.common.hw,
		[CLK_AP_I2C0] = &ap_i2c0_clk.common.hw,
		[CLK_AP_I2C1] = &ap_i2c1_clk.common.hw,
		[CLK_AP_I2C2] = &ap_i2c2_clk.common.hw,
		[CLK_AP_I2C3] = &ap_i2c3_clk.common.hw,
		[CLK_AP_I2C4] = &ap_i2c4_clk.common.hw,
		[CLK_AP_SPI0] = &ap_spi0_clk.common.hw,
		[CLK_AP_SPI1] = &ap_spi1_clk.common.hw,
		[CLK_AP_SPI2] = &ap_spi2_clk.common.hw,
		[CLK_AP_SPI3] = &ap_spi3_clk.common.hw,
		[CLK_AP_IIS0] = &ap_iis0_clk.common.hw,
		[CLK_AP_IIS1] = &ap_iis1_clk.common.hw,
		[CLK_AP_IIS2] = &ap_iis2_clk.common.hw,
		[CLK_AP_SIM] = &ap_sim_clk.common.hw,
		[CLK_AP_CE] = &ap_ce_clk.common.hw,
		[CLK_AP_SDIO0_2X] = &sdio0_2x_clk.common.hw,
		[CLK_AP_SDIO1_2X] = &sdio1_2x_clk.common.hw,
		[CLK_AP_EMMC_2X] = &emmc_2x_clk.common.hw,
		[CLK_AP_UFS] = &ufs_clk.common.hw,
		[CLK_AP_VSP] = &vsp_clk.common.hw,
		[CLK_AP_DISPC0] = &dispc0_clk.common.hw,
		[CLK_AP_DISPC0_DPI] = &dispc0_dpi_clk.common.hw,
		[CLK_AP_DSI_RXESC] = &dsi_rxesc.common.hw,
		[CLK_AP_DSI_LANEBYTE] = &dsi_lanebyte.common.hw,
		[CLK_AP_VDSP] = &vdsp_clk.common.hw,
		[CLK_AP_VDSP_EDAP] = &vdsp_edap.common.hw,
		[CLK_AP_VDSP_M0] = &vdsp_m0.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static struct sprd_clk_desc roc1_ap_clk_desc = {
	.clk_clks	= roc1_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_ap_clks),
	.hw_clks	= &roc1_ap_clk_hws,
};

/* ipa gate */
static SPRD_SC_GATE_CLK(ipa_usb_eb, "ipa-usb-eb", "ext-26m", 0x4,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_usb_suspend_eb, "ipa-usb-suspend-eb", "ext-26m",
		     0x4, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_usb_ref_eb, "ipa-usb-ref-eb", "ext-26m", 0x4,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_top_eb, "ipa-top-eb", "ext-26m", 0x4,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pam_usb_eb, "pam-usb-eb", "ext-26m", 0x4,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pam_ipa_eb, "pam-ipa-eb", "ext-26m", 0x4,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pam_wifi_eb, "pam-wifi-eb", "ext-26m", 0x4,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_pcie3_eb, "ipa-pcie3-eb", "ext-26m", 0x4,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_pcie2_eb, "ipa-pcie2-eb", "ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bp_pam_u3_eb, "bp-pam-u3-eb", "ext-26m", 0x4,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bp_pam_ipa_eb, "bp-pam-ipa-eb", "ext-26m", 0x4,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bp_pam_top_eb, "bp-pam-top-eb", "ext-26m", 0x4,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_intc_eb, "ipa-intc-eb", "ext-26m", 0x4,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_bm_dbg_eb, "ipa-bm-dbg-eb", "ext-26m", 0x4,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_uart_eb, "ipa-uart-eb", "ext-26m", 0x4,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_timer_eb, "ipa-timer-eb", "ext-26m", 0x4,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_wdg_eb, "ipa-wdg-eb", "ext-26m", 0x4,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_cm4_eb, "ipa-cm4-eb", "ext-26m", 0x4,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_ipa_gate_clks[] = {
	/* address base is 0x21040000 */
	&ipa_usb_eb.common,
	&ipa_usb_suspend_eb.common,
	&ipa_usb_ref_eb.common,
	&ipa_top_eb.common,
	&pam_usb_eb.common,
	&pam_ipa_eb.common,
	&pam_wifi_eb.common,
	&ipa_pcie3_eb.common,
	&ipa_pcie2_eb.common,
	&bp_pam_u3_eb.common,
	&bp_pam_ipa_eb.common,
	&bp_pam_top_eb.common,
	&ipa_intc_eb.common,
	&ipa_bm_dbg_eb.common,
	&ipa_uart_eb.common,
	&ipa_timer_eb.common,
	&ipa_wdg_eb.common,
	&ipa_cm4_eb.common,
};

static struct clk_hw_onecell_data roc1_ipa_gate_clk_hws = {
	.hws	= {
		[CLK_IPA_USB_EB] = &ipa_usb_eb.common.hw,
		[CLK_IPA_USB_SUSPEND_EB] = &ipa_usb_suspend_eb.common.hw,
		[CLK_IPA_USB_REF_EB] = &ipa_usb_ref_eb.common.hw,
		[CLK_IPA_TOP_EB] = &ipa_top_eb.common.hw,
		[CLK_PAM_USB_EB] = &pam_usb_eb.common.hw,
		[CLK_PAM_IPA_EB] = &pam_ipa_eb.common.hw,
		[CLK_PAM_WIFI_EB] = &pam_wifi_eb.common.hw,
		[CLK_IPA_PCIE3_EB] = &ipa_pcie3_eb.common.hw,
		[CLK_IPA_PCIE2_EB] = &ipa_pcie2_eb.common.hw,
		[CLK_BP_PAM_U3_EB] = &bp_pam_u3_eb.common.hw,
		[CLK_BP_PAM_IPA_EB] = &bp_pam_ipa_eb.common.hw,
		[CLK_BP_PAM_TOP_EB] = &bp_pam_top_eb.common.hw,
		[CLK_IPA_INTC_EB] = &ipa_intc_eb.common.hw,
		[CLK_IPA_BM_DBG_EB] = &ipa_bm_dbg_eb.common.hw,
		[CLK_IPA_UART_EB] = &ipa_uart_eb.common.hw,
		[CLK_IPA_TIMER_EB] = &ipa_timer_eb.common.hw,
		[CLK_IPA_WDG_EB] = &ipa_wdg_eb.common.hw,
		[CLK_IPA_CM4_EB] = &ipa_cm4_eb.common.hw,
	},
	.num	= CLK_IPA_GATE_NUM,
};

static struct sprd_clk_desc roc1_ipa_gate_desc = {
	.clk_clks	= roc1_ipa_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_ipa_gate_clks),
	.hw_clks	= &roc1_ipa_gate_clk_hws,
};

/* ipa clocks */
static const char * const ipa_core_parents[] = { "ext-26m", "twpll-192m",
					       "twpll-384m", "lpll-409m6" };
static SPRD_MUX_CLK(ipa_core_clk, "ipa-core-clk", ipa_core_parents, 0x20,
			0, 2, ROC1_MUX_FLAG);
static SPRD_DIV_CLK(ipa_mtx_clk, "ipa-mtx-clk", "ipa-core-clk", 0x24,
		    8, 2, 0);
static SPRD_DIV_CLK(ipa_apb_clk, "ipa-apb-clk", "ipa-core-clk", 0x28,
		    8, 2, 0);

static const char * const pcie2_aux_parents[] = { "clk-2m", "rco-2m" };
static SPRD_MUX_CLK(pcie2_aux_clk, "pcie2-aux-clk", pcie2_aux_parents, 0x2c,
		     0, 1, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(pcie3_aux_clk, "pcie3-aux-clk", pcie2_aux_parents, 0x30,
		     0, 1, ROC1_MUX_FLAG);

static const char * const usb_ref_parents[] = { "ext-32k", "twpll-24m" };
static SPRD_MUX_CLK(usb_ref_clk, "usb-ref-clk", usb_ref_parents, 0x34,
		     0, 1, ROC1_MUX_FLAG);

static SPRD_GATE_CLK(usb_pipe, "usb-pipe", "ext-26m", 0x38,
		     BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(usb_utmi, "usb-utmi", "ext-26m", 0x3c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(pcie2_pipe, "pcie2-pipe", "ext-26m", 0x40,
		     BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(pcie3_pipe, "pcie3-pipe", "ext-26m", 0x44,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const ipa_uart_parents[] = { "ext-26m", "twpll-128m" };
static SPRD_COMP_CLK(ipa_uart_clk, "ipa-uart-clk", ipa_uart_parents, 0x4c,
		     0, 1, 8, 3, 0);

static struct sprd_clk_common *roc1_ipa_clks[] = {
	/* address base is 0x21050000 */
	&ipa_core_clk.common,
	&ipa_mtx_clk.common,
	&ipa_apb_clk.common,
	&pcie2_aux_clk.common,
	&pcie3_aux_clk.common,
	&usb_ref_clk.common,
	&usb_pipe.common,
	&usb_utmi.common,
	&pcie2_pipe.common,
	&pcie3_pipe.common,
	&ipa_uart_clk.common,
};

static struct clk_hw_onecell_data roc1_ipa_clk_hws = {
	.hws	= {
		[CLK_IPA_CORE] = &ipa_core_clk.common.hw,
		[CLK_IPA_MTX]  = &ipa_mtx_clk.common.hw,
		[CLK_IPA_APB]  = &ipa_apb_clk.common.hw,
		[CLK_PCIE2_AUX] = &pcie2_aux_clk.common.hw,
		[CLK_PCIE3_AUX] = &pcie3_aux_clk.common.hw,
		[CLK_USB_REF]   = &usb_ref_clk.common.hw,
		[CLK_USB_PIPE]  = &usb_pipe.common.hw,
		[CLK_USB_UTMI]  = &usb_utmi.common.hw,
		[CLK_PCIE2_PIPE] = &pcie2_pipe.common.hw,
		[CLK_PCIE3_PIPE] = &pcie3_pipe.common.hw,
		[CLK_IPA_UART]  = &ipa_uart_clk.common.hw,
	},
	.num	= CLK_IPA_CLK_NUM,
};

static struct sprd_clk_desc roc1_ipa_clk_desc = {
	.clk_clks	= roc1_ipa_clks,
	.num_clk_clks	= ARRAY_SIZE(roc1_ipa_clks),
	.hw_clks	= &roc1_ipa_clk_hws,
};

/* aon apb clocks -- 0x32080000 */
static const char * const aon_apb_parents[] = { "rco-4m", "clk-4m",
					"clk-13m", "rco-25m", "ext-26m",
					"twpll-96m", "rco-100m",
					"twpll-128m"};
static SPRD_COMP_CLK(aon_apb_clk, "aon-apb-clk", aon_apb_parents, 0x220,
		     0, 3, 8, 2, 0);

static const char * const adi_parents[] = { "rco-4m", "ext-26m",
					"rco-25m", "twpll-38m4",
					"twpll-51m2"};
static SPRD_MUX_CLK(adi_clk, "adi-clk", adi_parents, 0x224,
			0, 3, ROC1_MUX_FLAG);

static const char * const aux_parents[] = { "ext-32k", "ext-26m" };
static SPRD_COMP_CLK(aux0_clk, "aux0-clk", aux_parents, 0x228,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK(aux1_clk, "aux1-clk", aux_parents, 0x22c,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK(aux2_clk, "aux2-clk", aux_parents, 0x230,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK(probe_clk, "probe-clk", aux_parents, 0x234,
		     0, 1, 8, 4, 0);

static const char * const pwm_parents[] = { "clk-32k", "ext-26m",
					"rco-4m", "rco-25m", "twpll-48m" };
static SPRD_MUX_CLK(pwm0_clk, "pwm0-clk", pwm_parents, 0x238,
			0, 3, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(pwm1_clk, "pwm1-clk", pwm_parents, 0x23c,
			0, 3, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(pwm2_clk, "pwm2-clk", pwm_parents, 0x240,
			0, 3, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(pwm3_clk, "pwm3-clk", pwm_parents, 0x244,
			0, 3, ROC1_MUX_FLAG);

static const char * const efuse_parents[] = { "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(efuse_clk, "efuse-clk", efuse_parents, 0x248,
			0, 1, ROC1_MUX_FLAG);

static const char * const uart_parents[] = { "rco-4m", "ext-26m",
					"twpll-48m", "twpll-51m2",
					"twpll-96m", "rco-100m",
					"twpll-128m" };
static SPRD_MUX_CLK(uart0_clk, "uart0-clk", uart_parents, 0x24c,
			0, 3, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(uart1_clk, "uart1-clk", uart_parents, 0x250,
			0, 3, ROC1_MUX_FLAG);

static const char * const aon_thm_parents[] = { "ext-32k", "ext-250k" };
static SPRD_MUX_CLK(thm0_clk, "thm0-clk", aon_thm_parents, 0x260,
			0, 1, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(thm1_clk, "thm1-clk", aon_thm_parents, 0x264,
			0, 1, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(thm2_clk, "thm2-clk", aon_thm_parents, 0x268,
			0, 1, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(thm3_clk, "thm3-clk", aon_thm_parents, 0x26c,
			0, 1, ROC1_MUX_FLAG);

static const char * const aon_i2c_parents[] = { "rco-4m", "ext-26m",
					"twpll-48m", "twpll-51m2",
					"rco-100m", "twpll-153m6" };
static SPRD_MUX_CLK(aon_i2c_clk, "aon-i2c-clk", aon_i2c_parents, 0x27c,
			0, 3, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(aon_i2c1_clk, "aon-i2c1-clk", aon_i2c_parents, 0x280,
			0, 3, ROC1_MUX_FLAG);

static const char * const aon_iis_parents[] = { "ext-26m", "twpll-128m",
					"twpll-153m6" };
static SPRD_MUX_CLK(aon_iis_clk, "aon-iis-clk", aon_iis_parents, 0x284,
			0, 2, ROC1_MUX_FLAG);

static const char * const scc_parents[] = { "ext-26m", "twpll-48m",
					"twpll-51m2", "twpll-96m" };
static SPRD_MUX_CLK(scc_clk, "scc-clk", scc_parents, 0x288,
			0, 2, ROC1_MUX_FLAG);

static const char * const apcpu_dap_parents[] = { "ext-26m", "rco-4m",
					"twpll-76m8", "rco-100m",
					"twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(apcpu_dap_clk, "apcpu-dap-clk", apcpu_dap_parents, 0x28c,
			0, 3, ROC1_MUX_FLAG);
static SPRD_GATE_CLK(apcpu_dap_mtck, "apcpu-dap-mtck", "aon-apb-clk", 0x290,
		     BIT(16), 0, 0);

static const char * const apcpu_ts_parents[] = { "ext-32k", "ext-26m",
					"twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(apcpu_ts_clk, "apcpu-ts-clk", apcpu_ts_parents, 0x294,
			0, 2, ROC1_MUX_FLAG);

static const char * const debug_ts_parents[] = { "ext-26m", "twpll-76m8",
					"twpll-128m", "twpll-192m" };
static SPRD_MUX_CLK(debug_ts_clk, "debug-ts-clk", debug_ts_parents, 0x298,
			0, 2, ROC1_MUX_FLAG);

static SPRD_GATE_CLK(dsi_test_s, "dsi-test-s", "aon-apb-clk", 0x29c,
		     BIT(16), 0, 0);

static const char * const djtag_tck_parents[] = { "rco-4m", "ext-26m" };
static SPRD_MUX_CLK(djtag_tck_clk, "djtag-tck-clk", djtag_tck_parents, 0x2b8,
			0, 1, ROC1_MUX_FLAG);

static SPRD_GATE_CLK(djtag_tck_hw, "djtag-tck-hw", "aon-apb-clk", 0x2bc,
		     BIT(16), 0, 0);

static const char * const aon_tmr_parents[] = { "rco-4m", "rco-25m",
						"ext-26m" };
static SPRD_MUX_CLK(aon_tmr_clk, "aon-tmr-clk", aon_tmr_parents, 0x2c4,
			0, 2, ROC1_MUX_FLAG);

static const char * const aon_pmu_parents[] = { "ext-32k", "rco-4m", "ext-4m" };
static SPRD_MUX_CLK(aon_pmu_clk, "aon-pmu-clk", aon_pmu_parents, 0x2cc,
			0, 2, ROC1_MUX_FLAG);

static const char * const debounce_parents[] = { "ext-32k", "rco-4m",
						"rco-25m", "ext-26m" };
static SPRD_MUX_CLK(debounce_clk, "debounce-clk", debounce_parents, 0x2d0,
			0, 2, ROC1_MUX_FLAG);

static const char * const apcpu_pmu_parents[] = { "ext-26m", "twpll-76m8",
						"rco-100m", "twpll-128m" };
static SPRD_MUX_CLK(apcpu_pmu_clk, "apcpu-pmu-clk", apcpu_pmu_parents, 0x2d4,
			0, 2, ROC1_MUX_FLAG);

static const char * const top_dvfs_parents[] = { "ext-26m", "twpll-96m",
						"rco-100m", "twpll-128m" };
static SPRD_MUX_CLK(top_dvfs_clk, "top-dvfs-clk", top_dvfs_parents, 0x2dc,
			0, 2, ROC1_MUX_FLAG);

static const char * const pmu_26m_parents[] = { "rco-20m", "ext-26m" };
static SPRD_MUX_CLK(pmu_26m_clk, "pmu-26m-clk", pmu_26m_parents, 0x2e0,
			0, 1, ROC1_MUX_FLAG);

static SPRD_GATE_CLK(otg_utmi, "otg-utmi", "aon-apb-clk", 0x2e4,
		     BIT(16), 0, 0);

static const char * const otg_ref_parents[] = { "twpll-12m", "ext-26m" };
static SPRD_MUX_CLK(otg_ref_clk, "otg-ref-clk", otg_ref_parents, 0x2e8,
			0, 1, ROC1_MUX_FLAG);

static const char * const cssys_parents[] = { "rco-25m", "ext-26m", "rco-100m",
					"twpll-153m6", "twpll-384m",
					"twpll-512m" };
static SPRD_COMP_CLK(cssys_clk,	"cssys-clk",	cssys_parents, 0x2ec,
		     0, 3, 8, 2, 0);
static SPRD_DIV_CLK(cssys_pub_clk, "cssys-pub-clk", "cssys-clk", 0x2f0,
		    8, 2, 0);
static SPRD_DIV_CLK(cssys_apb_clk, "cssys-apb-clk", "cssys-clk", 0x2f4,
		    8, 2, 0);

static const char * const ap_axi_parents[] = { "ext-26m", "twpll-76m8",
					"twpll-128m", "twpll-256m" };
static SPRD_MUX_CLK(ap_axi_clk, "ap-axi-clk", ap_axi_parents, 0x2fc,
			0, 2, ROC1_MUX_FLAG);

static const char * const ap_mm_parents[] = { "ext-26m", "twpll-96m",
					"twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(ap_mm_clk, "ap-mm-clk", ap_mm_parents, 0x300,
			0, 2, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(ap_gpu_clk, "ap-gpu-clk", ap_mm_parents, 0x304,
			0, 2, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(ap_ai_clk, "ap-ai-clk", ap_mm_parents, 0x308,
			0, 2, ROC1_MUX_FLAG);

static const char * const sdio2_2x_parents[] = { "ext-1m", "ext-26m",
					"twpll-307m2", "twpll-384m",
					"rpll", "lpll-409m6" };
static SPRD_MUX_CLK(sdio2_2x_clk, "sdio2-2x-clk", sdio2_2x_parents, 0x30c,
			0, 3, ROC1_MUX_FLAG);

static const char * const analog_io_apb_parents[] = { "ext-26m", "twpll-48m" };
static SPRD_COMP_CLK(analog_io_apb, "analog-io-apb", analog_io_apb_parents,
		     0x314, 0, 1, 8, 2, 0);

static const char * const dmc_ref_parents[] = { "ext-6m5", "ext-13m",
					"ext-26m" };
static SPRD_MUX_CLK(dmc_ref_clk, "dmc-ref-clk", dmc_ref_parents, 0x318,
			0, 2, ROC1_MUX_FLAG);

static const char * const emc_parents[] = { "ext-26m", "twpll-384m",
					"twpll-512m", "twpll-768m", "twpll" };
static SPRD_MUX_CLK(emc_clk, "emc-clk", emc_parents, 0x320,
			0, 3, ROC1_MUX_FLAG);

static const char * const usb_parents[] = { "rco-25m", "ext-26m", "isppll-78m",
					"twpll-96m", "rco-100m", "twpll-128m" };
static SPRD_COMP_CLK(usb_clk, "usb-clk", usb_parents, 0x324,
		     0, 3, 8, 2, 0);

static const char * const efuse_ese_parents[] = { "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(efuse_ese_clk, "efuse-ese-clk", efuse_ese_parents, 0x328,
			0, 1, ROC1_MUX_FLAG);

static SPRD_DIV_CLK(aapc_test_clk, "aapc-test-clk", "ext-1m", 0x32c,
		    8, 4, 0);

static const char * const usb_suspend_parents[] = { "ext-32k", "ext-1m" };
static SPRD_MUX_CLK(usb_suspend_clk, "usb-suspend-clk", usb_suspend_parents,
			0x330, 0, 2, ROC1_MUX_FLAG);

/* twpll should be ese-128m ???? */
static SPRD_DIV_CLK(ese_sys_clk, "ese-sys-clk", "twpll-128m", 0x338,
		    8, 3, 0);

static struct sprd_clk_common *roc1_aon_apb[] = {
	/* address base is 0x32080200 */
	&aon_apb_clk.common,
	&adi_clk.common,
	&aux0_clk.common,
	&aux1_clk.common,
	&aux2_clk.common,
	&probe_clk.common,
	&pwm0_clk.common,
	&pwm1_clk.common,
	&pwm2_clk.common,
	&pwm3_clk.common,
	&efuse_clk.common,
	&uart0_clk.common,
	&uart1_clk.common,
	&thm0_clk.common,
	&thm1_clk.common,
	&thm2_clk.common,
	&thm3_clk.common,
	&aon_i2c_clk.common,
	&aon_i2c1_clk.common,
	&aon_iis_clk.common,
	&scc_clk.common,
	&apcpu_dap_clk.common,
	&apcpu_dap_mtck.common,
	&apcpu_ts_clk.common,
	&debug_ts_clk.common,
	&dsi_test_s.common,
	&djtag_tck_clk.common,
	&djtag_tck_hw.common,
	&aon_tmr_clk.common,
	&aon_pmu_clk.common,
	&debounce_clk.common,
	&apcpu_pmu_clk.common,
	&top_dvfs_clk.common,
	&pmu_26m_clk.common,
	&otg_utmi.common,
	&otg_ref_clk.common,
	&cssys_clk.common,
	&cssys_pub_clk.common,
	&cssys_apb_clk.common,
	&ap_axi_clk.common,
	&ap_mm_clk.common,
	&ap_gpu_clk.common,
	&ap_ai_clk.common,
	&sdio2_2x_clk.common,
	&analog_io_apb.common,
	&dmc_ref_clk.common,
	&emc_clk.common,
	&usb_clk.common,
	&efuse_ese_clk.common,
	&aapc_test_clk.common,
	&usb_suspend_clk.common,
	&ese_sys_clk.common,
};

static struct clk_hw_onecell_data roc1_aon_apb_hws = {
	.hws	= {
		[CLK_AON_APB] = &aon_apb_clk.common.hw,
		[CLK_AON_ADI] = &adi_clk.common.hw,
		[CLK_AON_AUX0] = &aux0_clk.common.hw,
		[CLK_AON_AUX1] = &aux1_clk.common.hw,
		[CLK_AON_AUX2] = &aux2_clk.common.hw,
		[CLK_AON_PROBE] = &probe_clk.common.hw,
		[CLK_AON_PWM0] = &pwm0_clk.common.hw,
		[CLK_AON_PWM1] = &pwm1_clk.common.hw,
		[CLK_AON_PWM2] = &pwm2_clk.common.hw,
		[CLK_AON_PWM3] = &pwm3_clk.common.hw,
		[CLK_AON_EFUSE] = &efuse_clk.common.hw,
		[CLK_AON_UART0] = &uart0_clk.common.hw,
		[CLK_AON_UART1] = &uart1_clk.common.hw,
		[CLK_AON_THM0] = &thm0_clk.common.hw,
		[CLK_AON_THM1] = &thm1_clk.common.hw,
		[CLK_AON_THM2] = &thm2_clk.common.hw,
		[CLK_AON_THM3] = &thm3_clk.common.hw,
		[CLK_AON_I2C] = &aon_i2c_clk.common.hw,
		[CLK_AON_I2C1] = &aon_i2c1_clk.common.hw,
		[CLK_AON_I2S] = &aon_iis_clk.common.hw,
		[CLK_AON_SCC] = &scc_clk.common.hw,
		[CLK_APCPU_DAP] = &apcpu_dap_clk.common.hw,
		[CLK_APCPU_DAP_MTCK] = &apcpu_dap_mtck.common.hw,
		[CLK_APCPU_TS] = &apcpu_ts_clk.common.hw,
		[CLK_DEBUG_TS] = &debug_ts_clk.common.hw,
		[CLK_DSI_TEST_S] = &dsi_test_s.common.hw,
		[CLK_DJTAG_TCK] = &djtag_tck_clk.common.hw,
		[CLK_DJTAG_TCK_HW] = &djtag_tck_hw.common.hw,
		[CLK_AON_TMR] = &aon_tmr_clk.common.hw,
		[CLK_PMU] = &aon_pmu_clk.common.hw,
		[CLK_DEBOUNCE] = &debounce_clk.common.hw,
		[CLK_APCPU_PMU] = &apcpu_pmu_clk.common.hw,
		[CLK_TOP_DVFS] = &top_dvfs_clk.common.hw,
		[CLK_26M_PMU] = &pmu_26m_clk.common.hw,
		[CLK_OTG_UTMI] = &otg_utmi.common.hw,
		[CLK_OTG_REF] = &otg_ref_clk.common.hw,
		[CLK_CSSYS] = &cssys_clk.common.hw,
		[CLK_CSSYS_PUB] = &cssys_pub_clk.common.hw,
		[CLK_CSSYS_APB] = &cssys_apb_clk.common.hw,
		[CLK_AP_AXI] = &ap_axi_clk.common.hw,
		[CLK_AP_MM] = &ap_mm_clk.common.hw,
		[CLK_AP_GPU] = &ap_gpu_clk.common.hw,
		[CLK_AP_AI] = &ap_ai_clk.common.hw,
		[CLK_SDIO2_2X] = &sdio2_2x_clk.common.hw,
		[CLK_ANALOG_IO_APB] = &analog_io_apb.common.hw,
		[CLK_DMC_REF] = &dmc_ref_clk.common.hw,
		[CLK_EMC] = &emc_clk.common.hw,
		[CLK_USB] = &usb_clk.common.hw,
		[CLK_EFUSE_ESE] = &efuse_ese_clk.common.hw,
		[CLK_AAPC_TEST] = &aapc_test_clk.common.hw,
		[CLK_USB_SUSPEND] = &usb_suspend_clk.common.hw,
		[CLK_ESE_SYS] = &ese_sys_clk.common.hw,
	},
	.num	= CLK_AON_CLK_NUM,
};

static struct sprd_clk_desc roc1_aon_apb_desc = {
	.clk_clks	= roc1_aon_apb,
	.num_clk_clks	= ARRAY_SIZE(roc1_aon_apb),
	.hw_clks	= &roc1_aon_apb_hws,
};

/* audcp apb gates */
static SPRD_SC_GATE_CLK(audcp_wdg_eb,	"audcp-wdg-eb",	"ext-26m", 0x0,
		     0x100, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_rtc_wdg_eb, "audcp-rtc-wdg-eb", "ext-26m", 0x0,
		     0x100, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_tmr0_eb,	"audcp-tmr0-eb", "ext-26m", 0x0,
		     0x100, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_tmr1_eb,	"audcp-tmr1-eb", "ext-26m", 0x0,
		     0x100, BIT(6), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_audcpapb_gate[] = {
	/* address base is 0x3350d000 */
	&audcp_wdg_eb.common,
	&audcp_rtc_wdg_eb.common,
	&audcp_tmr0_eb.common,
	&audcp_tmr1_eb.common,
};

static struct clk_hw_onecell_data roc1_audcpapb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_WDG_EB]	= &audcp_wdg_eb.common.hw,
		[CLK_AUDCP_RTC_WDG_EB]	= &audcp_rtc_wdg_eb.common.hw,
		[CLK_AUDCP_TMR0_EB]	= &audcp_tmr0_eb.common.hw,
		[CLK_AUDCP_TMR1_EB]	= &audcp_tmr1_eb.common.hw,
	},
	.num	= CLK_AUDCP_APB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_audcpapb_gate_desc = {
	.clk_clks	= roc1_audcpapb_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_audcpapb_gate),
	.hw_clks	= &roc1_audcpapb_gate_hws,
};

/* audcp ahb gates */
static SPRD_SC_GATE_CLK(audcp_iis0_eb, "audcp-iis0-eb", "ext-26m", 0x0,
		     0x100, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_iis1_eb, "audcp-iis1-eb", "ext-26m", 0x0,
		     0x100, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_iis2_eb, "audcp-iis2-eb", "ext-26m", 0x0,
		     0x100, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_iis3_eb, "audcp-iis3-eb", "ext-26m", 0x0,
		     0x100, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_uart_eb,	"audcp-uart-eb", "ext-26m", 0x0,
		     0x100, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_dma_cp_eb, "audcp-dma-cp-eb", "ext-26m", 0x0,
		     0x100, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_dma_ap_eb, "audcp-dma-ap-eb", "ext-26m", 0x0,
		     0x100, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_src48k_eb, "audcp-src48k-eb", "ext-26m", 0x0,
		     0x100, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_mcdt_eb,  "audcp-mcdt-eb", "ext-26m", 0x0,
		     0x100, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_vbcifd_eb, "audcp-vbcifd-eb", "ext-26m", 0x0,
		     0x100, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_vbc_eb,   "audcp-vbc-eb", "ext-26m", 0x0,
		     0x100, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_splk_eb,  "audcp-splk-eb", "ext-26m", 0x0,
		     0x100, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_icu_eb,   "audcp-icu-eb", "ext-26m", 0x0,
		     0x100, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_ap_ashb_eb, "dma-ap-ashb-eb", "ext-26m", 0x0,
		     0x100, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_cp_ashb_eb, "dma-cp-ashb-eb", "ext-26m", 0x0,
		     0x100, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_aud_eb,   "audcp-aud-eb", "ext-26m", 0x0,
		     0x100, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_vbc_24m_eb, "audcp-vbc-24m-eb", "ext-26m", 0x0,
		     0x100, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_tmr_26m_eb, "audcp-tmr-26m-eb", "ext-26m", 0x0,
		     0x100, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_dvfs_ashb_eb, "audcp-dvfs-ashb-eb", "ext-26m",
		     0x0, 0x100, BIT(23), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_audcpahb_gate[] = {
	/* address base is 0x335e0000 */
	&audcp_iis0_eb.common,
	&audcp_iis1_eb.common,
	&audcp_iis2_eb.common,
	&audcp_iis3_eb.common,
	&audcp_uart_eb.common,
	&audcp_dma_cp_eb.common,
	&audcp_dma_ap_eb.common,
	&audcp_src48k_eb.common,
	&audcp_mcdt_eb.common,
	&audcp_vbcifd_eb.common,
	&audcp_vbc_eb.common,
	&audcp_splk_eb.common,
	&audcp_icu_eb.common,
	&dma_ap_ashb_eb.common,
	&dma_cp_ashb_eb.common,
	&audcp_aud_eb.common,
	&audcp_vbc_24m_eb.common,
	&audcp_tmr_26m_eb.common,
	&audcp_dvfs_ashb_eb.common,
};

static struct clk_hw_onecell_data roc1_audcpahb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_IIS0_EB]	= &audcp_iis0_eb.common.hw,
		[CLK_AUDCP_IIS1_EB]	= &audcp_iis1_eb.common.hw,
		[CLK_AUDCP_IIS2_EB]	= &audcp_iis2_eb.common.hw,
		[CLK_AUDCP_IIS3_EB]	= &audcp_iis3_eb.common.hw,
		[CLK_AUDCP_UART_EB]	= &audcp_uart_eb.common.hw,
		[CLK_AUDCP_DMA_CP_EB]	= &audcp_dma_cp_eb.common.hw,
		[CLK_AUDCP_DMA_AP_EB]	= &audcp_dma_ap_eb.common.hw,
		[CLK_AUDCP_SRC48K_EB]	= &audcp_src48k_eb.common.hw,
		[CLK_AUDCP_MCDT_EB]	= &audcp_mcdt_eb.common.hw,
		[CLK_AUDCP_VBCIFD_EB]	= &audcp_vbcifd_eb.common.hw,
		[CLK_AUDCP_VBC_EB]	= &audcp_vbc_eb.common.hw,
		[CLK_AUDCP_SPLK_EB]	= &audcp_splk_eb.common.hw,
		[CLK_AUDCP_ICU_EB]	= &audcp_icu_eb.common.hw,
		[CLK_AUDCP_DMA_AP_ASHB_EB] = &dma_ap_ashb_eb.common.hw,
		[CLK_AUDCP_DMA_CP_ASHB_EB] = &dma_cp_ashb_eb.common.hw,
		[CLK_AUDCP_AUD_EB]	= &audcp_aud_eb.common.hw,
		[CLK_AUDCP_VBC_24M_EB]	= &audcp_vbc_24m_eb.common.hw,
		[CLK_AUDCP_TMR_26M_EB]	= &audcp_tmr_26m_eb.common.hw,
		[CLK_AUDCP_DVFS_ASHB_EB] = &audcp_dvfs_ashb_eb.common.hw,
	},
	.num	= CLK_AUDCP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_audcpahb_gate_desc = {
	.clk_clks	= roc1_audcpahb_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_audcpahb_gate),
	.hw_clks	= &roc1_audcpahb_gate_hws,
};

/* gpu clocks */
static SPRD_GATE_CLK(gpu_core_gate, "gpu-core-gate", "ap-mm-clk", 0x4,
			BIT(0), CLK_IGNORE_UNUSED, 0);

static const char * const gpu_parents[] = { "ext-26m", "twpll-384m",
					"twpll-512m", "lpll-614m4",
					"twpll-768m", "gpll" };
static SPRD_COMP_CLK(gpu_core_clk, "gpu-core-clk", gpu_parents, 0x4,
		     4, 3, 8, 3, 0);

static SPRD_GATE_CLK(gpu_mem_gate, "gpu-mem-gate", "ap-mm-clk", 0x8,
			BIT(0), CLK_IGNORE_UNUSED, 0);

static SPRD_COMP_CLK(gpu_mem_clk, "gpu-mem-clk", gpu_parents, 0x8,
		     4, 3, 8, 3, 0);

static SPRD_GATE_CLK(gpu_sys_gate, "gpu-sys-gate", "ap-mm-clk", 0xc,
			BIT(0), CLK_IGNORE_UNUSED, 0);

static SPRD_DIV_CLK(gpu_sys_clk, "gpu-sys-clk", "gpu-mem-clk", 0xc,
		    4, 3, 0);

static struct sprd_clk_common *roc1_gpu_clk[] = {
	/* address base is 0x60100000 */
	&gpu_core_gate.common,
	&gpu_core_clk.common,
	&gpu_mem_gate.common,
	&gpu_mem_clk.common,
	&gpu_sys_gate.common,
	&gpu_sys_clk.common,
};

static struct clk_hw_onecell_data roc1_gpu_clk_hws = {
	.hws	= {
		[CLK_GPU_CORE_EB] = &gpu_core_gate.common.hw,
		[CLK_GPU_CORE] = &gpu_core_clk.common.hw,
		[CLK_GPU_MEM_EB] = &gpu_mem_gate.common.hw,
		[CLK_GPU_MEM] = &gpu_mem_clk.common.hw,
		[CLK_GPU_SYS_EB] = &gpu_sys_gate.common.hw,
		[CLK_GPU_SYS] = &gpu_sys_clk.common.hw,
	},
	.num	= CLK_GPU_CLK_NUM,
};

static struct sprd_clk_desc roc1_gpu_clk_desc = {
	.clk_clks	= roc1_gpu_clk,
	.num_clk_clks	= ARRAY_SIZE(roc1_gpu_clk),
	.hw_clks	= &roc1_gpu_clk_hws,
};

/* mm clocks */
static const char * const mm_ahb_parents[] = { "ext-26m", "twpll-96m",
					       "twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(mm_ahb_clk, "mm-ahb-clk", mm_ahb_parents, 0x20,
			0, 2, ROC1_MUX_FLAG);

static const char * const mm_mtx_parents[] = { "twpll-76m8", "twpll-128m",
					"twpll-256m", "twpll-307m2",
					"twpll-384m", "isppll-468m",
					"twpll-512m" };
static SPRD_MUX_CLK(mm_mtx_clk, "mm-mtx-clk", mm_mtx_parents, 0x24,
			0, 3, ROC1_MUX_FLAG);

static const char * const sensor_parents[] = { "ext-26m", "twpll-48m",
					"twpll-76m8", "twpll-96m" };
static SPRD_COMP_CLK(sensor0_clk, "sensor0-clk", sensor_parents, 0x28,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(sensor1_clk, "sensor1-clk", sensor_parents, 0x2c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(sensor2_clk, "sensor2-clk", sensor_parents, 0x30,
		     0, 2, 8, 3, 0);

static const char * const cpp_parents[] = { "twpll-76m8", "twpll-128m",
					"twpll-256m", "twpll-384m" };
static SPRD_MUX_CLK(cpp_clk, "cpp-clk", cpp_parents, 0x34,
			0, 2, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(jpg_clk, "jpg-clk", cpp_parents, 0x38,
			0, 2, ROC1_MUX_FLAG);

static const char * const fd_parents[] = { "twpll-76m8", "twpll-192m",
					"twpll-307m2", "twpll-384m" };
static SPRD_MUX_CLK(fd_clk, "fd-clk", fd_parents, 0x3c,
			0, 2, ROC1_MUX_FLAG);

static const char * const dcam_if_parents[] = { "twpll-192m", "twpll-256m",
					"twpll-307m2", "twpll-384m" };
static SPRD_MUX_CLK(dcam_if_clk, "dcam-if-clk", dcam_if_parents, 0x40,
			0, 2, ROC1_MUX_FLAG);

static const char * const dcam_axi_parents[] = { "twpll-256m", "twpll-307m2",
					"twpll-384m", "lpll-468m" };
static SPRD_MUX_CLK(dcam_axi_clk, "dcam-axi-clk", dcam_axi_parents, 0x44,
			0, 2, ROC1_MUX_FLAG);

static const char * const isp_parents[] = { "twpll-256m", "twpll-307m2",
					"twpll-384m", "isppll-468m",
					"twpll-512m" };
static SPRD_MUX_CLK(isp_clk, "isp-clk", isp_parents, 0x48,
			0, 3, ROC1_MUX_FLAG);

static SPRD_GATE_CLK(mipi_csi0, "mipi-csi0", "mm-ahb-clk", 0x4c,
			BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(mipi_csi1, "mipi-csi1", "mm-ahb-clk", 0x50,
			BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(mipi_csi2, "mipi-csi2", "mm-ahb-clk", 0x54,
			BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_mm_clk[] = {
	/* address base is 0x62100000 */
	&mm_ahb_clk.common,
	&mm_mtx_clk.common,
	&sensor0_clk.common,
	&sensor1_clk.common,
	&sensor2_clk.common,
	&cpp_clk.common,
	&jpg_clk.common,
	&fd_clk.common,
	&dcam_if_clk.common,
	&dcam_axi_clk.common,
	&isp_clk.common,
	&mipi_csi0.common,
	&mipi_csi1.common,
	&mipi_csi2.common,
};

static struct clk_hw_onecell_data roc1_mm_clk_hws = {
	.hws	= {
		[CLK_MM_AHB] = &mm_ahb_clk.common.hw,
		[CLK_MM_MTX] = &mm_mtx_clk.common.hw,
		[CLK_SENSOR0] = &sensor0_clk.common.hw,
		[CLK_SENSOR1] = &sensor1_clk.common.hw,
		[CLK_SENSOR2] = &sensor2_clk.common.hw,
		[CLK_CPP] = &cpp_clk.common.hw,
		[CLK_JPG] = &jpg_clk.common.hw,
		[CLK_FD] = &fd_clk.common.hw,
		[CLK_DCAM_IF] = &dcam_if_clk.common.hw,
		[CLK_DCAM_AXI] = &dcam_axi_clk.common.hw,
		[CLK_ISP] = &isp_clk.common.hw,
		[CLK_MIPI_CSI0] = &mipi_csi0.common.hw,
		[CLK_MIPI_CSI1] = &mipi_csi1.common.hw,
		[CLK_MIPI_CSI2] = &mipi_csi2.common.hw,
	},
	.num	= CLK_MM_CLK_NUM,
};

static struct sprd_clk_desc roc1_mm_clk_desc = {
	.clk_clks	= roc1_mm_clk,
	.num_clk_clks	= ARRAY_SIZE(roc1_mm_clk),
	.hw_clks	= &roc1_mm_clk_hws,
};

/* mm gate clocks */
static SPRD_SC_GATE_CLK(mm_cpp_eb, "mm-cpp-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_jpg_eb, "mm-jpg-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_dcam_eb, "mm-dcam-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_isp_eb, "mm-isp-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_csi2_eb, "mm-csi2-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_csi1_eb, "mm-csi1-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_csi0_eb, "mm-csi0-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_ckg_eb, "mm-ckg-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_isp_ahb_eb, "mm-isp-ahb-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_dvfs_eb, "mm-dvfs-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_fd_eb, "mm-fd-eb", "mm_ahb_clk", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_sensor2_en, "mm-sensor2-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_sensor1_en, "mm-sensor1-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_sensor0_en, "mm-sensor0-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_mipi_csi2_en, "mm-mipi-csi2-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_mipi_csi1_en, "mm-mipi-csi1-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_mipi_csi0_en, "mm-mipi-csi0-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_dcam_axi_en, "mm-dcam-axi-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_isp_axi_en, "mm-isp-axi-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_cphy_en, "mm-cphy-en", "mm_ahb_clk", 0x8,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_mm_gate_clk[] = {
	/* address base is 0x62200000 */
	&mm_cpp_eb.common,
	&mm_jpg_eb.common,
	&mm_dcam_eb.common,
	&mm_isp_eb.common,
	&mm_csi2_eb.common,
	&mm_csi1_eb.common,
	&mm_csi0_eb.common,
	&mm_ckg_eb.common,
	&mm_isp_ahb_eb.common,
	&mm_dvfs_eb.common,
	&mm_fd_eb.common,
	&mm_sensor2_en.common,
	&mm_sensor1_en.common,
	&mm_sensor0_en.common,
	&mm_mipi_csi2_en.common,
	&mm_mipi_csi1_en.common,
	&mm_mipi_csi0_en.common,
	&mm_dcam_axi_en.common,
	&mm_isp_axi_en.common,
	&mm_cphy_en.common,
};

static struct clk_hw_onecell_data roc1_mm_gate_clk_hws = {
	.hws	= {
		[CLK_MM_CPP_EB] = &mm_cpp_eb.common.hw,
		[CLK_MM_JPG_EB] = &mm_jpg_eb.common.hw,
		[CLK_MM_DCAM_EB] = &mm_dcam_eb.common.hw,
		[CLK_MM_ISP_EB] = &mm_isp_eb.common.hw,
		[CLK_MM_CSI2_EB] = &mm_csi2_eb.common.hw,
		[CLK_MM_CSI1_EB] = &mm_csi1_eb.common.hw,
		[CLK_MM_CSI0_EB] = &mm_csi0_eb.common.hw,
		[CLK_MM_CKG_EB] = &mm_ckg_eb.common.hw,
		[CLK_ISP_AHB_EB] = &mm_isp_ahb_eb.common.hw,
		[CLK_MM_DVFS_EB] = &mm_dvfs_eb.common.hw,
		[CLK_MM_FD_EB] = &mm_fd_eb.common.hw,
		[CLK_MM_SENSOR2_EB] = &mm_sensor2_en.common.hw,
		[CLK_MM_SENSOR1_EB] = &mm_sensor1_en.common.hw,
		[CLK_MM_SENSOR0_EB] = &mm_sensor0_en.common.hw,
		[CLK_MM_MIPI_CSI2_EB] = &mm_mipi_csi2_en.common.hw,
		[CLK_MM_MIPI_CSI1_EB] = &mm_mipi_csi1_en.common.hw,
		[CLK_MM_MIPI_CSI0_EB] = &mm_mipi_csi0_en.common.hw,
		[CLK_DCAM_AXI_EB] = &mm_dcam_axi_en.common.hw,
		[CLK_ISP_AXI_EB] = &mm_isp_axi_en.common.hw,
		[CLK_MM_CPHY_EB] = &mm_cphy_en.common.hw,
	},
	.num	= CLK_MM_GATE_CLK_NUM,
};

static struct sprd_clk_desc roc1_mm_gate_clk_desc = {
	.clk_clks	= roc1_mm_gate_clk,
	.num_clk_clks	= ARRAY_SIZE(roc1_mm_gate_clk),
	.hw_clks	= &roc1_mm_gate_clk_hws,
};

/* ai gate */
static SPRD_SC_GATE_CLK(ai_top_apb_eb, "ai-top-apb-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_dvfs_apb_eb, "ai-dvfs-apb-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_mmu_apb_eb, "ai-mmu-apb-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_cambricon_eb, "ai-cambricon-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_powervr_eb, "ai-powervr-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_ai_gate[] = {
	/* address base is 0x6fd00000 */
	&ai_top_apb_eb.common,
	&ai_dvfs_apb_eb.common,
	&ai_mmu_apb_eb.common,
	&ai_cambricon_eb.common,
	&ai_powervr_eb.common,
};

static struct clk_hw_onecell_data roc1_ai_gate_hws = {
	.hws	= {
		[CLK_AI_TOP_APB_EB] = &ai_top_apb_eb.common.hw,
		[CLK_AI_DVFS_APB_EB] = &ai_dvfs_apb_eb.common.hw,
		[CLK_AI_MMU_APB_EB] = &ai_mmu_apb_eb.common.hw,
		[CLK_AI_CAMBRICON_EB] = &ai_cambricon_eb.common.hw,
		[CLK_AI_POWERVR_EB] = &ai_powervr_eb.common.hw,
	},
	.num	= CLK_AI_GATE_NUM,
};

static struct sprd_clk_desc roc1_ai_gate_desc = {
	.clk_clks	= roc1_ai_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_ai_gate),
	.hw_clks	= &roc1_ai_gate_hws,
};

/* ai clocks */
static const char * const cambricon_clk_parents[] = { "twpll-512m",
					"twpll-768m", "apll" };
static SPRD_COMP_CLK(cambricon_clk, "cambricon-clk",
		     cambricon_clk_parents, 0x20,
		     0, 2, 8, 3, 0);

static const char * const powervr_clk_parents[] = { "twpll-512m",
					"apll" };
static SPRD_COMP_CLK(powervr_clk, "powervr-clk",
		     powervr_clk_parents, 0x24, 0, 1, 8, 3, 0);

static const char * const ai_mtx_parents[] = { "ext-26m", "twpll-512m",
					"twpll-768m", "apll" };
static SPRD_COMP_CLK(ai_mtx_clk, "ai-mtx-clk",
		     ai_mtx_parents, 0x28, 0, 2, 8, 3, 0);

static const char * const ai_cfg_parents[] = { "ext-26m", "twpll-153m6" };
static SPRD_MUX_CLK(cambricon_cfg_clk, "cambricon-cfg-clk", ai_cfg_parents,
			0x30, 0, 1, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(powervr_cfg_clk, "powervr-cfg-clk", ai_cfg_parents, 0x34,
			0, 1, ROC1_MUX_FLAG);
static SPRD_MUX_CLK(dvfs_ai_clk, "dvfs-ai-clk", ai_cfg_parents, 0x38,
			0, 1, ROC1_MUX_FLAG);

static struct sprd_clk_common *roc1_ai[] = {
	/* address base is 0x6fd04000 */
	&cambricon_clk.common,
	&powervr_clk.common,
	&ai_mtx_clk.common,
	&cambricon_cfg_clk.common,
	&powervr_cfg_clk.common,
	&dvfs_ai_clk.common,
};

static struct clk_hw_onecell_data roc1_ai_hws = {
	.hws	= {
		[CLK_CAMBRICON] = &cambricon_clk.common.hw,
		[CLK_POWERVR] = &powervr_clk.common.hw,
		[CLK_AI_MTX] = &ai_mtx_clk.common.hw,
		[CLK_CAMBRICON_CFG] = &cambricon_cfg_clk.common.hw,
		[CLK_POWERVR_CFG] = &powervr_cfg_clk.common.hw,
		[CLK_DVFS_AI] = &dvfs_ai_clk.common.hw,
	},
	.num	= CLK_AI_NUM,
};

static struct sprd_clk_desc roc1_ai_desc = {
	.clk_clks	= roc1_ai,
	.num_clk_clks	= ARRAY_SIZE(roc1_ai),
	.hw_clks	= &roc1_ai_hws,
};

/* ap ahb gates */
static SPRD_SC_GATE_CLK(dsi_eb,		"dsi-eb",	"ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dispc_eb,	"dispc-eb",	"ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vsp_eb,		"vsp-eb",	"ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdma_eb,	"vdma-eb",	"ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pub_dma_eb,	"pub-dma-eb",	"ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sec_dma_eb,	"sec-dma-eb",	"ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(icu_eb,		"icu-eb",	"ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apahb_ckg_eb,	"apahb-ckg-eb",	"ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bm_clk_eb,	"bm-clk-eb",	"ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_apahb_gate[] = {
	/* address base is 0x20100000 */
	&dsi_eb.common,
	&dispc_eb.common,
	&vsp_eb.common,
	&vdma_eb.common,
	&pub_dma_eb.common,
	&sec_dma_eb.common,
	&icu_eb.common,
	&apahb_ckg_eb.common,
	&bm_clk_eb.common,
};

static struct clk_hw_onecell_data roc1_apahb_gate_hws = {
	.hws	= {
		[CLK_DSI_EB]		= &dsi_eb.common.hw,
		[CLK_DISPC_EB]		= &dispc_eb.common.hw,
		[CLK_VSP_EB]		= &vsp_eb.common.hw,
		[CLK_VDMA_EB]		= &vdma_eb.common.hw,
		[CLK_DMA_PUB_EB]	= &pub_dma_eb.common.hw,
		[CLK_DMA_SEC_EB]	= &sec_dma_eb.common.hw,
		[CLK_ICU_EB]		= &icu_eb.common.hw,
		[CLK_AP_AHB_CKG_EB]	= &apahb_ckg_eb.common.hw,
		[CLK_BUSM_CLK_EB]	= &bm_clk_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_apahb_gate_desc = {
	.clk_clks	= roc1_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_apahb_gate),
	.hw_clks	= &roc1_apahb_gate_hws,
};

/* aon apb gates */
static SPRD_SC_GATE_CLK(rc100m_cal_eb,	"rc100m-cal-eb",	"ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_tck_eb,	"djtag-tck-eb",		"ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb,	"djtag-eb",		"ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb,	"aux0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux1_eb,	"aux1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux2_eb,	"aux2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(probe_eb,	"probe-eb",	"ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_eb,		"ipa-eb",	"ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_eb,		"mm-eb",	"ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb,		"gpu-eb",	"ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mspi_eb,	"mspi-eb",	"ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_eb,		"ai-eb",	"ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_dap_eb,	"apcpu-dap-eb",	"ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_cssys_eb,	"aon-cssys-eb",	"ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_apb_eb,	"cssys-apb-eb",	"ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_pub_eb,	"cssys-pub-eb",	"ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdphy_cfg_eb,	"sdphy-cfg-eb",	"ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdphy_ref_eb,	"sdphy-ref-eb",	"ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(efuse_eb,	"efuse-eb",	"ext-26m", 0x4,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpio_eb,	"gpio-eb",	"ext-26m", 0x4,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb,	"mbox-eb",	"ext-26m", 0x4,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_eb,		"kpd-eb",	"ext-26m", 0x4,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_eb,	"aon-syst-eb",	"ext-26m", 0x4,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_eb,	"ap-syst-eb",	"ext-26m", 0x4,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_eb,	"aon-tmr-eb",	"ext-26m", 0x4,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_utmi_eb,	"otg-utmi-eb",	"ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_phy_eb,	"otg-phy-eb",	"ext-26m", 0x4,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(splk_eb,	"splk-eb",	"ext-26m", 0x4,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_eb,		"pin-eb",	"ext-26m", 0x4,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ana_eb,		"ana-eb",	"ext-26m", 0x4,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aonapb_ckg_eb,	"aonapb-ckg-eb", "ext-26m", 0x4,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_i2c1_eb,	"aon-i2c1-eb",	"ext-26m", 0x4,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ufs_ao_eb,	"ufs-ao-eb",	"ext-26m", 0x4,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm3_eb,	"thm3-eb",	"ext-26m", 0x4,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_ts0_eb,	"apcpu-ts0-eb",	"ext-26m", 0x4,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debug_filter_eb, "debug-filter-eb",	"ext-26m", 0x4,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_iis_eb, "aon-iis-eb", "ext-26m", 0x4,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(scc_eb, "scc-eb", "ext-26m", 0x4,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm0_eb,	"thm0-eb",	"ext-26m", 0x8,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm1_eb,	"thm1-eb",	"ext-26m", 0x8,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm2_eb,	"thm2-eb",	"ext-26m", 0x8,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(asim_top_eb,	"asim-top-eb",	"ext-26m", 0x8,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_i2c0_eb,	"aon-i2c0-eb",	"ext-26m", 0x8,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_eb,		"pmu-eb",	"ext-26m", 0x8,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(adi_eb, "adi-eb", "ext-26m", 0x8,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_eb,		"eic-eb",	"ext-26m", 0x8,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc0_eb,	"ap-intc0-eb",	"ext-26m", 0x8,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc1_eb,	"ap-intc1-eb",	"ext-26m", 0x8,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc2_eb,	"ap-intc2-eb",	"ext-26m", 0x8,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc3_eb,	"ap-intc3-eb",	"ext-26m", 0x8,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc4_eb,	"ap-intc4-eb",	"ext-26m", 0x8,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc5_eb,	"ap-intc5-eb",	"ext-26m", 0x8,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_intc_eb,	"audcp-intc-eb",	"ext-26m", 0x8,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_eb,	"ap-tmr0-eb",	"ext-26m", 0x8,
			       0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb,	"ap-tmr1-eb",	"ext-26m", 0x8,
			       0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb,	"ap-tmr2-eb",	"ext-26m", 0x8,
			       0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm0_eb,	"pwm0-eb",	"ext-26m", 0x8,
			       0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm1_eb,	"pwm1-eb",	"ext-26m", 0x8,
			       0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm2_eb,	"pwm2-eb",	"ext-26m", 0x8,
			       0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm3_eb,	"pwm3-eb",	"ext-26m", 0x8,
			       0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb,	"ap-wdg-eb",	"ext-26m", 0x8,
			       0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_wdg_eb,	"apcpu-wdg-eb",	"ext-26m", 0x8,
			       0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes_eb,	"serdes-eb",	"ext-26m", 0x8,
			       0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(arch_rtc_eb,	"arch-rtc-eb",	"ext-26m", 0x18,
			       0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_rtc_eb,	"kpd-rtc-eb",	"ext-26m", 0x18,
			       0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_rtc_eb,	"ap-syst-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_rtc_eb,	"aon-tmr-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtc_eb,	"eic-rtc-eb",	"ext-26m", 0x18,
			       0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtcdv5_eb,	"eic-rtcdv5-eb", "ext-26m", 0x18,
			       0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_rtc_eb,	"ap-wdg-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ac_wdg_rtc_eb,	"ac-wdg-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_rtc_eb,	"ap-tmr0-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_rtc_eb,	"ap-tmr1-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_rtc_eb,	"ap-tmr2-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcxo_lc_rtc_eb,	"dcxo-lc-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bb_cal_rtc_eb,	"bb-cal-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi_csi_test_eb, "dsi-csi-test-eb", "ext-26m", 0x138,
			       0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_tck_en, "djtag-tck-en", "ext-26m", 0x138,
			       0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dphy_ref_eb, "dphy-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_ref_eb, "dmc-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_ref_eb, "otg-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tsen_eb, "tsen-eb", "ext-26m", 0x138,
			       0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tmr_eb, "tmr-eb", "ext-26m", 0x138,
			       0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100m_ref_eb, "rc100m-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100m_fdk_eb, "rc100m-fdk-eb", "ext-26m", 0x138,
			       0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debounce_eb, "debounce-eb", "ext-26m", 0x138,
			       0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(det_32k_eb, "det-32k-eb", "ext-26m", 0x138,
			       0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb_suspend_en, "usb_suspend_en", "ext-26m", 0x138,
			       0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ufs_ao_en, "ufs_ao_en", "ext-26m", 0x138,
			       0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(top_cssys_en, "top-cssys-en", "ext-26m", 0x13c,
			       0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_axi_en, "ap-axi-en", "ext-26m", 0x13c,
			       0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_2x_en, "sdio0-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_1x_en, "sdio0-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_2x_en, "sdio1-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_1x_en, "sdio1-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_2x_en, "sdio2-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_1x_en, "sdio2-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_2x_en, "emmc-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_1x_en, "emmc-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pll_test_en, "pll-test-en", "ext-26m", 0x13c,
			       0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cphy_cfg_en, "cphy-cfg-en", "ext-26m", 0x13c,
			       0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aapc_clk_test_en, "aapc-clk-test-en", "ext-26m", 0x13c,
			       0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ese_efuse_ctrl_en, "ese_efuse_ctrl_en", "ext-26m", 0x13c,
			       0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debug_ts_en, "debug-ts-en", "ext-26m", 0x13c,
			       0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_aon_gate[] = {
	/* address base is 0x327d0000 */
	&rc100m_cal_eb.common,
	&djtag_tck_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&ipa_eb.common,
	&mm_eb.common,
	&gpu_eb.common,
	&mspi_eb.common,
	&ai_eb.common,
	&apcpu_dap_eb.common,
	&aon_cssys_eb.common,
	&cssys_apb_eb.common,
	&cssys_pub_eb.common,
	&sdphy_cfg_eb.common,
	&sdphy_ref_eb.common,
	&efuse_eb.common,
	&gpio_eb.common,
	&mbox_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&otg_utmi_eb.common,
	&otg_phy_eb.common,
	&splk_eb.common,
	&pin_eb.common,
	&ana_eb.common,
	&aonapb_ckg_eb.common,
	&aon_i2c1_eb.common,
	&ufs_ao_eb.common,
	&thm3_eb.common,
	&apcpu_ts0_eb.common,
	&debug_filter_eb.common,
	&aon_iis_eb.common,
	&scc_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&thm2_eb.common,
	&asim_top_eb.common,
	&aon_i2c0_eb.common,
	&pmu_eb.common,
	&adi_eb.common,
	&eic_eb.common,
	&ap_intc0_eb.common,
	&ap_intc1_eb.common,
	&ap_intc2_eb.common,
	&ap_intc3_eb.common,
	&ap_intc4_eb.common,
	&ap_intc5_eb.common,
	&audcp_intc_eb.common,
	&ap_tmr0_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&ap_wdg_eb.common,
	&apcpu_wdg_eb.common,
	&serdes_eb.common,
	&arch_rtc_eb.common,
	&kpd_rtc_eb.common,
	&aon_syst_rtc_eb.common,
	&ap_syst_rtc_eb.common,
	&aon_tmr_rtc_eb.common,
	&eic_rtc_eb.common,
	&eic_rtcdv5_eb.common,
	&ap_wdg_rtc_eb.common,
	&ac_wdg_rtc_eb.common,
	&ap_tmr0_rtc_eb.common,
	&ap_tmr1_rtc_eb.common,
	&ap_tmr2_rtc_eb.common,
	&dcxo_lc_rtc_eb.common,
	&bb_cal_rtc_eb.common,
	&dsi_csi_test_eb.common,
	&djtag_tck_en.common,
	&dphy_ref_eb.common,
	&dmc_ref_eb.common,
	&otg_ref_eb.common,
	&tsen_eb.common,
	&tmr_eb.common,
	&rc100m_ref_eb.common,
	&rc100m_fdk_eb.common,
	&debounce_eb.common,
	&det_32k_eb.common,
	&usb_suspend_en.common,
	&ufs_ao_en.common,
	&top_cssys_en.common,
	&ap_axi_en.common,
	&sdio0_2x_en.common,
	&sdio0_1x_en.common,
	&sdio1_2x_en.common,
	&sdio1_1x_en.common,
	&sdio2_2x_en.common,
	&sdio2_1x_en.common,
	&emmc_2x_en.common,
	&emmc_1x_en.common,
	&pll_test_en.common,
	&cphy_cfg_en.common,
	&aapc_clk_test_en.common,
	&ese_efuse_ctrl_en.common,
	&debug_ts_en.common,
};

static struct clk_hw_onecell_data roc1_aon_gate_hws = {
	.hws	= {
		[CLK_RC100M_CAL_EB]	= &rc100m_cal_eb.common.hw,
		[CLK_DJTAG_TCK_EB]	= &djtag_tck_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_IPA_EB]		= &ipa_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_GPU_EB]		= &gpu_eb.common.hw,
		[CLK_MSPI_EB]		= &mspi_eb.common.hw,
		[CLK_AI_EB]		= &ai_eb.common.hw,
		[CLK_APCPU_DAP_EB]	= &apcpu_dap_eb.common.hw,
		[CLK_AON_CSSYS_EB]	= &aon_cssys_eb.common.hw,
		[CLK_CSSYS_APB_EB]	= &cssys_apb_eb.common.hw,
		[CLK_CSSYS_PUB_EB]	= &cssys_pub_eb.common.hw,
		[CLK_SDPHY_CFG_EB]	= &sdphy_cfg_eb.common.hw,
		[CLK_SDPHY_REF_EB]	= &sdphy_ref_eb.common.hw,
		[CLK_EFUSE_EB]		= &efuse_eb.common.hw,
		[CLK_GPIO_EB]		= &gpio_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_KPD_EB]		= &kpd_eb.common.hw,
		[CLK_AON_SYST_EB]	= &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB]	= &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB]	= &aon_tmr_eb.common.hw,
		[CLK_OTG_UTMI_EB]	= &otg_utmi_eb.common.hw,
		[CLK_OTG_PHY_EB]	= &otg_phy_eb.common.hw,
		[CLK_SPLK_EB]		= &splk_eb.common.hw,
		[CLK_PIN_EB]		= &pin_eb.common.hw,
		[CLK_ANA_EB]		= &ana_eb.common.hw,
		[CLK_AON_APB_CKG_EB]	= &aonapb_ckg_eb.common.hw,
		[CLK_AON_I2C1_EB]	= &aon_i2c1_eb.common.hw,
		[CLK_UFS_AO_EB]		= &ufs_ao_eb.common.hw,
		[CLK_THM3_EB]		= &thm3_eb.common.hw,
		[CLK_APCPU_TS0_EB]	= &apcpu_ts0_eb.common.hw,
		[CLK_DEBUG_FILTER_EB]	= &debug_filter_eb.common.hw,
		[CLK_AON_IIS_EB]	= &aon_iis_eb.common.hw,
		[CLK_SCC_EB]		= &scc_eb.common.hw,
		[CLK_THM0_EB]		= &thm0_eb.common.hw,
		[CLK_THM1_EB]		= &thm1_eb.common.hw,
		[CLK_THM2_EB]		= &thm2_eb.common.hw,
		[CLK_ASIM_TOP_EB]	= &asim_top_eb.common.hw,
		[CLK_AON_I2C0_EB]	= &aon_i2c0_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_ADI_EB]		= &adi_eb.common.hw,
		[CLK_EIC_EB]		= &eic_eb.common.hw,
		[CLK_AP_INTC0_EB]	= &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB]	= &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB]	= &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB]	= &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB]	= &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB]	= &ap_intc5_eb.common.hw,
		[CLK_AUDCP_INTC_EB]	= &audcp_intc_eb.common.hw,
		[CLK_AP_TMR0_EB]	= &ap_tmr0_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_PWM0_EB]		= &pwm0_eb.common.hw,
		[CLK_PWM1_EB]		= &pwm1_eb.common.hw,
		[CLK_PWM2_EB]		= &pwm2_eb.common.hw,
		[CLK_PWM3_EB]		= &pwm3_eb.common.hw,
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_APCPU_WDG_EB]	= &apcpu_wdg_eb.common.hw,
		[CLK_SERDES_EB]		= &serdes_eb.common.hw,
		[CLK_ARCH_RTC_EB]	= &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB]	= &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB]	= &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB]	= &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB]	= &aon_tmr_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB]	= &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB]	= &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB]	= &ap_wdg_rtc_eb.common.hw,
		[CLK_AC_WDG_RTC_EB]	= &ac_wdg_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB]	= &ap_tmr0_rtc_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB]	= &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB]	= &ap_tmr2_rtc_eb.common.hw,
		[CLK_DCXO_LC_RTC_EB]	= &dcxo_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB]	= &bb_cal_rtc_eb.common.hw,
		[CLK_DSI_CSI_TEST_EB]	= &dsi_csi_test_eb.common.hw,
		[CLK_DJTAG_TCK_EN]	= &djtag_tck_en.common.hw,
		[CLK_DPHY_REF_EN]	= &dphy_ref_eb.common.hw,
		[CLK_DMC_REF_EN]	= &dmc_ref_eb.common.hw,
		[CLK_OTG_REF_EN]	= &otg_ref_eb.common.hw,
		[CLK_TSEN_EN]		= &tsen_eb.common.hw,
		[CLK_TMR_EN]		= &tmr_eb.common.hw,
		[CLK_RC100M_REF_EN]	= &rc100m_ref_eb.common.hw,
		[CLK_RC100M_FDK_EN]	= &rc100m_fdk_eb.common.hw,
		[CLK_DEBOUNCE_EN]	= &debounce_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_USB_SUSPEND_EN]	= &usb_suspend_en.common.hw,
		[CLK_UFS_AO_EN]		= &ufs_ao_en.common.hw,
		[CLK_TOP_CSSYS_EN]	= &top_cssys_en.common.hw,
		[CLK_AP_AXI_EN]		= &ap_axi_en.common.hw,
		[CLK_SDIO0_2X_EN]	= &sdio0_2x_en.common.hw,
		[CLK_SDIO0_1X_EN]	= &sdio0_1x_en.common.hw,
		[CLK_SDIO1_2X_EN]	= &sdio1_2x_en.common.hw,
		[CLK_SDIO1_1X_EN]	= &sdio1_1x_en.common.hw,
		[CLK_SDIO2_2X_EN]	= &sdio2_2x_en.common.hw,
		[CLK_SDIO2_1X_EN]	= &sdio2_1x_en.common.hw,
		[CLK_EMMC_2X_EN]	= &emmc_2x_en.common.hw,
		[CLK_EMMC_1X_EN]	= &emmc_1x_en.common.hw,
		[CLK_PLL_TEST_EN]	= &pll_test_en.common.hw,
		[CLK_CPHY_CFG_EN]	= &cphy_cfg_en.common.hw,
		[CLK_AAPC_CLK_TEST_EN]	= &aapc_clk_test_en.common.hw,
		[CLK_ESE_EFUSE_CTRL_EN]	= &ese_efuse_ctrl_en.common.hw,
		[CLK_DEBUG_TS_EN]	= &debug_ts_en.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_aon_gate_desc = {
	.clk_clks	= roc1_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_aon_gate),
	.hw_clks	= &roc1_aon_gate_hws,
};

/* ap apb gates */
static SPRD_SC_GATE_CLK(sim0_eb,	"sim0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis0_eb,	"iis0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis1_eb,	"iis1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis2_eb,	"iis2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apb_reg_eb,	"apb-reg-eb",	"ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_eb,	"spi0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_eb,	"spi1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_eb,	"spi2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi3_eb,	"spi3-eb",	"ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c0_eb,	"i2c0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c1_eb,	"i2c1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c2_eb,	"i2c2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c3_eb,	"i2c3-eb",	"ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c4_eb,	"i2c4-eb",	"ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart0_eb,	"uart0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart1_eb,	"uart1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart2_eb,	"uart2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_32k_eb,	"sim0-32k-eb",	"ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_lfin_eb,	"spi0-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_lfin_eb,	"spi1-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_lfin_eb,	"spi2-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi3_lfin_eb,	"spi3-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_eb,	"sdio0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_eb,	"sdio1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_eb,	"sdio2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_eb,	"emmc-eb",	"ext-26m", 0x0,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_sec_eb,	"ce-sec-eb",	"ext-26m", 0x0,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_pub_eb,	"ce-pub-eb",	"ext-26m", 0x0,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_apapb_gate[] = {
	/* address base is 0x71000000 */
	&sim0_eb.common,
	&iis0_eb.common,
	&iis1_eb.common,
	&iis2_eb.common,
	&apb_reg_eb.common,
	&spi0_eb.common,
	&spi1_eb.common,
	&spi2_eb.common,
	&spi3_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&i2c3_eb.common,
	&i2c4_eb.common,
	&uart0_eb.common,
	&uart1_eb.common,
	&uart2_eb.common,
	&sim0_32k_eb.common,
	&spi0_lfin_eb.common,
	&spi1_lfin_eb.common,
	&spi2_lfin_eb.common,
	&spi3_lfin_eb.common,
	&sdio0_eb.common,
	&sdio1_eb.common,
	&sdio2_eb.common,
	&emmc_eb.common,
	&ce_sec_eb.common,
	&ce_pub_eb.common,
};

static struct clk_hw_onecell_data roc1_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_IIS1_EB]		= &iis1_eb.common.hw,
		[CLK_IIS2_EB]		= &iis2_eb.common.hw,
		[CLK_AP_APB_REG_EB]	= &apb_reg_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_SPI1_EB]		= &spi1_eb.common.hw,
		[CLK_SPI2_EB]		= &spi2_eb.common.hw,
		[CLK_SPI3_EB]		= &spi3_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_I2C3_EB]		= &i2c3_eb.common.hw,
		[CLK_I2C4_EB]		= &i2c4_eb.common.hw,
		[CLK_UART0_EB]		= &uart0_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_UART2_EB]		= &uart2_eb.common.hw,
		[CLK_SIM0_32K_DET]	= &sim0_32k_eb.common.hw,
		[CLK_SPI0_LF_IN_EB]	= &spi0_lfin_eb.common.hw,
		[CLK_SPI1_LF_IN_EB]	= &spi1_lfin_eb.common.hw,
		[CLK_SPI2_LF_IN_EB]	= &spi2_lfin_eb.common.hw,
		[CLK_SPI3_LF_IN_EB]	= &spi3_lfin_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO2_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_CE_SEC_EB]		= &ce_sec_eb.common.hw,
		[CLK_CE_PUB_EB]		= &ce_pub_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_apapb_gate_desc = {
	.clk_clks	= roc1_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_apapb_gate),
	.hw_clks	= &roc1_apapb_gate_hws,
};

static const struct of_device_id sprd_roc1_clk_ids[] = {
	{ .compatible = "sprd,roc1-apahb-gate",	/* 0x20100000 */
	  .data = &roc1_apahb_gate_desc },
	{ .compatible = "sprd,roc1-ap-clk",	/* 0x20200000 */
	  .data = &roc1_ap_clk_desc },
	{ .compatible = "sprd,roc1-ipa-gate",	/* 0x21040000 */
	  .data = &roc1_ipa_gate_desc },
	{ .compatible = "sprd,roc1-ipa-clk",	/* 0x21050000 */
	  .data = &roc1_ipa_clk_desc },
	{ .compatible = "sprd,roc1-aonapb-clk",	/* 0x32080000 */
	  .data = &roc1_aon_apb_desc },
	{ .compatible = "sprd,roc1-pmu-gate",	/* 0x32280000 */
	  .data = &roc1_pmu_gate_desc },
	{ .compatible = "sprd,roc1-g0-pll",		/* 0x32390000 */
	  .data = &roc1_g0_pll_desc },
	{ .compatible = "sprd,roc1-g4-pll",		/* 0x323c0000 */
	  .data = &roc1_g4_pll_desc },
	{ .compatible = "sprd,roc1-g5-pll",		/* 0x323d0000 */
	  .data = &roc1_g5_pll_desc },
	{ .compatible = "sprd,roc1-g9-pll",		/* 0x32410000 */
	  .data = &roc1_g9_pll_desc },
	{ .compatible = "sprd,roc1-g12-pll",		/* 0x32414000 */
	  .data = &roc1_g12_pll_desc },
	{ .compatible = "sprd,roc1-g17-pll",		/* 0x32418000 */
	  .data = &roc1_g17_pll_desc },
	{ .compatible = "sprd,roc1-audcpapb-gate",	/* 0x3350d000 */
	  .data = &roc1_audcpapb_gate_desc },
	{ .compatible = "sprd,roc1-audcpahb-gate",	/* 0x335e0000 */
	  .data = &roc1_audcpahb_gate_desc },
	{ .compatible = "sprd,roc1-gpu-clk",		/* 0x60100000 */
	  .data = &roc1_gpu_clk_desc },
	{ .compatible = "sprd,roc1-mm-clk",		/* 0x62100000 */
	  .data = &roc1_mm_clk_desc },
	{ .compatible = "sprd,roc1-mm-gate-clk",	/* 0x62200000 */
	  .data = &roc1_mm_gate_clk_desc },
	{ .compatible = "sprd,roc1-ai-gate-clk",	/* 0x6fd00000 */
	  .data = &roc1_ai_gate_desc },
	{ .compatible = "sprd,roc1-ai-clk",	/* 0x6fd04000 */
	  .data = &roc1_ai_desc },
	{ .compatible = "sprd,roc1-aon-gate",	/* 0x32090000 */
	  .data = &roc1_aon_gate_desc },
	{ .compatible = "sprd,roc1-apapb-gate",	/* 0x71000000 */
	  .data = &roc1_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_roc1_clk_ids);

static int roc1_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;
	int ret;

	match = of_match_node(sprd_roc1_clk_ids, pdev->dev.of_node);
	if (!match) {
		pr_err("%s: of_match_node() failed", __func__);
		return -ENODEV;
	}

	desc = match->data;

	ret = sprd_clk_regmap_init(pdev, desc);
	if (ret)
		return ret;

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver roc1_clk_driver = {
	.probe	= roc1_clk_probe,
	.driver	= {
		.name	= "roc1-clk",
		.of_match_table	= sprd_roc1_clk_ids,
	},
};
module_platform_driver(roc1_clk_driver);

MODULE_DESCRIPTION("Spreadtrum Roc1 Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:roc1-clk");
