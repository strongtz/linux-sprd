/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2019-01-11 11:21:44
 *
 */


#ifndef SPRD_HWDVFS_SHARKL3_H
#define SPRD_HWDVFS_SHARKL3_H

#define REG_DVFS_CTRL_VERSION            (0x0000)
#define REG_DVFS_CTRL_USER_LOCK00        (0x0004)
#define REG_DVFS_CTRL_USER_LOCK01        (0x0008)
#define REG_DVFS_CTRL_USER_LOCK02        (0x000C)
#define REG_DVFS_CTRL_MAGIC_NUM          (0x0010)
#define REG_DVFS_CTRL_GLB_CFG            (0x0014)
#define REG_DVFS_CTRL_TMR_CLR            (0x0018)
#define REG_DVFS_CTRL_SW_FLUSH           (0x001C)
#define REG_DVFS_CTRL_VTUNE_STEP_CORE00  (0x0020)
#define REG_DVFS_CTRL_VTUNE_STEP_CORE01  (0x0024)
#define REG_DVFS_CTRL_VOLT_VALID_BIT     (0x0028)
#define REG_DVFS_CTRL_TUNE_EN            (0x002C)
#define REG_DVFS_CTRL_HW_CHNL_EN         (0x0030)
#define REG_DVFS_CTRL_SW_CHNL_EN         (0x0034)
#define REG_DVFS_CTRL_CHNL_CORE_MAP      (0x0038)
#define REG_DVFS_CTRL_IRQ_EN_CHNL00      (0x003C)
#define REG_DVFS_CTRL_IRQ_EN_CHNL01      (0x0040)
#define REG_DVFS_CTRL_IRQ_EN_CHNL02      (0x0044)
#define REG_DVFS_CTRL_TMR_PRESCALE       (0x0048)
#define REG_DVFS_CTRL_TMR_CTRL1_CORE00   (0x004C)
#define REG_DVFS_CTRL_TMR_CTRL2_CORE00   (0x0050)
#define REG_DVFS_CTRL_TMR_CTRL1_CORE01   (0x0054)
#define REG_DVFS_CTRL_TMR_CTRL2_CORE01   (0x0058)
#define REG_DVFS_CTRL_TMR_CTRL_PLL       (0x005C)
#define REG_DVFS_CTRL_TMR_CTRL_FMUX      (0x0060)
#define REG_DVFS_CTRL_CFG_CHNL00         (0x0064)
#define REG_DVFS_CTRL_CFG_CHNL01         (0x0068)
#define REG_DVFS_CTRL_CFG_CHNL02         (0x006C)
#define REG_DVFS_CTRL_HW_DVFS_SEL        (0x0070)
#define REG_DVFS_CTRL_SW_TRG_CHNL00      (0x0074)
#define REG_DVFS_CTRL_SW_TRG_CHNL01      (0x0078)
#define REG_DVFS_CTRL_SW_TRG_CHNL02      (0x007C)
#define REG_DVFS_CTRL_STS_CHNL00         (0x0080)
#define REG_DVFS_CTRL_STS_CHNL01         (0x0084)
#define REG_DVFS_CTRL_STS_CHNL02         (0x0088)
#define REG_DVFS_CTRL_DBG_STS0           (0x008C)
#define REG_DVFS_CTRL_DBG_STS1           (0x0090)
#define REG_DVFS_CTRL_DBG_STS2           (0x0094)
#define REG_DVFS_CTRL_DBG_STS3           (0x0098)
#define REG_DVFS_CTRL_DBG_STS4           (0x009C)
#define REG_DVFS_CTRL_DBG_STS5           (0x00A0)
#define REG_DVFS_CTRL_DBG_STS6           (0x00A4)
#define REG_DVFS_CTRL_DBG_STS7           (0x00A8)
#define REG_DVFS_CTRL_DBG_STS8           (0x00AC)
#define REG_DVFS_CTRL_DBG_STS9           (0x00B0)
#define REG_DVFS_CTRL_DBG_STS10          (0x00B4)
#define REG_DVFS_CTRL_DBG_STS11          (0x00B8)
#define REG_DVFS_CTRL_DBG_STS12          (0x00BC)
#define REG_DVFS_CTRL_CHNL00_SCALE00     (0x0100)
#define REG_DVFS_CTRL_CHNL00_SCALE01     (0x0104)
#define REG_DVFS_CTRL_CHNL00_SCALE02     (0x0108)
#define REG_DVFS_CTRL_CHNL00_SCALE03     (0x010C)
#define REG_DVFS_CTRL_CHNL00_SCALE04     (0x0110)
#define REG_DVFS_CTRL_CHNL00_SCALE05     (0x0114)
#define REG_DVFS_CTRL_CHNL00_SCALE06     (0x0118)
#define REG_DVFS_CTRL_CHNL00_SCALE07     (0x011C)
#define REG_DVFS_CTRL_CHNL01_SCALE00     (0x0120)
#define REG_DVFS_CTRL_CHNL01_SCALE01     (0x0124)
#define REG_DVFS_CTRL_CHNL01_SCALE02     (0x0128)
#define REG_DVFS_CTRL_CHNL01_SCALE03     (0x012C)
#define REG_DVFS_CTRL_CHNL01_SCALE04     (0x0130)
#define REG_DVFS_CTRL_CHNL01_SCALE05     (0x0134)
#define REG_DVFS_CTRL_CHNL01_SCALE06     (0x0138)
#define REG_DVFS_CTRL_CHNL01_SCALE07     (0x013C)
#define REG_DVFS_CTRL_CHNL02_SCALE00     (0x0140)
#define REG_DVFS_CTRL_CHNL02_SCALE01     (0x0144)
#define REG_DVFS_CTRL_CHNL02_SCALE02     (0x0148)
#define REG_DVFS_CTRL_CHNL02_SCALE03     (0x014C)
#define REG_DVFS_CTRL_CHNL02_SCALE04     (0x0150)
#define REG_DVFS_CTRL_CHNL02_SCALE05     (0x0154)
#define REG_DVFS_CTRL_CHNL02_SCALE06     (0x0158)
#define REG_DVFS_CTRL_CHNL02_SCALE07     (0x015C)

