/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt) "sprd_hwdvfs: " fmt

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <dt-bindings/soc/sprd,sharkl3-mask.h>
#include <dt-bindings/soc/sprd,sharkl3-regs.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/cpufreq.h>
#include "sprd-cpufreqhw.h"
#include "sprd-hwdvfs-sharkl3.h"

#define MPLL_HW_DVFS_EACH_NUM 6
#define MPLL_HW_DVFS_MAX_NUM (MPLL_HW_DVFS_EACH_NUM * 3)
#define SWDVFS_HWDVFS_KINT_DIFF_BITS 21

#define REG_DVFS_CTRL_BASE 0x0

#define BIT_DVFS_CTRL_PROJ_NAME1 \
	BIT_DVFS_CTRL_PROJ_NAME(0xff00)
#define BIT_DVFS_CTRL_PROJ_NAME0 \
	BIT_DVFS_CTRL_PROJ_NAME(0xff)
#define BIT_DVFS_CTRL_VERSION1 \
	BIT_DVFS_CTRL_VERSION(0xff00)
#define BIT_DVFS_CTRL_VERSION0 \
	BIT_DVFS_CTRL_VERSION(0xff)

#define DVFS_CTRL_MAGIC_NUM_LOCK 0x5348554c
#define DVFS_CTRL_MAGIC_NUM_UNLOCK 0x00000000

#define SW_CHNL02_EN_MSK \
	BIT_DVFS_CTRL_SW_CHNL_EN(0x1 << 2)
#define SW_CHNL01_EN_MSK \
	BIT_DVFS_CTRL_SW_CHNL_EN(0x1 << 1)
#define SW_CHNL00_EN_MSK \
	BIT_DVFS_CTRL_SW_CHNL_EN(0x1)

/* chnl1/2-adi-core0,chnl0-i2c-core1 */
#define DCDC_MAP_ADI_CHNL12_I2C_CHNL0 0x001

#define GLB_CFG_DEBUG_SEL_EN 0x1d

#define TMR_PRESCALE_1US 25

#define TMR_CTRLx_DCDCx_STABLE_US 10
#define TMR_CTRLx_DCDCx_I2C_PAUSE_US (75 + TMR_CTRLx_DCDCx_STABLE_US + 50)
#define TMR_CTRLx_DCDCx_ADI_PAUSE_US (25 + TMR_CTRLx_DCDCx_STABLE_US)

/* DCDC 2721 CONFIG */
#define VTUNE_STEP_CORE0_MV 25
#define VTUNE_STEP_CORE1_MV 25
#define TMR_CTRL2_CORE1_STABLE_US_2721 (VTUNE_STEP_CORE0_MV * 10 / 50)
#define VTUNE_STEP_VAL_CORE00_2721 (((int)VTUNE_STEP_CORE0_MV * 32) / 100)

/* DCDC FAN5355 CONFIG */
#define TMR_CTRL2_CORE1_STABLE_US_FAN5355 20

#define VTUNE_STEP_VAL_CORE00_DEFALUT 20
#define VTUNE_STEP_VAL_CORE00_VAL  VTUNE_STEP_VAL_CORE00_2721

#define TMR_CTRL1_CORE0_HOLD_US 50
#define TMR_CTRL1_CORE0_PAUSE_US TMR_CTRL2_CORE1_STABLE_US_2721

#define TMR_CTRL2_CORE0_TIMEOUT_US 800
#define TMR_CTRL2_CORE0_STABLE_US TMR_CTRL2_CORE1_STABLE_US_2721

#define TMR_CTRL1_CORE1_HOLD_US 25
#define TMR_CTRL1_CORE1_PAUSE_US TMR_CTRL2_CORE1_STABLE_US_FAN5355

#define TMR_CTRL2_CORE1_TIMEOUT_US 800
#define TMR_CTRL2_CORE1_STABLE_US TMR_CTRL2_CORE1_STABLE_US_FAN5355

#define TMR_CTRL_PLL_PD_US 2
#define TMR_CTRL_PLL_STABLE_US 199

/**
 * clock mux change stable period. it begins
 * from the end of fsel changes to backup.
 * all pll use the same value.
 * suggestive value is four clock period of the
 * slowest clock. and the possible slowest clock
 * during hw dvfs may be 512MHz,
 * so set to one 26MHz clock period is enough.
 */
#define TMR_CTRL_FMUX_US 0

#define SCALE_TAB_VTUNE_MSK (0x3ff << 7)
#define SCALE_TAB_FCFG_MSK (0xf << 3)
#define SCALE_TAB_FSEL_MSK 0x7
#define SCALE_TAB_EACH_NUM 8
#define SCALE_TAB_MAX_NUM (SCALE_TAB_EACH_NUM * 3)

#define REG_DCDC0_VOL_CTL 0xc54
#define DCDC0_VOL_CTL_MSK 0x1ff
#define REG_DCDC1_VOL_CTL 0xc64
#define DCDC1_VOL_CTL_MSK 0x1ff

#define REG_DCDC0_VOL_CTL_FAN5355 0x01
#define DCDC0_VOL_CTL_MSK_FAN5355 0xff

#define VAL2REG(val, msk) \
	((((unsigned int)(val)) << (__ffs(msk))) & ((unsigned int)(msk)))

#define REG2VAL(val_in_reg, msk) \
	((((unsigned int)(val_in_reg)) & ((unsigned int)(msk))) >> \
	__ffs(msk))

/* frequency calculation */
#define FCFG_REFIN_MHZ 26
#define FCFG_NINT(fvco_khz) \
	((unsigned long)(fvco_khz * 1000) / (FCFG_REFIN_MHZ * 1000000))

#define FCFG_KINT_REMAINDER(fvco_khz) \
	((unsigned long)(((fvco_khz * 1000) - \
	FCFG_REFIN_MHZ * FCFG_NINT(fvco_khz) * 1000000) / \
	10000))

#define FCFG_KINT_COEF \
	((unsigned long)(( \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_KINT >> \
	__ffs(MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_KINT)) + \
	1))

#define FCFG_KINT_DIVISOR ((unsigned long)(FCFG_REFIN_MHZ * 100))

#ifdef CONFIG_64BIT
#define FCFG_KINT(fvco_khz)  \
	((FCFG_KINT_REMAINDER(fvco_khz) *  \
	FCFG_KINT_COEF) / \
	FCFG_KINT_DIVISOR)
#else
#define FCFG_KINT(fvco_khz) \
	((FCFG_KINT_REMAINDER(fvco_khz) * \
	(FCFG_KINT_COEF / FCFG_KINT_DIVISOR)) + \
	((FCFG_KINT_REMAINDER(fvco_khz) * \
	(FCFG_KINT_COEF % FCFG_KINT_DIVISOR)) / FCFG_KINT_DIVISOR))
#endif
#define VAL2REG_IBIAS_RESERVED(in) (VAL2REG(in, \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_IBIAS_DVFS_0 |  \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_RESERVED_DVFS_0))

#define VAL2REG_IBIAS(ibias) (VAL2REG(ibias, \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_IBIAS_DVFS_0))

#define VAL2REG_POSTDIV(postdiv) (VAL2REG(postdiv, \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_POSTDIV_DVFS_0))

#define VAL2REG_KN_HW(fvco_khz) \
	(VAL2REG(FCFG_NINT(fvco_khz), \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_NINT_DVFS_0) |  \
	VAL2REG((FCFG_KINT(fvco_khz) >> SWDVFS_HWDVFS_KINT_DIFF_BITS), \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_KINT_DVFS_0))

#define VAL2REG_KN_MPLL(nINT, kINT) \
	(VAL2REG(nINT, \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_NINT_DVFS_0) | \
	VAL2REG((kINT >> SWDVFS_HWDVFS_KINT_DIFF_BITS), \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_KINT_DVFS_0))

#define VAL2REG_KN_SW(fvco_khz)  \
	(VAL2REG(FCFG_NINT(fvco_khz), \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_NINT) | \
	VAL2REG((FCFG_KINT(fvco_khz)), \
	MASK_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_KINT))

#define VAL2REG_FSEL(val) VAL2REG(val, SCALE_TAB_FSEL_MSK)
#define VAL2REG_FCFG(val) VAL2REG(val, SCALE_TAB_FCFG_MSK)

#define VAL2REG_DCDC(to_uv, min_uv, step_uv, msk) \
	VAL2REG(DIV_ROUND_UP((int)((to_uv) - (min_uv)), (step_uv)), msk)

#define VAL2REG_DCDC_FAN5355(to_uv, min_uv, step_uv, msk) \
	VAL2REG(DIV_ROUND_UP((int)((to_uv) - (min_uv)), (step_uv)) | 0x80, msk)

#define REG2VAL_DCDC(reg_uv, min_uv, step_uv, hide_offset_uv, msk) \
	((min_uv + REG2VAL(reg_uv, msk) * step_uv))

/* channel0:little core */
#define HW_DVFS_TAB_CLUSTER0(hz, uv, idx_freq, idx_dcdc) \
	(sprd_val2reg_dcdcx_vtune(uv, idx_dcdc) | \
	VAL2REG_FCFG((((hz) <= 768000000UL ? 1 : 0) << 3) | idx_freq) | \
	VAL2REG_FSEL((hz) <= 768000000UL ? 0x02 : 0x06))

/* channel1:big core */
#define HW_DVFS_TAB_CLUSTER1(hz, uv, idx_freq, idx_dcdc) \
	(sprd_val2reg_dcdcx_vtune(uv, idx_dcdc) | \
	VAL2REG_FCFG((((hz) <= 768000000UL ? 1 : 0) << 3) | idx_freq) | \
	VAL2REG_FSEL((hz) <= 768000000UL ? 0x02 : 0x07))

/* channel2:fcm */
#define HW_DVFS_TAB_CLUSTER2(hz, uv, idx_freq, idx_dcdc) \
	(sprd_val2reg_dcdcx_vtune(uv, idx_dcdc) | \
	VAL2REG_FCFG((((hz) <= 768000000UL ? 1 : 0) << 3) | idx_freq) | \
	VAL2REG_FSEL((hz) <= 768000000UL ? 0x02 : 0x05))

/* FAN5355 + 2721 verify, update dvfs table */
#define MPLL_HW_DVFS_TAB_CLUSTER0(khz)  \
	(VAL2REG_IBIAS((khz) < 1200000UL ? 0x00 : 0x01) | \
	VAL2REG_POSTDIV((khz) < 1000000UL ? 0x01 : 0x00) | \
	VAL2REG_KN_HW((khz) * ((khz) < 1000000UL ? 0x02 : 0x01)))

