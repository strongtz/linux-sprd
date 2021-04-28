// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum SC9832e clock driver
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

#include <dt-bindings/clock/sprd,sc9832e-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

static CLK_FIXED_FACTOR(fac_13m, "fac-13m", "ext-26m", 2, 1, 0);
static CLK_FIXED_FACTOR(fac_6m5, "fac-6m5", "ext-26m", 4, 1, 0);
static CLK_FIXED_FACTOR(fac_4m3, "fac-4m3", "ext-26m", 6, 1, 0);
static CLK_FIXED_FACTOR(fac_2m, "fac-2m", "ext-26m", 13, 1, 0);
static CLK_FIXED_FACTOR(fac_1m, "fac-1m", "ext-26m", 26, 1, 0);
static CLK_FIXED_FACTOR(fac_250k, "fac-250k", "ext-26m", 104, 1, 0);
static SPRD_PLL_SC_GATE_CLK(isppll_gate, "isppll-gate", "ext-26m", 0x88,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll_gate, "mpll-gate", "ext-26m", 0x94,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(dpll_gate, "dpll-gate", "ext-26m", 0x98,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(lpll_gate, "lpll-gate", "ext-26m", 0x9c,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(gpll_gate, "gpll-gate", "ext-26m", 0xa8,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *sc9832e_pmu_gate_clks[] = {
	/* address base is 0x402b0000 */
	&isppll_gate.common,
	&mpll_gate.common,
	&dpll_gate.common,
	&lpll_gate.common,
	&gpll_gate.common,
};

static struct clk_hw_onecell_data sc9832e_pmu_gate_hws = {
	.hws	= {
		[CLK_FAC_13M]		= &fac_13m.hw,
		[CLK_FAC_6M5]		= &fac_6m5.hw,
		[CLK_FAC_4M3]		= &fac_4m3.hw,
		[CLK_FAC_2M]		= &fac_2m.hw,
		[CLK_FAC_1M]		= &fac_1m.hw,
		[CLK_FAC_250K]		= &fac_250k.hw,
		[CLK_ISPPLL_GATE]	= &isppll_gate.common.hw,
		[CLK_MPLL_GATE]		= &mpll_gate.common.hw,
		[CLK_DPLL_GATE]		= &dpll_gate.common.hw,
		[CLK_LPLL_GATE]		= &lpll_gate.common.hw,
		[CLK_GPLL_GATE]		= &gpll_gate.common.hw,
	},
	.num	= CLK_PMU_GATE_NUM,
};

static const struct sprd_clk_desc sc9832e_pmu_gate_desc = {
	.clk_clks	= sc9832e_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_pmu_gate_clks),
	.hw_clks        = &sc9832e_pmu_gate_hws,
};

static const struct freq_table ftable[4] = {
	{ .ibias = 0, .max_freq = 936000000ULL },
	{ .ibias = 1, .max_freq = 1248000000ULL },
	{ .ibias = 2, .max_freq = 1600000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static const struct clk_bit_field f_twpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 6,	.width = 2 },	/* ibias	*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 6 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(twpll_clk, "twpll", "ext-26m", 0xc,
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

#define f_lpll f_twpll
static SPRD_PLL_WITH_ITABLE_1K(lpll_clk, "lpll", "lpll-gate", 0x1c,
				   2, ftable, f_lpll, 240);
static CLK_FIXED_FACTOR(lpll_409m6, "lpll-409m6", "lpll", 3, 1, 0);
static CLK_FIXED_FACTOR(lpll_245m76, "lpll-245m76", "lpll", 5, 1, 0);

static const struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 1,	.width = 1 },	/* div_s	*/
	{ .shift = 2,	.width = 1 },	/* mod_en	*/
	{ .shift = 3,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 7,	.width = 2 },	/* ibias	*/
	{ .shift = 9,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 6 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 64,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(gpll_clk, "gpll", "gpll-gate", 0x2c,
				   3, ftable, f_gpll, 240,
				   1000, 1000, 1, 400000000);

static const struct clk_bit_field f_isppll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 1,	.width = 1 },	/* div_s	*/
	{ .shift = 2,	.width = 1 },	/* mod_en	*/
	{ .shift = 3,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 7,	.width = 2 },	/* ibias	*/
	{ .shift = 9,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 6 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(isppll_clk, "isppll", "isppll-gate", 0x3c,
				   2, ftable, f_isppll, 240);
static CLK_FIXED_FACTOR(isppll_468m, "isppll-468m", "isppll", 2, 1, 0);

static struct sprd_clk_common *sc9832e_pll_clks[] = {
	/* address base is 0x403c0000 */
	&twpll_clk.common,
	&lpll_clk.common,
	&gpll_clk.common,
	&isppll_clk.common,
};

static struct clk_hw_onecell_data sc9832e_pll_hws = {
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
	.num	= CLK_PLL_NUM,
};

static const struct sprd_clk_desc sc9832e_pll_desc = {
	.clk_clks	= sc9832e_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_pll_clks),
	.hw_clks        = &sc9832e_pll_hws,
};

static const struct clk_bit_field f_dpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 9,	.width = 1 },	/* div_s	*/
	{ .shift = 10,	.width = 1 },	/* mod_en	*/
	{ .shift = 11,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 15,	.width = 2 },	/* ibias	*/
	{ .shift = 17,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 6 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(dpll_clk, "dpll", "dpll-gate", 0x0,
				   2, ftable, f_dpll, 240);

static struct sprd_clk_common *sc9832e_dpll_clks[] = {
	/* address base is 0x403d0000 */
	&dpll_clk.common,

};

static struct clk_hw_onecell_data sc9832e_dpll_hws = {
	.hws	= {
		[CLK_DPLL]	= &dpll_clk.common.hw,

	},
	.num	= CLK_DPLL_NUM,
};

static const struct sprd_clk_desc sc9832e_dpll_desc = {
	.clk_clks	= sc9832e_dpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_dpll_clks),
	.hw_clks        = &sc9832e_dpll_hws,
};

#define f_mpll f_twpll
static SPRD_PLL_WITH_ITABLE_1K(mpll_clk, "mpll", "mpll-gate", 0x0,
				2, ftable, f_mpll, 240);

static struct sprd_clk_common *sc9832e_mpll_clks[] = {
	/* address base is 0x403f0000 */
	&mpll_clk.common,

};

static struct clk_hw_onecell_data sc9832e_mpll_hws = {
	.hws	= {
		[CLK_MPLL]	= &mpll_clk.common.hw,

	},
	.num	= CLK_MPLL_NUM,
};

static const struct sprd_clk_desc sc9832e_mpll_desc = {
	.clk_clks	= sc9832e_mpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_mpll_clks),
	.hw_clks        = &sc9832e_mpll_hws,
};

static SPRD_SC_GATE_CLK(audio_gate,	"audio-gate",	"ext-26m", 0x8,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

static const struct clk_bit_field f_rpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 3,	.width = 1 },	/* div_s	*/
	{ .shift = 4,	.width = 1 },	/* mod_en	*/
	{ .shift = 5,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 9,	.width = 2 },	/* ibias	*/
	{ .shift = 11,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 6 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(rpll_clk, "rpll", "ext-26m", 0x14,
				   2, ftable, f_rpll, 240);

static SPRD_SC_GATE_CLK(rpll_d1_en,	"rpll-d1-en",	"rpll", 0x1c,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

static CLK_FIXED_FACTOR(rpll_390m, "rpll-390m", "rpll", 2, 1, 0);
static CLK_FIXED_FACTOR(rpll_260m, "rpll-260m", "rpll", 3, 1, 0);
static CLK_FIXED_FACTOR(rpll_195m, "rpll-195m", "rpll", 4, 1, 0);
static CLK_FIXED_FACTOR(rpll_26m, "rpll-26m", "rpll", 30, 1, 0);

static struct sprd_clk_common *sc9832e_rpll_clks[] = {
	/* address base is 0x40410000 */
	&audio_gate.common,
	&rpll_clk.common,
	&rpll_d1_en.common,
};

static struct clk_hw_onecell_data sc9832e_rpll_hws = {
	.hws	= {
		[CLK_AUDIO_GATE]	= &audio_gate.common.hw,
		[CLK_RPLL]		= &rpll_clk.common.hw,
		[CLK_RPLL_D1_EN]	= &rpll_d1_en.common.hw,
		[CLK_RPLL_390M]		= &rpll_390m.hw,
		[CLK_RPLL_260M]		= &rpll_260m.hw,
		[CLK_RPLL_195M]		= &rpll_195m.hw,
		[CLK_RPLL_26M]		= &rpll_26m.hw,
	},
	.num	= CLK_RPLL_NUM,
};

static const struct sprd_clk_desc sc9832e_rpll_desc = {
	.clk_clks	= sc9832e_rpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_rpll_clks),
	.hw_clks        = &sc9832e_rpll_hws,
};

#define SC9832E_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* 0x21500000 ap clocks */
static const char * const ap_apb_parents[] = { "ext-26m", "twpll-64m",
					       "twpll-96m", "twpll-128m" };
static SPRD_MUX_CLK(ap_apb, "ap-apb", ap_apb_parents, 0x20,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const nandc_ecc_parents[] = { "ext-26m", "twpll-256m",
						  "twpll-307m2"};
static SPRD_COMP_CLK(nandc_ecc,	"nandc-ecc", nandc_ecc_parents, 0x24,
		     0, 2, 8, 3, 0);

static const char * const otg_ref_parents[] = { "twpll-12m", "twpll-24m" };
static SPRD_MUX_CLK(otg_ref, "otg-ref", otg_ref_parents, 0x28,
		    0, 1, SC9832E_MUX_FLAG);

static SPRD_GATE_CLK(otg_utmi, "otg-utmi", "ap-apb", 0x2c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const uart_parents[] = { "ext-26m", "twpll-48m",
					     "twpll-51m2", "twpll-96m" };
static SPRD_COMP_CLK(uart1_clk,	"uart1", uart_parents, 0x30,
		     0, 2, 8, 3, 0);

static const char * const i2c_parents[] = { "ext-26m", "twpll-48m",
					    "twpll-51m2", "twpll-153m6" };
static SPRD_COMP_CLK(i2c0_clk,	"i2c0", i2c_parents, 0x34,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(i2c1_clk,	"i2c1", i2c_parents, 0x38,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(i2c2_clk,	"i2c2", i2c_parents, 0x3c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(i2c3_clk,	"i2c3", i2c_parents, 0x40,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(i2c4_clk,	"i2c4", i2c_parents, 0x44,
		     0, 2, 8, 3, 0);

static const char * const spi_parents[] = { "ext-26m", "twpll-128m",
					    "twpll-153m6", "twpll-192m" };
static SPRD_COMP_CLK(spi0_clk,	"spi0",	spi_parents, 0x48,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(spi2_clk,	"spi2",	spi_parents, 0x4c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(hs_spi_clk, "hs_spi", spi_parents, 0x50,
		     0, 2, 8, 3, 0);

static const char * const iis_parents[] = { "ext-26m", "twpll-128m",
					    "twpll-153m6" };
static SPRD_COMP_CLK(iis0_clk,	"iis0",	iis_parents, 0x54,
		     0, 2, 8, 3, 0);

static const char * const ce_parents[] = { "ext-26m", "twpll-153m6",
					   "twpll-170m6", "rpll-195m",
					   "twpll-219m4", "lpll-245m76",
					   "rpll-260m", "twpll-307m2",
					   "rpll-390m" };
static SPRD_MUX_CLK(ce_clk, "ce", ce_parents, 0x58,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const nandc_2x_parents[] = { "ext-26m", "twpll-48m",
						 "twpll-51m2", "twpll-153m6" };
static SPRD_COMP_CLK(nandc_2x_clk, "nandc-2x", nandc_2x_parents, 0x78,
		     0, 4, 8, 4, 0);

static const char * const sdio_parents[] = { "fac-1m", "ext-26m",
					     "twpll-307m2", "twpll-384m",
					     "rpll-390m", "lpll-409m6" };
static SPRD_MUX_CLK(sdio0_2x_clk, "sdio0-2x", sdio_parents, 0x80,
		    0, 3, SC9832E_MUX_FLAG);
static SPRD_MUX_CLK(sdio1_2x_clk, "sdio1-2x", sdio_parents, 0x88,
		    0, 3, SC9832E_MUX_FLAG);
static SPRD_MUX_CLK(emmc_2x_clk, "emmc-2x", sdio_parents, 0x90,
		    0, 3, SC9832E_MUX_FLAG);

static const char * const vsp_parents[] = { "twpll-76m8", "twpll-128m",
					    "twpll-256m", "twpll-307m2" };
static SPRD_MUX_CLK(vsp_clk, "vsp", vsp_parents, 0x98,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const gsp_parents[] = { "twpll-153m6", "twpll-192m",
					    "twpll-256m", "twpll-384m" };
static SPRD_MUX_CLK(gsp_clk, "gsp", gsp_parents, 0x9c,
		    0, 2, SC9832E_MUX_FLAG);
static SPRD_MUX_CLK(dispc0_clk, "dispc0", gsp_parents, 0xa0,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const dispc_parents[] = { "twpll-96m", "twpll-128m",
					      "twpll-153m6" };
static SPRD_COMP_CLK(dispc0_dpi, "dispc0-dpi", dispc_parents,	0xa4,
		     0, 2, 8, 4, 0);

static SPRD_GATE_CLK(dsi_rxesc, "dsi-rxesc", "ap-apb", 0xa8,
		     BIT(16), 0, 0);

static SPRD_GATE_CLK(dsi_lanebyte, "dsi-lanebyte", "ap-apb", 0xac,
		     BIT(16), 0, 0);

static struct sprd_clk_common *sc9832e_ap_clks[] = {
	/* address base is 0x21500000 */
	&ap_apb.common,
	&nandc_ecc.common,
	&otg_ref.common,
	&otg_utmi.common,
	&uart1_clk.common,
	&i2c0_clk.common,
	&i2c1_clk.common,
	&i2c2_clk.common,
	&i2c3_clk.common,
	&i2c4_clk.common,
	&spi0_clk.common,
	&spi2_clk.common,
	&hs_spi_clk.common,
	&iis0_clk.common,
	&ce_clk.common,
	&nandc_2x_clk.common,
	&sdio0_2x_clk.common,
	&sdio1_2x_clk.common,
	&emmc_2x_clk.common,
	&vsp_clk.common,
	&gsp_clk.common,
	&dispc0_clk.common,
	&dispc0_dpi.common,
	&dsi_rxesc.common,
	&dsi_lanebyte.common,
};

static struct clk_hw_onecell_data sc9832e_ap_clk_hws = {
	.hws	= {
		[CLK_AP_APB]		= &ap_apb.common.hw,
		[CLK_NANDC_ECC]		= &nandc_ecc.common.hw,
		[CLK_OTG_REF]		= &otg_ref.common.hw,
		[CLK_OTG_UTMI]		= &otg_utmi.common.hw,
		[CLK_UART1]		= &uart1_clk.common.hw,
		[CLK_I2C0]		= &i2c0_clk.common.hw,
		[CLK_I2C1]		= &i2c1_clk.common.hw,
		[CLK_I2C2]		= &i2c2_clk.common.hw,
		[CLK_I2C3]		= &i2c3_clk.common.hw,
		[CLK_I2C4]		= &i2c4_clk.common.hw,
		[CLK_SPI0]		= &spi0_clk.common.hw,
		[CLK_SPI2]		= &spi2_clk.common.hw,
		[CLK_HS_SPI]		= &hs_spi_clk.common.hw,
		[CLK_IIS0]		= &iis0_clk.common.hw,
		[CLK_CE]		= &ce_clk.common.hw,
		[CLK_NANDC_2X]		= &nandc_2x_clk.common.hw,
		[CLK_SDIO0_2X]		= &sdio0_2x_clk.common.hw,
		[CLK_SDIO1_2X]		= &sdio1_2x_clk.common.hw,
		[CLK_EMMC_2X]		= &emmc_2x_clk.common.hw,
		[CLK_VSP]		= &vsp_clk.common.hw,
		[CLK_GSP]		= &gsp_clk.common.hw,
		[CLK_DISPC0]		= &dispc0_clk.common.hw,
		[CLK_DISPC0_DPI]	= &dispc0_dpi.common.hw,
		[CLK_DSI_RXESC]		= &dsi_rxesc.common.hw,
		[CLK_DSI_LANEBYTE]	= &dsi_lanebyte.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static const struct sprd_clk_desc sc9832e_ap_clk_desc = {
	.clk_clks	= sc9832e_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_ap_clks),
	.hw_clks	= &sc9832e_ap_clk_hws,
};

/* 0x402d0000 aon clocks */
static const char * const aon_apb_parents[] = { "ext-26m", "twpll-76m8",
						"twpll-96m", "twpll-128m" };
static SPRD_COMP_CLK(aon_apb, "aon-apb", aon_apb_parents, 0x220,
		     0, 2, 8, 2, 0);

static const char * const adi_parents[] = { "ext-26m", "twpll-38m4",
					    "twpll-51m2" };
static SPRD_MUX_CLK(adi_clk, "adi", adi_parents, 0x224,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const pwm_parents[] = { "ext-32k", "ext-26m",
					    "rpll-26m", "twpll-48m" };
static SPRD_MUX_CLK(pwm0_clk, "pwm0", pwm_parents, 0x238,
		    0, 2, SC9832E_MUX_FLAG);
static SPRD_MUX_CLK(pwm1_clk, "pwm1", pwm_parents, 0x23c,
		    0, 2, SC9832E_MUX_FLAG);
static SPRD_MUX_CLK(pwm2_clk, "pwm2", pwm_parents, 0x240,
		    0, 2, SC9832E_MUX_FLAG);
static SPRD_MUX_CLK(pwm3_clk, "pwm3", pwm_parents, 0x244,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const thm_parents[] = { "ext-32k", "fac-250k" };
static SPRD_MUX_CLK(thm0_clk, "thm0", thm_parents, 0x258,
		    0, 1, SC9832E_MUX_FLAG);
static SPRD_MUX_CLK(thm1_clk, "thm1", thm_parents, 0x25c,
		    0, 1, SC9832E_MUX_FLAG);

static const char * const audif_parents[] = { "ext-26m-aud", "twpll-38m4",
					      "twpll-51m2" };
static SPRD_MUX_CLK(audif_clk, "audif", audif_parents, 0x264,
		    0, 2, SC9832E_MUX_FLAG);


static SPRD_GATE_CLK(aud_iis_da0, "aud-iis-da0", "ap-apb", 0x26c,
		     BIT(16), 0, 0);
static SPRD_GATE_CLK(aud_iis_ad0, "aud-iis-ad0", "ap-apb", 0x270,
		     BIT(16), 0, 0);

static const char * const ca53_dap_parents[] = { "ext-26m", "twpll-76m8",
						 "twpll-128m",	"twpll-153m6" };
static SPRD_MUX_CLK(ca53_dap, "ca53-dap", ca53_dap_parents, 0x274,
		    0, 2, SC9832E_MUX_FLAG);

static SPRD_GATE_CLK(ca53_dmtck, "ca53-dmtck", "ap-apb", 0x278,
		     BIT(16), 0, 0);

static const char * const ca53_ts_parents[] = {	"ext-32k", "ext-26m",
						"clk-twpll-128m",
						"clk-twpll-153m6" };
static SPRD_MUX_CLK(ca53_ts, "ca53-ts", ca53_ts_parents, 0x27c,
		    0, 2, SC9832E_MUX_FLAG);

static SPRD_GATE_CLK(djtag_tck, "djtag-tck", "ap-apb", 0x280,
		     BIT(16), 0, 0);

static const char * const emc_ref_parents[] = {	"fac-6m5", "fac-13m",
						"ext-26m" };
static SPRD_MUX_CLK(emc_ref, "emc-ref", emc_ref_parents, 0x28c,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const cssys_parents[] = { "ext-26m", "twpll-96m",
					      "twpll-128m", "twpll-153m6",
					      "twpll-256m" };
static SPRD_COMP_CLK(cssys_clk, "cssys", cssys_parents, 0x290,
		     0, 3, 8, 2, 0);

static const char * const tmr_parents[] = { "ext-32k", "fac-4m3" };
static SPRD_MUX_CLK(tmr_clk, "tmr", tmr_parents, 0x298,
		    0, 1, SC9832E_MUX_FLAG);

static SPRD_GATE_CLK(dsi_test, "dsi-test", "ap-apb", 0x2a0,
		     BIT(16), 0, 0);

static const char * const sdphy_apb_parents[] = { "ext-26m", "twpll-48m" };
static SPRD_MUX_CLK(sdphy_apb, "sdphy-apb", sdphy_apb_parents, 0x2b8,
		    0, 1, SC9832E_MUX_FLAG);
static SPRD_COMP_CLK(aio_apb, "aio-apb", sdphy_apb_parents, 0x2c4,
		     0, 1, 8, 2, 0);

static SPRD_GATE_CLK(dtck_hw, "dtck-hw", "ap-apb", 0x2c8,
		     BIT(16), 0, 0);

static const char * const ap_mm_parents[] = { "ext-26m", "twpll-96m",
					      "twpll-128m" };
static SPRD_COMP_CLK(ap_mm, "ap-mm", ap_mm_parents, 0x2cc,
		     0, 2, 8, 2, 0);

static const char * const ap_axi_parents[] = { "ext-26m", "twpll-76m8",
					       "twpll-128m", "twpll-256m" };
static SPRD_MUX_CLK(ap_axi, "ap-axi", ap_axi_parents, 0x2d0,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const nic_gpu_parents[] = { "twpll-256m", "twpll-307m2",
						"twpll-384m", "twpll-512m",
						"gpll"};
static SPRD_COMP_CLK(nic_gpu, "nic-gpu", nic_gpu_parents, 0x2d8,
		     0, 3, 8, 3, 0);

static const char * const mm_isp_parents[] = { "twpll-128m", "twpll-256m",
					       "twpll-307m2", "twpll-384m",
					       "isppll-468m" };
static SPRD_MUX_CLK(mm_isp, "mm-isp", mm_isp_parents, 0x2dc,
		    0, 3, SC9832E_MUX_FLAG);

static struct sprd_clk_common *sc9832e_aon_prediv[] = {
	/* address base is 0x402d0000 */
	&aon_apb.common,
	&adi_clk.common,
	&pwm0_clk.common,
	&pwm1_clk.common,
	&pwm2_clk.common,
	&pwm3_clk.common,
	&thm0_clk.common,
	&thm1_clk.common,
	&audif_clk.common,
	&aud_iis_da0.common,
	&aud_iis_ad0.common,
	&ca53_dap.common,
	&ca53_dmtck.common,
	&ca53_ts.common,
	&djtag_tck.common,
	&emc_ref.common,
	&cssys_clk.common,
	&tmr_clk.common,
	&dsi_test.common,
	&sdphy_apb.common,
	&aio_apb.common,
	&dtck_hw.common,
	&ap_mm.common,
	&ap_axi.common,
	&nic_gpu.common,
	&mm_isp.common,
};

static struct clk_hw_onecell_data sc9832e_aon_prediv_hws = {
	.hws	= {
		[CLK_AON_APB]		= &aon_apb.common.hw,
		[CLK_ADI]		= &adi_clk.common.hw,
		[CLK_PWM0]		= &pwm0_clk.common.hw,
		[CLK_PWM1]		= &pwm1_clk.common.hw,
		[CLK_PWM2]		= &pwm2_clk.common.hw,
		[CLK_PWM3]		= &pwm3_clk.common.hw,
		[CLK_THM0]		= &thm0_clk.common.hw,
		[CLK_THM1]		= &thm1_clk.common.hw,
		[CLK_AUDIF]		= &audif_clk.common.hw,
		[CLK_AUD_IIS_DA0]	= &aud_iis_da0.common.hw,
		[CLK_AUD_IIS_AD0]	= &aud_iis_ad0.common.hw,
		[CLK_CA53_DAP]		= &ca53_dap.common.hw,
		[CLK_CA53_DMTCK]	= &ca53_dmtck.common.hw,
		[CLK_CA53_TS]		= &ca53_ts.common.hw,
		[CLK_DJTAG_TCK]		= &djtag_tck.common.hw,
		[CLK_EMC_REF]		= &emc_ref.common.hw,
		[CLK_CSSYS]		= &cssys_clk.common.hw,
		[CLK_TMR]		= &tmr_clk.common.hw,
		[CLK_DSI_TEST]		= &dsi_test.common.hw,
		[CLK_SDPHY_APB]		= &sdphy_apb.common.hw,
		[CLK_AIO_APB]		= &aio_apb.common.hw,
		[CLK_DTCK_HW]		= &dtck_hw.common.hw,
		[CLK_AP_MM]		= &ap_mm.common.hw,
		[CLK_AP_AXI]		= &ap_axi.common.hw,
		[CLK_NIC_GPU]		= &nic_gpu.common.hw,
		[CLK_MM_ISP]		= &mm_isp.common.hw,
	},
	.num	= CLK_AON_PREDIV_NUM,
};

static const struct sprd_clk_desc sc9832e_aon_prediv_desc = {
	.clk_clks	= sc9832e_aon_prediv,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_aon_prediv),
	.hw_clks	= &sc9832e_aon_prediv_hws,
};

/* 0x402e0000 aon_apb gate clocks */
static SPRD_SC_GATE_CLK(adc_eb, "adc-eb", "aon-apb", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(fm_eb, "fm-eb", "aon-apb", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tpc_eb, "tpc-eb", "aon-apb", 0x0,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpio_eb, "gpio-eb", "aon-apb", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm0_eb, "pwm0-eb", "aon-apb", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm1_eb, "pwm1-eb", "aon-apb", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm2_eb, "pwm2-eb", "aon-apb", 0x0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm3_eb, "pwm3-eb", "aon-apb", 0x0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_eb,	 "kpd-eb", "aon-apb", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_eb, "aon-syst-eb", "aon-apb", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_eb, "ap-syst-eb", "aon-apb", 0x0,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_eb, "aon-tmr-eb", "aon-apb", 0x0,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_eb, "ap-tmr0-eb", "aon-apb", 0x0,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(efuse_eb, "efuse-eb", "aon-apb", 0x0,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_eb,	 "eic-eb", "aon-apb", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(intc_eb, "intc-eb", "aon-apb", 0x0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(adi_eb,	 "adi-eb", "aon-apb", 0x0,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audif_eb, "audif-eb", "aon-apb", 0x0,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aud_eb,	 "aud-eb", "aon-apb", 0x0,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vbc_eb,	 "vbc-eb", "aon-apb", 0x0,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_eb,	 "pin-eb", "aon-apb", 0x0,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipi_eb,	 "ipi-eb", "aon-apb", 0x0,
			0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(splk_eb, "splk-eb", "aon-apb", 0x0,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb, "ap-wdg-eb", "aon-apb", 0x0,
			0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_eb,	 "mm-eb", "aon-apb", 0x0,
			0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_ckg_eb, "aon-apb-ckg-eb", "aon-apb", 0x0,
			0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb,	 "gpu-eb", "aon-apb", 0x0,
			0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_ts0_eb, "ca53-ts0-eb", "aon-apb", 0x0,
			0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(wtlcp_intc_eb, "wtlcp-intc-eb", "aon-apb", 0x0,
			0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pubcp_intc_eb, "pubcp-intc-eb", "aon-apb", 0x0,
			0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_dap_eb, "ca53-dap-eb", "aon-apb", 0x0,
			0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_eb,	 "pmu-eb", "aon-apb",
			0x4, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm0_eb, "thm0-eb", "aon-apb",
			0x4, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb, "aux0-eb", "aon-apb",
			0x4, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux1_eb, "aux1-eb", "aon-apb",
			0x4, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux2_eb, "aux2-eb", "aon-apb",
			0x4, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(probe_eb, "probe-eb", "aon-apb",
			0x4, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emc_ref_eb, "emc-ref-eb", "aon-apb",
			0x4, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_wdg_eb, "ca53-wdg-eb", "aon-apb",
			0x4, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb, "ap-tmr1-eb", "aon-apb",
			0x4, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb, "ap-tmr2-eb", "aon-apb",
			0x4, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(disp_emc_eb, "disp-emc-eb", "aon-apb",
			0x4, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(zip_emc_eb, "zip-emc-eb", "aon-apb",
			0x4, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gsp_emc_eb, "gsp-emc-eb", "aon-apb",
			0x4, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_vsp_eb, "mm-vsp-eb", "aon-apb",
			0x4, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mdar_eb, "mdar-eb", "aon-apb",
			0x4, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_intc_eb, "aon-intc-eb", "aon-apb",
			0x4, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm1_eb, "thm1-eb", "aon-apb",
			0x4, 0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb, "djtag-eb", "aon-apb",
			0x4, 0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb, "mbox-eb", "aon-apb",
			0x4, 0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_dma_eb, "aon-dma-eb", "aon-apb",
			0x4, 0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(l_pll_d_eb, "l-pll-d-eb", "aon-apb",
			0x4, 0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(orp_jtag_eb, "orp-tag-eb", "aon-apb",
			0x4, 0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dbg_eb,	 "dbg-eb", "aon-apb",
			0x4, 0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dbg_emc_eb, "dbg-emc-eb", "aon-apb",
			0x4, 0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cross_trig_eb, "cross-trig-eb", "aon-apb",
			0x4, 0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes_dphy_eb, "serdes-dphy-eb", "aon-apb",
			0x4, 0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(arch_rtc_eb, "arch-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_rtc_eb, "kpd-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_rtc_eb, "aon-syst-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_rtc_eb, "ap-syst-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_rtc_eb, "aon-tmr-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_rtc_eb, "ap-tmr0-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtc_eb, "eic-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtcdv5_eb, "eic-rtcdv5-eb", "aon-apb",
			0x10, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_rtc_eb, "ap-wdg-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_wdg_rtc_eb, "ca53-wdg-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm_rtc_eb, "thm-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(athma_rtc_eb, "athma-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gthma_rtc_eb, "gthma-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(athma_rtc_a_eb, "athma-rtc-a-eb", "aon-apb",
			0x10, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gthma_rtc_a_eb, "gthma-rtc-a-eb", "aon-apb",
			0x10, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_rtc_eb, "ap-tmr2-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dxco_lc_rtc_eb, "dxco-lc-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bb_cal_rtc_eb, "bb-cal-rtc-eb", "aon-apb",
			0x10, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_eb, "cssys-eb", "aon-apb",
			0xb0, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_eb, "dmc_eb", "aon-apb",
			0xb0, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rosc_eb, "rosc-eb", "aon-apb",
			0xb0, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(s_d_cfg_eb, "s-d-cfg-eb", "aon-apb",
			0xb0, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(s_d_ref_eb, "s-d-ref-eb", "aon-apb",
			0xb0, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(b_dma_eb, "b-dma-eb", "aon-apb",
			0xb0, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(anlg_eb, "anlg-eb", "aon-apb",
			0xb0, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_apb_eb, "pin-apb-eb", "aon-apb",
			0xb0, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(anlg_apb_eb, "anlg-apb-eb", "aon-apb",
			0xb0, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bsmtmr_eb, "bsmtmr-eb", "aon-apb",
			0xb0, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_dap_eb, "ap-dap-eb", "aon-apb",
			0xb0, 0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apsim_aontop_eb, "apsim-aontop-eb", "aon-apb",
			0xb0, 0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tsen_eb, "tsen-eb", "aon-apb", 0x134,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_ca53_eb, "cssys-ca53-eb", "aon-apb", 0x134,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_hs_spi_eb, "ap-hs-spi-eb", "aon-apb", 0x134,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(det_32k_eb, "det-32k-eb", "aon-apb", 0x134,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tmr_eb, "tmr-eb", "aon-apb", 0x134,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apll_test_eb, "apll-test-eb", "aon-apb", 0x134,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9832e_aonapb_gate[] = {
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
	&ap_wdg_eb.common,
	&mm_eb.common,
	&aon_apb_ckg_eb.common,
	&gpu_eb.common,
	&ca53_ts0_eb.common,
	&wtlcp_intc_eb.common,
	&pubcp_intc_eb.common,
	&ca53_dap_eb.common,
	&pmu_eb.common,
	&thm0_eb.common,
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
	&aon_intc_eb.common,
	&thm1_eb.common,
	&djtag_eb.common,
	&mbox_eb.common,
	&aon_dma_eb.common,
	&l_pll_d_eb.common,
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
	&cssys_eb.common,
	&dmc_eb.common,
	&rosc_eb.common,
	&s_d_cfg_eb.common,
	&s_d_ref_eb.common,
	&b_dma_eb.common,
	&anlg_eb.common,
	&pin_apb_eb.common,
	&anlg_apb_eb.common,
	&bsmtmr_eb.common,
	&ap_dap_eb.common,
	&apsim_aontop_eb.common,
	&tsen_eb.common,
	&cssys_ca53_eb.common,
	&ap_hs_spi_eb.common,
	&det_32k_eb.common,
	&tmr_eb.common,
	&apll_test_eb.common,
};

static struct clk_hw_onecell_data sc9832e_aonapb_gate_hws = {
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
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_AON_APB_CKG_EB]	= &aon_apb_ckg_eb.common.hw,
		[CLK_GPU_EB]		= &gpu_eb.common.hw,
		[CLK_CA53_TS0_EB]	= &ca53_ts0_eb.common.hw,
		[CLK_WTLCP_INTC_EB]	= &wtlcp_intc_eb.common.hw,
		[CLK_PUBCP_INTC_EB]	= &pubcp_intc_eb.common.hw,
		[CLK_CA53_DAP_EB]	= &ca53_dap_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_THM0_EB]		= &thm0_eb.common.hw,
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
		[CLK_AON_INTC_EB]	= &aon_intc_eb.common.hw,
		[CLK_THM1_EB]		= &thm1_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_AON_DMA_EB]	= &aon_dma_eb.common.hw,
		[CLK_L_PLL_D_EB]	= &l_pll_d_eb.common.hw,
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
		[CLK_CSSYS_EB]		= &cssys_eb.common.hw,
		[CLK_DMC_EB]		= &dmc_eb.common.hw,
		[CLK_ROSC_EB]		= &rosc_eb.common.hw,
		[CLK_S_D_CFG_EB]	= &s_d_cfg_eb.common.hw,
		[CLK_S_D_REF_EB]	= &s_d_ref_eb.common.hw,
		[CLK_B_DMA_EB]		= &b_dma_eb.common.hw,
		[CLK_ANLG_EB]		= &anlg_eb.common.hw,
		[CLK_PIN_APB_EB]	= &pin_apb_eb.common.hw,
		[CLK_ANLG_APB_EB]	= &anlg_apb_eb.common.hw,
		[CLK_BSMTMR_EB]		= &bsmtmr_eb.common.hw,
		[CLK_AP_DAP_EB]		= &ap_dap_eb.common.hw,
		[CLK_APSIM_AONTOP_EB]	= &apsim_aontop_eb.common.hw,
		[CLK_TSEN_EB]		= &tsen_eb.common.hw,
		[CLK_CSSYS_CA53_EB]	= &cssys_ca53_eb.common.hw,
		[CLK_AP_HS_SPI_EB]	= &ap_hs_spi_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_TMR_EB]		= &tmr_eb.common.hw,
		[CLK_APLL_TEST_EB]	= &apll_test_eb.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc9832e_aonapb_gate_desc = {
	.clk_clks	= sc9832e_aonapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_aonapb_gate),
	.hw_clks	= &sc9832e_aonapb_gate_hws,
};

/* 0x71300000  ap_apb gate clocks */
static SPRD_SC_GATE_CLK(sim0_eb, "sim0-eb", "ext-26m", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis0_eb, "iis0-eb", "ext-26m", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apb_reg_eb, "apb-reg-eb", "ext-26m", 0x0,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_eb, "spi0-eb", "ext-26m", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_eb, "spi2-eb", "ext-26m", 0x0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c0_eb, "i2c0-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c1_eb, "i2c1-eb", "ext-26m", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c2_eb, "i2c2-eb", "ext-26m", 0x0,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c3_eb, "i2c3-eb", "ext-26m", 0x0,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c4_eb, "i2c4-eb", "ext-26m", 0x0,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart1_eb, "uart1-eb", "ext-26m", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_32k_eb, "sim0_32k-eb", "ext-26m", 0x0,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(intc0_eb, "intc0-eb", "ext-26m", 0x0,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(intc1_eb, "intc1-eb", "ext-26m", 0x0,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(intc2_eb, "intc2-eb", "ext-26m", 0x0,
			0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(intc3_eb, "intc3-eb", "ext-26m", 0x0,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9832e_apapb_gate[] = {
	/* address base is 0x71300000 */
	&sim0_eb.common,
	&iis0_eb.common,
	&apb_reg_eb.common,
	&spi0_eb.common,
	&spi2_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&i2c3_eb.common,
	&i2c4_eb.common,
	&uart1_eb.common,
	&sim0_32k_eb.common,
	&intc0_eb.common,
	&intc1_eb.common,
	&intc2_eb.common,
	&intc3_eb.common,
};

static struct clk_hw_onecell_data sc9832e_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_APB_REG_EB]	= &apb_reg_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_SPI2_EB]		= &spi2_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_I2C3_EB]		= &i2c3_eb.common.hw,
		[CLK_I2C4_EB]		= &i2c4_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_SIM0_32K_EB]	= &sim0_32k_eb.common.hw,
		[CLK_INTC0_EB]		= &intc0_eb.common.hw,
		[CLK_INTC1_EB]		= &intc1_eb.common.hw,
		[CLK_INTC2_EB]		= &intc2_eb.common.hw,
		[CLK_INTC3_EB]		= &intc3_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc9832e_apapb_gate_desc = {
	.clk_clks	= sc9832e_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_apapb_gate),
	.hw_clks	= &sc9832e_apapb_gate_hws,
};

/* 0x60e00000 mm domain clocks */
static const char * const mm_ahb_parents[] = { "ext-26m", "twpll-96m",
					       "twpll-128m", "twpll-153m6" };
static SPRD_MUX_CLK(mm_ahb, "mm-ahb", mm_ahb_parents, 0x20,
		    0, 2, SC9832E_MUX_FLAG);

static const char * const sensor_parents[] = { "ext-26m", "twpll-48m",
					       "twpll-76m8", "twpll-96m" };
static SPRD_COMP_CLK(sensor0_clk, "sensor0-clk", sensor_parents, 0x24,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(sensor1_clk, "sensor1-clk", sensor_parents, 0x28,
		     0, 2, 8, 3, 0);

static const char * const dcam_if_parents[] = { "twpll-76m8", "twpll-153m6",
						"twpll-256m", "twpll-307m2" };
static SPRD_MUX_CLK(dcam_if_clk, "dcam-if-clk", dcam_if_parents, 0x2c,
		    0, 2, SC9832E_MUX_FLAG);

static SPRD_MUX_CLK(jpg_clk, "jpg-clk", vsp_parents, 0x30,
		    0, 2, SC9832E_MUX_FLAG);
static SPRD_GATE_CLK(mipi_csi_clk, "mipi-csi-clk", "mm-ahb", 0x34,
		     BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mcsi_s_clk, "mcsi-s-clk", "mm-ahb", 0x3c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9832e_mm_clk[] = {
	&mm_ahb.common,
	&sensor0_clk.common,
	&sensor1_clk.common,
	&dcam_if_clk.common,
	&jpg_clk.common,
	&mipi_csi_clk.common,
	&mcsi_s_clk.common,
};

static struct clk_hw_onecell_data sc9832e_mm_clk_hws = {
	.hws	= {
		[CLK_MM_AHB]	= &mm_ahb.common.hw,
		[CLK_SENSOR0]	= &sensor0_clk.common.hw,
		[CLK_SENSOR1]	= &sensor1_clk.common.hw,
		[CLK_DCAM_IF]	= &dcam_if_clk.common.hw,
		[CLK_JPG]	= &jpg_clk.common.hw,
		[CLK_MIPI_CSI]	= &mipi_csi_clk.common.hw,
		[CLK_MCSI_S]	= &mcsi_s_clk.common.hw,
	},
	.num	= CLK_MM_CLK_NUM,
};

static const struct sprd_clk_desc sc9832e_mm_clk_desc = {
	.clk_clks	= sc9832e_mm_clk,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_mm_clk),
	.hw_clks	= &sc9832e_mm_clk_hws,
};

/* 0x60d00000 mm gate clocks */
static SPRD_GATE_CLK(dcam_eb, "dcam-eb", "mm-ahb", 0x0,
		     BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(isp_eb, "isp-eb", "mm-ahb", 0x0,
		     BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(cpp_eb, "cpp-eb", "mm-ahb", 0x0,
		     BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(csi_eb, "csi-eb", "mm-ahb", 0x0,
		     BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(csi_s_eb, "csi-s-eb", "mm-ahb", 0x0,
		     BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(jpg_eb, "jpg-eb", "mm-ahb", 0x0,
		     BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mahb_ckg_eb, "mahb-ckg-eb", "mm-ahb", 0x0,
		     BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(cphy_cfg_eb, "cphy-cfg-eb", "mm-ahb", 0x8,
		     BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(sensor0_eb, "sensor0-eb", "mm-ahb", 0x8,
		     BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(sensor1_eb, "sensor1-eb", "mm-ahb", 0x8,
		     BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(isp_axi_eb, "isp-axi-eb", "mm-ahb", 0x8,
		     BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mipi_csi_eb, "mipi-csi-eb", "mm-ahb", 0x8,
		     BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mipi_csi_s_eb, "mipi-csi-s-eb", "mm-ahb", 0x8,
		     BIT(5), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9832e_mm_gate[] = {
	&dcam_eb.common,
	&isp_eb.common,
	&cpp_eb.common,
	&csi_eb.common,
	&csi_s_eb.common,
	&jpg_eb.common,
	&mahb_ckg_eb.common,
	&cphy_cfg_eb.common,
	&sensor0_eb.common,
	&sensor1_eb.common,
	&isp_axi_eb.common,
	&mipi_csi_eb.common,
	&mipi_csi_s_eb.common,
};

static struct clk_hw_onecell_data sc9832e_mm_gate_hws = {
	.hws	= {
		[CLK_DCAM_EB]		= &dcam_eb.common.hw,
		[CLK_ISP_EB]		= &isp_eb.common.hw,
		[CLK_CPP_EB]		= &cpp_eb.common.hw,
		[CLK_CSI_EB]		= &csi_eb.common.hw,
		[CLK_CSI_S_EB]		= &csi_s_eb.common.hw,
		[CLK_JPG_EB]		= &jpg_eb.common.hw,
		[CLK_MAHB_CKG_EB]	= &mahb_ckg_eb.common.hw,
		[CLK_CPHY_CFG_EB]	= &cphy_cfg_eb.common.hw,
		[CLK_SENSOR0_EB]	= &sensor0_eb.common.hw,
		[CLK_SENSOR1_EB]	= &sensor1_eb.common.hw,
		[CLK_ISP_AXI_EB]	= &isp_axi_eb.common.hw,
		[CLK_MIPI_CSI_EB]	= &mipi_csi_eb.common.hw,
		[CLK_MIPI_CSI_S_EB]	= &mipi_csi_s_eb.common.hw,
	},
	.num	= CLK_MM_GATE_NUM,
};

static const struct sprd_clk_desc sc9832e_mm_gate_desc = {
	.clk_clks	= sc9832e_mm_gate,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_mm_gate),
	.hw_clks	= &sc9832e_mm_gate_hws,
};

/* 0x60100000 gpu clocks */
static const char * const gpu_parents[] = { "twpll-256m", "twpll-307m2",
					    "twpll-384m", "twpll-512m",
					    "gpll" };
static SPRD_COMP_CLK(gpu_clk, "gpu-clk", gpu_parents, 0x4,
		     0, 3, 4, 3, 0);

static struct sprd_clk_common *sc9832e_gpu_clk[] = {
	&gpu_clk.common,
};

static struct clk_hw_onecell_data sc9832e_gpu_clk_hws = {
	.hws	= {
		[CLK_GPU] = &gpu_clk.common.hw,
	},
	.num	= CLK_GPU_CLK_NUM,
};

static struct sprd_clk_desc sc9832e_gpu_clk_desc = {
	.clk_clks	= sc9832e_gpu_clk,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_gpu_clk),
	.hw_clks	= &sc9832e_gpu_clk_hws,
};

/* 0x20e00000 ap_ahb gate clocks */
static SPRD_SC_GATE_CLK(dsi_eb, "dsi-eb", "ap-axi", 0x0, 0x1000,
			BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dispc_eb, "dispc-eb", "ap-axi", 0x0, 0x1000,
			BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vsp_eb, "vsp-eb", "ap-axi", 0x0, 0x1000,
			BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gsp_eb, "gsp-eb", "ap-axi", 0x0, 0x1000,
			BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_eb, "otg-eb", "ap-axi", 0x0, 0x1000,
			BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_pub_eb, "dma-pub-eb", "ap-axi", 0x0, 0x1000,
			BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_pub_eb, "ce-pub-eb", "ap-axi", 0x0, 0x1000,
			BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ahb_ckg_eb, "ahb-ckg-eb", "ap-axi", 0x0, 0x1000,
			BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_eb, "sdio0-eb", "ap-axi", 0x0, 0x1000,
			BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_eb, "sdio1-eb", "ap-axi", 0x0, 0x1000,
			BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_eb, "nandc-eb", "ap-axi", 0x0, 0x1000,
			BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_eb, "emmc-eb", "ap-axi", 0x0, 0x1000,
			BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spinlock_eb, "spinlock-eb", "ap-axi", 0x0, 0x1000,
			BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_efuse_eb, "ce-efuse-eb", "ap-axi", 0x0, 0x1000,
			BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb, "emmc-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_32k_eb, "sdio0-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_32k_eb, "sdio1-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(29), CLK_IGNORE_UNUSED, 0);

static const char * const mcu_parents[] = { "ext-26m", "twpll-512m",
					    "twpll-768m", "mpll" };
static SPRD_COMP_CLK(mcu_clk, "mcu", mcu_parents, 0x54,
		     0, 3, 8, 3, 0);

static struct sprd_clk_common *sc9832e_apahb_gate[] = {
	/* address base is 0x20e00000 */
	&dsi_eb.common,
	&dispc_eb.common,
	&vsp_eb.common,
	&gsp_eb.common,
	&otg_eb.common,
	&dma_pub_eb.common,
	&ce_pub_eb.common,
	&ahb_ckg_eb.common,
	&sdio0_eb.common,
	&sdio1_eb.common,
	&nandc_eb.common,
	&emmc_eb.common,
	&spinlock_eb.common,
	&ce_efuse_eb.common,
	&emmc_32k_eb.common,
	&sdio0_32k_eb.common,
	&sdio1_32k_eb.common,
	&mcu_clk.common,
};

static struct clk_hw_onecell_data sc9832e_apahb_gate_hws = {
	.hws	= {
		[CLK_DSI_EB]		= &dsi_eb.common.hw,
		[DISPC_EB]		= &dispc_eb.common.hw,
		[VSP_EB]		= &vsp_eb.common.hw,
		[GSP_EB]		= &gsp_eb.common.hw,
		[OTG_EB]		= &otg_eb.common.hw,
		[DMA_PUB_EB]		= &dma_pub_eb.common.hw,
		[CE_PUB_EB]		= &ce_pub_eb.common.hw,
		[AHB_CKG_EB]		= &ahb_ckg_eb.common.hw,
		[SDIO0_EB]		= &sdio0_eb.common.hw,
		[SDIO1_EB]		= &sdio1_eb.common.hw,
		[NANDC_EB]		= &nandc_eb.common.hw,
		[EMMC_EB]		= &emmc_eb.common.hw,
		[SPINLOCK_EB]		= &spinlock_eb.common.hw,
		[CE_EFUSE_EB]		= &ce_efuse_eb.common.hw,
		[EMMC_32K_EB]		= &emmc_32k_eb.common.hw,
		[SDIO0_32K_EB]		= &sdio0_32k_eb.common.hw,
		[SDIO1_32K_EB]		= &sdio1_32k_eb.common.hw,
		[CLK_MCU]		= &mcu_clk.common.hw,
	},
	.num	= CLK_APAHB_GATE_NUM,
};

static const struct sprd_clk_desc sc9832e_apahb_gate_desc = {
	.clk_clks	= sc9832e_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(sc9832e_apahb_gate),
	.hw_clks	= &sc9832e_apahb_gate_hws,
};

static const struct of_device_id sprd_sc9832e_clk_ids[] = {
	{ .compatible = "sprd,sc9832e-pmu-gate",	/* 0x402b0000 */
	  .data = &sc9832e_pmu_gate_desc },
	{ .compatible = "sprd,sc9832e-pll",	/* 0x403c0000 */
	  .data = &sc9832e_pll_desc },
	{ .compatible = "sprd,sc9832e-dpll",	/* 0x403d0000 */
	  .data = &sc9832e_dpll_desc },
	{ .compatible = "sprd,sc9832e-mpll",	/* 0x403f0000 */
	  .data = &sc9832e_mpll_desc },
	{ .compatible = "sprd,sc9832e-rpll",	/* 0x40410000 */
	  .data = &sc9832e_rpll_desc },
	{ .compatible = "sprd,sc9832e-ap-clks",		/* 0x21500000 */
	  .data = &sc9832e_ap_clk_desc },
	{ .compatible = "sprd,sc9832e-aon-prediv",	/* 0x402d0000 */
	  .data = &sc9832e_aon_prediv_desc },
	{ .compatible = "sprd,sc9832e-apahb-gate",		/* 0x20e00000 */
	  .data = &sc9832e_apahb_gate_desc },
	{ .compatible = "sprd,sc9832e-aonapb-gate",		/* 0x402e0000 */
	  .data = &sc9832e_aonapb_gate_desc },
	{ .compatible = "sprd,sc9832e-gpu-clk",		/* 0x60100000 */
	  .data = &sc9832e_gpu_clk_desc },
	{ .compatible = "sprd,sc9832e-mm-gate",		/* 0x60d00000 */
	  .data = &sc9832e_mm_gate_desc },
	{ .compatible = "sprd,sc9832e-mm-clk",		/* 0x60e00000 */
	  .data = &sc9832e_mm_clk_desc },
	{ .compatible = "sprd,sc9832e-apapb-gate",		/* 0x71300000 */
	  .data = &sc9832e_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_sc9832e_clk_ids);

static int sc9832e_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;
	int ret;

	match = of_match_node(sprd_sc9832e_clk_ids, pdev->dev.of_node);
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

static struct platform_driver sc9832e_clk_driver = {
	.probe	= sc9832e_clk_probe,
	.driver	= {
		.name	= "sc9832e-clk",
		.of_match_table	= sprd_sc9832e_clk_ids,
	},
};
module_platform_driver(sc9832e_clk_driver);

MODULE_DESCRIPTION("Spreadtrum SC9832E Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sc9832e-clk");