/* REG_DVFS_CTRL_VERSION */

#define BIT_DVFS_CTRL_PROJ_NAME(x)               (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_VERSION(x)                 (((x) & 0xFFFF))

/* REG_DVFS_CTRL_USER_LOCK00 */

#define BIT_DVFS_CTRL_USER_LOCK00                BIT(0)

/* REG_DVFS_CTRL_USER_LOCK01 */

#define BIT_DVFS_CTRL_USER_LOCK01                BIT(0)

/* REG_DVFS_CTRL_USER_LOCK02 */

#define BIT_DVFS_CTRL_USER_LOCK02                BIT(0)

/* REG_DVFS_CTRL_MAGIC_NUM */

#define BIT_DVFS_CTRL_MAGIC_NUM(x)               (((x) & 0xFFFFFFFF))

/* REG_DVFS_CTRL_GLB_CFG */

#define BIT_DVFS_CTRL_DEBUG_SEL(x)               (((x) & 0xFF) << 8)
#define BIT_DVFS_CTRL_TO_CHECK_EN                BIT(2)
#define BIT_DVFS_CTRL_AUTO_GATE_EN               BIT(1)
#define BIT_DVFS_CTRL_SMART_MODE                 BIT(0)

/* REG_DVFS_CTRL_TMR_CLR */

#define BIT_DVFS_CTRL_TMR_CLR(x)                 (((x) & 0x3))

/* REG_DVFS_CTRL_SW_FLUSH */

#define BIT_DVFS_CTRL_SW_FLUSH(x)                (((x) & 0x3))

/* REG_DVFS_CTRL_VTUNE_STEP_CORE00 */

#define BIT_DVFS_CTRL_VTUNE_STEP_FAST_CORE00     BIT(16)
#define BIT_DVFS_CTRL_VTUNE_STEP_VAL_CORE00(x)   (((x) & 0x3FF))

/* REG_DVFS_CTRL_VTUNE_STEP_CORE01 */

#define BIT_DVFS_CTRL_VTUNE_STEP_FAST_CORE01     BIT(16)
#define BIT_DVFS_CTRL_VTUNE_STEP_VAL_CORE01(x)   (((x) & 0x3FF))

/* REG_DVFS_CTRL_VOLT_VALID_BIT */

#define BIT_DVFS_CTRL_VTUNE_VLD_BIT_CORE01(x)    (((x) & 0x3FF) << 16)
#define BIT_DVFS_CTRL_VTUNE_VLD_BIT_CORE00(x)    (((x) & 0x3FF))

/* REG_DVFS_CTRL_TUNE_EN */

#define BIT_DVFS_CTRL_VTUNE_EN(x)                (((x) & 0x3) << 16)
#define BIT_DVFS_CTRL_DCDC_EN_DTCT_EN(x)         (((x) & 0x3) << 8)
#define BIT_DVFS_CTRL_FCFG_TUNE_EN(x)            (((x) & 0x7))