#define MPLL_HW_DVFS_TAB_CLUSTER1(khz) \
	(VAL2REG_IBIAS((khz) < 1225000UL ? 0x01 : \
		       ((khz) < 1500000UL ? 0x02 : 0x03)) | \
	VAL2REG_POSTDIV((khz) < 1000000UL ? 0x01 : 0x00) | \
	VAL2REG_KN_HW((khz) * ((khz) < 1000000UL ? 0x02 : 0x01)))

#define MPLL_HW_DVFS_TAB_CLUSTER2(khz) \
	(VAL2REG_IBIAS((khz) < 1066000UL ? 0x00 : \
		       ((khz) < 1350000UL ? 0x01 : 0x02)) | \
	VAL2REG_POSTDIV((khz) < 1000000UL ? 0x01 : 0x00) |  \
	VAL2REG_KN_HW((khz) * ((khz) < 1000000UL ? 0x02 : 0x01)))

struct sprd_hwdvfs_l3_reg {
	unsigned int default_uv;
	unsigned int step_uv;
	unsigned int min_uv;
	unsigned int max_uv;
	unsigned int hide_offset_uv;
	unsigned long reg_vol_trm;
	unsigned int vol_trm_bits;
	/* const bits in vol reg */
	unsigned int vol_const_bits;
	unsigned int vol_valid_bits;
};

enum sprd_hwdvfs_l3_state {
	HWDVFS_STATE_UNKNOWN,
	HWDVFS_STATE_RUNNING,
	HWDVFS_STATE_DONE,
};

enum sprd_hwdvfs_l3_i2c_state {
	HWDVFS_I2C_UNLOCK,
	HWDVFS_I2C_LOCK,
};

enum sprd_hwdvfs_l3_dcdc_ctrl {
	DCDC_CTRL0,
	DCDC_CTRL1,
	DCDC_CTRL_MAX,
};

enum sprd_hwdvfs_l3_dcdc_data {
	DCDC_2731_VOL0,
	DCDC_2731_VOL1,
	DCDC_5355_VOL0,
	DCDC_2703_VOL0,
	DCDC_DATA_MAX,
};

static struct sprd_hwdvfs_l3_reg dcdc_data[DCDC_DATA_MAX] = {
	/* DCDC_2731_VOL0 */
	{900000, 3125, 400000, 1996000, 1000, REG_DCDC0_VOL_CTL,
	 DCDC0_VOL_CTL_MSK, 0, 0x3ff},
	 /* DCDC_2731_VOL1 */
	{900000, 3125, 400000, 1996000, 1000, REG_DCDC1_VOL_CTL,
	 DCDC1_VOL_CTL_MSK, 0, 0x3ff},
	 /* DCDC_5355_VOL0 */
	{603000 + 602822, 12826, 603000, 1411000, 1000,
	 REG_DCDC0_VOL_CTL_FAN5355,
	 DCDC0_VOL_CTL_MSK_FAN5355, 0x80, 0x3f},
	  /* DCDC_2703_VOL0 */
	{300000 + 10000*0x46, 10000, 300000, 1570000, 1000,
	 0x03, 0x7f, 0x00, 0x7f},
};

#define SPRD_HWDVFS_MAX_FREQ_VOLT 8

#define SPRD_HWDVFS_BUSY_DURATOIN (2ul * HZ)

enum sprd_hwdvfs_l3_chnl {
	HWDVFS_CHNL00,
	HWDVFS_CHNL01,
	HWDVFS_CHNL02,
	HWDVFS_CHNL_MAX,
};

enum sprd_hwdvfs_l3_type {
	UNKNOWN_HWDVFS,
	SPRD_HWDVFS_SHARKL3,
	SPRD_HWDVFS_SHARKL3_3H10,
};

struct sprd_hwdvfs_l3_group {
	/* HZ */
	unsigned long freq;
	/* uV */
	unsigned long volt;
};

struct sprd_hwdvfs_l3_info {
	unsigned int type;
	unsigned int def_freq0;
	unsigned int def_freq1;
	unsigned int def_freq2;
};

static const struct sprd_hwdvfs_l3_info sprd_hwdvfs_l3_info_1h10 = {
	.type = SPRD_HWDVFS_SHARKL3,
	.def_freq0 = 2,
	.def_freq1 = 3,
	.def_freq2 = 3,
};

static const struct sprd_hwdvfs_l3_info sprd_hwdvfs_l3_info_3h10 = {
	.type = SPRD_HWDVFS_SHARKL3_3H10,
	.def_freq0 = 2,
	.def_freq1 = 3,
	.def_freq2 = 3,
};

struct sprd_hwdvfs_l3 {
	struct regmap *aon_apb_base;
	struct regmap *anlg_phy_g4_ctrl;
	void __iomem *base;
	const struct sprd_hwdvfs_l3_info *info;
	struct i2c_client *i2c_client;
	struct completion dvfs_done[HWDVFS_CHNL_MAX];
	unsigned int on_i2c[HWDVFS_CHNL_MAX];
	unsigned int dcdc_index[HWDVFS_CHNL_MAX];
	int irq;
	bool probed;
	bool ready;
	bool enabled;
	bool triggered[HWDVFS_CHNL_MAX];
	/* busy time */
	unsigned long busy[HWDVFS_CHNL_MAX];
	/* 0-cluster0, 1-cluster1, 2-suc */
	/* min freq is on freqvolt[0] */
	struct sprd_hwdvfs_l3_group
	    freqvolt[HWDVFS_CHNL_MAX][SPRD_HWDVFS_MAX_FREQ_VOLT];
	atomic_t state[HWDVFS_CHNL_MAX];
	int idx_max[HWDVFS_CHNL_MAX];
};


static struct sprd_hwdvfs_l3 *hwdvfs_l3;
static struct kobject *hwdvfs_l3_kobj;
static atomic_t hwdvfs_l3_suspend;
static void sprd_hwdvfs_l3_dump(bool forcedump);
static int sprd_hwdvfs_set_clst0(unsigned int scalecode00,
				 bool sync, bool force);
static int sprd_hwdvfs_set_clst1_scu(unsigned int scalecode01,
				     bool sync, bool force);

static const struct of_device_id sprd_hwdvfs_l3_of_match[] = {
	{
		 .compatible = "sprd,sharkl3-hwdvfs",
		 .data = (void *)&sprd_hwdvfs_l3_info_1h10,
	},
	{
		 .compatible = "sprd,sharkl3-hwdvfs-3h10",
		 .data = (void *)&sprd_hwdvfs_l3_info_3h10,
	},
};

static unsigned int dvfs_rd(unsigned int reg)
{
	if (hwdvfs_l3 == NULL || hwdvfs_l3->base == NULL)
		return 0;

	return readl_relaxed((void __iomem *)(reg + hwdvfs_l3->base));

}

static void dvfs_wr(unsigned int val, unsigned int reg)
{
	if (hwdvfs_l3 == NULL || hwdvfs_l3->base == NULL)
		return;

	writel_relaxed(val, (void __iomem *)(reg + hwdvfs_l3->base));
}

static inline unsigned int sprd_val2reg_dcdcx_vtune(unsigned int to_uv,
						    unsigned int idx)
{
	unsigned int valreg;

	valreg = DIV_ROUND_UP(to_uv - dcdc_data[idx].min_uv,
			      dcdc_data[idx].step_uv);
	valreg |= dcdc_data[idx].vol_const_bits;
	valreg = VAL2REG(valreg, SCALE_TAB_VTUNE_MSK);

	return valreg;
}

