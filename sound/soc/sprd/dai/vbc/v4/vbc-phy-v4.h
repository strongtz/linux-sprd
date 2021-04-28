/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-phy-r3p0.h
 *
 * SPRD SoC VBC -- SpreadTrum SOC R3P0 VBC driver function.
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

#ifndef __VBC_V4_PHY_DRV_H
#define __VBC_V4_PHY_DRV_H

#include "sprd-audio.h"
#include "audio-sipc.h"

#define SPRD_VBC_VERSION	"vbc.v4"

/**********************************************************
 * ap defines
 *********************************************************/
/* fifo ap reg address */
#define VBC_AP_ADDR_START VBC_AUDPLY_FIFO_WR_0
#define VBC_AUDPLY_FIFO_WR_0 0x0000
#define VBC_AUDPLY_FIFO_WR_1 0x0004
#define VBC_AUDRCD_FIFO_RD_0 0x0008
#define VBC_AUDRCD_FIFO_RD_1 0x000c
#define VBC_AUDPLY_FIFO_WR_2 0x0010
#define VBC_AUDPLY_FIFO_WR_3 0x0014
#define VBC_AUDRCD_FIFO_RD_2 0x0018
#define VBC_AUDRCD_FIFO_RD_3 0x001c

/* ap control reg address */
#define REG_VBC_AUDPLY_FIFO_CTRL 0x0020
#define REG_VBC_AUDRCD_FIFO_CTRL 0x0024
#define REG_VBC_AUD_SRC_CTRL 0x0028
#define REG_VBC_AUD_EN 0x002c
#define REG_VBC_AUD_CLR 0x0030
#define REG_VBC_AUDPLY_FIFO0_STS 0x0034
#define REG_VBC_AUDPLY_FIFO1_STS 0x0038
#define REG_VBC_AUDRCD_FIFO0_STS 0x003c
#define REG_VBC_AUDRCD_FIFO1_STS 0x0040
#define REG_VBC_AUD_INT_EN 0x0044
#define REG_VBC_AUD_INT_STS 0x0048
#define REG_VBC_AUD_CHNL_INT_SEL 0x004c
#define REG_VBC_AUD_DMA_EN 0x0050
#define REG_VBC_AUDPLY_FIFO23_CTRL 0x0054
#define REG_VBC_AUDRCD_FIFO23_CTRL 0x0058
#define REG_VBC_AUDPLY_FIFO2_STS 0x005c
#define REG_VBC_AUDPLY_FIFO3_STS 0x0060
#define REG_VBC_AUDRCD_FIFO2_STS 0x0064
#define REG_VBC_AUDRCD_FIFO3_STS 0x0068
#define VBC_AP_ADDR_END REG_VBC_AUDRCD_FIFO3_STS

/* bits definitions for register REG_VBC_AUDPLY_FIFO_CTRL */
#define AUDPLY01_FIFO_FULL_LVL_MASK (BIT(16) | BIT(17) |\
BIT(18) | BIT(19) | BIT(20) | BIT(21) | BIT(22) | BIT(23))
#define BITS_RF_AUDPLY_FIFO_FULL_LVL(_X_) ((_X_) << 16 &\
AUDPLY01_FIFO_FULL_LVL_MASK)
#define BIT_GET_RF_AUDPLY_FIFO_FULL_LVL(x) (((x) >> 16) & GENMASK(8, 0))
#define AUDPLY_DAT_FORMAT_MASK (BIT(9) | BIT(10))
#define AUDPLY_DAT_FORMAT_CTL_SHIFT (9)
#define BITS_RF_AUDPLY_DAT_FORMAT_CTL(_X_) ((_X_) << 9 & (BIT(9) | BIT(10)))
#define BIT_GET_RF_AUDPLY_DAT_FORMAT_CTL(x) (((x) >> 9) & GENMASK(1, 0))
#define AUDPLY01_FIFO_EMPTY_LVL_MASK (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7))
#define BITS_RF_AUDPLY_FIFO_EMPTY_LVL(_X_) ((_X_) &\
AUDPLY01_FIFO_EMPTY_LVL_MASK)
#define BIT_GET_RF_AUDPLY_FIFO_EMPTY_LVL(x) ((x) & GENMASK(8, 0))

/* bits definitions for register REG_VBC_AUDRCD_FIFO_CTRL */
#define AUDRCD01_FIFO_FULL_LVL_MASK (BIT(16) | BIT(17) |\
BIT(18) | BIT(19) | BIT(20) | BIT(21) | BIT(22) | BIT(23))
#define BITS_RF_AUDRCD_FIFO_FULL_LVL(_X_) ((_X_) << 16 &\
AUDRCD01_FIFO_FULL_LVL_MASK)
#define BIT_GET_RF_AUDRCD_FIFO_FULL_LVL(x) (((x) >> 16) & GENMASK(8, 0))
#define AUDRCD_DAT_FORMAT_MASK (BIT(9)|BIT(10))
#define AUDRCD_DAT_FORMAT_CTL_SHIFT (9)
#define BITS_RF_AUDRCD_DAT_FORMAT_CTL(_X_) ((_X_) << 9 & (BIT(9)|BIT(10)))
#define BIT_GET_RF_AUDRCD_DAT_FORMAT_CTL(x) (((x) >> 9) & GENMASK(1, 0))
#define AUDRCD01_FIFO_EMPTY_LVL_MASK (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7))
#define BITS_RF_AUDRCD_FIFO_EMPTY_LVL(_X_) ((_X_) &\
	AUDRCD01_FIFO_EMPTY_LVL_MASK)
#define BIT_GET_RF_AUDRCD_FIFO_EMPTY_LVL(x) ((x) & GENMASK(8, 0))

/* bits definitions for register REG_VBC_AUD_SRC_CTRL */
#define AUDPLY_AP01_SRC_MODE_MASK GENMASK(7, 4)
#define AUDPLY_AP_SRC_MODE(x) (x << 4)

/* bits definitions for register REG_VBC_AUD_EN */
#define BIT_RF_AUDRCD_AP_FIFO3_EN BIT(15)
#define BIT_RF_AUDRCD_AP_FIFO2_EN BIT(14)
#define BIT_RF_AUDRCD_AP_FIFO1_EN BIT(13)
#define BIT_RF_AUDRCD_AP_FIFO0_EN BIT(12)
#define BIT_RF_AUDPLY_AP_FIFO3_EN BIT(11)
#define BIT_RF_AUDPLY_AP_FIFO2_EN BIT(10)
#define BIT_RF_AUDPLY_AP_FIFO1_EN BIT(9)
#define BIT_RF_AUDPLY_AP_FIFO0_EN BIT(8)
#define BIT_AP01_RCD_SRC_EN_0 BIT(2)
#define BIT_AP01_RCD_SRC_EN_1 BIT(3)

/* bits definitions for register REG_VBC_AUD_CLR */
#define BITS_RF_AUD23_INT_CLR(_X_) ((_X_) << 8 & (BIT(8) | BIT(9)))
#define BIT_RF_AUDRCD23_FIFO_CLR BIT(7)
#define BIT_RF_AUDPLY23_FIFO_CLR BIT(6)
#define BITS_RF_AUD_INT_CLR(_X_) ((_X_) << 4 & (BIT(4) | BIT(5)))
#define BIT_RF_AUD_PLY_INT_CLR BIT(4)
#define BIT_RF_AUD_RCD_INT_CLR BIT(5)
#define BIT_RF_AUDRCD_FIFO_CLR BIT(3)
#define BIT_RF_AUDPLY_FIFO_CLR BIT(2)

/* bits definitions for register REG_VBC_AUDPLY_FIFO0_STS */
#define BIT_AUDPLY_FIFO0_AF_R BIT(21)
#define BIT_AUDPLY_FIFO0_AE_W BIT(20)
#define BIT_AUDPLY_FIFO0_FULL_R BIT(19)
#define BIT_AUDPLY_FIFO0_EMPTY_W BIT(18)
#define BITS_AUDPLY_FIFO0_ADDR_R(_X_) ((_X_) << 10 & (BIT(10) | BIT(11) |\
BIT(12) | BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BIT_GET_AUDPLY_FIFO0_ADDR_R(x) (((x) >> 10) & GENMASK(7, 0))
#define BITS_AUDPLY_FIFO0_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7)))
#define BIT_GET_AUDPLY_FIFO0_ADDR_W(x) ((x) & GENMASK(7, 0))

/* bits definitions for register REG_VBC_AUDPLY_FIFO1_STS */
#define BIT_AUDPLY_FIFO1_AF_R BIT(21)
#define BIT_AUDPLY_FIFO1_AE_W BIT(20)
#define BIT_AUDPLY_FIFO1_FULL_R BIT(19)
#define BIT_AUDPLY_FIFO1_EMPTY_W BIT(18)
#define BITS_AUDPLY_FIFO1_ADDR_R(_X_) ((_X_) << 10 & (BIT(10) | BIT(11) |\
BIT(12) | BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BIT_GET_AUDPLY_FIFO1_ADDR_R(x) (((x) >> 10) & GENMASK(7, 0))
#define BITS_AUDPLY_FIFO1_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7)))
#define BIT_GET_AUDPLY_FIFO1_ADDR_W(x) ((x) & GENMASK(7, 0))

/* bits definitions for register REG_VBC_AUDRCD_FIFO0_STS */
#define BIT_AUDRCD_FIFO0_AF_R BIT(21)
#define BIT_AUDRCD_FIFO0_AE_W BIT(20)
#define BIT_AUDRCD_FIFO0_FULL_R BIT(19)
#define BIT_AUDRCD_FIFO0_EMPTY_W BIT(18)
#define BITS_AUDRCD_FIFO0_ADDR_R(_X_) ((_X_) << 10 & (BIT(10) | BIT(11) |\
BIT(12) | BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BIT_GET_AUDRCD_FIFO0_ADDR_R(x) (((x) >> 10) & GENMASK(7, 0))
#define BITS_AUDRCD_FIFO0_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7)))
#define BIT_GET_AUDRCD_FIFO0_ADDR_W(x) ((x) & GENMASK(7, 0))

