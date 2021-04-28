/*
 * sound/soc/sprd/codec/sprd/v5/sprd-codec.h
 *
 * SPRD-CODEC -- SpreadTrum Tiger intergrated codec.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SPRD_CODEC_2721_H
#define __SPRD_CODEC_2721_H

#include <linux/io.h>
#include "sprd-audio.h"
#include "sprd-audio-sc2721.h"

/* unit: ms */
#define SPRD_CODEC_HP_POP_TIMEOUT	(1000)

#define SPRD_CODEC_INFO	(AUDIO_CODEC_2721)

#define SPRD_CODEC_RATE_8000   (10)
#define SPRD_CODEC_RATE_9600   (9)
#define SPRD_CODEC_RATE_11025  (8)
#define SPRD_CODEC_RATE_12000  (7)
#define SPRD_CODEC_RATE_16000  (6)
#define SPRD_CODEC_RATE_22050  (5)
#define SPRD_CODEC_RATE_24000  (4)
#define SPRD_CODEC_RATE_32000  (3)
#define SPRD_CODEC_RATE_44100  (2)
#define SPRD_CODEC_RATE_48000  (1)
#define SPRD_CODEC_RATE_96000  (0)

/* AUD_TOP_CTL */
#define DAC_EN_L		(0)
#define ADC_EN_L		(1)
#define DAC_EN_R		(2)
#define ADC_EN_R		(3)
#define ADC_SINC_SEL		(8)
#define ADC_SINC_SEL_ADC	(0)
#define ADC_SINC_SEL_DAC	(1)
#define ADC_SINC_SEL_D		(2)
#define ADC_SINC_SEL_MASK	(0x3)
#define ADC1_EN_L		(10)
#define ADC1_EN_R		(11)
#define ADC1_SINC_SEL		(14)
#define ADC1_SINC_SEL_ADC	(0)
#define ADC1_SINC_SEL_DAC	(1)
#define ADC1_SINC_SEL_D		(2)
#define ADC1_SINC_SEL_MASK	(0x3)

/* AUD_I2S_CTL */
#define ADC_LR_SEL		(2)
#define DAC_LR_SEL		(1)

/* AUD_DAC_CTL */
#define DAC_MUTE_EN		(15)
#define DAC_MUTE_CTL		(14)
#define DAC_FS_MODE		(0)
#define DAC_FS_MODE_96k		(0)
#define DAC_FS_MODE_48k		(1)
#define DAC_FS_MODE_MASK	(0xf)

/* AUD_ADC_CTL */
#define ADC_SRC_N		(0)
#define ADC_SRC_N_MASK		(0xf)
#define ADC1_SRC_N		(4)
#define ADC1_SRC_N_MASK		(0xf)

#define ADC_FS_MODE_48k		(0xc)
#define ADC_FS_MODE		(0)


/* AUD_LOOP_CTL */
#define AUD_LOOP_TEST		(0)
#define LOOP_ADC_PATH_SEL	(9)
#define LOOP_PATH_SEL		(1)
#define LOOP_PATH_SEL_MASK	(0x3)
/* AUD_AUD_STS0 */
#define DAC_MUTE_U_MASK		(5)
#define DAC_MUTE_D_MASK		(4)
#define DAC_MUTE_U_RAW		(3)
#define DAC_MUTE_D_RAW		(2)
#define DAC_MUTE_ST		(0)
#define DAC_MUTE_ST_MASK	(0x3)

/* AUD_INT_CLR */
/* AUD_INT_EN */
#define DAC_MUTE_U		(1)
#define DAC_MUTE_D		(0)

/*AUD_DMIC_CTL*/
#define ADC_DMIC_CLK_MODE	(0)
#define ADC_DMIC_CLK_MODE_MASK	(0x3)
#define ADC_DMIC_LR_SEL		(2)
#define ADC1_DMIC_CLK_MODE	(3)
#define ADC1_DMIC_CLK_MODE_MASK	(0x3)
#define ADC1_DMIC_LR_SEL	(5)
#define ADC_DMIC_EN		(6)
#define ADC1_DMIC1_EN		(7)

/* AUD_ADC1_I2S_CTL */
#define ADC1_LR_SEL		(0)

/*DAC_SDM_DC_L*/
#define DAC_SDM_DC_L		(0)
#define DAC_SDM_DC_L_MASK	(0xffff)
/*DAC_SDM_DC_H*/
#define DAC_SDM_DC_R		(0)
#define DAC_SDM_DC_R_MASK	(0xff)