static inline unsigned int sprd_wr_ctrl_hold_pause(unsigned int ctrl,
						   unsigned int hold,
						   unsigned int pause)
{
	switch (ctrl) {
	case 0:
		dvfs_wr(BIT_DVFS_CTRL_HOLD_VAL_CORE00(hold) |
			BIT_DVFS_CTRL_PAUSE_VAL_CORE00(pause),
			REG_DVFS_CTRL_TMR_CTRL1_CORE00);
		break;
	case 1:
		dvfs_wr(BIT_DVFS_CTRL_HOLD_VAL_CORE01(hold) |
			BIT_DVFS_CTRL_PAUSE_VAL_CORE01(pause),
			REG_DVFS_CTRL_TMR_CTRL1_CORE01);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline unsigned int sprd_wr_ctrl_timeout_stable(unsigned int ctrl,
						       unsigned int timeout,
						       unsigned int stable)
{
	switch (ctrl) {
	case 0:
		dvfs_wr(BIT_DVFS_CTRL_TO_VAL_CORE00(timeout) |
			BIT_DVFS_CTRL_DCDC_STABLE_VAL_CORE00(stable),
			REG_DVFS_CTRL_TMR_CTRL2_CORE00);
		break;
	case 1:
		dvfs_wr(BIT_DVFS_CTRL_TO_VAL_CORE01(timeout) |
			BIT_DVFS_CTRL_DCDC_STABLE_VAL_CORE01(stable),
			REG_DVFS_CTRL_TMR_CTRL2_CORE01);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline unsigned int sprd_wr_ctrl_step(unsigned int ctrl,
					     unsigned int chnl,
					     unsigned int step_uv)
{
	unsigned int dcdc_index;

	dcdc_index = hwdvfs_l3->dcdc_index[chnl];

	switch (ctrl) {
	case 0:
		if (step_uv >= 1200000U)
			step_uv = BIT_DVFS_CTRL_VTUNE_STEP_VAL_CORE00(0x3ff);
		else
			step_uv = step_uv/dcdc_data[dcdc_index].step_uv;

		dvfs_wr(VAL2REG(0, BIT_DVFS_CTRL_VTUNE_STEP_FAST_CORE00) |
			BIT_DVFS_CTRL_VTUNE_STEP_VAL_CORE00(step_uv),
			REG_DVFS_CTRL_VTUNE_STEP_CORE00);
		break;
	case 1:
		if (step_uv >= 1200000U)
			step_uv = BIT_DVFS_CTRL_VTUNE_STEP_VAL_CORE01(0x3ff);
		else
			step_uv = step_uv/dcdc_data[dcdc_index].step_uv;

		dvfs_wr(VAL2REG(0, BIT_DVFS_CTRL_VTUNE_STEP_FAST_CORE01) |
			BIT_DVFS_CTRL_VTUNE_STEP_VAL_CORE01(step_uv),
			REG_DVFS_CTRL_VTUNE_STEP_CORE01);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline unsigned int sprd_wr_ctrl_vtune_valid_bits(unsigned int ctrl,
							 unsigned int dcdc_idx)
{
	unsigned int regval;
	unsigned int valid_bits;

	regval = dvfs_rd(REG_DVFS_CTRL_VOLT_VALID_BIT);
	valid_bits = dcdc_data[dcdc_idx].vol_valid_bits;

	switch (ctrl) {
	case 0:
		regval &= ~BIT_DVFS_CTRL_VTUNE_VLD_BIT_CORE00(0x3ff);
		regval |= BIT_DVFS_CTRL_VTUNE_VLD_BIT_CORE00(valid_bits);
		break;
	case 1:
		regval &= ~BIT_DVFS_CTRL_VTUNE_VLD_BIT_CORE01(0x3ff);
		regval |= BIT_DVFS_CTRL_VTUNE_VLD_BIT_CORE01(valid_bits);
		break;
	default:
		return -EINVAL;
	}

	dvfs_wr(regval, REG_DVFS_CTRL_VOLT_VALID_BIT);

	return 0;
}

static unsigned int hwdvfs_mpll_table(unsigned long freq_khz)
{
	unsigned int mpll_hw = 0x0000082e;
	unsigned long vco_khz;
	unsigned long ibias_reserved;
	unsigned long postdiv;

	if (freq_khz < 400000UL) {
		return mpll_hw;
	}

	postdiv = (freq_khz < 800000UL) ? 1 : 0;
	vco_khz = freq_khz * (postdiv + 1);

	if (vco_khz > 2000000UL) {
		return mpll_hw;
	}

	if (vco_khz < 1000000UL)
		ibias_reserved = 0x0;
	else if (vco_khz < 1200000UL)
		ibias_reserved = 0x1;
	else if (vco_khz < 1400000UL)
		ibias_reserved = 0x2;
	else if (vco_khz < 1600000UL)
		ibias_reserved = 0x3;
	else if (vco_khz < 1800000UL)
		ibias_reserved = 0x4;
	else
		ibias_reserved = 0x5;

	mpll_hw = VAL2REG_IBIAS_RESERVED(ibias_reserved) |
		VAL2REG_POSTDIV(postdiv) |
		VAL2REG_KN_HW(vco_khz);

	return mpll_hw;
}

static inline int hwdvfs_mpll_write(unsigned int cluster,
				    unsigned int idx_freq,
				    unsigned long hz_freq)
{
	unsigned int reg = REG_ANLG_PHY_G4_ANALOG_MPLL_THM_TOP_MPLL0_DVFS_0;

	reg = reg + (cluster * MPLL_HW_DVFS_EACH_NUM + idx_freq) * 0x4;

	return regmap_write(hwdvfs_l3->anlg_phy_g4_ctrl,
			    reg,
			    hwdvfs_mpll_table(hz_freq / 1000UL));
}

static ssize_t hwdvfs_enable_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int ret;

	if (hwdvfs_l3 == NULL)
		return sprintf(buf, "NULL\n");
	else {
		ret = (hwdvfs_l3->probed ?
		       sprintf(buf, "1\n") :
		       sprintf(buf, "0\n"));

		pr_debug("state:[%d %d %d]\n",
			 atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL00]),
			 atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL01]),
			 atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL02]));
	}

	return ret;
}

static ssize_t hwdvfs_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t n)
{
	unsigned int en;

	if (n != 2 || hwdvfs_l3 == NULL)
		return -EINVAL;

	if (kstrtouint(buf, 0, &en))
		return -EINVAL;

	/*  TODO: need to enable magic number and add spinlock here */
	switch (en) {
	case 0:
		if (!sprd_hwdvfs_set_clst0(hwdvfs_l3->info->def_freq0,
					   true, true) &&
		    !sprd_hwdvfs_set_clst1_scu(hwdvfs_l3->info->def_freq1,
					       true, true)) {
			hwdvfs_l3->probed = false;
			dvfs_wr(DVFS_CTRL_MAGIC_NUM_UNLOCK,
				REG_DVFS_CTRL_MAGIC_NUM);
			dvfs_wr(VAL2REG(0x0, BIT_DVFS_CTRL_HW_DVFS_SEL),
				REG_DVFS_CTRL_HW_DVFS_SEL);
			pr_debug("DISABLE HWDVFS!\n");
		}
		break;
	case 1:
		dvfs_wr(DVFS_CTRL_MAGIC_NUM_LOCK, REG_DVFS_CTRL_MAGIC_NUM);
		dvfs_wr(VAL2REG(0x1, BIT_DVFS_CTRL_HW_DVFS_SEL),
			REG_DVFS_CTRL_HW_DVFS_SEL);
		hwdvfs_l3->probed = true;
		sprd_hwdvfs_set_clst0(hwdvfs_l3->info->def_freq0,
				      true, true);
		sprd_hwdvfs_set_clst1_scu(hwdvfs_l3->info->def_freq1,
					  true, true);
		pr_debug("ENABLE HWDVFS!\n");
		break;
	default:
		return -EINVAL;
	}

	return n;
}


DEVICE_ATTR_RW(hwdvfs_enable);

static struct attribute *hwdvfs_enable_attrs[] = {
	&dev_attr_hwdvfs_enable.attr, NULL,
};

ATTRIBUTE_GROUPS(hwdvfs_enable);

static void sprd_hwdvfs_l3_dump(bool forcedump)
{
	unsigned int i;
	static int prt_cnt;

	/* dump twice */
	if (!forcedump && prt_cnt > 3)
		return;

	prt_cnt++;
	for (i = REG_DVFS_CTRL_BASE; i < REG_DVFS_CTRL_CHNL02_SCALE07; i += 32)
		pr_info("0x%03x: %08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
			i,
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 0),
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 4),
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 8),
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 12),
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 16),
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 20),
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 24),
			(unsigned int)dvfs_rd(REG_DVFS_CTRL_BASE + i + 28));
}

static int sprd_hwdvfs_l3_i2c_clr_nack(int chnl)
{
	int ret;
	unsigned char start = 0x00;
	unsigned char buf[2] = {0x00, 0x00};
	struct i2c_msg msg[2];

	if (hwdvfs_l3 == NULL || chnl >= HWDVFS_CHNL_MAX)
		return -ENODEV;

	if (!(hwdvfs_l3->on_i2c[chnl] &&
	      hwdvfs_l3->i2c_client != NULL))
		return 0;

	msg[0].addr = hwdvfs_l3->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start;

	msg[1].addr = hwdvfs_l3->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = __i2c_transfer(hwdvfs_l3->i2c_client->adapter, &msg[0], 2);

	pr_err("hwdvfs_l3_i2c_clr_nack ret %d recv 0x%x", ret, buf[0]);

	return ret < 0 ? ret : 0;
}

static int sprd_hwdvfs_l3_i2c_trylock(struct i2c_adapter *adapter)
{
	struct i2c_adapter *parent = i2c_parent_is_i2c_adapter(adapter);

	if (parent)
		return sprd_hwdvfs_l3_i2c_trylock(parent);
	else
		return rt_mutex_trylock(&adapter->bus_lock);
}

static int sprd_hwdvfs_l3_completing(unsigned int cluster)
{
	if (!wait_for_completion_timeout(&(hwdvfs_l3->dvfs_done[cluster]),
					 msecs_to_jiffies(2000)))
		pr_err("CHNL%u gets dvfs timeout\n", cluster);

	if (hwdvfs_l3->on_i2c[cluster] &&
	    hwdvfs_l3->i2c_client != NULL) {
		i2c_unlock_adapter(hwdvfs_l3->i2c_client->adapter);
		pr_debug("CHNL%u DONE i2c_unlock_adapter\n", cluster);
	}

	return 0;
}

static int sprd_hwdvfs_l3_complete(unsigned int cluster)
{
	if (atomic_read(&hwdvfs_l3->state[cluster]) ==
	    HWDVFS_STATE_RUNNING) {
		complete(&hwdvfs_l3->dvfs_done[cluster]);
		pr_debug("CHNL%u i2c_unlock_async\n", cluster);
	}
	return 0;
}

static int sprd_hwdvfs_l3_try_lock(unsigned int cluster)
{
	int ret = 0;

	if (atomic_read(&hwdvfs_l3->state[cluster]) !=
	    HWDVFS_STATE_RUNNING) {
		if (hwdvfs_l3->on_i2c[cluster] &&
		    hwdvfs_l3->i2c_client != NULL) {
			if (sprd_hwdvfs_l3_i2c_trylock(
			    hwdvfs_l3->i2c_client->adapter))
				pr_debug("CHNL%d i2c_lock_adapter\n", cluster);
			else {
				pr_warn("CHNL%d i2c_lock_adapter fail\n",
					cluster);
				return -EBUSY;
			}
		}
		atomic_set(&hwdvfs_l3->state[cluster],
			   HWDVFS_STATE_RUNNING);
	} else {
		ret = -EBUSY;
	}

	return ret;
}

static int sprd_hwdvfs_l3_try_lock_clst1(void)
{
	int ret = 0;

	if (atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL01]) !=
	    HWDVFS_STATE_RUNNING &&
	    atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL02]) !=
	    HWDVFS_STATE_RUNNING) {
		if (hwdvfs_l3->on_i2c[HWDVFS_CHNL01] &&
		    hwdvfs_l3->i2c_client != NULL) {
			if (sprd_hwdvfs_l3_i2c_trylock(
			    hwdvfs_l3->i2c_client->adapter))
				pr_debug("CHNL1 i2c_lock_adapter\n");
			else {
				pr_warn("CHNL1 i2c_lock_adapter fail\n");
				return -EBUSY;
			}
		}
		atomic_set(&hwdvfs_l3->state[HWDVFS_CHNL01],
			   HWDVFS_STATE_RUNNING);
		atomic_set(&hwdvfs_l3->state[HWDVFS_CHNL02],
			   HWDVFS_STATE_RUNNING);
	} else {
		ret = -EBUSY;
	}

	return ret;
}