/* REG_DVFS_CTRL_HW_CHNL_EN */

#define BIT_DVFS_CTRL_HW_CHNL_EN(x)              (((x) & 0x7))

/* REG_DVFS_CTRL_SW_CHNL_EN */

#define BIT_DVFS_CTRL_SW_CHNL_EN(x)              (((x) & 0x7))

/* REG_DVFS_CTRL_CHNL_CORE_MAP */

#define BIT_DVFS_CTRL_CORE_DCDC_MAP(x)           (((x) & 0x3) << 8)
#define BIT_DVFS_CTRL_CHNL_CORE_MAP(x)           (((x) & 0x7))

/* REG_DVFS_CTRL_IRQ_EN_CHNL00 */

#define BIT_DVFS_CTRL_IRQ_VREAD_MIS_EN_CHNL00    BIT(2)
#define BIT_DVFS_CTRL_IRQ_TO_EN_CHNL00           BIT(1)
#define BIT_DVFS_CTRL_IRQ_DONE_EN_CHNL00         BIT(0)

/* REG_DVFS_CTRL_IRQ_EN_CHNL01 */

#define BIT_DVFS_CTRL_IRQ_VREAD_MIS_EN_CHNL01    BIT(2)
#define BIT_DVFS_CTRL_IRQ_TO_EN_CHNL01           BIT(1)
#define BIT_DVFS_CTRL_IRQ_DONE_EN_CHNL01         BIT(0)

/* REG_DVFS_CTRL_IRQ_EN_CHNL02 */

#define BIT_DVFS_CTRL_IRQ_VREAD_MIS_EN_CHNL02    BIT(2)
#define BIT_DVFS_CTRL_IRQ_TO_EN_CHNL02           BIT(1)
#define BIT_DVFS_CTRL_IRQ_DONE_EN_CHNL02         BIT(0)

/* REG_DVFS_CTRL_TMR_PRESCALE */

#define BIT_DVFS_CTRL_TMR_PRESCALE(x)            (((x) & 0xFF))

/* REG_DVFS_CTRL_TMR_CTRL1_CORE00 */

#define BIT_DVFS_CTRL_HOLD_VAL_CORE00(x)         (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_PAUSE_VAL_CORE00(x)        (((x) & 0xFFFF))

/* REG_DVFS_CTRL_TMR_CTRL2_CORE00 */

#define BIT_DVFS_CTRL_TO_VAL_CORE00(x)           (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_DCDC_STABLE_VAL_CORE00(x)  (((x) & 0xFFFF))

/* REG_DVFS_CTRL_TMR_CTRL1_CORE01 */

#define BIT_DVFS_CTRL_HOLD_VAL_CORE01(x)         (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_PAUSE_VAL_CORE01(x)        (((x) & 0xFFFF))

/* REG_DVFS_CTRL_TMR_CTRL2_CORE01 */

#define BIT_DVFS_CTRL_TO_VAL_CORE01(x)           (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_DCDC_STABLE_VAL_CORE01(x)  (((x) & 0xFFFF))

/* REG_DVFS_CTRL_TMR_CTRL_PLL */

#define BIT_DVFS_CTRL_PLL_PD_VAL(x)              (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_PLL_STABLE_VAL(x)          (((x) & 0xFFFF))

/* REG_DVFS_CTRL_TMR_CTRL_FMUX */

#define BIT_DVFS_CTRL_FMUX_STABLE_VAL(x)         (((x) & 0xFFFF))

/* REG_DVFS_CTRL_CFG_CHNL00 */

#define BIT_DVFS_CTRL_FCFG_PD_SW_CHNL00          BIT(31)
#define BIT_DVFS_CTRL_FSEL_PARK_CHNL00(x)        (((x) & 0x7) << 16)
#define BIT_DVFS_CTRL_FSEL_BKP_CHNL00(x)         (((x) & 0x7) << 8)
#define BIT_DVFS_CTRL_FSEL_SW_CHNL00(x)          (((x) & 0x7))

/* REG_DVFS_CTRL_CFG_CHNL01 */

#define BIT_DVFS_CTRL_FCFG_PD_SW_CHNL01          BIT(31)
#define BIT_DVFS_CTRL_FSEL_PARK_CHNL01(x)        (((x) & 0x7) << 16)
#define BIT_DVFS_CTRL_FSEL_BKP_CHNL01(x)         (((x) & 0x7) << 8)
#define BIT_DVFS_CTRL_FSEL_SW_CHNL01(x)          (((x) & 0x7))