/*VB_V*/
#define LDO_V_3000		(4)
#define LDO_V_3025		(5)
#define LDO_V_3050		(6)
#define LDO_V_3075		(7)
#define LDO_V_3100		(8)
#define LDO_V_3125		(9)
#define LDO_V_3150		(10)
#define LDO_V_3175		(11)
#define LDO_V_3200		(12)
#define LDO_V_3225		(13)
#define LDO_V_3250		(14)
#define LDO_V_3275		(15)
#define LDO_V_3300		(16)
#define LDO_V_3325		(17)
#define LDO_V_3350		(18)
#define LDO_V_3375		(19)
#define LDO_V_3400		(20)
#define LDO_V_3425		(21)
#define LDO_V_3450		(22)
#define LDO_V_3475		(23)
#define LDO_V_3500		(24)
#define LDO_V_3525		(25)
#define LDO_V_3550		(26)
#define LDO_V_3575		(27)
#define LDO_V_3600		(28)


#define MIC_LDO_V_21		(0)
#define MIC_LDO_V_19		(1)
#define MIC_LDO_V_23		(2)
#define MIC_LDO_V_25		(3)

/******Summer Modify below******/
/*bits definitions for register ANA_PMU0*/
#define VB_EN				(15)
#define VB_NLEAK_PD			(14)
#define VB_HDMIC_SP_PD			(13)
#define BG_EN				(12)
#define BIAS_EN				(11)
#define MICBIAS_EN			(10)
#define HMIC_BIAS_EN			(9)
#define HMIC_SLEEP_EN			(8)
#define MIC_SLEEP_EN			(7)
#define VBG_SEL				(6)
#define VBG_SEL_MASK			(1)
#define VBG_TEMP_BIASTUNE		(5)

#define VBG_TEMP_TUNE			(3)
#define VBG_TEMP_TUNE_MASK		(3)
#define VBG_TEMP_TUNE_NORMAL		(0)
#define VBG_TEMP_TUNE_TC_REDUCE		(1)
#define VBG_TEMP_TUNE_TC_REDUCE_MORE	(2)
#define VBG_TEMP_TUNE_TC_ENHANCE	(3)

#define MICBIAS_PLGB			(2)
#define HMICBIAS_VREF_SEL		(1)
#define HMIC_COMP_MODE1_EN		(0)


/*bits definitions for register ANA_PMU1*/
#define VB_CAL				(11)
#define VB_CAL_MASK			(0X1F)

#define VB_V				(6)
#define VB_V_MASK			(0X1F)

#define HMIC_BIAS_V			(3)
#define HMIC_BIAS_V_MASK		(0X7)

#define MICBIAS_V			(0)
#define MICBIAS_V_MASK			(0X7)


/*bits definitions for register ANA_PMU2*/
#define HP_IB				(14)
#define HP_IB_MASK			(0X3)
#define HP_IB_X1			(0)
#define HP_IB_X2			(3)

#define HP_IBCUR3			(11)
#define HP_IBCUR3_MASK			(0X7)
#define HP_IBCUR3_5UA			(0X0)
#define HP_IBCUR3_7P5UA			(0X1)
#define HP_IBCUR3_10UA			(0X2)

#define PA_AB_I				(9)
#define PA_AB_I_MASK			(0X3)

#define ADPGA_IBIAS_SEL			(5)
#define ADPGA_IBIAS_SEL_MASK		(0XF)
#define ADPGA_IBIAS_SEL_PGA_7P5_ADC_3P75	(5)

#define DA_IG				(3)
#define DA_IG_MASK			(0X3)

#define DRV_PM_SEL			(1)
#define DRV_PM_SEL_MASK			(0X3)
#define DRV_PM_SEL_1			(0X1)
#define DRV_PM_SEL_3			(0X3)


/*bits definitions for register ANA_PMU3*/
#define PA_OTP_PD			(15)

#define PA_OTP_T			(12)
#define PA_OTP_T_MASK			(0X7)

#define PA_OVP_PD			(11)
#define PA_OVP_THD			(10)

#define PA_OVP_V			(7)
#define PA_OVP_V_MASK			(0X7)
#define PA_OVP_V_5P8			(0)
#define PA_OVP_V_6P0			(1)
#define PA_OVP_V_6P2			(2)
#define PA_OVP_V_6P4			(3)
#define PA_OVP_V_6P6			(4)
#define PA_OVP_V_6P8			(5)
#define PA_OVP_V_7P0			(6)
#define PA_OVP_V_7P2			(7)