/* bits definitions for register REG_VBC_AUDRCD_FIFO1_STS */
#define BIT_AUDRCD_FIFO1_AF_R BIT(21)
#define BIT_AUDRCD_FIFO1_AE_W BIT(20)
#define BIT_AUDRCD_FIFO1_FULL_R BIT(19)
#define BIT_AUDRCD_FIFO1_EMPTY_W BIT(18)
#define BITS_AUDRCD_FIFO1_ADDR_R(_X_) ((_X_) << 10 & (BIT(10) | BIT(11) |\
BIT(12) | BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BIT_GET_AUDRCD_FIFO1_ADDR_R(x) (((x) >> 10) & GENMASK(7, 0))
#define BITS_AUDRCD_FIFO1_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7)))
#define BIT_GET_AUDRCD_FIFO1_ADDR_W(x) ((x) & GENMASK(7, 0))

/* bits definitions for register REG_VBC_AUD_INT_EN */
#define BITS_RF_AUD23_INT_EN(_X_) ((_X_) << 3 & (BIT(3) | BIT(4)))
#define BIT_RF_OFLD_INT_TYPE_SEL BIT(2)
#define BITS_RF_AUD_INT_EN(_X_) ((_X_) & (BIT(0) | BIT(1)))
#define BIT_GET_RF_AUD_INT_EN(x) ((x) & GENMASK(1, 0))
#define BIT_RF_AUD_PLY_INT BIT(0)
#define BIT_RF_AUD_RCD_INT BIT(1)

/* bits definitions for register REG_VBC_AUD_INT_STS */
#define BITS_AUD23_INT_STS_MSK(_X_) ((_X_) << 6 & (BIT(6) | BIT(7)))
#define BITS_AUD23_INT_STS_RAW(_X_) ((_X_) << 4 & (BIT(4) | BIT(5)))
#define BITS_AUD_INT_STS_MSK(_X_) ((_X_) << 2 & (BIT(2) | BIT(3)))
#define BIT_GET_AUD_INT_STS_MSK(x) (((x) >> 2) & GENMASK(1, 0))
#define BITS_AUD_INT_STS_RAW(_X_) ((_X_) & (BIT(0) | BIT(1)))
#define BIT_GET_AUD_INT_STS_RAW(x) ((x) & GENMASK(1, 0))

/* bits definitions for register REG_VBC_AUD_CHNL_INT_SEL */
#define BIT_AUDRCD_CHNL_INT_SEL BIT(1)
#define BIT_AUDPLY_CHNL_INT_SEL BIT(0)

/* bits definitions for register REG_VBC_AUD_DMA_EN */
#define BIT_RF_AUDRCD_DMA_AD3_EN BIT(7)
#define BIT_RF_AUDRCD_DMA_AD2_EN BIT(6)
#define BIT_RF_AUDPLY_DMA_DA3_EN BIT(5)
#define BIT_RF_AUDPLY_DMA_DA2_EN BIT(4)
#define BIT_RF_AUDRCD_DMA_AD1_EN BIT(3)
#define BIT_RF_AUDRCD_DMA_AD0_EN BIT(2)
#define BIT_RF_AUDPLY_DMA_DA1_EN BIT(1)
#define BIT_RF_AUDPLY_DMA_DA0_EN BIT(0)

/* bits definitions for register REG_VBC_AUDPLY_FIFO23_CTRL */
#define AUDPLY23_FIFO_FULL_LVL_MASK (BIT(16) | BIT(17) |\
BIT(18) | BIT(19) | BIT(20) | BIT(21) | BIT(22))
#define BITS_RF_AUDPLY_FIFO23_FULL_LVL(_X_) ((_X_) << 16 &\
AUDPLY23_FIFO_FULL_LVL_MASK)
#define AUDPLY23_FIFO_EMPTY_LVL_MASK (BIT(0) | BIT(1) |\
BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6))
#define BITS_RF_AUDPLY_FIFO23_EMPTY_LVL(_X_) ((_X_) &\
AUDPLY23_FIFO_EMPTY_LVL_MASK)

/* bits definitions for register REG_VBC_AUDRCD_FIFO23_CTRL */
#define AUDRCD23_FIFO_FULL_LVL_MASK (BIT(16) | BIT(17) |\
BIT(18) | BIT(19) | BIT(20) | BIT(21) | BIT(22))
#define BITS_RF_AUDRCD_FIFO23_FULL_LVL(_X_) ((_X_) << 16 &\
AUDRCD23_FIFO_FULL_LVL_MASK)
#define AUDRCD23_FIFO_EMPTY_LVL_MASK (BIT(0) | BIT(1) |\
BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6))
#define BITS_RF_AUDRCD_FIFO23_EMPTY_LVL(_X_) ((_X_) &\
AUDRCD23_FIFO_EMPTY_LVL_MASK)

/* bits definitions for register REG_VBC_AUDPLY_FIFO2_STS */
#define BIT_AUDPLY_FIFO2_AF_W BIT(21)
#define BIT_AUDPLY_FIFO2_AE_W BIT(20)
#define BIT_AUDPLY_FIFO2_FULL_W BIT(19)
#define BIT_AUDPLY_FIFO2_EMPTY_W BIT(18)
#define BITS_AUDPLY_FIFO2_ADDR_R(_X_) ((_X_) << 11 & (BIT(11) | BIT(12) |\
BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BITS_AUDPLY_FIFO2_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6)))

/* bits definitions for register REG_VBC_AUDPLY_FIFO3_STS */
#define BIT_AUDPLY_FIFO3_AF_W BIT(21)
#define BIT_AUDPLY_FIFO3_AE_W BIT(20)
#define BIT_AUDPLY_FIFO3_FULL_W BIT(19)
#define BIT_AUDPLY_FIFO3_EMPTY_W BIT(18)
#define BITS_AUDPLY_FIFO3_ADDR_R(_X_) ((_X_) << 11 & (BIT(11) | BIT(12) |\
BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BITS_AUDPLY_FIFO3_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6)))

/* bits definitions for register REG_VBC_AUDRCD_FIFO2_STS */
#define BIT_AUDRCD_FIFO2_AF_R BIT(21)
#define BIT_AUDRCD_FIFO2_AE_R BIT(20)
#define BIT_AUDRCD_FIFO2_FULL_R BIT(19)
#define BIT_AUDRCD_FIFO2_EMPTY_R BIT(18)
#define BITS_AUDRCD_FIFO2_ADDR_R(_X_) ((_X_) << 11 & (BIT(11) | BIT(12) |\
BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BITS_AUDRCD_FIFO2_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6)))

/* bits definitions for register REG_VBC_AUDRCD_FIFO3_STS */
#define BIT_AUDRCD_FIFO3_AF_R BIT(21)
#define BIT_AUDRCD_FIFO3_AE_R BIT(20)
#define BIT_AUDRCD_FIFO3_FULL_R BIT(19)
#define BIT_AUDRCD_FIFO3_EMPTY_R BIT(18)
#define BITS_AUDRCD_FIFO3_ADDR_R(_X_) ((_X_) << 11 & (BIT(11) | BIT(12) |\
BIT(13) | BIT(14) | BIT(15) | BIT(16) | BIT(17)))
#define BITS_AUDRCD_FIFO3_ADDR_W(_X_) ((_X_) & (BIT(0) | BIT(1) | BIT(2) |\
BIT(3) | BIT(4) | BIT(5) | BIT(6)))

#define IS_AP_VBC_RANG(reg) (((reg) >= VBC_AP_ADDR_START) &&\
((reg) <= VBC_AP_ADDR_END))

#define VBC_AUDPLAY01_FIFO_DEPTH 256
#define VBC_AUDRECORD01_FIFO_DEPTH 256

#define VBC_AUDPLAY23_FIFO_DEPTH 128
#define VBC_AUDRECORD23_FIFO_DEPTH 128

#define VBC_AUDPLY01_EMPTY_WATERMARK 160
#define VBC_AUDREC01_FULL_WATERMARK 160
#define VBC_AUDPLY23_EMPTY_WATERMARK 80
#define VBC_AUDREC23_FULL_WATERMARK 80
/*
 * Thefollowing 4 defines ignore it.
 * AUDPLYxx_FULL and AUDRECxx_EMPTY are not requied for
 * dma request. It can be used for just vbc interrupt.
 */
#define VBC_AUDPLY01_FULL_WATERMARK 80
#define VBC_AUDREC01_EMPTY_WATERMARK 80
#define VBC_AUDPLY23_FULL_WATERMARK 80
#define VBC_AUDREC23_EMPTY_WATERMARK 80

#define DEFAULT_RATE 48000
#define STREAM_CNT 2

enum {
	VBC_LEFT = 0,
	VBC_RIGHT = 1,
	VBC_ALL_CHAN = 2,
	VBC_CHAN_MAX
};

enum VBC_DAT_FORMAT {
	VBC_DAT_H24 = 0,
	VBC_DAT_L24,
	VBC_DAT_H16,
	VBC_DAT_L16,
	VBC_DAT_FMT_MAX,
};

enum VBC_AP_FIFO_ID {
	AP01_PLY_FIFO,
	AP01_REC_FIFO,
	AP23_PLY_FIFO,
	AP23_REC_FIFO,
	AP_FIFO_MAX,
};

enum VBC_FIFO_WARTERMARK_TYPE {
	FULL_WATERMARK,
	EMPTY_WATERMARK,
	WATERMARK_TYPE_MAX,
};

/**********************************************************
 * dsp defines
 *********************************************************/

#define VBC_DSP_ADDR_BASE 0x01700000
/* dsp fifo register address */
#define VBC_DSP_ADDR_START VBC_DAC0_FIFO_WR_0
#define VBC_DAC0_FIFO_WR_0 (VBC_DSP_ADDR_BASE + 0x0000)
#define VBC_DAC0_FIFO_WR_1 (VBC_DSP_ADDR_BASE + 0x0004)
#define VBC_DAC1_FIFO_WR_0 (VBC_DSP_ADDR_BASE + 0x0008)
#define VBC_DAC1_FIFO_WR_1 (VBC_DSP_ADDR_BASE + 0x000c)

