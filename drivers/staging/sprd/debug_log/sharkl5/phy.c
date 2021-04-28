/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "core.h"
#include "phy.h"

#define REG_DBG_APB_TEST_CTRL			0x0050
#define REG_DBG_APB_TEST_DIN			0x0050
#define REG_DBG_APB_TEST_DOUT			0x0050
#define REG_DBG_APB_DBG_PHY_CTRL_0		0x004c
#define REG_DBG_APB_DBG_PHY_CTRL_5		0x004c
#define REG_DBG_APB_DBG_PHY_CTRL_RO_1		0x0028

/* REG_DBG_APB_TEST_CTRL */
#define BIT_DBG_APB_DBG_TESTCLR			BIT(0)
#define BIT_DBG_APB_DBG_TESTCLK			BIT(1)
#define BIT_DBG_APB_DBG_TESTEN			BIT(2)

/* REG_DBG_APB_DBG_PHY_CTRL_0 */
#define BIT_DBG_APB_RSTZ_DB			BIT(14)
#define BIT_DBG_APB_SHUTDOWN_DB			BIT(15)

/* REG_DBG_APB_DBG_PHY_CTRL_5 */
#define BIT_DBG_APB_ENABLECLK_DB		BIT(9)
#define BIT_DBG_APB_ENABLE_0_DB			BIT(10)
#define BIT_DBG_APB_ENABLE_1_DB			BIT(11)
#define BIT_DBG_APB_ENABLE_2_DB			BIT(12)
#define BIT_DBG_APB_ENABLE_3_DB			BIT(13)

/* REG_DBG_APB_DBG_PHY_CTRL_RO_1 */
#define BIT_DBG_APB_PLLLOCK_DB			BIT(16)

/**
 * Reset D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param reset
 */
static void dbg_phy_rstz(unsigned long base, int level)
{
	if (level) {
		reg_bits_set(base + REG_DBG_APB_DBG_PHY_CTRL_0,
			     BIT_DBG_APB_RSTZ_DB);
	} else {
		reg_bits_clr(base + REG_DBG_APB_DBG_PHY_CTRL_0,
			     BIT_DBG_APB_RSTZ_DB);
	}
}

/**
 * Power up/down D-PHY module
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param enable (1: shutdown)
 */
static void dbg_phy_shutdownz(unsigned long base, int level)
{
	if (level) {
		reg_bits_set(base + REG_DBG_APB_DBG_PHY_CTRL_0,
			     BIT_DBG_APB_SHUTDOWN_DB);
	} else {
		reg_bits_clr(base + REG_DBG_APB_DBG_PHY_CTRL_0,
			     BIT_DBG_APB_SHUTDOWN_DB);
	}
}

/**
 * Set number of active lanes
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param no_of_lanes
 */
static void dbg_phy_datalane_en(unsigned long base, int no)
{
	reg_bits_set(base + REG_DBG_APB_DBG_PHY_CTRL_5,
		     BIT_DBG_APB_ENABLE_0_DB |
		     BIT_DBG_APB_ENABLE_1_DB |
		     BIT_DBG_APB_ENABLE_2_DB | BIT_DBG_APB_ENABLE_3_DB);
}

/**
 * Enable clock lane module
 * @param phy pointer to structure
 *  which holds information about the d-phy module
 * @param en
 */
static void dbg_phy_clklane_en(unsigned long base, int en)
{
	if (en) {
		reg_bits_set(base + REG_DBG_APB_DBG_PHY_CTRL_5,
			     BIT_DBG_APB_ENABLECLK_DB);
	} else {
		reg_bits_clr(base + REG_DBG_APB_DBG_PHY_CTRL_5,
			     BIT_DBG_APB_ENABLECLK_DB);
	}
}

/**
 * Get D-PHY PPI status
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param mask
 * @return status
 */