#define PA_OCP_PD			(6)
#define PA_OCP_S			(5)
#define DRV_OCP_PD			(4)

#define DRV_OCP_MODE			(2)
#define DRV_OCP_MODE_MASK		(0X3)
#define DRV_OCP_MODE_156MA		(0x2)

#define PA_VCM_V			(0)
#define PA_VCM_V_MASK			(0X3)


/*bits definitions for register ANA_PMU4*/
#define PA_KSEL				(13)
#define PA_KSEL_MASK			(0X3)

#define PA_DEGSEL			(11)
#define PA_DEGSEL_MASK			(0X3)

#define PA_EMI_L			(8)
#define PA_EMI_L_MASK			(0X7)

#define PA_SS_EN			(7)
#define PA_SS_RST			(6)

#define PA_SS_F				(4)
#define PA_SS_F_MASK			(0X3)

#define PA_SS_32K_EN			(3)

#define PA_SS_T				(0)
#define PA_SS_T_MASK			(0X7)


/*bits definitions for register ANA_PMU5*/
#define PA_D_EN				(14)
#define PA_DFLCK_EN			(13)
#define PA_DFLCK_RSL			(12)

#define PA_DTRI_FC			(9)
#define PA_DTRI_FC_MASK			(0X7)

#define PA_DTRI_FF			(3)
#define PA_DTRI_FF_MASK			(0X3F)

#define PA_STOP_EN			(2)
#define PA_SH_DET_EN			(1)
#define PA_SL_DET_EN			(0)


/*bits definitions for register ANA_CLK0 summer*/
#define DIG_CLK_6P5M_EN		(14)
#define DIG_CLK_LOOP_EN		(13)
#define ANA_CLK_EN		(12)
#define AD_CLK_EN		(11)
#define AD_CLK_RST		(10)
#define DA_CLK_EN		(9)
#define DRV_CLK_EN		(8)
#define DCDCGEN_CLK_EN		(7)
#define DCDCMEM_CLK_EN		(6)
#define DCDCCORE_CLK_EN		(5)
#define DCDCCHG_CLK_EN		(4)

#define AD_CLK_F		(2)
#define AD_CLK_F_MASK		(0X3)
#define AD_CLK_F_HALF		(2)

#define DA_CLK_F		(0)
#define DA_CLK_F_MASK		(0X3)


/*bits definitions for register ANA_CDC0*/
#define ADPGA_IBIAS_EN		(15)
#define ADPGA_IBUF_EN		(14)
#define ADPGAL_EN		(13)
#define ADPGAR_EN		(12)

#define ADPGAL_BYP		(10)
#define ADPGAL_BYP_MASK		(0X3)

#define ADPGAR_BYP		(8)
#define ADPGAR_BYP_MASK		(0X3)
#define ADPGAR_BYP_NORMAL	(0)
#define ADPGAR_BYP_HDST2ADC	(1)
#define ADPGAR_BYP_ALL_DISCON	(2)

#define ADL_EN			(7)
#define ADL_RST			(6)
#define ADR_EN			(5)
#define ADR_RST			(4)
#define VREF_SFCUR		(3)
#define SHMIC_DPOP		(2)
#define SHMIC_DPOPVCM_EN	(1)


/*bits definitions for register ANA_CDC1*/
#define ADVCMI_INT_SEL		(12)
#define ADVCMI_INT_SEL_MASK	(0X3)

#define ADPGAL_G		(9)
#define ADPGAL_G_MASK		(0X7)

#define ADPGAR_G		(6)
#define ADPGAR_G_MASK		(0X7)

#define DALR_OS_D		(3)
#define DALR_OS_D_MASK		(0X7)
#define DALR_OS_D_0		(0)
#define DALR_OS_D_2		(0x2)

#define DAS_OS_D		(0)
#define DAS_OS_D_MASK		(0X7)
#define DAS_OS_D_0		(0)
#define DAS_OS_D_2		(0x2)


/*bits definitions for register ANA_CDC2*/
#define DAS_EN		(13)
#define DAL_EN		(12)
#define DAR_EN		(11)
#define HPL_FLOOPEN	(10)
#define HPL_FLOOP_END	(9)
#define HPR_FLOOPEN	(8)
#define HPR_FLOOP_END	(7)
#define RCV_FLOOPEN	(6)
#define RCV_FLOOP_END	(5)
#define HPL_EN		(4)
#define HPR_EN		(3)
#define HPBUF_EN	(2)
#define RCV_EN		(1)
#define PA_EN		(0)