#define VBC_ADC0_FIFO_RD_0 (VBC_DSP_ADDR_BASE + 0x0010)
#define VBC_ADC0_FIFO_RD_1 (VBC_DSP_ADDR_BASE + 0x0014)
#define VBC_ADC1_FIFO_RD_0 (VBC_DSP_ADDR_BASE + 0x0018)
#define VBC_ADC1_FIFO_RD_1 (VBC_DSP_ADDR_BASE + 0x001c)
#define VBC_ADC2_FIFO_RD_0 (VBC_DSP_ADDR_BASE + 0x0020)
#define VBC_ADC2_FIFO_RD_1 (VBC_DSP_ADDR_BASE + 0x0024)
#define VBC_ADC3_FIFO_RD_0 (VBC_DSP_ADDR_BASE + 0x0028)
#define VBC_ADC3_FIFO_RD_1 (VBC_DSP_ADDR_BASE + 0x002c)

#define VBC_AUD_PLY_DSP_FIFO_RD_0 (VBC_DSP_ADDR_BASE + 0x0030)
#define VBC_AUD_PLY_DSP_FIFO_RD_1 (VBC_DSP_ADDR_BASE + 0x0034)
#define VBC_AUD_RCD_DSP_FIFO_WR_0 (VBC_DSP_ADDR_BASE + 0x0038)
#define VBC_AUD_RCD_DSP_FIFO_WR_1 (VBC_DSP_ADDR_BASE + 0x003c)
/* dsp coefficent address */
/* 24bit */
#define VBC_DAC0_EQ6_ALC_COEF_START (VBC_DSP_ADDR_BASE + 0x0050)
#define VBC_DAC0_HPCOEF42 (VBC_DSP_ADDR_BASE + 0x00F8)
/* 16bit */
#define VBC_DAC0_ALC_RISE (VBC_DSP_ADDR_BASE + 0x00FC)
#define VBC_DAC0_EQ6_ALC_COEF_END (VBC_DSP_ADDR_BASE + 0x013c)
/* 24bit */
#define VBC_DAC0_EQ4_COEF_START (VBC_DSP_ADDR_BASE + 0x0140)
#define VBC_DAC0_EQ4_COEF_END (VBC_DSP_ADDR_BASE + 0x019c)
/* 24bit */
#define VBC_DAC0_MBDRC_BLEQ_DRC_COEF_START (VBC_DSP_ADDR_BASE + 0x01A0)
#define VBC_DAC0_MBDRC_BLEQ1_COEF11 (VBC_DSP_ADDR_BASE + 0x01CC)
/* 16bit */
#define VBC_DAC0_BLDRC_RISE (VBC_DSP_ADDR_BASE + 0x01D0)
#define VBC_DAC0_MBDRC_BLEQ_DRC_COEF_END (VBC_DSP_ADDR_BASE + 0x0210)
/* 24bit */
#define VBC_DAC0_MBDRC_CROSS_EQ_COEF_START (VBC_DSP_ADDR_BASE + 0x0214)
#define VBC_DAC0_MBDRC_CROSS_EQ_COEF_END (VBC_DSP_ADDR_BASE + 0x0390)
/* 16bit */
#define VBC_DAC0_MBDRC_CROS_DRC_COEF_START (VBC_DSP_ADDR_BASE + 0x0394)
#define VBC_DAC0_MBDRC_CROS_DRC_COEF_END (VBC_DSP_ADDR_BASE + 0x04a0)
/* 24bit */
#define VBC_DAC0_NCH_COEF_START (VBC_DSP_ADDR_BASE + 0x04a4)
#define VBC_DAC0_NCH_COEF_END (VBC_DSP_ADDR_BASE + 0x0590)
/* 24bit */
#define VBC_DAC1_EQ6_ALC_COEF_START (VBC_DSP_ADDR_BASE + 0x0594)
#define VBC_DAC1_HP_COEF42 (VBC_DSP_ADDR_BASE + 0x063c)
/* 16bit */
#define VBC_DAC1_ALC_RISE (VBC_DSP_ADDR_BASE + 0x0640)
#define VBC_DAC1_EQ6_ALC_COEF_END (VBC_DSP_ADDR_BASE + 0x0680)
/* 24bit */
#define VBC_DAC1_EQ4_COEF_START (VBC_DSP_ADDR_BASE + 0x0684)
#define VBC_DAC1_EQ4_COEF_END (VBC_DSP_ADDR_BASE + 0x06e0)
/* 24bit */
#define VBC_DAC1_MBDRC_COEF2_START (VBC_DSP_ADDR_BASE + 0x06e4)
#define VBC_DAC1_MBDRC_BLEQ1_COEF11 (VBC_DSP_ADDR_BASE + 0x0710)
/* 16bit */
#define VBC_DAC1_BLDRC_RISE (VBC_DSP_ADDR_BASE + 0x0714)
#define VBC_DAC1_MBDRC_BLEQ_DRC_COEF_END (VBC_DSP_ADDR_BASE + 0x0754)
/* 24bit */
#define VBC_DAC1_MBDRC_COEF0_START (VBC_DSP_ADDR_BASE + 0x0758)
#define VBC_DAC1_MBDRC_COEF0_END (VBC_DSP_ADDR_BASE + 0x08d4)
/* 16bit */
#define VBC_DAC1_MBDRC_CROS_DRC_COEF_START (VBC_DSP_ADDR_BASE + 0x08d8)
#define VBC_DAC1_MBDRC_CROS_DRC_COEF_END (VBC_DSP_ADDR_BASE + 0x09e4)
/* 24bit */
#define VBC_DAC1_NCH_COEF_START (VBC_DSP_ADDR_BASE + 0x09e8)
#define VBC_DAC1_NCH_COEF_END (VBC_DSP_ADDR_BASE + 0x0ad4)
/* 24bit */
#define VBC_ADC0_EQ6_COEF_START (VBC_DSP_ADDR_BASE + 0x0ad8)
#define VBC_ADC0_EQ6_COEF_END (VBC_DSP_ADDR_BASE + 0x0b80)
/* 24bit */
#define VBC_ADC1_EQ6_COEF_START (VBC_DSP_ADDR_BASE + 0x0b84)
#define VBC_ADC1_EQ6_COEF_END (VBC_DSP_ADDR_BASE + 0x0c2c)
/* 24bit */
#define VBC_ADC2_EQ6_COEF_START (VBC_DSP_ADDR_BASE + 0x0c30)
#define VBC_ADC2_EQ6_COEF_END (VBC_DSP_ADDR_BASE + 0x0cd8)
/* 24bit */
#define VBC_ADC3_EQ6_COEF_START (VBC_DSP_ADDR_BASE + 0x0cdc)
#define VBC_ADC3_EQ6_COEF_END (VBC_DSP_ADDR_BASE + 0x0d84)
/* 16bit */
#define VBC_ST_DRC_COEF_START (VBC_DSP_ADDR_BASE + 0x0d88)
#define VBC_ST_DRC_COEF_END (VBC_DSP_ADDR_BASE + 0x0dc8)
/* vbc dsp control register */
#define REG_VBC_DAC_DG_CFG (VBC_DSP_ADDR_BASE + 0x0DD8)
#define REG_VBC_DAC0_SMTH_DG_CFG (VBC_DSP_ADDR_BASE + 0x0DDC)
#define REG_VBC_DAC0_DGMIXER_DG_CFG (VBC_DSP_ADDR_BASE + 0x0DE4)
#define REG_VBC_DAC1_DGMIXER_DG_CFG (VBC_DSP_ADDR_BASE + 0x0DE8)
#define REG_VBC_DAC_DGMIXER_DG_STEP (VBC_DSP_ADDR_BASE + 0x0DF0)
#define REG_VBC_DRC_MODE (VBC_DSP_ADDR_BASE + 0x0DF4)
#define REG_VBC_DAC_EQ4_COEF_UPDT (VBC_DSP_ADDR_BASE + 0x0DF8)
#define REG_VBC_DAC_TONE_GEN_CTRL (VBC_DSP_ADDR_BASE + 0x0DFC)
#define REG_VBC_DAC0_TONE_GEN_PARAM0 (VBC_DSP_ADDR_BASE + 0x0E00)
#define REG_VBC_DAC0_TONE_GEN_PARAM1 (VBC_DSP_ADDR_BASE + 0x0E04)
#define REG_VBC_DAC0_TONE_GEN_PARAM2 (VBC_DSP_ADDR_BASE + 0x0E08)
#define REG_VBC_DAC0_TONE_GEN_PARAM3 (VBC_DSP_ADDR_BASE + 0x0E0C)
#define REG_VBC_DAC0_TONE_GEN_PARAM4 (VBC_DSP_ADDR_BASE + 0x0E10)
#define REG_VBC_DAC0_TONE_GEN_PARAM5 (VBC_DSP_ADDR_BASE + 0x0E14)
#define REG_VBC_DAC0_TONE_GEN_PARAM6 (VBC_DSP_ADDR_BASE + 0x0E18)
#define REG_VBC_DAC0_TONE_GEN_PARAM7 (VBC_DSP_ADDR_BASE + 0x0E1C)
#define REG_VBC_DAC0_TONE_GEN_PARAM8 (VBC_DSP_ADDR_BASE + 0x0E20)
#define REG_VBC_DAC0_TONE_GEN_PARAM9 (VBC_DSP_ADDR_BASE + 0x0E24)
#define REG_VBC_DAC0_MIXER_CFG (VBC_DSP_ADDR_BASE + 0x0E50)
#define REG_VBC_DAC1_MIXER_CFG (VBC_DSP_ADDR_BASE + 0x0E54)
#define REG_VBC_ADC_DG_CFG0 (VBC_DSP_ADDR_BASE + 0x0E58)
#define REG_VBC_ADC_DG_CFG1 (VBC_DSP_ADDR_BASE + 0x0E5C)
#define REG_VBC_FM_FIFO_CTRL (VBC_DSP_ADDR_BASE + 0x0E60)
#define REG_VBC_FM_MIXER_CTRL (VBC_DSP_ADDR_BASE + 0x0E64)
#define REG_VBC_FM_HPF_CTRL (VBC_DSP_ADDR_BASE + 0x0E68)
#define REG_VBC_ST_FIFO_CTRL (VBC_DSP_ADDR_BASE + 0x0E6C)
#define REG_VBC_ST_MIXER_CTRL (VBC_DSP_ADDR_BASE + 0x0E70)
#define REG_VBC_ST_HPF_CTRL (VBC_DSP_ADDR_BASE + 0x0E74)
#define REG_VBC_ST_DRC_MODE (VBC_DSP_ADDR_BASE + 0x0E78)
#define REG_VBC_ST_FIFO_STS (VBC_DSP_ADDR_BASE + 0x0E94)
#define REG_VBC_FM_FIFO_STS (VBC_DSP_ADDR_BASE + 0x0E98)
#define REG_VBC_SRC_MODE (VBC_DSP_ADDR_BASE + 0x0EC4)
#define REG_VBC_SRC_PAUSE (VBC_DSP_ADDR_BASE + 0x0EC8)
#define REG_VBC_PATH_CTRL0 (VBC_DSP_ADDR_BASE + 0x0ECC)
#define REG_VBC_DAC0_MTDG_CTRL (VBC_DSP_ADDR_BASE + 0x0ED0)
#define REG_VBC_DAC1_MTDG_CTRL (VBC_DSP_ADDR_BASE + 0x0ED4)
#define REG_VBC_DATAPATH_EN (VBC_DSP_ADDR_BASE + 0x0ED8)
#define REG_VBC_MODULE_EN0 (VBC_DSP_ADDR_BASE + 0x0EDC)
#define REG_VBC_MODULE_EN1 (VBC_DSP_ADDR_BASE + 0x0EE0)
#define REG_VBC_MODULE_EN2 (VBC_DSP_ADDR_BASE + 0x0EE4)
#define REG_VBC_MODULE_EN3 (VBC_DSP_ADDR_BASE + 0x0EE8)
#define REG_VBC_DAC0_FIFO_CFG (VBC_DSP_ADDR_BASE + 0x0EEC)
#define REG_VBC_DAC1_FIFO_CFG (VBC_DSP_ADDR_BASE + 0x0EF0)
#define REG_VBC_ADC0_FIFO_CFG0 (VBC_DSP_ADDR_BASE + 0x0EF4)
#define REG_VBC_ADC1_FIFO_CFG0 (VBC_DSP_ADDR_BASE + 0x0EF8)
#define REG_VBC_ADC2_FIFO_CFG0 (VBC_DSP_ADDR_BASE + 0x0EFC)
#define REG_VBC_ADC3_FIFO_CFG0 (VBC_DSP_ADDR_BASE + 0x0F00)
#define REG_VBC_AUDPLY_DSP_FIFO_CTRL (VBC_DSP_ADDR_BASE + 0x0F04)
#define REG_VBC_AUDRCD_DSP_FIFO_CTRL (VBC_DSP_ADDR_BASE + 0x0F08)
#define REG_VBC_PATH_CTRL1 (VBC_DSP_ADDR_BASE + 0x0F0C)
#define REG_VBC_DAC_NOISE_GEN_GAIN (VBC_DSP_ADDR_BASE + 0x0F10)
#define REG_VBC_DAC1_NOISE_GEN_SEED (VBC_DSP_ADDR_BASE + 0x0F1C)
#define REG_VBC_DAC0_NOISE_GEN_SEED (VBC_DSP_ADDR_BASE + 0x0F20)
#define REG_VBC_MODULE_EN4 (VBC_DSP_ADDR_BASE + 0x0F24)
#define REG_VBC_PATH_EX_FIFO_STATUS (VBC_DSP_ADDR_BASE + 0x0F28)
#define REG_VBC_DAC2ADC_LOOP_CTL (VBC_DSP_ADDR_BASE + 0x0F2C)
#define REG_VBC_AUDPLY_DAC_FIFO_CTL (VBC_DSP_ADDR_BASE + 0x0F38)
#define REG_VBC_ADC_AUDRCD_FIFO_STATUS (VBC_DSP_ADDR_BASE + 0x0F3C)
#define REG_VBC_DAC_SMTH_DG_STP (VBC_DSP_ADDR_BASE + 0x0F5C)
#define REG_VBC_DAC0_DGMIXER_DG_OUT_CFG (VBC_DSP_ADDR_BASE + 0x0F78)
#define REG_VBC_DAC1_DGMIXER_DG_OUT_CFG (VBC_DSP_ADDR_BASE + 0x0F7C)
#define REG_VBC_BAK_REG (VBC_DSP_ADDR_BASE + 0x0F80)
#define REG_VBC_ADC_IIS_AFIFO_STS (VBC_DSP_ADDR_BASE + 0x0F84)
#define REG_VBC_DAC_IIS_AFIFO_STS (VBC_DSP_ADDR_BASE + 0x0F88)
#define REG_VBC_PATH_EX_FIFO_STATUS1 (VBC_DSP_ADDR_BASE + 0x0F90)
#define REG_VBC_INNER_FIFO_STS1 (VBC_DSP_ADDR_BASE + 0x0F94)
#define REG_VBC_AUDRCD23_IN_SEL (VBC_DSP_ADDR_BASE + 0x0FC0)
#define REG_VBC_MODULE_CLR0 (VBC_DSP_ADDR_BASE + 0x0DCC)
#define REG_VBC_MODULE_CLR1 (VBC_DSP_ADDR_BASE + 0x0DD0)
#define REG_VBC_MODULE_CLR2 (VBC_DSP_ADDR_BASE + 0x0DD4)
#define REG_VBC_DAC0_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0E7C)
#define REG_VBC_DAC0_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0E80)
#define REG_VBC_ADC2_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0E84)
#define REG_VBC_ADC2_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0E88)
#define REG_VBC_ADC3_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0E8C)
#define REG_VBC_ADC3_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0E90)
#define REG_VBC_AUDPLY_OFLD_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0E9C)
#define REG_VBC_AUDPLY_OFLD_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0EA0)
#define REG_VBC_AUDRCD_OFLD_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0EA4)
#define REG_VBC_INT_EN (VBC_DSP_ADDR_BASE + 0x0EA8)
#define REG_VBC_INT_CLR (VBC_DSP_ADDR_BASE + 0x0EAC)
#define REG_VBC_DMA_EN (VBC_DSP_ADDR_BASE + 0x0EB0)
#define REG_VBC_INT_STS_RAW (VBC_DSP_ADDR_BASE + 0x0EB4)
#define REG_VBC_INT_STS_MSK (VBC_DSP_ADDR_BASE + 0x0EB8)
#define REG_VBC_IIS_CTRL (VBC_DSP_ADDR_BASE + 0x0EBC)
#define REG_VBC_IIS_IF_IN_SEL (VBC_DSP_ADDR_BASE + 0x0EC0)
#define REG_VBC_DAT_FORMAT_CTL (VBC_DSP_ADDR_BASE + 0x0F30)
#define REG_VBC_PATH_EX_FIFO_CLR (VBC_DSP_ADDR_BASE + 0x0F34)
#define REG_VBC_VERSION (VBC_DSP_ADDR_BASE + 0x0F40)
#define REG_VBC_DAC1_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0F44)
#define REG_VBC_DAC1_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0F48)
#define REG_VBC_ADC0_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0F4C)
#define REG_VBC_ADC0_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0F50)
#define REG_VBC_ADC1_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0F54)
#define REG_VBC_ADC1_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0F58)
#define REG_VBC_AUDRCD_OFLD_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0F60)
#define REG_VBC_AUDPLY_DSP_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0F64)
#define REG_VBC_AUDPLY_DSP_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0F68)
#define REG_VBC_AUDRCD_DSP_FIFO1_STS (VBC_DSP_ADDR_BASE + 0x0F6C)
#define REG_VBC_AUDRCD_DSP_FIFO0_STS (VBC_DSP_ADDR_BASE + 0x0F70)
#define REG_VBC_CHNL_INT_SEL (VBC_DSP_ADDR_BASE + 0x0F74)
#define REG_VBC_TDM_CTRL (VBC_DSP_ADDR_BASE + 0x0F98)
#define REG_VBC_WHOLE_FIFO_CFG (VBC_DSP_ADDR_BASE + 0x0F9C)
#define REG_IIS_CTL0 (VBC_DSP_ADDR_BASE + 0x0FA0)
#define REG_IIS_CTL1 (VBC_DSP_ADDR_BASE + 0x0FA4)
#define REG_IIS_DSPWAIT (VBC_DSP_ADDR_BASE + 0x0FB0)
#define REG_VBC_MODULE_EN_V4_LITE (VBC_DSP_ADDR_BASE + 0x0FB4)
#define REG_VBC_TDM_CTRL1 (VBC_DSP_ADDR_BASE + 0x0FB8)
#define REG_VBC_IIS_IN_STS (VBC_DSP_ADDR_BASE + 0x0F8C)
#define REG_VBC_TDM_CTRL2 (VBC_DSP_ADDR_BASE + 0x0FBC)
#define VBC_DSP_ADDR_END REG_VBC_TDM_CTRL2
#define IS_DSP_VBC_RANG(reg) (((reg) >= VBC_DSP_ADDR_START) &&\
((reg) <= VBC_DSP_ADDR_END))
/* bits defines define by dsp */

