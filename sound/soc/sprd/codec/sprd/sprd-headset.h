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

#ifndef __HEADSET_SPRD_H__
#define __HEADSET_SPRD_H__

#if defined(CONFIG_SND_SOC_SPRD_CODEC_SC2730)
#include "./sc2730/sprd-headset-2730.h"
#elif defined(CONFIG_SND_SOC_SPRD_CODEC_SC2721)
#include "./sc2721/sprd-headset-2721.h"
#endif

#endif