/* REG_DVFS_CTRL_CFG_CHNL02 */

#define BIT_DVFS_CTRL_FCFG_PD_SW_CHNL02          BIT(31)
#define BIT_DVFS_CTRL_FSEL_PARK_CHNL02(x)        (((x) & 0x7) << 16)
#define BIT_DVFS_CTRL_FSEL_BKP_CHNL02(x)         (((x) & 0x7) << 8)
#define BIT_DVFS_CTRL_FSEL_SW_CHNL02(x)          (((x) & 0x7))

/* REG_DVFS_CTRL_HW_DVFS_SEL */

#define BIT_DVFS_CTRL_HW_DVFS_SEL                BIT(0)

/* REG_DVFS_CTRL_SW_TRG_CHNL00 */

#define BIT_DVFS_CTRL_SW_TRG_CHNL00              BIT(8)
#define BIT_DVFS_CTRL_SW_SCL_CHNL00(x)           (((x) & 0x7))

/* REG_DVFS_CTRL_SW_TRG_CHNL01 */

#define BIT_DVFS_CTRL_SW_TRG_CHNL01              BIT(8)
#define BIT_DVFS_CTRL_SW_SCL_CHNL01(x)           (((x) & 0x7))

/* REG_DVFS_CTRL_SW_TRG_CHNL02 */

#define BIT_DVFS_CTRL_SW_TRG_CHNL02              BIT(8)
#define BIT_DVFS_CTRL_SW_SCL_CHNL02(x)           (((x) & 0x7))

/* REG_DVFS_CTRL_STS_CHNL00 */

#define BIT_DVFS_CTRL_DONE_SCL_CHNL00(x)         (((x) & 0x7) << 24)
#define BIT_DVFS_CTRL_DTCT_REQ_SCL_CHNL00(x)     (((x) & 0x7) << 16)
#define BIT_DVFS_CTRL_FREQ_UPDATE_MASK_CHNL00    BIT(9)
#define BIT_DVFS_CTRL_REQ_MASK_CHNL00            BIT(8)
#define BIT_DVFS_CTRL_DTCT_REQ_CHNL00            BIT(7)
#define BIT_DVFS_CTRL_STORE_REQ_CHNL00           BIT(6)
#define BIT_DVFS_CTRL_STS_BUSY_CHNL00            BIT(5)
#define BIT_DVFS_CTRL_CONFLICT1_CHNL00           BIT(4)
#define BIT_DVFS_CTRL_CONFLICT0_CHNL00           BIT(3)
#define BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL00       BIT(2)
#define BIT_DVFS_CTRL_IRQ_TO_CHNL00              BIT(1)
#define BIT_DVFS_CTRL_IRQ_DONE_CHNL00            BIT(0)

/* REG_DVFS_CTRL_STS_CHNL01 */

#define BIT_DVFS_CTRL_DONE_SCL_CHNL01(x)         (((x) & 0x7) << 24)
#define BIT_DVFS_CTRL_DTCT_REQ_SCL_CHNL01(x)     (((x) & 0x7) << 16)
#define BIT_DVFS_CTRL_FREQ_UPDATE_MASK_CHNL01    BIT(9)
#define BIT_DVFS_CTRL_REQ_MASK_CHNL01            BIT(8)
#define BIT_DVFS_CTRL_DTCT_REQ_CHNL01            BIT(7)
#define BIT_DVFS_CTRL_STORE_REQ_CHNL01           BIT(6)
#define BIT_DVFS_CTRL_STS_BUSY_CHNL01            BIT(5)
#define BIT_DVFS_CTRL_CONFLICT1_CHNL01           BIT(4)
#define BIT_DVFS_CTRL_CONFLICT0_CHNL01           BIT(3)
#define BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL01       BIT(2)
#define BIT_DVFS_CTRL_IRQ_TO_CHNL01              BIT(1)
#define BIT_DVFS_CTRL_IRQ_DONE_CHNL01            BIT(0)

/* REG_DVFS_CTRL_STS_CHNL02 */

