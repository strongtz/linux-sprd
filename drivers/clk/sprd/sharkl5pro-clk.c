// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum Sharkl5Pro clock driver
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

#include <dt-bindings/clock/sprd,sharkl5pro-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

#define SHARKL5PRO_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

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
static SPRD_PLL_SC_GATE_CLK(cppll_gate,  "cppll-gate",  "ext-26m", 0xe4,
			    0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll0_gate,  "mpll0-gate",  "ext-26m", 0x190,
			    0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll1_gate,  "mpll1-gate",  "ext-26m", 0x194,
			    0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll2_gate,  "mpll2-gate",  "ext-26m", 0x198,
			    0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *sharkl5pro_pmu_gate_clks[] = {
	/* address base is 0x327e0000 */
	&isppll_gate.common,
	&dpll0_gate.common,
	&dpll1_gate.common,
	&lpll_gate.common,
	&twpll_gate.common,
	&gpll_gate.common,
	&rpll_gate.common,
	&cppll_gate.common,
	&mpll0_gate.common,
	&mpll1_gate.common,
	&mpll2_gate.common,
};

static struct clk_hw_onecell_data sharkl5pro_pmu_gate_hws = {
	.hws	= {
		[CLK_ISPPLL_GATE] = &isppll_gate.common.hw,
		[CLK_DPLL0_GATE]  = &dpll0_gate.common.hw,
		[CLK_DPLL1_GATE]  = &dpll1_gate.common.hw,
		[CLK_LPLL_GATE]   = &lpll_gate.common.hw,
		[CLK_TWPLL_GATE]  = &twpll_gate.common.hw,
		[CLK_GPLL_GATE]   = &gpll_gate.common.hw,
		[CLK_RPLL_GATE]   = &rpll_gate.common.hw,
		[CLK_CPPLL_GATE]  = &cppll_gate.common.hw,
		[CLK_MPLL0_GATE]  = &mpll0_gate.common.hw,
		[CLK_MPLL1_GATE]  = &mpll1_gate.common.hw,
		[CLK_MPLL2_GATE]  = &mpll2_gate.common.hw,
	},
	.num = CLK_PMU_GATE_NUM,
};

static struct sprd_clk_desc sharkl5pro_pmu_gate_desc = {
	.clk_clks	= sharkl5pro_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_pmu_gate_clks),
	.hw_clks        = &sharkl5pro_pmu_gate_hws,
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
	{ .shift = 18,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 67,	.width = 1 },	/* mod_en	*/
	{ .shift = 1,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 4,	.width = 3 },	/* icp		*/
	{ .shift = 7,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(dpll0_clk, "dpll0", "dpll0-gate", 0x4,
			       3, dpll0_ftable, f_dpll0, 240);
static CLK_FIXED_FACTOR(dpll0_58m31, "dpll0-58m31", "dpll0", 32, 1, 0);

static struct sprd_clk_common *sharkl5pro_g0_pll_clks[] = {
	/* address base is 0x32390000 */
	&dpll0_clk.common,
};

static struct clk_hw_onecell_data sharkl5pro_g0_pll_hws = {
	.hws	= {
		[CLK_DPLL0]		= &dpll0_clk.common.hw,
		[CLK_DPLL0_58M31]	= &dpll0_58m31.hw,
	},
	.num	= CLK_ANLG_PHY_G0_NUM,
};

static struct sprd_clk_desc sharkl5pro_g0_pll_desc = {
	.clk_clks	= sharkl5pro_g0_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_g0_pll_clks),
	.hw_clks	= &sharkl5pro_g0_pll_hws,
};

/* pll clock at g2 */
static struct freq_table mpll_ftable[7] = {
	{ .ibias = 1, .max_freq = 1400000000ULL },
	{ .ibias = 2, .max_freq = 1600000000ULL },
	{ .ibias = 3, .max_freq = 1800000000ULL },
	{ .ibias = 4, .max_freq = 2000000000ULL },
	{ .ibias = 5, .max_freq = 2200000000ULL },
	{ .ibias = 6, .max_freq = 2500000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_mpll[PLL_FACT_MAX] = {
	{ .shift = 17,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 67,	.width = 1 },	/* mod_en	*/
	{ .shift = 1,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 2,	.width = 3 },	/* icp		*/
	{ .shift = 5,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 77,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll1_clk, "mpll1", "mpll1-gate", 0x0,
				   3, mpll_ftable, f_mpll, 240,
				   1000, 1000, 1, 1200000000);
static CLK_FIXED_FACTOR(mpll1_63m38, "mpll1-63m38", "mpll1", 32, 1, 0);

static struct sprd_clk_common *sharkl5pro_g2_pll_clks[] = {
	/* address base is 0x323B0000 */
	&mpll1_clk.common,
};

static struct clk_hw_onecell_data sharkl5pro_g2_pll_hws = {
	.hws	= {
		[CLK_MPLL1]		= &mpll1_clk.common.hw,
		[CLK_MPLL1_63M38]	= &mpll1_63m38.hw,
	},
	.num	= CLK_ANLG_PHY_G2_NUM,
};

static struct sprd_clk_desc sharkl5pro_g2_pll_desc = {
	.clk_clks	= sharkl5pro_g2_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_g2_pll_clks),
	.hw_clks	= &sharkl5pro_g2_pll_hws,
};

/* pll at g3 */
static struct freq_table ftable[6] = {
	{ .ibias = 2, .max_freq = 900000000ULL },
	{ .ibias = 3, .max_freq = 1100000000ULL },
	{ .ibias = 4, .max_freq = 1300000000ULL },
	{ .ibias = 5, .max_freq = 1500000000ULL },
	{ .ibias = 6, .max_freq = 1600000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_pll[PLL_FACT_MAX] = {
	{ .shift = 18,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 67,	.width = 1 },	/* mod_en	*/
	{ .shift = 1,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 2,	.width = 3 },	/* icp		*/
	{ .shift = 5,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 77,	.width = 1 },	/* postdiv	*/
};

#define f_rpll f_pll
static SPRD_PLL_WITH_ITABLE_K_FVCO(rpll_clk, "rpll", "ext-26m", 0x0,
				   3, ftable, f_rpll, 240,
				   1000, 1000, 1, 750000000);

static SPRD_SC_GATE_CLK(audio_gate,	"audio-gate",	"ext-26m", 0x24,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);

static struct clk_bit_field f_mpll2[PLL_FACT_MAX] = {
	{ .shift = 16,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 67,	.width = 1 },	/* mod_en	*/
	{ .shift = 1,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 2,	.width = 3 },	/* icp		*/
	{ .shift = 5,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 77,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll0_clk, "mpll0", "mpll0-gate", 0x54,
				   3, mpll_ftable, f_mpll, 240,
				   1000, 1000, 1, 1200000000);
static CLK_FIXED_FACTOR(mpll0_56m88, "mpll0-56m88", "mpll0", 32, 1, 0);

static struct freq_table mpll2_ftable[6] = {
	{ .ibias = 0, .max_freq = 1200000000ULL },
	{ .ibias = 1, .max_freq = 1400000000ULL },
	{ .ibias = 2, .max_freq = 1600000000ULL },
	{ .ibias = 3, .max_freq = 1800000000ULL },
	{ .ibias = 4, .max_freq = 2000000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll2_clk, "mpll2", "mpll2-gate", 0x9c,
				   3, mpll2_ftable, f_mpll2, 240,
				   1000, 1000, 1, 1000000000);
static CLK_FIXED_FACTOR(mpll2_47m13, "mpll2-47m13", "mpll2", 32, 1, 0);

static struct sprd_clk_common *sharkl5pro_g3_pll_clks[] = {
	/* address base is 0x323c0000 */
	&rpll_clk.common,
	&audio_gate.common,
	&mpll0_clk.common,
	&mpll2_clk.common,
};

static struct clk_hw_onecell_data sharkl5pro_g3_pll_hws = {
	.hws	= {
		[CLK_RPLL]		= &rpll_clk.common.hw,
		[CLK_AUDIO_GATE]	= &audio_gate.common.hw,
		[CLK_MPLL0]		= &mpll0_clk.common.hw,
		[CLK_MPLL0_56M88]	= &mpll0_56m88.hw,
		[CLK_MPLL2]		= &mpll2_clk.common.hw,
		[CLK_MPLL2_47M13]	= &mpll2_47m13.hw,
	},
	.num	= CLK_ANLG_PHY_G3_NUM,
};

static struct sprd_clk_desc sharkl5pro_g3_pll_desc = {
	.clk_clks	= sharkl5pro_g3_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_g3_pll_clks),
	.hw_clks	= &sharkl5pro_g3_pll_hws,
};

/* pll clock at gc */
#define f_twpll f_pll
static SPRD_PLL_WITH_ITABLE_K_FVCO(twpll_clk, "twpll", "ext-26m", 0x0,
				   3, ftable, f_twpll, 240,
				   1000, 1000, 1, 750000000);
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
static CLK_FIXED_FACTOR(twpll_12m29, "twpll-12m29", "twpll", 125, 1, 0);

#define f_lpll f_twpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(lpll_clk, "lpll", "lpll-gate", 0x18,
				   3, ftable, f_lpll, 240,
				   1000, 1000, 1, 750000000);
static CLK_FIXED_FACTOR(lpll_614m4, "lpll-614m4", "lpll", 2, 1, 0);
static CLK_FIXED_FACTOR(lpll_409m6, "lpll-409m6", "lpll", 3, 1, 0);
static CLK_FIXED_FACTOR(lpll_245m76, "lpll-245m76", "lpll", 5, 1, 0);
static CLK_FIXED_FACTOR(lpll_30m72, "lpll-30m72", "lpll", 40, 1, 0);

#define f_isppll f_twpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(isppll_clk, "isppll", "isppll-gate", 0x30,
				   3, ftable, f_isppll, 240,
				   1000, 1000, 1, 750000000);
static CLK_FIXED_FACTOR(isppll_468m, "isppll-468m", "isppll", 2, 1, 0);
static CLK_FIXED_FACTOR(isppll_78m, "isppll-78m", "isppll", 12, 1, 0);

#define f_gpll f_twpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(gpll_clk, "gpll", "gpll-gate", 0x48,
				   3, ftable, f_gpll, 240,
				   1000, 1000, 1, 750000000);
static CLK_FIXED_FACTOR(gpll_40m, "gpll-40m", "gpll", 20, 1, 0);

#define f_cppll f_twpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(cppll_clk, "cppll", "cppll-gate", 0x60,
				   3, ftable, f_cppll, 240,
				   1000, 1000, 1, 750000000);
static CLK_FIXED_FACTOR(cppll_39m32, "cppll-39m32", "cppll", 26, 1, 0);

static struct sprd_clk_common *sharkl5pro_gc_pll_clks[] = {
	/* address base is 0x323e0000 */
	&twpll_clk.common,
	&lpll_clk.common,
	&isppll_clk.common,
	&gpll_clk.common,
	&cppll_clk.common,
};

static struct clk_hw_onecell_data sharkl5pro_gc_pll_hws = {
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
		[CLK_TWPLL_12M29]	= &twpll_12m29.hw,
		[CLK_LPLL]		= &lpll_clk.common.hw,
		[CLK_LPLL_614M4]	= &lpll_614m4.hw,
		[CLK_LPLL_409M6]	= &lpll_409m6.hw,
		[CLK_LPLL_245M76]	= &lpll_245m76.hw,
		[CLK_LPLL_30M72]	= &lpll_30m72.hw,
		[CLK_ISPPLL]		= &isppll_clk.common.hw,
		[CLK_ISPPLL_468M]	= &isppll_468m.hw,
		[CLK_ISPPLL_78M]	= &isppll_78m.hw,
		[CLK_GPLL]		= &gpll_clk.common.hw,
		[CLK_GPLL_40M]		= &gpll_40m.hw,
		[CLK_CPPLL]		= &cppll_clk.common.hw,
		[CLK_CPPLL_39M32]	= &cppll_39m32.hw,
	},
	.num	= CLK_ANLG_PHY_GC_NUM,
};

static struct sprd_clk_desc sharkl5pro_gc_pll_desc = {
	.clk_clks	= sharkl5pro_gc_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_gc_pll_clks),
	.hw_clks	= &sharkl5pro_gc_pll_hws,
};

