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

#include <dt-bindings/clock/sprd,orca-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

/* pmu gates clock */
static SPRD_PLL_SC_GATE_CLK(mpll0_gate, "mpll0-gate", "ext-26m", 0x190,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpll1_gate, "mpll1-gate", "ext-26m", 0x194,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *orca_pmu_gate_clks[] = {
	/* address base is 0x64010000 */
	&mpll0_gate.common,
	&mpll1_gate.common,
};

static struct clk_hw_onecell_data orca_pmu_gate_hws = {
	.hws	= {
		[CLK_MPLL0_GATE]  = &mpll0_gate.common.hw,
		[CLK_MPLL1_GATE]  = &mpll1_gate.common.hw,
	},
	.num = CLK_PMU_GATE_NUM,
};

static struct sprd_clk_desc orca_pmu_gate_desc = {
	.clk_clks	= orca_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(orca_pmu_gate_clks),
	.hw_clks        = &orca_pmu_gate_hws,
};

/* pll clock at g3 */
static CLK_FIXED_FACTOR(v3rpll, "v3rpll", "ext-26m", 1, 15, 0);
static CLK_FIXED_FACTOR(v3rpll_195m, "v3rpll-195m", "v3rpll", 2, 1, 0);

static struct freq_table v3pll_ftable[7] = {
	{ .ibias = 2, .max_freq = 900000000ULL },
	{ .ibias = 3, .max_freq = 1100000000ULL },
	{ .ibias = 4, .max_freq = 1300000000ULL },
	{ .ibias = 5, .max_freq = 1500000000ULL },
	{ .ibias = 6, .max_freq = 1600000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_v3pll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(v3pll, "v3pll", "ext-26m", 0x14,
			       2, v3pll_ftable, f_v3pll, 240);
static CLK_FIXED_FACTOR(v3pll_768m, "v3pll-768m", "v3pll", 2, 1, 0);
static CLK_FIXED_FACTOR(v3pll_384m, "v3pll-384m", "v3pll", 4, 1, 0);
static CLK_FIXED_FACTOR(v3pll_192m, "v3pll-192m", "v3pll", 8, 1, 0);
static CLK_FIXED_FACTOR(v3pll_96m, "v3pll-96m", "v3pll", 16, 1, 0);
static CLK_FIXED_FACTOR(v3pll_48m, "v3pll-48m", "v3pll", 32, 1, 0);
static CLK_FIXED_FACTOR(v3pll_24m, "v3pll-24m", "v3pll", 64, 1, 0);
static CLK_FIXED_FACTOR(v3pll_12m, "v3pll-12m", "v3pll", 128, 1, 0);
static CLK_FIXED_FACTOR(v3pll_512m, "v3pll-512m", "v3pll", 3, 1, 0);
static CLK_FIXED_FACTOR(v3pll_256m, "v3pll-256m", "v3pll", 6, 1, 0);
static CLK_FIXED_FACTOR(v3pll_128m, "v3pll-128m", "v3pll", 12, 1, 0);
static CLK_FIXED_FACTOR(v3pll_64m, "v3pll-64m", "v3pll", 24, 1, 0);
static CLK_FIXED_FACTOR(v3pll_307m2, "v3pll-307m2", "v3pll", 5, 1, 0);
static CLK_FIXED_FACTOR(v3pll_219m4, "v3pll-219m4", "v3pll", 7, 1, 0);
static CLK_FIXED_FACTOR(v3pll_170m6, "v3pll-170m6", "v3pll", 9, 1, 0);
static CLK_FIXED_FACTOR(v3pll_153m6, "v3pll-153m6", "v3pll", 10, 1, 0);
static CLK_FIXED_FACTOR(v3pll_76m8, "v3pll-76m8", "v3pll", 20, 1, 0);
static CLK_FIXED_FACTOR(v3pll_51m2, "v3pll-51m2", "v3pll", 30, 1, 0);
static CLK_FIXED_FACTOR(v3pll_38m4, "v3pll-38m4", "v3pll", 40, 1, 0);
static CLK_FIXED_FACTOR(v3pll_19m2, "v3pll-19m2", "v3pll", 80, 1, 0);

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
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 1,	.width = 0 },	/* mod_en	*/
	{ .shift = 2,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 46,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll0, "mpll0", "mpll0-gate", 0x9c,
				   2, mpll_ftable, f_mpll, 240,
				   1000, 1000, 1, 1200000000ULL);
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll1, "mpll1", "mpll1-gate", 0xb0,
				   2, mpll_ftable, f_mpll, 240,
				   1000, 1000, 1, 1200000000ULL);

static struct sprd_clk_common *orca_g3_pll_clks[] = {
	/* address base is 0x634b0000 */
	&v3pll.common,
	&mpll0.common,
	&mpll1.common,
};

static struct clk_hw_onecell_data orca_g3_pll_hws = {
	.hws	= {
		[CLK_V3RPLL] = &v3rpll.hw,
		[CLK_V3RPLL_195M] = &v3rpll_195m.hw,
		[CLK_V3PLL] = &v3pll.common.hw,
		[CLK_TWPLL_768M] = &v3pll_768m.hw,
		[CLK_TWPLL_384M] = &v3pll_384m.hw,
		[CLK_TWPLL_192M] = &v3pll_192m.hw,
		[CLK_TWPLL_96M] = &v3pll_96m.hw,
		[CLK_TWPLL_48M] = &v3pll_48m.hw,
		[CLK_TWPLL_24M] = &v3pll_24m.hw,
		[CLK_TWPLL_12M] = &v3pll_12m.hw,
		[CLK_TWPLL_512M] = &v3pll_512m.hw,
		[CLK_TWPLL_256M] = &v3pll_256m.hw,
		[CLK_TWPLL_128M] = &v3pll_128m.hw,
		[CLK_TWPLL_64M] = &v3pll_64m.hw,
		[CLK_TWPLL_307M2] = &v3pll_307m2.hw,
		[CLK_TWPLL_219M4] = &v3pll_219m4.hw,
		[CLK_TWPLL_170M6] = &v3pll_170m6.hw,
		[CLK_TWPLL_153M6] = &v3pll_153m6.hw,
		[CLK_TWPLL_76M8] = &v3pll_76m8.hw,
		[CLK_TWPLL_51M2] = &v3pll_51m2.hw,
		[CLK_TWPLL_38M4] = &v3pll_38m4.hw,
		[CLK_TWPLL_19M2] = &v3pll_19m2.hw,
		[CLK_MPLL0] = &mpll0.common.hw,
		[CLK_MPLL1] = &mpll1.common.hw,
	},
	.num	= CLK_ANLG_PHY_G3_NUM,
};

static struct sprd_clk_desc orca_g3_pll_desc = {
	.clk_clks	= orca_g3_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(orca_g3_pll_clks),
	.hw_clks	= &orca_g3_pll_hws,
};

/* apcpu clocks */
static const char * const core_parents[] = { "ext-26m", "v3pll-512m",
					     "v3pll-768m", "mpll0" };
static SPRD_COMP_CLK(core0_clk, "core0-clk", core_parents,
		     0x20, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK(core1_clk, "core1-clk", core_parents,
		     0x24, 0, 2, 8, 3, 0);

static const char * const scu_parents[] = { "ext-26m", "v3pll-512m",
					    "v3pll-768m", "mpll1" };
static SPRD_COMP_CLK(scu_clk, "scu-clk", scu_parents,
		     0x28, 0, 2, 8, 3, 0);
static SPRD_DIV_CLK(ace_clk, "ace-clk", "scu-clk", 0x2c,
		    8, 3, 0);

static const char * const gic_parents[] = { "ext-26m", "v3pll-153m6",
					    "v3pll-384m", "v3pll-512m" };
static SPRD_COMP_CLK(gic_clk, "gic-clk", gic_parents,
		     0x38, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK(periph_clk, "periph-clk", gic_parents,
		     0x3c, 0, 2, 8, 3, 0);

static struct sprd_clk_common *orca_apcpu_clk_clks[] = {
	/* address base is 0x63970000 */
	&core0_clk.common,
	&core1_clk.common,
	&scu_clk.common,
	&ace_clk.common,
	&gic_clk.common,
	&periph_clk.common,
};

static struct clk_hw_onecell_data orca_apcpu_clk_hws = {
	.hws	= {
		[CLK_CORE0_CLK] = &core0_clk.common.hw,
		[CLK_CORE1_CLK] = &core1_clk.common.hw,
		[CLK_SCU_CLK] = &scu_clk.common.hw,
		[CLK_ACE_CLK] = &ace_clk.common.hw,
		[CLK_GIC_CLK] = &gic_clk.common.hw,
		[CLK_PERIPH_CLK] = &periph_clk.common.hw,
	},
	.num	= CLK_APCPU_CLK_NUM,
};

static struct sprd_clk_desc orca_apcpu_clk_desc = {
	.clk_clks	= orca_apcpu_clk_clks,
	.num_clk_clks	= ARRAY_SIZE(orca_apcpu_clk_clks),
	.hw_clks	= &orca_apcpu_clk_hws,
};

/* ap ahb gates */
static SPRD_SC_GATE_CLK(apahb_ckg_eb, "apahb-ckg-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_eb, "nandc-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_ecc_eb, "nandc-ecc-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_26m_eb, "nandc-26m-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_eb, "dma-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_eb2, "dma-eb2", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb0_eb, "usb0-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb0_suspend_eb, "usb0-suspend-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb0_ref_eb, "usb0-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio_mst_eb, "sdio-mst-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio_mst_32k_eb, "sdio-mst-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_eb, "emmc-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb, "emmc-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_apahb_gate[] = {
	/* address base is 0x21000000 */
	&apahb_ckg_eb.common,
	&nandc_eb.common,
	&nandc_ecc_eb.common,
	&nandc_26m_eb.common,
	&dma_eb.common,
	&dma_eb2.common,
	&usb0_eb.common,
	&usb0_suspend_eb.common,
	&usb0_ref_eb.common,
	&sdio_mst_eb.common,
	&sdio_mst_32k_eb.common,
	&emmc_eb.common,
	&emmc_32k_eb.common,
};

static struct clk_hw_onecell_data orca_apahb_gate_hws = {
	.hws	= {
		[CLK_APAHB_CKG_EB] = &apahb_ckg_eb.common.hw,
		[CLK_NANDC_EB] = &nandc_eb.common.hw,
		[CLK_NANDC_ECC_EB] = &nandc_ecc_eb.common.hw,
		[CLK_NANDC_26M_EB] = &nandc_26m_eb.common.hw,
		[CLK_DMA_EB] = &dma_eb.common.hw,
		[CLK_DMA_EB2] = &dma_eb2.common.hw,
		[CLK_USB0_EB] = &usb0_eb.common.hw,
		[CLK_USB0_SUSPEND_EB] = &usb0_suspend_eb.common.hw,
		[CLK_USB0_REF_EB] = &usb0_ref_eb.common.hw,
		[CLK_SDIO_MST_EB] = &sdio_mst_eb.common.hw,
		[CLK_SDIO_MST_32K_EB] = &sdio_mst_32k_eb.common.hw,
		[CLK_EMMC_EB] = &emmc_eb.common.hw,
		[CLK_EMMC_32K_EB] = &emmc_32k_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc orca_apahb_gate_desc = {
	.clk_clks	= orca_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(orca_apahb_gate),
	.hw_clks	= &orca_apahb_gate_hws,
};

/* ap clocks */
#define ORCA_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

static const char * const ap_axi_parents[] = { "ext-26m", "v3pll-64m",
					       "v3pll-96m", "v3pll-128m",
					       "v3pll-256m", "v3rpll" };
static SPRD_MUX_CLK(ap_axi_clk, "ap-axi-clk", ap_axi_parents, 0x20,
			0, 3, ORCA_MUX_FLAG);

static const char * const peri_apb_parents[] = { "ext-26m", "v3pll-64m",
						 "v3pll-96m", "v3pll-128m" };
static SPRD_MUX_CLK(peri_apb_clk, "peri-apb-clk", peri_apb_parents, 0x24,
			0, 2, ORCA_MUX_FLAG);

static const char * const nandc_ecc_parents[] = { "ext-26m", "v3pll-256m",
						  "v3pll-307m2" };
static SPRD_COMP_CLK(nandc_ecc_clk, "nandc-ecc-clk", nandc_ecc_parents,
		     0x28, 0, 2, 8, 3, 0);

static const char * const usb_ref_parents[] = { "ext-32k", "v3pll-24m" };
static SPRD_MUX_CLK(usb0_ref_clk, "usb0-ref-clk", usb_ref_parents, 0x3c,
			0, 1, ORCA_MUX_FLAG);
static SPRD_MUX_CLK(usb1_ref_clk, "usb1-ref-clk", usb_ref_parents, 0x40,
			0, 1, ORCA_MUX_FLAG);

static SPRD_GATE_CLK(pcie_aux_clk, "pcie-aux-clk", "ext-26m", 0x44,
			BIT(0), CLK_IGNORE_UNUSED, 0);

static const char * const ap_uart_parents[] = { "ext-26m", "v3pll-48m",
						"v3pll-51m2", "v3pll-96m" };
static SPRD_COMP_CLK(ap_uart0_clk, "ap-uart0-clk", ap_uart_parents,
		     0x48, 0, 2, 8, 3, 0);

static const char * const ap_i2c_parents[] = { "ext-26m", "v3pll-48m",
					       "v3pll-51m2", "v3pll-153m6" };
static SPRD_COMP_CLK(ap_i2c0_clk, "ap-i2c0-clk", ap_i2c_parents,
		     0x4c, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c1_clk, "ap-i2c1-clk", ap_i2c_parents,
		     0x50, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c2_clk, "ap-i2c2-clk", ap_i2c_parents,
		     0x54, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c3_clk, "ap-i2c3-clk", ap_i2c_parents,
		     0x58, 0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_i2c4_clk, "ap-i2c4-clk", ap_i2c_parents,
		     0x5c, 0, 2, 8, 3, 0);

static const char * const ap_sim_parents[] = { "ext-26m", "v3pll-51m2",
					       "v3pll-64m", "v3pll-96m",
					       "v3pll-128m" };
static SPRD_COMP_CLK(ap_sim_clk, "ap-sim-clk", ap_sim_parents,
		     0x64, 0, 3, 8, 3, 0);

static const char * const pwm_parents[] = { "clk-32k", "ext-26m",
					    "rco-4m", "rco-25m",
					    "v3pll-48m" };
static SPRD_MUX_CLK(pwm0_clk, "pwm0-clk", pwm_parents, 0x68,
		    0, 3, ORCA_MUX_FLAG);
static SPRD_MUX_CLK(pwm1_clk, "pwm1-clk", pwm_parents, 0x6c,
		    0, 3, ORCA_MUX_FLAG);
static SPRD_MUX_CLK(pwm2_clk, "pwm2-clk", pwm_parents, 0x70,
		    0, 3, ORCA_MUX_FLAG);
static SPRD_MUX_CLK(pwm3_clk, "pwm3-clk", pwm_parents, 0x74,
		    0, 3, ORCA_MUX_FLAG);

static SPRD_GATE_CLK(usb0_pipe_clk, "usb0-pipe-clk", "ext-26m", 0x78,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(usb0_utmi_clk, "usb0-utmi-clk", "ext-26m", 0x7c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(usb1_pipe_clk, "usb1-pipe-clk", "ext-26m", 0x80,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(usb1_utmi_clk, "usb1-utmi-clk", "ext-26m", 0x84,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(pcie_pipe_clk, "pcie-pipe-clk", "ext-26m", 0x88,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_ap_clks[] = {
	/* address base is 0x21100000 */
	&ap_axi_clk.common,
	&peri_apb_clk.common,
	&nandc_ecc_clk.common,
	&usb0_ref_clk.common,
	&usb1_ref_clk.common,
	&pcie_aux_clk.common,
	&ap_uart0_clk.common,
	&ap_i2c0_clk.common,
	&ap_i2c1_clk.common,
	&ap_i2c2_clk.common,
	&ap_i2c3_clk.common,
	&ap_i2c4_clk.common,
	&ap_sim_clk.common,
	&pwm0_clk.common,
	&pwm1_clk.common,
	&pwm2_clk.common,
	&pwm3_clk.common,
	&usb0_pipe_clk.common,
	&usb0_utmi_clk.common,
	&usb1_pipe_clk.common,
	&usb1_utmi_clk.common,
	&pcie_pipe_clk.common,
};

static struct clk_hw_onecell_data orca_ap_clk_hws = {
	.hws	= {
		[CLK_AP_AXI] = &ap_axi_clk.common.hw,
		[CLK_PERI_APB] = &peri_apb_clk.common.hw,
		[CLK_NANDC_ECC] = &nandc_ecc_clk.common.hw,
		[CLK_USB0_REF] = &usb0_ref_clk.common.hw,
		[CLK_USB1_REF] = &usb1_ref_clk.common.hw,
		[CLK_PCIE_AUX] = &pcie_aux_clk.common.hw,
		[CLK_AP_UART0] = &ap_uart0_clk.common.hw,
		[CLK_AP_I2C0] = &ap_i2c0_clk.common.hw,
		[CLK_AP_I2C1] = &ap_i2c1_clk.common.hw,
		[CLK_AP_I2C2] = &ap_i2c2_clk.common.hw,
		[CLK_AP_I2C3] = &ap_i2c3_clk.common.hw,
		[CLK_AP_I2C4] = &ap_i2c4_clk.common.hw,
		[CLK_AP_SIM] = &ap_sim_clk.common.hw,
		[CLK_PWM0] = &pwm0_clk.common.hw,
		[CLK_PWM1] = &pwm1_clk.common.hw,
		[CLK_PWM2] = &pwm2_clk.common.hw,
		[CLK_PWM3] = &pwm3_clk.common.hw,
		[CLK_USB0_PIPE] = &usb0_pipe_clk.common.hw,
		[CLK_USB0_UTMI] = &usb0_utmi_clk.common.hw,
		[CLK_USB1_PIPE] = &usb1_pipe_clk.common.hw,
		[CLK_USB1_UTMI] = &usb1_utmi_clk.common.hw,
		[CLK_PCIE_PIPE] = &pcie_pipe_clk.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static const struct sprd_clk_desc orca_ap_clk_desc = {
	.clk_clks	= orca_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(orca_ap_clks),
	.hw_clks	= &orca_ap_clk_hws,
};

/* ap apb gates */
static SPRD_SC_GATE_CLK(apapb_reg_eb, "apapb-reg-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_uart0_eb, "ap-uart0-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c0_eb, "ap-i2c0-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c1_eb, "ap-i2c1-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c2_eb, "ap-i2c2-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c3_eb2, "ap-i2c3-eb2", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c4_eb, "ap-i2c4-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_apb_spi0_eb, "ap-apb-spi0-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_lf_in_eb, "spi0-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_apb_spi1_eb, "ap-apb-spi1-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_lf_in_eb, "spi1-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_apb_spi2_eb, "ap-apb-spi2-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_lf_in_eb, "spi2-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm0_eb, "pwm0-eb", "ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm1_eb, "pwm1-eb", "ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm2_eb, "pwm2-eb", "ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm3_eb, "pwm3-eb", "ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_eb, "sim0-eb", "ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_32k_eb, "sim0-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_apapb_gate[] = {
	/* address base is 0x24000000 */
	&apapb_reg_eb.common,
	&ap_uart0_eb.common,
	&ap_i2c0_eb.common,
	&ap_i2c1_eb.common,
	&ap_i2c2_eb.common,
	&ap_i2c3_eb2.common,
	&ap_i2c4_eb.common,
	&ap_apb_spi0_eb.common,
	&spi0_lf_in_eb.common,
	&ap_apb_spi1_eb.common,
	&spi1_lf_in_eb.common,
	&ap_apb_spi2_eb.common,
	&spi2_lf_in_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&sim0_eb.common,
	&sim0_32k_eb.common,
};

static struct clk_hw_onecell_data orca_apapb_gate_hws = {
	.hws	= {
		[CLK_APAPB_REG_EB] = &apapb_reg_eb.common.hw,
		[CLK_AP_UART0_EB] = &ap_uart0_eb.common.hw,
		[CLK_AP_I2C0_EB] = &ap_i2c0_eb.common.hw,
		[CLK_AP_I2C1_EB] = &ap_i2c1_eb.common.hw,
		[CLK_AP_I2C2_EB] = &ap_i2c2_eb.common.hw,
		[CLK_AP_I2C3_EB] = &ap_i2c3_eb2.common.hw,
		[CLK_AP_I2C4_EB] = &ap_i2c4_eb.common.hw,
		[CLK_AP_APB_SPI0_EB] = &ap_apb_spi0_eb.common.hw,
		[CLK_SPI0_LF_IN_EB] = &spi0_lf_in_eb.common.hw,
		[CLK_AP_APB_SPI1_EB] = &ap_apb_spi1_eb.common.hw,
		[CLK_SPI1_IF_IN_EB] = &spi1_lf_in_eb.common.hw,
		[CLK_AP_APB_SPI2_EB] = &ap_apb_spi2_eb.common.hw,
		[CLK_SPI2_IF_IN_EB] = &spi2_lf_in_eb.common.hw,
		[CLK_PWM0_EB] = &pwm0_eb.common.hw,
		[CLK_PWM1_EB] = &pwm1_eb.common.hw,
		[CLK_PWM2_EB] = &pwm2_eb.common.hw,
		[CLK_PWM3_EB] = &pwm3_eb.common.hw,
		[CLK_SIM0_EB] = &sim0_eb.common.hw,
		[CLK_SIM0_32K_EB] = &sim0_32k_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static const struct sprd_clk_desc orca_apapb_gate_desc = {
	.clk_clks	= orca_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(orca_apapb_gate),
	.hw_clks	= &orca_apapb_gate_hws,
};

/* ap ipa gate */
static SPRD_SC_GATE_CLK(ipa_usb1_eb, "ipa-usb1-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb1_suspend_eb, "usb1-suspend-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_usb1_ref_eb, "ipa-usb1-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio_slv_eb, "sdio-slv-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd_slv_frun_eb, "sd-slv-frun-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pcie_eb, "pcie-eb", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pcie_aux_eb, "pcie-aux-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_eb, "ipa-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb_pam_eb, "usb-pam-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pcie_sel, "pcie-sel", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_ipa_gate_clks[] = {
	/* address base is 0x29000000 */
	&ipa_usb1_eb.common,
	&usb1_suspend_eb.common,
	&ipa_usb1_ref_eb.common,
	&sdio_slv_eb.common,
	&sd_slv_frun_eb.common,
	&pcie_eb.common,
	&pcie_aux_eb.common,
	&ipa_eb.common,
	&usb_pam_eb.common,
	&pcie_sel.common,
};

static struct clk_hw_onecell_data orca_ipa_gate_clk_hws = {
	.hws	= {
		[CLK_IPA_USB1_EB] = &ipa_usb1_eb.common.hw,
		[CLK_USB1_SUSPEND_EB] = &usb1_suspend_eb.common.hw,
		[CLK_IPA_USB1_REF_EB] = &ipa_usb1_ref_eb.common.hw,
		[CLK_SDIO_SLV_EB] = &sdio_slv_eb.common.hw,
		[CLK_SD_SLV_FRUN_EB] = &sd_slv_frun_eb.common.hw,
		[CLK_PCIE_EB] = &pcie_eb.common.hw,
		[CLK_PCIE_AUX_EB] = &pcie_aux_eb.common.hw,
		[CLK_IPA_EB] = &ipa_eb.common.hw,
		[CLK_USB_PAM_EB] = &usb_pam_eb.common.hw,
		[CLK_PCIE_SEL] = &pcie_sel.common.hw,
	},
	.num	= CLK_IPA_GATE_NUM,
};

static struct sprd_clk_desc orca_ipa_gate_desc = {
	.clk_clks	= orca_ipa_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(orca_ipa_gate_clks),
	.hw_clks	= &orca_ipa_gate_clk_hws,
};

/* aon clocks */
static CLK_FIXED_FACTOR(clk_13m,	"clk-13m",	"ext-26m",
			2, 1, 0);
static CLK_FIXED_FACTOR(clk_6m5,	"clk-6m5",	"ext-26m",
			4, 1, 0);
static CLK_FIXED_FACTOR(clk_4m,		"clk-4m",	"ext-26m",
			6, 1, 0);
static CLK_FIXED_FACTOR(clk_2m,		"clk-2m",	"ext-26m",
			13, 1, 0);
static CLK_FIXED_FACTOR(clk_1m,		"clk-1m",	"ext-26m",
			26, 1, 0);
static CLK_FIXED_FACTOR(clk_250k,	"clk-250k",	"ext-26m",
			104, 1, 0);

static CLK_FIXED_FACTOR(rco_25m,	"rco-25m",	"rco-100m",
			4, 1, 0);
static CLK_FIXED_FACTOR(rco_20m,	"rco-20m",	"rco-100m",
			4, 1, 0);
static CLK_FIXED_FACTOR(rco_4m,		"rco-4m",	"rco-100m",
			25, 1, 0);
static CLK_FIXED_FACTOR(rco_2m,		"rco-2m",	"rco-100m",
			50, 1, 0);

static const char * const aon_apb_parents[] = { "rco-4m", "clk-4m",
						"clk-13m", "rco-25m",
						"ext-26m", "v3pll-96m",
						"rco-100m", "v3pll-128m" };
static SPRD_COMP_CLK(aon_apb_clk, "aon-apb-clk", aon_apb_parents, 0x220,
		     0, 3, 8, 2, 0);

static const char * const adi_parents[] = { "rco-4m", "ext-26m",
					    "rco-25m", "v3pll-38m4",
					    "v3pll-51m2" };
static SPRD_MUX_CLK(adi_clk, "adi-clk", adi_parents, 0x228,
			0, 3, ORCA_MUX_FLAG);

static const char * const aon_uart_parents[] = { "rco-4m", "ext-26m",
						 "v3pll-48m", "v3pll-51m2",
						 "v3pll-96m", "rco-100m",
						 "v3pll-128m" };
static SPRD_MUX_CLK(aon_uart0_clk, "aon-uart0-clk", aon_uart_parents, 0x22c,
		    0, 3, ORCA_MUX_FLAG);

static const char * const aon_i2c_parents[] = { "rco-4m", "ext-26m",
						"v3pll-48m", "v3pll-51m2",
						"rco-100m", "v3pll-153m6" };
static SPRD_MUX_CLK(aon_i2c_clk, "aon-i2c-clk", aon_i2c_parents, 0x230,
		    0, 3, ORCA_MUX_FLAG);

static const char * const efuse_parents[] = { "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(efuse_clk, "efuse-clk", efuse_parents, 0x234,
		    0, 1, ORCA_MUX_FLAG);

static const char * const tmr_parents[] = { "rco-4m", "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(tmr_clk, "tmr-clk", tmr_parents, 0x238,
		    0, 2, ORCA_MUX_FLAG);

static const char * const thm_parents[] = { "ext-32k", "clk-250k" };
static SPRD_MUX_CLK(thm0_clk, "thm0-clk", thm_parents, 0x23c,
		    0, 1, ORCA_MUX_FLAG);
static SPRD_MUX_CLK(thm1_clk, "thm1-clk", thm_parents, 0x240,
		    0, 1, ORCA_MUX_FLAG);
static SPRD_MUX_CLK(thm2_clk, "thm2-clk", thm_parents, 0x244,
		    0, 1, ORCA_MUX_FLAG);

static const char * const pmu_parents[] = { "ext-32k", "rco-4m", "clk-4m" };
static SPRD_MUX_CLK(pmu_clk, "pmu-clk", pmu_parents, 0x250,
		    0, 2, ORCA_MUX_FLAG);

static const char * const apcpu_pmu_parents[] = { "ext-26m", "v3pll-96m",
						  "rco-100m", "v3pll-128m" };
static SPRD_MUX_CLK(apcpu_pmu_clk, "apcpu-pmu-clk", pmu_parents, 0x254,
		    0, 2, ORCA_MUX_FLAG);

static const char * const aux_parents[] = { "ext-32k", "ext-26m" };
static SPRD_COMP_CLK(aux0_clk, "aux0-clk", aux_parents, 0x260,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK(aux1_clk, "aux1-clk", aux_parents, 0x264,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK(aux2_clk, "aux2-clk", aux_parents, 0x268,
		     0, 1, 8, 4, 0);
static SPRD_COMP_CLK(probe_clk, "probe-clk", aux_parents, 0x26c,
		     0, 1, 8, 4, 0);

static const char * const apcpu_dap_parents[] = { "ext-26m", "rco-4m",
						  "v3pll-76m8", "rco-100m",
						  "v3pll-128m", "v3pll-153m6" };
static SPRD_MUX_CLK(apcpu_dap_clk, "apcpu-dap-clk", thm_parents, 0x27c,
		    0, 3, ORCA_MUX_FLAG);

static SPRD_GATE_CLK(apcpu_dap_mtck, "apcpu-dap-mtck", "ext-26m", 0x280,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const debug_ts_parents[] = { "ext-26m", "v3pll-76m8",
						 "v3pll-128m", "v3pll-192m" };
static SPRD_MUX_CLK(debug_ts_clk, "debug-ts-clk", debug_ts_parents, 0x288,
		    0, 2, ORCA_MUX_FLAG);

static SPRD_GATE_CLK(dsi0_test_clk, "dsi0-test-clk", "ext-26m", 0x28c,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(dsi1_test_clk, "dsi1-test-clk", "ext-26m", 0x290,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_GATE_CLK(dsi2_test_clk, "dsi2-test-clk", "ext-26m", 0x294,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const djtag_tck_parents[] = { "rco-4m", "ext-26m" };
static SPRD_MUX_CLK(djtag_tck_clk, "djtag-tck-clk", djtag_tck_parents, 0x2a4,
		    0, 1, ORCA_MUX_FLAG);

static SPRD_GATE_CLK(djtag_tck_hw, "djtag-tck-hw", "ext-26m", 0x2a8,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static const char * const debounce_parents[] = { "ext-32k", "rco-4m",
						 "rco-25m", "ext-26m" };
static SPRD_MUX_CLK(debounce_clk, "debounce-clk", debounce_parents, 0x2b0,
		    0, 2, ORCA_MUX_FLAG);

static const char * const scc_parents[] = { "ext-26m", "v3pll-48m",
					    "v3pll-51m2", "v3pll-96m" };
static SPRD_MUX_CLK(scc_clk, "scc-clk", scc_parents, 0x2b8,
		    0, 2, ORCA_MUX_FLAG);

static const char * const top_dvfs_parents[] = { "ext-26m", "v3pll-96m",
						 "rco-100m", "v3pll-128m" };
static SPRD_MUX_CLK(top_dvfs_clk, "top-dvfs-clk", top_dvfs_parents, 0x2bc,
		    0, 2, ORCA_MUX_FLAG);

static const char * const sdio_2x_parents[] = { "clk-1m", "ext-26m",
					"v3pll-307m2", "v3rpll" };
static SPRD_COMP_CLK(sdio2_2x_clk, "sdio2-2x-clk", sdio_2x_parents, 0x2c4,
		     0, 2, 16, 11, 0);
static SPRD_DIV_CLK(sdio2_1x_clk, "sdio2-1x-clk", "sdio2-2x-clk", 0x2c8,
		    8, 1, 0);

static const char * const cssys_parents[] = { "rco-25m", "ext-26m",
					      "rco-100m", "v3pll-256m",
					      "v3pll-307m2", "v3pll-384m" };
static SPRD_COMP_CLK(cssys_clk,	"cssys-clk",	cssys_parents, 0x2cc,
		     0, 3, 8, 2, 0);
static SPRD_DIV_CLK(cssys_apb_clk, "cssys-apb-clk", "cssys-clk", 0x2d0,
		    8, 2, 0);

static const char * const apcpu_axi_parents[] = { "ext-26m", "v3pll-76m8",
						  "v3pll-128m", "v3pll-256m" };
static SPRD_MUX_CLK(apcpu_axi_clk, "apcpu-axi-clk", apcpu_axi_parents, 0x2d4,
		    0, 2, ORCA_MUX_FLAG);

static SPRD_COMP_CLK(sdio1_2x_clk, "sdio1-2x-clk", sdio_2x_parents, 0x2d8,
		     0, 2, 16, 11, 0);
static SPRD_DIV_CLK(sdio1_1x_clk, "sdio1-1x-clk", "sdio1-2x-clk", 0x2dc,
		    8, 1, 0);

static SPRD_GATE_CLK(sdio0_slv_clk, "sdio0-slv-clk", "ext-26m", 0x2e0,
		     BIT(16), CLK_IGNORE_UNUSED, 0);

static SPRD_COMP_CLK(emmc_2x_clk, "emmc-2x-clk", sdio_2x_parents, 0x2e4,
		     0, 2, 16, 11, 0);
static SPRD_DIV_CLK(emmc_1x_clk, "emmc-1x-clk", "emmc-2x-clk", 0x2e8,
		    8, 1, 0);

static const char * const nandc_2x_parents[] = { "ext-26m", "v3pll-153m6",
						 "v3pll-170m6", "v3rpll-195m",
						 "v3pll-256m", "v3pll-307m2",
						 "v3rpll" };
static SPRD_COMP_CLK(nandc_2x_clk, "nandc-2x-clk", nandc_2x_parents, 0x2ec,
		     0, 2, 8, 11, 0);
static SPRD_DIV_CLK(nandc_1x_clk, "nandc-1x-clk", "nandc-2x-clk", 0x2f0,
		    8, 1, 0);

static const char * const ap_spi_parents[] = { "ext-26m", "v3pll-128m",
					       "v3pll-153m6", "v3pll-192m" };
static SPRD_COMP_CLK(ap_spi0_clk, "ap-spi0-clk", ap_spi_parents, 0x2f4,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi1_clk, "ap-spi1-clk", ap_spi_parents, 0x2f8,
		     0, 2, 8, 3, 0);
static SPRD_COMP_CLK(ap_spi2_clk, "ap-spi2-clk", ap_spi_parents, 0x2fc,
		     0, 2, 8, 3, 0);

static const char * const otg2_ref_parents[] = { "clk-12m", "ext-26m" };
static SPRD_MUX_CLK(otg2a_ref_clk, "otg2a-ref-clk", otg2_ref_parents, 0x300,
		    0, 1, ORCA_MUX_FLAG);

static const char * const usb3_ref_parents[] = { "ext-32k", "clk-1m" };
static SPRD_MUX_CLK(u3a_suspend_ref, "u3a-suspend-ref", usb3_ref_parents, 0x308,
		    0, 1, ORCA_MUX_FLAG);

static SPRD_MUX_CLK(otg2b_ref_clk, "otg2b-ref-clk", otg2_ref_parents, 0x30c,
		    0, 1, ORCA_MUX_FLAG);

static SPRD_MUX_CLK(u3b_suspend_ref, "u3b-suspend-ref", usb3_ref_parents, 0x308,
		    0, 1, ORCA_MUX_FLAG);

static const char * const analog_io_parents[] = { "ext-26m", "v3pll-48m" };
static SPRD_COMP_CLK(analog_io_clk, "analog-io-clk", analog_io_parents, 0x328,
		     0, 1, 8, 2, 0);

static const char * const dmc_ref_parents[] = { "clk-6m5", "clk-13m",
						"ext-26m" };
static SPRD_MUX_CLK(dmc_ref_clk, "dmc-ref-clk", dmc_ref_parents, 0x32c,
		    0, 2, ORCA_MUX_FLAG);

static const char * const emc_parents[] = { "ext-26m", "v3pll-384m",
					    "v3pll-512m", "v3pll-768m",
					    "v3pll" };
static SPRD_MUX_CLK(emc_clk, "emc-clk", emc_parents, 0x330,
		    0, 3, ORCA_MUX_FLAG);

static const char * const sc_cc_parents[] = { "ext-26m", "rco-25m",
					      "v3pll-96m" };
static SPRD_MUX_CLK(sc_cc_clk, "sc-cc-clk", sc_cc_parents, 0x398,
		    0, 2, ORCA_MUX_FLAG);

static const char * const pmu_26m_parents[] = { "rco-20m", "ext-26m" };
static SPRD_MUX_CLK(pmu_26m_clk, "pmu-26m-clk", pmu_26m_parents, 0x39c,
		    0, 1, ORCA_MUX_FLAG);

static struct sprd_clk_common *orca_aon_clks[] = {
	/* address base is 0x63170000 */
	&aon_apb_clk.common,
	&adi_clk.common,
	&aon_uart0_clk.common,
	&aon_i2c_clk.common,
	&efuse_clk.common,
	&tmr_clk.common,
	&thm0_clk.common,
	&thm1_clk.common,
	&thm2_clk.common,
	&pmu_clk.common,
	&apcpu_pmu_clk.common,
	&aux0_clk.common,
	&aux1_clk.common,
	&aux2_clk.common,
	&probe_clk.common,
	&apcpu_dap_clk.common,
	&apcpu_dap_mtck.common,
	&debug_ts_clk.common,
	&dsi0_test_clk.common,
	&dsi1_test_clk.common,
	&dsi2_test_clk.common,
	&djtag_tck_clk.common,
	&djtag_tck_hw.common,
	&debounce_clk.common,
	&scc_clk.common,
	&top_dvfs_clk.common,
	&sdio2_2x_clk.common,
	&sdio2_1x_clk.common,
	&cssys_clk.common,
	&cssys_apb_clk.common,
	&apcpu_axi_clk.common,
	&sdio1_2x_clk.common,
	&sdio1_1x_clk.common,
	&sdio0_slv_clk.common,
	&emmc_2x_clk.common,
	&emmc_1x_clk.common,
	&nandc_2x_clk.common,
	&nandc_1x_clk.common,
	&ap_spi0_clk.common,
	&ap_spi1_clk.common,
	&ap_spi2_clk.common,
	&otg2a_ref_clk.common,
	&u3a_suspend_ref.common,
	&otg2b_ref_clk.common,
	&u3b_suspend_ref.common,
	&analog_io_clk.common,
	&dmc_ref_clk.common,
	&emc_clk.common,
	&sc_cc_clk.common,
	&pmu_26m_clk.common,
};

static struct clk_hw_onecell_data orca_aon_clk_hws = {
	.hws	= {
		[CLK_13M]  = &clk_13m.hw,
		[CLK_6M5]  = &clk_6m5.hw,
		[CLK_4M]   = &clk_4m.hw,
		[CLK_2M]   = &clk_2m.hw,
		[CLK_1M]   = &clk_1m.hw,
		[CLK_250K] = &clk_250k.hw,
		[CLK_RCO25M] = &rco_25m.hw,
		[CLK_RCO20M] = &rco_20m.hw,
		[CLK_RCO4M] = &rco_4m.hw,
		[CLK_RCO2M] = &rco_2m.hw,
		[CLK_AON_APB] = &aon_apb_clk.common.hw,
		[CLK_ADI] = &adi_clk.common.hw,
		[CLK_AON_UART0] = &aon_uart0_clk.common.hw,
		[CLK_AON_I2C] = &aon_i2c_clk.common.hw,
		[CLK_EFUSE] = &efuse_clk.common.hw,
		[CLK_TMR] = &tmr_clk.common.hw,
		[CLK_THM0] = &thm0_clk.common.hw,
		[CLK_THM1] = &thm1_clk.common.hw,
		[CLK_THM2] = &thm2_clk.common.hw,
		[CLK_PMU] = &pmu_clk.common.hw,
		[CLK_APCPU_PMU] = &apcpu_pmu_clk.common.hw,
		[CLK_AUX0] = &aux0_clk.common.hw,
		[CLK_AUX1] = &aux1_clk.common.hw,
		[CLK_AUX2] = &aux2_clk.common.hw,
		[CLK_PROBE] = &probe_clk.common.hw,
		[CLK_APCPU_DAP] = &apcpu_dap_clk.common.hw,
		[CLK_APCPU_DAP_MTCK] = &apcpu_dap_mtck.common.hw,
		[CLK_DEBUG_TS] = &debug_ts_clk.common.hw,
		[CLK_DSI0_TEST] = &dsi0_test_clk.common.hw,
		[CLK_DSI1_TEST] = &dsi1_test_clk.common.hw,
		[CLK_DSI2_TEST] = &dsi2_test_clk.common.hw,
		[CLK_DJTAG_TCK] = &djtag_tck_clk.common.hw,
		[CLK_DJTAG_TCK_HW] = &djtag_tck_hw.common.hw,
		[CLK_DEBOUNCE] = &debounce_clk.common.hw,
		[CLK_SCC] = &scc_clk.common.hw,
		[CLK_TOP_DVFS] = &top_dvfs_clk.common.hw,
		[CLK_SDIO2_2X] = &sdio2_2x_clk.common.hw,
		[CLK_SDIO2_1X] = &sdio2_1x_clk.common.hw,
		[CLK_CSSYS] = &cssys_clk.common.hw,
		[CLK_CSSYS_APB] = &cssys_apb_clk.common.hw,
		[CLK_APCPU_AXI] = &apcpu_axi_clk.common.hw,
		[CLK_SDIO1_2X] = &sdio1_2x_clk.common.hw,
		[CLK_SDIO1_1X] = &sdio1_1x_clk.common.hw,
		[CLK_SDIO0_SLV] = &sdio0_slv_clk.common.hw,
		[CLK_EMMC_2X] = &emmc_2x_clk.common.hw,
		[CLK_EMMC_1X] = &emmc_1x_clk.common.hw,
		[CLK_NANDC_2X] = &nandc_2x_clk.common.hw,
		[CLK_NANDC_1X] = &nandc_1x_clk.common.hw,
		[CLK_AP_SPI0] = &ap_spi0_clk.common.hw,
		[CLK_AP_SPI1] = &ap_spi1_clk.common.hw,
		[CLK_AP_SPI2] = &ap_spi2_clk.common.hw,
		[CLK_OTG2A_REF] = &otg2a_ref_clk.common.hw,
		[CLK_U3A_SUSPEND_REF] = &u3a_suspend_ref.common.hw,
		[CLK_OTG2B_REF] = &otg2b_ref_clk.common.hw,
		[CLK_U3B_SUSPEND_REF] = &u3b_suspend_ref.common.hw,
		[CLK_ANALOG_IO] = &analog_io_clk.common.hw,
		[CLK_DMC_REF] = &dmc_ref_clk.common.hw,
		[CLK_EMC] = &emc_clk.common.hw,
		[CLK_SC_CC] = &sc_cc_clk.common.hw,
		[CLK_PMU_26M] = &pmu_26m_clk.common.hw,
	},
	.num	= CLK_AON_CLK_NUM,
};

static const struct sprd_clk_desc orca_aon_clk_desc = {
	.clk_clks	= orca_aon_clks,
	.num_clk_clks	= ARRAY_SIZE(orca_aon_clks),
	.hw_clks	= &orca_aon_clk_hws,
};

/* aon gates */
static SPRD_SC_GATE_CLK(rc100_cal_eb, "rc100-cal-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_spi_eb, "aon-spi-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_tck_eb, "djtag-tck-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb, "djtag-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb, "aux0-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux1_eb, "aux1-eb", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux2_eb, "aux2-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(probe_eb, "probe-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bsm_tmr_eb, "bsm-tmr-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_bm_eb, "aon-apb-bm-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_apb_bm_eb, "pmu-apb-bm-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_cssys_eb, "apcpu-cssys-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debug_filter_eb, "debug-filter-eb", "ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_dap_eb, "apcpu-dap-eb", "ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_eb, "cssys-eb", "ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_apb_eb, "cssys-apb-eb", "ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_pub_eb, "cssys-pub-eb", "ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd0_cfg_eb, "sd0-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd0_ref_eb, "sd0-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd1_cfg_eb, "sd1-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd1_ref_eb, "sd1-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd2_cfg_eb, "sd2-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd2_ref_eb, "sd2-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes0_eb, "serdes0-eb", "ext-26m", 0x0,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes1_eb, "serdes1-eb", "ext-26m", 0x0,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes2_eb, "serdes2-eb", "ext-26m", 0x0,
		     0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtm_eb, "rtm-eb", "ext-26m", 0x0,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtm_atb_eb, "rtm-atb-eb", "ext-26m", 0x0,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_nr_spi_eb, "aon-nr-spi-eb", "ext-26m", 0x0,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_bm_s5_eb, "aon-bm-s5-eb", "ext-26m", 0x0,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(efuse_eb, "efuse-eb", "ext-26m", 0x4,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpio_eb, "gpio-eb", "ext-26m", 0x4,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb, "mbox-eb", "ext-26m", 0x4,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_eb, "kpd-eb", "ext-26m", 0x4,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_eb, "aon-syst-eb", "ext-26m", 0x4,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_eb, "ap-syst-eb", "ext-26m", 0x4,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_eb, "aon-tmr-eb", "ext-26m", 0x4,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dvfs_top_eb, "dvfs-top-eb", "ext-26m", 0x4,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_clk_eb, "apcpu-clk-rf-eb", "ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(splk_eb, "splk-eb", "ext-26m", 0x4,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_eb, "pin-eb", "ext-26m", 0x4,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ana_eb, "ana-eb", "ext-26m", 0x4,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_ckg_eb, "aon-ckg-eb", "ext-26m", 0x4,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_ctrl_eb, "djtag-ctrl-eb", "ext-26m", 0x4,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_ts0_eb, "apcpu-ts0-eb", "ext-26m", 0x4,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nic400_aon_eb, "nic400-aon-eb", "ext-26m", 0x4,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(scc_eb, "scc-eb", "ext-26m", 0x4,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_spi0_eb, "ap-spi0-eb", "ext-26m", 0x4,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_spi1_eb, "ap-spi1-eb", "ext-26m", 0x4,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_spi2_eb, "ap-spi2-eb", "ext-26m", 0x4,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_bm_s3_eb, "aon-bm-s3-eb", "ext-26m", 0x4,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sc_cc_eb, "sc-cc-eb", "ext-26m", 0x4,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(thm0_eb, "thm0-eb", "ext-26m", 0x8,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm1_eb, "thm1-eb", "ext-26m", 0x8,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_sim_eb, "ap-sim-eb", "ext-26m", 0x8,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_i2c_eb, "aon-i2c-eb", "ext-26m", 0x8,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_eb, "pmu-eb", "ext-26m", 0x8,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(adi_eb, "adi-eb", "ext-26m", 0x8,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_eb, "eic-eb", "ext-26m", 0x8,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc0_eb, "ap-intc0-eb", "ext-26m", 0x8,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc1_eb, "ap-intc1-eb", "ext-26m", 0x8,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc2_eb, "ap-intc2-eb", "ext-26m", 0x8,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc3_eb, "ap-intc3-eb", "ext-26m", 0x8,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc4_eb, "ap-intc4-eb", "ext-26m", 0x8,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc5_eb, "ap-intc5-eb", "ext-26m", 0x8,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_intc_eb, "audcp-intc-eb", "ext-26m", 0x8,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_eb, "ap-tmr0-eb", "ext-26m", 0x8,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb, "ap-tmr1-eb", "ext-26m", 0x8,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb, "ap-tmr2-eb", "ext-26m", 0x8,
		     0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb, "ap-wdg-eb", "ext-26m", 0x8,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_wdg_eb, "apcpu-wdg-eb", "ext-26m", 0x8,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm2_eb, "thm2-eb", "ext-26m", 0x8,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(arch_rtc_eb, "arch-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_rtc_eb, "kpd-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_rtc_eb, "ap-syst-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_rtc_eb, "aon-tmr-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtc_eb, "eic-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtcdv5_eb, "eic-rtcdv5-eb", "ext-26m", 0xc,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_rtc_eb, "ap-wdg-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ac_wdg_rtc_eb, "ac-wdg-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_rtc_eb, "ap-tmr0-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_rtc_eb, "ap-tmr2-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcxo_lc_rtc_eb, "dcxo-lc-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bb_cal_rtc_eb, "bb-cal-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(dsi0_test_eb, "dsi0-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi1_test_eb, "dsi1-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi2_test_eb, "dsi2-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_ref_en, "dmc-ref-eb", "ext-26m", 0x20,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tsen_en, "tsen-en", "ext-26m", 0x20,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tmr_en, "tmr-en", "ext-26m", 0x20,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100_ref_en, "rc100-ref-en", "ext-26m", 0x20,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100_fdk_en, "rc100-fdk-en", "ext-26m", 0x20,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debounce_en, "debounce-en", "ext-26m", 0x20,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(det_32k_eb, "det-32k-eb", "ext-26m", 0x20,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(cssys_en, "cssys-en", "ext-26m", 0x24,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_2x_en, "sdio0-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_1x_en, "sdio0-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_2x_en, "sdio1-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_1x_en, "sdio1-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_2x_en, "sdio2-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_1x_en, "sdio2-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_1x_en, "emmc-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_2x_en, "emmc-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_1x_en, "nandc-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_2x_en, "nandc-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(all_pll_test_eb, "all-pll-test-eb", "ext-26m", 0x24,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aapc_test_eb, "aapc-test-eb", "ext-26m", 0x24,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debug_ts_eb, "debug-ts-eb", "ext-26m", 0x24,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(u2_0_ref_en, "u2-0-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(u2_1_ref_en, "u2-1-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(u3_0_ref_en, "u3-0-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(u3_0_suspend_en, "u3-0-suspend-en", "ext-26m", 0x564,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(u3_1_ref_en, "u3-1-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(u3_1_suspend_en, "u3-1-suspend-en", "ext-26m", 0x564,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi0_ref_en, "dsi0-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi1_ref_en, "dsi1-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi2_ref_en, "dsi2-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pcie_ref_en, "pcie-ref-en", "ext-26m", 0x564,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_aon_gate[] = {
	/* address base is 0x64020000 */
	&rc100_cal_eb.common,
	&aon_spi_eb.common,
	&djtag_tck_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&bsm_tmr_eb.common,
	&aon_apb_bm_eb.common,
	&pmu_apb_bm_eb.common,
	&apcpu_cssys_eb.common,
	&debug_filter_eb.common,
	&apcpu_dap_eb.common,
	&cssys_eb.common,
	&cssys_apb_eb.common,
	&cssys_pub_eb.common,
	&sd0_cfg_eb.common,
	&sd0_ref_eb.common,
	&sd1_cfg_eb.common,
	&sd1_ref_eb.common,
	&sd2_cfg_eb.common,
	&sd2_ref_eb.common,
	&serdes0_eb.common,
	&serdes1_eb.common,
	&serdes2_eb.common,
	&rtm_eb.common,
	&rtm_atb_eb.common,
	&aon_nr_spi_eb.common,
	&aon_bm_s5_eb.common,
	&efuse_eb.common,
	&gpio_eb.common,
	&mbox_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&dvfs_top_eb.common,
	&apcpu_clk_eb.common,
	&splk_eb.common,
	&pin_eb.common,
	&ana_eb.common,
	&aon_ckg_eb.common,
	&djtag_ctrl_eb.common,
	&apcpu_ts0_eb.common,
	&nic400_aon_eb.common,
	&scc_eb.common,
	&ap_spi0_eb.common,
	&ap_spi1_eb.common,
	&ap_spi2_eb.common,
	&aon_bm_s3_eb.common,
	&sc_cc_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&ap_sim_eb.common,
	&aon_i2c_eb.common,
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
	&ap_wdg_eb.common,
	&apcpu_wdg_eb.common,
	&thm2_eb.common,
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
	&dsi0_test_eb.common,
	&dsi1_test_eb.common,
	&dsi2_test_eb.common,
	&dmc_ref_en.common,
	&tsen_en.common,
	&tmr_en.common,
	&rc100_ref_en.common,
	&rc100_fdk_en.common,
	&debounce_en.common,
	&det_32k_eb.common,
	&cssys_en.common,
	&sdio0_2x_en.common,
	&sdio0_1x_en.common,
	&sdio1_2x_en.common,
	&sdio1_1x_en.common,
	&sdio2_2x_en.common,
	&sdio2_1x_en.common,
	&emmc_1x_en.common,
	&emmc_2x_en.common,
	&nandc_1x_en.common,
	&nandc_2x_en.common,
	&all_pll_test_eb.common,
	&aapc_test_eb.common,
	&debug_ts_eb.common,
	&u2_0_ref_en.common,
	&u2_1_ref_en.common,
	&u3_0_ref_en.common,
	&u3_0_suspend_en.common,
	&u3_1_ref_en.common,
	&u3_1_suspend_en.common,
	&dsi0_ref_en.common,
	&dsi1_ref_en.common,
	&dsi2_ref_en.common,
	&pcie_ref_en.common,
};

static struct clk_hw_onecell_data orca_aon_gate_hws = {
	.hws	= {
		[CLK_RC100_CAL_EB] = &rc100_cal_eb.common.hw,
		[CLK_AON_SPI_EB] = &aon_spi_eb.common.hw,
		[CLK_DJTAG_TCK_EB] = &djtag_tck_eb.common.hw,
		[CLK_DJTAG_EB] = &djtag_eb.common.hw,
		[CLK_AUX0_EB] = &aux0_eb.common.hw,
		[CLK_AUX1_EB] = &aux1_eb.common.hw,
		[CLK_AUX2_EB] = &aux2_eb.common.hw,
		[CLK_PROBE_EB] = &probe_eb.common.hw,
		[CLK_BSM_TMR_EB] = &bsm_tmr_eb.common.hw,
		[CLK_AON_APB_BM_EB] = &aon_apb_bm_eb.common.hw,
		[CLK_PMU_APB_BM_EB] = &pmu_apb_bm_eb.common.hw,
		[CLK_APCPU_CSSYS_EB] = &apcpu_cssys_eb.common.hw,
		[CLK_DEBUG_FILTER_EB] = &debug_filter_eb.common.hw,
		[CLK_APCPU_DAP_EB] = &apcpu_dap_eb.common.hw,
		[CLK_CSSYS_EB] = &cssys_eb.common.hw,
		[CLK_CSSYS_APB_EB] = &cssys_apb_eb.common.hw,
		[CLK_CSSYS_PUB_EB] = &cssys_pub_eb.common.hw,
		[CLK_SD0_CFG_EB] = &sd0_cfg_eb.common.hw,
		[CLK_SD0_REF_EB] = &sd0_ref_eb.common.hw,
		[CLK_SD1_CFG_EB] = &sd1_cfg_eb.common.hw,
		[CLK_SD1_REF_EB] = &sd1_ref_eb.common.hw,
		[CLK_SD2_CFG_EB] = &sd2_cfg_eb.common.hw,
		[CLK_SD2_REF_EB] = &sd2_ref_eb.common.hw,
		[CLK_SERDES0_EB] = &serdes0_eb.common.hw,
		[CLK_SERDES1_EB] = &serdes1_eb.common.hw,
		[CLK_SERDES2_EB] = &serdes2_eb.common.hw,
		[CLK_RTM_EB] = &rtm_eb.common.hw,
		[CLK_RTM_ATB_EB] = &rtm_atb_eb.common.hw,
		[CLK_AON_NR_SPI_EB] = &aon_nr_spi_eb.common.hw,
		[CLK_AON_BM_S5_EB] = &aon_bm_s5_eb.common.hw,
		[CLK_EFUSE_EB] = &efuse_eb.common.hw,
		[CLK_GPIO_EB] = &gpio_eb.common.hw,
		[CLK_MBOX_EB] = &mbox_eb.common.hw,
		[CLK_KPD_EB] = &kpd_eb.common.hw,
		[CLK_AON_SYST_EB] = &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB] = &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB] = &aon_tmr_eb.common.hw,
		[CLK_DVFS_TOP_EB] = &dvfs_top_eb.common.hw,
		[CLK_APCPU_CLK_EB] = &apcpu_clk_eb.common.hw,
		[CLK_SPLK_EB] = &splk_eb.common.hw,
		[CLK_PIN_EB] = &pin_eb.common.hw,
		[CLK_ANA_EB] = &ana_eb.common.hw,
		[CLK_AON_CKG_EB] = &aon_ckg_eb.common.hw,
		[CLK_DJTAG_CTRL_EB] = &djtag_ctrl_eb.common.hw,
		[CLK_APCPU_TS0_EB] = &apcpu_ts0_eb.common.hw,
		[CLK_NIC400_AON_EB] = &nic400_aon_eb.common.hw,
		[CLK_SCC_EB] = &scc_eb.common.hw,
		[CLK_AP_SPI0_EB] = &ap_spi0_eb.common.hw,
		[CLK_AP_SPI1_EB] = &ap_spi1_eb.common.hw,
		[CLK_AP_SPI2_EB] = &ap_spi2_eb.common.hw,
		[CLK_AON_BM_S3_EB] = &aon_bm_s3_eb.common.hw,
		[CLK_SC_CC_EB] = &sc_cc_eb.common.hw,
		[CLK_THM0_EB] = &thm0_eb.common.hw,
		[CLK_THM1_EB] = &thm1_eb.common.hw,
		[CLK_AP_SIM_EB] = &ap_sim_eb.common.hw,
		[CLK_AON_I2C_EB] = &aon_i2c_eb.common.hw,
		[CLK_PMU_EB] = &pmu_eb.common.hw,
		[CLK_ADI_EB] = &adi_eb.common.hw,
		[CLK_EIC_EB] = &eic_eb.common.hw,
		[CLK_AP_INTC0_EB] = &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB] = &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB] = &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB] = &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB] = &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB] = &ap_intc5_eb.common.hw,
		[CLK_AUDCP_INTC_EB] = &audcp_intc_eb.common.hw,
		[CLK_AP_TMR0_EB] = &ap_tmr0_eb.common.hw,
		[CLK_AP_TMR1_EB] = &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB] = &ap_tmr2_eb.common.hw,
		[CLK_AP_WDG_EB] = &ap_wdg_eb.common.hw,
		[CLK_APCPU_WDG_EB] = &apcpu_wdg_eb.common.hw,
		[CLK_THM2_EB] = &thm2_eb.common.hw,
		[CLK_ARCH_RTC_EB] = &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB] = &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB] = &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB] = &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB] = &aon_tmr_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB] = &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB] = &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB] = &ap_wdg_rtc_eb.common.hw,
		[CLK_AC_WDG_RTC_EB] = &ac_wdg_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB] = &ap_tmr0_rtc_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB] = &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB] = &ap_tmr2_rtc_eb.common.hw,
		[CLK_DCXO_LC_RTC_EB] = &dcxo_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB] = &bb_cal_rtc_eb.common.hw,
		[CLK_DSI0_TEST_EB] = &dsi0_test_eb.common.hw,
		[CLK_DSI1_TEST_EB] = &dsi1_test_eb.common.hw,
		[CLK_DSI2_TEST_EB] = &dsi2_test_eb.common.hw,
		[CLK_DMC_REF_EN] = &dmc_ref_en.common.hw,
		[CLK_TSEN_EN] = &tsen_en.common.hw,
		[CLK_TMR_EN] = &tmr_en.common.hw,
		[CLK_RC100_REF_EN] = &rc100_ref_en.common.hw,
		[CLK_RC100_FDK_EN] = &rc100_fdk_en.common.hw,
		[CLK_DEBOUNCE_EN] = &debounce_en.common.hw,
		[CLK_DET_32K_EB] = &det_32k_eb.common.hw,
		[CLK_CSSYS_EN] = &cssys_en.common.hw,
		[CLK_SDIO0_2X_EN] = &sdio0_2x_en.common.hw,
		[CLK_SDIO0_1X_EN] = &sdio0_1x_en.common.hw,
		[CLK_SDIO1_2X_EN] = &sdio1_2x_en.common.hw,
		[CLK_SDIO1_1X_EN] = &sdio1_1x_en.common.hw,
		[CLK_SDIO2_2X_EN] = &sdio2_2x_en.common.hw,
		[CLK_SDIO2_1X_EN] = &sdio2_1x_en.common.hw,
		[CLK_EMMC_1X_EN] = &emmc_1x_en.common.hw,
		[CLK_EMMC_2X_EN] = &emmc_2x_en.common.hw,
		[CLK_NANDC_1X_EN] = &nandc_1x_en.common.hw,
		[CLK_NANDC_2X_EN] = &nandc_2x_en.common.hw,
		[CLK_ALL_PLL_TEST_EB] = &all_pll_test_eb.common.hw,
		[CLK_AAPC_TEST_EB] = &aapc_test_eb.common.hw,
		[CLK_DEBUG_TS_EB] = &debug_ts_eb.common.hw,
		[CLK_U2_0_REF_EN] = &u2_0_ref_en.common.hw,
		[CLK_U2_1_REF_EN] = &u2_1_ref_en.common.hw,
		[CLK_U3_0_REF_EN] = &u3_0_ref_en.common.hw,
		[CLK_U3_0_SUSPEND_EN] = &u3_0_suspend_en.common.hw,
		[CLK_U3_1_REF_EN] = &u3_1_ref_en.common.hw,
		[CLK_U3_1_SUSPEND_EN] = &u3_1_suspend_en.common.hw,
		[CLK_DSI0_REF_EN] = &dsi0_ref_en.common.hw,
		[CLK_DSI1_REF_EN] = &dsi1_ref_en.common.hw,
		[CLK_DSI2_REF_EN] = &dsi2_ref_en.common.hw,
		[CLK_PCIE_REF_EN] = &pcie_ref_en.common.hw,
	},
	.num	= CLK_AON_GATE_NUM,
};

static const struct sprd_clk_desc orca_aon_gate_desc = {
	.clk_clks	= orca_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(orca_aon_gate),
	.hw_clks	= &orca_aon_gate_hws,
};

static const struct of_device_id sprd_orca_clk_ids[] = {
	{ .compatible = "sprd,orca-apahb-gate",	/* 0x21000000 */
	  .data = &orca_apahb_gate_desc },
	{ .compatible = "sprd,orca-ap-clk",	/* 0x21100000 */
	  .data = &orca_ap_clk_desc },
	{ .compatible = "sprd,orca-apapb-gate",	/* 0x24000000 */
	  .data = &orca_apapb_gate_desc },
	{ .compatible = "sprd,orca-ipa-gate",	/* 0x29000000 */
	  .data = &orca_ipa_gate_desc },
	{ .compatible = "sprd,orca-aon-clk",	/* 0x63170000 */
	  .data = &orca_aon_clk_desc },
	{ .compatible = "sprd,orca-g3-pll",	/* 0x634b0000 */
	  .data = &orca_g3_pll_desc },
	{ .compatible = "sprd,orca-apcpu-clk",	/* 0x63970000 */
	  .data = &orca_apcpu_clk_desc },
	{ .compatible = "sprd,orca-pmu-gate",	/* 0x64010000 */
	  .data = &orca_pmu_gate_desc },
	{ .compatible = "sprd,orca-aon-gate",	/* 0x64020000 */
	  .data = &orca_aon_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_orca_clk_ids);

static int orca_clk_probe(struct platform_device *pdev)
{
	const struct sprd_clk_desc *desc;
	int ret;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc) {
		pr_err("%s: Cannot find matching driver data!\n", __func__);
		return -EINVAL;
	}

	ret = sprd_clk_regmap_init(pdev, desc);
	if (ret)
		return ret;

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver orca_clk_driver = {
	.probe	= orca_clk_probe,
	.driver	= {
		.name	= "orca-clk",
		.of_match_table	= sprd_orca_clk_ids,
	},
};
module_platform_driver(orca_clk_driver);

MODULE_DESCRIPTION("Spreadtrum Orca Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:orca-clk");
