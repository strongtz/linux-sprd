// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum SC7731E clock driver
//
// Copyright (C) 2019 Spreadtrum, Inc.
// Author: Hao Li <ben.li@spreadtrum.com>

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,sc7731e-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

#define SC7731E_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* 0x402b0000 pmu apb, pll gates */
static CLK_FIXED_FACTOR(fac_13m, "fac-13m", "ext-26m", 2, 1, 0);
static CLK_FIXED_FACTOR(fac_6m5, "fac-6m5", "ext-26m", 4, 1, 0);
static CLK_FIXED_FACTOR(fac_4m3, "fac-4m3", "ext-26m", 6, 1, 0);
static CLK_FIXED_FACTOR(fac_2m, "fac-2m", "ext-26m", 13, 1, 0);
static CLK_FIXED_FACTOR(fac_1m, "fac-1m", "ext-26m", 26, 1, 0);
static CLK_FIXED_FACTOR(fac_250k, "fac-250k", "ext-26m", 104, 1, 0);
SPRD_PLL_SC_GATE_CLK(cpll_gate, "cpll-gate", "ext-26m", 0x88,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(gpll_gate, "gpll-gate", "ext-26m", 0x90,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(mpll_gate, "mpll-gate", "ext-26m", 0x94,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(dpll_gate, "dpll-gate", "ext-26m", 0x98,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(bbpll_gate, "bbpll-gate", "ext-26m", 0x344,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *sc7731e_pmu_gate_clks[] = {
	&cpll_gate.common,
	&gpll_gate.common,
	&mpll_gate.common,
	&dpll_gate.common,
	&bbpll_gate.common,
};

static struct clk_hw_onecell_data sc7731e_pmu_gate_hws = {
	.hws	= {
		[CLK_FAC_13M]		= &fac_13m.hw,
		[CLK_FAC_6M5]		= &fac_6m5.hw,
		[CLK_FAC_4M3]		= &fac_4m3.hw,
		[CLK_FAC_2M]		= &fac_2m.hw,
		[CLK_FAC_1M]		= &fac_1m.hw,
		[CLK_FAC_250K]		= &fac_250k.hw,
		[CLK_CPLL_GATE]		= &cpll_gate.common.hw,
		[CLK_GPLL_GATE]		= &gpll_gate.common.hw,
		[CLK_MPLL_GATE]		= &mpll_gate.common.hw,
		[CLK_DPLL_GATE]		= &dpll_gate.common.hw,
		[CLK_BBPLL_GATE]	= &bbpll_gate.common.hw,
	},
	.num	= CLK_PMU_APB_NUM,
};

static const struct sprd_clk_desc sc7731e_pmu_gate_desc = {
	.clk_clks	= sc7731e_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_pmu_gate_clks),
	.hw_clks        = &sc7731e_pmu_gate_hws,
};

/* 0x402e0000 aon apb, pll register */
static const struct freq_table ftable[5] = {
	{ .ibias = 0, .max_freq = 951000000ULL },
	{ .ibias = 1, .max_freq = 1131000000ULL },
	{ .ibias = 2, .max_freq = 1145000000ULL },
	{ .ibias = 3, .max_freq = 1600000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static const struct clk_bit_field f_twpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 20,	.width = 1 },	/* div_s	*/
	{ .shift = 19,	.width = 1 },	/* mod_en	*/
	{ .shift = 18,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 11,	.width = 2 },	/* ibias	*/
	{ .shift = 0,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};

#define f_mpll f_twpll
static SPRD_PLL_WITH_ITABLE_1K(mpll_clk, "mpll", "mpll-gate", 0x44,
			       2, ftable, f_mpll, 240);
#define f_dpll f_twpll
static SPRD_PLL_WITH_ITABLE_1K(dpll_clk, "dpll", "dpll-gate", 0x4c,
			       2, ftable, f_dpll, 240);

static SPRD_PLL_WITH_ITABLE_1K(twpll_clk, "twpll", "ext-26m", 0x54,
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

#define f_cpll f_twpll
static SPRD_PLL_WITH_ITABLE_1K(cpll_clk, "cpll", "cpll-gate", 0x150,
			       2, ftable, f_cpll, 240);
static CLK_FIXED_FACTOR(cpll_800m, "cpll-800m", "cpll", 2, 1, 0);
static CLK_FIXED_FACTOR(cpll_533m, "cpll-533m", "cpll", 3, 1, 0);
static CLK_FIXED_FACTOR(cpll_400m, "cpll-400m", "cpll", 4, 1, 0);
static CLK_FIXED_FACTOR(cpll_320m, "cpll-320m", "cpll", 5, 1, 0);
static CLK_FIXED_FACTOR(cpll_266m67, "cpll-266m67", "cpll", 6, 1, 0);
static CLK_FIXED_FACTOR(cpll_228m57, "cpll-228m57", "cpll", 7, 1, 0);
static CLK_FIXED_FACTOR(cpll_200m, "cpll-200m", "cpll", 8, 1, 0);
static CLK_FIXED_FACTOR(cpll_160m, "cpll-160m", "cpll", 10, 1, 0);
static CLK_FIXED_FACTOR(cpll_133m34, "cpll-133m34", "cpll", 12, 1, 0);
static CLK_FIXED_FACTOR(cpll_100m, "cpll-100m", "cpll", 16, 1, 0);
static CLK_FIXED_FACTOR(cpll_50m, "cpll-50m", "cpll", 32, 1, 0);
static CLK_FIXED_FACTOR(cpll_40m, "cpll-40m", "cpll", 40, 1, 0);

static const struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 20,	.width = 1 },	/* div_s	*/
	{ .shift = 19,	.width = 1 },	/* mod_en	*/
	{ .shift = 18,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 11,	.width = 2 },	/* ibias	*/
	{ .shift = 0,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 13,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(gpll_clk, "gpll", "gpll-gate", 0x158,
				   2, ftable, f_gpll, 240,
				   1000, 1000, 1, 600000000);

static CLK_FIXED_FACTOR(bbpll_416m, "bbpll-416m", "bbpll", 3, 1, 0);

static struct sprd_clk_common *sc7731e_pll_clks[] = {
	&twpll_clk.common,
	&cpll_clk.common,
	&mpll_clk.common,
	&dpll_clk.common,
	&gpll_clk.common,
};

static struct clk_hw_onecell_data sc7731e_pll_hws = {
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
		[CLK_CPLL]		= &cpll_clk.common.hw,
		[CLK_CPPLL_800M]	= &cpll_800m.hw,
		[CLK_CPPLL_533M]	= &cpll_533m.hw,
		[CLK_CPPLL_400M]	= &cpll_400m.hw,
		[CLK_CPPLL_320M]	= &cpll_320m.hw,
		[CLK_CPPLL_266M]	= &cpll_266m67.hw,
		[CLK_CPPLL_228M]	= &cpll_228m57.hw,
		[CLK_CPPLL_200M]	= &cpll_200m.hw,
		[CLK_CPPLL_160M]	= &cpll_160m.hw,
		[CLK_CPPLL_133M]	= &cpll_133m34.hw,
		[CLK_CPPLL_100M]	= &cpll_100m.hw,
		[CLK_CPPLL_50M]		= &cpll_50m.hw,
		[CLK_CPPLL_40M]		= &cpll_40m.hw,
		[CLK_MPLL]		= &mpll_clk.common.hw,
		[CLK_DPLL]		= &dpll_clk.common.hw,
		[CLK_GPLL]		= &gpll_clk.common.hw,
		[CLK_BBPLL_416M]	= &bbpll_416m.hw,

	},
	.num	= CLK_AON_PLL_NUM,
};

static const struct sprd_clk_desc sc7731e_pll_desc = {
	.clk_clks	= sc7731e_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_pll_clks),
	.hw_clks        = &sc7731e_pll_hws,
};

/* 0x20e00000 ap-ahb clocks */
static SPRD_SC_GATE_CLK(dsi_eb, "dsi-eb", "ap-axi", 0x0, 0x1000,
			BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dispc_eb, "dispc-eb", "ap-axi", 0x0, 0x1000,
			BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gsp_eb, "gsp-eb", "ap-axi", 0x0, 0x1000,
			BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_eb, "otg-eb", "ap-axi", 0x0, 0x1000,
			BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_eb, "dma-eb", "ap-axi", 0x0, 0x1000,
			BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_eb, "ce-eb", "ap-axi", 0x0, 0x1000,
			BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_eb, "sdio0-eb", "ap-axi", 0x0, 0x1000,
			BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_eb, "nandc-eb", "ap-axi", 0x0, 0x1000,
			BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_eb, "emmc-eb", "ap-axi", 0x0, 0x1000,
			BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_sec_eb, "ce-sec-eb", "ap-axi", 0x0, 0x1000,
			BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb, "emmc-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_32k_eb, "sdio0-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_ecc_eb, "nandc-ecc-eb", "ap-axi", 0x0, 0x1000,
			BIT(30), CLK_IGNORE_UNUSED, 0);

/*
 * notice: composite clock driver don't support 2 regs!!!
 */
static const char * const mcu_parents[] = { "ext-26m", "twpll-512m",
					"twpll-768m", "dpll",
					"cpll", "twpll",
					"mpll" };
static SPRD_COMP_CLK(mcu_clk, "mcu-clk", mcu_parents, 0x54,
		     0, 3, 4, 3, 0);
static SPRD_DIV_CLK(ca7_axi_clk, "ca7-axi-clk", "mcu-clk", 0x54,
		    8, 3, 0);
static SPRD_DIV_CLK(ca7_dbg_clk, "ca7-dbg-clk", "mcu-clk", 0x54,
		    16, 3, 0);

static struct sprd_clk_common *sc7731e_apahb_gate_clks[] = {
	&dsi_eb.common,
	&dispc_eb.common,
	&gsp_eb.common,
	&otg_eb.common,
	&dma_eb.common,
	&ce_eb.common,
	&sdio0_eb.common,
	&nandc_eb.common,
	&emmc_eb.common,
	&ce_sec_eb.common,
	&emmc_32k_eb.common,
	&sdio0_32k_eb.common,
	&nandc_ecc_eb.common,
	&mcu_clk.common,
	&ca7_axi_clk.common,
	&ca7_dbg_clk.common,
};

static struct clk_hw_onecell_data sc7731e_apahb_gate_hws = {
	.hws	= {
		[CLK_DSI_EB]		= &dsi_eb.common.hw,
		[CLK_DISPC_EB]		= &dispc_eb.common.hw,
		[CLK_GSP_EB]		= &gsp_eb.common.hw,
		[CLK_OTG_EB]		= &otg_eb.common.hw,
		[CLK_DMA_EB]		= &dma_eb.common.hw,
		[CLK_CE_EB]		= &ce_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_NANDC_EB]		= &nandc_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_CE_SEC_EB]		= &ce_sec_eb.common.hw,
		[CLK_EMMC_32K_EB]	= &emmc_32k_eb.common.hw,
		[CLK_SDIO0_32K_EB]	= &sdio0_32k_eb.common.hw,
		[CLK_NANDC_ECC_EB]	= &nandc_ecc_eb.common.hw,
		[CLK_MCU]		= &mcu_clk.common.hw,
		[CLK_CA7_AXI]		= &ca7_axi_clk.common.hw,
		[CLK_CA7_DBG]		= &ca7_dbg_clk.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc sc7731e_apahb_gate_desc = {
	.clk_clks	= sc7731e_apahb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_apahb_gate_clks),
	.hw_clks	= &sc7731e_apahb_gate_hws,
};

/* 0x21500000 ap clocks */
static const char * const ap_axi_parents[] = { "ext-26m", "twpll-76m8",
					       "twpll-128m", "twpll-256m",
					       "cpll-266m67" };
static SPRD_MUX_CLK(ap_axi, "ap-axi", ap_axi_parents, 0x20,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const ap_ahb_parents[] = { "ext-26m", "twpll-76m8",
					       "twpll-128m", "twpll-192m",
					       "cpll-200m" };
static SPRD_MUX_CLK(ap_ahb_clk, "ap-ahb-clk", ap_ahb_parents, 0x24,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const ap_apb_parents[] = { "ext-26m", "twpll-64m",
					       "twpll-96m", "cpll-100m",
					       "twpll-128m" };
static SPRD_MUX_CLK(ap_apb_clk, "ap-apb-clk", ap_apb_parents, 0x28,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const gsp_parents[] = { "twpll-153m6", "twpll-256m",
					"twpll-307m2", "cpll-320m",
					"twpll-384m" };
static SPRD_MUX_CLK(gsp_clk, "gsp-clk", gsp_parents, 0x2c,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const dispc0_parents[] = { "twpll-153m6", "twpll-153m6",
					"twpll-192m", "twpll-256m",
					"cpll-320m", "twpll-384m" };
static SPRD_MUX_CLK(dispc0_clk, "dispc0-clk", dispc0_parents, 0x30,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const dispc0_dpi_parents[] = { "twpll-96m", "cpll-100m",
					"twpll-128m", "twpll-153m6",
					"twpll-192m" };
static SPRD_COMP_CLK(dispc0_dpi_clk, "dispc0-dpi-clk", dispc0_dpi_parents, 0x34,
		     0, 3, 8, 4, 0);

static SPRD_GATE_CLK(dsi_rxsec_clk, "dsi-rxsec-clk", "ap-ahb-clk", 0x38,
		     BIT(16), 0, 0);
static SPRD_GATE_CLK(dlanebyte_clk, "dlanebyte-clk", "ap-ahb-clk", 0x3c,
		     BIT(16), 0, 0);
static SPRD_GATE_CLK(otg_utmi_clk, "otg-utmi-clk", "ap-ahb-clk", 0x4c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const ap_uart_parents[] = { "ext-26m", "twpll-48m",
					"twpll-51m2", "twpll-96m" };
static SPRD_COMP_CLK(ap_uart0_clk, "ap-uart0-clk", ap_uart_parents, 0x50,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_uart1_clk, "ap-uart1-clk", ap_uart_parents, 0x54,
		     0, 2, 8, 3, 0);

static const char * const i2c_parents[] = { "ext-26m", "twpll-48m",
					    "twpll-51m2", "twpll-153m6" };
static SPRD_COMP_CLK(ap_i2c0_clk, "ap-i2c0-clk", i2c_parents, 0x58,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c1_clk, "ap-i2c1-clk", i2c_parents, 0x5c,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c2_clk, "ap-i2c2-clk", i2c_parents, 0x60,
		     0, 2, 8, 3, 0);

static const char * const spi_parents[] = { "ext-26m", "cpll-100m",
					"twpll_128m", "twpll-153m6",
					"twpll-192m" };
static SPRD_COMP_CLK(ap_spi0_clk, "ap-spi0-clk", spi_parents, 0x64,
		     0, 3, 8, 3, 0);

static const char * const iis_parents[] = { "ext-26m", "twpll-128m",
					"twpll-153m6" };
static SPRD_COMP_CLK(ap_iis0_clk, "ap-iis0-clk", iis_parents, 0x68,
		     0, 2, 8, 3, 0);

static const char * const ap_ce_parents[] = { "ext-26m", "twpll-96m",
					"cpll-100m", "twpll-192m",
					"twpll-256m" };

static SPRD_MUX_CLK(ap_ce_clk, "ap-ce-clk", ap_ce_parents, 0x6c,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const nandc_ecc_parents[] = { "ext-26m", "cpll-200m",
					"twpll-256m", "twpll-307m2" };
static SPRD_MUX_CLK(nandc_ecc_clk, "nandc-ecc-clk", nandc_ecc_parents, 0x78,
		    0, 2, SC7731E_MUX_FLAG);

static struct sprd_clk_common *sc7731e_ap_clks[] = {
	&ap_axi.common,
	&ap_ahb_clk.common,
	&ap_apb_clk.common,
	&gsp_clk.common,
	&dispc0_clk.common,
	&dispc0_dpi_clk.common,
	&dsi_rxsec_clk.common,
	&dlanebyte_clk.common,
	&otg_utmi_clk.common,
	&ap_uart0_clk.common,
	&ap_uart1_clk.common,
	&ap_i2c0_clk.common,
	&ap_i2c1_clk.common,
	&ap_i2c2_clk.common,
	&ap_spi0_clk.common,
	&ap_iis0_clk.common,
	&ap_ce_clk.common,
	&nandc_ecc_clk.common,
};

static struct clk_hw_onecell_data sc7731e_ap_clk_hws = {
	.hws	= {
		[CLK_AP_AXI]		= &ap_axi.common.hw,
		[CLK_AP_AHB]	= &ap_ahb_clk.common.hw,
		[CLK_AP_APB]	= &ap_apb_clk.common.hw,
		[CLK_GSP]	= &gsp_clk.common.hw,
		[CLK_DISPC0]	= &dispc0_clk.common.hw,
		[CLK_DISPC0_DPI]	= &dispc0_dpi_clk.common.hw,
		[CLK_DSI_RXSEC]	= &dsi_rxsec_clk.common.hw,
		[CLK_DLANEBYTE]	= &dlanebyte_clk.common.hw,
		[CLK_OTG_UTMI]	= &otg_utmi_clk.common.hw,
		[CLK_AP_UART0]	= &ap_uart0_clk.common.hw,
		[CLK_AP_UART1]	= &ap_uart1_clk.common.hw,
		[CLK_AP_I2C0]	= &ap_i2c0_clk.common.hw,
		[CLK_AP_I2C1]	= &ap_i2c1_clk.common.hw,
		[CLK_AP_I2C2]	= &ap_i2c2_clk.common.hw,
		[CLK_AP_SPI0]	= &ap_spi0_clk.common.hw,
		[CLK_AP_IIS0]	= &ap_iis0_clk.common.hw,
		[CLK_AP_CE]	= &ap_ce_clk.common.hw,
		[CLK_NANDC_ECC]	= &nandc_ecc_clk.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static const struct sprd_clk_desc sc7731e_ap_clk_desc = {
	.clk_clks	= sc7731e_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_ap_clks),
	.hw_clks	= &sc7731e_ap_clk_hws,
};

/* 0x402d0000 aon clocks */
static const char * const emc_clk_parents[] = { "ext-26m", "twpll-384m",
						 "twpll-512m", "twpll-768m",
						 "cpll-800m", "dpll" };
static SPRD_COMP_CLK(emc_clk, "emc-clk", emc_clk_parents, 0x220,
		     0, 3, 8, 2, 0);

static const char * const aon_apb_parents[] = { "ext-26m", "cpll-100m",
						"twpll-128m" };
static SPRD_COMP_CLK(aon_apb_clk, "aon-apb", aon_apb_parents, 0x230,
		     0, 2, 8, 2, 0);

static const char * const adi_parents[] = { "ext-26m", "cpll-50m",
					"twpll-51m2" };
static SPRD_MUX_CLK(adi_clk, "adi-clk", adi_parents, 0x234,
		    0, 2, SC7731E_MUX_FLAG);

static const char * const pwm_parents[] = { "ext-32k", "ext-26m-cp",
					"ext-26m", "cpll-40m",
					"twpll-48m" };
static SPRD_MUX_CLK(pwm0_clk, "pwm0-clk", pwm_parents, 0x248,
		    0, 3, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK(pwm1_clk, "pwm1-clk", pwm_parents, 0x24c,
		    0, 3, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK(pwm2_clk, "pwm2-clk", pwm_parents, 0x250,
		    0, 3, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK(pwm3_clk, "pwm3-clk", pwm_parents, 0x254,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const aon_thm_parents[] = { "ext-32k", "fac-250k" };
static SPRD_MUX_CLK(aon_thm_clk, "aon-thm-clk", aon_thm_parents, 0x268,
		    0, 1, SC7731E_MUX_FLAG);

static const char * const aon_i2c_parents[] = { "ext-26m", "twpll-48m",
					    "twpll-51m2", "cpll-133m34",
					    "twpll-153m6" };
static SPRD_COMP_CLK(aon_i2c0_clk, "aon-i2c0-clk", aon_i2c_parents, 0x26c,
		     0, 2, 8, 3, 0);

static const char * const avs_parents[] = { "ext-32k", "twpll-48m",
					"cpll-50m", "twpll-51m2",
					"twpll-96m" };
static SPRD_MUX_CLK(avs_clk, "avs-clk", avs_parents, 0x270,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const audif_parents[] = { "ext-26m", "twpll-38m4",
					"cpll-50m", "twpll-51m2" };
static SPRD_MUX_CLK(audif_clk, "audif-clk", audif_parents, 0x278,
		    0, 2, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK(iis_da0_clk, "iis-da0-clk", "aon-apb", 0x280,
		     BIT(16), 0, 0);
static SPRD_GATE_CLK(iis0_ad0_clk, "iis0-ad0-clk", "aon-apb", 0x284,
		     BIT(16), 0, 0);
static SPRD_GATE_CLK(iis1_ad0_clk, "iis1-ad0-clk", "aon-apb", 0x288,
		     BIT(16), 0, 0);

static const char * const cpu_dap_parents[] = { "ext-26m-aud", "twpll-76m8",
						"cpll-100m", "twpll-153m6" };
static SPRD_MUX_CLK(cpu_dap_clk, "cpu-dap-clk", cpu_dap_parents, 0x28c,
		    0, 2, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK(cdap_mtck_clk, "cdap-mtck-clk", "aon-apb", 0x290,
		     BIT(16), 0, 0);

static const char * const cpu_ts_parents[] = { "ext-32k", "ext-26m",
					       "twpll-128m", "cpll-133m34",
					       "twpll-153m6" };
static SPRD_MUX_CLK(cpu_ts_clk, "cpu-ts-clk", cpu_ts_parents, 0x294,
		    0, 3, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK(djtag_tck_clk, "djtag-tck-clk", "aon-apb", 0x298,
		     BIT(16), 0, 0);

static const char * const emc_ref_parents[] = { "fac-6m5", "fac-13m",
						"ext-26m" };
static SPRD_MUX_CLK(emc_ref_clk, "emc-ref-clk", emc_ref_parents, 0x2a8,
		    0, 2, SC7731E_MUX_FLAG);

static const char * const cssys_parents[] = { "ext-26m", "twpll-96m",
					      "twpll-128m", "twpll-153m6",
					      "cpll-266m67", "twpll-384m",
					      "twpll-512m" };
static SPRD_COMP_CLK(cssys_clk,	"cssys-clk", cssys_parents, 0x2ac,
		     0, 3, 8, 2, 0);

static SPRD_DIV_CLK(cssys_ca7_clk, "cssys-ca7-clk", "cssys-clk", 0x2b0,
		    8, 1, 0);

static const char * const sdio_parents[] = { "fac-1m", "ext-26m",
					"twpll-307m2", "twpll-384m",
					"cpll-400m", "bbpll-416m" };
static SPRD_MUX_CLK(sdio0_2x, "sdio0-2x", sdio_parents, 0x2bc,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const nandc_parents[] = { "fac-1m", "ext-26m",
					"twpll-153m6", "twpll-170m6",
					"cpll-200m", "twpll-219m4",
					"cpll-228m57", "cpll-266m67" };
static SPRD_MUX_CLK(nandc_2x, "nandc-2x", nandc_parents, 0x2c4,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const emmc_parents[] = { "fac-1m", "ext-26m",
					"twpll-307m2", "twpll-384m",
					"cpll-400m", "bbpll-416m" };
static SPRD_MUX_CLK(emmc_2x, "emmc-2x", emmc_parents, 0x2cc,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const ap_hs_parents[] = { "ext-26m", "twpll-128m",
					      "cpll-133m34", "twpll-153m6",
					      "twpll-192m" };
static SPRD_COMP_CLK(ap_hs_spi_clk, "ap-hs-spi-clk", ap_hs_parents, 0x2e0,
		     0, 3, 8, 3, 0);

static const char * const sdphy_apb_parents[] = { "ext-26m", "cpll-40m",
						"twpll-48m" };
static SPRD_MUX_CLK(sdphy_apb_clk, "sdphy-apb-clk", sdphy_apb_parents, 0x2e4,
		    0, 2, SC7731E_MUX_FLAG);

static const char * const analog_apb_parents[] = { "ext-26m", "cpll-40m",
						"twpll-48m" };
static SPRD_MUX_CLK(analog_apb_clk, "analog-apb-clk", analog_apb_parents, 0x2f0,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const io_apb_parents[] = { "ext-26m", "cpll-40m",
						"twpll-48m" };
static SPRD_COMP_CLK(io_apb_clk, "io-apb-clk", io_apb_parents, 0x2f4,
		     0, 2, 8, 2, 0);

static SPRD_GATE_CLK(dtck_hw_clk, "dtck-hw-clk", "aon-apb", 0x2f8,
		     BIT(16), 0, 0);

static struct sprd_clk_common *sc7731e_aon_clks[] = {
	&emc_clk.common,
	&aon_apb_clk.common,
	&adi_clk.common,
	&pwm0_clk.common,
	&pwm1_clk.common,
	&pwm2_clk.common,
	&pwm3_clk.common,
	&aon_thm_clk.common,
	&aon_i2c0_clk.common,
	&avs_clk.common,
	&audif_clk.common,
	&iis_da0_clk.common,
	&iis0_ad0_clk.common,
	&iis1_ad0_clk.common,
	&cpu_dap_clk.common,
	&cdap_mtck_clk.common,
	&cpu_ts_clk.common,
	&djtag_tck_clk.common,
	&emc_ref_clk.common,
	&cssys_clk.common,
	&cssys_ca7_clk.common,
	&sdio0_2x.common,
	&nandc_2x.common,
	&emmc_2x.common,
	&ap_hs_spi_clk.common,
	&sdphy_apb_clk.common,
	&analog_apb_clk.common,
	&io_apb_clk.common,
	&dtck_hw_clk.common,
};

static struct clk_hw_onecell_data sc7731e_aon_clk_hws = {
	.hws	= {
		[CLK_EMC]	= &emc_clk.common.hw,
		[CLK_AON_APB]	= &aon_apb_clk.common.hw,
		[CLK_ADI]	= &adi_clk.common.hw,
		[CLK_PWM0]	= &pwm0_clk.common.hw,
		[CLK_PWM1]	= &pwm1_clk.common.hw,
		[CLK_PWM2]	= &pwm2_clk.common.hw,
		[CLK_PWM3]	= &pwm3_clk.common.hw,
		[CLK_AON_THM]	= &aon_thm_clk.common.hw,
		[CLK_AON_I2C0]	= &aon_i2c0_clk.common.hw,
		[CLK_AVS]	= &avs_clk.common.hw,
		[CLK_AUDIF]	= &audif_clk.common.hw,
		[CLK_IIS_DA0]	= &iis_da0_clk.common.hw,
		[CLK_IIS0_AD0]	= &iis0_ad0_clk.common.hw,
		[CLK_IIS1_AD0]	= &iis1_ad0_clk.common.hw,
		[CLK_CPU_DAP]	= &cpu_dap_clk.common.hw,
		[CLK_CDAP_MTCK]	= &cdap_mtck_clk.common.hw,
		[CLK_CPU_TS]	= &cpu_ts_clk.common.hw,
		[CLK_DJTAG_TCK]	= &djtag_tck_clk.common.hw,
		[CLK_EMC_REF]	= &emc_ref_clk.common.hw,
		[CLK_CSSYS]	= &cssys_clk.common.hw,
		[CLK_CSSYS_CA7]	= &cssys_ca7_clk.common.hw,
		[CLK_SDIO0_2X]	= &sdio0_2x.common.hw,
		[CLK_NANDC_2X]	= &nandc_2x.common.hw,
		[CLK_EMMC_2X]	= &emmc_2x.common.hw,
		[CLK_AP_HS_SPI]	= &ap_hs_spi_clk.common.hw,
		[CLK_SDPHY_APB]	= &sdphy_apb_clk.common.hw,
		[CLK_ANALOG_APB]	= &analog_apb_clk.common.hw,
		[CLK_IO_APB]	= &io_apb_clk.common.hw,
		[CLK_DTCK_HW]	= &dtck_hw_clk.common.hw,
	},
	.num	= CLK_AON_CLK_NUM,
};

static const struct sprd_clk_desc sc7731e_aon_clk_desc = {
	.clk_clks	= sc7731e_aon_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_aon_clks),
	.hw_clks	= &sc7731e_aon_clk_hws,
};

/* 0x402e0000 aon gate clocks */
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
static SPRD_SC_GATE_CLK(splk_eb,	"splk-eb",	"aon-apb", 0x0,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb,	"ap-wdg-eb",	"aon-apb", 0x0,
			0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_eb,		"mm-eb",	"aon-apb", 0x0,
			0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_ckg_eb,	"aon-apb-ckg-eb", "aon-apb", 0x0,
			0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb,		"gpu-eb",	"aon-apb", 0x0,
			0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
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
static SPRD_SC_GATE_CLK(avs_eb,	"avs-eb",		"aon-apb",
			0x4, 0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
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
static SPRD_SC_GATE_CLK(gsp_emc_eb,	"gsp-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_vsp_eb,	"mm-vsp-eb",		"aon-apb",
			0x4, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mdar_eb,	"mdar-eb",		"aon-apb",
			0x4, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtc4m0_cal_eb,	"rtc4m0-cal-eb",	"aon-apb",
			0x4, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb,	"djtag-eb",		"aon-apb",
			0x4, 0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb,	"mbox-eb",		"aon-apb",
			0x4, 0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_dma_eb,	"aon-dma-eb",		"aon-apb",
			0x4, 0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cm4_djtag_eb,	"cm4-djtag-eb",		"aon-apb",
			0x4, 0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(wcn_eb,	"wcn-eb",		"aon-apb",
			0x4, 0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_def_eb,	"aon-apb-def-eb",	"aon-apb",
			0x4, 0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
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

static SPRD_SC_GATE_CLK(audio_gate, "audio-gate", "ext-26m", 0x14,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);

static const char * const aux_parents[] = { "ext-32k", "ext-26m-cp",
					"ext-26m" };
static SPRD_COMP_CLK(aux0_clk, "aux0-clk", aux_parents, 0x88,
		     0, 2, 16, 4, 0);

static SPRD_SC_GATE_CLK(cssys_eb, "cssys-eb", "aon-apb", 0xb0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_eb, "dmc-eb", "aon-apb", 0xb0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pub_reg_eb, "pub-reg-eb", "aon-apb", 0xb0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
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
static SPRD_SC_GATE_CLK(pin_apb_eb, "pin-apb-eb", "aon-apb", 0xb0,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(anlg_apb_eb, "anlg-apb-eb", "aon-apb", 0xb0,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bsmtmr_eb, "bsmtmr-eb", "aon-apb", 0xb0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_dap_eb, "ap-dap-eb", "aon-apb", 0xb0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(emmc_1x_eb, "emmc-1x-eb", "aon-apb", 0x134,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_2x_eb, "emmc-2x-eb", "aon-apb", 0x134,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_1x_eb, "sdio0-1x-eb", "aon-apb", 0x134,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_2x_eb, "sdio0-2x-eb", "aon-apb", 0x134,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_1x_eb, "sdio1-1x-eb", "aon-apb", 0x134,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_2x_eb, "sdio1-2x-eb", "aon-apb", 0x134,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_1x_eb, "nandc-1x-eb", "aon-apb", 0x134,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_2x_eb, "nandc-2x-eb", "aon-apb", 0x134,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_ca7_eb, "cssys-ca7-eb", "aon-apb", 0x134,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_hs_spi_eb, "ap-hs-spi-eb", "aon-apb", 0x134,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(det_32k_eb, "det-32k-eb", "aon-apb", 0x134,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tmr_eb, "tmr-eb", "aon-apb", 0x134,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apll_test_eb, "apll-test-eb", "aon-apb", 0x134,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc7731e_aonapb_gate_clks[] = {
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
	&splk_eb.common,
	&ap_wdg_eb.common,
	&mm_eb.common,
	&aon_apb_ckg_eb.common,
	&gpu_eb.common,
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
	&avs_eb.common,
	&emc_ref_eb.common,
	&ca53_wdg_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&disp_emc_eb.common,
	&gsp_emc_eb.common,
	&mm_vsp_eb.common,
	&mdar_eb.common,
	&rtc4m0_cal_eb.common,
	&djtag_eb.common,
	&mbox_eb.common,
	&aon_dma_eb.common,
	&cm4_djtag_eb.common,
	&wcn_eb.common,
	&aon_apb_def_eb.common,
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
	&audio_gate.common,
	&aux0_clk.common,
	&cssys_eb.common,
	&dmc_eb.common,
	&pub_reg_eb.common,
	&rosc_eb.common,
	&s_d_cfg_eb.common,
	&s_d_ref_eb.common,
	&b_dma_eb.common,
	&anlg_eb.common,
	&pin_apb_eb.common,
	&anlg_apb_eb.common,
	&bsmtmr_eb.common,
	&ap_dap_eb.common,
	&emmc_1x_eb.common,
	&emmc_2x_eb.common,
	&sdio0_1x_eb.common,
	&sdio0_2x_eb.common,
	&sdio1_1x_eb.common,
	&sdio1_2x_eb.common,
	&nandc_1x_eb.common,
	&nandc_2x_eb.common,
	&cssys_ca7_eb.common,
	&ap_hs_spi_eb.common,
	&det_32k_eb.common,
	&tmr_eb.common,
	&apll_test_eb.common,
};

static struct clk_hw_onecell_data sc7731e_aonapb_gate_hws = {
	.hws	= {
		[CLK_GPIO_EB]	= &gpio_eb.common.hw,
		[CLK_PWM0_EB]	= &pwm0_eb.common.hw,
		[CLK_PWM1_EB]	= &pwm1_eb.common.hw,
		[CLK_PWM2_EB]	= &pwm2_eb.common.hw,
		[CLK_PWM3_EB]	= &pwm3_eb.common.hw,
		[CLK_KPD_EB]	= &kpd_eb.common.hw,
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
		[CLK_SPLK_EB]		= &splk_eb.common.hw,
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_AON_APB_CKG_EB]	= &aon_apb_ckg_eb.common.hw,
		[CLK_GPU_EB]		= &gpu_eb.common.hw,
		[CLK_CA53_TS0_EB]	= &ca53_ts0_eb.common.hw,
		[CLK_CA53_TS1_EB]	= &ca53_ts1_eb.common.hw,
		[CLK_CA53_DAP_EB]	= &ca53_dap_eb.common.hw,
		[CLK_I2C_EB]		= &i2c_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_THM_EB]		= &thm_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_AVS_EB]		= &avs_eb.common.hw,
		[CLK_EMC_REF_EB]	= &emc_ref_eb.common.hw,
		[CLK_CA53_WDG_EB]	= &ca53_wdg_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_DISP_EMC_EB]	= &disp_emc_eb.common.hw,
		[CLK_GSP_EMC_EB]	= &gsp_emc_eb.common.hw,
		[CLK_MM_VSP_EB]		= &mm_vsp_eb.common.hw,
		[CLK_MDAR_EB]		= &mdar_eb.common.hw,
		[CLK_RTC4M0_CAL_EB]	= &rtc4m0_cal_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_AON_DMA_EB]	= &aon_dma_eb.common.hw,
		[CLK_CM4_DJTAG_EB]	= &cm4_djtag_eb.common.hw,
		[CLK_WCN_EB]		= &wcn_eb.common.hw,
		[CLK_AON_APB_DEF_EB]	= &aon_apb_def_eb.common.hw,
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
		[CLK_AUDIO_GATE]	= &audio_gate.common.hw,
		[CLK_AUX0]		= &aux0_clk.common.hw,
		[CLK_CSSYS_EB]		= &cssys_eb.common.hw,
		[CLK_DMC_EB]		= &dmc_eb.common.hw,
		[CLK_PUB_REG_EB]	= &pub_reg_eb.common.hw,
		[CLK_ROSC_EB]		= &rosc_eb.common.hw,
		[CLK_S_D_CFG_EB]	= &s_d_cfg_eb.common.hw,
		[CLK_S_D_REF_EB]	= &s_d_ref_eb.common.hw,
		[CLK_B_DMA_EB]		= &b_dma_eb.common.hw,
		[CLK_ANLG_EB]		= &anlg_eb.common.hw,
		[CLK_PIN_APB_EB]	= &pin_apb_eb.common.hw,
		[CLK_ANLG_APB_EB]	= &anlg_apb_eb.common.hw,
		[CLK_BSMTMR_EB]		= &bsmtmr_eb.common.hw,
		[CLK_AP_DAP_EB]		= &ap_dap_eb.common.hw,
		[CLK_EMMC_1X_EB]	= &emmc_1x_eb.common.hw,
		[CLK_EMMC_2X_EB]	= &emmc_2x_eb.common.hw,
		[CLK_SDIO0_1X_EB]	= &sdio0_1x_eb.common.hw,
		[CLK_SDIO0_2X_EB]	= &sdio0_2x_eb.common.hw,
		[CLK_SDIO1_1X_EB]	= &sdio1_1x_eb.common.hw,
		[CLK_SDIO1_2X_EB]	= &sdio1_2x_eb.common.hw,
		[CLK_NANDC_1X_EB]	= &nandc_1x_eb.common.hw,
		[CLK_NANDC_2X_EB]	= &nandc_2x_eb.common.hw,
		[CLK_CSSYS_CA7_EB]	= &cssys_ca7_eb.common.hw,
		[CLK_AP_HS_SPI_EB]	= &ap_hs_spi_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_TMR_EB]		= &tmr_eb.common.hw,
		[CLK_APLL_TEST_EB]	= &apll_test_eb.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc7731e_aonapb_gate_desc = {
	.clk_clks	= sc7731e_aonapb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_aonapb_gate_clks),
	.hw_clks	= &sc7731e_aonapb_gate_hws,
};

/* 0x60100000 gpu clocks */
static const char * const gpu_parents[] = { "twpll-153m6", "twpll-192m",
					"twpll-256m", "twpll-307m2",
					"twpll-384m", "twpll-512m",
					"gpll" };
static SPRD_COMP_CLK(gpu_clk, "gpu-clk", gpu_parents, 0x4,
		     0, 3, 4, 2, 0);

static struct sprd_clk_common *sc7731e_gpu_clk[] = {
	&gpu_clk.common,
};

static struct clk_hw_onecell_data sc7731e_gpu_clk_hws = {
	.hws	= {
		[CLK_GPU] = &gpu_clk.common.hw,
	},
	.num	= CLK_GPU_CLK_NUM,
};

static struct sprd_clk_desc sc7731e_gpu_clk_desc = {
	.clk_clks	= sc7731e_gpu_clk,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_gpu_clk),
	.hw_clks	= &sc7731e_gpu_clk_hws,
};

/* 0x60d00000 mm gate clocks */
static SPRD_SC_GATE_CLK(dcam_eb, "dcam-eb", "mm-ahb", 0x0, 0x1000,
			BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(isp_eb, "isp-eb", "mm-ahb", 0x0, 0x1000,
			BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vsp_eb, "vsp-eb", "mm-ahb", 0x0, 0x1000,
			BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(csi_eb, "csi-eb", "mm-ahb", 0x0, 0x1000,
			BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(jpg_eb, "jpg-eb", "mm-ahb", 0x0, 0x1000,
			BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_ckg_eb, "mm-ckg-eb", "mm-ahb", 0x0, 0x1000,
			BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vsp_mq_eb, "vsp-mq-eb", "mm-ahb", 0x0, 0x1000,
			BIT(6), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(mcphy_cfg_eb, "mcphy-cfg-eb", "mm-ahb", 0x8,
		     BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(msensor0_eb, "msensor0-eb", "mm-ahb", 0x8,
		     BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(misp_axi_eb,	"misp-axi-eb", "mm-ahb", 0x8,
		     BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mdcam_axi_eb, "mdcam-axi-eb", "mm-ahb", 0x8,
		     BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(mmipi_csi_eb, "mmipi-csi-eb", "mm-ahb", 0x8,
		     BIT(4), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc7731e_mm_gate_clks[] = {
	&dcam_eb.common,
	&isp_eb.common,
	&vsp_eb.common,
	&csi_eb.common,
	&jpg_eb.common,
	&mm_ckg_eb.common,
	&vsp_mq_eb.common,
	&mcphy_cfg_eb.common,
	&msensor0_eb.common,
	&misp_axi_eb.common,
	&mdcam_axi_eb.common,
	&mmipi_csi_eb.common,
};

static struct clk_hw_onecell_data sc7731e_mm_gate_hws = {
	.hws	= {
		[CLK_DCAM_EB]		= &dcam_eb.common.hw,
		[CLK_ISP_EB]		= &isp_eb.common.hw,
		[CLK_VSP_EB]		= &vsp_eb.common.hw,
		[CLK_CSI_EB]		= &csi_eb.common.hw,
		[CLK_JPG_EB]		= &jpg_eb.common.hw,
		[CLK_MM_CKG_EB]		= &mm_ckg_eb.common.hw,
		[CLK_VSP_MQ_EB]		= &vsp_mq_eb.common.hw,
		[CLK_MCPHY_CFG_EB]	= &mcphy_cfg_eb.common.hw,
		[CLK_MSENSOR0_EB]	= &msensor0_eb.common.hw,
		[CLK_MISP_AXI_EB]	= &misp_axi_eb.common.hw,
		[CLK_MDCAM_AXI_EB]	= &mdcam_axi_eb.common.hw,
		[CLK_MMIPI_CSI_EB]	= &mmipi_csi_eb.common.hw,
	},
	.num	= CLK_MM_GATE_NUM,
};

static const struct sprd_clk_desc sc7731e_mm_gate_desc = {
	.clk_clks	= sc7731e_mm_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_mm_gate_clks),
	.hw_clks	= &sc7731e_mm_gate_hws,
};

/* 0x60e00000 mm domain clocks */
static const char * const mm_ahb_parents[] = { "ext-26m", "twpll-96m",
						"twpll-128m", "twpll-153m6",
						"cpll-160m" };
static SPRD_MUX_CLK(mm_ahb, "mm-ahb", mm_ahb_parents, 0x20,
		    0, 3, SC7731E_MUX_FLAG);

static const char * const sensor_parents[] = { "ext-26m", "twpll-48m",
						"twpll-76m8", "twpll-96m" };
static SPRD_COMP_CLK(sensor0_clk, "sensor0-clk", sensor_parents, 0x24,
		     0, 2, 8, 3, 0);

static const char * const dcam_if_parents[] = { "twpll-128m", "twpll-256m",
						"twpll-307m2", "cpll-320m" };
static SPRD_MUX_CLK(dcam_if_clk, "dcam-if-clk", dcam_if_parents, 0x28,
		    0, 2, SC7731E_MUX_FLAG);

static const char * const vsp_parents[] = { "twpll-128m", "twpll-256m",
					"twpll-307m2", "cpll-320m"};
static SPRD_MUX_CLK(clk_vsp, "vsp-clk", vsp_parents, 0x2c,
		    0, 2, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK(isp_clk, "isp-clk", vsp_parents, 0x30,
		    0, 2, SC7731E_MUX_FLAG);

static const char * const jpg_parents[] = { "twpll-76m8", "twpll-128m",
					    "twpll-256m", "cpll-266m67" };
static SPRD_MUX_CLK(jpg_clk, "jpg-clk", jpg_parents, 0x34,
		    0, 2, SC7731E_MUX_FLAG);

static SPRD_GATE_CLK(mipi_csi_clk, "mipi-csi-clk", "mm-ahb", 0x38,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const dcam_axi_parents[] = { "twpll-307m2", "cpll-320m",
						"twpll-512m", "cpll-533m" };
static SPRD_MUX_CLK(dcam_axi_clk, "dcam-axi-clk", dcam_axi_parents, 0x40,
		    0, 2, SC7731E_MUX_FLAG);
static SPRD_MUX_CLK(isp_axi_clk, "isp-axi-clk", dcam_axi_parents, 0x44,
		    0, 2, SC7731E_MUX_FLAG);

static struct sprd_clk_common *sc7731e_mm_clk_clks[] = {
	&mm_ahb.common,
	&sensor0_clk.common,
	&dcam_if_clk.common,
	&clk_vsp.common,
	&isp_clk.common,
	&jpg_clk.common,
	&mipi_csi_clk.common,
	&dcam_axi_clk.common,
	&isp_axi_clk.common,
};

static struct clk_hw_onecell_data sc7731e_mm_clk_hws = {
	.hws	= {
		[CLK_MM_AHB]	= &mm_ahb.common.hw,
		[CLK_SENSOR0]	= &sensor0_clk.common.hw,
		[CLK_DCAM_IF]	= &dcam_if_clk.common.hw,
		[CLK_VSP]	= &clk_vsp.common.hw,
		[CLK_ISP]	= &isp_clk.common.hw,
		[CLK_JPG]	= &jpg_clk.common.hw,
		[CLK_MIPI_CSI]	= &mipi_csi_clk.common.hw,
		[CLK_DCAM_AXI]	= &dcam_axi_clk.common.hw,
		[CLK_ISP_AXI]	= &isp_axi_clk.common.hw,
	},
	.num	= CLK_MM_CLK_NUM,
};

static const struct sprd_clk_desc sc7731e_mm_clk_desc = {
	.clk_clks	= sc7731e_mm_clk_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_mm_clk_clks),
	.hw_clks	= &sc7731e_mm_clk_hws,
};

/* 0x71300000 ap-apb clocks */
static SPRD_SC_GATE_CLK(sim0_eb, "sim0-eb", "ext-26m", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis0_eb, "iis0-eb", "ext-26m", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_eb, "spi0-eb", "ext-26m", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c0_eb, "i2c0-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c1_eb, "i2c1-eb", "ext-26m", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c2_eb, "i2c2-eb", "ext-26m", 0x0,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart0_eb, "uart0-eb", "ext-26m", 0x0,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
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

static struct sprd_clk_common *sc7731e_apapb_gate[] = {
	&sim0_eb.common,
	&iis0_eb.common,
	&spi0_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&uart0_eb.common,
	&uart1_eb.common,
	&sim0_32k_eb.common,
	&intc0_eb.common,
	&intc1_eb.common,
	&intc2_eb.common,
	&intc3_eb.common,
};

static struct clk_hw_onecell_data sc7731e_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_UART0_EB]		= &uart0_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_SIM0_32K_EB]	= &sim0_32k_eb.common.hw,
		[CLK_INTC0_EB]		= &intc0_eb.common.hw,
		[CLK_INTC1_EB]		= &intc1_eb.common.hw,
		[CLK_INTC2_EB]		= &intc2_eb.common.hw,
		[CLK_INTC3_EB]		= &intc3_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc7731e_apapb_gate_desc = {
	.clk_clks	= sc7731e_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_apapb_gate),
	.hw_clks	= &sc7731e_apapb_gate_hws,
};

static const struct of_device_id sprd_sc7731e_clk_ids[] = {
	{ .compatible = "sprd,sc7731e-pmu-gate",	/* 0x402b0000 */
	  .data = &sc7731e_pmu_gate_desc },
	{ .compatible = "sprd,sc7731e-pll",		/* 0x402e0000 */
	  .data = &sc7731e_pll_desc },
	{ .compatible = "sprd,sc7731e-apahb-gate",	/* 0x20e00000 */
	  .data = &sc7731e_apahb_gate_desc },
	{ .compatible = "sprd,sc7731e-ap-clk",		/* 0x21500000 */
	  .data = &sc7731e_ap_clk_desc },
	{ .compatible = "sprd,sc7731e-aon-clk",		/* 0x402d0000 */
	  .data = &sc7731e_aon_clk_desc },
	{ .compatible = "sprd,sc7731e-aonapb-gate",	/* 0x402e0000 */
	  .data = &sc7731e_aonapb_gate_desc },
	{ .compatible = "sprd,sc7731e-gpu-clk",		/* 0x60100000 */
	  .data = &sc7731e_gpu_clk_desc },
	{ .compatible = "sprd,sc7731e-mm-gate",		/* 0x60d00000 */
	  .data = &sc7731e_mm_gate_desc },
	{ .compatible = "sprd,sc7731e-mm-clk",		/* 0x60e00000 */
	  .data = &sc7731e_mm_clk_desc },
	{ .compatible = "sprd,sc7731e-apapb-gate",	/* 0x71300000 */
	  .data = &sc7731e_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_sc7731e_clk_ids);

static int sc7731e_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;

	match = of_match_node(sprd_sc7731e_clk_ids, pdev->dev.of_node);
	if (!match) {
		pr_err("%s: of_match_node() failed", __func__);
		return -ENODEV;
	}

	desc = match->data;
	sprd_clk_regmap_init(pdev, desc);

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver sc7731e_clk_driver = {
	.probe	= sc7731e_clk_probe,
	.driver	= {
		.name	= "sc7731e-clk",
		.of_match_table	= sprd_sc7731e_clk_ids,
	},
};
module_platform_driver(sc7731e_clk_driver);

MODULE_DESCRIPTION("Spreadtrum SC7731E Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sc7731e-clk");
