/*
 * sound/soc/sprd/dai/i2s/i2s.h
 *
 * SPRD SoC CPU-DAI -- SpreadTrum SOC DAI i2s.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
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
#ifndef __SPRD_DAI_I2S_H
#define __SPRD_DAI_I2S_H

#include <linux/io.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/soc.h>

#include "sprd-i2s.h"
#include "sprd-audio.h"

#define I2S_VERSION	"i2s.r0p0"

enum {
	AUDIO_NO_CHANGE,
	AUDIO_TO_CP0_DSP_CTRL,
	AUDIO_TO_CP1_DSP_CTRL,
	AUDIO_TO_AP_ARM_CTRL,
	AUDIO_TO_ARM_CTRL = AUDIO_TO_AP_ARM_CTRL,
	AUDIO_TO_CP0_ARM_CTRL,
	AUDIO_TO_CP1_ARM_CTRL,
	AUDIO_TO_CP2_ARM_CTRL,
};

/* bus_type */
#define I2S_BUS 0
#define PCM_BUS 1

/* byte_per_chan */
#define I2S_BPCH_8 0
#define I2S_BPCH_16 1
#define I2S_BPCH_32 2

/* mode */
#define I2S_MASTER 0
#define I2S_SLAVE 1

/* lsb */
#define I2S_MSB 0
#define I2S_LSB 1

/* rtx_mode */
#define I2S_RTX_DIS 0
#define I2S_RX_MODE 1
#define I2S_TX_MODE 2
#define I2S_RTX_MODE 3

/* sync_mode */
#define I2S_LRCK 0
#define I2S_SYNC 1

/* lrck_inv */
#define I2S_L_LEFT 0
#define I2S_L_RIGTH 1

/* clk_inv */
#define I2S_CLK_N 0
#define I2S_CLK_R 1

/* i2s_bus_mode */
#define I2S_MSBJUSTFIED 0
#define I2S_COMPATIBLE 1

/* pcm_bus_mode */
#define I2S_LONG_FRAME 0
#define I2S_SHORT_FRAME 1

/**********i2s-debug*********/
#define SAMPLATE_MIN 8000
#define SAMPLATE_MAX 96000

#endif /* __SPRD_DAI_I2S_H */
