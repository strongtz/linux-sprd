/*
 * sound/soc/sprd/sprd-asoc-common.h
 *
 * SPRD ASoC Common include -- SpreadTrum ASOC Common.
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
#ifndef __SPRD_ASOC_COMMON_H
#define __SPRD_ASOC_COMMON_H

#define STR_ON_OFF(on) (on ? "On" : "Off")

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#include <linux/delay.h>
#include <sound/soc.h>
#include <sound/simple_card.h>

#define SP_AUDIO_DEBUG_BASIC	BIT(0)
#define SP_AUDIO_DEBUG_MORE	BIT(1)
#define SP_AUDIO_DEBUG_REG	BIT(2)
#define SP_AUDIO_DEBUG_DEFAULT SP_AUDIO_DEBUG_BASIC

#define SP_AUDIO_CODEC_NUM	6
#define AUDIO_CODEC_2713	0
#define AUDIO_CODEC_2723	1
#define AUDIO_CODEC_2723E	2
#define AUDIO_CODEC_2723T	2
#define AUDIO_CODEC_2723M	2
#define AUDIO_CODEC_2731	4
#define AUDIO_CODEC_RT5659	5
#define AUDIO_CODEC_2721	6
#define AUDIO_CODEC_2730	6

#define AUDIO_2723_VER_AA	0
#define AUDIO_2723_VER_S	0x000
#define AUDIO_2723_VER_E	0x090
#define AUDIO_2723_VER_T	0x0c0
#define AUDIO_2723_VER_M	0x0c1

#define CODEC_HW_INFO \
	"2713", "2723", "2723E", "2723T", "2731S", \
	"Realtek", "2721", "2730"

struct snd_card;
int sprd_audio_debug_init(struct snd_card *card);
int get_sp_audio_debug_flag(void);
#define sp_audio_debug(mask) (get_sp_audio_debug_flag() & (mask))

#define sp_asoc_pr_info(fmt, ...) do {			\
	if (sp_audio_debug(SP_AUDIO_DEBUG_BASIC)) {	\
		pr_info(fmt, ##__VA_ARGS__);		\
	}						\
} while (0)

#define sp_asoc_pr_reg(fmt, ...) do {			\
	if (sp_audio_debug(SP_AUDIO_DEBUG_REG)) {	\
		pr_info(fmt, ##__VA_ARGS__);		\
	}						\
} while (0)

#if defined(CONFIG_DYNAMIC_DEBUG)
#define SP_AUDIO_DEBUG_DBG_FLAG SP_AUDIO_DEBUG_BASIC
#else
#define DEBUG
#define SP_AUDIO_DEBUG_DBG_FLAG SP_AUDIO_DEBUG_MORE
#endif

#define sp_asoc_dev_dbg(dev, format, ...) do {			\
	if (sp_audio_debug(SP_AUDIO_DEBUG_DBG_FLAG)) {		\
		dev_dbg(dev, pr_fmt(format), ##__VA_ARGS__);	\
	}							\
} while (0)

#define sp_asoc_pr_dbg(fmt, ...) do {				\
	if (sp_audio_debug(SP_AUDIO_DEBUG_DBG_FLAG)) {		\
		pr_debug(fmt, ##__VA_ARGS__);			\
	}							\
} while (0)

static inline void sprd_msleep(unsigned long ms)
{
	if ((ms) < 20)
		usleep_range((ms) * 1000, (ms) * 1000 + 100);
	else
		msleep(ms);
}

#endif /* __SPRD_ASOC_COMMON_H */
