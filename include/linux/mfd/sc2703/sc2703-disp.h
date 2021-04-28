/* SPDX-License-Identifier: GPL-2.0+
 *
 * LCD, WLED device driver registers for SC2703
 *
 * Copyright (c) 2018 Dialog Semiconductor.
 */

#ifndef __SC2703_DISPLAY_REGISTERS_H
#define __SC2703_DISPLAY_REGISTERS_H

#include <linux/bitops.h>

#define SC2703_SYSCTRL_EVENT                   0x00
#define SC2703_SYSCTRL_STATUS                  0x01
#define SC2703_SYSCTRL_IRQ_MASK                0x02
#define SC2703_SYSCTRL_CHIP_ID                 0x03
#define SC2703_SYSCTRL_DISPLAY_ACTIVE          0x08
#define SC2703_SYSCTRL_DISPLAY_STATUS          0x09
#define SC2703_SYSCTRL_SEQ_MODE_CONTROL1       0x0a
#define SC2703_DISPLAY_EVENT_A                 0x40
#define SC2703_DISPLAY_STATUS_A                0x41
#define SC2703_DISPLAY_IRQ_MASK_A              0x42
#define SC2703_DISPLAY_CONFIG1                 0x43
#define SC2703_DISPLAY_CONFIG2                 0x44
#define SC2703_DISPLAY_CONFIG3                 0x45
#define SC2703_DISPLAY_CONFIG4                 0x46
#define SC2703_DISPLAY_CONFIG5                 0x47
#define SC2703_DISPLAY_CONFIG6                 0x48
#define SC2703_DISPLAY_BOOST_VOLTAGE           0x50
#define SC2703_DISPLAY_BOOST_SLEW_RATE         0x51
#define SC2703_DISPLAY_BOOST_CONFIG1           0x52
#define SC2703_DISPLAY_CP_VOLTAGE              0x60
#define SC2703_DISPLAY_CP_SLEW_RATE            0x61
#define SC2703_DISPLAY_CP_CONFIG8              0x69
#define SC2703_DISPLAY_CP_CONFIG9              0x6a
#define SC2703_DISPLAY_LDO_VOLTAGE             0x73
#define SC2703_DISPLAY_LDO_SLEW_RATE           0x74
#define SC2703_WLED_EVENT                      0x80
#define SC2703_WLED_STATUS                     0x81
#define SC2703_WLED_IRQ_MASK                   0x82
#define SC2703_WLED_CONFIG1                    0x83
#define SC2703_WLED_CONFIG2                    0x84
#define SC2703_WLED_CONFIG3                    0x85
#define SC2703_WLED_CONFIG4                    0x86
#define SC2703_WLED_CONFIG5                    0x87
#define SC2703_WLED_CONFIG6                    0x88
#define SC2703_WLED_CONFIG7                    0x89
#define SC2703_WLED_CONFIG9                    0x8B
#define SC2703_WLED_BOOST_CONTROL1             0x90
#define SC2703_WLED_BOOST_CONTROL2             0x91