/**********************************************************
 **********************************************************
 * COMMUNICATION FOR AP AND DSP AUDIO
 **********************************************************
 *********************************************************/

/**********************************************************
 * cmds of sipc channel AMSG_CH_VBC_CTL
 *********************************************************/
#define SND_VBC_DSP_FUNC_STARTUP 4
#define SND_VBC_DSP_FUNC_SHUTDOWN 5
#define SND_VBC_DSP_FUNC_HW_PARAMS 6
#define SND_VBC_DSP_FUNC_HW_TRIGGER 7
#define SND_VBC_DSP_IO_KCTL_GET 8
#define SND_VBC_DSP_IO_KCTL_SET 9
#define SND_VBC_DSP_IO_SHAREMEM_GET 10
#define SND_VBC_DSP_IO_SHAREMEM_SET 11

/************************************************************
 * define for SND_VBC_DSP_IO_KCTL_GET / SND_VBC_DSP_IO_KCTL_SET
 ************************************************************/
enum KCTL_TYPE {
	SND_KCTL_TYPE_REG = 0,
	SND_KCTL_TYPE_MDG,
	SND_KCTL_TYPE_SRC,
	SND_KCTL_TYPE_DG,
	SND_KCTL_TYPE_SMTHDG,
	SND_KCTL_TYPE_SMTHDG_STEP,
	SND_KCTL_TYPE_MIXERDG_MAIN,
	SND_KCTL_TYPE_MIXERDG_MIX,
	SND_KCTL_TYPE_MIXERDG_STEP,
	SND_KCTL_TYPE_MIXER,
	SND_KCTL_TYPE_MUX_ADC_SOURCE,
	SND_KCTL_TYPE_MUX_DAC_OUT,
	SND_KCTL_TYPE_MUX_ADC,
	SND_KCTL_TYPE_MUX_FM,
	SND_KCTL_TYPE_MUX_ST,
	SND_KCTL_TYPE_MUX_LOOP_DA0,
	SND_KCTL_TYPE_MUX_LOOP_DA1,
	SND_KCTL_TYPE_MUX_LOOP_DA0_DA1,
	SND_KCTL_TYPE_MUX_AUDRCD,
	SND_KCTL_TYPE_MUX_TDM_AUDRCD23,
	SND_KCTL_TYPE_MUX_AP01_DSP,
	SND_KCTL_TYPE_MUX_IIS_TX,
	SND_KCTL_TYPE_MUX_IIS_RX,
	SND_KCTL_TYPE_IIS_PORT_DO,
	SND_KCTL_TYPE_VOLUME,
	SND_KCTL_TYPE_ADDER,
	SND_KCTL_TYPE_LOOPBACK_TYPE,
	SND_KCTL_TYPE_DATAPATH,
	SND_KCTL_TYPE_CALL_MUTE,
	SND_KCTL_TYPE_IIS_TX_WIDTH_SEL,
	SND_KCTL_TYPE_IIS_TX_LRMOD_SEL,
	SND_KCTL_TYPE_IIS_RX_WIDTH_SEL,
	SND_KCTL_TYPE_IIS_RX_LRMOD_SEL,
	SND_KCTL_TYPE_VBC_IIS_MASTER_START,
	/* not send to dsp, dsp only use at startup */
	SND_KCTL_TYPE_SBCPARA_SET,
	SND_KCTL_TYPE_MAIN_MIC_PATH_FROM,
	SND_KCTL_TYPE_IVSENCE_FUNC,
	SND_KCTL_TYPE_EXT_INNER_IIS_MST_SEL,
	SND_KCTL_TYPE_VBC_IIS_MASTER_WIDTH_SET,
	SND_KCTL_TYPE_END,
};