static u8 dbg_phy_is_pll_locked(unsigned long base)
{
	/*sharkle not support*/
	return 1;
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dbg_phy_test_clk(unsigned long base, u8 value)
{
	if (value) {
		reg_bits_set(base + REG_DBG_APB_TEST_CTRL,
			     BIT_DBG_APB_DBG_TESTCLK);
	} else {
		reg_bits_clr(base + REG_DBG_APB_TEST_CTRL,
			     BIT_DBG_APB_DBG_TESTCLK);
	}
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param value
 */
static void dbg_phy_test_clr(unsigned long base, u8 value)
{
	if (value) {
		reg_bits_set(base + REG_DBG_APB_TEST_CTRL,
			     BIT_DBG_APB_DBG_TESTCLR);
	} else {
		reg_bits_clr(base + REG_DBG_APB_TEST_CTRL,
			     BIT_DBG_APB_DBG_TESTCLR);
	}
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param on_falling_edge
 */
static void dbg_phy_test_en(unsigned long base, u8 value)
{
	if (value) {
		reg_bits_set(base + REG_DBG_APB_TEST_CTRL,
			     BIT_DBG_APB_DBG_TESTEN);
	} else {
		reg_bits_clr(base + REG_DBG_APB_TEST_CTRL,
			     BIT_DBG_APB_DBG_TESTEN);
	}
}

/**
 * @param phy pointer to structure which holds information about the d-phy
 * module
 * @param test_data
 */
static void dbg_phy_test_din(unsigned long base, u8 data)
{
	reg_write(base + REG_DBG_APB_TEST_DIN, data);
}

/**
 * Write to D-PHY module (encapsulating the digital interface)
 * @param base pointer to structure which holds information about the d-base
 * module
 * @param address offset inside the D-PHY digital interface
 * @param data array of bytes to be written to D-PHY
 * @param data_length of the data array
 */
static void dbg_phy_test_write(unsigned long base, u8 address, u8 data)
{
	dbg_phy_test_en(base, 1);
	dbg_phy_test_din(base, address);
	dbg_phy_test_clk(base, 1);
	dbg_phy_test_clk(base, 0);
	dbg_phy_test_en(base, 0);
	dbg_phy_test_din(base, data);
	dbg_phy_test_clk(base, 1);
	dbg_phy_test_clk(base, 0);
}

static int dbg_phy_wait_pll_locked(unsigned long base)
{
	int i = 0;

	/*5000 cycle delay to wait pll locked*/
	for (i = 0; i < 5000; i++) {
		if (dbg_phy_is_pll_locked(base))
			return 0;
		udelay(3);
	}

	pr_err("error: base pll can not be locked\n");
	return -EINVAL;
}

static void dbg_phy_pll_config(unsigned long base, u32 freq)
{
	switch (freq) {
	/*1.5G phy freq*/
	case 1500000:
		dbg_phy_test_write(base, 0x06, 0x39);
		dbg_phy_test_write(base, 0x08, 0x5f);
		dbg_phy_test_write(base, 0x09, 0xb1);
		dbg_phy_test_write(base, 0x0a, 0x3b);
		dbg_phy_test_write(base, 0x0b, 0x11);
		break;
	case 1400000:
		dbg_phy_test_write(base, 0x06, 0x35);
		dbg_phy_test_write(base, 0x08, 0xaf);
		dbg_phy_test_write(base, 0x09, 0xd8);
		dbg_phy_test_write(base, 0x0a, 0x9d);
		dbg_phy_test_write(base, 0x0b, 0x81);
		break;
	case 1300000:
		dbg_phy_test_write(base, 0x06, 0x32);
		dbg_phy_test_write(base, 0x08, 0xff);
		dbg_phy_test_write(base, 0x09, 0x00);
		dbg_phy_test_write(base, 0x0a, 0x00);
		dbg_phy_test_write(base, 0x0b, 0x01);
		break;
	case 1200000:
		dbg_phy_test_write(base, 0x06, 0x2e);
		dbg_phy_test_write(base, 0x08, 0x4f);
		dbg_phy_test_write(base, 0x09, 0x27);
		dbg_phy_test_write(base, 0x0a, 0x62);
		dbg_phy_test_write(base, 0x0b, 0x71);
		break;
	case 1000000:
		break;
	default:
		pr_err("the target freq is not supported\n");
		return;
	}
	dbg_phy_test_write(base, 0x32, 0x20);
	dbg_phy_test_write(base, 0x42, 0x20);
	dbg_phy_test_write(base, 0x52, 0x20);
	dbg_phy_test_write(base, 0x62, 0x20);
	dbg_phy_test_write(base, 0x72, 0x20);
	dbg_phy_test_write(base, 0x33, 0x20);
	dbg_phy_test_write(base, 0x43, 0x20);
	dbg_phy_test_write(base, 0x53, 0x20);
	dbg_phy_test_write(base, 0x63, 0x20);
	dbg_phy_test_write(base, 0x73, 0x20);

	pr_info("set phy freq %u OK\n", freq);
}

/**
 * Configure D-PHY and PLL module to desired operation mode
 * @param base: pointer to structure
 *  which holds information about the d-base module
 * @param no_of_lanes active
 * @param freq desired high speed frequency
 * @return error code
 */
int dbg_phy_init(struct phy_ctx *phy)
{
	unsigned long base = phy->base;

	dbg_phy_rstz(base, 0);
	dbg_phy_shutdownz(base, 0);
	dbg_phy_clklane_en(base, 0);
	dbg_phy_test_clr(base, 1);
	dbg_phy_test_clr(base, 0);
	/*sharkle not support */
	dbg_phy_pll_config(base, phy->freq);

	dbg_phy_datalane_en(base, phy->lanes);
	dbg_phy_clklane_en(base, 1);
	dbg_phy_shutdownz(base, 1);
	dbg_phy_rstz(base, 1);
	if (dbg_phy_wait_pll_locked(base))
		return -EINVAL;

	return 0;
}

/**
 * Close and power down D-PHY module
 * @param base pointer to structure which holds information about the d-base
 * module
 * @return error code
 */
int dbg_phy_exit(struct phy_ctx *phy)
{
	unsigned long base = phy->base;

	dbg_phy_rstz(base, 0);
	dbg_phy_shutdownz(base, 0);
	dbg_phy_rstz(base, 1);

	return 0;
}