/* apcpu sec clk */
static const char * const acore_parents[] = { "ext-26m", "lpll",
					      "twpll", "mpll0" };
static SPRD_COMP_CLK_SEC(acore0_clk, "acore0-clk", acore_parents, 0,
			 0, 3, 4, 3, 0);
static SPRD_COMP_CLK_SEC(acore1_clk, "acore1-clk", acore_parents, 1,
			 8, 3, 12, 3, 0);
static SPRD_COMP_CLK_SEC(acore2_clk, "acore2-clk", acore_parents, 2,
			 16, 3, 20, 3, 0);
static SPRD_COMP_CLK_SEC(acore3_clk, "acore3-clk", acore_parents, 3,
			 24, 3, 28, 3, 0);
static SPRD_COMP_CLK_SEC(acore4_clk, "acore4-clk", acore_parents, 4,
			 0, 3, 4, 3, 0);
static SPRD_COMP_CLK_SEC(acore5_clk, "acore5-clk", acore_parents, 5,
			 8, 3, 12, 3, 0);

static const char * const pcore_parents[] = { "ext-26m", "lpll",
					      "twpll", "mpll1" };
static SPRD_COMP_CLK_SEC(pcore0_clk, "pcore0-clk", pcore_parents, 6,
			 16, 3, 20, 3, 0);
static SPRD_COMP_CLK_SEC(pcore1_clk, "pcore1-clk", pcore_parents, 7,
			 24, 3, 28, 3, 0);

static const char * const scu_parents[] = { "ext-26m", "twpll-768m",
					    "lpll", "mpll2" };