/*bits definitions for register ANA_CDC3*/
#define DALR_OS_EN	(12)
#define DAS_OS_EN	(11)
#define PA_NG_EN	(10)
#define SDALHPL		(9)
#define SDARHPR		(8)
#define SDALRCV		(7)
#define SDAPA		(6)
#define SHMICPA_DEBUG	(5)
#define SMICDRV_DEBUG	(4)
#define SMIC1PGAL	(3)
#define SMIC2PGAR	(2)
#define SHMICPGAL	(1)
#define SHMICPGAR	(0)


/*bits definitions for register ANA_CDC4*/
#define PA_G		(12)
#define PA_G_MASK	(0XF)
#define PA_AB_G_MASK	(0X3)
#define PA_AB_G_0DB	(0X1)
#define PA_D_G		(14)

#define RCV_G		(8)
#define RCV_G_MASK	(0XF)
#define RCV_G_MUTE	(0xF)

#define HPL_G		(4)
#define HPL_G_MASK	(0XF)
#define HPL_G_MUTE	(0xF)

#define HPR_G		(0)
#define HPR_G_MASK	(0XF)
#define HPR_G_MUTE	(0xF)


/*bits definitions for register ANA_HDT0*/
#define HEDET_MUX2ADC_SEL_P	(15)
#define HEDET_BUF_EN		(14)
#define HEDET_BDET_EN		(13)
#define HEDET_V21_EN		(12)
#define HEDET_VREF_EN		(11)
#define HEDET_MICDET_EN		(10)
#define HEDET_V2AD_SCALE	(9)
#define HEDET_LDET_L_FILTER	(8)
#define HEDET_BUF_CHOP		(7)

#define HEDET_MUX2ADC_SEL			(4)
#define HEDET_MUX2ADC_SEL_MASK			(0X7)
#define HEDET_MUX2ADC_SEL_HEADMIC_IN_DETECT	(0)
#define HEDET_MUX2ADC_SEL_HEADSET_L_INT		(1)

#define HEDET_V21_SEL				(0)
#define HEDET_V21_SEL_MASK			(0XF)
#define HEDET_V21_SEL_1U			(0x2)

/*bits definitions for register ANA_HDT1*/
#define HEDET_MICDET_REF_SEL		(13)
#define HEDET_MICDET_REF_SEL_MASK	(0X7)
#define HEDET_MICDET_REF_SEL_2P6	(0x6)

#define HEDET_MICDET_HYS_SEL		(11)
#define HEDET_MICDET_HYS_SEL_MASK	(0X3)
#define HEDET_MICDET_HYS_SEL_20MV	(0x1)

#define HEDET_LDET_REFL_SEL		(8)
#define HEDET_LDET_REFL_SEL_MASK	(0X7)
#define HEDET_LDET_REFL_SEL_300MV	(0x6)
#define HEDET_LDET_REFL_SEL_50MV	(0x1)

#define HEDET_LDET_REFH_SEL		(6)
#define HEDET_LDET_REFH_SEL_MASK	(0X3)
#define HEDET_LDET_REFH_SEL_1P7		(0)
#define HEDET_LDET_REFH_SEL_1P9		(2)

#define HEDET_LDET_PU_PD		(4)
#define HEDET_LDET_PU_PD_MASK		(0X3)
#define HEDET_LDET_PU_PD_PU		(0)

#define HEDET_LDET_L_HYS_SEL		(2)
#define HEDET_LDET_L_HYS_SEL_MASK	(0X3)
#define HEDET_LDET_L_HYS_SEL_20MV	(0x1)

#define HEDET_LDET_H_HYS_SEL		(0)
#define HEDET_LDET_H_HYS_SEL_MASK	(0X3)
#define HEDET_LDET_H_HYS_SEL_20MV	(0x1)


/*bits definitions for register ANA_HDT2*/
#define CHG_PROC_STS_BYPASS		(13)

#define HEDET_JACK_TYPE			(11)
#define HEDET_JACK_TYPE_MASK		(0X3)

#define HEDET_BDET_REF_SEL		(7)
#define HEDET_BDET_REF_SEL_MASK		(0XF)

