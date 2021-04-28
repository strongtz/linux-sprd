/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 */

#ifndef _ANA_APB_IF_REG_H
#define _ANA_APB_IF_REG_H

#define CTL_BASE_ANA_APB_IF 0x00000000

#define REG_CHIP_ID_LOW           (CTL_BASE_ANA_APB_IF + 0x0000)
#define REG_CHIP_ID_HIGH          (CTL_BASE_ANA_APB_IF + 0x0001)
#define REG_MODULE_EN             (CTL_BASE_ANA_APB_IF + 0x0002)
#define REG_SOFT_RST              (CTL_BASE_ANA_APB_IF + 0x0003)
#define REG_PMU_REG0              (CTL_BASE_ANA_APB_IF + 0x0004)
#define REG_PMU_REG1              (CTL_BASE_ANA_APB_IF + 0x0005)
#define REG_PMU_RESERVED          (CTL_BASE_ANA_APB_IF + 0x0006)
#define REG_BST_REG0              (CTL_BASE_ANA_APB_IF + 0x0007)
#define REG_BST_REG1              (CTL_BASE_ANA_APB_IF + 0x0008)
#define REG_BST_REG2              (CTL_BASE_ANA_APB_IF + 0x0009)
#define REG_BST_RESERVED          (CTL_BASE_ANA_APB_IF + 0x000A)
#define REG_CLSD_REG0             (CTL_BASE_ANA_APB_IF + 0x000B)
#define REG_CLSD_REG1             (CTL_BASE_ANA_APB_IF + 0x000C)
#define REG_CLSAB_REG0            (CTL_BASE_ANA_APB_IF + 0x000D)
#define REG_CLS_RESERVED          (CTL_BASE_ANA_APB_IF + 0x000E)
#define REG_IV_SENSE_FILTER_REG0  (CTL_BASE_ANA_APB_IF + 0x000F)
#define REG_IV_SENSE_ADC_REG0     (CTL_BASE_ANA_APB_IF + 0x0010)
#define REG_IV_SENSE_ADC_REG1     (CTL_BASE_ANA_APB_IF + 0x0011)
#define REG_RESERVED_REG0         (CTL_BASE_ANA_APB_IF + 0x0012)
#define REG_EFS_WR_DATA           (CTL_BASE_ANA_APB_IF + 0x0013)
#define REG_EFS_ADDR_INDEX        (CTL_BASE_ANA_APB_IF + 0x0014)
#define REG_EFS_GLB_CTRL          (CTL_BASE_ANA_APB_IF + 0x0015)
#define REG_EFS_MODE_CTRL         (CTL_BASE_ANA_APB_IF + 0x0016)
#define REG_EFS_MAGIC_NUM         (CTL_BASE_ANA_APB_IF + 0x0017)
#define REG_EFS_STATUS            (CTL_BASE_ANA_APB_IF + 0x0018)
#define REG_EFS_RD_DATA_T         (CTL_BASE_ANA_APB_IF + 0x0019)
#define REG_EFS_RD_DATA_H         (CTL_BASE_ANA_APB_IF + 0x001A)
#define REG_EFS_RD_DATA_M         (CTL_BASE_ANA_APB_IF + 0x001B)
#define REG_EFS_RD_DATA_L         (CTL_BASE_ANA_APB_IF + 0x001C)
#define REG_EFS_RW_STROBE_WIDTH   (CTL_BASE_ANA_APB_IF + 0x001D)
#define REG_PDM_PH_SEL            (CTL_BASE_ANA_APB_IF + 0x001E)
#define REG_AGC_PROTECT_GAIN      (CTL_BASE_ANA_APB_IF + 0x001F)
#define REG_AGC_VCUT              (CTL_BASE_ANA_APB_IF + 0x0020)
#define REG_AGC_M1                (CTL_BASE_ANA_APB_IF + 0x0021)
#define REG_AGC_MK2               (CTL_BASE_ANA_APB_IF + 0x0022)
#define REG_AGC_N1                (CTL_BASE_ANA_APB_IF + 0x0023)
#define REG_AGC_N2                (CTL_BASE_ANA_APB_IF + 0x0024)
#define REG_AGC_NB                (CTL_BASE_ANA_APB_IF + 0x0025)
#define REG_AGC_P1_H              (CTL_BASE_ANA_APB_IF + 0x0026)
#define REG_AGC_P1_L              (CTL_BASE_ANA_APB_IF + 0x0027)
#define REG_AGC_P2_H              (CTL_BASE_ANA_APB_IF + 0x0028)
#define REG_AGC_P2_L              (CTL_BASE_ANA_APB_IF + 0x0029)
#define REG_AGC_PB_H              (CTL_BASE_ANA_APB_IF + 0x002A)
#define REG_AGC_PB_L              (CTL_BASE_ANA_APB_IF + 0x002B)
#define REG_AGC_M4                (CTL_BASE_ANA_APB_IF + 0x002C)
#define REG_AGC_M5                (CTL_BASE_ANA_APB_IF + 0x002D)
#define REG_AGC_GAIN0             (CTL_BASE_ANA_APB_IF + 0x002E)
#define REG_AGC_EN                (CTL_BASE_ANA_APB_IF + 0x002F)
#define REG_PIN_REG0              (CTL_BASE_ANA_APB_IF + 0x0030)
#define REG_PIN_REG1              (CTL_BASE_ANA_APB_IF + 0x0031)
#define REG_RESERVED_REG1         (CTL_BASE_ANA_APB_IF + 0x0032)
#define UCP1301_REG_MAX 0x32