/* SC2703_SYSCTRL_EVENT = 0x00 */
#define SC2703_EVT_VSYS_UV_OT_VREF_FLT_SHIFT         1
#define SC2703_EVT_VSYS_UV_OT_VREF_FLT_MASK          BIT(1)
#define SC2703_EVT_CHCR_SHIFT                        2
#define SC2703_EVT_CHCR_MASK                         BIT(2)
#define SC2703_EVT_VDDIO_FLT_SHIFT                   3
#define SC2703_EVT_VDDIO_FLT_MASK                    BIT(3)
#define SC2703_EVT_SYS_WDT_SHIFT                     4
#define SC2703_EVT_SYS_WDT_MASK                      BIT(4)
/* SC2703_SYSCTRL_STATUS = 0x01 */
#define SC2703_VSYS_UV_OT_VREF_FLT_SHIFT             1
#define SC2703_VSYS_UV_OT_VREF_FLT_MASK              BIT(1)
#define SC2703_VDDIO_FLT_SHIFT                       3
#define SC2703_VDDIO_FLT_MASK                        BIT(3)
#define SC2703_SYS_WDT_TIMEOUT_SHIFT                 4
#define SC2703_SYS_WDT_TIMEOUT_MASK                  BIT(4)
/* SC2703_SYSCTRL_IRQ_MASK = 0x02 */
#define SC2703_M_VSYS_UV_OT_VREF_FLT_SHIFT           1
#define SC2703_M_VSYS_UV_OT_VREF_FLT_MASK            BIT(1)
#define SC2703_M_CHCR_SHIFT                          2
#define SC2703_M_CHCR_MASK                           BIT(2)
#define SC2703_M_VDDIO_FLT_SHIFT                     3
#define SC2703_M_VDDIO_FLT_MASK                      BIT(3)
#define SC2703_M_SYS_WDT_SHIFT                       4
#define SC2703_M_SYS_WDT_MASK                        BIT(4)
/* SC2703_SYSCTRL_CHIP_ID = 0x03 */
#define SC2703_MRC_SHIFT                             0
#define SC2703_MRC_MASK                              GENMASK(3, 0)
#define SC2703_TRC_SHIFT                             4
#define SC2703_TRC_MASK                              GENMASK(7, 4)
/* SC2703_SYSCTRL_DISPLAY_ACTIVE = 0x08 */
#define SC2703_DISPLAY_EN_SHIFT                      0
#define SC2703_DISPLAY_EN_MASK                       BIT(0)
#define SC2703_WLED_EN_SHIFT                         1
#define SC2703_WLED_EN_MASK                          BIT(1)
/* SC2703_SYSCTRL_DISPLAY_STATUS = 0x09 */
#define SC2703_DISPLAY_IS_ACTIVE_SHIFT               0
#define SC2703_DISPLAY_IS_ACTIVE_MASK                BIT(0)
#define SC2703_WLED_IS_ACTIVE_SHIFT                  1
#define SC2703_WLED_IS_ACTIVE_MASK                   BIT(1)
#define SC2703_DISPLAY_IS_PG_SHIFT                   4
#define SC2703_DISPLAY_IS_PG_MASK                    BIT(4)
#define SC2703_WLED_IS_PG_SHIFT                      5
#define SC2703_WLED_IS_PG_MASK                       BIT(5)
/* SC2703_SYSCTRL_SEQ_MODE_CONTROL1 = 0x0a */
#define SC2703_COREBUCK_POWER_CTRL_SELECT_SHIFT      0
#define SC2703_COREBUCK_POWER_CTRL_SELECT_MASK       BIT(0)
#define SC2703_COREBUCK_RST_N_CTRL_SELECT_SHIFT      1
#define SC2703_COREBUCK_RST_N_CTRL_SELECT_MASK       BIT(1)
#define SC2703_DISPLAY_POWER_CTRL_SELECT_SHIFT       2
#define SC2703_DISPLAY_POWER_CTRL_SELECT_MASK        BIT(2)
#define SC2703_WLED_POWER_CTRL_SELECT_SHIFT          3
#define SC2703_WLED_POWER_CTRL_SELECT_MASK           BIT(3)
#define SC2703_COREBUCK_EN_CTRL_SELECT_SHIFT         4
#define SC2703_COREBUCK_EN_CTRL_SELECT_MASK          BIT(4)
/* SC2703_DISPLAY_EVENT_A = 0x40 */
#define SC2703_EVT_BOOST_OVP_SHIFT                   0
#define SC2703_EVT_BOOST_OVP_MASK                    BIT(0)
#define SC2703_EVT_BOOST_OCP_SHIFT                   1
#define SC2703_EVT_BOOST_OCP_MASK                    BIT(1)
#define SC2703_EVT_BOOST_UVP_SHIFT                   2
#define SC2703_EVT_BOOST_UVP_MASK                    BIT(2)
#define SC2703_EVT_DLDO_SC_SHIFT                     3
#define SC2703_EVT_DLDO_SC_MASK                      BIT(3)
#define SC2703_EVT_CP_OVP_SHIFT                      4
#define SC2703_EVT_CP_OVP_MASK                       BIT(4)
#define SC2703_EVT_CP_UVP_SHIFT                      5
#define SC2703_EVT_CP_UVP_MASK                       BIT(5)
/* SC2703_DISPLAY_STATUS_A = 0x41 */
#define SC2703_BOOST_OVP_SHIFT                       0
#define SC2703_BOOST_OVP_MASK                        BIT(0)
#define SC2703_BOOST_OCP_SHIFT                       1
#define SC2703_BOOST_OCP_MASK                        BIT(1)
#define SC2703_BOOST_UVP_SHIFT                       2
#define SC2703_BOOST_UVP_MASK                        BIT(2)
#define SC2703_DLDO_SC_SHIFT                         3
#define SC2703_DLDO_SC_MASK                          BIT(3)
#define SC2703_CP_OVP_SHIFT                          4
#define SC2703_CP_OVP_MASK                           BIT(4)
#define SC2703_CP_UVP_SHIFT                          5
#define SC2703_CP_UVP_MASK                           BIT(5)
/* SC2703_DISPLAY_IRQ_MASK_A = 0x42 */
#define SC2703_M_BOOST_OVP_SHIFT                     0
#define SC2703_M_BOOST_OVP_MASK                      BIT(0)
#define SC2703_M_BOOST_OCP_SHIFT                     1
#define SC2703_M_BOOST_OCP_MASK                      BIT(1)
#define SC2703_M_BOOST_UVP_SHIFT                     2
#define SC2703_M_BOOST_UVP_MASK                      BIT(2)
#define SC2703_M_DLDO_SC_SHIFT                       3
#define SC2703_M_DLDO_SC_MASK                        BIT(3)
#define SC2703_M_CP_OVP_SHIFT                        4
#define SC2703_M_CP_OVP_MASK                         BIT(4)
#define SC2703_M_CP_UVP_SHIFT                        5
#define SC2703_M_CP_UVP_MASK                         BIT(5)
/* SC2703_DISPLAY_CONFIG1 = 0x43 */
#define SC2703_SEQ_UP_SIMULTANOUS_SHIFT              0
#define SC2703_SEQ_UP_SIMULTANOUS_MASK               BIT(0)
#define SC2703_SEQ_DN_SIMULTANOUS_SHIFT              1
#define SC2703_SEQ_DN_SIMULTANOUS_MASK               BIT(1)
#define SC2703_UP_VPOS_VNEG_SHIFT                    2
#define SC2703_UP_VPOS_VNEG_MASK                     BIT(2)
#define SC2703_DN_VPOS_VNEG_SHIFT                    3
#define SC2703_DN_VPOS_VNEG_MASK                     BIT(3)
/* SC2703_DISPLAY_CONFIG2 = 0x44 */
#define SC2703_UP_DELAY_SRC_SHIFT                    0
#define SC2703_UP_DELAY_SRC_MASK                     GENMASK(6, 0)
#define SC2703_UP_DELAY_SRC_SCALE_SHIFT              7
#define SC2703_UP_DELAY_SRC_SCALE_MASK               BIT(7)
/* SC2703_DISPLAY_CONFIG3 = 0x45 */
#define SC2703_UP_DELAY_POS_NEG_SHIFT                0
#define SC2703_UP_DELAY_POS_NEG_MASK                 GENMASK(6, 0)
#define SC2703_UP_DELAY_POS_NEG_SCALE_SHIFT          7
#define SC2703_UP_DELAY_POS_NEG_SCALE_MASK           BIT(7)
/* SC2703_DISPLAY_CONFIG4 = 0x46 */
#define SC2703_DN_DELAY_SRC_SHIFT                    0
#define SC2703_DN_DELAY_SRC_MASK                     GENMASK(6, 0)
#define SC2703_DN_DELAY_SRC_SCALE_SHIFT              7
#define SC2703_DN_DELAY_SRC_SCALE_MASK               BIT(7)
/* SC2703_DISPLAY_CONFIG5 = 0x47 */
#define SC2703_DN_DELAY_POS_NEG_SHIFT                0
#define SC2703_DN_DELAY_POS_NEG_MASK                 GENMASK(6, 0)
#define SC2703_DN_DELAY_POS_NEG_SCALE_SHIFT          7
#define SC2703_DN_DELAY_POS_NEG_SCALE_MASK           BIT(7)
/* SC2703_DISPLAY_CONFIG6 = 0x48 */
#define SC2703_BOOST_MAP_SHIFT                       0
#define SC2703_BOOST_MAP_MASK                        GENMASK(1, 0)
#define SC2703_LDO_MAP_SHIFT                         2
#define SC2703_LDO_MAP_MASK                          GENMASK(3, 2)
#define SC2703_CP_MAP_SHIFT                          4
#define SC2703_CP_MAP_MASK                           GENMASK(5, 4)
#define SC2703_MAP_SWITCH_ENABLE_SHIFT               7
#define SC2703_MAP_SWITCH_ENABLE_MASK                BIT(7)
/* SC2703_DISPLAY_BOOST_VOLTAGE = 0x50 */
#define SC2703_BOOST_VOLTAGE_SHIFT                   0
#define SC2703_BOOST_VOLTAGE_MASK                    GENMASK(7, 0)
/* SC2703_DISPLAY_BOOST_SLEW_RATE = 0x51 */
#define SC2703_BOOST_SLEW_RATE_SHIFT                 0
#define SC2703_BOOST_SLEW_RATE_MASK                  GENMASK(2, 0)
/* SC2703_DISPLAY_BOOST_CONFIG1 = 0x52 */
#define SC2703_BOOST_SEL_POC_LIMIT_SHIFT             0
#define SC2703_BOOST_SEL_POC_LIMIT_MASK              GENMASK(2, 0)
/* SC2703_DISPLAY_CP_VOLTAGE = 0x60 */
#define SC2703_CP_VOLTAGE_SHIFT                      0
#define SC2703_CP_VOLTAGE_MASK                       GENMASK(7, 0)
/* SC2703_DISPLAY_CP_SLEW_RATE = 0x61 */
#define SC2703_CP_SLEW_RATE_SHIFT                    0
#define SC2703_CP_SLEW_RATE_MASK                     GENMASK(2, 0)
/* SC2703_DISPLAY_CP_CONFIG8 = 0x69 */
#define SC2703_CP_SEL_RON_MN2_NM_SHIFT               2
#define SC2703_CP_SEL_RON_MN2_NM_MASK                GENMASK(3, 2)
#define SC2703_CP_SEL_RON_MP1_NM_SHIFT               6
#define SC2703_CP_SEL_RON_MP1_NM_MASK                GENMASK(7, 6)
/* SC2703_DISPLAY_CP_CONFIG9 = 0x6a */
#define SC2703_CP_DIN_SPARE_SHIFT                    0
#define SC2703_CP_DIN_SPARE_MASK                     GENMASK(3, 0)
#define SC2703_CP_OUTN_SHRT_PNGD_N5V_SHIFT           5
#define SC2703_CP_OUTN_SHRT_PNGD_N5V_MASK            BIT(5)
/* SC2703_DISPLAY_LDO_VOLTAGE = 0x73 */
#define SC2703_LDO_VOLTAGE_SHIFT                     0
#define SC2703_LDO_VOLTAGE_MASK                      GENMASK(7, 0)
/* SC2703_DISPLAY_LDO_SLEW_RATE = 0x74 */
#define SC2703_LDO_SLEW_RATE_SHIFT                   0
#define SC2703_LDO_SLEW_RATE_MASK                    GENMASK(2, 0)
/* SC2703_WLED_EVENT = 0x80 */
#define SC2703_EVT_WLED_OC_SHIFT                     0
#define SC2703_EVT_WLED_OC_MASK                      BIT(0)
#define SC2703_EVT_WLED_OV_SHIFT                     1
#define SC2703_EVT_WLED_OV_MASK                      BIT(1)
#define SC2703_EVT_WLED_UV_SHIFT                     2
#define SC2703_EVT_WLED_UV_MASK                      BIT(2)
/* SC2703_WLED_STATUS = 0x81 */
#define SC2703_WLED_OC_SHIFT                         0
#define SC2703_WLED_OC_MASK                          BIT(0)
#define SC2703_WLED_OV_SHIFT                         1
#define SC2703_WLED_OV_MASK                          BIT(1)
#define SC2703_WLED_UV_SHIFT                         2
#define SC2703_WLED_UV_MASK                          BIT(2)
/* SC2703_WLED_IRQ_MASK = 0x82 */
#define SC2703_M_WLED_OC_SHIFT                       0
#define SC2703_M_WLED_OC_MASK                        BIT(0)
#define SC2703_M_WLED_OV_SHIFT                       1
#define SC2703_M_WLED_OV_MASK                        BIT(1)
#define SC2703_M_WLED_UV_SHIFT                       2
#define SC2703_M_WLED_UV_MASK                        BIT(2)
/* SC2703_WLED_CONFIG1 = 0x83 */
#define SC2703_WLED_MODE_SHIFT                       0
#define SC2703_WLED_MODE_MASK                        GENMASK(2, 0)
#define SC2703_WLED_IDAC_EN_SHIFT                    3
#define SC2703_WLED_IDAC_EN_MASK                     BIT(3)
#define SC2703_IDAC_LINEAR_SHIFT                     4
#define SC2703_IDAC_LINEAR_MASK                      BIT(4)
/* SC2703_WLED_CONFIG2 = 0x84 */
#define SC2703_IDAC_TARGET_SHIFT                     0
#define SC2703_IDAC_TARGET_MASK                      GENMASK(7, 0)
/* SC2703_WLED_CONFIG3 = 0x85 */
#define SC2703_IDAC_RAMP_RATE_SHIFT                  0
#define SC2703_IDAC_RAMP_RATE_MASK                   GENMASK(3, 0)
#define SC2703_PWM_IN_FREQ_RANGE_SHIFT               4
#define SC2703_PWM_IN_FREQ_RANGE_MASK                BIT(4)
#define SC2703_PWM_OUT_FREQ_STEP_SHIFT               5
#define SC2703_PWM_OUT_FREQ_STEP_MASK                GENMASK(7, 5)
/* SC2703_WLED_CONFIG4 = 0x86 */
#define SC2703_PWM_IN_DUTY_THRESHOLD_SHIFT           0
#define SC2703_PWM_IN_DUTY_THRESHOLD_MASK            GENMASK(7, 0)
/* SC2703_WLED_CONFIG5 = 0x87 */
#define SC2703_PWM_OUT_DUTY_SHIFT                    0
#define SC2703_PWM_OUT_DUTY_MASK                     GENMASK(7, 0)
/* SC2703_WLED_CONFIG6 = 0x88 */
#define SC2703_WLED_PANIC_VTH_SHIFT                  0
#define SC2703_WLED_PANIC_VTH_MASK                   GENMASK(1, 0)
#define SC2703_WLED_PANIC_EN_SHIFT                   2
#define SC2703_WLED_PANIC_EN_MASK                    BIT(2)
/* SC2703_WLED_CONFIG7 = 0x89 */
#define SC2703_IDAC_RAMP_DIS_SHIFT                   0
#define SC2703_IDAC_RAMP_DIS_MASK                    BIT(0)
#define SC2703_PWM_THRESHOLD_PLUS_SHIFT              1
#define SC2703_PWM_THRESHOLD_PLUS_MASK               BIT(1)
#define SC2703_WLED_DISCHARGE_SEL_SHIFT              2
#define SC2703_WLED_DISCHARGE_SEL_MASK               GENMASK(4, 2)
#define SC2703_WLED_PANIC_PERIOD_SHIFT               5
#define SC2703_WLED_PANIC_PERIOD_MASK                GENMASK(6, 5)
/* SC2703_WLED_BOOST_CONTROL1 = 0x90 */
#define SC2703_SEL_POC_LIMIT_SHIFT                   0
#define SC2703_SEL_POC_LIMIT_MASK                    GENMASK(1, 0)
#define SC2703_SEL_OVP_TH_SHIFT                      2
#define SC2703_SEL_OVP_TH_MASK                       BIT(2)
#define SC2703_PANIC_FB_SEL_SHIFT                    3
#define SC2703_PANIC_FB_SEL_MASK                     GENMASK(4, 3)
#define SC2703_VDAC_SLEW_RATE_SHIFT                  5
#define SC2703_VDAC_SLEW_RATE_MASK                   GENMASK(7, 5)
/* SC2703_WLED_BOOST_CONTROL2 = 0x91 */
#define SC2703_VDAC_SEL_SHIFT                        0
#define SC2703_VDAC_SEL_MASK                         GENMASK(7, 0)

#endif /* __SC2703_DISPLAY_REGISTERS_H */
