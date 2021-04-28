/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/kernel.h>
#include <linux/delay.h>

#include "sprd_dphy_hal.h"

static int dphy_wait_pll_locked(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 50000; i++) {
		if (dphy_hal_is_pll_locked(dphy))
			return 0;
		udelay(3);
	}

	pr_err("error: dphy pll can not be locked\n");
	return -1;
}

static int dphy_wait_datalane_stop_state(struct sprd_dphy *dphy, u8 mask)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_stop_state_datalane(dphy) == mask)
			return 0;
		udelay(10);
	}

	pr_err("wait datalane stop-state time out\n");
	return -1;
}

static int dphy_wait_datalane_ulps_active(struct sprd_dphy *dphy, u8 mask)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_ulps_active_datalane(dphy) == mask)
			return 0;
		udelay(10);
	}

	pr_err("wait datalane ulps-active time out\n");
	return -1;
}

static int dphy_wait_clklane_stop_state(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_stop_state_clklane(dphy))
			return 0;
		udelay(10);
	}

	pr_err("wait clklane stop-state time out\n");
	return -1;
}

static int dphy_wait_clklane_ulps_active(struct sprd_dphy *dphy)
{
	u32 i = 0;

	for (i = 0; i < 5000; i++) {
		if (dphy_hal_is_ulps_active_clklane(dphy))
			return 0;
		udelay(10);
	}

	pr_err("wait clklane ulps-active time out\n");
	return -1;
}

int sprd_dphy_configure(struct sprd_dphy *dphy)
{
	struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	pr_info("lanes : %d\n", ctx->lanes);
	pr_info("freq : %d\n", ctx->freq);

	dphy_hal_rstz(dphy, 0);
	dphy_hal_shutdownz(dphy, 0);
	dphy_hal_clklane_en(dphy, 0);

	dphy_hal_test_clr(dphy, 0);
	dphy_hal_test_clr(dphy, 1);
	dphy_hal_test_clr(dphy, 0);

	pll->pll_config(ctx);
	pll->timing_config(ctx);

	dphy_hal_shutdownz(dphy, 1);
	dphy_hal_rstz(dphy, 1);
	dphy_hal_stop_wait_time(dphy, 0x1C);
	dphy_hal_clklane_en(dphy, 1);
	dphy_hal_datalane_en(dphy);

	if (dphy_wait_pll_locked(dphy))
		return -1;

	return 0;
}

void sprd_dphy_reset(struct sprd_dphy *dphy)
{
	dphy_hal_rstz(dphy, 0);
	udelay(10);
	dphy_hal_rstz(dphy, 1);
}

void sprd_dphy_shutdown(struct sprd_dphy *dphy)
{
	dphy_hal_shutdownz(dphy, 0);
	udelay(10);
	dphy_hal_shutdownz(dphy, 1);
}

int sprd_dphy_hop_config(struct sprd_dphy *dphy, int delta, int period)
{
	struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	if (pll->hop_config)
		return pll->hop_config(ctx, delta, period);

	return 0;
}

int sprd_dphy_ssc_en(struct sprd_dphy *dphy, bool en)
{
	struct dphy_pll_ops *pll = dphy->pll;
	struct dphy_context *ctx = &dphy->ctx;

	if (pll->ssc_en)
		return pll->ssc_en(ctx, en);

	return 0;
}

int sprd_dphy_close(struct sprd_dphy *dphy)
{
	if (!dphy)
		return -1;

	dphy_hal_rstz(dphy, 0);
	dphy_hal_shutdownz(dphy, 0);
	dphy_hal_rstz(dphy, 1);

	return 0;
}

int sprd_dphy_data_ulps_enter(struct sprd_dphy *dphy)
{
	u8 lane_mask = (1 << dphy->ctx.lanes) - 1;

	dphy_hal_datalane_ulps_rqst(dphy, 1);

	dphy_wait_datalane_ulps_active(dphy, lane_mask);

	/*
	 * WARNING:
	 * Don't clear ulps_request signal here, otherwise the
	 * Mark-1 patten could not be generated when ULPS exit.
	 */

	return 0;
}

int sprd_dphy_data_ulps_exit(struct sprd_dphy *dphy)
{
	u8 lane_mask = (1 << dphy->ctx.lanes) - 1;

	dphy_hal_datalane_ulps_exit(dphy, 1);

	dphy_hal_datalane_ulps_rqst(dphy, 0);

	dphy_wait_datalane_stop_state(dphy, lane_mask);

	dphy_hal_datalane_ulps_exit(dphy, 0);

	return 0;
}

int sprd_dphy_clk_ulps_enter(struct sprd_dphy *dphy)
{
	dphy_hal_clklane_ulps_rqst(dphy, 1);

	dphy_wait_clklane_ulps_active(dphy);

	/*
	 * WARNING:
	 * Don't clear ulps_request signal here, otherwise the
	 * Mark-1 patten could not be generated when ULPS exit.
	 */

	return 0;
}

int sprd_dphy_clk_ulps_exit(struct sprd_dphy *dphy)
{
	dphy_hal_clklane_ulps_exit(dphy, 1);

	dphy_hal_clklane_ulps_rqst(dphy, 0);

	dphy_wait_clklane_stop_state(dphy);

	dphy_hal_clklane_ulps_exit(dphy, 0);

	return 0;
}

void sprd_dphy_force_pll(struct sprd_dphy *dphy, bool enable)
{
	dphy_hal_force_pll(dphy, enable);
}

void sprd_dphy_hs_clk_en(struct sprd_dphy *dphy, bool enable)
{
	dphy_hal_clk_hs_rqst(dphy, enable);

	dphy_wait_pll_locked(dphy);
}

void sprd_dphy_test_write(struct sprd_dphy *dphy, u8 address, u8 data)
{
	dphy_hal_test_en(dphy, 1);

	dphy_hal_test_din(dphy, address);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);

	dphy_hal_test_en(dphy, 0);

	dphy_hal_test_din(dphy, data);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);
}

u8 sprd_dphy_test_read(struct sprd_dphy *dphy, u8 address)
{
	dphy_hal_test_en(dphy, 1);

	dphy_hal_test_din(dphy, address);

	dphy_hal_test_clk(dphy, 1);
	dphy_hal_test_clk(dphy, 0);

	dphy_hal_test_en(dphy, 0);

	udelay(1);

	return dphy_hal_test_dout(dphy);
}