static void sprd_hwdvfs_l3_unlock(unsigned int cluster, bool complete)
{
	if (atomic_read(&hwdvfs_l3->state[cluster]) !=
	    HWDVFS_STATE_RUNNING) {
		pr_err("ERROR! CHNL%u get multiple DVFS INT\n", cluster);
		sprd_hwdvfs_l3_dump(false);
	}

	if (complete)
		sprd_hwdvfs_l3_complete(cluster);
	hwdvfs_l3->busy[cluster] = 0;
	atomic_set(&hwdvfs_l3->state[cluster],
		   HWDVFS_STATE_DONE);
}

/**
 * sprd_hwdvfs_l3_parse_dt - init hw dvfs common registers
 */
static int sprd_hwdvfs_l3_parse_dt(struct device_node *np)
{
	int ret;
	unsigned int regval;
	unsigned int cfg[4];
	struct device_node *np0, *np1;
	unsigned int chnl = 0, ctrl;

	if (hwdvfs_l3 == NULL  || np == NULL)
		return -ENODEV;

	for_each_child_of_node(np, np0) {
		np1 = of_parse_phandle(np0, "dcdc-ctrl", 0);
		if (np1 == NULL) {
			ret = -ENODEV;
			goto exit_np0;
		}

		/* configure DCDC MAP */
		ret = of_property_read_u32_index(np1, "reg", 0, &cfg[0]);
		if (ret || cfg[0] >= DCDC_CTRL_MAX)
			goto exit_np1;
		ctrl = cfg[0];
		regval = dvfs_rd(REG_DVFS_CTRL_CHNL_CORE_MAP);
		regval |= cfg[0] << chnl;
		dvfs_wr(regval, REG_DVFS_CTRL_CHNL_CORE_MAP);

		/* get dcdc reg index */
		ret = of_property_read_u32_index(np1, "reg", 1, &cfg[0]);
		if (ret || cfg[0] >= DCDC_DATA_MAX)
			goto exit_np1;
		hwdvfs_l3->dcdc_index[chnl] = cfg[0];

		/* configure valid bits */
		sprd_wr_ctrl_vtune_valid_bits(ctrl, cfg[0]);

		/* configure chnl in i2c */
		hwdvfs_l3->on_i2c[chnl] = of_property_read_bool(np1,
								"volt-via-i2c");

		/* configure hold pause time */
		ret = of_property_read_u32(np1, "volt-hold-us", &cfg[0]);
		if (ret)
			goto exit_np1;
		ret = of_property_read_u32(np1, "volt-pause-us", &cfg[1]);
		if (ret)
			goto exit_np1;
		sprd_wr_ctrl_hold_pause(ctrl, cfg[0], cfg[1]);

		/* configure timeout stable time */
		ret = of_property_read_u32(np1, "volt-timeout-us", &cfg[0]);
		if (ret)
			goto exit_np1;
		ret = of_property_read_u32(np1, "volt-stable-us", &cfg[1]);
		if (ret)
			goto exit_np1;
		sprd_wr_ctrl_timeout_stable(ctrl, cfg[0], cfg[1]);

		/* configure max step */
		ret = of_property_read_u32(np1, "volt-max-step-microvolt",
					   &cfg[0]);
		if (ret)
			goto exit_np1;
		sprd_wr_ctrl_step(ctrl, chnl, cfg[0]);

		of_node_put(np1);
		of_node_put(np0);

		if (++chnl >= HWDVFS_CHNL_MAX)
			break;
	}

	return 0;

exit_np1:
	of_node_put(np1);
exit_np0:
	of_node_put(np0);
	pr_err("%s fail!\n", __func__);

	return ret;
}

/**
 * sprd_hwdvfs_l3_init_param - init hw dvfs common registers
 */
static int sprd_hwdvfs_l3_init_param(struct device_node *np)
{
	struct regmap *aon_apb;
	unsigned int regval;
	int ret;

	if (hwdvfs_l3 == NULL)
		return -ENODEV;

	aon_apb = hwdvfs_l3->aon_apb_base;

	/* enable  HW dvfs */
	ret = regmap_update_bits(aon_apb,
				 REG_AON_APB_APB_EB4, MASK_AON_APB_DVFS_EB,
				 MASK_AON_APB_DVFS_EB);
	if (ret)
		return ret;
	/* Reset  HW dvfs */
	ret = regmap_update_bits(aon_apb, REG_AON_APB_APB_RST4,
				 MASK_AON_APB_DVFS_SOFT_RST,
				 MASK_AON_APB_DVFS_SOFT_RST);
	if (ret)
		return ret;
	/*  wait for dvfs reset */
	udelay(50);
	ret = regmap_update_bits(aon_apb, REG_AON_APB_APB_RST4,
				 MASK_AON_APB_DVFS_SOFT_RST,
				 (unsigned int)(~MASK_AON_APB_DVFS_SOFT_RST));
	if (ret)
		return ret;

	/* not select HW DVFS at first */
	dvfs_wr(VAL2REG(0x0, BIT_DVFS_CTRL_HW_DVFS_SEL),
		REG_DVFS_CTRL_HW_DVFS_SEL);

	regval = dvfs_rd(REG_DVFS_CTRL_VERSION);
	pr_debug("Project Name:0x%x-0x%x;Version:0x%x-0x%x\n",
		(char)(REG2VAL(regval, BIT_DVFS_CTRL_PROJ_NAME1)),
		(char)(REG2VAL(regval, BIT_DVFS_CTRL_PROJ_NAME0)),
		(REG2VAL(regval, BIT_DVFS_CTRL_VERSION1)),
		(REG2VAL(regval, BIT_DVFS_CTRL_VERSION0)));

	/* Debug_sel: 0x1b or 0x1d */
	dvfs_wr(BIT_DVFS_CTRL_DEBUG_SEL(0x1b) |
		VAL2REG(0x1, BIT_DVFS_CTRL_TO_CHECK_EN) |
		VAL2REG(0x0, BIT_DVFS_CTRL_AUTO_GATE_EN) |
		VAL2REG(0x0, BIT_DVFS_CTRL_SMART_MODE),
		REG_DVFS_CTRL_GLB_CFG);


	/* configure TUNE_EN */
	dvfs_wr(BIT_DVFS_CTRL_VTUNE_EN(0x3) |
		BIT_DVFS_CTRL_DCDC_EN_DTCT_EN(0x0) |
		BIT_DVFS_CTRL_FCFG_TUNE_EN(0x7),
		REG_DVFS_CTRL_TUNE_EN);

	/* configure IRQ */
	dvfs_wr(VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_VREAD_MIS_EN_CHNL00) |
		VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_TO_EN_CHNL00) |
		VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_DONE_EN_CHNL00),
		REG_DVFS_CTRL_IRQ_EN_CHNL00);
	dvfs_wr(VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_VREAD_MIS_EN_CHNL01) |
		VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_TO_EN_CHNL01) |
		VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_DONE_EN_CHNL01),
		REG_DVFS_CTRL_IRQ_EN_CHNL01);
	dvfs_wr(VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_VREAD_MIS_EN_CHNL02) |
		VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_TO_EN_CHNL02) |
		VAL2REG(0x1, BIT_DVFS_CTRL_IRQ_DONE_EN_CHNL02),
		REG_DVFS_CTRL_IRQ_EN_CHNL02);

	/* configure timer, stable time... */
	dvfs_wr(BIT_DVFS_CTRL_TMR_PRESCALE(TMR_PRESCALE_1US),
		REG_DVFS_CTRL_TMR_PRESCALE);

	dvfs_wr(BIT_DVFS_CTRL_PLL_PD_VAL(TMR_CTRL_PLL_PD_US) |
		BIT_DVFS_CTRL_PLL_STABLE_VAL(TMR_CTRL_PLL_STABLE_US),
		REG_DVFS_CTRL_TMR_CTRL_PLL);
	dvfs_wr(BIT_DVFS_CTRL_FMUX_STABLE_VAL(TMR_CTRL_FMUX_US),
		REG_DVFS_CTRL_TMR_CTRL_FMUX);

	regval = hwdvfs_l3->info->def_freq0 ? 0x06 : 0x02;
	dvfs_wr(VAL2REG(0x00, BIT_DVFS_CTRL_FCFG_PD_SW_CHNL00) |
		BIT_DVFS_CTRL_FSEL_PARK_CHNL00(0x02) |
		BIT_DVFS_CTRL_FSEL_BKP_CHNL00(0x02) |
		BIT_DVFS_CTRL_FSEL_SW_CHNL00(regval),
		REG_DVFS_CTRL_CFG_CHNL00);

	regval = hwdvfs_l3->info->def_freq1 ? 0x07 : 0x02;
	dvfs_wr(VAL2REG(0x00, BIT_DVFS_CTRL_FCFG_PD_SW_CHNL01) |
		BIT_DVFS_CTRL_FSEL_PARK_CHNL01(0x02) |
		BIT_DVFS_CTRL_FSEL_BKP_CHNL01(0x02) |
		BIT_DVFS_CTRL_FSEL_SW_CHNL01(regval),
		REG_DVFS_CTRL_CFG_CHNL01);

	regval = hwdvfs_l3->info->def_freq2 ? 0x05 : 0x02;
	dvfs_wr(VAL2REG(0x00, BIT_DVFS_CTRL_FCFG_PD_SW_CHNL02) |
		BIT_DVFS_CTRL_FSEL_PARK_CHNL02(0x02) |
		BIT_DVFS_CTRL_FSEL_BKP_CHNL02(0x02) |
		BIT_DVFS_CTRL_FSEL_SW_CHNL02(regval),
		REG_DVFS_CTRL_CFG_CHNL02);


	return  sprd_hwdvfs_l3_parse_dt(np);
}