#define HEDET_BDET_HYS_SEL		(5)
#define HEDET_BDET_HYS_SEL_MASK		(0X3)
#define HEDET_BDET_HYS_SEL_20MV		(0x1)

#define PLGPD_EN			(4)
#define HP_DRVIVER_EN			(3)
#define HPL_EN_D2HDT_EN			(2)

#define HPL_EN_D2HDT_T			(0)
#define HPL_EN_D2HDT_T_MASK		(0X3)


/*bits definitions for register ANA_DCL0 summer*/
#define DCL_EN		(6)
#define DCL_RST		(5)

#define DRV_SOFT_T	(2)
#define DRV_SOFT_T_MASK	(0X7)

#define DRV_SOFT_EN	(1)
#define DPOP_AUTO_RST	(0)

/*bits definitions for register ANA_DCL1*/
#define PACAL_EN		(10)

#define PACAL_DIV		(8)
#define PACAL_DIV_MASK		(0X3)

#define PA_OVP_ABMOD_PD		(7)

#define PA_OVP_ABMOD_T		(4)
#define PA_OVP_ABMOD_T_MASK	(0X3)

#define PA_OVP_DEG_EN		(3)

#define PA_OVP_DEG_T		(0)
#define PA_OVP_DEG_T_MASK	(0X7)


/*bits definitions for register ANA_DCL2*/
#define PA_OTP_DEG_EN			(12)

#define PA_OTP_DEG_T			(9)
#define PA_OTP_DEG_T_MASK		(0X7)

#define PA_OTP_MUTE_EN			(8)
#define PA_OCP_DEG_EN			(7)

#define PA_OCP_DEG_T			(4)
#define PA_OCP_DEG_T_MASK		(0X7)

#define PA_OCP_MUTE_EN			(3)

#define PA_OCP_MUTE_T			(0)
#define PA_OCP_MUTE_T_MASK		(0X7)


/*bits definitions for register ANA_DCL3*/

/*bits definitions for register ANA_DCL4*/

/*bits definitions for register ANA_DCL5*/
#define HPL_RDAC_START			(15)
#define HPR_RDAC_START			(14)
#define HP_DPOP_FDIN_EN			(13)
#define HP_DPOP_FDOUT_EN		(12)

#define HP_DPOP_GAIN_N1			(9)
#define HP_DPOP_GAIN_N1_MASK		(0X7)

#define HP_DPOP_GAIN_N2			(6)
#define HP_DPOP_GAIN_N2_MASK		(0X7)

#define HP_DPOP_GAIN_T			(3)
#define HP_DPOP_GAIN_T_MASK		(0X7)

#define HPL_RDAC_STS			(2)
#define HPR_RDAC_STS			(1)

/*bits definitions for register ANA_DCL6*/
#define CALDC_WAIT_T		(12)
#define CALDC_WAIT_T_MASK	(0X7)

#define HPL_DPOP_CLKN1		(10)
#define HPL_DPOP_CLKN1_MASK	(0X3)

#define HPL_DPOP_N1		(8)
#define HPL_DPOP_N1_MASK	(0X3)

#define HPL_DPOP_VAL1		(5)
#define HPL_DPOP_VAL1_MASK	(0X7)

#define HPL_DPOP_CLKN2		(3)
#define HPL_DPOP_CLKN2_MASK	(0X3)

#define HPL_DPOP_N2		(1)
#define HPL_DPOP_N2_MASK	(0X3)


/*bits definitions for register ANA_DCL7*/
#define DEPOPL_PCUR_OPT		(13)
#define DEPOPL_PCUR_OPT_MASK	(0X3)

#define DEPOPR_PCUR_OPT		(11)
#define DEPOPR_PCUR_OPT_MASK	(0X3)

#define HPR_DPOP_CLKN1		(9)
#define HPR_DPOP_CLKN1_MASK	(0X3)

#define HPR_DPOP_N1		(7)
#define HPR_DPOP_N1_MASK	(0X3)

#define HPR_DPOP_VAL1		(4)
#define HPR_DPOP_VAL1_MASK	(0X7)

#define HPR_DPOP_CLKN2		(2)
#define HPR_DPOP_CLKN2_MASK	(0X3)

#define HPR_DPOP_N2		(0)
#define HPR_DPOP_N2_MASK	(0X3)