#define BIT_DVFS_CTRL_DONE_SCL_CHNL02(x)         (((x) & 0x7) << 24)
#define BIT_DVFS_CTRL_DTCT_REQ_SCL_CHNL02(x)     (((x) & 0x7) << 16)
#define BIT_DVFS_CTRL_FREQ_UPDATE_MASK_CHNL02    BIT(9)
#define BIT_DVFS_CTRL_REQ_MASK_CHNL02            BIT(8)
#define BIT_DVFS_CTRL_DTCT_REQ_CHNL02            BIT(7)
#define BIT_DVFS_CTRL_STORE_REQ_CHNL02           BIT(6)
#define BIT_DVFS_CTRL_STS_BUSY_CHNL02            BIT(5)
#define BIT_DVFS_CTRL_CONFLICT1_CHNL02           BIT(4)
#define BIT_DVFS_CTRL_CONFLICT0_CHNL02           BIT(3)
#define BIT_DVFS_CTRL_IRQ_VREAD_MIS_CHNL02       BIT(2)
#define BIT_DVFS_CTRL_IRQ_TO_CHNL02              BIT(1)
#define BIT_DVFS_CTRL_IRQ_DONE_CHNL02            BIT(0)

/* REG_DVFS_CTRL_DBG_STS0 */

#define BIT_DVFS_CTRL_FSM_CORE01(x)              (((x) & 0xF) << 4)
#define BIT_DVFS_CTRL_FSM_CORE00(x)              (((x) & 0xF))

/* REG_DVFS_CTRL_DBG_STS1 */

#define BIT_DVFS_CTRL_FTMR_VAL_CORE00(x)         (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_VTMR_VAL_CORE00(x)         (((x) & 0xFFFF))

/* REG_DVFS_CTRL_DBG_STS2 */

#define BIT_DVFS_CTRL_FTMR_VAL_CORE01(x)         (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_VTMR_VAL_CORE01(x)         (((x) & 0xFFFF))

/* REG_DVFS_CTRL_DBG_STS3 */

#define BIT_DVFS_CTRL_VREAD_VAL_LATCH_CORE01(x)  (((x) & 0x3FF) << 16)
#define BIT_DVFS_CTRL_VREAD_VAL_LATCH_CORE00(x)  (((x) & 0x3FF))

/* REG_DVFS_CTRL_DBG_STS4 */

#define BIT_DVFS_CTRL_VOLT_VAL_MAX_CORE01(x)     (((x) & 0x3FF) << 16)
#define BIT_DVFS_CTRL_VOLT_VAL_MAX_CORE00(x)     (((x) & 0x3FF))

/* REG_DVFS_CTRL_DBG_STS5 */

#define BIT_DVFS_CTRL_VTUNE_VAL_DONE_CORE01(x)   (((x) & 0x3FF) << 16)
#define BIT_DVFS_CTRL_VTUNE_VAL_DONE_CORE00(x)   (((x) & 0x3FF))

/* REG_DVFS_CTRL_DBG_STS6 */

#define BIT_DVFS_CTRL_VTUNE_REQ_DCDC00           BIT(31)
#define BIT_DVFS_CTRL_VTUNE_ACK_DCDC00           BIT(30)
#define BIT_DVFS_CTRL_VTUNE_VAL_DCDC00(x)        (((x) & 0x3FF) << 16)
#define BIT_DVFS_CTRL_VREAD_REQ_DCDC00           BIT(15)
#define BIT_DVFS_CTRL_VREAD_ACK_DCDC00           BIT(14)
#define BIT_DVFS_CTRL_VREAD_VAL_DCDC00(x)        (((x) & 0x3FF))

/* REG_DVFS_CTRL_DBG_STS7 */

#define BIT_DVFS_CTRL_VTUNE_REQ_DCDC01           BIT(31)
#define BIT_DVFS_CTRL_VTUNE_ACK_DCDC01           BIT(30)
#define BIT_DVFS_CTRL_VTUNE_VAL_DCDC01(x)        (((x) & 0x3FF) << 16)
#define BIT_DVFS_CTRL_VREAD_REQ_DCDC01           BIT(15)
#define BIT_DVFS_CTRL_VREAD_ACK_DCDC01           BIT(14)
#define BIT_DVFS_CTRL_VREAD_VAL_DCDC01(x)        (((x) & 0x3FF))

/* REG_DVFS_CTRL_DBG_STS8 */

#define BIT_DVFS_CTRL_VTUNE_REQ_DCDC02           BIT(31)
#define BIT_DVFS_CTRL_VTUNE_ACK_DCDC02           BIT(30)
#define BIT_DVFS_CTRL_VTUNE_VAL_DCDC02(x)        (((x) & 0x3FF) << 16)
#define BIT_DVFS_CTRL_VREAD_REQ_DCDC02           BIT(15)
#define BIT_DVFS_CTRL_VREAD_ACK_DCDC02           BIT(14)
#define BIT_DVFS_CTRL_VREAD_VAL_DCDC02(x)        (((x) & 0x3FF))