static int sprd_hwdvfs_set_clst0(unsigned int scalecode00,
				 bool sync, bool force)
{
	unsigned int regval, i = 0;
	const unsigned int RETRY_MAX = 150;
	unsigned int scalecodeing;

	if (scalecode00 >= SCALE_TAB_EACH_NUM)
		return -EINVAL;

	if (!hwdvfs_l3->enabled)
		return -ENODEV;

	regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL00);
	scalecodeing = dvfs_rd(REG_DVFS_CTRL_SW_TRG_CHNL00) &
		BIT_DVFS_CTRL_SW_SCL_CHNL00(0xf);

	if (scalecodeing == scalecode00 &&
	    hwdvfs_l3->triggered[HWDVFS_CHNL00] &&
	    !force) {
		pr_debug("CHNL0 reject same idx-%u int-%u\n", scalecode00,
			 REG2VAL(regval, BIT_DVFS_CTRL_IRQ_DONE_CHNL00));
		return 0;
	}

	if (sprd_hwdvfs_l3_try_lock(HWDVFS_CHNL00)) {
		if (hwdvfs_l3->busy[HWDVFS_CHNL00] == 0)
			pr_debug("CHNL0 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL00(0xf)),
				 scalecodeing,
				 scalecode00);
		else
			pr_debug("CHNL0 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL00(0xf)),
				 scalecodeing,
				 scalecode00);
		if (hwdvfs_l3->busy[HWDVFS_CHNL00] == 0)
			hwdvfs_l3->busy[HWDVFS_CHNL00] =
				jiffies + SPRD_HWDVFS_BUSY_DURATOIN;
		if (hwdvfs_l3->busy[HWDVFS_CHNL00] &&
		    time_after(jiffies,
			       hwdvfs_l3->busy[HWDVFS_CHNL00])) {
			pr_warn("CHNL0 busy expired! cur-%u ing-%u to-%u\n",
				REG2VAL(regval,
					BIT_DVFS_CTRL_DONE_SCL_CHNL00(0xf)),
				scalecodeing,
				scalecode00);
			sprd_hwdvfs_l3_unlock(HWDVFS_CHNL00, false);
		}
		return 0;
	}

	if (force)
		pr_debug("CHNL0 idx %d START\n", scalecode00);

	dvfs_wr(regval |
		BIT_DVFS_CTRL_CONFLICT1_CHNL00 |
		BIT_DVFS_CTRL_CONFLICT0_CHNL00 |
		BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL00 |
		BIT_DVFS_CTRL_IRQ_TO_CHNL00 |
		BIT_DVFS_CTRL_IRQ_DONE_CHNL00,
		REG_DVFS_CTRL_STS_CHNL00);

	reinit_completion(&hwdvfs_l3->dvfs_done[HWDVFS_CHNL00]);

	dvfs_wr(BIT_DVFS_CTRL_SW_TRG_CHNL00 |
		BIT_DVFS_CTRL_SW_SCL_CHNL00(scalecode00),
		REG_DVFS_CTRL_SW_TRG_CHNL00);

	hwdvfs_l3->triggered[HWDVFS_CHNL00] = true;
	sprd_hwdvfs_l3_completing(HWDVFS_CHNL00);

	if (!sync)
		return 0;

	while (i++ < RETRY_MAX &&
	       atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL00]) !=
	       HWDVFS_STATE_DONE)
		udelay(100);

	if (i >= RETRY_MAX) {
		regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL00);
		pr_debug("CHN0=%d fail (0x%x), DONE=0x%x,SCALE=0x%x\n",
			scalecode00, regval,
			REG2VAL(regval, BIT_DVFS_CTRL_IRQ_DONE_CHNL00),
			REG2VAL(regval,
			BIT_DVFS_CTRL_DONE_SCL_CHNL00(0xf)));
		return -EBUSY;
	}
	pr_debug("index(%d)retry(%d)ok\n", scalecode00, i);

	return 0;
}

static int sprd_hwdvfs_set_clst1(unsigned int scalecode01,
				 bool sync, bool force)
{
	unsigned int regval, i = 0;
	const unsigned int RETRY_MAX = 100;
	unsigned int scalecodeing;

	if (scalecode01 >= SCALE_TAB_EACH_NUM)
		return -EINVAL;

	if (!hwdvfs_l3->enabled)
		return -ENODEV;

	regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL01);
	scalecodeing = dvfs_rd(REG_DVFS_CTRL_SW_TRG_CHNL01) &
		BIT_DVFS_CTRL_SW_SCL_CHNL01(0xf);

	if (scalecodeing == scalecode01 &&
	    hwdvfs_l3->triggered[HWDVFS_CHNL01] &&
	    !force) {
		pr_debug("CHNL1 reject same idx-%u int-%u\n", scalecode01,
			 REG2VAL(regval, BIT_DVFS_CTRL_IRQ_DONE_CHNL01));
		return 0;
	}

	if (sprd_hwdvfs_l3_try_lock(HWDVFS_CHNL01)) {
		if (hwdvfs_l3->busy[HWDVFS_CHNL01] == 0)
			pr_debug("CHNL1 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)),
				 scalecodeing,
				 scalecode01);
		else
			pr_debug("CHNL1 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)),
				 scalecodeing,
				 scalecode01);
		if (hwdvfs_l3->busy[HWDVFS_CHNL01] == 0)
			hwdvfs_l3->busy[HWDVFS_CHNL01] =
			    jiffies + SPRD_HWDVFS_BUSY_DURATOIN;
		if (hwdvfs_l3->busy[HWDVFS_CHNL01] &&
		    time_after(jiffies,
			       hwdvfs_l3->busy[HWDVFS_CHNL01])) {
			pr_warn("CHNL1 busy expired! cur-%u ing-%u to-%u\n",
				REG2VAL(regval,
					BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)),
				scalecodeing,
				scalecode01);
			sprd_hwdvfs_l3_unlock(HWDVFS_CHNL01, false);
		}
		return 0;
	}

	if (force)
		pr_debug("do index %d\n", scalecode01);

	dvfs_wr(regval |
		BIT_DVFS_CTRL_CONFLICT1_CHNL01 |
		BIT_DVFS_CTRL_CONFLICT0_CHNL01 |
		BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL01 |
		BIT_DVFS_CTRL_IRQ_TO_CHNL01 |
		BIT_DVFS_CTRL_IRQ_DONE_CHNL01,
		REG_DVFS_CTRL_STS_CHNL01);

	reinit_completion(&hwdvfs_l3->dvfs_done[HWDVFS_CHNL01]);

	dvfs_wr(BIT_DVFS_CTRL_SW_TRG_CHNL01 |
		BIT_DVFS_CTRL_SW_SCL_CHNL01(scalecode01),
		REG_DVFS_CTRL_SW_TRG_CHNL01);

	hwdvfs_l3->triggered[HWDVFS_CHNL01] = true;

	sprd_hwdvfs_l3_completing(HWDVFS_CHNL01);

	if (!sync)
		return 0;

	while (i++ < RETRY_MAX &&
	       atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL01]) !=
	       HWDVFS_STATE_DONE)
		udelay(100);

	if (i >= RETRY_MAX) {
		regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL01);
		pr_debug("CHN1=%d fail (0x%x), DONE=0x%x,SCALE=0x%x\n",
			 scalecode01, regval,
			 REG2VAL(regval, BIT_DVFS_CTRL_IRQ_DONE_CHNL01),
			 REG2VAL(regval,
			 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)));
		return -EBUSY;
	}
	pr_debug("index(%d)retry(%d)ok\n", scalecode01, i);

	return 0;
}

static int sprd_hwdvfs_set_scu(unsigned int scalecode02, bool sync, bool force)
{
	unsigned int regval, i = 0;
	const unsigned int RETRY_MAX = 50;
	unsigned int scalecodeing;

	if (scalecode02 >= SCALE_TAB_EACH_NUM)
		return -EINVAL;

	if (!hwdvfs_l3->enabled)
		return -ENODEV;

	regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL02);
	scalecodeing = dvfs_rd(REG_DVFS_CTRL_SW_TRG_CHNL02) &
		BIT_DVFS_CTRL_SW_SCL_CHNL02(0xf);

	if (scalecodeing == scalecode02 &&
	    hwdvfs_l3->triggered[HWDVFS_CHNL02] &&
	    !force) {
		pr_debug("CHNL2 reject same idx-%u int-%u\n", scalecode02,
			 REG2VAL(regval, BIT_DVFS_CTRL_IRQ_DONE_CHNL02));
		return 0;
	}

	if (sprd_hwdvfs_l3_try_lock(HWDVFS_CHNL02)) {
		if (hwdvfs_l3->busy[HWDVFS_CHNL02] == 0)
			pr_debug("CHNL2 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL02(0xf)),
				 scalecodeing,
				 scalecode02);
		else
			pr_debug("CHNL2 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL02(0xf)),
				 scalecodeing,
				 scalecode02);
		if (hwdvfs_l3->busy[HWDVFS_CHNL02] == 0)
			hwdvfs_l3->busy[HWDVFS_CHNL02] =
				jiffies + SPRD_HWDVFS_BUSY_DURATOIN;
		if (hwdvfs_l3->busy[HWDVFS_CHNL02] &&
		    time_after(jiffies,
			       hwdvfs_l3->busy[HWDVFS_CHNL02])) {
			pr_warn("CHNL2 busy expired! cur-%u ing-%u to-%u\n",
				REG2VAL(regval,
					BIT_DVFS_CTRL_DONE_SCL_CHNL02(0xf)),
				scalecodeing,
				scalecode02);
			sprd_hwdvfs_l3_unlock(HWDVFS_CHNL02, false);
		}
		return 0;
	}

	if (force)
		pr_debug("CHNL2 idx %d START\n", scalecode02);

	dvfs_wr(regval |
		BIT_DVFS_CTRL_CONFLICT1_CHNL02 |
		BIT_DVFS_CTRL_CONFLICT0_CHNL02 |
		BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL02 |
		BIT_DVFS_CTRL_IRQ_TO_CHNL02 |
		BIT_DVFS_CTRL_IRQ_DONE_CHNL02,
		REG_DVFS_CTRL_STS_CHNL02);

	reinit_completion(&hwdvfs_l3->dvfs_done[HWDVFS_CHNL02]);

	dvfs_wr(BIT_DVFS_CTRL_SW_TRG_CHNL02 |
		BIT_DVFS_CTRL_SW_SCL_CHNL02(scalecode02),
		REG_DVFS_CTRL_SW_TRG_CHNL02);

	hwdvfs_l3->triggered[HWDVFS_CHNL02] = true;
	sprd_hwdvfs_l3_completing(HWDVFS_CHNL02);

	if (!sync)
		return 0;

	while (i++ < RETRY_MAX &&
	       atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL02]) !=
	       HWDVFS_STATE_DONE)
		udelay(100);

	if (i >= RETRY_MAX) {
		regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL02);
		pr_debug("CHN2=%d fail (0x%x), DONE=0x%x,SCALE=0x%x\n",
			 scalecode02, regval,
			 REG2VAL(regval, BIT_DVFS_CTRL_IRQ_DONE_CHNL02),
			 REG2VAL(regval,
				 BIT_DVFS_CTRL_DONE_SCL_CHNL02(0xf)));
		return -EBUSY;
	}
	pr_debug("index(%d)retry(%d)ok\n", scalecode02, i);

	return 0;
}