/*bits definitions for register ANA_STS0*/
#define HP_DEPOP_WAIT_T1		(13)
#define HP_DEPOP_WAIT_T1_MASK		(0X7)
#define HP_DEPOP_WAIT_T2		(10)
#define HP_DEPOP_WAIT_T2_MASK		(0X7)

#define HP_DEPOP_WAIT_T3		(8)
#define HP_DEPOP_WAIT_T3_MASK		(0X3)

#define HP_DEPOP_WAIT_T4		(6)
#define HP_DEPOP_WAIT_T4_MASK		(0X3)

#define DC_CALI_IDACVAL			(3)
#define DC_CALI_IDACVAL_MASK		(0X7)

#define DC_CALI_IDAC_CURSEL		(1)
#define DC_CALI_IDAC_CURSEL_MASK	(0X3)

#define DC_CALI_RDACI_ADJ		(0)

/*bits definitions for register ANA_STS1*/

/*bits definitions for register ANA_STS2*/
#define CALDC_START		(15)
#define CALDC_EN		(14)
#define CALDC_ENO		(13)
#define DCCAL_STS		(12)
#define DCCALI_STS_BYPASS	(11)
#define HP_DPOP_DVLD		(10)
#define DEPOP_CHG_START		(9)
#define DEPOP_CHG_EN		(8)
#define PLUGIN			(7)
#define DEPOP_EN		(6)
#define DEPOP_CHG_STS		(5)
#define RCV_DPOP_DVLD		(4)
#define HPL_PU_ENB		(3)
#define HPR_PU_ENB		(2)
#define INSBUF_EN		(1)


/*bits definitions for register ANA_STS3*/
#define DEPOP_BIAS_SEL		(8)
#define DEPOP_BIAS_SEL_MASK	(0X3)
#define DEPOP_BIAS_SEL_2P5UA	(0X1)

#define DEPOP_OPA_SEL		(6)
#define DEPOP_OPA_SEL_MASK	(0X3)

#define HWSW_SEL		(0)
#define HWSW_MASK		(0X2F)


/*bits definitions for register ANA_STS4*/

/*bits definitions for register ANA_STS5*/

/*bits definitions for register ANA_STS6*/
#define PACAL_DVLD		(2)

/*bits definitions for register ANA_STS7*/
#define HEAD_INSERT_ALL		(12)
#define HEAD_INSERT3		(11)
#define HEAD_INSERT2		(10)
#define HEAD_INSERT		(9)
#define HEAD_BUTTON		(8)
#define PA_SH_FLAG		(7)
#define PA_SL_FLAG		(6)
#define PA_OVP_FLAG		(5)
#define PA_OTP_FLAG		(4)
#define DRV_OCP_FLAG_SPK	(2)
#define DRV_OCP_FLAG_HPRCV	(0)
#define DRV_OCP_FLAG_MASK	(0x3)


/*bits definitions for register ANA_CLK1*/
#define DCDCGEN_CLK_F		(14)
#define DCDCGEN_CLK_F_MASK	(0X3)

#define DCDCCORE_CLK_F		(12)
#define DCDCCORE_CLK_F_MASK	(0X3)

#define DCDCCHG_CLK_F		(10)
#define DCDCCHG_CLK_F_MASK	(0X3)

#define PA_CLK_F		(8)
#define PA_CLK_F_MASK		(0X3)

#define CLK_PN_SEL		(0)
#define CLK_PN_SEL_MASK		(0XFF)



/**********************************************************************/

#define SPRD_CODEC_DP_BASE (CODEC_DP_BASE)
#define AUD_TOP_CTL		(SPRD_CODEC_DP_BASE + 0x0000)
#define AUD_AUD_CTR		(SPRD_CODEC_DP_BASE + 0x0004)
#define AUD_I2S_CTL		(SPRD_CODEC_DP_BASE + 0x0008)
#define AUD_DAC_CTL		(SPRD_CODEC_DP_BASE + 0x000C)
#define AUD_SDM_CTL0		(SPRD_CODEC_DP_BASE + 0x0010)
#define AUD_SDM_CTL1		(SPRD_CODEC_DP_BASE + 0x0014)
#define AUD_ADC_CTL		(SPRD_CODEC_DP_BASE + 0x0018)
#define AUD_LOOP_CTL		(SPRD_CODEC_DP_BASE + 0x001C)
#define AUD_AUD_STS0		(SPRD_CODEC_DP_BASE + 0x0020)
#define AUD_INT_CLR		(SPRD_CODEC_DP_BASE + 0x0024)
#define AUD_INT_EN		(SPRD_CODEC_DP_BASE + 0x0028)
#define AUDIF_FIFO_CTL		(SPRD_CODEC_DP_BASE + 0x002C)
#define AUD_DMIC_CTL		(SPRD_CODEC_DP_BASE + 0x0030)
#define AUD_ADC1_I2S_CTL		(SPRD_CODEC_DP_BASE + 0x0034)
#define AUD_DAC_SDM_L		(SPRD_CODEC_DP_BASE + 0x0038)
#define AUD_DAC_SDM_H		(SPRD_CODEC_DP_BASE + 0x003C)
#define SPRD_CODEC_DP_END	(SPRD_CODEC_DP_BASE + 0x0040)
#define IS_SPRD_CODEC_DP_RANG(reg) (((reg) >= SPRD_CODEC_DP_BASE) \
	&& ((reg) < SPRD_CODEC_DP_END))