/* SND_KCTL_TYPE_REG */
struct sprd_vbc_kcontrol {
	u32 reg;
	u32 mask;
	u32 value;
};

/* SND_KCTL_TYPE_MDG */
enum VBC_MDG_ID_E {
	VBC_MDG_DAC0_DSP,
	VBC_MDG_DAC1_DSP,
	VBC_MDG_AP01,
	VBC_MDG_AP23,
	VBC_MDG_MAX
};

struct vbc_mute_dg_para {
	u32 vbc_startup_reload;
	enum VBC_MDG_ID_E mdg_id;
	u32 mdg_mute;
	u32 mdg_step;
};

/* SND_KCTL_TYPE_SRC */
enum SRC_MODE_E {
	SRC_MODE_48000,
	SRC_MODE_44100,
	SRC_MODE_32000,
	SRC_MODE_24000,
	SRC_MODE_22050,
	SRC_MODE_16000,
	SRC_MODE_12000,
	SRC_MODE_11025,
	SRC_MODE_NA,
	SRC_MODE_8000
};

enum VBC_SRC_ID_E {
	VBC_SRC_DAC0,
	VBC_SRC_DAC1,
	VBC_SRC_ADC0,
	VBC_SRC_ADC1,
	VBC_SRC_ADC2,
	VBC_SRC_ADC3,
	VBC_SRC_BT_DAC,
	VBC_SRC_BT_ADC,
	VBC_SRC_FM,
	VBC_SRC_MAX
};

struct vbc_src_para {
	enum VBC_SRC_ID_E src_id;
	int32_t fs;
};

/* SND_KCTL_TYPE_DG */
enum VBC_DG_ID_E {
	VBC_DG_DAC0,
	VBC_DG_DAC1,
	VBC_DG_ADC0,
	VBC_DG_ADC1,
	VBC_DG_ADC2,
	VBC_DG_ADC3,
	VBC_DG_FM,
	VBC_DG_ST,
	/* offload dg is software dg to dsp */
	OFFLOAD_DG,
	VBC_DG_MAX
};

struct vbc_dg_para {
	u32 vbc_startup_reload;
	enum VBC_DG_ID_E dg_id;
	u32 dg_left;
	u32 dg_right;
};

/* SND_KCTL_TYPE_SMTHDG */
enum VBC_SMTHDG_ID_E {
	VBC_SMTHDG_DAC0,
	VBC_SMTHDG_MAX
};

struct vbc_smthdg_para {
	enum VBC_SMTHDG_ID_E smthdg_id;
	u32 smthdg_left;
	u32 smthdg_right;
};

struct vbc_smthdg_step_para {
	enum VBC_SMTHDG_ID_E smthdg_id;
	u32 step;
};

struct vbc_smthdg_module {
	u32 vbc_startup_reload;
	struct vbc_smthdg_para smthdg_dg;
	struct vbc_smthdg_step_para smthdg_step;
};

/* SND_KCTL_TYPE_MIXERDG_MAIN / SND_KCTL_TYPE_MIXERDG_MIX */
enum VBC_MIXERDG_ID_E {
	VBC_MIXERDG_DAC0,
	VBC_MIXERDG_DAC1,
	VBC_MIXERDG_MAX
};

struct vbc_mixerdg_mainpath_para {
	enum VBC_MIXERDG_ID_E mixerdg_id;
	u32 mixerdg_main_left;
	u32 mixerdg_main_right;
};

struct vbc_mixerdg_mixpath_para {
	enum VBC_MIXERDG_ID_E mixerdg_id;
	u32 mixerdg_mix_left;
	u32 mixerdg_mix_right;
};

struct vbc_mixerdg_para {
	u32 vbc_startup_reload;
	enum VBC_MIXERDG_ID_E mixerdg_id;
	struct vbc_mixerdg_mainpath_para main_path;
	struct vbc_mixerdg_mixpath_para mix_path;
};

/* SND_KCTL_TYPE_MIXER */
enum VBC_MIXER_ID_E {
	VBC_MIXER0_DAC0,
	VBC_MIXER1_DAC0,
	VBC_MIXER0_DAC1,
	VBC_MIXER_ST,
	VBC_MIXER_FM,
	VBC_MIXER_MAX
};

enum MIXER_OPS_TYPE {
	NOT_MIX,
	INTERCHANGE,
	HALF_ADD,
	HALF_SUB,
	DATA_INV,
	INTERCHANGE_INV,
	HALF_ADD_INV,
	HALF_SUB_INV,
	MIXER_OPS_TYPE_MAX
};

struct vbc_mixer_para {
	u32 vbc_startup_reload;
	enum VBC_MIXER_ID_E mixer_id;
	enum MIXER_OPS_TYPE type;
};

/* SND_KCTL_TYPE_MUX_ADC_SOURCE */
enum VBC_MUX_ADC_SOURCE_ID_E {
	VBC_MUX_ADC0_SOURCE,
	VBC_MUX_ADC1_SOURCE,
	VBC_MUX_ADC2_SOURCE,
	VBC_MUX_ADC3_SOURCE,
	VBC_MUX_ADC_SOURCE_MAX,
};

enum ADC_SOURCE_VAL {
	ADC_SOURCE_IIS,
	ADC_SOURCE_VBCIF,
	ADC_SOURCE_VAL_MAX,
};

struct vbc_mux_adc_source {
	enum VBC_MUX_ADC_SOURCE_ID_E id;
	enum ADC_SOURCE_VAL val;
};

/* SND_KCTL_TYPE_MUX_DAC_OUT */
enum VBC_MUX_DAC_OUT_ID_E {
	VBC_MUX_DAC0_OUT_SEL,
	VBC_MUX_DAC1_OUT_SEL,
	VBC_MUX_DAC_OUT_MAX,
};

enum VBC_DAC_OUT_MUX_VAL {
	DAC_OUT_FROM_IIS,
	DAC_OUT_FROM_VBCIF,
	DAC_OUT_FORM_MAX,
};

struct vbc_mux_dac_out {
	enum VBC_MUX_DAC_OUT_ID_E id;
	enum VBC_DAC_OUT_MUX_VAL val;
};

/* SND_KCTL_TYPE_MUX_ADC */
enum VBC_MUX_ADCIN_ID_E {
	VBC_MUX_IN_ADC0,
	VBC_MUX_IN_ADC1,
	VBC_MUX_IN_ADC2,
	VBC_MUX_IN_ADC3,
	VBC_MUX_IN_ADC_ID_MAX,
};