static int sprd_hwdvfs_set_clst1_scu(unsigned int scalecode01, bool sync,
				     bool force)
{
	unsigned int regval1, regval2, i = 0;
	const unsigned int RETRY_MAX = 100;
	unsigned int scalecodeing;

	if (scalecode01 >= SCALE_TAB_EACH_NUM)
		return -EINVAL;

	if (!hwdvfs_l3->enabled)
		return -ENODEV;

	regval1 = dvfs_rd(REG_DVFS_CTRL_STS_CHNL01);
	regval2 = dvfs_rd(REG_DVFS_CTRL_STS_CHNL02);
	scalecodeing = dvfs_rd(REG_DVFS_CTRL_SW_TRG_CHNL01) &
		BIT_DVFS_CTRL_SW_SCL_CHNL01(0xf);


	if (scalecodeing == scalecode01 &&
	    hwdvfs_l3->triggered[HWDVFS_CHNL01] &&
	    !force) {
		pr_debug("CHNL1 CHNL2 reject same idx-%u int-%u\n", scalecode01,
			 REG2VAL(regval1, BIT_DVFS_CTRL_IRQ_DONE_CHNL01));
		return 0;
	}

	if (sprd_hwdvfs_l3_try_lock_clst1()) {
		if (hwdvfs_l3->busy[HWDVFS_CHNL01] == 0)
			pr_debug("CHNL1 CHNL2 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval1,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)),
				 scalecodeing,
				 scalecode01);
		else
			pr_debug("CHNL1 CHNL2 busy! cur-%u ing-%u to-%u\n",
				 REG2VAL(regval1,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)),
				 scalecodeing,
				 scalecode01);
		if (hwdvfs_l3->busy[HWDVFS_CHNL01] == 0)
			hwdvfs_l3->busy[HWDVFS_CHNL01] =
			    jiffies + SPRD_HWDVFS_BUSY_DURATOIN;
		if (hwdvfs_l3->busy[HWDVFS_CHNL01] &&
		    time_after(jiffies,
			       hwdvfs_l3->busy[HWDVFS_CHNL01])) {
			pr_warn("CHNL1 CHNL2 busy expired! cur-%u ing-%u to-%u\n",
				REG2VAL(regval1,
					BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)),
				scalecodeing,
				scalecode01);
			sprd_hwdvfs_l3_unlock(HWDVFS_CHNL01, false);
			sprd_hwdvfs_l3_unlock(HWDVFS_CHNL02, false);
		}
		return 0;
	}

	if (force)
		pr_debug("CHNL1 CHNL2 idx %d START\n", scalecode01);

	dvfs_wr(regval1 |
		BIT_DVFS_CTRL_CONFLICT1_CHNL01 |
		BIT_DVFS_CTRL_CONFLICT0_CHNL01 |
		BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL01 |
		BIT_DVFS_CTRL_IRQ_TO_CHNL01 |
		BIT_DVFS_CTRL_IRQ_DONE_CHNL01,
		REG_DVFS_CTRL_STS_CHNL01);
	dvfs_wr(regval2 |
		BIT_DVFS_CTRL_CONFLICT1_CHNL02 |
		BIT_DVFS_CTRL_CONFLICT0_CHNL02 |
		BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL02 |
		BIT_DVFS_CTRL_IRQ_TO_CHNL02 |
		BIT_DVFS_CTRL_IRQ_DONE_CHNL02,
		REG_DVFS_CTRL_STS_CHNL02);

	reinit_completion(&hwdvfs_l3->dvfs_done[HWDVFS_CHNL01]);

	dvfs_wr(BIT_DVFS_CTRL_SW_TRG_CHNL02 |
		BIT_DVFS_CTRL_SW_SCL_CHNL02(scalecode01),
		REG_DVFS_CTRL_SW_TRG_CHNL02);
	dvfs_wr(BIT_DVFS_CTRL_SW_TRG_CHNL01 |
		BIT_DVFS_CTRL_SW_SCL_CHNL01(scalecode01),
		REG_DVFS_CTRL_SW_TRG_CHNL01);

	hwdvfs_l3->triggered[HWDVFS_CHNL01] = true;
	hwdvfs_l3->triggered[HWDVFS_CHNL02] = true;

	sprd_hwdvfs_l3_completing(HWDVFS_CHNL01);

	if (!sync)
		return 0;

	while (i++ < RETRY_MAX &&
	       atomic_read(&hwdvfs_l3->state[HWDVFS_CHNL01]) !=
	       HWDVFS_STATE_DONE)
		udelay(100);

	if (i >= RETRY_MAX) {
		regval1 = dvfs_rd(REG_DVFS_CTRL_STS_CHNL01);
		pr_debug("CHN1=%d fail (0x%x), DONE=0x%x,SCALE=0x%x\n",
			 scalecode01,
			 regval1,
			 REG2VAL(regval1,
				 BIT_DVFS_CTRL_IRQ_DONE_CHNL01),
			 REG2VAL(regval1,
				 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)));
		return -EBUSY;
	}
	pr_debug("index(%d)retry(%d)ok\n", scalecode01, i);

	return 0;
}

static irqreturn_t sprd_hwdvfs_l3_isr(int irq, void *dev_id)
{
	unsigned int regval00, regval01, regval02;

	regval00 = dvfs_rd(REG_DVFS_CTRL_STS_CHNL00);
	regval01 = dvfs_rd(REG_DVFS_CTRL_STS_CHNL01);
	regval02 = dvfs_rd(REG_DVFS_CTRL_STS_CHNL02);

	if (regval00 & BIT_DVFS_CTRL_IRQ_DONE_CHNL00)
		pr_debug("CHNL0 0x%x DONE\n",
			 REG2VAL(regval00,
				 BIT_DVFS_CTRL_DONE_SCL_CHNL00(0xf)));

	if (regval01 & BIT_DVFS_CTRL_IRQ_DONE_CHNL01)
		pr_debug("CHNL1 0x%x DONE\n",
			 REG2VAL(regval01,
				 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)));

	if (regval02 & BIT_DVFS_CTRL_IRQ_DONE_CHNL02)
		pr_debug("CHNL2 0x%x DONE\n",
			 REG2VAL(regval02,
				 BIT_DVFS_CTRL_DONE_SCL_CHNL02(0xf)));

	if (regval00 & BIT_DVFS_CTRL_IRQ_TO_CHNL00)
		sprd_hwdvfs_l3_i2c_clr_nack(HWDVFS_CHNL00);

	if (regval01 & BIT_DVFS_CTRL_IRQ_TO_CHNL01)
		sprd_hwdvfs_l3_i2c_clr_nack(HWDVFS_CHNL01);

	if (regval02 & BIT_DVFS_CTRL_IRQ_TO_CHNL02)
		sprd_hwdvfs_l3_i2c_clr_nack(HWDVFS_CHNL02);

	/* CHNL00: ERROR INTERRUPT */
	if (regval00 & BIT_DVFS_CTRL_CONFLICT1_CHNL00)
		pr_warn("CHNL0 CONFLICT1\n");

	if (regval00 & BIT_DVFS_CTRL_CONFLICT0_CHNL00)
		pr_warn("CHNL0 CONFLICT0\n");

	if (regval00 & BIT_DVFS_CTRL_IRQ_TO_CHNL00)
		pr_warn("CHNL0 TIMEOUT\n");

	if (regval00 & BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL00)
		pr_warn("CHNL0 VREAD_MIS in 0x%x\n",
			REG2VAL(regval00,
				BIT_DVFS_CTRL_DONE_SCL_CHNL00(0xf)));
	if ((regval00 & BIT_DVFS_CTRL_IRQ_DONE_CHNL00) ||
	    (regval00 & BIT_DVFS_CTRL_CONFLICT1_CHNL00) ||
	    (regval00 & BIT_DVFS_CTRL_IRQ_TO_CHNL00))
		sprd_hwdvfs_l3_unlock(HWDVFS_CHNL00, true);

	/* CHNL01: ERROR INTERRUPT */
	if (regval01 & BIT_DVFS_CTRL_CONFLICT1_CHNL01)
		pr_warn("CHNL1 CONFLICT1\n");

	if (regval01 & BIT_DVFS_CTRL_CONFLICT0_CHNL01)
		pr_warn("CHNL1 CONFLICT0\n");

	if (regval01 & BIT_DVFS_CTRL_IRQ_TO_CHNL01)
		pr_warn("CHNL1 TIMEOUT\n");

	if (regval01 & BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL01)
		pr_warn("CHNL1 VREAD_MIS in 0x%x\n",
			REG2VAL(regval01,
				BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf)));
	if ((regval01 & BIT_DVFS_CTRL_IRQ_DONE_CHNL01) ||
	    (regval01 & BIT_DVFS_CTRL_CONFLICT1_CHNL01) ||
	    (regval01 & BIT_DVFS_CTRL_IRQ_TO_CHNL01))
		sprd_hwdvfs_l3_unlock(HWDVFS_CHNL01, true);

	/* CHNL02: ERROR INTERRUPT */
	if (regval02 & BIT_DVFS_CTRL_CONFLICT1_CHNL02)
		pr_warn("CHNL2 CONFLICT1\n");

	if (regval02 & BIT_DVFS_CTRL_CONFLICT0_CHNL02)
		pr_warn("CHNL2 CONFLICT0\n");

	if (regval02 & BIT_DVFS_CTRL_IRQ_TO_CHNL02)
		pr_warn("CHNL2 TIMEOUT\n");

	if (regval02 & BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL02)
		pr_warn("CHNL2 VREAD_MIS in 0x%x\n",
			REG2VAL(regval02,
				BIT_DVFS_CTRL_DONE_SCL_CHNL02(0xf)));
	if ((regval02 & BIT_DVFS_CTRL_IRQ_DONE_CHNL02) ||
	    (regval02 & BIT_DVFS_CTRL_CONFLICT1_CHNL02) ||
	    (regval02 & BIT_DVFS_CTRL_IRQ_TO_CHNL02))
		sprd_hwdvfs_l3_unlock(HWDVFS_CHNL02, true);

	/* Clear INT anyway, never wait for next success */
	dvfs_wr(regval00, REG_DVFS_CTRL_STS_CHNL00);
	dvfs_wr(regval01, REG_DVFS_CTRL_STS_CHNL01);
	dvfs_wr(regval02, REG_DVFS_CTRL_STS_CHNL02);

	return IRQ_HANDLED;
}
/**
 * sprd_hwdvfs_l3_table_store - store freq&volt table for cluster
 * @cluster: 0-cluster0, 1-cluster1, 3-scu
 *
 * This is the last known freq, without actually getting it from the driver.
 * Return value will be same as what is shown in scaling_cur_freq in sysfs.
 */