/*
 * Register Name   : REG_CHIP_ID_LOW
 * Register Offset : 0x0000
 * Description     :
 */

#define BIT_CHIP_ID_LOW(x)                                 (((x) & 0xFFFF))

/*
 * Register Name   : REG_CHIP_ID_HIGH
 * Register Offset : 0x0001
 * Description     :
 */

#define BIT_CHIP_ID_HIGH(x)                                (((x) & 0xFFFF))

/*
 * Register Name   : REG_MODULE_EN
 * Register Offset : 0x0002
 * Description     :
 */

#define BIT_CLSD_ACTIVE                                    BIT(2)
#define BIT_BST_ACTIVE                                     BIT(1)
#define BIT_CHIP_EN                                        BIT(0)

/*
 * Register Name   : REG_SOFT_RST
 * Register Offset : 0x0003
 * Description     :
 */

#define BIT_SOFT_RST                                       BIT(0)

/*
 * Register Name   : REG_PMU_REG0
 * Register Offset : 0x0004
 * Description     :
 */

#define BIT_RG_PMU_OVLO_ENB                                BIT(12)
#define BIT_RG_PMU_OTP_ENB                                 BIT(11)
#define BIT_RG_PMU_PWM_AUTOTRACK_EN                        BIT(10)
#define BIT_RG_PMU_PWM_BIT(x)                              (((x) & 0x3) << 8)
#define BIT_RG_PMU_PWM_AMPL(x)                             (((x) & 0x7) << 5)
#define BIT_RG_PMU_VBAT_DETECT_BIT(x)                      (((x) & 0x3) << 3)
#define BIT_RG_PMU_AVDD_BIT(x)                             (((x) & 0x7))

/*
 * Register Name   : REG_PMU_REG1
 * Register Offset : 0x0005
 * Description     :
 */

#define BIT_BST_VOTRIM_SW_SEL                              BIT(12)
#define BIT_PMU_OSC1P6M_DIG_SW_SEL                         BIT(11)
#define BIT_PMU_OSC1P6M_CLSD_SW_SEL                        BIT(10)
#define BIT_RG_PMU_OSC1P6M_DIG_TRIM(x)                     (((x) & 0x1F) << 5)
#define BIT_RG_PMU_OSC1P6M_CLSD_TRIM(x)                    (((x) & 0x1F))

/*
 * Register Name   : REG_PMU_RESERVED
 * Register Offset : 0x0006
 * Description     :
 */

/*
 * Register Name   : REG_BST_REG0
 * Register Offset : 0x0007
 * Description     :
 */