enum ADC_IN_VAL {
	ADC_IN_IIS0_ADC,
	ADC_IN_IIS1_ADC,
	ADC_IN_IIS2_ADC,
	ADC_IN_IIS3_ADC,
	ADC_IN_DAC0,
	ADC_IN_DAC1,
	ADC_IN_DAC_LOOP,
	ADC_IN_TDM,
	ADC_IN_MAX,
};

struct vbc_mux_adc_in {
	enum VBC_MUX_ADCIN_ID_E id;
	enum ADC_IN_VAL val;
};

/* SND_KCTL_TYPE_MUX_FM */
enum VBC_MUX_FM_ID_E {
	VBC_FM_MUX,
	VBC_FM_MUX_ID_MAX
};

enum FM_INSEL_VAL {
	FM_IN_FM_SRC_OUT,
	FM_IN_VBC_IF_ADC0,
	FM_IN_VBC_IF_ADC1,
	FM_IN_VBC_IF_ADC2,
	FM_IN_VAL_MAX,
};

struct vbc_mux_fm {
	enum VBC_MUX_FM_ID_E id;
	enum FM_INSEL_VAL val;
};

/* SND_KCTL_TYPE_MUX_ST */
enum VBC_MUX_ST_ID_E {
	VBC_ST_MUX,
	VBC_ST_MUX_ID_MAX
};

enum ST_INSEL_VAL {
	ST_IN_ADC0,
	ST_IN_ADC0_DG,
	ST_IN_ADC1,
	ST_IN_ADC1_DG,
	ST_IN_ADC2,
	ST_IN_ADC2_DG,
	ST_IN_ADC3,
	ST_IN_ADC3_DG,
	ST_IN_VAL_MAX,
};

struct vbc_mux_st {
	enum VBC_MUX_ST_ID_E id;
	enum ST_INSEL_VAL val;
};

/* SND_KCTL_TYPE_MUX_LOOP_DA0 */
enum VBC_MUX_LOOP_DAC0_ID {
	VBC_MUX_LOOP_DAC0,
	VBC_MUX_LOOP_DAC0_MAX,
};

enum LOOP_DAC0_IN_VAL {
	DAC0_SMTHDG_OUT,
	DAC0_MIX1_OUT,
	DAC0_EQ4_OUT,
	DAC0_MBDRC_OUT,
	DAC0_LOOP_OUT_MAX
};

struct vbc_mux_loop_dac0 {
	enum VBC_MUX_LOOP_DAC0_ID id;
	enum LOOP_DAC0_IN_VAL val;
};

/* SND_KCTL_TYPE_MUX_LOOP_DA1 */
enum VBC_MUX_LOOP_DAC1_ID {
	VBC_MUX_LOOP_DAC1,
	VBC_MUX_LOOP_DAC1_MAX,
};

enum LOOP_DAC1_IN_VAL {
	DAC1_MIXER_OUT,
	DAC1_MIXERDG_OUT,
	DA1_LOOP_OUT_MAX,
};

struct vbc_mux_loop_dac1 {
	enum VBC_MUX_LOOP_DAC1_ID id;
	enum LOOP_DAC1_IN_VAL val;
};

/* SND_KCTL_TYPE_MUX_LOOP_DA0_DA1 */
enum VBC_MUX_LOOP_DAC0_DAC1_ID {
	VBC_MUX_LOOP_DAC0_DAC1,
	VBC_MUX_LOOP_DAC0_DAC1_MAX,
};

enum LOOP_DAC0_DAC1_SEL_VAL {
	DAC0_DAC1_SEL_DAC1,
	DAC0_DAC1_SEL_DAC0,
	DAC0_DAC1_SEL_MAX
};

struct vbc_mux_loop_dac0_dac1 {
	enum VBC_MUX_LOOP_DAC0_DAC1_ID id;
	enum LOOP_DAC0_DAC1_SEL_VAL val;
};

/* SND_KCTL_TYPE_MUX_AUDRCD */
enum VBC_MUX_AUDRCD_IN_ID {
	VBC_MUX_AUDRCD01,
	VBC_MUX_AUDRCD23,
	VBC_MUX_AUDRCD_ID_MAX
};

enum VBC_AUDRCD_IN_VAL {
	AUDRCD_IN_ADC0,
	AUDRCD_IN_ADC1,
	AUDRCD_IN_ADC2,
	AUDRCD_IN_ADC3,
	AUDRCD_ADC_IN_MAX,
};

struct vbc_mux_audrcd_in {
	enum VBC_MUX_AUDRCD_IN_ID id;
	enum VBC_AUDRCD_IN_VAL val;
};

/* SND_KCTL_TYPE_MUX_TDM_AUDRCD23 */
enum VBC_MUX_TDM_AUDRCD23_ID {
	VBC_MUX_TDM_AUDRCD23,
	VBC_MUX_TDM_AUDRCD23_MAX,
};

enum VBC_MUX_TDM_AUDRCD23_VAL {
	AUDRCD23_TDM_SEL_AUDRCD23,
	AUDRCD23_TDM_SEL_TDM,
	AUDRCD23_TMD_SEL_MAX,
};

struct vbc_mux_tdm_audrcd23 {
	enum VBC_MUX_TDM_AUDRCD23_ID id;
	enum VBC_MUX_TDM_AUDRCD23_VAL val;
};

/* SND_KCTL_TYPE_MUX_AP01_DSP */
enum VBC_MUX_AP01_DSP_ID_E {
	VBC_MUX_AP01_DSP_PLY,
	VBC_MUX_AP01_DSP_RCD,
	VBC_MUX_AP01_DSP_ID_MAX,
};

enum VBC_MUX_AP01_DSP_VAL {
	AP01_TO_DSP_DISABLE,
	AP01_TO_DSP_ENABLE,
	AP01_TO_DSP_MAX,
};

struct vbc_mux_ap01_dsp {
	enum VBC_MUX_AP01_DSP_ID_E id;
	enum VBC_MUX_AP01_DSP_VAL val;
};

/*
 * SND_KCTL_TYPE_MUX_IIS_TX
 * SND_KCTL_TYPE_MUX_IIS_RX
 */
enum VBC_MUX_IIS_TX_ID_E {
	VBC_MUX_IIS_TX_DAC0,
	VBC_MUX_IIS_TX_DAC1,
	VBC_MUX_IIS_TX_DAC2,
	VBC_MUX_IIS_TX_ID_MAX,
};

enum VBC_MUX_IIS_RX_ID_E {
	VBC_MUX_IIS_RX_ADC0,
	VBC_MUX_IIS_RX_ADC1,
	VBC_MUX_IIS_RX_ADC2,
	VBC_MUX_IIS_RX_ADC3,
	VBC_MUX_IIS_RX_ID_MAX,
};

/* SND_KCTL_TYPE_IIS_PORT_DO */
enum VBC_MUX_IIS_PORT_ID_E {
	VBC_IIS_PORT_IIS0,
	VBC_IIS_PORT_IIS1,
	VBC_IIS_PORT_IIS2,
	VBC_IIS_PORT_IIS3,
	VBC_IIS_PORT_MST_IIS0,
	VBC_IIS_PORT_ID_MAX,
};

enum VBC_IIS_PORT_DO_VAL {
	IIS_DO_VAL_DAC0,
	IIS_DO_VAL_DAC1,
	IIS_DO_VAL_DAC2,
	IIS_DO_VAL_MAX,
};

struct vbc_mux_iis_tx {
	enum VBC_MUX_IIS_TX_ID_E id;
	enum VBC_MUX_IIS_PORT_ID_E val;
};

struct vbc_mux_iis_rx {
	enum VBC_MUX_IIS_RX_ID_E id;
	enum VBC_MUX_IIS_PORT_ID_E val;
};

struct vbc_mux_iis_port_do {
	enum VBC_MUX_IIS_PORT_ID_E id;
	enum VBC_IIS_PORT_DO_VAL val;
};

/* SND_KCTL_TYPE_ADDER */
enum VBC_ADDER_ID_E {
	VBC_ADDER_OFLD,
	VBC_ADDER_FM_DAC0,
	VBC_ADDER_ST_DAC0,
	VBC_ADDER_MAX
};

enum VBC_ADDER_MODE_E {
	ADDER_MOD_IGNORE = 0,
	ADDER_MOD_ADD,
	ADDER_MOD_MINUS,
	ADDER_MOD_MAX,
};

struct vbc_adder_para {
	enum VBC_ADDER_ID_E adder_id;
	enum VBC_ADDER_MODE_E adder_mode_l;
	enum VBC_ADDER_MODE_E adder_mode_r;
};

/* SND_KCTL_TYPE_VOLUME no struct define */

/* SND_KCTL_TYPE_LOOPBACK_TYPE */
enum VBC_LOOPBACK_TYPE_E {
	VBC_LOOPBACK_ADDA,
	/*echo cancellation, noise cancellation etc*/
	VBC_LOOPBACK_AD_ULDL_DA_PROCESS,
	VBC_LOOPBACK_AD_UL_ENCODE_DECODE_DL_DA_PROCESS,
	VBC_LOOPBACK_TYPE_MAX,
};

struct vbc_loopback_para {
	enum VBC_LOOPBACK_TYPE_E loopback_type;
	int voice_fmt;
	int amr_rate;
	int loop_mode;
};

/* SND_KCTL_TYPE_DATAPATH */
enum VBC_DATAPATH_ID_E {
	VBC_DAC0_DP_EN,
	VBC_DAC1_DP_EN,
	VBC_DAC2_DP_EN,
	VBC_ADC0_DP_EN,
	VBC_ADC1_DP_EN,
	VBC_ADC2_DP_EN,
	VBC_ADC3_DP_EN,
	VBC_OFLD_DP_EN,
	VBC_FM_DP_EN,
	VBC_ST_DP_EN,
	VBC_DP_EN_MAX,
};

struct vbc_dp_en_para {
	int id;
	u16 enable;
};

/* SND_KCTL_TYPE_CALL_MUTE */
enum CALL_MUTE_ID {
	VBC_UL_MUTE,
	VBC_DL_MUTE,
	VBC_MUTE_MAX,
};