static SPRD_COMP_CLK_SEC(scu_clk, "scu-clk", scu_parents, 8,
			 0, 3, 4, 3, 0);
static SPRD_DIV_CLK_SEC(ace_clk, "ace-clk", "scu-clk", 9,
			12, 3, 0);

static const char * const periph_parents[] = { "ext-26m", "twpll-153m6",
					       "twpll-512m", "twpll-768m" };
static SPRD_COMP_CLK_SEC(periph_clk, "periph-clk", periph_parents, 10,
			 24, 3, 28, 3, 0);

static const char * const gic_parents[] = { "ext-26m", "twpll-153m6",
					    "twpll-384m", "twpll-512m" };
static SPRD_COMP_CLK_SEC(gic_clk, "gic-clk", gic_parents, 11,
			 16, 3, 20, 3, 0);

static SPRD_COMP_CLK_SEC(atb_clk, "atb-clk", periph_parents, 12,
			 0, 3, 4, 3, 0);
static SPRD_DIV_CLK_SEC(debug_apb_clk, "debug-apb-clk", "atb-clk", 13,
			12, 3, 0);

static struct sprd_clk_common *sharkl5pro_apcpu_clks[] = {
	/* address base is 0x32880000 */
	&acore0_clk.common,
	&acore1_clk.common,
	&acore2_clk.common,
	&acore3_clk.common,
	&acore4_clk.common,
	&acore5_clk.common,
	&pcore0_clk.common,
	&pcore1_clk.common,
	&scu_clk.common,
	&ace_clk.common,
	&periph_clk.common,
	&gic_clk.common,
	&atb_clk.common,
	&debug_apb_clk.common,
};

static struct clk_hw_onecell_data sharkl5pro_apcpu_hws = {
	.hws	= {
		[CLK_ACORE0]	= &acore0_clk.common.hw,
		[CLK_ACORE1]	= &acore1_clk.common.hw,
		[CLK_ACORE2]	= &acore2_clk.common.hw,
		[CLK_ACORE3]	= &acore3_clk.common.hw,
		[CLK_ACORE4]	= &acore4_clk.common.hw,
		[CLK_ACORE5]	= &acore5_clk.common.hw,
		[CLK_PCORE0]	= &pcore0_clk.common.hw,
		[CLK_PCORE1]	= &pcore1_clk.common.hw,
		[CLK_SCU]	= &scu_clk.common.hw,
		[CLK_ACE]	= &ace_clk.common.hw,
		[CLK_PERIPH]	= &periph_clk.common.hw,
		[CLK_GIC]	= &gic_clk.common.hw,
		[CLK_ATB]	= &atb_clk.common.hw,
		[CLK_DEBUG_APB]	= &debug_apb_clk.common.hw,
	},
	.num	= CLK_APCPU_SEC_NUM,
};

static struct sprd_clk_desc sharkl5pro_apcpu_clk_sec_desc = {
	.clk_clks	= sharkl5pro_apcpu_clks,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_apcpu_clks),
	.hw_clks	= &sharkl5pro_apcpu_hws,
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

static struct sprd_clk_common *sharkl5pro_audcpapb_gate[] = {
	/* address base is 0x3350d000 */
	&audcp_wdg_eb.common,
	&audcp_rtc_wdg_eb.common,
	&audcp_tmr0_eb.common,
	&audcp_tmr1_eb.common,
};

static struct clk_hw_onecell_data sharkl5pro_audcpapb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_WDG_EB]	= &audcp_wdg_eb.common.hw,
		[CLK_AUDCP_RTC_WDG_EB]	= &audcp_rtc_wdg_eb.common.hw,
		[CLK_AUDCP_TMR0_EB]	= &audcp_tmr0_eb.common.hw,
		[CLK_AUDCP_TMR1_EB]	= &audcp_tmr1_eb.common.hw,
	},
	.num	= CLK_AUDCP_APB_GATE_NUM,
};

static const struct sprd_clk_desc sharkl5pro_audcpapb_gate_desc = {
	.clk_clks	= sharkl5pro_audcpapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_audcpapb_gate),
	.hw_clks	= &sharkl5pro_audcpapb_gate_hws,
};

/* audcp ahb gates */
static SPRD_SC_GATE_CLK(audcp_iis0_eb, "audcp-iis0-eb", "ext-26m", 0x0,
			0x100, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_iis1_eb, "audcp-iis1-eb", "ext-26m", 0x0,
			0x100, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_iis2_eb, "audcp-iis2-eb", "ext-26m", 0x0,
			0x100, BIT(2), CLK_IGNORE_UNUSED, 0);
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

static struct sprd_clk_common *sharkl5pro_audcpahb_gate[] = {
	/* address base is 0x335e0000 */
	&audcp_iis0_eb.common,
	&audcp_iis1_eb.common,
	&audcp_iis2_eb.common,
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

static struct clk_hw_onecell_data sharkl5pro_audcpahb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_IIS0_EB]	= &audcp_iis0_eb.common.hw,
		[CLK_AUDCP_IIS1_EB]	= &audcp_iis1_eb.common.hw,
		[CLK_AUDCP_IIS2_EB]	= &audcp_iis2_eb.common.hw,
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

static const struct sprd_clk_desc sharkl5pro_audcpahb_gate_desc = {
	.clk_clks	= sharkl5pro_audcpahb_gate,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_audcpahb_gate),
	.hw_clks	= &sharkl5pro_audcpahb_gate_hws,
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

static struct sprd_clk_common *sharkl5pro_gpu_clk[] = {
	/* address base is 0x60100000 */
	&gpu_core_gate.common,
	&gpu_core_clk.common,
	&gpu_mem_gate.common,
	&gpu_mem_clk.common,
	&gpu_sys_gate.common,
	&gpu_sys_clk.common,
};

static struct clk_hw_onecell_data sharkl5pro_gpu_clk_hws = {
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

static struct sprd_clk_desc sharkl5pro_gpu_clk_desc = {
	.clk_clks	= sharkl5pro_gpu_clk,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_gpu_clk),
	.hw_clks	= &sharkl5pro_gpu_clk_hws,
};

/* mm clocks */
static const char * const mm_ahb_parents[] = { "ext-26m", "twpll-96m",
					       "twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(mm_ahb_clk, "mm-ahb-clk", mm_ahb_parents, 0x20,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const mm_mtx_parents[] = { "twpll-76m8", "twpll-128m",
					       "twpll-256m", "twpll-307m2",
					       "twpll-384m", "isppll-468m",
					       "twpll-512m" };
static SPRD_MUX_CLK(mm_mtx_clk, "mm-mtx-clk", mm_mtx_parents, 0x24,
		    0, 3, SHARKL5PRO_MUX_FLAG);

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
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const jpg_parents[] = { "twpll-76m8", "twpll-128m",
					    "twpll-256m", "twpll-384m" };
