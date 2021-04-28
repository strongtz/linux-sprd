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

#ifndef _MCDT_CTL_REG_V0_H
#define _MCDT_CTL_REG_V0_H
#include <linux/bitops.h>

#define MCDT_CTL_ADDR     0x41490000

#define MCDT_CH0_TXD      0X0000
#define MCDT_CH1_TXD      0X0004
#define MCDT_CH2_TXD      0X0008
#define MCDT_CH3_TXD      0X000c
#define MCDT_CH4_TXD      0X0010
#define MCDT_CH5_TXD      0X0014
#define MCDT_CH6_TXD      0X0018
#define MCDT_CH7_TXD      0X001c
#define MCDT_CH8_TXD      0X0020
#define MCDT_CH9_TXD      0X0024

#define MCDT_CH0_RXD     0X0028
#define MCDT_CH1_RXD     0X002c
#define MCDT_CH2_RXD     0X0030
#define MCDT_CH3_RXD     0X0034
#define MCDT_CH4_RXD     0X0038
#define MCDT_CH5_RXD     0X003c
#define MCDT_CH6_RXD     0X0040
#define MCDT_CH7_RXD     0X0044
#define MCDT_CH8_RXD     0X0048
#define MCDT_CH9_RXD     0x004c

#define MCDT_DAC0_WTMK   0x0060
#define MCDT_DAC1_WTMK   0x0064
#define MCDT_DAC2_WTMK   0x0068
#define MCDT_DAC3_WTMK   0x006c
#define MCDT_DAC4_WTMK   0x0070
#define MCDT_DAC5_WTMK   0x0074
#define MCDT_DAC6_WTMK   0x0078
#define MCDT_DAC7_WTMK   0x007c
#define MCDT_DAC8_WTMK   0x0080
#define MCDT_DAC9_WTMK   0x0084

#define MCDT_ADC0_WTMK   0x0088
#define MCDT_ADC1_WTMK   0x008c
#define MCDT_ADC2_WTMK   0x0090
#define MCDT_ADC3_WTMK   0x0094
#define MCDT_ADC4_WTMK   0x0098
#define MCDT_ADC5_WTMK   0x009c
#define MCDT_ADC6_WTMK   0x00a0
#define MCDT_ADC7_WTMK   0x00a4
#define MCDT_ADC8_WTMK   0x00a8
#define MCDT_ADC9_WTMK   0x00ac

#define MCDT_DMA_EN      0x00b0

#define MCDT_INT_EN0     0x00b4
#define MCDT_INT_EN1     0x00b8
#define MCDT_INT_EN2     0x00bc

#define MCDT_INT_CLR0     0x00c0
#define MCDT_INT_CLR1     0x00c4
#define MCDT_INT_CLR2     0x00c8

#define MCDT_INT_RAW1     0x00cc
#define MCDT_INT_RAW2     0x00d0
#define MCDT_INT_RAW3     0x00d4

#define MCDT_INT_MSK1     0x00d8
#define MCDT_INT_MSK2     0x00dc
#define MCDT_INT_MSK3     0x00e0

#define MCDT_DAC0_FIFO_ADDR_ST    0x00e4
#define MCDT_ADC0_FIFO_ADDR_ST    0x00e8
#define MCDT_DAC1_FIFO_ADDR_ST    0x00ec
#define MCDT_ADC1_FIFO_ADDR_ST    0x00f0
#define MCDT_DAC2_FIFO_ADDR_ST    0x00f4
#define MCDT_ADC2_FIFO_ADDR_ST    0x00f8
#define MCDT_DAC3_FIFO_ADDR_ST    0x00fc
#define MCDT_ADC3_FIFO_ADDR_ST    0x0100
#define MCDT_DAC4_FIFO_ADDR_ST    0x0104
#define MCDT_ADC4_FIFO_ADDR_ST    0x0108
#define MCDT_DAC5_FIFO_ADDR_ST    0x010c
#define MCDT_ADC5_FIFO_ADDR_ST    0x0110
#define MCDT_DAC6_FIFO_ADDR_ST    0x0114
#define MCDT_ADC6_FIFO_ADDR_ST    0x0118
#define MCDT_DAC7_FIFO_ADDR_ST    0x011c
#define MCDT_ADC7_FIFO_ADDR_ST    0x0120
#define MCDT_DAC8_FIFO_ADDR_ST    0x0124
#define MCDT_ADC8_FIFO_ADDR_ST    0x0128
#define MCDT_DAC9_FIFO_ADDR_ST    0x012c
#define MCDT_ADC9_FIFO_ADDR_ST    0x0130

#define MCDT_CH_FIFO_ST0    0x0134
#define MCDT_CH_FIFO_ST1    0x0138
#define MCDT_CH_FIFO_ST2    0x013c

#define MCDT_INT_MSK_CFG0    0X0140
#define MCDT_INT_MSK_CFG1    0X0144

#define MCDT_DMA_CFG0    0X0148
#define MCDT_FIFO_CLR    0X014c

#define MCDT_DMA_CFG1    0X0150
#define MCDT_DMA_CFG2    0X0154
#define MCDT_DMA_CFG3    0X0158
#define MCDT_DMA_CFG4    0X015c
#define MCDT_DMA_CFG5    0X0160


#define BIT_DAC0_FIFO_AE_LVL(x)    (((x) & GENMASK(8, 0)) << 16)
#define BIT_DAC0_FIFO_AF_LVL(x)    ((x) & GENMASK(8, 0))

#define BIT_ADC0_FIFO_AE_LVL(x)    (((x) & GENMASK(8, 0)) << 16)
#define BIT_ADC0_FIFO_AF_LVL(x)    ((x) & GENMASK(8, 0))