#define BIT_RG_BST_BYPASS                                  BIT(14)
#define BIT_RG_BST_RC(x)                                   (((x) & 0x3) << 12)
#define BIT_RG_BST_OVP_ENB                                 BIT(11)
#define BIT_RG_BST_NSR(x)                                  (((x) & 0x3) << 9)
#define BIT_RG_BST_LP                                      BIT(8)
#define BIT_RG_BST_ISENSE(x)                               (((x) & 0x3) << 6)
#define BIT_RG_BST_ILIMITP(x)                              (((x) & 0x3) << 4)
#define BIT_RG_BST_ILIMIT(x)                               (((x) & 0x3) << 2)
#define BIT_RG_BST_FPWM                                    BIT(1)
#define BIT_RG_BST_ANTIRING_ENB                            BIT(0)

/*
 * Register Name   : REG_BST_REG1
 * Register Offset : 0x0008
 * Description     :
 */

#define BIT_RG_BST_VOTRIM(x)                               (((x) & 0xF) << 11)
#define BIT_RG_BST_VOSEL(x)                                (((x) & 0xF) << 7)
#define BIT_RG_BST_VL(x)                                   (((x) & 0x3) << 5)
#define BIT_RG_BST_VH(x)                                   (((x) & 0x3) << 3)
#define BIT_RG_BST_SLOPE(x)                                (((x) & 0x3) << 1)
#define BIT_RG_BST_SHORT_ENB                               BIT(0)

/*
 * Register Name   : REG_BST_REG2
 * Register Offset : 0x0009
 * Description     :
 */

#define BIT_RG_BST_ZXOFFSET(x)                             (((x) & 0x7) << 2)
#define BIT_RG_BST_ZXDET_ENB                               BIT(1)
#define BIT_RG_BST_WELLSEL_ENB                             BIT(0)

/*
 * Register Name   : REG_BST_RESERVED
 * Register Offset : 0x000A
 * Description     :
 */

/*
 * Register Name   : REG_CLSD_REG0
 * Register Offset : 0x000B
 * Description     :
 */

#define BIT_TIME_FOR_CLSD_CIN_FAST(x)                      (((x) & 0x3) << 14)
#define BIT_TIME_BETWEEN_BST_CLSD(x)                       (((x) & 0x3) << 12)
#define BIT_RG_ADC_CLK_DATA_EDGE                           BIT(11)
#define BIT_CLSD_DEPOP_TIME(x)                             (((x) & 0x3) << 9)
#define BIT_RG_CLSD_PCC_EN                                 BIT(7)
#define BIT_RG_CLSD_DEADTIME(x)                            (((x) & 0x3) << 5)
#define BIT_RG_CLSD_SLOPE(x)                               (((x) & 0x3) << 3)
#define BIT_RG_CLSD_OCP(x)                                 (((x) & 0x3) << 1)
#define BIT_RG_CLSD_MODE_SEL                               BIT(0)

/*
 * Register Name   : REG_CLSD_REG1
 * Register Offset : 0x000C
 * Description     :
 */

#define BIT_READ_ADC_FLAG_DLY(x)                           (((x) & 0x7) << 3)
#define BIT_READ_CLSD_FLAG_DLY(x)                          (((x) & 0x7))

/*
 * Register Name   : REG_CLSAB_REG0
 * Register Offset : 0x000D
 * Description     :
 */

#define BIT_RG_CLSAB_OCP_S                                 BIT(5)
#define BIT_RG_CLSAB_OCP_PD                                BIT(4)
#define BIT_RG_CLSAB_IB(x)                                 (((x) & 0x3) << 2)
#define BIT_RG_CLSAB_RSTN                                  BIT(1)
#define BIT_RG_CLSAB_MODE_EN                               BIT(0)

/*
 * Register Name   : REG_CLS_RESERVED
 * Register Offset : 0x000E
 * Description     :
 */

/*
 * Register Name   : REG_IV_SENSE_FILTER_REG0
 * Register Offset : 0x000F
 * Description     :
 */