struct call_mute_para {
	enum CALL_MUTE_ID id;
	u32 mute;
};

/*
 * SND_KCTL_TYPE_IIS_TX_WIDTH_SEL
 * SND_KCTL_TYPE_IIS_TX_LRMOD_SEL
 * SND_KCTL_TYPE_IIS_RX_WIDTH_SEL
 * SND_KCTL_TYPE_IIS_RX_LRMOD_SEL
 */
enum IIS_TX_RX_WD_VAL {
	WD_16BIT,
	WD_24BIT,
	IIS_WD_MAX,
};

enum IIS_TX_RX_LR_MOD_VAL {
	LEFT_HIGH,
	RIGHT_HIGH,
	LR_MOD_MAX,
};

struct vbc_iis_tx_wd_para {
	enum VBC_MUX_IIS_TX_ID_E id;
	enum IIS_TX_RX_WD_VAL value;
};

struct vbc_iis_tx_lr_mod_para {
	enum VBC_MUX_IIS_TX_ID_E id;
	enum IIS_TX_RX_LR_MOD_VAL value;
};

struct vbc_iis_rx_wd_para {
	enum VBC_MUX_IIS_RX_ID_E id;
	enum IIS_TX_RX_WD_VAL value;
};

struct vbc_iis_rx_lr_mod_para {
	enum VBC_MUX_IIS_RX_ID_E id;
	enum IIS_TX_RX_LR_MOD_VAL value;
};

/* SND_KCTL_TYPE_VBC_IIS_MASTER_START */
struct vbc_iis_master_para {
	u32 vbc_startup_reload;
	u32 enable;
};

/* SND_KCTL_TYPE_SBCPARA_SET */
struct sbcenc_param_t {
	int32_t SBCENC_Mode;
	int32_t SBCENC_Blocks;
	int32_t SBCENC_SubBands;
	int32_t SBCENC_SamplingFreq;
	int32_t SBCENC_AllocMethod;
	int32_t SBCENC_min_Bitpool;
	int32_t SBCENC_max_Bitpool;
};

/* SND_KCTL_TYPE_MAIN_MIC_PATH_FROM */
enum VBC_DSP_USED_MAINMIC_TYPE {
	MAINMIC_USED_DSP_NORMAL_ADC,
	MAINMIC_USED_DSP_REF_ADC,
	MAINMIC_USED_MAINMIC_TYPE_MAX,
};

enum VBC_MAINMIC_PATH_VAL {
	MAINMIC_FROM_LEFT,
	MAINMIC_FROM_RIGHT,
	MAINMIC_FROM_MAX,
};

struct mainmic_from_para {
	enum VBC_DSP_USED_MAINMIC_TYPE type;
	enum VBC_MAINMIC_PATH_VAL main_mic_from;
};

/*
 * SND_KCTL_TYPE_EXT_INNER_IIS_MST_SEL
 * selection for vbc iis master from which iis controler.
 */
enum VBC_IIS_MST_SEL_ID {
	IIS_MST_SEL_0,
	IIS_MST_SEL_1,
	IIS_MST_SEL_2,
	IIS_MST_SEL_3,
	IIS_MST_SEL_ID_MAX,
};

enum VBC_MASTER_TYPE_VAL {
	VBC_MASTER_EXTERNAL,
	VBC_MASTER_INTERNAL,
	VBC_MASTER_TYPE_MAX,
};

struct vbc_iis_mst_sel_para {
	enum VBC_IIS_MST_SEL_ID id;
	enum VBC_MASTER_TYPE_VAL mst_type;
};

/**********************************************************************
 * define for SND_VBC_DSP_IO_SHAREMEM_GET / SND_VBC_DSP_IO_SHAREMEM_SET
 **********************************************************************/

/* --------------------defined in audio_sipc.h---------------------------- */

enum{
	VBC_EQ4_DAC0,
	VBC_EQ4_MAX
};

enum{
	VBC_EQ6_DAC0,
	VBC_EQ6_MAX
};

/******************************************************************
 * define for SND_VBC_DSP_FUNC_STARTUP / SND_VBC_DSP_FUNC_SHUTDOWN
 ******************************************************************/
enum {
	VBC_DAI_ID_NORMAL_AP01 = 0,
	VBC_DAI_ID_NORMAL_AP23,
	VBC_DAI_ID_CAPTURE_DSP,
	VBC_DAI_ID_FAST_P,
	VBC_DAI_ID_OFFLOAD,
	VBC_DAI_ID_VOICE,
	VBC_DAI_ID_VOIP,
	VBC_DAI_ID_FM,
	VBC_DAI_ID_LOOP,
	VBC_DAI_ID_PCM_A2DP,
	VBC_DAI_ID_OFFLOAD_A2DP,
	VBC_DAI_ID_BT_CAPTURE_AP,
	VBC_DAI_ID_FM_CAPTURE_AP,
	VBC_DAI_ID_VOICE_CAPTURE,
	VBC_DAI_ID_FM_CAPTURE_DSP,
	VBC_DAI_ID_BT_SCO_CAPTURE_DSP,
	VBC_DAI_ID_FM_DSP,
	VBC_DAI_ID_MAX
};

enum VBC_DA_ID_E {
	VBC_DA0,
	VBC_DA1,
	VBC_DA2,
	VBC_DA_MAX,
};

enum VBC_AD_ID_E {
	VBC_AD0,
	VBC_AD1,
	VBC_AD2,
	VBC_AD3,
	VBC_TDM,
	VBC_AD_MAX,
};

struct snd_pcm_stream_info {
	char name[32];
	int id;
	int stream;
};

struct ivsense_smartpa_t {
	int32_t enable;
	int32_t iv_adc_id;
};

#define STARTUP_RESERVED_CNT 8
struct snd_pcm_startup_paras {
	enum VBC_DA_ID_E dac_id;
	enum VBC_AD_ID_E adc_id;
	enum VBC_AD_ID_E ref_adc_id;
	struct vbc_mux_adc_source adc_source[VBC_MUX_ADC_SOURCE_MAX];
	struct vbc_mux_dac_out dac_out[VBC_MUX_DAC_OUT_MAX];
	struct vbc_mux_iis_tx mux_tx[VBC_MUX_IIS_TX_ID_MAX];
	struct vbc_mux_iis_rx mux_rx[VBC_MUX_IIS_RX_ID_MAX];
	struct vbc_mux_iis_port_do iis_do[VBC_IIS_PORT_ID_MAX];
	struct vbc_iis_tx_wd_para tx_wd[VBC_MUX_IIS_TX_ID_MAX];
	struct vbc_iis_tx_lr_mod_para tx_lr_mod[VBC_MUX_IIS_TX_ID_MAX];
	struct vbc_iis_rx_wd_para rx_wd[VBC_MUX_IIS_RX_ID_MAX];
	struct vbc_iis_rx_lr_mod_para rx_lr_mod[VBC_MUX_IIS_RX_ID_MAX];
	struct vbc_loopback_para loopback_para;
	struct vbc_iis_master_para iis_master_para;
	struct vbc_mute_dg_para mdg_para[VBC_MDG_MAX];
	struct vbc_smthdg_module smthdg_modle[VBC_SMTHDG_MAX];
	struct vbc_mixerdg_para mixerdg_para[VBC_MIXERDG_MAX];
	u32 mixerdg_step;
	struct vbc_mixer_para mixer_para[VBC_MIXER_MAX];
	struct sbcenc_param_t sbcenc_para;
	struct ivsense_smartpa_t ivs_smtpa;
	struct vbc_iis_mst_sel_para mst_sel_para[IIS_MST_SEL_ID_MAX];
};

struct sprd_vbc_stream_startup_shutdown {
	struct snd_pcm_stream_info stream_info;
	struct snd_pcm_startup_paras startup_para;
};

/**********************************************************
 * define for SND_VBC_DSP_FUNC_HW_PARAMS
 *********************************************************/
struct snd_pcm_hw_paras {
	u32 channels;
	u32 rate;
	u32 format;
};

struct sprd_vbc_stream_hw_paras {
	struct snd_pcm_stream_info stream_info;
	struct snd_pcm_hw_paras hw_params_info;
};

/**********************************************************
 * define for SND_VBC_DSP_FUNC_HW_TRIGGER
 *********************************************************/
struct sprd_vbc_stream_trigger {
	struct snd_pcm_stream_info stream_info;
	int pcm_trigger_cmd;
};

enum SYS_IIS_E {
	SYS_IIS0,
	SYS_IIS1,
	SYS_IIS2,
	SYS_IIS3,
	SYS_IIS4,
	SYS_IIS5,
	SYS_IIS6,
	SYS_IIS_MAX,
};

/* AUDIO PM */
struct aud_pm_vbc {
	bool is_startup;
	/*false:resume, true: suspend */
	bool suspend_resume;
	/* prot is_startup */
	struct mutex pm_mtx_cmd_prot;
	/* prot is_pm_shutdown */
	spinlock_t pm_spin_cmd_prot;
	/* lock for normal suspend resume reference */
	struct mutex lock_mtx_suspend_resume;
	int ref_suspend_resume;
	/* prot scene_case_flag */
	struct mutex lock_scene_flag;
	int scene_flag[VBC_DAI_ID_MAX][STREAM_CNT];
};

/* FIRMWARE */
#define VBC_PROFILE_FIRMWARE_MAGIC_LEN 16
#define VBC_PROFILE_FIRMWARE_MAGIC_ID ("audio_profile")
#define VBC_PROFILE_CNT_MAX 0x0fffffff
#define AUD_FIRMWARE_PATHNAME_LEN_MAX 256

/* header of the data */
struct vbc_fw_header {
	char magic[VBC_PROFILE_FIRMWARE_MAGIC_LEN];
	u32 num_mode;
	u32 len_mode;
};