static int sprd_hwdvfs_l3_opp_add(void *drvdata, unsigned int cluster,
				  unsigned long hz_freq, unsigned long u_volt,
				  int idx_volt)
{
	int ret = 0;
	unsigned int idx_freq, index;

	if (hwdvfs_l3 == NULL || !hwdvfs_l3->probed)
		return -ENODEV;

	if (cluster > HWDVFS_CHNL_MAX)
		return -EINVAL;

	if (cluster == HWDVFS_CHNL_MAX)
		cluster = HWDVFS_CHNL02;

	if (atomic_read(&hwdvfs_l3->state[cluster]) ==
	    HWDVFS_STATE_RUNNING) {
		pr_err("cluster%d hwdvfs_l3_opp_add error!busy!\n", cluster);
		return -EBUSY;
	}

	idx_freq = (hz_freq <= 768000000UL ? 0 : (idx_volt - 1));
	switch (cluster) {
	case HWDVFS_CHNL00:
		index = hwdvfs_l3->dcdc_index[HWDVFS_CHNL00];
		dvfs_wr(HW_DVFS_TAB_CLUSTER0(hz_freq, u_volt, idx_freq, index),
			REG_DVFS_CTRL_CHNL00_SCALE00 +
			(cluster * SCALE_TAB_EACH_NUM + idx_volt) * 0x4);

		if (hz_freq > 768000000UL)
			hwdvfs_mpll_write(cluster, idx_freq, hz_freq);

		pr_debug("cluster%d hw_opp[freq-0x%x & volt-0x%x]\n",
			 cluster,
			 hwdvfs_mpll_table(hz_freq / 1000UL),
			 HW_DVFS_TAB_CLUSTER0(hz_freq, u_volt, idx_freq,
					      index));
		break;
	case HWDVFS_CHNL01:
		index = hwdvfs_l3->dcdc_index[HWDVFS_CHNL01];
		dvfs_wr(HW_DVFS_TAB_CLUSTER1(hz_freq, u_volt, idx_freq, index),
			REG_DVFS_CTRL_CHNL00_SCALE00 +
			(cluster * SCALE_TAB_EACH_NUM + idx_volt) * 0x4);

		if (hz_freq > 768000000UL)
			hwdvfs_mpll_write(cluster, idx_freq, hz_freq);

		pr_debug("cluster%d hw_opp[freq-0x%x & volt-0x%x]\n",
			 cluster,
			 hwdvfs_mpll_table(hz_freq / 1000UL),
			 HW_DVFS_TAB_CLUSTER1(hz_freq, u_volt, idx_freq,
					      index));
		break;
	case HWDVFS_CHNL02:
		index = hwdvfs_l3->dcdc_index[HWDVFS_CHNL02];
		dvfs_wr(HW_DVFS_TAB_CLUSTER2(hz_freq, u_volt, idx_freq, index),
			REG_DVFS_CTRL_CHNL00_SCALE00 +
			(cluster * SCALE_TAB_EACH_NUM + idx_volt) * 0x4);

		if (hz_freq > 768000000UL)
			hwdvfs_mpll_write(cluster, idx_freq, hz_freq);

		pr_debug("cluster%d hw_opp[freq-0x%x & volt-0x%x]\n",
			 cluster,
			 hwdvfs_mpll_table(hz_freq / 1000UL),
			 HW_DVFS_TAB_CLUSTER2(hz_freq, u_volt, idx_freq,
					      index));
		break;
	default:
		ret = -ENODEV;
		pr_warn("hwdvfs_l3_opp_add cluster %u error!\n", cluster);
		break;
	}

	if (ret == 0) {
		hwdvfs_l3->freqvolt[cluster][idx_volt].freq = hz_freq;
		hwdvfs_l3->freqvolt[cluster][idx_volt].volt = u_volt;
		if (idx_volt > hwdvfs_l3->idx_max[cluster])
			hwdvfs_l3->idx_max[cluster] = idx_volt;
	}

	return ret;
}

/*
 * @idx:        0 points to min freq, ascending order
 */
static int sprd_hwdvfs_l3_set_target(void *drvdata, u32 cluster, u32 idx_volt)
{
	int ret;

	if (hwdvfs_l3 == NULL || !hwdvfs_l3->probed)
		return -ENODEV;

	if (cluster == HWDVFS_CHNL_MAX)
		cluster = HWDVFS_CHNL02;

	if (cluster > HWDVFS_CHNL_MAX ||
	    idx_volt > hwdvfs_l3->idx_max[cluster]) {
		pr_debug("opp_add cluster%u idx%d error\n", cluster, idx_volt);
		return -EINVAL;
	}

	pr_debug("CHNL%u idx %u START\n", cluster, idx_volt);

	/* trigger freq&volt */
	switch (cluster) {
	case HWDVFS_CHNL00:
		ret = sprd_hwdvfs_set_clst0(idx_volt, false, false);
		break;
	case HWDVFS_CHNL01:
		ret = sprd_hwdvfs_set_clst1_scu(idx_volt, false, false);
		break;
	case HWDVFS_CHNL02:
		ret = 0;
		break;
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static unsigned int sprd_hwdvfs_l3_get(void *drvdata, int cluster)
{
	unsigned int regval = 0, reg = 0, freq_khz;

	if (hwdvfs_l3 == NULL || !hwdvfs_l3->probed)
		return -ENODEV;

	if (cluster > HWDVFS_CHNL_MAX)
		return -EINVAL;

	if (cluster == HWDVFS_CHNL_MAX)
		cluster = HWDVFS_CHNL02;

	if (hwdvfs_l3->enabled &&
	    hwdvfs_l3->triggered[cluster]) {
		switch (cluster) {
		case HWDVFS_CHNL00:
			regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL00);
			reg = regval;
			regval = REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL00(0xf));
			break;
		case HWDVFS_CHNL01:
			regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL01);
			reg = regval;
			regval = REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL01(0xf));
			break;
		case HWDVFS_CHNL02:
			regval = dvfs_rd(REG_DVFS_CTRL_STS_CHNL02);
			reg = regval;
			regval = REG2VAL(regval,
					 BIT_DVFS_CTRL_DONE_SCL_CHNL02(0xf));
			break;
		default:
			break;
		}
	} else {
		switch (cluster) {
		case HWDVFS_CHNL00:
			regval = hwdvfs_l3->info->def_freq0;
			break;
		case HWDVFS_CHNL01:
			regval = hwdvfs_l3->info->def_freq1;
			break;
		case HWDVFS_CHNL02:
			regval = hwdvfs_l3->info->def_freq2;
			break;
		default:
			break;
		}
	}

	if (regval > hwdvfs_l3->idx_max[cluster]) {
		switch (cluster) {
		case HWDVFS_CHNL00:
			regval = hwdvfs_l3->info->def_freq0;
			break;
		case HWDVFS_CHNL01:
			regval = hwdvfs_l3->info->def_freq1;
			break;
		case HWDVFS_CHNL02:
			regval = hwdvfs_l3->info->def_freq2;
			break;
		default:
			break;
		}
	}

	freq_khz = hwdvfs_l3->freqvolt[cluster][regval].freq / 1000;

	pr_debug("[%d,%ukhz,en-%d:%d, reg-0x%x, val-0x%x]\n",
		 cluster, freq_khz,
		 hwdvfs_l3->enabled,
		 hwdvfs_l3->triggered[cluster],
		 reg, regval);

	return freq_khz;
}

static bool sprd_hwdvfs_l3_enable(void *drvdata, int cluster, bool en)
{
	unsigned int regval0, regval1;

	if (hwdvfs_l3 == NULL || !hwdvfs_l3->probed)
		return false;

	if (cluster > HWDVFS_CHNL_MAX)
		return false;

	if (cluster == HWDVFS_CHNL_MAX)
		cluster = HWDVFS_CHNL02;

	pr_debug("cluster %u en %u\n", cluster, en);

	regval1 = dvfs_rd(REG_DVFS_CTRL_SW_CHNL_EN);
	if (en) {
		switch (cluster) {
		case HWDVFS_CHNL00:
			if (!(regval1 & SW_CHNL00_EN_MSK))
				dvfs_wr(regval1 |
					VAL2REG(0x1, SW_CHNL00_EN_MSK),
					REG_DVFS_CTRL_SW_CHNL_EN);
			pr_debug("SW_CHNL00_EN\n");
			break;
		case HWDVFS_CHNL01:
			if (!(regval1 & SW_CHNL01_EN_MSK))
				dvfs_wr(regval1 |
					VAL2REG(0x1, SW_CHNL01_EN_MSK),
					REG_DVFS_CTRL_SW_CHNL_EN);
			pr_debug("SW_CHNL01_EN\n");
			break;
		case HWDVFS_CHNL02:
			if (!(regval1 & SW_CHNL02_EN_MSK))
				dvfs_wr(regval1 |
					VAL2REG(0x1, SW_CHNL02_EN_MSK),
					REG_DVFS_CTRL_SW_CHNL_EN);
			pr_debug("SW_CHNL02_EN\n");
			break;
		default:
			pr_warn("%s cluster %u error!\n", __func__, cluster);
			break;
		}

		regval0 = dvfs_rd(REG_DVFS_CTRL_HW_DVFS_SEL);
		regval1 = dvfs_rd(REG_DVFS_CTRL_SW_CHNL_EN);
		if (!(regval0 & BIT_DVFS_CTRL_HW_DVFS_SEL) &&
		    (regval1 & SW_CHNL00_EN_MSK) &&
		    (regval1 & SW_CHNL01_EN_MSK) &&
		    (regval1 & SW_CHNL02_EN_MSK)) {
			dvfs_wr(DVFS_CTRL_MAGIC_NUM_LOCK,
				REG_DVFS_CTRL_MAGIC_NUM);
			dvfs_wr(VAL2REG(0x1, BIT_DVFS_CTRL_HW_DVFS_SEL),
				REG_DVFS_CTRL_HW_DVFS_SEL);
			hwdvfs_l3->enabled = true;
			sprd_hwdvfs_set_clst0(hwdvfs_l3->info->def_freq0,
					      true, true);
			sprd_hwdvfs_set_clst1_scu(hwdvfs_l3->info->def_freq1,
						  true, true);
			pr_info("ENABLE HWDVFS!\n");
		}
	} else if (atomic_read(&hwdvfs_l3_suspend) != 1) {
		switch (cluster) {
		case HWDVFS_CHNL00:
			if (!sprd_hwdvfs_set_clst0(0, false, false))
				pr_debug("SW_CHNL00_DISABLE\n");
			break;
		case HWDVFS_CHNL01:
			if (!sprd_hwdvfs_set_clst1(0, false, false))
				pr_debug("SW_CHNL01_DISABLE\n");
			break;
		case HWDVFS_CHNL02:
			if (!sprd_hwdvfs_set_scu(0, false, false))
				pr_debug("SW_CHNL02_DISABLE\n");
			break;
		default:
			pr_warn("%s cluster %u error!\n", __func__, cluster);
			break;
		}

		regval0 = dvfs_rd(REG_DVFS_CTRL_HW_DVFS_SEL);
		regval1 = dvfs_rd(REG_DVFS_CTRL_SW_CHNL_EN);
		if ((regval0 & BIT_DVFS_CTRL_HW_DVFS_SEL) &&
		    !(regval1 & SW_CHNL00_EN_MSK) &&
		    !(regval1 & SW_CHNL01_EN_MSK) &&
		    !(regval1 & SW_CHNL02_EN_MSK)) {
			dvfs_wr(DVFS_CTRL_MAGIC_NUM_UNLOCK,
				REG_DVFS_CTRL_MAGIC_NUM);
			dvfs_wr(VAL2REG(0x0, BIT_DVFS_CTRL_HW_DVFS_SEL),
				REG_DVFS_CTRL_HW_DVFS_SEL);
			hwdvfs_l3->enabled = false;
			pr_info("DISABLE HWDVFS!\n");
		}
	}

	return true;
}