#define BIT_RG_AUD_SP_VCMO_S(x)                            (((x) & 0x3) << 11)
#define BIT_RG_AUD_PA_SP_CHOP_VSEN                         BIT(10)
#define BIT_RG_AUD_PA_SP_CHOP_ISEN                         BIT(9)
#define BIT_RG_AUD_PA_VS_G(x)                              (((x) & 0x3) << 6)
#define BIT_RG_AUD_PA_IS_G(x)                              (((x) & 0x3) << 4)
#define BIT_RG_AUD_PA_SVSNSAD                              BIT(3)
#define BIT_RG_AUD_PA_SISNSAD                              BIT(2)
#define BIT_RG_AUD_PA_VSNS_EN                              BIT(1)
#define BIT_RG_AUD_PA_ISNS_EN                              BIT(0)

/*
 * Register Name   : REG_IV_SENSE_ADC_REG0
 * Register Offset : 0x0010
 * Description     :
 */

#define BIT_RG_AUD_ADPGA_IBIAS_EN                          BIT(15)
#define BIT_RG_AUD_VCM_VREF_BUF_EN                         BIT(14)
#define BIT_RG_AUD_VREF_SFCUR                              BIT(13)
#define BIT_RG_AUD_AD_DATA_INVERSE_V                       BIT(12)
#define BIT_RG_AUD_AD_DATA_INVERSE_I                       BIT(11)
#define BIT_RG_AUD_AD_CLK_RST_V                            BIT(10)
#define BIT_RG_AUD_AD_CLK_EN_V                             BIT(9)
#define BIT_RG_AUD_AD_CLK_RST_I                            BIT(8)
#define BIT_RG_AUD_AD_CLK_EN_I                             BIT(7)
#define BIT_RG_AUD_ADC_V_RST                               BIT(6)
#define BIT_RG_AUD_ADC_V_EN                                BIT(5)
#define BIT_RG_AUD_ADC_I_RST                               BIT(4)
#define BIT_RG_AUD_ADC_I_EN                                BIT(3)
#define BIT_RG_VB_EN                                       BIT(2)
#define BIT_RG_AUD_AD_D_GATE_V                             BIT(1)
#define BIT_RG_AUD_AD_D_GATE_I                             BIT(0)

/*
 * Register Name   : REG_IV_SENSE_ADC_REG1
 * Register Offset : 0x0011
 * Description     :
 */

#define BIT_RG_AUD_ADPGA_VCMI_V(x)                         (((x) & 0x3) << 6)
#define BIT_RG_AUD_ADPGA_IBIAS_SEL(x)                      (((x) & 0xF) << 2)
#define BIT_RG_AUD_DAC_I_ADJ(x)                            (((x) & 0x3))

/*
 * Register Name   : REG_RESERVED_REG0
 * Register Offset : 0x0012
 * Description     :
 */

#define BIT_RG_RESERVED0(x)                                (((x) & 0xFFFF))

/*
 * Register Name   : REG_EFS_WR_DATA
 * Register Offset : 0x0013
 * Description     :
 */

#define BIT_EFS_WR_DATA(x)                                 (((x) & 0xFFFF))

/*
 * Register Name   : REG_EFS_ADDR_INDEX
 * Register Offset : 0x0014
 * Description     :
 */

#define BIT_EFS_ADDR_INDEX(x)                              (((x) & 0x3))

/*
 * Register Name   : REG_EFS_GLB_CTRL
 * Register Offset : 0x0015
 * Description     :
 */

#define BIT_EFS_PGM_EN                                     BIT(0)

/*
 * Register Name   : REG_EFS_MODE_CTRL
 * Register Offset : 0x0016
 * Description     :
 */

#define BIT_EFS_NORMAL_READ_DONE_FLAG_CLR                  BIT(2)
#define BIT_EFS_NORMAL_READ_START                          BIT(1)
#define BIT_EFS_PGM_START                                  BIT(0)

/*
 * Register Name   : REG_EFS_MAGIC_NUM
 * Register Offset : 0x0017
 * Description     :
 */

#define BIT_EFS_MAGIC_NUM(x)                               (((x) & 0xFFFF))