/*DMA channel sel for AP*/
#define BIT_MCDT_DAC_DMA_CH4_SEL0(x)    (((x) & GENMASK(3, 0)) << 16)
#define BIT_GET_MCDT_DAC_DMA_CH4_SEL0(x)    (((x) >> 16) & GENMASK(3, 0))
#define BIT_MCDT_DAC_DMA_CH3_SEL0(x)    (((x) & GENMASK(3, 0)) << 12)
#define BIT_GET_MCDT_DAC_DMA_CH3_SEL0(x)    (((x) >> 12) & GENMASK(3, 0))
#define BIT_MCDT_DAC_DMA_CH2_SEL0(x)    (((x) & GENMASK(3, 0)) << 8)
#define BIT_GET_MCDT_DAC_DMA_CH2_SEL0(x)    (((x) >> 8) & GENMASK(3, 0))
#define BIT_MCDT_DAC_DMA_CH1_SEL0(x)    (((x) & GENMASK(3, 0)) << 4)
#define BIT_GET_MCDT_DAC_DMA_CH1_SEL0(x)    (((x) >> 4) & GENMASK(3, 0))
#define BIT_MCDT_DAC_DMA_CH0_SEL0(x)    ((x) & GENMASK(3, 0))
#define BIT_GET_MCDT_DAC_DMA_CH0_SEL0(x)    ((x) & GENMASK(3, 0))

/*DMA channel sel for AP*/
#define BIT_MCDT_ADC_DMA_CH4_SEL0(x)    (((x) & GENMASK(3, 0)) <<  16)
#define BIT_GET_MCDT_ADC_DMA_CH4_SEL0(x)    (((x) >> 16) & GENMASK(3, 0))
#define BIT_MCDT_ADC_DMA_CH3_SEL0(x)    (((x) & GENMASK(3, 0)) << 12)
#define BIT_GET_MCDT_ADC_DMA_CH3_SEL0(x)    (((x) >> 12) & GENMASK(3, 0))
#define BIT_MCDT_ADC_DMA_CH2_SEL0(x)    (((x) & GENMASK(3, 0)) << 8)
#define BIT_GET_MCDT_ADC_DMA_CH2_SEL0(x)    (((x) >> 8) & GENMASK(3, 0))
#define BIT_MCDT_ADC_DMA_CH1_SEL0(x)    (((x) & GENMASK(3, 0)) << 4)
#define BIT_GET_MCDT_ADC_DMA_CH1_SEL0(x)    (((x) >> 4) & GENMASK(3, 0))
#define BIT_MCDT_ADC_DMA_CH0_SEL0(x)    ((x) & GENMASK(3, 0))
#define BIT_GET_MCDT_ADC_DMA_CH0_SEL0(x)    ((x) & GENMASK(3, 0))

enum MCDT_CHAN_NUM {
	MCDT_CHAN0 = 0,
	MCDT_CHAN1,
	MCDT_CHAN2,
	MCDT_CHAN3,
	MCDT_CHAN4,
	MCDT_CHAN5,
	MCDT_CHAN6,
	MCDT_CHAN7,
	MCDT_CHAN8,
	MCDT_CHAN9
};

enum MCDT_FIFO_INT {
	MCDT_ADC_FIFO_AE_INT = 0,
	MCDT_ADC_FIFO_AF_INT,
	MCDT_DAC_FIFO_AE_INT,
	MCDT_DAC_FIFO_AF_INT,
	MCDT_ADC_FIFO_OV_INT,
	MCDT_DAC_FIFO_OV_INT
};

enum MCDT_FIFO_STS {
	MCDT_ADC_FIFO_REAL_FULL = 0,
	MCDT_ADC_FIFO_REAL_EMPTY,
	MCDT_ADC_FIFO_AF,
	MCDT_ADC_FIFO_AE,
	MCDT_DAC_FIFO_REAL_FULL,
	MCDT_DAC_FIFO_REAL_EMPTY,
	MCDT_DAC_FIFO_AF,
	MCDT_DAC_FIFO_AE
};

enum MCDT_DMA_ACK {
	MCDT_AP_ACK0 = 0,
	MCDT_AP_ACK1,
	MCDT_AP_ACK2,
	MCDT_AP_ACK3,
	MCDT_AP_ACK4,
	MCDT_PUB_CP_ACK0,
	MCDT_TGDSP_ACK0,
	MCDT_LDSP_ACK0
};

enum MCDT_AP_DMA_CHAN {
	MCDT_AP_DMA_CH0 = 0,
	MCDT_AP_DMA_CH1,
	MCDT_AP_DMA_CH2,
	MCDT_AP_DMA_CH3,
	MCDT_AP_DMA_CH4
};

enum {
	MCDT_I2S_RW_FIFO_DEF = 0,
	MCDT_I2S_RW_FIFO_I2S_16
};

/* mcdt dam request uid. note you may add one
 * when use it,depending on your dma interface
 */

#define MCDT_AP_DAC_CH0_WR_REQ 4
#define MCDT_AP_DAC_CH1_WR_REQ 5
#define MCDT_AP_DAC_CH2_WR_REQ 6
#define MCDT_AP_DAC_CH3_WR_REQ 7
#define MCDT_AP_DAC_CH4_WR_REQ 8

#define MCDT_AP_ADC_CH0_RD_REQ 9
#define MCDT_AP_ADC_CH1_RD_REQ 10
#define MCDT_AP_ADC_CH2_RD_REQ 11
#define MCDT_AP_ADC_CH3_RD_REQ 12
#define MCDT_AP_ADC_CH4_RD_REQ 13

#endif