#define SPRD_CODEC_AP_BASE (CODEC_AP_BASE)
#define ANA_PMU0                                (SPRD_CODEC_AP_BASE + 0x0000)
#define ANA_PMU1                                (SPRD_CODEC_AP_BASE + 0x0004)
#define ANA_PMU2                                (SPRD_CODEC_AP_BASE + 0x0008)
#define ANA_PMU3                                (SPRD_CODEC_AP_BASE + 0x000C)
#define ANA_PMU4                                (SPRD_CODEC_AP_BASE + 0x0010)
#define ANA_PMU5                                (SPRD_CODEC_AP_BASE + 0x0014)
#define ANA_CLK0                                (SPRD_CODEC_AP_BASE + 0x0018)
#define ANA_CDC0                                (SPRD_CODEC_AP_BASE + 0x001C)
#define ANA_CDC1                                (SPRD_CODEC_AP_BASE + 0x0020)
#define ANA_CDC2                                (SPRD_CODEC_AP_BASE + 0x0024)
#define ANA_CDC3                                (SPRD_CODEC_AP_BASE + 0x0028)
#define ANA_CDC4                                (SPRD_CODEC_AP_BASE + 0x002C)
#define ANA_HDT0                                (SPRD_CODEC_AP_BASE + 0x0030)
#define ANA_HDT1                                (SPRD_CODEC_AP_BASE + 0x0034)
#define ANA_HDT2                                (SPRD_CODEC_AP_BASE + 0x0038)
#define ANA_DCL0                                (SPRD_CODEC_AP_BASE + 0x003C)
#define ANA_DCL1                                (SPRD_CODEC_AP_BASE + 0x0040)
#define ANA_DCL2                                (SPRD_CODEC_AP_BASE + 0x0044)
#define ANA_DCL4                                (SPRD_CODEC_AP_BASE + 0x004C)
#define ANA_DCL5                                (SPRD_CODEC_AP_BASE + 0x0050)
#define ANA_DCL6                                (SPRD_CODEC_AP_BASE + 0x0054)
#define ANA_DCL7                                (SPRD_CODEC_AP_BASE + 0x0058)
#define ANA_STS0                                (SPRD_CODEC_AP_BASE + 0x005C)
#define ANA_STS2                                (SPRD_CODEC_AP_BASE + 0x0064)
#define ANA_STS3                                (SPRD_CODEC_AP_BASE + 0x0068)
#define ANA_STS4                                (SPRD_CODEC_AP_BASE + 0x006C)
#define ANA_STS5                                (SPRD_CODEC_AP_BASE + 0x0070)
#define ANA_STS6                                (SPRD_CODEC_AP_BASE + 0x0074)
#define ANA_STS7                                (SPRD_CODEC_AP_BASE + 0x0078)
#define ANA_CLK1                                (SPRD_CODEC_AP_BASE + 0x007C)

#define SPRD_CODEC_AP_ANA_END	(SPRD_CODEC_AP_BASE + 0x0080)