/* REG_DVFS_CTRL_DBG_STS9 */

#define BIT_DVFS_CTRL_FCFG_PD_CHNL00             BIT(18)
#define BIT_DVFS_CTRL_FCFG_RST_CHNL00            BIT(17)
#define BIT_DVFS_CTRL_FCFG_EN_CHNL00             BIT(16)
#define BIT_DVFS_CTRL_FCFG_CHNL00(x)             (((x) & 0xF) << 8)
#define BIT_DVFS_CTRL_FSEL_CHNL00(x)             (((x) & 0x7))

/* REG_DVFS_CTRL_DBG_STS10 */

#define BIT_DVFS_CTRL_FCFG_PD_CHNL01             BIT(18)
#define BIT_DVFS_CTRL_FCFG_RST_CHNL01            BIT(17)
#define BIT_DVFS_CTRL_FCFG_EN_CHNL01             BIT(16)
#define BIT_DVFS_CTRL_FCFG_CHNL01(x)             (((x) & 0xF) << 8)
#define BIT_DVFS_CTRL_FSEL_CHNL01(x)             (((x) & 0x7))

/* REG_DVFS_CTRL_DBG_STS11 */

#define BIT_DVFS_CTRL_FCFG_PD_CHNL02             BIT(18)
#define BIT_DVFS_CTRL_FCFG_RST_CHNL02            BIT(17)
#define BIT_DVFS_CTRL_FCFG_EN_CHNL02             BIT(16)
#define BIT_DVFS_CTRL_FCFG_CHNL02(x)             (((x) & 0xF) << 8)
#define BIT_DVFS_CTRL_FSEL_CHNL02(x)             (((x) & 0x7))

/* REG_DVFS_CTRL_DBG_STS12 */

#define BIT_DVFS_CTRL_DEBUG_CNT_CORE01(x)        (((x) & 0xFFFF) << 16)
#define BIT_DVFS_CTRL_DEBUG_CNT_CORE00(x)        (((x) & 0xFFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE00 */

#define BIT_DVFS_CTRL_CHNL00_SCALE00(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE01 */

#define BIT_DVFS_CTRL_CHNL00_SCALE01(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE02 */

#define BIT_DVFS_CTRL_CHNL00_SCALE02(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE03 */

#define BIT_DVFS_CTRL_CHNL00_SCALE03(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE04 */

#define BIT_DVFS_CTRL_CHNL00_SCALE04(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE05 */

#define BIT_DVFS_CTRL_CHNL00_SCALE05(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE06 */

#define BIT_DVFS_CTRL_CHNL00_SCALE06(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL00_SCALE07 */

#define BIT_DVFS_CTRL_CHNL00_SCALE07(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE00 */

#define BIT_DVFS_CTRL_CHNL01_SCALE00(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE01 */

#define BIT_DVFS_CTRL_CHNL01_SCALE01(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE02 */

#define BIT_DVFS_CTRL_CHNL01_SCALE02(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE03 */

#define BIT_DVFS_CTRL_CHNL01_SCALE03(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE04 */

#define BIT_DVFS_CTRL_CHNL01_SCALE04(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE05 */

#define BIT_DVFS_CTRL_CHNL01_SCALE05(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE06 */

#define BIT_DVFS_CTRL_CHNL01_SCALE06(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL01_SCALE07 */

#define BIT_DVFS_CTRL_CHNL01_SCALE07(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE00 */

#define BIT_DVFS_CTRL_CHNL02_SCALE00(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE01 */

#define BIT_DVFS_CTRL_CHNL02_SCALE01(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE02 */

#define BIT_DVFS_CTRL_CHNL02_SCALE02(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE03 */

#define BIT_DVFS_CTRL_CHNL02_SCALE03(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE04 */

#define BIT_DVFS_CTRL_CHNL02_SCALE04(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE05 */

#define BIT_DVFS_CTRL_CHNL02_SCALE05(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE06 */

#define BIT_DVFS_CTRL_CHNL02_SCALE06(x)          (((x) & 0x1FFFF))

/* REG_DVFS_CTRL_CHNL02_SCALE07 */

#define BIT_DVFS_CTRL_CHNL02_SCALE07(x)          (((x) & 0x1FFFF))


#endif /* SPRD_HWDVFS_SHARKL3_H */

