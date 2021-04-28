/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>

#include "dwc_mipi_dsi_host.h"
#include "sprd_dphy.h"

#define read32(c)	readl((void __force __iomem *)(c))
#define write32(v, c)	writel(v, (void __force __iomem *)(c))

/**
 * Reset D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param reset
 */
static void dsi_phy_rstz(struct dphy_context *ctx, int level)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA0 phy_rstz;

	phy_rstz.val = read32(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_rstz = level;

	write32(phy_rstz.val, &reg->PHY_RSTZ);
}

/**
 * Power up/down D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param enable (1: shutdown)
 */
static void dsi_phy_shutdownz(struct dphy_context *ctx, int level)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA0 phy_rstz;

	phy_rstz.val = read32(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_shutdownz = level;

	write32(phy_rstz.val, &reg->PHY_RSTZ);
}

/**
 * Force D-PHY PLL to stay on while in ULPS
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param force (1) disable (0)
 * @note To follow the programming model, use wakeup_pll function
 */
static void dsi_phy_force_pll(struct dphy_context *ctx, int force)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA0 phy_rstz;

	phy_rstz.val = read32(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_forcepll = force;

	write32(phy_rstz.val, &reg->PHY_RSTZ);
}

static void dsi_phy_clklane_ulps_rqst(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA8 phy_ulps_ctrl;

	phy_ulps_ctrl.val = read32(&reg->PHY_ULPS_CTRL);
	phy_ulps_ctrl.bits.phy_txrequlpsclk = en;

	write32(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
}

static void dsi_phy_clklane_ulps_exit(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA8 phy_ulps_ctrl;

	phy_ulps_ctrl.val = read32(&reg->PHY_ULPS_CTRL);
	phy_ulps_ctrl.bits.phy_txexitulpsclk = en;

	write32(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
}

static void dsi_phy_datalane_ulps_rqst(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA8 phy_ulps_ctrl;

	phy_ulps_ctrl.val = read32(&reg->PHY_ULPS_CTRL);
	phy_ulps_ctrl.bits.phy_txrequlpslan = en;

	write32(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
}

static void dsi_phy_datalane_ulps_exit(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA8 phy_ulps_ctrl;

	phy_ulps_ctrl.val = read32(&reg->PHY_ULPS_CTRL);
	phy_ulps_ctrl.bits.phy_txexitulpslan = en;

	write32(phy_ulps_ctrl.val, &reg->PHY_ULPS_CTRL);
}

/**
 * Configure minimum wait period for HS transmission request after a stop state
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param no_of_byte_cycles [in byte (lane) clock cycles]
 */
static void dsi_phy_stop_wait_time(struct dphy_context *ctx, u8 byte_cycle)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA4 phy_if_cfg;

	phy_if_cfg.val = read32(&reg->PHY_IF_CFG);
	phy_if_cfg.bits.phy_stop_wait_time = byte_cycle;

	write32(phy_if_cfg.val, &reg->PHY_IF_CFG);
}

/**
 * Set number of active lanes
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param no_of_lanes
 */
static void dsi_phy_datalane_en(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA4 phy_if_cfg;

	phy_if_cfg.val = read32(&reg->PHY_IF_CFG);
	phy_if_cfg.bits.n_lanes = ctx->lanes - 1;

	write32(phy_if_cfg.val, &reg->PHY_IF_CFG);
}

/**
 * Enable clock lane module
 * @param phy pointer to structure
 *  which holds information about the d-phy module
 * @param en
 */
static void dsi_phy_clklane_en(struct dphy_context *ctx, int en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xA0 phy_rstz;

	phy_rstz.val = read32(&reg->PHY_RSTZ);
	phy_rstz.bits.phy_enableclk = en;

	write32(phy_rstz.val, &reg->PHY_RSTZ);
}

/**
 * Request the PHY module to start transmission of high speed clock.
 * This causes the clock lane to start transmitting DDR clock on the
 * lane interconnect.
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param enable
 * @note this function should be called explicitly by user always except for
 * transmitting
 */
static void dsi_phy_clk_hs_rqst(struct dphy_context *ctx, int enable)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0x94 lpclk_ctrl;

	lpclk_ctrl.val = read32(&reg->LPCLK_CTRL);

	lpclk_ctrl.bits.auto_clklane_ctrl = 0;
	lpclk_ctrl.bits.phy_txrequestclkhs = enable;

	write32(lpclk_ctrl.val, &reg->LPCLK_CTRL);
}

/**
 * Get D-PHY PPI status
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param mask
 * @return status
 */
static u8 dsi_phy_is_pll_locked(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB0 phy_status;

	phy_status.val = read32(&reg->PHY_STATUS);

	return phy_status.bits.phy_lock;
}

static u8 dsi_phy_is_rx_direction(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB0 phy_status;

	phy_status.val = read32(&reg->PHY_STATUS);

	return phy_status.bits.phy_direction;
}

static u8 dsi_phy_is_rx_ulps_esc_lane0(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB0 phy_status;

	phy_status.val = read32(&reg->PHY_STATUS);

	return phy_status.bits.phy_rxulpsesc0lane;
}

static u8 dsi_phy_is_stop_state_datalane(struct dphy_context *ctx)
{
	u8 state = 0;
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB0 phy_status;

	phy_status.val = read32(&reg->PHY_STATUS);

	if (phy_status.bits.phy_stopstate0lane)
		state |= BIT(0);
	if (phy_status.bits.phy_stopstate1lane)
		state |= BIT(1);
	if (phy_status.bits.phy_stopstate2lane)
		state |= BIT(2);
	if (phy_status.bits.phy_stopstate3lane)
		state |= BIT(3);

	return state;
}

static u8 dsi_phy_is_stop_state_clklane(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB0 phy_status;

	phy_status.val = read32(&reg->PHY_STATUS);

	return phy_status.bits.phy_stopstateclklane;
}

static u8 dsi_phy_is_ulps_active_datalane(struct dphy_context *ctx)
{
	u8 state = 0;
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB0 phy_status;

	phy_status.val = read32(&reg->PHY_STATUS);

	if (!phy_status.bits.phy_ulpsactivenot0lane)
		state |= BIT(0);
	if (!phy_status.bits.phy_ulpsactivenot1lane)
		state |= BIT(1);
	if (!phy_status.bits.phy_ulpsactivenot2lane)
		state |= BIT(2);
	if (!phy_status.bits.phy_ulpsactivenot3lane)
		state |= BIT(3);

	return state;
}

static u8 dsi_phy_is_ulps_active_clklane(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB0 phy_status;

	phy_status.val = read32(&reg->PHY_STATUS);

	return !phy_status.bits.phy_ulpsactivenotclk;
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dsi_phy_test_clk(struct dphy_context *ctx, u8 value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB4 phy_tst_ctrl0;

	phy_tst_ctrl0.val = read32(&reg->PHY_TST_CTRL0);
	phy_tst_ctrl0.bits.phy_testclk = value;

	write32(phy_tst_ctrl0.val, &reg->PHY_TST_CTRL0);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dsi_phy_test_clr(struct dphy_context *ctx, u8 value)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB4 phy_tst_ctrl0;

	phy_tst_ctrl0.val = read32(&reg->PHY_TST_CTRL0);
	phy_tst_ctrl0.bits.phy_testclr = value;

	write32(phy_tst_ctrl0.val, &reg->PHY_TST_CTRL0);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param on_falling_edge
 */
static void dsi_phy_test_en(struct dphy_context *ctx, u8 en)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB8 phy_tst_ctrl1;

	phy_tst_ctrl1.val = read32(&reg->PHY_TST_CTRL1);
	phy_tst_ctrl1.bits.phy_testen = en;

	write32(phy_tst_ctrl1.val, &reg->PHY_TST_CTRL1);
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 */
static u8 dsi_phy_test_dout(struct dphy_context *ctx)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB8 phy_tst_ctrl1;

	phy_tst_ctrl1.val = read32(&reg->PHY_TST_CTRL1);

	return phy_tst_ctrl1.bits.phy_testdout;
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param test_data
 */
static void dsi_phy_test_din(struct dphy_context *ctx, u8 data)
{
	struct dsi_reg *reg = (struct dsi_reg *)ctx->ctrlbase;
	union _0xB8 phy_tst_ctrl1;

	phy_tst_ctrl1.val = read32(&reg->PHY_TST_CTRL1);
	phy_tst_ctrl1.bits.phy_testdin = data;

	write32(phy_tst_ctrl1.val, &reg->PHY_TST_CTRL1);
}

static struct dphy_ppi_ops dwc_mipi_dsi_host_ppi_ops = {
	.rstz                     = dsi_phy_rstz,
	.shutdownz                = dsi_phy_shutdownz,
	.force_pll                = dsi_phy_force_pll,
	.clklane_ulps_rqst        = dsi_phy_clklane_ulps_rqst,
	.clklane_ulps_exit        = dsi_phy_clklane_ulps_exit,
	.datalane_ulps_rqst       = dsi_phy_datalane_ulps_rqst,
	.datalane_ulps_exit       = dsi_phy_datalane_ulps_exit,
	.stop_wait_time           = dsi_phy_stop_wait_time,
	.datalane_en              = dsi_phy_datalane_en,
	.clklane_en               = dsi_phy_clklane_en,
	.clk_hs_rqst              = dsi_phy_clk_hs_rqst,
	.is_pll_locked            = dsi_phy_is_pll_locked,
	.is_rx_direction          = dsi_phy_is_rx_direction,
	.is_rx_ulps_esc_lane0     = dsi_phy_is_rx_ulps_esc_lane0,
	.is_stop_state_datalane   = dsi_phy_is_stop_state_datalane,
	.is_stop_state_clklane    = dsi_phy_is_stop_state_clklane,
	.is_ulps_active_datalane  = dsi_phy_is_ulps_active_datalane,
	.is_ulps_active_clklane   = dsi_phy_is_ulps_active_clklane,
	.tst_clk                  = dsi_phy_test_clk,
	.tst_clr                  = dsi_phy_test_clr,
	.tst_en                   = dsi_phy_test_en,
	.tst_dout                 = dsi_phy_test_dout,
	.tst_din                  = dsi_phy_test_din,
	.bist_en                  = NULL,
	.is_bist_ok               = NULL,
};

static struct ops_entry entry = {
	.ver = "synps,dwc-mipi-dsi-host",
	.ops = &dwc_mipi_dsi_host_ppi_ops,
};

static int __init dphy_ppi_register(void)
{
	return dphy_ppi_ops_register(&entry);
}

subsys_initcall(dphy_ppi_register);