static int sprd_hwdvfs_l3_i2c_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	hwdvfs_l3->i2c_client = client;

	return 0;
}

static int sprd_hwdvfs_l3_i2c_remove(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_PM
static int sprd_hwdvfs_l3_i2c_suspend(struct device *dev)
{
	return 0;
}

static int sprd_hwdvfs_l3_i2c_resume(struct device *dev)
{
	return 0;
}
static UNIVERSAL_DEV_PM_OPS(sprd_hwdvfs_l3_i2c_pm,
	sprd_hwdvfs_l3_i2c_suspend,
	sprd_hwdvfs_l3_i2c_resume,
	NULL);
#endif

static const struct i2c_device_id hwdvfs_l3_i2c_id[] = {
	{"hwdvfs_l3", 0},
	{}
};

static const struct of_device_id hwdvfs_l3_i2c_of_match[] = {
	{.compatible = "sprd,hwdvfs-regulator-sharkl3",},
	{}
};

static struct i2c_driver hwdvfs_l3_i2c_driver = {
	.driver = {
		.name = "hwdvfs_l3",
		.owner = THIS_MODULE,
		.of_match_table = hwdvfs_l3_i2c_of_match,
#ifdef CONFIG_PM
		.pm	= &sprd_hwdvfs_l3_i2c_pm,
#endif
	},
	.probe = sprd_hwdvfs_l3_i2c_probe,
	.remove = sprd_hwdvfs_l3_i2c_remove,
	.id_table = hwdvfs_l3_i2c_id,
};

static bool sprd_hwdvfs_l3_probed(void *drvdata, int cluster)
{
	if (hwdvfs_l3 == NULL || !hwdvfs_l3->probed)
		return false;

	if (cluster > HWDVFS_CHNL_MAX) {
		pr_warn("%s cluster%u  error!\n", __func__, cluster);
		return false;
	}

	return true;
}

static struct sprd_cpudvfs_device hwdvfs_l3_dev = {
	.name = "sprd-hwdvfs-l3-plat",
	.ops = {
		.probed = sprd_hwdvfs_l3_probed,
		.enable = sprd_hwdvfs_l3_enable,
		.opp_add = sprd_hwdvfs_l3_opp_add,
		.set = sprd_hwdvfs_l3_set_target,
		.get = sprd_hwdvfs_l3_get,
	},
};

static int sprd_hwdvfs_l3_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct regmap *aon_apb_base;
	struct regmap *anlg_phy_g4_ctrl_base;
	const struct sprd_hwdvfs_l3_info *pdata;
	void __iomem *base;
	int ret, i;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -ENODEV;
	}

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "get id of hwdvfs_l3 failed!\n");
		return -ENODEV;
	}

	aon_apb_base =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-enable");
	if (IS_ERR(aon_apb_base)) {
		dev_err(&pdev->dev, "hwdvfs_l3 syscon failed!\n");
		return PTR_ERR(aon_apb_base);
	}
	anlg_phy_g4_ctrl_base =
		syscon_regmap_lookup_by_phandle(np, "sprd,anlg-phy-g4-ctrl");
	if (IS_ERR(anlg_phy_g4_ctrl_base)) {
		dev_err(&pdev->dev, "get anlg-phy-g4 failed!\n");
		return PTR_ERR(anlg_phy_g4_ctrl_base);
	}

	hwdvfs_l3 =
		devm_kzalloc(&pdev->dev,
			     sizeof(struct sprd_hwdvfs_l3), GFP_KERNEL);
	if (!hwdvfs_l3)
		return -ENOMEM;

	atomic_set(&hwdvfs_l3_suspend, 0);
	hwdvfs_l3->probed = false;
	hwdvfs_l3->aon_apb_base = aon_apb_base;
	hwdvfs_l3->anlg_phy_g4_ctrl = anlg_phy_g4_ctrl_base;
	hwdvfs_l3->info = pdata;

	for (i = HWDVFS_CHNL00; i < HWDVFS_CHNL_MAX; i++)
		init_completion(&hwdvfs_l3->dvfs_done[i]);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (!base) {
		ret = -ENOMEM;
		goto exit_err;
	}
	hwdvfs_l3->base = base;

	hwdvfs_l3->irq = platform_get_irq(pdev, 0);
	if (hwdvfs_l3->irq < 0) {
		dev_err(&pdev->dev, "no IRQ resource info\n");
		ret = hwdvfs_l3->irq;
		goto exit_err;
	}

	ret = devm_request_irq(&pdev->dev, hwdvfs_l3->irq, sprd_hwdvfs_l3_isr,
			       IRQF_NO_SUSPEND, "sprd_hwdvfs_l3", NULL);
	if (ret)
		dev_warn(&pdev->dev, "sprd hwdvfs_l3 isr register failed\n");

	ret = sprd_hwdvfs_l3_init_param(np);
	if (ret) {
		dev_err(&pdev->dev, "failed to sprd_hwdvfs_l3_init\n");
		goto exit_err;
	}

	hwdvfs_l3_kobj =
	    kobject_create_and_add("cpufreqhw", cpufreq_global_kobject);
	if (hwdvfs_l3_kobj == NULL) {
		dev_err(&pdev->dev,
			"sysfs failed to add cpufreqhw\n");
		ret = -EPERM;
		goto exit_err;
	}

	ret = sysfs_create_groups(hwdvfs_l3_kobj, hwdvfs_enable_groups);
	if (ret) {
		dev_err(&pdev->dev,
			"sysfs failed to add hwdvfs_enable\n");
		goto exit_err;
	}

	atomic_set(&hwdvfs_l3->state[HWDVFS_CHNL00],
		   HWDVFS_STATE_UNKNOWN);
	atomic_set(&hwdvfs_l3->state[HWDVFS_CHNL01],
		   HWDVFS_STATE_UNKNOWN);
	atomic_set(&hwdvfs_l3->state[HWDVFS_CHNL02],
		   HWDVFS_STATE_UNKNOWN);

	hwdvfs_l3->i2c_client = NULL;
	ret = i2c_add_driver(&hwdvfs_l3_i2c_driver);
	if (ret)
		dev_warn(&pdev->dev, "failed to get i2c client\n");

	hwdvfs_l3_dev.archdata = hwdvfs_l3;
	platform_set_drvdata(pdev, &hwdvfs_l3_dev);
	ret = sprd_hardware_dvfs_device_register(pdev);
	if (ret) {
		dev_err(&pdev->dev,
			"hwdvfs_l3_register fail\n");
		goto exit_err;
	}

	hwdvfs_l3->probed = true;

	return ret;

exit_err:
	hwdvfs_l3 = NULL;

	return ret;
}

static int sprd_hwdvfs_l3_remove(struct platform_device *pdev)
{
	if (hwdvfs_l3_kobj)
		sysfs_remove_groups(hwdvfs_l3_kobj, hwdvfs_enable_groups);

	i2c_del_driver(&hwdvfs_l3_i2c_driver);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sprd_hwdvfs_l3_suspend(struct device *dev)
{
	atomic_set(&hwdvfs_l3_suspend, 1);
	return 0;
}

static int sprd_hwdvfs_l3_resume(struct device *dev)
{
	atomic_set(&hwdvfs_l3_suspend, 0);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sprd_hwdvfs_l3_pm_ops,
			 sprd_hwdvfs_l3_suspend, sprd_hwdvfs_l3_resume);

static struct platform_driver sprd_hwdvfs_l3_driver = {
	.probe = sprd_hwdvfs_l3_probe,
	.remove = sprd_hwdvfs_l3_remove,
	.driver = {
		.name = "sprd_hwdvfs_l3",
		.pm = &sprd_hwdvfs_l3_pm_ops,
		.of_match_table = sprd_hwdvfs_l3_of_match,
	},
};

static int __init sprd_hwdvfs_l3_init(void)
{
	return platform_driver_register(&sprd_hwdvfs_l3_driver);
}

static void __exit sprd_hwdvfs_l3_exit(void)
{
	platform_driver_unregister(&sprd_hwdvfs_l3_driver);
}

subsys_initcall(sprd_hwdvfs_l3_init);
module_exit(sprd_hwdvfs_l3_exit);

MODULE_AUTHOR("Ling Xu <ling_ling.xu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum cpufreq hardware Driver");
MODULE_LICENSE("GPL v2");
