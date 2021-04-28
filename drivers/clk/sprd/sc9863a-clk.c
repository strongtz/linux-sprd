// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum SC9863a clock driver
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Chunyan Zhang <chunyan.zhang@spreadtrum.com>

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,sc9863a-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

SPRD_PLL_SC_GATE_CLK(mpll0_gate, "mpll0-gate", "ext-26m", 0x94,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(dpll0_gate, "dpll0-gate", "ext-26m", 0x98,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(lpll_gate, "lpll-gate", "ext-26m", 0x9c,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(gpll_gate, "gpll-gate", "ext-26m", 0xa8,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(dpll1_gate, "dpll1-gate", "ext-26m", 0x1dc,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(mpll1_gate, "mpll1-gate", "ext-26m", 0x1e0,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(mpll2_gate, "mpll2-gate", "ext-26m", 0x1e4,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(isppll_gate, "isppll-gate", "ext-26m", 0x1e8,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *sc9863a_pmu_gate_clks[] = {
	/* address base is 0x402b0000 */
	&mpll0_gate.common,
	&dpll0_gate.common,
	&lpll_gate.common,
	&gpll_gate.common,
	&dpll1_gate.common,
	&mpll1_gate.common,
	&mpll2_gate.common,
	&isppll_gate.common,
};

static struct clk_hw_onecell_data sc9863a_pmu_gate_hws = {
	.hws	= {
		[CLK_MPLL0_GATE]	= &mpll0_gate.common.hw,
		[CLK_DPLL0_GATE]	= &dpll0_gate.common.hw,
		[CLK_LPLL_GATE]		= &lpll_gate.common.hw,
		[CLK_GPLL_GATE]		= &gpll_gate.common.hw,
		[CLK_DPLL1_GATE]	= &dpll1_gate.common.hw,
		[CLK_MPLL1_GATE]	= &mpll1_gate.common.hw,
		[CLK_MPLL2_GATE]	= &mpll2_gate.common.hw,
		[CLK_ISPPLL_GATE]	= &isppll_gate.common.hw,
	},
	.num	= CLK_PMU_APB_NUM,
};

static const struct sprd_clk_desc sc9863a_pmu_gate_desc = {
	.clk_clks	= sc9863a_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_pmu_gate_clks),
	.hw_clks        = &sc9863a_pmu_gate_hws,
};

static const struct freq_table ftable[5] = {
	{ .ibias = 0, .max_freq = 1000000000ULL },
	{ .ibias = 1, .max_freq = 1200000000ULL },
	{ .ibias = 2, .max_freq = 1400000000ULL },
	{ .ibias = 3, .max_freq = 1600000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static const struct clk_bit_field f_twpll[PLL_FACT_MAX] = {
	{ .shift = 95,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* ibias	*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(twpll_clk, "twpll", "ext-26m", 0x4,
				   3, ftable, f_twpll, 240);
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

static const struct clk_bit_field f_lpll[PLL_FACT_MAX] = {
	{ .shift = 95,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 6,	.width = 2 },	/* ibias	*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(lpll_clk, "lpll", "lpll-gate", 0x20,
				   3, ftable, f_lpll, 240);
static CLK_FIXED_FACTOR(lpll_409m6, "lpll-409m6", "lpll", 3, 1, 0);
static CLK_FIXED_FACTOR(lpll_245m76, "lpll-245m76", "lpll", 5, 1, 0);

static const struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 95,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 6,	.width = 2 },	/* ibias	*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 80,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(gpll_clk, "gpll", "gpll-gate", 0x38,
				   3, ftable, f_gpll, 240,
				   1000, 1000, 1, 400000000);

#define f_isppll f_gpll
static SPRD_PLL_WITH_ITABLE_1K(isppll_clk, "isppll", "isppll-gate", 0x50,
				   3, ftable, f_isppll, 240);
static CLK_FIXED_FACTOR(isppll_468m, "isppll-468m", "isppll", 2, 1, 0);

static struct sprd_clk_common *sc9863a_pll_clks[] = {
	/* address base is 0x40353000 */
	&twpll_clk.common,
	&lpll_clk.common,
	&gpll_clk.common,
	&isppll_clk.common,
};

static struct clk_hw_onecell_data sc9863a_pll_hws = {
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
		[CLK_LPLL_409M6]	= &lpll_409m6.hw,
		[CLK_LPLL_245M76]	= &lpll_245m76.hw,
		[CLK_GPLL]		= &gpll_clk.common.hw,
		[CLK_ISPPLL]		= &isppll_clk.common.hw,
		[CLK_ISPPLL_468M]	= &isppll_468m.hw,

	},
	.num	= CLK_ANLG_PHY_G1_NUM,
};

static const struct sprd_clk_desc sc9863a_pll_desc = {
	.clk_clks	= sc9863a_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_pll_clks),
	.hw_clks        = &sc9863a_pll_hws,
};

#define f_mpll f_gpll
static const struct freq_table ftable_mpll[6] = {
	{ .ibias = 0, .max_freq = 1000000000ULL },
	{ .ibias = 1, .max_freq = 1200000000ULL },
	{ .ibias = 2, .max_freq = 1400000000ULL },
	{ .ibias = 3, .max_freq = 1600000000ULL },
	{ .ibias = 4, .max_freq = 1800000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll0_clk, "mpll0", "mpll0-gate", 0x0,
				   3, ftable_mpll, f_mpll, 240,
				   1000, 1000, 1, 1000000000);
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll1_clk, "mpll1", "mpll1-gate", 0x18,
				   3, ftable_mpll, f_mpll, 240,
				   1000, 1000, 1, 1000000000);
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll2_clk, "mpll2", "mpll2-gate", 0x30,
				   3, ftable_mpll, f_mpll, 240,
				   1000, 1000, 1, 1000000000);
static CLK_FIXED_FACTOR(mpll2_675m, "mpll2-675m", "mpll2", 2, 1, 0);

static struct sprd_clk_common *sc9863a_mpll_clks[] = {
	/* address base is 0x40359000 */
	&mpll0_clk.common,
	&mpll1_clk.common,
	&mpll2_clk.common,
};

static struct clk_hw_onecell_data sc9863a_mpll_hws = {
	.hws	= {
		[CLK_MPLL0]		= &mpll0_clk.common.hw,
		[CLK_MPLL1]		= &mpll1_clk.common.hw,
		[CLK_MPLL2]		= &mpll2_clk.common.hw,
		[CLK_MPLL2_675M]	= &mpll2_675m.hw,

	},
	.num	= CLK_ANLG_PHY_G4_NUM,
};

static const struct sprd_clk_desc sc9863a_mpll_desc = {
	.clk_clks	= sc9863a_mpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_mpll_clks),
	.hw_clks        = &sc9863a_mpll_hws,
};

static SPRD_SC_GATE_CLK(audio_gate,	"audio-gate",	"ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

#define f_rpll f_lpll
static SPRD_PLL_WITH_ITABLE_1K(rpll_clk, "rpll", "ext-26m", 0x10,
				   3, ftable, f_rpll, 240);

static CLK_FIXED_FACTOR(rpll_390m, "rpll-390m", "rpll", 2, 1, 0);
static CLK_FIXED_FACTOR(rpll_260m, "rpll-260m", "rpll", 3, 1, 0);
static CLK_FIXED_FACTOR(rpll_195m, "rpll-195m", "rpll", 4, 1, 0);
static CLK_FIXED_FACTOR(rpll_26m, "rpll-26m", "rpll", 30, 1, 0);

static struct sprd_clk_common *sc9863a_rpll_clks[] = {
	/* address base is 0x4035c000 */
	&audio_gate.common,
	&rpll_clk.common,
};

static struct clk_hw_onecell_data sc9863a_rpll_hws = {
	.hws	= {
		[CLK_AUDIO_GATE]	= &audio_gate.common.hw,
		[CLK_RPLL]		= &rpll_clk.common.hw,
		[CLK_RPLL_390M]		= &rpll_390m.hw,
		[CLK_RPLL_260M]		= &rpll_260m.hw,
		[CLK_RPLL_195M]		= &rpll_195m.hw,
		[CLK_RPLL_26M]		= &rpll_26m.hw,
	},
	.num	= CLK_ANLG_PHY_G5_NUM,
};

static const struct sprd_clk_desc sc9863a_rpll_desc = {
	.clk_clks	= sc9863a_rpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_rpll_clks),
	.hw_clks        = &sc9863a_rpll_hws,
};

#define f_dpll f_lpll
static const struct freq_table ftable_dpll[5] = {
	{ .ibias = 0, .max_freq = 1211000000ULL },
	{ .ibias = 1, .max_freq = 1320000000ULL },
	{ .ibias = 2, .max_freq = 1570000000ULL },
	{ .ibias = 3, .max_freq = 1866000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static SPRD_PLL_WITH_ITABLE_1K(dpll0_clk, "dpll0", "dpll0-gate", 0x0,
				   3, ftable_dpll, f_dpll, 240);
static SPRD_PLL_WITH_ITABLE_1K(dpll1_clk, "dpll1", "dpll1-gate", 0x18,
				   3, ftable_dpll, f_dpll, 240);

static CLK_FIXED_FACTOR(dpll0_933m, "dpll0-933m", "dpll0", 2, 1, 0);
static CLK_FIXED_FACTOR(dpll0_622m3, "dpll0-622m3", "dpll0", 3, 1, 0);
static CLK_FIXED_FACTOR(dpll1_400m, "dpll1-400m", "dpll1", 4, 1, 0);
static CLK_FIXED_FACTOR(dpll1_266m7, "dpll1-266m7", "dpll1", 6, 1, 0);
static CLK_FIXED_FACTOR(dpll1_123m1, "dpll1-123m1", "dpll1", 13, 1, 0);
static CLK_FIXED_FACTOR(dpll1_50m, "dpll1-50m", "dpll1", 32, 1, 0);

static struct sprd_clk_common *sc9863a_dpll_clks[] = {
	/* address base is 0x40363000 */
	&dpll0_clk.common,
	&dpll1_clk.common,
};

static struct clk_hw_onecell_data sc9863a_dpll_hws = {
	.hws	= {
		[CLK_DPLL0]		= &dpll0_clk.common.hw,
		[CLK_DPLL1]		= &dpll1_clk.common.hw,
		[CLK_DPLL0_933M]	= &dpll0_933m.hw,
		[CLK_DPLL0_622M3]	= &dpll0_622m3.hw,
		[CLK_DPLL0_400M]	= &dpll1_400m.hw,
		[CLK_DPLL0_266M7]	= &dpll1_266m7.hw,
		[CLK_DPLL0_123M1]	= &dpll1_123m1.hw,
		[CLK_DPLL0_50M]		= &dpll1_50m.hw,

	},
	.num	= CLK_ANLG_PHY_G7_NUM,
};

static const struct sprd_clk_desc sc9863a_dpll_desc = {
	.clk_clks	= sc9863a_dpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_dpll_clks),
	.hw_clks        = &sc9863a_dpll_hws,
};

#define SC9863A_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* ap clocks */
static const char * const ap_apb_parents[] = { "ext-26m", "twpll-64m",
					       "twpll-96m", "twpll-128m" };
static SPRD_MUX_CLK(ap_apb_clk, "ap-apb-clk", ap_apb_parents, 0x20,
			0, 2, SC9863A_MUX_FLAG);

static const char * const ap_ce_parents[] = { "ext-26m", "twpll-256m"};
static SPRD_COMP_CLK(ap_ce, "ap-ce", ap_ce_parents, 0x24,
		     0, 1, 8, 3, 0);

static const char * const nandc_ecc_parents[] = { "ext-26m", "twpll-256m",
						"twpll-307m2" };
static SPRD_COMP_CLK(nandc_ecc_clk, "nandc-ecc-clk", nandc_ecc_parents, 0x28,
		     0, 2, 8, 3, 0);

static const char * const nandc_26m_parents[] = { "ext-32k", "ext-26m" };
static SPRD_MUX_CLK(nandc_26m_clk, "nandc-26m-clk", nandc_26m_parents, 0x2c,
			0, 1, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(emmc_32k_clk, "emmc-32k-clk", nandc_26m_parents, 0x30,
			0, 1, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(sdio0_32k_clk, "sdio0-32k-clk", nandc_26m_parents, 0x34,
			0, 1, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(sdio1_32k_clk, "sdio1-32k-clk", nandc_26m_parents, 0x38,
			0, 1, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(sdio2_32k_clk, "sdio2-32k-clk", nandc_26m_parents, 0x3c,
			0, 1, SC9863A_MUX_FLAG);

static SPRD_GATE_CLK(otg_utmi, "otg-utmi", "aon-apb-clk", 0x40,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const ap_uart_parents[] = { "ext-26m", "twpll-48m",
					"twpll-51m2", "twpll-96m" };
static SPRD_COMP_CLK(ap_uart0_clk,	"ap-uart0-clk",	ap_uart_parents, 0x44,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_uart1_clk,	"ap-uart1-clk",	ap_uart_parents, 0x48,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_uart2_clk,	"ap-uart2-clk",	ap_uart_parents, 0x4c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_uart3_clk,	"ap-uart3-clk",	ap_uart_parents, 0x50,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_uart4_clk,	"ap-uart4-clk",	ap_uart_parents, 0x54,
		     0, 2, 8, 3, 0);

static const char * const i2c_parents[] = { "ext-26m", "twpll-48m",
					    "twpll-51m2", "twpll-153m6" };
static SPRD_COMP_CLK(ap_i2c0_clk, "ap-i2c0-clk", i2c_parents, 0x58,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c1_clk, "ap-i2c1-clk", i2c_parents, 0x5c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c2_clk, "ap-i2c2-clk", i2c_parents, 0x60,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c3_clk, "ap-i2c3-clk", i2c_parents, 0x64,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c4_clk, "ap-i2c4-clk", i2c_parents, 0x68,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c5_clk, "ap-i2c5-clk", i2c_parents, 0x6c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c6_clk, "ap-i2c6-clk", i2c_parents, 0x70,
		     0, 2, 8, 3, 0);

static const char * const spi_parents[] = { "ext-26m", "twpll-128m",
					"twpll-153m6", "twpll-192m" };
static SPRD_COMP_CLK(ap_spi0_clk, "ap-spi0-clk", spi_parents, 0x74,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi1_clk, "ap-spi1-clk", spi_parents, 0x78,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi2_clk, "ap-spi2-clk", spi_parents, 0x7c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi3_clk, "ap-spi3-clk", spi_parents, 0x80,
		     0, 2, 8, 3, 0);

static const char * const iis_parents[] = { "ext-26m",
					    "twpll-128m",
					    "twpll-153m6" };
static SPRD_COMP_CLK(ap_iis0_clk, "ap-iis0-clk", iis_parents, 0x84,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_iis1_clk, "ap-iis1-clk", iis_parents, 0x88,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_iis2_clk, "ap-iis2-clk", iis_parents, 0x8c,
		     0, 2, 8, 3, 0);

static const char * const sim0_parents[] = { "ext-26m", "twpll-51m2",
					"twpll-64m", "twpll-96m",
					"twpll-128m" };
static SPRD_COMP_CLK(sim0_clk, "sim0-clk", sim0_parents, 0x90,
		     0, 3, 8, 3, 0);

static const char * const sim0_32k_parents[] = { "ext-32k", "ext-26m" };
static SPRD_MUX_CLK(sim0_32k_clk, "sim0-32k-clk", sim0_32k_parents, 0x94,
			0, 1, SC9863A_MUX_FLAG);

static struct sprd_clk_common *sc9863a_ap_clks[] = {
	/* address base is 0x21500000 */
	&ap_apb_clk.common,
	&ap_ce.common,
	&nandc_ecc_clk.common,
	&nandc_26m_clk.common,
	&emmc_32k_clk.common,
	&sdio0_32k_clk.common,
	&sdio1_32k_clk.common,
	&sdio2_32k_clk.common,
	&otg_utmi.common,
	&ap_uart0_clk.common,
	&ap_uart1_clk.common,
	&ap_uart2_clk.common,
	&ap_uart3_clk.common,
	&ap_uart4_clk.common,
	&ap_i2c0_clk.common,
	&ap_i2c1_clk.common,
	&ap_i2c2_clk.common,
	&ap_i2c3_clk.common,
	&ap_i2c4_clk.common,
	&ap_i2c5_clk.common,
	&ap_i2c6_clk.common,
	&ap_spi0_clk.common,
	&ap_spi1_clk.common,
	&ap_spi2_clk.common,
	&ap_spi3_clk.common,
	&ap_iis0_clk.common,
	&ap_iis1_clk.common,
	&ap_iis2_clk.common,
	&sim0_clk.common,
	&sim0_32k_clk.common,
};

static struct clk_hw_onecell_data sc9863a_ap_clk_hws = {
	.hws	= {
		[CLK_AP_APB]	= &ap_apb_clk.common.hw,
		[CLK_AP_CE]	= &ap_ce.common.hw,
		[CLK_NANDC_ECC]	= &nandc_ecc_clk.common.hw,
		[CLK_NANDC_26M]	= &nandc_26m_clk.common.hw,
		[CLK_EMMC_32K]	= &emmc_32k_clk.common.hw,
		[CLK_SDIO0_32K]	= &sdio0_32k_clk.common.hw,
		[CLK_SDIO1_32K]	= &sdio1_32k_clk.common.hw,
		[CLK_SDIO2_32K]	= &sdio2_32k_clk.common.hw,
		[CLK_OTG_UTMI]	= &otg_utmi.common.hw,
		[CLK_AP_UART0]	= &ap_uart0_clk.common.hw,
		[CLK_AP_UART1]	= &ap_uart1_clk.common.hw,
		[CLK_AP_UART2]	= &ap_uart2_clk.common.hw,
		[CLK_AP_UART3]	= &ap_uart3_clk.common.hw,
		[CLK_AP_UART4]	= &ap_uart4_clk.common.hw,
		[CLK_AP_I2C0]	= &ap_i2c0_clk.common.hw,
		[CLK_AP_I2C1]	= &ap_i2c1_clk.common.hw,
		[CLK_AP_I2C2]	= &ap_i2c2_clk.common.hw,
		[CLK_AP_I2C3]	= &ap_i2c3_clk.common.hw,
		[CLK_AP_I2C4]	= &ap_i2c4_clk.common.hw,
		[CLK_AP_I2C5]	= &ap_i2c5_clk.common.hw,
		[CLK_AP_I2C6]	= &ap_i2c6_clk.common.hw,
		[CLK_AP_SPI0]	= &ap_spi0_clk.common.hw,
		[CLK_AP_SPI1]	= &ap_spi1_clk.common.hw,
		[CLK_AP_SPI2]	= &ap_spi2_clk.common.hw,
		[CLK_AP_SPI3]	= &ap_spi3_clk.common.hw,
		[CLK_AP_IIS0]	= &ap_iis0_clk.common.hw,
		[CLK_AP_IIS1]	= &ap_iis1_clk.common.hw,
		[CLK_AP_IIS2]	= &ap_iis2_clk.common.hw,
		[CLK_SIM0]	= &sim0_clk.common.hw,
		[CLK_SIM0_32K]	= &sim0_32k_clk.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static const struct sprd_clk_desc sc9863a_ap_clk_desc = {
	.clk_clks	= sc9863a_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_ap_clks),
	.hw_clks	= &sc9863a_ap_clk_hws,
};

/* aon clocks */
static CLK_FIXED_FACTOR(clk_13m,	"clk-13m",	"ext-26m",
			2, 1, 0);
static CLK_FIXED_FACTOR(clk_6m5,	"clk-6m5",	"ext-26m",
			4, 1, 0);
static CLK_FIXED_FACTOR(clk_4m3,	"clk-4m3",	"ext-26m",
			6, 1, 0);
static CLK_FIXED_FACTOR(clk_2m,		"clk-2m",	"ext-26m",
			13, 1, 0);
static CLK_FIXED_FACTOR(clk_250k,	"clk-250k",	"ext-26m",
			104, 1, 0);

static CLK_FIXED_FACTOR(fac_rco_25m,	"rco-25m",	"rco-100m",
			4, 1, 0);
static CLK_FIXED_FACTOR(fac_rco_4m,	"rco-4m",	"rco-100m",
			25, 1, 0);
static CLK_FIXED_FACTOR(fac_rco_2m,	"rco-2m",	"rco-100m",
			50, 1, 0);

static const char * const emc_clk_parents[] = { "ext-26m", "twpll-384m",
						 "twpll-512m", "twpll-768m",
						 "twpll" };
static SPRD_MUX_CLK(emc_clk, "emc-clk", emc_clk_parents, 0x220,
			0, 3, SC9863A_MUX_FLAG);

static const char * const aon_apb_parents[] = { "rco-4m",	"rco-25m",
						"ext-26m",	"twpll-96m",
						"rco-100m",	"twpll-128m" };
static SPRD_COMP_CLK(aon_apb, "aon-apb", aon_apb_parents, 0x224,
		     0, 3, 8, 2, 0);

static const char * const adi_parents[] = { "rco-4m", "rco-25m", "ext-26m",
					    "twpll-38m4", "twpll-51m2" };
static SPRD_MUX_CLK(adi_clk, "adi-clk", adi_parents, 0x228,
			0, 3, SC9863A_MUX_FLAG);

static const char * const aux_parents[] = { "ext-32k", "rpll-26m", "ext-26m" };
static SPRD_COMP_CLK(aux0_clk, "aux0-clk", aux_parents, 0x22c,
		     0, 5, 8, 4, 0);
static SPRD_COMP_CLK(aux1_clk, "aux1-clk", aux_parents, 0x230,
		     0, 5, 8, 4, 0);
static SPRD_COMP_CLK(aux2_clk, "aux2-clk", aux_parents, 0x234,
		     0, 5, 8, 4, 0);
static SPRD_COMP_CLK(probe_clk, "probe-clk", aux_parents, 0x238,
		     0, 5, 8, 4, 0);

static const char * const pwm_parents[] = { "clk-32k", "rpll-26m",
					"ext-26m", "twpll-48m" };
static SPRD_MUX_CLK(pwm0_clk, "pwm0-clk", pwm_parents, 0x23c,
			0, 2, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(pwm1_clk, "pwm1-clk", pwm_parents, 0x240,
			0, 2, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(pwm2_clk, "pwm2-clk", pwm_parents, 0x244,
			0, 2, SC9863A_MUX_FLAG);

static const char * const aon_thm_parents[] = { "ext-32k", "clk-250k" };
static SPRD_MUX_CLK(aon_thm_clk, "aon-thm-clk", aon_thm_parents, 0x25c,
		    0, 1, SC9863A_MUX_FLAG);

static const char * const audif_parents[] = { "ext-26m", "twpll-38m4",
					      "twpll-51m2" };
static SPRD_MUX_CLK(audif_clk, "audif-clk", audif_parents, 0x264,
		    0, 2, SC9863A_MUX_FLAG);

static const char * const cpu_dap_parents[] = { "rco-4m", "rco-25m", "ext-26m",
						"twpll-76m8", "rco-100m",
						"twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(cpu_dap_clk, "cpu-dap-clk", cpu_dap_parents, 0x26c,
		    0, 3, SC9863A_MUX_FLAG);

static const char * const cpu_ts_parents[] = { "ext-32k", "ext-26m",
					       "twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(cpu_ts_clk, "cpu-ts-clk", cpu_ts_parents, 0x274,
		    0, 2, SC9863A_MUX_FLAG);

static const char * const djtag_tck_parents[] = { "rco-4m", "ext-26m" };
static SPRD_MUX_CLK(djtag_tck_clk, "djtag-tck-clk", djtag_tck_parents, 0x28c,
			0, 1, SC9863A_MUX_FLAG);

static const char * const emc_ref_parents[] = { "clk-6m5", "clk-13m", "ext-26m" };
static SPRD_MUX_CLK(emc_ref_clk, "emc-ref-clk", emc_ref_parents, 0x29c,
			0, 2, SC9863A_MUX_FLAG);

static const char * const cssys_parents[] = { "rco-4m", "ext-26m", "twpll-96m",
					      "rco-100m", "twpll-128m",
					      "twpll-153m6", "twpll-384m",
					      "twpll-512m", "mpll2-675m" };
static SPRD_COMP_CLK(cssys_clk,	"cssys-clk", cssys_parents, 0x2a0,
		     0, 4, 8, 2, 0);

static const char * const aon_pmu_parents[] = { "ext-32k", "rco-4m", "ext-4m" };
static SPRD_MUX_CLK(aon_pmu_clk, "aon-pmu-clk", aon_pmu_parents, 0x2a8,
			0, 2, SC9863A_MUX_FLAG);

static const char * const pmu_26m_parents[] = { "rco-4m", "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(pmu_26m_clk, "26m-pmu-clk", pmu_26m_parents, 0x2ac,
			0, 2, SC9863A_MUX_FLAG);

static const char * const aon_tmr_parents[] = { "rco-4m", "ext-26m" };
static SPRD_MUX_CLK(aon_tmr_clk, "aon-tmr-clk", aon_tmr_parents, 0x2b0,
			0, 1, SC9863A_MUX_FLAG);

static const char * const power_cpu_parents[] = { "ext-26m", "rco-25m",
						  "rco-100m", "twpll-128m" };
static SPRD_MUX_CLK(power_cpu_clk, "power-cpu-clk", power_cpu_parents, 0x2c4,
			0, 2, SC9863A_MUX_FLAG);

static const char * const ap_axi_parents[] = { "ext-26m", "twpll-76m8",
					       "twpll-128m", "twpll-256m" };
static SPRD_MUX_CLK(ap_axi, "ap-axi", ap_axi_parents, 0x2c8,
		    0, 2, SC9863A_MUX_FLAG);

static const char * const sdio_parents[] = { "ext-26m", "twpll-307m2",
					     "twpll-384m", "rpll-390m",
					     "dpll1-400m", "lpll-409m6" };
static SPRD_MUX_CLK(sdio0_2x, "sdio0-2x", sdio_parents, 0x2cc,
			0, 3, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(sdio1_2x, "sdio1-2x", sdio_parents, 0x2d4,
			0, 3, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(sdio2_2x, "sdio2-2x", sdio_parents, 0x2dc,
			0, 3, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(emmc_2x, "emmc-2x", sdio_parents, 0x2e4,
			0, 3, SC9863A_MUX_FLAG);

static const char * const dpu_parents[] = { "twpll-153m6", "twpll-192m",
					    "twpll-256m", "twpll-384m"};
static SPRD_MUX_CLK(dpu_clk, "dpu", dpu_parents, 0x2f4,
			0, 2, SC9863A_MUX_FLAG);

static const char * const dpu_dpi_parents[] = { "twpll-128m", "twpll-153m6",
					    "twpll-192m" };
static SPRD_COMP_CLK(dpu_dpi,	"dpu-dpi", dpu_dpi_parents, 0x2f8,
		     0, 2, 8, 4, 0);

static const char * const otg_ref_parents[] = { "twpll-12m", "ext-26m" };
static SPRD_MUX_CLK(otg_ref_clk, "otg-ref-clk", otg_ref_parents, 0x308,
			0, 1, SC9863A_MUX_FLAG);

static const char * const sdphy_apb_parents[] = { "ext-26m", "twpll-48m" };
static SPRD_MUX_CLK(sdphy_apb_clk, "sdphy-apb-clk", sdphy_apb_parents, 0x330,
			0, 1, SC9863A_MUX_FLAG);

static const char * const alg_io_apb_parents[] = { "rco-4m", "ext-26m",
						   "twpll-48m", "twpll-96m" };
static SPRD_MUX_CLK(alg_io_apb_clk, "alg-io-apb-clk", alg_io_apb_parents, 0x33c,
			0, 1, SC9863A_MUX_FLAG);

static const char * const gpu_parents[] = { "twpll-153m6", "twpll-192m",
						 "twpll-256m", "twpll-307m2",
						 "twpll-384m", "twpll-512m",
						 "gpll" };
static SPRD_COMP_CLK(gpu_core, "gpu-core", gpu_parents, 0x344,
		     0, 3, 8, 2, 0);
static SPRD_COMP_CLK(gpu_soc, "gpu-soc", gpu_parents, 0x348,
		     0, 3, 8, 2, 0);

static const char * const mm_emc_parents[] = { "ext-26m", "twpll-384m",
					    "isppll-468m", "twpll-512m"};
static SPRD_MUX_CLK(mm_emc, "mm-emc", mm_emc_parents, 0x350,
			0, 2, SC9863A_MUX_FLAG);

static const char * const mm_ahb_parents[] = { "ext-26m", "twpll-96m",
					    "twpll-128m", "twpll-153m6"};
static SPRD_MUX_CLK(mm_ahb, "mm-ahb", mm_ahb_parents, 0x354,
			0, 2, SC9863A_MUX_FLAG);

static const char * const bpc_clk_parents[] = { "twpll-192m", "twpll-307m2",
					    "twpll-384m", "isppll-468m",
					    "dpll0-622m3" };
static SPRD_MUX_CLK(bpc_clk, "bpc-clk", bpc_clk_parents, 0x358,
			0, 3, SC9863A_MUX_FLAG);

static const char * const dcam_if_parents[] = { "twpll-192m", "twpll-256m",
						"twpll-307m2", "twpll-384m" };
static SPRD_MUX_CLK(dcam_if_clk, "dcam-if-clk", dcam_if_parents, 0x35c,
			0, 2, SC9863A_MUX_FLAG);

static const char * const isp_parents[] = { "twpll-128m", "twpll-256m",
					    "twpll-307m2", "twpll-384m",
					    "isppll-468m" };
static SPRD_MUX_CLK(isp_clk, "isp-clk", isp_parents, 0x360,
			0, 3, SC9863A_MUX_FLAG);

static const char * const jpg_parents[] = { "twpll-76m8", "twpll-128m",
					    "twpll-256m", "twpll-307m2" };
static SPRD_MUX_CLK(jpg_clk, "jpg-clk", jpg_parents, 0x364,
			0, 2, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(cpp_clk, "cpp-clk", jpg_parents, 0x368,
			0, 2, SC9863A_MUX_FLAG);

static const char * const sensor_parents[] = { "ext-26m", "twpll-48m",
					"twpll-76m8", "twpll-96m" };
static SPRD_COMP_CLK(sensor0_clk, "sensor0-clk", sensor_parents, 0x36c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(sensor1_clk, "sensor1-clk", sensor_parents, 0x370,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(sensor2_clk, "sensor2-clk", sensor_parents, 0x374,
		     0, 2, 8, 3, 0);

static const char * const mm_vemc_parents[] = { "ext-26m", "twpll-307m2",
					    "twpll-384m", "isppll-468m"};
static SPRD_MUX_CLK(mm_vemc, "mm-vemc", mm_vemc_parents, 0x378,
			0, 2, SC9863A_MUX_FLAG);

static SPRD_MUX_CLK(mm_vahb, "mm-vahb", mm_ahb_parents, 0x37c,
			0, 2, SC9863A_MUX_FLAG);

static const char * const vsp_parents[] = { "twpll-76m8", "twpll-128m",
					    "twpll-256m", "twpll-307m2",
					    "twpll-384m"};
static SPRD_MUX_CLK(clk_vsp, "vsp-clk", vsp_parents, 0x380,
			0, 3, SC9863A_MUX_FLAG);

static const char * const core_parents[] = { "ext-26m", "twpll-512m",
					     "twpll-768m", "lpll", "dpll0",
					     "mpll2", "mpll0", "mpll1" };
static SPRD_COMP_CLK(core0_clk, "core0-clk", core_parents, 0xa20,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(core1_clk, "core1-clk", core_parents, 0xa24,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(core2_clk, "core2-clk", core_parents, 0xa28,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(core3_clk, "core3-clk", core_parents, 0xa2c,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(core4_clk, "core4-clk", core_parents, 0xa30,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(core5_clk, "core5-clk", core_parents, 0xa34,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(core6_clk, "core6-clk", core_parents, 0xa38,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(core7_clk, "core7-clk", core_parents, 0xa3c,
		     0, 3, 8, 3, 0);
static SPRD_COMP_CLK(scu_clk, "scu-clk", core_parents, 0xa40,
		     0, 3, 8, 3, 0);
static SPRD_DIV_CLK(ace_clk, "ace-clk", "scu-clk", 0xa44,
		    8, 3, 0);
static SPRD_DIV_CLK(axi_periph_clk, "axi-periph-clk", "scu-clk", 0xa48,
		    8, 3, 0);
static SPRD_DIV_CLK(axi_acp_clk, "axi-acp-clk", "scu-clk", 0xa4c,
		    8, 3, 0);

static const char * const atb_parents[] = { "ext-26m", "twpll-384m",
					    "twpll-512m", "mpll2" };
static SPRD_COMP_CLK(atb_clk, "atb-clk", atb_parents, 0xa50,
		     0, 2, 8, 3, 0);
static SPRD_DIV_CLK(debug_apb_clk, "debug-apb-clk", "atb-clk", 0xa54,
		    8, 3, 0);

static const char * const gic_parents[] = { "ext-26m", "twpll-153m6",
					    "twpll-384m", "twpll-512m" };
static SPRD_COMP_CLK(gic_clk, "gic-clk", gic_parents, 0xa58,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(periph_clk, "periph-clk", gic_parents, 0xa5c,
		     0, 2, 8, 3, 0);

static struct sprd_clk_common *sc9863a_aon_clks[] = {
	/* address base is 0x402d0000 */
	&emc_clk.common,
	&aon_apb.common,
	&adi_clk.common,
	&aux0_clk.common,
	&aux1_clk.common,
	&aux2_clk.common,
	&probe_clk.common,
	&pwm0_clk.common,
	&pwm1_clk.common,
	&pwm2_clk.common,
	&aon_thm_clk.common,
	&audif_clk.common,
	&cpu_dap_clk.common,
	&cpu_ts_clk.common,
	&djtag_tck_clk.common,
	&emc_ref_clk.common,
	&cssys_clk.common,
	&aon_pmu_clk.common,
	&pmu_26m_clk.common,
	&aon_tmr_clk.common,
	&power_cpu_clk.common,
	&ap_axi.common,
	&sdio0_2x.common,
	&sdio1_2x.common,
	&sdio2_2x.common,
	&emmc_2x.common,
	&dpu_clk.common,
	&dpu_dpi.common,
	&otg_ref_clk.common,
	&sdphy_apb_clk.common,
	&alg_io_apb_clk.common,
	&gpu_core.common,
	&gpu_soc.common,
	&mm_emc.common,
	&mm_ahb.common,
	&bpc_clk.common,
	&dcam_if_clk.common,
	&isp_clk.common,
	&jpg_clk.common,
	&cpp_clk.common,
	&sensor0_clk.common,
	&sensor1_clk.common,
	&sensor2_clk.common,
	&mm_vemc.common,
	&mm_vahb.common,
	&clk_vsp.common,
	&core0_clk.common,
	&core1_clk.common,
	&core2_clk.common,
	&core3_clk.common,
	&core4_clk.common,
	&core5_clk.common,
	&core6_clk.common,
	&core7_clk.common,
	&scu_clk.common,
	&ace_clk.common,
	&axi_periph_clk.common,
	&axi_acp_clk.common,
	&atb_clk.common,
	&debug_apb_clk.common,
	&gic_clk.common,
	&periph_clk.common,
};

static struct clk_hw_onecell_data sc9863a_aon_clk_hws = {
	.hws	= {
		[CLK_13M]		= &clk_13m.hw,
		[CLK_6M5]		= &clk_6m5.hw,
		[CLK_4M3]		= &clk_4m3.hw,
		[CLK_2M]		= &clk_2m.hw,
		[CLK_250K]		= &clk_250k.hw,
		[CLK_FAC_RCO25M]	= &fac_rco_25m.hw,
		[CLK_FAC_RCO4M]		= &fac_rco_4m.hw,
		[CLK_FAC_RCO2M]		= &fac_rco_2m.hw,
		[CLK_EMC]		= &emc_clk.common.hw,
		[CLK_AON_APB]		= &aon_apb.common.hw,
		[CLK_ADI]		= &adi_clk.common.hw,
		[CLK_AUX0]		= &aux0_clk.common.hw,
		[CLK_AUX1]		= &aux1_clk.common.hw,
		[CLK_AUX2]		= &aux2_clk.common.hw,
		[CLK_PROBE]		= &probe_clk.common.hw,
		[CLK_PWM0]		= &pwm0_clk.common.hw,
		[CLK_PWM1]		= &pwm1_clk.common.hw,
		[CLK_PWM2]		= &pwm2_clk.common.hw,
		[CLK_AON_THM]		= &aon_thm_clk.common.hw,
		[CLK_AUDIF]		= &audif_clk.common.hw,
		[CLK_CPU_DAP]		= &cpu_dap_clk.common.hw,
		[CLK_CPU_TS]		= &cpu_ts_clk.common.hw,
		[CLK_DJTAG_TCK]		= &djtag_tck_clk.common.hw,
		[CLK_EMC_REF]		= &emc_ref_clk.common.hw,
		[CLK_CSSYS]		= &cssys_clk.common.hw,
		[CLK_AON_PMU]		= &aon_pmu_clk.common.hw,
		[CLK_PMU_26M]		= &pmu_26m_clk.common.hw,
		[CLK_AON_TMR]		= &aon_tmr_clk.common.hw,
		[CLK_POWER_CPU]		= &power_cpu_clk.common.hw,
		[CLK_AP_AXI]		= &ap_axi.common.hw,
		[CLK_SDIO0_2X]		= &sdio0_2x.common.hw,
		[CLK_SDIO1_2X]		= &sdio1_2x.common.hw,
		[CLK_SDIO2_2X]		= &sdio2_2x.common.hw,
		[CLK_EMMC_2X]		= &emmc_2x.common.hw,
		[CLK_DPU]		= &dpu_clk.common.hw,
		[CLK_DPU_DPI]		= &dpu_dpi.common.hw,
		[CLK_OTG_REF]		= &otg_ref_clk.common.hw,
		[CLK_SDPHY_APB]		= &sdphy_apb_clk.common.hw,
		[CLK_ALG_IO_APB]	= &alg_io_apb_clk.common.hw,
		[CLK_GPU_CORE]		= &gpu_core.common.hw,
		[CLK_GPU_SOC]		= &gpu_soc.common.hw,
		[CLK_MM_EMC]		= &mm_emc.common.hw,
		[CLK_MM_AHB]		= &mm_ahb.common.hw,
		[CLK_BPC]		= &bpc_clk.common.hw,
		[CLK_DCAM_IF]		= &dcam_if_clk.common.hw,
		[CLK_ISP]		= &isp_clk.common.hw,
		[CLK_JPG]		= &jpg_clk.common.hw,
		[CLK_CPP]		= &cpp_clk.common.hw,
		[CLK_SENSOR0]		= &sensor0_clk.common.hw,
		[CLK_SENSOR1]		= &sensor1_clk.common.hw,
		[CLK_SENSOR2]		= &sensor2_clk.common.hw,
		[CLK_MM_VEMC]		= &mm_vemc.common.hw,
		[CLK_MM_VAHB]		= &mm_vahb.common.hw,
		[CLK_VSP]		= &clk_vsp.common.hw,
		[CLK_CORE0]		= &core0_clk.common.hw,
		[CLK_CORE1]		= &core1_clk.common.hw,
		[CLK_CORE2]		= &core2_clk.common.hw,
		[CLK_CORE3]		= &core3_clk.common.hw,
		[CLK_CORE4]		= &core4_clk.common.hw,
		[CLK_CORE5]		= &core5_clk.common.hw,
		[CLK_CORE6]		= &core6_clk.common.hw,
		[CLK_CORE7]		= &core7_clk.common.hw,
		[CLK_SCU]		= &scu_clk.common.hw,
		[CLK_ACE]		= &ace_clk.common.hw,
		[CLK_AXI_PERIPH]	= &axi_periph_clk.common.hw,
		[CLK_AXI_ACP]		= &axi_acp_clk.common.hw,
		[CLK_ATB]		= &atb_clk.common.hw,
		[CLK_DEBUG_APB]		= &debug_apb_clk.common.hw,
		[CLK_GIC]		= &gic_clk.common.hw,
		[CLK_PERIPH]		= &periph_clk.common.hw,
	},
	.num	= CLK_AON_CLK_NUM,
};

static const struct sprd_clk_desc sc9863a_aon_clk_desc = {
	.clk_clks	= sc9863a_aon_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_aon_clks),
	.hw_clks	= &sc9863a_aon_clk_hws,
};

static SPRD_SC_GATE_CLK(otg_eb, "otg-eb", "ap-axi", 0x0, 0x1000,
			BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_eb, "dma-eb", "ap-axi", 0x0, 0x1000,
			BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_eb, "ce-eb", "ap-axi", 0x0, 0x1000,
			BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_eb, "nandc-eb", "ap-axi", 0x0, 0x1000,
			BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_eb, "sdio0-eb", "ap-axi", 0x0, 0x1000,
			BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_eb, "sdio1-eb", "ap-axi", 0x0, 0x1000,
			BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_eb, "sdio2-eb", "ap-axi", 0x0, 0x1000,
			BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_eb, "emmc-eb", "ap-axi", 0x0, 0x1000,
			BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb, "emmc-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_32k_eb, "sdio0-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_32k_eb, "sdio1-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_32k_eb, "sdio2-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_26m_eb, "nandc-26m-eb", "ap-axi", 0x0, 0x1000,
			BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_eb2, "dma-eb2", "ap-axi", 0x18, 0x1000,
			BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_eb2, "ce-eb2", "ap-axi", 0x18, 0x1000,
			BIT(1), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9863a_apahb_gate_clks[] = {
	/* address base is 0x20e00000 */
	&otg_eb.common,
	&dma_eb.common,
	&ce_eb.common,
	&nandc_eb.common,
	&sdio0_eb.common,
	&sdio1_eb.common,
	&sdio2_eb.common,
	&emmc_eb.common,
	&emmc_32k_eb.common,
	&sdio0_32k_eb.common,
	&sdio1_32k_eb.common,
	&sdio2_32k_eb.common,
	&nandc_26m_eb.common,
	&dma_eb2.common,
	&ce_eb2.common,
};

static struct clk_hw_onecell_data sc9863a_apahb_gate_hws = {
	.hws	= {
		[CLK_OTG_EB]		= &otg_eb.common.hw,
		[CLK_DMA_EB]		= &dma_eb.common.hw,
		[CLK_CE_EB]		= &ce_eb.common.hw,
		[CLK_NANDC_EB]		= &nandc_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO2_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_EMMC_32K_EB]	= &emmc_32k_eb.common.hw,
		[CLK_SDIO0_32K_EB]	= &sdio0_32k_eb.common.hw,
		[CLK_SDIO1_32K_EB]	= &sdio1_32k_eb.common.hw,
		[CLK_SDIO2_32K_EB]	= &sdio2_32k_eb.common.hw,
		[CLK_NANDC_26M_EB]	= &nandc_26m_eb.common.hw,
		[CLK_DMA_EB2]		= &dma_eb2.common.hw,
		[CLK_CE_EB2]		= &ce_eb2.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_apahb_gate_desc = {
	.clk_clks	= sc9863a_apahb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_apahb_gate_clks),
	.hw_clks	= &sc9863a_apahb_gate_hws,
};

/* aon gate clocks */
static SPRD_SC_GATE_CLK(adc_eb,	"adc-eb",	"aon-apb", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(fm_eb,	"fm-eb",	"aon-apb", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tpc_eb,	"tpc-eb",	"aon-apb", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpio_eb, "gpio-eb",	"aon-apb", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm0_eb,	"pwm0-eb",	"aon-apb", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm1_eb,	"pwm1-eb",	"aon-apb", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm2_eb,	"pwm2-eb",	"aon-apb", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm3_eb,	"pwm3-eb",	"aon-apb", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_eb,		"kpd-eb",	"aon-apb", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_eb,	"aon-syst-eb",	"aon-apb", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_eb,	"ap-syst-eb",	"aon-apb", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_eb,	"aon-tmr-eb",	"aon-apb", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_eb,	"ap-tmr0-eb",	"aon-apb", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(efuse_eb,	"efuse-eb",	"aon-apb", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_eb,		"eic-eb",	"aon-apb", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(intc_eb,	"intc-eb",	"aon-apb", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(adi_eb,		"adi-eb",	"aon-apb", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audif_eb,	"audif-eb",	"aon-apb", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aud_eb,		"aud-eb",	"aon-apb", 0x0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vbc_eb,		"vbc-eb",	"aon-apb", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_eb,		"pin-eb",	"aon-apb", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipi_eb,		"ipi-eb",	"aon-apb", 0x0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(splk_eb,	"splk-eb",	"aon-apb", 0x0,
		     0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mspi_eb,	"mspi-eb",	"aon-apb", 0x0,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb,	"ap-wdg-eb",	"aon-apb", 0x0,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_eb,		"mm-eb",	"aon-apb", 0x0,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_ckg_eb,	"aon-apb-ckg-eb", "aon-apb", 0x0,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_ts0_eb, "ca53-ts0-eb",	"aon-apb", 0x0,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_ts1_eb, "ca53-ts1-eb",	"aon-apb", 0x0,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_dap_eb, "ca53-dap-eb",	"aon-apb", 0x0,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c_eb,		"i2c-eb",	"aon-apb", 0x0,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_eb,		"pmu-eb",		"aon-apb",
			0x4, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm_eb,		"thm-eb",		"aon-apb",
			0x4, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb,	"aux0-eb",		"aon-apb",
			0x4, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux1_eb,	"aux1-eb",		"aon-apb",
			0x4, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux2_eb,	"aux2-eb",		"aon-apb",
			0x4, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(probe_eb,	"probe-eb",		"aon-apb",
			0x4, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emc_ref_eb,	"emc-ref-eb",		"aon-apb",
			0x4, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_wdg_eb,	"ca53-wdg-eb",		"aon-apb",
			0x4, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb,	"ap-tmr1-eb",		"aon-apb",
			0x4, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb,	"ap-tmr2-eb",		"aon-apb",
			0x4, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(disp_emc_eb,	"disp-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(zip_emc_eb,	"zip-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gsp_emc_eb,	"gsp-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_vsp_eb,	"mm-vsp-eb",		"aon-apb",
			0x4, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mdar_eb,	"mdar-eb",		"aon-apb",
			0x4, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtc4m0_cal_eb,	"rtc4m0-cal-eb",	"aon-apb",
			0x4, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtc4m1_cal_eb,	"rtc4m1-cal-eb",	"aon-apb",
			0x4, 0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb,	"djtag-eb",		"aon-apb",
			0x4, 0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb,	"mbox-eb",		"aon-apb",
			0x4, 0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_dma_eb,	"aon-dma-eb",		"aon-apb",
			0x4, 0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_def_eb,	"aon-apb-def-eb",	"aon-apb",
			0x4, 0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(orp_jtag_eb,	"orp-jtag-eb",		"aon-apb",
			0x4, 0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dbg_eb,		"dbg-eb",		"aon-apb",
			0x4, 0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dbg_emc_eb,	"dbg-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cross_trig_eb,	"cross-trig-eb",	"aon-apb",
			0x4, 0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes_dphy_eb,	"serdes-dphy-eb",	"aon-apb",
			0x4, 0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(arch_rtc_eb,	"arch-rtc-eb",	"aon-apb",
			0x10, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_rtc_eb,	"kpd-rtc-eb",	"aon-apb",
			0x10, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_rtc_eb, "aon-syst-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_rtc_eb,	"ap-syst-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_rtc_eb,	"aon-tmr-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_rtc_eb,	"ap-tmr0-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtc_eb, "eic-rtc-eb",	"aon-apb",
			0x10, 0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtcdv5_eb,	"eic-rtcdv5-eb", "aon-apb",
			0x10, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_rtc_eb,	"ap-wdg-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_wdg_rtc_eb, "ca53-wdg-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm_rtc_eb, "thm-rtc-eb",	"aon-apb",
			0x10, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(athma_rtc_eb, "athma-rtc-eb",	"aon-apb",
			0x10, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gthma_rtc_eb,	"gthma-rtc-eb",	"aon-apb",
			0x10, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(athma_rtc_a_eb,	"athma-rtc-a-eb", "aon-apb",
			0x10, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gthma_rtc_a_eb, "gthma-rtc-a-eb", "aon-apb",
			0x10, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_rtc_eb,	"ap-tmr2-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dxco_lc_rtc_eb, "dxco-lc-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bb_cal_rtc_eb, "bb-cal-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb,		"gpu-eb",	"aon-apb", 0x50,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(disp_eb,	"disp-eb",	"aon-apb", 0x50,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_emc_eb,		"mm-emc-eb",	"aon-apb", 0x50,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(power_cpu_eb,	"power-cpu-eb",	"aon-apb", 0x50,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(hw_i2c_eb,		"hw-i2c-eb",	"aon-apb", 0x50,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_vsp_emc_eb, "mm-vsp-emc-eb",	"aon-apb", 0x50,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vsp_eb,		"vsp-eb",	"aon-apb", 0x50,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_eb, "cssys-eb", "aon-apb", 0xb0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_eb, "dmc-eb", "aon-apb", 0xb0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rosc_eb, "rosc-eb", "aon-apb", 0xb0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(s_d_cfg_eb, "s-d-cfg-eb", "aon-apb", 0xb0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(s_d_ref_eb, "s-d-ref-eb", "aon-apb", 0xb0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(b_dma_eb, "b-dma-eb", "aon-apb", 0xb0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(anlg_eb, "anlg-eb", "aon-apb", 0xb0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(anlg_apb_eb, "anlg-apb-eb", "aon-apb", 0xb0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bsmtmr_eb, "bsmtmr-eb", "aon-apb", 0xb0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_axi_eb, "ap-axi-eb", "aon-apb", 0xb0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc0_eb, "ap-intc0-eb", "aon-apb", 0xb0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc1_eb, "ap-intc1-eb", "aon-apb", 0xb0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc2_eb, "ap-intc2-eb", "aon-apb", 0xb0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc3_eb, "ap-intc3-eb", "aon-apb", 0xb0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc4_eb, "ap-intc4-eb", "aon-apb", 0xb0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc5_eb, "ap-intc5-eb", "aon-apb", 0xb0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(scc_eb, "scc-eb", "aon-apb", 0xb0,
		     0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dphy_cfg_eb, "dphy-cfg-eb", "aon-apb", 0xb0,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dphy_ref_eb, "dphy-ref-eb", "aon-apb", 0xb0,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cphy_cfg_eb, "cphy-cfg-eb", "aon-apb", 0xb0,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_ref_eb, "otg-ref-eb", "aon-apb", 0xb0,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes_eb, "serdes-eb", "aon-apb", 0xb0,
		     0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_ap_emc_eb, "aon-ap-emc-eb", "aon-apb", 0xb0,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static struct sprd_clk_common *sc9863a_aonapb_gate_clks[] = {
	/* address base is 0x402e0000 */
	&adc_eb.common,
	&fm_eb.common,
	&tpc_eb.common,
	&gpio_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&ap_tmr0_eb.common,
	&efuse_eb.common,
	&eic_eb.common,
	&intc_eb.common,
	&adi_eb.common,
	&audif_eb.common,
	&aud_eb.common,
	&vbc_eb.common,
	&pin_eb.common,
	&ipi_eb.common,
	&splk_eb.common,
	&mspi_eb.common,
	&ap_wdg_eb.common,
	&mm_eb.common,
	&aon_apb_ckg_eb.common,
	&ca53_ts0_eb.common,
	&ca53_ts1_eb.common,
	&ca53_dap_eb.common,
	&i2c_eb.common,
	&pmu_eb.common,
	&thm_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&emc_ref_eb.common,
	&ca53_wdg_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&disp_emc_eb.common,
	&zip_emc_eb.common,
	&gsp_emc_eb.common,
	&mm_vsp_eb.common,
	&mdar_eb.common,
	&rtc4m0_cal_eb.common,
	&rtc4m1_cal_eb.common,
	&djtag_eb.common,
	&mbox_eb.common,
	&aon_dma_eb.common,
	&aon_apb_def_eb.common,
	&orp_jtag_eb.common,
	&dbg_eb.common,
	&dbg_emc_eb.common,
	&cross_trig_eb.common,
	&serdes_dphy_eb.common,
	&arch_rtc_eb.common,
	&kpd_rtc_eb.common,
	&aon_syst_rtc_eb.common,
	&ap_syst_rtc_eb.common,
	&aon_tmr_rtc_eb.common,
	&ap_tmr0_rtc_eb.common,
	&eic_rtc_eb.common,
	&eic_rtcdv5_eb.common,
	&ap_wdg_rtc_eb.common,
	&ca53_wdg_rtc_eb.common,
	&thm_rtc_eb.common,
	&athma_rtc_eb.common,
	&gthma_rtc_eb.common,
	&athma_rtc_a_eb.common,
	&gthma_rtc_a_eb.common,
	&ap_tmr1_rtc_eb.common,
	&ap_tmr2_rtc_eb.common,
	&dxco_lc_rtc_eb.common,
	&bb_cal_rtc_eb.common,
	&gpu_eb.common,
	&disp_eb.common,
	&mm_emc_eb.common,
	&power_cpu_eb.common,
	&hw_i2c_eb.common,
	&mm_vsp_emc_eb.common,
	&vsp_eb.common,
	&cssys_eb.common,
	&dmc_eb.common,
	&rosc_eb.common,
	&s_d_cfg_eb.common,
	&s_d_ref_eb.common,
	&b_dma_eb.common,
	&anlg_eb.common,
	&anlg_apb_eb.common,
	&bsmtmr_eb.common,
	&ap_axi_eb.common,
	&ap_intc0_eb.common,
	&ap_intc1_eb.common,
	&ap_intc2_eb.common,
	&ap_intc3_eb.common,
	&ap_intc4_eb.common,
	&ap_intc5_eb.common,
	&scc_eb.common,
	&dphy_cfg_eb.common,
	&dphy_ref_eb.common,
	&cphy_cfg_eb.common,
	&otg_ref_eb.common,
	&serdes_eb.common,
	&aon_ap_emc_eb.common,
};

static struct clk_hw_onecell_data sc9863a_aonapb_gate_hws = {
	.hws	= {
		[CLK_ADC_EB]		= &adc_eb.common.hw,
		[CLK_FM_EB]		= &fm_eb.common.hw,
		[CLK_TPC_EB]		= &tpc_eb.common.hw,
		[CLK_GPIO_EB]		= &gpio_eb.common.hw,
		[CLK_PWM0_EB]		= &pwm0_eb.common.hw,
		[CLK_PWM1_EB]		= &pwm1_eb.common.hw,
		[CLK_PWM2_EB]		= &pwm2_eb.common.hw,
		[CLK_PWM3_EB]		= &pwm3_eb.common.hw,
		[CLK_KPD_EB]		= &kpd_eb.common.hw,
		[CLK_AON_SYST_EB]	= &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB]	= &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB]	= &aon_tmr_eb.common.hw,
		[CLK_AP_TMR0_EB]	= &ap_tmr0_eb.common.hw,
		[CLK_EFUSE_EB]		= &efuse_eb.common.hw,
		[CLK_EIC_EB]		= &eic_eb.common.hw,
		[CLK_INTC_EB]		= &intc_eb.common.hw,
		[CLK_ADI_EB]		= &adi_eb.common.hw,
		[CLK_AUDIF_EB]		= &audif_eb.common.hw,
		[CLK_AUD_EB]		= &aud_eb.common.hw,
		[CLK_VBC_EB]		= &vbc_eb.common.hw,
		[CLK_PIN_EB]		= &pin_eb.common.hw,
		[CLK_IPI_EB]		= &ipi_eb.common.hw,
		[CLK_SPLK_EB]		= &splk_eb.common.hw,
		[CLK_MSPI_EB]		= &mspi_eb.common.hw,
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_AON_APB_CKG_EB]	= &aon_apb_ckg_eb.common.hw,
		[CLK_CA53_TS0_EB]	= &ca53_ts0_eb.common.hw,
		[CLK_CA53_TS1_EB]	= &ca53_ts1_eb.common.hw,
		[CLK_CS53_DAP_EB]	= &ca53_dap_eb.common.hw,
		[CLK_I2C_EB]		= &i2c_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_THM_EB]		= &thm_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_EMC_REF_EB]	= &emc_ref_eb.common.hw,
		[CLK_CA53_WDG_EB]	= &ca53_wdg_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_DISP_EMC_EB]	= &disp_emc_eb.common.hw,
		[CLK_ZIP_EMC_EB]	= &zip_emc_eb.common.hw,
		[CLK_GSP_EMC_EB]	= &gsp_emc_eb.common.hw,
		[CLK_MM_VSP_EB]		= &mm_vsp_eb.common.hw,
		[CLK_MDAR_EB]		= &mdar_eb.common.hw,
		[CLK_RTC4M0_CAL_EB]	= &rtc4m0_cal_eb.common.hw,
		[CLK_RTC4M1_CAL_EB]	= &rtc4m1_cal_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_AON_DMA_EB]	= &aon_dma_eb.common.hw,
		[CLK_AON_APB_DEF_EB]	= &aon_apb_def_eb.common.hw,
		[CLK_ORP_JTAG_EB]	= &orp_jtag_eb.common.hw,
		[CLK_DBG_EB]		= &dbg_eb.common.hw,
		[CLK_DBG_EMC_EB]	= &dbg_emc_eb.common.hw,
		[CLK_CROSS_TRIG_EB]	= &cross_trig_eb.common.hw,
		[CLK_SERDES_DPHY_EB]	= &serdes_dphy_eb.common.hw,
		[CLK_ARCH_RTC_EB]	= &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB]	= &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB]	= &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB]	= &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB]	= &aon_tmr_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB]	= &ap_tmr0_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB]	= &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB]	= &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB]	= &ap_wdg_rtc_eb.common.hw,
		[CLK_CA53_WDG_RTC_EB]	= &ca53_wdg_rtc_eb.common.hw,
		[CLK_THM_RTC_EB]	= &thm_rtc_eb.common.hw,
		[CLK_ATHMA_RTC_EB]	= &athma_rtc_eb.common.hw,
		[CLK_GTHMA_RTC_EB]	= &gthma_rtc_eb.common.hw,
		[CLK_ATHMA_RTC_A_EB]	= &athma_rtc_a_eb.common.hw,
		[CLK_GTHMA_RTC_A_EB]	= &gthma_rtc_a_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB]	= &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB]	= &ap_tmr2_rtc_eb.common.hw,
		[CLK_DXCO_LC_RTC_EB]	= &dxco_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB]	= &bb_cal_rtc_eb.common.hw,
		[CLK_GNU_EB]		= &gpu_eb.common.hw,
		[CLK_DISP_EB]		= &disp_eb.common.hw,
		[CLK_MM_EMC_EB]		= &mm_emc_eb.common.hw,
		[CLK_POWER_CPU_EB]	= &power_cpu_eb.common.hw,
		[CLK_HW_I2C_EB]		= &hw_i2c_eb.common.hw,
		[CLK_MM_VSP_EMC_EB]	= &mm_vsp_emc_eb.common.hw,
		[CLK_VSP_EB]		= &vsp_eb.common.hw,
		[CLK_CSSYS_EB]		= &cssys_eb.common.hw,
		[CLK_DMC_EB]		= &dmc_eb.common.hw,
		[CLK_ROSC_EB]		= &rosc_eb.common.hw,
		[CLK_S_D_CFG_EB]	= &s_d_cfg_eb.common.hw,
		[CLK_S_D_REF_EB]	= &s_d_ref_eb.common.hw,
		[CLK_B_DMA_EB]		= &b_dma_eb.common.hw,
		[CLK_ANLG_EB]		= &anlg_eb.common.hw,
		[CLK_ANLG_APB_EB]	= &anlg_apb_eb.common.hw,
		[CLK_BSMTMR_EB]		= &bsmtmr_eb.common.hw,
		[CLK_AP_AXI_EB]		= &ap_axi_eb.common.hw,
		[CLK_AP_INTC0_EB]	= &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB]	= &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB]	= &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB]	= &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB]	= &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB]	= &ap_intc5_eb.common.hw,
		[CLK_SCC_EB]		= &scc_eb.common.hw,
		[CLK_DPHY_CFG_EB]	= &dphy_cfg_eb.common.hw,
		[CLK_DPHY_REF_EB]	= &dphy_ref_eb.common.hw,
		[CLK_CPHY_CFG_EB]	= &cphy_cfg_eb.common.hw,
		[CLK_OTG_REF_EB]	= &otg_ref_eb.common.hw,
		[CLK_SERDES_EB]		= &serdes_eb.common.hw,
		[CLK_AON_AP_EMC_EB]	= &aon_ap_emc_eb.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_aonapb_gate_desc = {
	.clk_clks	= sc9863a_aonapb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_aonapb_gate_clks),
	.hw_clks	= &sc9863a_aonapb_gate_hws,
};

/* mm gate clocks */
static SPRD_SC_GATE_CLK(mahb_ckg_eb, "mahb-ckg-eb", "mm-ahb", 0x0, 0x1000,
			BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mdcam_eb, "mdcam-eb", "mm-ahb", 0x0, 0x1000,
			BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(misp_eb, "misp-eb", "mm-ahb", 0x0, 0x1000,
			BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mahbcsi_eb, "mahbcsi-eb", "mm-ahb", 0x0, 0x1000,
			BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mcsi_s_eb, "mcsi-s-eb", "mm-ahb", 0x0, 0x1000,
			BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mcsi_t_eb, "mcsi-t-eb", "mm-ahb", 0x0, 0x1000,
			BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(dcam_axi_eb, "dcam-axi-eb", "mm-ahb", 0x8,
		     BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(isp_axi_eb, "isp-axi-eb", "mm-ahb", 0x8,
		     BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mcsi_eb,	"mcsi-eb", "mm-ahb", 0x8,
		     BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mcsi_s_ckg_eb, "mcsi-s-ckg-eb", "mm-ahb", 0x8,
		     BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mcsi_t_ckg_eb, "mcsi-t-ckg-eb", "mm-ahb", 0x8,
		     BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(sensor0_eb, "sensor0-eb", "mm-ahb", 0x8,
		     BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(sensor1_eb, "sensor1-eb", "mm-ahb", 0x8,
		     BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(sensor2_eb, "sensor2-eb", "mm-ahb", 0x8,
		     BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mcphy_cfg_eb, "mcphy-cfg-eb", "mm-ahb", 0x8,
		     BIT(8), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9863a_mm_gate_clks[] = {
	/* address base is 0x60800000 */
	&mahb_ckg_eb.common,
	&mdcam_eb.common,
	&misp_eb.common,
	&mahbcsi_eb.common,
	&mcsi_s_eb.common,
	&mcsi_t_eb.common,
	&dcam_axi_eb.common,
	&isp_axi_eb.common,
	&mcsi_eb.common,
	&mcsi_s_ckg_eb.common,
	&mcsi_t_ckg_eb.common,
	&sensor0_eb.common,
	&sensor1_eb.common,
	&sensor2_eb.common,
	&mcphy_cfg_eb.common,
};

static struct clk_hw_onecell_data sc9863a_mm_gate_hws = {
	.hws	= {
		[CLK_MAHB_CKG_EB]	= &mahb_ckg_eb.common.hw,
		[CLK_MDCAM_EB]		= &mdcam_eb.common.hw,
		[CLK_MISP_EB]		= &misp_eb.common.hw,
		[CLK_MAHBCSI_EB]	= &mahbcsi_eb.common.hw,
		[CLK_MCSI_S_EB]		= &mcsi_s_eb.common.hw,
		[CLK_MCSI_T_EB]		= &mcsi_t_eb.common.hw,
		[CLK_DCAM_AXI_EB]	= &dcam_axi_eb.common.hw,
		[CLK_ISP_AXI_EB]	= &isp_axi_eb.common.hw,
		[CLK_MCSI_EB]		= &mcsi_eb.common.hw,
		[CLK_MCSI_S_CKG_EB]	= &mcsi_s_ckg_eb.common.hw,
		[CLK_MCSI_T_CKG_EB]	= &mcsi_t_ckg_eb.common.hw,
		[CLK_SENSOR0_EB]	= &sensor0_eb.common.hw,
		[CLK_SENSOR1_EB]	= &sensor1_eb.common.hw,
		[CLK_SENSOR2_EB]	= &sensor2_eb.common.hw,
		[CLK_MCPHY_CFG_EB]	= &mcphy_cfg_eb.common.hw,
	},
	.num	= CLK_MM_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_mm_gate_desc = {
	.clk_clks	= sc9863a_mm_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_mm_gate_clks),
	.hw_clks	= &sc9863a_mm_gate_hws,
};

/* mm clocks */
static SPRD_GATE_CLK(mipi_csi_clk, "mipi-csi-clk", "mm-ahb", 0x20,
		     BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mipi_csi_s_clk, "mipi-csi-s-clk", "mm-ahb", 0x24,
		     BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mipi_csi_m_clk, "mipi-csi-m-clk", "mm-ahb", 0x28,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9863a_mm_clk_clks[] = {
	/* address base is 0x60900000 */
	&mipi_csi_clk.common,
	&mipi_csi_s_clk.common,
	&mipi_csi_m_clk.common,
};

static struct clk_hw_onecell_data sc9863a_mm_clk_hws = {
	.hws	= {
		[CLK_MIPI_CSI]		= &mipi_csi_clk.common.hw,
		[CLK_MIPI_CSI_S]	= &mipi_csi_s_clk.common.hw,
		[CLK_MIPI_CSI_M]	= &mipi_csi_m_clk.common.hw,
	},
	.num	= CLK_MM_CLK_NUM,
};

static const struct sprd_clk_desc sc9863a_mm_clk_desc = {
	.clk_clks	= sc9863a_mm_clk_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_mm_clk_clks),
	.hw_clks	= &sc9863a_mm_clk_hws,
};

/* vsp gate clocks */
static SPRD_SC_GATE_CLK(vckg_eb, "vckg-eb", "mm-ahb", 0x0, 0x1000,
			BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vvsp_eb, "vvsp-eb", "mm-ahb", 0x0, 0x1000,
			BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vjpg_eb, "vjpg-eb", "mm-ahb", 0x0, 0x1000,
			BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vcpp_eb, "vcpp-eb", "mm-ahb", 0x0, 0x1000,
			BIT(3), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9863a_vspahb_gate_clks[] = {
	/* address base is 0x62000000 */
	&vckg_eb.common,
	&vvsp_eb.common,
	&vjpg_eb.common,
	&vcpp_eb.common,
};

static struct clk_hw_onecell_data sc9863a_vspahb_gate_hws = {
	.hws	= {
		[CLK_VCKG_EB]		= &vckg_eb.common.hw,
		[CLK_VVSP_EB]		= &vvsp_eb.common.hw,
		[CLK_VJPG_EB]		= &vjpg_eb.common.hw,
		[CLK_VCPP_EB]		= &vcpp_eb.common.hw,
	},
	.num	= CLK_VSP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_vspahb_gate_desc = {
	.clk_clks	= sc9863a_vspahb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_vspahb_gate_clks),
	.hw_clks	= &sc9863a_vspahb_gate_hws,
};

static SPRD_SC_GATE_CLK(sim0_eb,	"sim0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis0_eb,	"iis0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis1_eb,	"iis1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis2_eb,	"iis2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_eb,	"spi0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_eb,	"spi1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_eb,	"spi2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c0_eb,	"i2c0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c1_eb,	"i2c1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c2_eb,	"i2c2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c3_eb,	"i2c3-eb",	"ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c4_eb,	"i2c4-eb",	"ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart0_eb,	"uart0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart1_eb,	"uart1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart2_eb,	"uart2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart3_eb,	"uart3-eb",	"ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart4_eb,	"uart4-eb",	"ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_32k_eb,	"sim0_32k-eb",	"ext-26m", 0x0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi3_eb,	"spi3-eb",	"ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c5_eb,	"i2c5-eb",	"ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c6_eb,	"i2c6-eb",	"ext-26m", 0x0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9863a_apapb_gate[] = {
	/* address base is 0x71300000 */
	&sim0_eb.common,
	&iis0_eb.common,
	&iis1_eb.common,
	&iis2_eb.common,
	&spi0_eb.common,
	&spi1_eb.common,
	&spi2_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&i2c3_eb.common,
	&i2c4_eb.common,
	&uart0_eb.common,
	&uart1_eb.common,
	&uart2_eb.common,
	&uart3_eb.common,
	&uart4_eb.common,
	&sim0_32k_eb.common,
	&spi3_eb.common,
	&i2c5_eb.common,
	&i2c6_eb.common,
};

static struct clk_hw_onecell_data sc9863a_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_IIS1_EB]		= &iis1_eb.common.hw,
		[CLK_IIS2_EB]		= &iis2_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_SPI1_EB]		= &spi1_eb.common.hw,
		[CLK_SPI2_EB]		= &spi2_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_I2C3_EB]		= &i2c3_eb.common.hw,
		[CLK_I2C4_EB]		= &i2c4_eb.common.hw,
		[CLK_UART0_EB]		= &uart0_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_UART2_EB]		= &uart2_eb.common.hw,
		[CLK_UART3_EB]		= &uart3_eb.common.hw,
		[CLK_UART4_EB]		= &uart4_eb.common.hw,
		[CLK_SIM0_32K_EB]	= &sim0_32k_eb.common.hw,
		[CLK_SPI3_EB]		= &spi3_eb.common.hw,
		[CLK_I2C5_EB]		= &i2c5_eb.common.hw,
		[CLK_I2C6_EB]		= &i2c6_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_apapb_gate_desc = {
	.clk_clks	= sc9863a_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_apapb_gate),
	.hw_clks	= &sc9863a_apapb_gate_hws,
};

static const struct of_device_id sprd_sc9863a_clk_ids[] = {
	{ .compatible = "sprd,sc9863a-ap-clk",	/* 0x21500000 */
	  .data = &sc9863a_ap_clk_desc },
	{ .compatible = "sprd,sc9863a-pmu-gate",	/* 0x402b0000 */
	  .data = &sc9863a_pmu_gate_desc },
	{ .compatible = "sprd,sc9863a-pll",	/* 0x40353000 */
	  .data = &sc9863a_pll_desc },
	{ .compatible = "sprd,sc9863a-mpll",	/* 0x40359000 */
	  .data = &sc9863a_mpll_desc },
	{ .compatible = "sprd,sc9863a-rpll",	/* 0x4035c000 */
	  .data = &sc9863a_rpll_desc },
	{ .compatible = "sprd,sc9863a-dpll",	/* 0x40363000 */
	  .data = &sc9863a_dpll_desc },
	{ .compatible = "sprd,sc9863a-aon-clk",	/* 0x402d0000 */
	  .data = &sc9863a_aon_clk_desc },
	{ .compatible = "sprd,sc9863a-apahb-gate",	/* 0x20e00000 */
	  .data = &sc9863a_apahb_gate_desc },
	{ .compatible = "sprd,sc9863a-aonapb-gate",	/* 0x402e0000 */
	  .data = &sc9863a_aonapb_gate_desc },
	{ .compatible = "sprd,sc9863a-mm-gate",	/* 0x60800000 */
	  .data = &sc9863a_mm_gate_desc },
	{ .compatible = "sprd,sc9863a-mm-clk",	/* 0x60900000 */
	  .data = &sc9863a_mm_clk_desc },
	{ .compatible = "sprd,sc9863a-vspahb-gate",	/* 0x62000000 */
	  .data = &sc9863a_vspahb_gate_desc },
	{ .compatible = "sprd,sc9863a-apapb-gate",	/* 0x71300000 */
	  .data = &sc9863a_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_sc9863a_clk_ids);

static int sc9863a_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;
	int ret;

	match = of_match_node(sprd_sc9863a_clk_ids, pdev->dev.of_node);
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

static struct platform_driver sc9863a_clk_driver = {
	.probe	= sc9863a_clk_probe,
	.driver	= {
		.name	= "sc9863a-clk",
		.of_match_table	= sprd_sc9863a_clk_ids,
	},
};
module_platform_driver(sc9863a_clk_driver);

MODULE_DESCRIPTION("Spreadtrum SC9863A Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sc9863a-clk");
