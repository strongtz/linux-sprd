/*
 * linux/sound/sprd-asoc-card-utils-legacy.h
 *
 * SPRD ASoC legacy utils -- Customer implement.
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
#ifndef __SPRD_ASOC_CARD_UTILS_LEGACY_H
#define __SPRD_ASOC_CARD_UTILS_LEGACY_H

#include <sound/soc.h>
#include <sound/control.h>

enum {
	/* hook function implemented, and execute normally */
	HOOK_OK = 0x1,
	/* hook function implemented, but not hook this parameter */
	HOOK_BPY = 0x2,
	/* not implement the hook functions */
	HOOK_NONE = HOOK_BPY,
};

enum {
	BOARD_FUNC_SPK = 0,
	BOARD_FUNC_SPK1,
	BOARD_FUNC_EAR,
	BOARD_FUNC_HP,
	BOARD_FUNC_MUTE_MAX,
	BOARD_FUNC_LINE = BOARD_FUNC_MUTE_MAX,
	BOARD_FUNC_MIC,
	BOARD_FUNC_AUXMIC,
	BOARD_FUNC_HP_MIC,
	BOARD_FUNC_DMIC,
	BOARD_FUNC_DMIC1,
	BOARD_FUNC_DFM,
	BOARD_FUNC_MAX
};

enum {
	EXT_CTRL_SPK,
	EXT_CTRL_HP,
	EXT_CTRL_EAR,
	EXT_CTRL_MIC,
	EXT_CTRL_DFM,
	EXT_CTRL_MAX
};

typedef int (*sprd_asoc_hook_func)(int id, int on);

struct sprd_asoc_ext_hook {
	sprd_asoc_hook_func ext_ctrl[EXT_CTRL_MAX];
};

struct sprd_array_size {
	const void *ptr;
	size_t size;
};

enum {
	SMARTAMP_BOOST_GPIO_ENABLE,
	SMARTAMP_BOOST_GPIO_MADE,
	SMARTAMP_BOOS_GPIO_MAX
};

struct smartamp_boost_data {
	unsigned int gpios[SMARTAMP_BOOS_GPIO_MAX];
	unsigned int boost_mode;
};

int sprd_asoc_ext_hook_register(struct sprd_asoc_ext_hook *hook);
int sprd_asoc_ext_hook_unregister(struct sprd_asoc_ext_hook *hook);
int sprd_asoc_board_comm_probe(void);
int sprd_asoc_board_comm_late_probe(struct snd_soc_card *card);
void sprd_asoc_shutdown(struct platform_device *pdev);

int sprd_asoc_card_parse_ext_hook(struct device *dev,
				  struct sprd_asoc_ext_hook *ext_hook);

int sprd_asoc_card_parse_smartamp_boost(struct device *dev,
				  struct smartamp_boost_data *boost_data);

extern struct sprd_array_size sprd_asoc_card_widgets;
extern struct sprd_array_size sprd_asoc_card_controls;

#endif /* __SPRD_ASOC_CARD_UTILS_LEGACY_H */