static SPRD_MUX_CLK(jpg_clk, "jpg-clk", jpg_parents, 0x38,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const fd_parents[] = { "twpll-76m8", "twpll-192m",
					   "twpll-307m2", "twpll-384m" };
static SPRD_MUX_CLK(fd_clk, "fd-clk", fd_parents, 0x3c,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const dcam_if_parents[] = { "twpll-192m", "twpll-256m",
						"twpll-307m2", "twpll-384m",
						"isppll-468m" };
static SPRD_MUX_CLK(dcam_if_clk, "dcam-if-clk", dcam_if_parents, 0x40,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const dcam_axi_parents[] = { "twpll-256m", "twpll-307m2",
						 "twpll-384m", "isppll-468m" };
static SPRD_MUX_CLK(dcam_axi_clk, "dcam-axi-clk", dcam_axi_parents, 0x44,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const isp_parents[] = { "twpll-256m", "twpll-307m2",
					    "twpll-384m", "isppll-468m",
					    "twpll-512m"};
static SPRD_MUX_CLK(isp_clk, "isp-clk", isp_parents, 0x48,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static SPRD_GATE_CLK(mipi_csi0, "mipi-csi0", "mm-ahb-clk", 0x4c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(mipi_csi1, "mipi-csi1", "mm-ahb-clk", 0x50,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(mipi_csi2, "mipi-csi2", "mm-ahb-clk", 0x54,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sharkl5pro_mm_clk[] = {
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

static struct clk_hw_onecell_data sharkl5pro_mm_clk_hws = {
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

static struct sprd_clk_desc sharkl5pro_mm_clk_desc = {
	.clk_clks	= sharkl5pro_mm_clk,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_mm_clk),
	.hw_clks	= &sharkl5pro_mm_clk_hws,
};

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

static struct sprd_clk_common *sharkl5pro_mm_gate_clk[] = {
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

static struct clk_hw_onecell_data sharkl5pro_mm_gate_clk_hws = {
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

static struct sprd_clk_desc sharkl5pro_mm_gate_clk_desc = {
	.clk_clks	= sharkl5pro_mm_gate_clk,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_mm_gate_clk),
	.hw_clks	= &sharkl5pro_mm_gate_clk_hws,
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
static SPRD_SC_GATE_CLK(dma_pub_eb,	"dma-pub-eb",	"ext-26m", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_sec_eb,	"dma-sec-eb",	"ext-26m", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipi_eb,		"ipi-eb",	"ext-26m", 0x0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ahb_ckg_eb,	"ahb-ckg-eb",	"ext-26m", 0x0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bm_clk_eb,	"bm-clk-eb",	"ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sharkl5pro_apahb_gate[] = {
	/* address base is 0x20100000 */
	&dsi_eb.common,
	&dispc_eb.common,
	&vsp_eb.common,
	&vdma_eb.common,
	&dma_pub_eb.common,
	&dma_sec_eb.common,
	&ipi_eb.common,
	&ahb_ckg_eb.common,
	&bm_clk_eb.common,
};

static struct clk_hw_onecell_data sharkl5pro_apahb_gate_hws = {
	.hws	= {
		[CLK_DSI_EB] = &dsi_eb.common.hw,
		[CLK_DISPC_EB] = &dispc_eb.common.hw,
		[CLK_VSP_EB] = &vsp_eb.common.hw,
		[CLK_VDMA_EB] = &vdma_eb.common.hw,
		[CLK_DMA_PUB_EB] = &dma_pub_eb.common.hw,
		[CLK_DMA_SEC_EB] = &dma_sec_eb.common.hw,
		[CLK_IPI_EB] = &ipi_eb.common.hw,
		[CLK_AHB_CKG_EB] = &ahb_ckg_eb.common.hw,
		[CLK_BM_CLK_EB] = &bm_clk_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static struct sprd_clk_desc sharkl5pro_apahb_gate_desc = {
	.clk_clks	= sharkl5pro_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_apahb_gate),
	.hw_clks	= &sharkl5pro_apahb_gate_hws,
};

/* ap clks */
static const char * const ap_apb_parents[] = { "ext-26m", "twpll-64m",
					       "twpll-96m", "twpll-128m" };
static SPRD_MUX_CLK(ap_apb_clk, "ap-apb-clk", ap_apb_parents, 0x20,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const ipi_parents[] = { "ext-26m", "twpll-64m",
					    "twpll-96m", "twpll-128m" };
static SPRD_MUX_CLK(ipi_clk, "ipi-clk", ipi_parents, 0x24,
		    0, 2, SHARKL5PRO_MUX_FLAG);

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
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_iis1_clk, "ap-iis1-clk", iis_parents, 0x5c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_iis2_clk, "ap-iis2-clk", iis_parents, 0x60,
		     0, 2, 8, 3, 0);

static const char * const sim_parents[] = { "ext-26m", "twpll-51m2",
					"twpll-64m", "twpll-96m",
					"twpll-128m"};
static SPRD_COMP_CLK(ap_sim_clk, "ap-sim-clk", sim_parents, 0x64,
		     0, 3, 8, 3, 0);

static const char * const ap_ce_parents[] = { "ext-26m", "twpll-96m",
					"twpll-192m", "twpll-256m"};
static SPRD_MUX_CLK(ap_ce_clk, "ap-ce-clk", ap_ce_parents, 0x68,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const sdio_parents[] = { "ext-1m", "ext-26m",
					     "twpll-307m2", "twpll-384m",
					     "rpll", "lpll-409m6" };
static SPRD_MUX_CLK(sdio0_2x_clk, "sdio0-2x", sdio_parents, 0x80,
		    0, 3, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(sdio1_2x_clk, "sdio1-2x", sdio_parents, 0x88,
		    0, 3, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(emmc_2x_clk, "emmc-2x", sdio_parents, 0x90,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const vsp_parents[] = { "twpll-256m", "twpll-307m2",
					"twpll-384m"};
static SPRD_MUX_CLK(vsp_clk, "vsp-clk", vsp_parents, 0x98,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const dispc0_parents[] = { "twpll-153m6", "twpll-192m",
					"twpll-256m", "twpll-307m2",
					"twpll-384m"};
static SPRD_MUX_CLK(dispc0_clk, "dispc0-clk", dispc0_parents, 0x9c,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const dispc0_dpi_parents[] = { "twpll-96m",
						   "twpll-128m", "twpll-153m6",
						   "twpll-192m", "dphy-273m",
						   "dphy-204m8"};
static SPRD_COMP_CLK(dispc0_dpi_clk, "dispc0-dpi-clk", dispc0_dpi_parents,
		     0xa0, 0, 3, 8, 4, 0);

static const char * const dsi_apb_parents[] = { "twpll-96m", "twpll-128m",
					"twpll-153m6", "twpll-192m"};
static SPRD_MUX_CLK(dsi_apb_clk, "dsi-apb-clk", dsi_apb_parents, 0xa4,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static SPRD_GATE_CLK(dsi_rxesc, "dsi-rxesc", "ap-apb-clk", 0xa8,
		     BIT(16), 0, 0);

static SPRD_GATE_CLK(dsi_lanebyte, "dsi-lanebyte", "ap-apb-clk", 0xac,
		     BIT(16), 0, 0);

static const char * const vdsp_parents[] = { "twpll-256m", "twpll-384m",
					     "twpll-512m", "lpll-614m4",
					     "twpll-768m", "isppll" };
static SPRD_MUX_CLK(vdsp_clk, "vdsp-clk", vdsp_parents, 0xb0,
		    0, 3, SHARKL5PRO_MUX_FLAG);
static SPRD_DIV_CLK(vdsp_m_clk, "vdsp-m-clk", "vdsp-clk", 0xb4,
		    8, 2, 0);

static struct sprd_clk_common *sharkl5pro_ap_clks[] = {
	/* address base is 0x20200000 */
	&ap_apb_clk.common,
	&ipi_clk.common,
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
	&vsp_clk.common,
	&dispc0_clk.common,
	&dispc0_dpi_clk.common,
	&dsi_apb_clk.common,
	&dsi_rxesc.common,
	&dsi_lanebyte.common,
	&vdsp_clk.common,
	&vdsp_m_clk.common,

};

static struct clk_hw_onecell_data sharkl5pro_ap_clk_hws = {
	.hws	= {
		[CLK_AP_APB] = &ap_apb_clk.common.hw,
		[CLK_IPI] = &ipi_clk.common.hw,
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
		[CLK_SDIO0_2X] = &sdio0_2x_clk.common.hw,
		[CLK_SDIO1_2X] = &sdio1_2x_clk.common.hw,
		[CLK_EMMC_2X] = &emmc_2x_clk.common.hw,
		[CLK_VSP] = &vsp_clk.common.hw,
		[CLK_DISPC0] = &dispc0_clk.common.hw,
		[CLK_DISPC0_DPI] = &dispc0_dpi_clk.common.hw,
		[CLK_DSI_APB] = &dsi_apb_clk.common.hw,
		[CLK_DSI_RXESC] = &dsi_rxesc.common.hw,
		[CLK_DSI_LANEBYTE] = &dsi_lanebyte.common.hw,
		[CLK_VDSP] = &vdsp_clk.common.hw,
		[CLK_VDSP_M] = &vdsp_m_clk.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static struct sprd_clk_desc sharkl5pro_ap_clk_desc = {
	.clk_clks	= sharkl5pro_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_ap_clks),
	.hw_clks	= &sharkl5pro_ap_clk_hws,
};

/* aon apb clks */
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
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const aux_parents[] = { "ext-32k", "ext-26m",
					    "ext-26m-aud", "rco-25m",
					    "cppll-39m32", "mpll0-56m88",
					    "mpll1-63m38", "mpll2-47m13",
					    "dpll0-58m31", "gpll-40m",
					    "twpll-48m"};
static const char * const aux1_parents[] = { "ext-32k", "ext-26m",
					    "ext-26m-aud", "rco-25m",
					    "cppll-39m32", "mpll0-56m88",
					    "mpll1-63m38", "mpll2-47m13",
					    "dpll0-58m31", "gpll-40m",
					    "twpll-19m2", "lpll-30m72",
					    "rpll", "twpll-48m",
					    "twpll-12m29"};
static SPRD_COMP_CLK(aux0_clk, "aux0-clk", aux_parents, 0x228,
		     0, 5, 8, 4, 0);
static SPRD_COMP_CLK(aux1_clk, "aux1-clk", aux1_parents, 0x22c,
		     0, 5, 8, 4, 0);
static SPRD_COMP_CLK(aux2_clk, "aux2-clk", aux_parents, 0x230,
		     0, 5, 8, 4, 0);
static SPRD_COMP_CLK(probe_clk, "probe-clk", aux_parents, 0x234,
		     0, 5, 8, 4, 0);

static const char * const pwm_parents[] = { "clk-32k", "ext-26m",
					"rco-4m", "rco-25m", "twpll-48m" };
static SPRD_MUX_CLK(pwm0_clk, "pwm0-clk", pwm_parents, 0x238,
		    0, 3, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(pwm1_clk, "pwm1-clk", pwm_parents, 0x23c,
		    0, 3, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(pwm2_clk, "pwm2-clk", pwm_parents, 0x240,
		    0, 3, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(pwm3_clk, "pwm3-clk", pwm_parents, 0x244,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const efuse_parents[] = { "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(efuse_clk, "efuse-clk", efuse_parents, 0x248,
		    0, 1, SHARKL5PRO_MUX_FLAG);

static const char * const uart_parents[] = { "rco-4m", "ext-26m",
					"twpll-48m", "twpll-51m2",
					"twpll-96m", "rco-100m",
					"twpll-128m" };
static SPRD_MUX_CLK(uart0_clk, "uart0-clk", uart_parents, 0x24c,
		    0, 3, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(uart1_clk, "uart1-clk", uart_parents, 0x250,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const thm_parents[] = { "ext-32k", "ext-250k" };
static SPRD_MUX_CLK(thm0_clk, "thm0-clk", thm_parents, 0x260,
		    0, 1, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(thm1_clk, "thm1-clk", thm_parents, 0x264,
		    0, 1, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(thm2_clk, "thm2-clk", thm_parents, 0x268,
		    0, 1, SHARKL5PRO_MUX_FLAG);
static SPRD_MUX_CLK(thm3_clk, "thm3-clk", thm_parents, 0x26c,
		    0, 1, SHARKL5PRO_MUX_FLAG);

static const char * const aon_i2c_parents[] = { "rco-4m", "ext-26m",
					"twpll-48m", "twpll-51m2",
					"rco-100m", "twpll-153m6" };
static SPRD_MUX_CLK(aon_i2c_clk, "aon-i2c-clk", aon_i2c_parents, 0x27c,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const aon_iis_parents[] = { "ext-26m", "twpll-128m",
					"twpll-153m6" };
static SPRD_MUX_CLK(aon_iis_clk, "aon-iis-clk", aon_iis_parents, 0x280,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const scc_parents[] = { "ext-26m", "twpll-48m",
					"twpll-51m2", "twpll-96m" };
static SPRD_MUX_CLK(scc_clk, "scc-clk", scc_parents, 0x284,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const apcpu_dap_parents[] = { "ext-26m", "rco-4m",
					"twpll-76m8", "rco-100m",
					"twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(apcpu_dap_clk, "apcpu-dap-clk", apcpu_dap_parents, 0x288,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static SPRD_GATE_CLK(apcpu_dap_mtck, "apcpu-dap-mtck", "aon-apb-clk", 0x28c,
		     BIT(16), 0, 0);

static const char * const apcpu_ts_parents[] = { "ext-32k", "ext-26m",
					"twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(apcpu_ts_clk, "apcpu-ts-clk", apcpu_ts_parents, 0x290,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const debug_ts_parents[] = { "ext-26m", "twpll-76m8",
					"twpll-128m", "twpll-192m" };
static SPRD_MUX_CLK(debug_ts_clk, "debug-ts-clk", debug_ts_parents, 0x294,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static SPRD_GATE_CLK(dsi_test_s, "dsi-test-s", "aon-apb-clk", 0x298,
		     BIT(16), 0, 0);

static const char * const djtag_tck_parents[] = { "rco-4m", "ext-26m" };
static SPRD_MUX_CLK(djtag_tck_clk, "djtag-tck-clk", djtag_tck_parents, 0x2b4,
		    0, 1, SHARKL5PRO_MUX_FLAG);

static SPRD_GATE_CLK(djtag_tck_hw, "djtag-tck-hw", "aon-apb-clk", 0x2b8,
		     BIT(16), 0, 0);

static const char * const aon_tmr_parents[] = { "rco-4m", "rco-25m",
						"ext-26m" };
static SPRD_MUX_CLK(aon_tmr_clk, "aon-tmr-clk", aon_tmr_parents, 0x2c0,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const aon_pmu_parents[] = { "ext-32k", "rco-4m", "ext-4m" };
static SPRD_MUX_CLK(aon_pmu_clk, "aon-pmu-clk", aon_pmu_parents, 0x2c8,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const debounce_parents[] = { "ext-32k", "rco-4m",
						"rco-25m", "ext-26m" };
static SPRD_MUX_CLK(debounce_clk, "debounce-clk", debounce_parents, 0x2cc,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const apcpu_pmu_parents[] = { "ext-26m", "twpll-76m8",
						"rco-100m", "twpll-128m" };
static SPRD_MUX_CLK(apcpu_pmu_clk, "apcpu-pmu-clk", apcpu_pmu_parents, 0x2d0,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const top_dvfs_parents[] = { "ext-26m", "twpll-96m",
						"rco-100m", "twpll-128m" };
static SPRD_MUX_CLK(top_dvfs_clk, "top-dvfs-clk", top_dvfs_parents, 0x2d8,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static SPRD_GATE_CLK(otg_utmi, "otg-utmi", "aon-apb-clk", 0x2dc,
		     BIT(16), 0, 0);

static const char * const otg_ref_parents[] = { "twpll-12m", "ext-26m" };
static SPRD_MUX_CLK(otg_ref_clk, "otg-ref-clk", otg_ref_parents, 0x2e0,
		    0, 1, SHARKL5PRO_MUX_FLAG);

static const char * const cssys_parents[] = { "rco-25m", "ext-26m", "rco-100m",
					"twpll-153m6", "twpll-384m",
					"twpll-512m" };
static SPRD_COMP_CLK(cssys_clk,	"cssys-clk",	cssys_parents, 0x2e4,
		     0, 3, 8, 2, 0);
static SPRD_DIV_CLK(cssys_pub_clk, "cssys-pub-clk", "cssys-clk", 0x2e8,
		    8, 2, 0);
static SPRD_DIV_CLK(cssys_apb_clk, "cssys-apb-clk", "cssys-clk", 0x2ec,
		    8, 3, 0);

static const char * const ap_axi_parents[] = { "ext-26m", "twpll-76m8",
					"twpll-128m", "twpll-256m" };
static SPRD_MUX_CLK(ap_axi_clk, "ap-axi-clk", ap_axi_parents, 0x2f0,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const ap_mm_parents[] = { "ext-26m", "twpll-96m",
					"twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(ap_mm_clk, "ap-mm-clk", ap_mm_parents, 0x2f4,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const sdio2_2x_parents[] = { "ext-1m", "ext-26m",
					"twpll-307m2", "twpll-384m",
					"rpll", "lpll-409m6" };
static SPRD_MUX_CLK(sdio2_2x_clk, "sdio2-2x-clk", sdio2_2x_parents, 0x2f8,
		    0, 3, SHARKL5PRO_MUX_FLAG);

static const char * const analog_io_apb_parents[] = { "ext-26m", "twpll-48m" };
static SPRD_COMP_CLK(analog_io_apb, "analog-io-apb", analog_io_apb_parents,
		     0x300, 0, 1, 8, 2, 0);

static const char * const dmc_ref_parents[] = { "ext-6m5", "ext-13m",
					"ext-26m" };
static SPRD_MUX_CLK(dmc_ref_clk, "dmc-ref-clk", dmc_ref_parents, 0x304,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const emc_parents[] = { "ext-26m", "twpll-384m",
					"twpll-512m", "twpll-768m" };
static SPRD_MUX_CLK(emc_clk, "emc-clk", emc_parents, 0x30c,
		    0, 2, SHARKL5PRO_MUX_FLAG);

static const char * const usb_parents[] = { "rco-25m", "ext-26m", "twpll-192m",
					    "twpll-96m", "rco-100m", "twpll-128m" };
static SPRD_COMP_CLK(usb_clk, "usb-clk", usb_parents, 0x310,
		     0, 3, 8, 2, 0);

static const char * const pmu_26m_parents[] = { "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(pmu_26m_clk, "26m-pmu-clk", pmu_26m_parents, 0x318,
		    0, 1, SHARKL5PRO_MUX_FLAG);

static struct sprd_clk_common *sharkl5pro_aon_apb[] = {
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
	&otg_utmi.common,
	&otg_ref_clk.common,
	&cssys_clk.common,
	&cssys_pub_clk.common,
	&cssys_apb_clk.common,
	&ap_axi_clk.common,
	&ap_mm_clk.common,
	&sdio2_2x_clk.common,
	&analog_io_apb.common,
	&dmc_ref_clk.common,
	&emc_clk.common,
	&usb_clk.common,
	&pmu_26m_clk.common,
};

static struct clk_hw_onecell_data sharkl5pro_aon_apb_hws = {
	.hws	= {
		[CLK_AON_APB] = &aon_apb_clk.common.hw,
		[CLK_ADI] = &adi_clk.common.hw,
		[CLK_AUX0] = &aux0_clk.common.hw,
		[CLK_AUX1] = &aux1_clk.common.hw,
		[CLK_AUX2] = &aux2_clk.common.hw,
		[CLK_PROBE] = &probe_clk.common.hw,
		[CLK_PWM0] = &pwm0_clk.common.hw,
		[CLK_PWM1] = &pwm1_clk.common.hw,
		[CLK_PWM2] = &pwm2_clk.common.hw,
		[CLK_PWM3] = &pwm3_clk.common.hw,
		[CLK_EFUSE] = &efuse_clk.common.hw,
		[CLK_UART0] = &uart0_clk.common.hw,
		[CLK_UART1] = &uart1_clk.common.hw,
		[CLK_THM0] = &thm0_clk.common.hw,
		[CLK_THM1] = &thm1_clk.common.hw,
		[CLK_THM2] = &thm2_clk.common.hw,
		[CLK_THM3] = &thm3_clk.common.hw,
		[CLK_AON_I2C] = &aon_i2c_clk.common.hw,
		[CLK_AON_IIS] = &aon_iis_clk.common.hw,
		[CLK_SCC] = &scc_clk.common.hw,
		[CLK_APCPU_DAP] = &apcpu_dap_clk.common.hw,
		[CLK_APCPU_DAP_MTCK] = &apcpu_dap_mtck.common.hw,
		[CLK_APCPU_TS] = &apcpu_ts_clk.common.hw,
		[CLK_DEBUG_TS] = &debug_ts_clk.common.hw,
		[CLK_DSI_TEST_S] = &dsi_test_s.common.hw,
		[CLK_DJTAG_TCK] = &djtag_tck_clk.common.hw,
		[CLK_DJTAG_TCK_HW] = &djtag_tck_hw.common.hw,
		[CLK_AON_TMR] = &aon_tmr_clk.common.hw,
		[CLK_AON_PMU] = &aon_pmu_clk.common.hw,
		[CLK_DEBOUNCE] = &debounce_clk.common.hw,
		[CLK_APCPU_PMU] = &apcpu_pmu_clk.common.hw,
		[CLK_TOP_DVFS] = &top_dvfs_clk.common.hw,
		[CLK_OTG_UTMI] = &otg_utmi.common.hw,
		[CLK_OTG_REF] = &otg_ref_clk.common.hw,
		[CLK_CSSYS] = &cssys_clk.common.hw,
		[CLK_CSSYS_PUB] = &cssys_pub_clk.common.hw,
		[CLK_CSSYS_APB] = &cssys_apb_clk.common.hw,
		[CLK_AP_AXI] = &ap_axi_clk.common.hw,
		[CLK_AP_MM] = &ap_mm_clk.common.hw,
		[CLK_SDIO2_2X] = &sdio2_2x_clk.common.hw,
		[CLK_ANALOG_IO_APB] = &analog_io_apb.common.hw,
		[CLK_DMC_REF_CLK] = &dmc_ref_clk.common.hw,
		[CLK_EMC] = &emc_clk.common.hw,
		[CLK_USB] = &usb_clk.common.hw,
		[CLK_26M_PMU] = &pmu_26m_clk.common.hw,
	},
	.num	= CLK_AON_APB_NUM,
};

static struct sprd_clk_desc sharkl5pro_aon_apb_desc = {
	.clk_clks	= sharkl5pro_aon_apb,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_aon_apb),
	.hw_clks	= &sharkl5pro_aon_apb_hws,
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
static SPRD_SC_GATE_CLK(mm_eb,		"mm-eb",	"ext-26m", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb,		"gpu-eb",	"ext-26m", 0x0,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mspi_eb,	"mspi-eb",	"ext-26m", 0x0,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
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
static SPRD_SC_GATE_CLK(apcpu_ts0_eb,	"apcpu-ts0-eb",	"ext-26m", 0x4,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apb_busmon_eb,	"apb-busmon-eb", "ext-26m", 0x4,
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
static SPRD_SC_GATE_CLK(asim_top_eb, "asim-top", "ext-26m", 0x8,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c_eb,		"i2c-eb",	"ext-26m", 0x8,
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
			0x1000, BIT(25), 0, 0);
static SPRD_SC_GATE_CLK(pwm1_eb,	"pwm1-eb",	"ext-26m", 0x8,
			0x1000, BIT(26), 0, 0);
static SPRD_SC_GATE_CLK(pwm2_eb,	"pwm2-eb",	"ext-26m", 0x8,
			0x1000, BIT(27), 0, 0);
static SPRD_SC_GATE_CLK(pwm3_eb,	"pwm3-eb",	"ext-26m", 0x8,
			0x1000, BIT(28), 0, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb,	"ap-wdg-eb",	"ext-26m", 0x8,
			0x1000, BIT(29), 0, 0);
static SPRD_SC_GATE_CLK(apcpu_wdg_eb,	"apcpu-wdg-eb",	"ext-26m", 0x8,
			0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK(serdes_eb,	"serdes-eb",	"ext-26m", 0x8,
			0x1000, BIT(31), 0, 0);
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
static SPRD_SC_GATE_CLK(ap_emmc_rtc_eb,	"ap-emmc-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_sdio0_rtc_eb, "ap-sdio0-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_sdio1_rtc_eb, "ap-sdio1-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_sdio2_rtc_eb, "ap-sdio2-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi_csi_test_eb, "dsi-csi-test-eb", "ext-26m", 0x138,
			0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(djtag_tck_en, "djtag-tck-en", "ext-26m", 0x138,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dphy_ref_eb, "dphy-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_ref_eb, "dmc-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_ref_eb, "otg-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tsen_eb, "tsen-eb", "ext-26m", 0x138,
			0x1000, BIT(13), 0, 0);
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
static SPRD_SC_GATE_CLK(top_cssys_en, "top-cssys-en", "ext-26m", 0x13c,
			0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK(ap_axi_en, "ap-axi-en", "ext-26m", 0x13c,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_2x_en, "sdio0-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(sdio0_1x_en, "sdio0-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_2x_en, "sdio1-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(4), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_1x_en, "sdio1-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_2x_en, "sdio2-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_1x_en, "sdio2-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK(emmc_2x_en, "emmc-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(emmc_1x_en, "emmc-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(pll_test_en, "pll-test-en", "ext-26m", 0x13c,
			0x1000, BIT(14), 0, 0);
static SPRD_SC_GATE_CLK(cphy_cfg_en, "cphy-cfg-en", "ext-26m", 0x13c,
			0x1000, BIT(15), 0, 0);
static SPRD_SC_GATE_CLK(debug_ts_en, "debug-ts-en", "ext-26m", 0x13c,
			0x1000, BIT(18), 0, 0);

static struct sprd_clk_common *sharkl5pro_aon_gate[] = {
	/* address base is 0x327d0000 */
	&rc100m_cal_eb.common,
	&djtag_tck_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&mm_eb.common,
	&gpu_eb.common,
	&mspi_eb.common,
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
	&apcpu_ts0_eb.common,
	&apb_busmon_eb.common,
	&aon_iis_eb.common,
	&scc_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&thm2_eb.common,
	&asim_top_eb.common,
	&i2c_eb.common,
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
	&ap_emmc_rtc_eb.common,
	&ap_sdio0_rtc_eb.common,
	&ap_sdio1_rtc_eb.common,
	&ap_sdio2_rtc_eb.common,
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
	&debug_ts_en.common,
};

static struct clk_hw_onecell_data sharkl5pro_aon_gate_hws = {
	.hws	= {
		[CLK_RC100M_CAL_EB]	= &rc100m_cal_eb.common.hw,
		[CLK_DJTAG_TCK_EB]	= &djtag_tck_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_GPU_EB]		= &gpu_eb.common.hw,
		[CLK_MSPI_EB]		= &mspi_eb.common.hw,
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
		[CLK_APCPU_TS0_EB]	= &apcpu_ts0_eb.common.hw,
		[CLK_APB_BUSMON_EB]	= &apb_busmon_eb.common.hw,
		[CLK_AON_IIS_EB]	= &aon_iis_eb.common.hw,
		[CLK_SCC_EB]		= &scc_eb.common.hw,
		[CLK_THM0_EB]		= &thm0_eb.common.hw,
		[CLK_THM1_EB]		= &thm1_eb.common.hw,
		[CLK_THM2_EB]		= &thm2_eb.common.hw,
		[CLK_ASIM_TOP_EB]	= &asim_top_eb.common.hw,
		[CLK_I2C_EB]		= &i2c_eb.common.hw,
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
		[CLK_AP_EMMC_RTC_EB]	= &ap_emmc_rtc_eb.common.hw,
		[CLK_AP_SDIO0_RTC_EB]	= &ap_sdio0_rtc_eb.common.hw,
		[CLK_AP_SDIO1_RTC_EB]	= &ap_sdio1_rtc_eb.common.hw,
		[CLK_AP_SDIO2_RTC_EB]	= &ap_sdio2_rtc_eb.common.hw,
		[CLK_DSI_CSI_TEST_EB]	= &dsi_csi_test_eb.common.hw,
		[CLK_DJTAG_TCK_EN]	= &djtag_tck_en.common.hw,
		[CLK_DPHY_REF_EB]	= &dphy_ref_eb.common.hw,
		[CLK_DMC_REF_EB]	= &dmc_ref_eb.common.hw,
		[CLK_OTG_REF_EB]	= &otg_ref_eb.common.hw,
		[CLK_TSEN_EB]		= &tsen_eb.common.hw,
		[CLK_TMR_EB]		= &tmr_eb.common.hw,
		[CLK_RC100M_REF_EB]	= &rc100m_ref_eb.common.hw,
		[CLK_RC100M_FDK_EB]	= &rc100m_fdk_eb.common.hw,
		[CLK_DEBOUNCE_EB]	= &debounce_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_TOP_CSSYS_EB]	= &top_cssys_en.common.hw,
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
		[CLK_DEBUG_TS_EN]	= &debug_ts_en.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static struct sprd_clk_desc sharkl5pro_aon_gate_desc = {
	.clk_clks	= sharkl5pro_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_aon_gate),
	.hw_clks	= &sharkl5pro_aon_gate_hws,
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
static SPRD_SC_GATE_CLK(sdio0_32k_eb,	"sdio0-32k-eb",	"ext-26m", 0x0,
			0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_32k_eb,	"sdio1-32k-eb",	"ext-26m", 0x0,
			0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_32k_eb,	"sdio2-32k-eb",	"ext-26m", 0x0,
			0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb,	"emmc-32k-eb",	"ext-26m", 0x0,
			0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sharkl5pro_apapb_gate[] = {
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
	&sdio0_32k_eb.common,
	&sdio1_32k_eb.common,
	&sdio2_32k_eb.common,
	&emmc_32k_eb.common,
};

static struct clk_hw_onecell_data sharkl5pro_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_IIS1_EB]		= &iis1_eb.common.hw,
		[CLK_IIS2_EB]		= &iis2_eb.common.hw,
		[CLK_APB_REG_EB]	= &apb_reg_eb.common.hw,
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
		[CLK_SIM0_32K_EB]	= &sim0_32k_eb.common.hw,
		[CLK_SPI0_LFIN_EB]	= &spi0_lfin_eb.common.hw,
		[CLK_SPI1_LFIN_EB]	= &spi1_lfin_eb.common.hw,
		[CLK_SPI2_LFIN_EB]	= &spi2_lfin_eb.common.hw,
		[CLK_SPI3_LFIN_EB]	= &spi3_lfin_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO2_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_SDIO0_32K_EB]	= &sdio0_32k_eb.common.hw,
		[CLK_SDIO1_32K_EB]	= &sdio1_32k_eb.common.hw,
		[CLK_SDIO2_32K_EB]	= &sdio2_32k_eb.common.hw,
		[CLK_EMMC_32K_EB]	= &emmc_32k_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static struct sprd_clk_desc sharkl5pro_apapb_gate_desc = {
	.clk_clks	= sharkl5pro_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_apapb_gate),
	.hw_clks	= &sharkl5pro_apapb_gate_hws,
};

static const struct of_device_id sprd_sharkl5pro_clk_ids[] = {
	{ .compatible = "sprd,sharkl5pro-apahb-gate",	/* 0x20100000 */
	  .data = &sharkl5pro_apahb_gate_desc },
	{ .compatible = "sprd,sharkl5pro-ap-clk",	/* 0x20200000 */
	  .data = &sharkl5pro_ap_clk_desc },
	{ .compatible = "sprd,sharkl5pro-aonapb-clk",	/* 0x32080200 */
	  .data = &sharkl5pro_aon_apb_desc },
	{ .compatible = "sprd,sharkl5pro-aon-gate",	/* 0x327d0000 */
	  .data = &sharkl5pro_aon_gate_desc },
	{ .compatible = "sprd,sharkl5pro-pmu-gate",	/* 0x327e0000 */
	  .data = &sharkl5pro_pmu_gate_desc },
	{ .compatible = "sprd,sharkl5pro-g0-pll",	/* 0x32390000 */
	  .data = &sharkl5pro_g0_pll_desc },
	{ .compatible = "sprd,sharkl5pro-g2-pll",	/* 0x323b0000 */
	  .data = &sharkl5pro_g2_pll_desc },
	{ .compatible = "sprd,sharkl5pro-g3-pll",	/* 0x323c0000 */
	  .data = &sharkl5pro_g3_pll_desc },
	{ .compatible = "sprd,sharkl5pro-gc-pll",	/* 0x323e0000 */
	  .data = &sharkl5pro_gc_pll_desc },
	{ .compatible = "sprd,sharkl5pro-apcpu-clk-sec",/* 0x32880000 */
	  .data = &sharkl5pro_apcpu_clk_sec_desc },
	{ .compatible = "sprd,sharkl5pro-audcpapb-gate",/* 0x3350d000 */
	  .data = &sharkl5pro_audcpapb_gate_desc },
	{ .compatible = "sprd,sharkl5pro-audcpahb-gate",/* 0x335e0000 */
	  .data = &sharkl5pro_audcpahb_gate_desc },
	{ .compatible = "sprd,sharkl5pro-gpu-clk",	/* 0x60100000 */
	  .data = &sharkl5pro_gpu_clk_desc },
	{ .compatible = "sprd,sharkl5pro-mm-clk",	/* 0x62100000 */
	  .data = &sharkl5pro_mm_clk_desc },
	{ .compatible = "sprd,sharkl5pro-mm-gate-clk",	/* 0x62200000 */
	  .data = &sharkl5pro_mm_gate_clk_desc },
	{ .compatible = "sprd,sharkl5pro-apapb-gate",	/* 0x71000000 */
	  .data = &sharkl5pro_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_sharkl5pro_clk_ids);

static int sharkl5pro_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;
	int ret;

	match = of_match_node(sprd_sharkl5pro_clk_ids, pdev->dev.of_node);
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

static struct platform_driver sharkl5pro_clk_driver = {
	.probe	= sharkl5pro_clk_probe,
	.driver	= {
		.name	= "sharkl5pro-clk",
		.of_match_table	= sprd_sharkl5pro_clk_ids,
	},
};
module_platform_driver(sharkl5pro_clk_driver);

MODULE_DESCRIPTION("Spreadtrum Sharkl5Pro Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sharkl5pro-clk");