struct vbc_profile {
	struct device *dev;
	struct snd_soc_codec *codec;
	int now_mode[SND_VBC_PROFILE_MAX];
	int is_loading[SND_VBC_PROFILE_MAX];
	struct vbc_fw_header hdr[SND_VBC_PROFILE_MAX];
	void *data[SND_VBC_PROFILE_MAX];
};

enum vbc_dump_position_e {
	DUMP_POS_DAC0_E,
	DUMP_POS_DAC1_E,
	DUMP_POS_A4,
	DUMP_POS_A3,
	DUMP_POS_A2,
	DUMP_POS_A1,
	DUMP_POS_V2,
	DUMP_POS_V1,
	DUMP_POS_MAX,
};

#define SBC_PARA_BYTES 64
struct vbc_codec_priv {
	struct snd_soc_codec *codec;
	struct vbc_profile vbc_profile_setting;
	struct mutex load_mutex;
	struct vbc_mute_dg_para mdg[VBC_MDG_MAX];
	int32_t src_fs[VBC_SRC_MAX];
	struct vbc_dg_para dg[VBC_DG_MAX];
	struct vbc_smthdg_para smthdg[VBC_SMTHDG_MAX];
	struct vbc_smthdg_step_para smthdg_step[VBC_SMTHDG_MAX];
	struct vbc_mixerdg_para mixerdg[VBC_MIXERDG_MAX];
	u32 mixerdg_step;
	struct vbc_mux_dac_out mux_dac_out[VBC_MUX_DAC_OUT_MAX];
	struct vbc_mux_adc_source mux_adc_source[VBC_MUX_ADC_SOURCE_MAX];
	struct vbc_mux_adc_in mux_adc_in[VBC_MUX_IN_ADC_ID_MAX];
	struct vbc_mux_fm mux_fm[VBC_FM_MUX_ID_MAX];
	struct vbc_mux_st mux_st[VBC_ST_MUX_ID_MAX];
	struct vbc_mux_loop_dac0 mux_loop_dac0[VBC_MUX_LOOP_DAC0_MAX];
	struct vbc_mux_loop_dac1 mux_loop_dac1[VBC_MUX_LOOP_DAC1_MAX];
	struct vbc_mux_loop_dac0_dac1 mux_loop_dac0_dac1[
		VBC_MUX_LOOP_DAC0_DAC1_MAX];
	struct vbc_mux_audrcd_in mux_audrcd_in[VBC_MUX_AUDRCD_ID_MAX];
	struct vbc_mux_tdm_audrcd23 mux_tdm_audrcd23[VBC_MUX_TDM_AUDRCD23_MAX];
	struct vbc_mux_ap01_dsp mux_ap01_dsp[VBC_MUX_AP01_DSP_ID_MAX];
	struct vbc_mux_iis_tx mux_iis_tx[VBC_MUX_IIS_TX_ID_MAX];
	struct vbc_mux_iis_rx mux_iis_rx[VBC_MUX_IIS_RX_ID_MAX];
	struct vbc_mux_iis_port_do mux_iis_port_do[VBC_IIS_PORT_ID_MAX];
	struct vbc_iis_tx_wd_para iis_tx_wd[VBC_MUX_IIS_TX_ID_MAX];
	struct vbc_iis_tx_lr_mod_para iis_tx_lr_mod[VBC_MUX_IIS_TX_ID_MAX];
	struct vbc_iis_rx_wd_para iis_rx_wd[VBC_MUX_IIS_RX_ID_MAX];
	struct vbc_iis_rx_lr_mod_para iis_rx_lr_mod[VBC_MUX_IIS_RX_ID_MAX];
	int volume;
	struct vbc_adder_para vbc_adder[VBC_ADDER_MAX];
	struct vbc_loopback_para loopback;
	struct vbc_dp_en_para vbc_dp_en[VBC_DP_EN_MAX];
	struct call_mute_para vbc_call_mute[VBC_MUTE_MAX];
	u16 sys_iis_sel[SYS_IIS_MAX];
	u16 ag_iis_ext_sel[AG_IIS_MAX];
	u16 vbc_iis_inf_sys_sel;
	struct mutex agcp_access_mutex;
	int agcp_access_aud_cnt;
	int agcp_access_a2dp_cnt;
	int agcp_access_enable;
	u32 dsp_reg;
	struct pinctrl *pctrl;
	struct vbc_iis_master_para iis_master;
	u32 aud_iis0_master_setting;
	struct vbc_mixer_para mixer[VBC_MIXER_MAX];
	/* For whale2, need internal codec(audio top) to provide vbc da clock
	 * when 2 da paths are used at the same time.
	 */
	int need_aud_top_clk;
	enum vbc_dump_position_e vbc_dump_position;
	struct sbcenc_param_t sbcenc_para;
	char firmware_path[AUD_FIRMWARE_PATHNAME_LEN_MAX];
	struct mainmic_from_para mainmic_from[MAINMIC_USED_MAINMIC_TYPE_MAX];
	struct vbc_iis_mst_sel_para mst_sel_para[IIS_MST_SEL_ID_MAX];
	/* to do */
	atomic_t aux_iis_master_start;
	int32_t ivs_smtpa_ctl_enable;
	int32_t is_use_ivs_smtpa;
	u32 iis_mst_width;
};

/********************************************************************
 * ap phy define interface
 *******************************************************************/
u32 vbc_phy_ap2dsp(u32 addr);
u32 get_vbc_dsp_ap_offset(void);
void set_vbc_dsp_ap_offset(u32 offset);
u32 get_ap_vbc_phy_base(void);
void set_ap_vbc_phy_base(u32 phy_addr);
void *get_ap_vbc_virt_base(void);
void set_ap_vbc_virt_base(void *virt_addr);
int ap_vbc_reg_update(u32 reg, u32 val, u32 mask);
u32 ap_vbc_reg_read(u32 reg);
int ap_vbc_reg_write(u32 reg, u32 val);
const char *ap_vbc_fifo_id2name(int id);
const char *ap_vbc_watermark_type2name(int type);
int ap_vbc_set_watermark(int fifo_id, int watermark_type, u32 watermark);
const char *ap_vbc_datafmt2name(int fmt);
void ap_vbc_data_format_set(int fifo_id, enum VBC_DAT_FORMAT dat_fmt);
const char *ap_vbc_chan_id2name(int chan_id);
int ap_vbc_fifo_enable(int fifo_id, int chan, int enable);
void ap_vbc_fifo_clear(int fifo_id);
void ap_vbc_aud_dma_chn_en(int fifo_id, int vbc_chan, int enable);
void vbc_phy_audply_set_src_mode(int en, int mode);

/********************************************************************
 * dsp phy define interface
 *******************************************************************/
int dsp_vbc_reg_write(u32 reg, int val, u32 mask);
u32 dsp_vbc_reg_read(u32 reg);
int dsp_vbc_mdg_set(int id, int enable, int mdg_step);
int dsp_vbc_src_set(int id, int32_t fs);
int dsp_vbc_dg_set(int id, int dg_l, int dg_r);
int dsp_vbc_smthdg_set(int id, int smthdg_l, int smthdg_r);
int dsp_vbc_smthdg_step_set(int id, int smthdg_step);
int dsp_vbc_mixerdg_mainpath_set(int id, int mixerdg_main_l,
	int mixerdg_main_r);
int dsp_vbc_mixerdg_mixpath_set(int id,
	int mixerdg_mix_l, int mixerdg_mix_r);
int dsp_vbc_mixerdg_step_set(int mixerdg_step);
int dsp_vbc_mixer_set(int id, int ops_type);
int dsp_vbc_mux_adc_source_set(int id, int val);
int dsp_vbc_mux_dac_out_set(int id, int val);
int dsp_vbc_mux_adc_set(int id, int val);
int dsp_vbc_mux_fm_set(int id, int val);
int dsp_vbc_mux_st_set(int id, int val);
int dsp_vbc_mux_loop_da0_set(int id, int val);
int dsp_vbc_mux_loop_da1_set(int id, int val);
int dsp_vbc_mux_loop_da0_da1_set(int id, int val);
int dsp_vbc_mux_audrcd_set(int id, int val);
int dsp_vbc_mux_tdm_audrcd23_set(int id, int val);
int dsp_vbc_mux_ap01_dsp_set(int id, int val);
int dsp_vbc_mux_iis_tx_set(int id, int val);
int dsp_vbc_mux_iis_rx_set(int id, int val);
int dsp_vbc_mux_iis_port_do_set(int id, int val);
int dsp_vbc_set_volume(int volume);
int dsp_vbc_adder_set(int id, int adder_mode_l, int adder_mode_r);
int dsp_vbc_loopback_set(struct vbc_loopback_para *loopback);
int dsp_vbc_dp_en_set(int id, u16 enable);
int dsp_call_mute_set(int id, u16 mute);
int dsp_vbc_iis_tx_width_set(int id, u32 width);
int dsp_vbc_iis_tx_lr_mod_set(int id, u32 lr_mod);
int dsp_vbc_iis_rx_width_set(int id, u32 width);
int dsp_vbc_iis_rx_lr_mod_set(int id, u32 lr_mod);
int dsp_vbc_mst_sel_type_set(int id, u32 type);
int dsp_vbc_iis_master_start(u32 enable);
void dsp_vbc_iis_master_width_set(u32 iis_width);
int dsp_vbc_mainmic_path_set(int type, int val);
int dsp_ivsence_func(int enable, int iv_adc_id);

int vbc_dsp_func_startup(int scene_id, int stream,
	struct sprd_vbc_stream_startup_shutdown *startup_info);
int vbc_dsp_func_shutdown(int scene_id, int stream,
	struct sprd_vbc_stream_startup_shutdown *shutdown_info);
int vbc_dsp_func_hwparam(int scene_id, int stream,
	struct sprd_vbc_stream_hw_paras *hw_data);
int vbc_dsp_func_trigger(int id, int stream, int up_down);
int aud_dig_iis_master(struct snd_soc_card *card, int setting);
int pm_shutdown(void);
#endif /* __VBC_V4_PHY_DRV_H */