/*
 * Register Name   : REG_EFS_STATUS
 * Register Offset : 0x0018
 * Description     :
 */

#define BIT_EFS_STATUS(x)                                  (((x) & 0xFF))
#define BIT_EFS_PGM_BUSY                                   BIT(0)
#define BIT_EFS_RD_BUSY                                    BIT(1)
#define BIT_EFS_IDLE                                       BIT(2)
#define BIT_EFS_GLB_PROT                                   BIT(3)
#define BIT_EFS_NORMAL_RD_DONE_FLAG                        BIT(4)
#define BIT_EFS_RESERVED                                   (((x) & 0x7) << 5)

/*
 * Register Name   : REG_EFS_RD_DATA_T
 * Register Offset : 0x0019
 * Description     :
 */

#define BIT_EFS_RD_DATA_T(x)                               (((x) & 0xFFFF))

/*
 * Register Name   : REG_EFS_RD_DATA_H
 * Register Offset : 0x001A
 * Description     :
 */

#define BIT_EFS_RD_DATA_H(x)                               (((x) & 0xFFFF))

/*
 * Register Name   : REG_EFS_RD_DATA_M
 * Register Offset : 0x001B
 * Description     :
 */

#define BIT_EFS_RD_DATA_M(x)                               (((x) & 0xFFFF))

/*
 * Register Name   : REG_EFS_RD_DATA_L
 * Register Offset : 0x001C
 * Description     :
 */

#define BIT_EFS_RD_DATA_L(x)                               (((x) & 0xFFFF))
#define BIT_EFS_RD_DATA_L_PRO                              BIT(0)
/* BIT_EFS_RD_DATA_L_OSC1P6M_CLSD_TRIM is related
 * to BIT_RG_PMU_OSC1P6M_CLSD_TRIM
 */
#define BIT_EFS_RD_DATA_L_OSC1P6M_CLSD_TRIM(x)             (((x) & 0x1f) << 9)

/*
 * Register Name   : REG_EFS_RW_STROBE_WIDTH
 * Register Offset : 0x001D
 * Description     :
 */

#define BIT_EFS_PGM_STROBE_WIDTH(x)                        (((x) & 0xFF) << 4)
#define BIT_EFS_RD_STROBE_WIDTH(x)                         (((x) & 0xF))

/*
 * Register Name   : REG_PDM_PH_SEL
 * Register Offset : 0x001E
 * Description     :
 */

#define BIT_PDM_PH_SEL                                     BIT(0)

/*
 * Register Name   : REG_AGC_PROTECT_GAIN
 * Register Offset : 0x001F
 * Description     :
 */

#define BIT_AGC_GAIN_STEP(x)                               (((x) & 0x7) << 7)
#define BIT_AGC_PROTECT_GAIN(x)                            (((x) & 0x7F))

/*
 * Register Name   : REG_AGC_VCUT
 * Register Offset : 0x0020
 * Description     :
 */

#define BIT_AGC_VCUT(x)                                    (((x) & 0x3FF))

/*
 * Register Name   : REG_AGC_M1
 * Register Offset : 0x0021
 * Description     :
 */

#define BIT_AGC_M1(x)                                      (((x) & 0xF))

/*
 * Register Name   : REG_AGC_MK2
 * Register Offset : 0x0022
 * Description     :
 */

#define BIT_AGC_MK2(x)                                     (((x) & 0xFF))

/*
 * Register Name   : REG_AGC_N1
 * Register Offset : 0x0023
 * Description     :
 */

#define BIT_AGC_N1(x)                                      (((x) & 0xFF))

/*
 * Register Name   : REG_AGC_N2
 * Register Offset : 0x0024
 * Description     :
 */

#define BIT_AGC_N2(x)                                      (((x) & 0xFF))

/*
 * Register Name   : REG_AGC_NB
 * Register Offset : 0x0025
 * Description     :
 */

#define BIT_AGC_NB(x)                                      (((x) & 0xFF))

/*
 *  Register Name   : REG_AGC_P1_H
 *  Register Offset : 0x0026
 *  Description     :
 */

#define BIT_AGC_P1_H(x)                                    (((x) & 0x7F))