#define AUD_CFGA_REG_BASE (CODEC_AP_BASE + 0x100)
#define AUD_CFGA_CLR                  (AUD_CFGA_REG_BASE + 0x0000)
#define AUD_CFGA_HID_CFG0             (AUD_CFGA_REG_BASE + 0x0004)
#define AUD_CFGA_HID_CFG1             (AUD_CFGA_REG_BASE + 0x0008)
#define AUD_CFGA_HID_CFG2             (AUD_CFGA_REG_BASE + 0x000C)
#define AUD_CFGA_HID_CFG3             (AUD_CFGA_REG_BASE + 0x0010)
#define AUD_CFGA_HID_CFG4             (AUD_CFGA_REG_BASE + 0x0014)
#define AUD_CFGA_HID_CFG5             (AUD_CFGA_REG_BASE + 0x0018)
#define AUD_CFGA_HID_STS0             (AUD_CFGA_REG_BASE + 0x001C)
#define AUD_CFGA_PRT_CFG_0            (AUD_CFGA_REG_BASE + 0x0020)
#define AUD_CFGA_PRT_CFG_1            (AUD_CFGA_REG_BASE + 0x0024)
#define AUD_CFGA_RD_STS               (AUD_CFGA_REG_BASE + 0x0028)
#define AUD_CFGA_INT_MODULE_CTRL      (AUD_CFGA_REG_BASE + 0x002C)
#define AUD_CFGA_LP_MODULE_CTRL       (AUD_CFGA_REG_BASE + 0x0030)
#define AUD_CFGA_ANA_ET2                       (AUD_CFGA_REG_BASE + 0x0034)
#define AUD_CFGA_CLK_EN                        (AUD_CFGA_REG_BASE + 0x0038)
#define AUD_CFGA_SOFT_RST                  (AUD_CFGA_REG_BASE + 0x003C)
#define SPRD_CODEC_AP_END	(AUD_CFGA_REG_BASE + 0x0040)
#define IS_SPRD_CODEC_AP_RANG(reg) (((reg) >= SPRD_CODEC_AP_BASE) \
	&& ((reg) < SPRD_CODEC_AP_END))


/******Summer Modify above******/

/*bits definitions for register AUD_CFGA_CLR*/
#define AUD_A_INT_CLR		(0)
#define AUD_A_INT_CLR_MASK	(0xFF)

/*bits definitions for register AUD_CFGA_PRT_CFG_0*/
#define AUD_PRT_EN		(3)

/*bits definitions for register AUD_CFGA_RD_STS*/
#define AUD_IRQ_MSK		(0)
#define AUD_IRQ_MSK_MASK	(0xFF)

/*bits definitions for register AUD_CFGA_INT_MODULE_CTRL*/
#define AUD_A_INT_EN		(0)
#define AUD_A_INT_EN_MASK	(0x7F)

#define AUDIO_RCV_DEPOP_IRQ	(7)
#define AUDIO_PACAL_IRQ		(6)
#define AUDIO_HP_DPOP_IRQ	(5)
#define OVP_IRQ		(4)
#define OTP_IRQ		(3)
#define PA_OCP_IRQ		(1)
#define HP_EAR_OCP_IRQ		(0)

/*bits definitions for register AUD_CFGA_LP_MODULE_CTRL*/
#define AUDIFA_ADCR_EN		(5)
#define AUDIFA_DACR_EN		(4)
#define AUDIFA_ADCL_EN		(3)
#define AUDIFA_DACL_EN		(2)
#define AUDIO_ADIE_LOOP_EN	(0)

/*bits definitions for register ANA_ET2*/
#define AUD_DAS_MIX_SEL		(0)
#define AUD_DAS_MIX_SEL_MASK	(0x3)
#define AUD_DAS_MIX_SEL_LR	(0)
#define AUD_DAS_MIX_SEL_2L	(1)
#define AUD_DAS_MIX_SEL_2R	(2)
#define AUD_DAS_MIX_SEL_0	(3)

/*bits definitions for register AUD_CFGA_CLK_EN summer*/
#define CLK_AUD_6P5M_EN		(4)
#define CLK_AUD_LOOP_EN		(3)
#define CLK_AUD_HID_EN		(2)
#define CLK_AUD_1K_EN		(1)
#define CLK_AUD_32K_EN		(0)

/*bits definitions for register  AUD_CFGA_SOFT_RST summer*/
#define DAC_POST_SOFT_RST	(5)
#define DIG_6P5M_SOFT_RST	(4)
#define AUD_1K_SOFT_RST		(1)
#define AUD_32K_SOFT_RST	(0)

#define SPRD_CODEC_IIS1_ID  111

unsigned long sprd_get_codec_dp_base(void);

struct snd_kcontrol;
struct snd_ctl_elem_value;
int sprd_codec_virt_mclk_mixer_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol);
void sprd_codec_wait(u32 wait_time);
#endif /* __SPRD_CODEC_H */