/*
 *  Register Name   : REG_AGC_P1_L
 *  Register Offset : 0x0027
 *  Description     :
 */

#define BIT_AGC_P1_L(x)                                    (((x) & 0xFFFF))

/*
 *  Register Name   : REG_AGC_P2_H
 *  Register Offset : 0x0028
 *  Description     :
 */

#define BIT_AGC_P2_H(x)                                    (((x) & 0x7F))

/*
 *  Register Name   : REG_AGC_P2_L
 *  Register Offset : 0x0029
 *  Description     :
 */

#define BIT_AGC_P2_L(x)                                    (((x) & 0xFFFF))

/*
 *  Register Name   : REG_AGC_PB_H
 *  Register Offset : 0x002A
 *  Description     :
 */

#define BIT_AGC_PB_H(x)                                    (((x) & 0x7F))

/*
 *  Register Name   : REG_AGC_PB_L
 *  Register Offset : 0x002B
 *  Description     :
 */

#define BIT_AGC_PB_L(x)                                    (((x) & 0xFFFF))

/*
 * Register Name   : REG_AGC_M4
 * Register Offset : 0x002C
 * Description     :
 */

#define BIT_AGC_M4(x)                                      (((x) & 0x3F))

/*
 * Register Name   : REG_AGC_M5
 * Register Offset : 0x002D
 * Description     :
 */

#define BIT_AGC_M5(x)                                      (((x) & 0x3F))

/*
 * Register Name   : REG_AGC_GAIN0
 * Register Offset : 0x002E
 * Description     :
 */

#define BIT_AGC_GAIN0(x)                                   (((x) & 0x7F))

/*
 * Register Name   : REG_AGC_EN
 * Register Offset : 0x002F
 * Description     :
 */

#define BIT_AGC_EN                                         BIT(0)

/*
 * Register Name   : REG_PIN_REG0
 * Register Offset : 0x0030
 * Description     :
 */

#define BIT_AD2_FUNC_DRV(x)                                (((x) & 0x3) << 14)
#define BIT_AD2_WPU                                        BIT(13)
#define BIT_AD2_WPDO                                       BIT(12)
#define BIT_AD1_FUNC_DRV(x)                                (((x) & 0x3) << 10)
#define BIT_AD1_WPU                                        BIT(9)
#define BIT_AD1_WPDO                                       BIT(8)
#define BIT_SCL_FUNC_DRV(x)                                (((x) & 0x3) << 6)
#define BIT_SCL_WPU                                        BIT(5)
#define BIT_SCL_WPDO                                       BIT(4)
#define BIT_SDA_FUNC_DRV(x)                                (((x) & 0x3) << 2)
#define BIT_SDA_WPU                                        BIT(1)
#define BIT_SDA_WPDO                                       BIT(0)

/*
 * Register Name   : REG_PIN_REG1
 * Register Offset : 0x0031
 * Description     :
 */

#define BIT_INP_FUNC_DRV(x)                                (((x) & 0x3) << 14)
#define BIT_INP_WPU                                        BIT(13)
#define BIT_INP_WPDO                                       BIT(12)
#define BIT_INN_FUNC_DRV(x)                                (((x) & 0x3) << 10)
#define BIT_INN_WPU                                        BIT(9)
#define BIT_INN_WPDO                                       BIT(8)
#define BIT_CLK_PDM_FUNC_DRV(x)                            (((x) & 0x3) << 6)
#define BIT_CLK_PDM_WPU                                    BIT(5)
#define BIT_CLK_PDM_WPDO                                   BIT(4)
#define BIT_DATA_PDM_FUNC_DRV(x)                           (((x) & 0x3) << 2)
#define BIT_DATA_PDM_WPU                                   BIT(1)
#define BIT_DATA_PDM_WPDO                                  BIT(0)

/*
 * Register Name   : REG_RESERVED_REG1
 * Register Offset : 0x0032
 * Description     :
 */

#define BIT_RG_RESERVED1(x)                                (((x) & 0xFFFF))

#endif /* _ANA_APB_IF_REG_H */
