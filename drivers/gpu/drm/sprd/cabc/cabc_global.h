/*
 *Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#ifndef _CABC_GLOBAL_H_
#define _CABC_GLOBAL_H_

#include "cabc_definition.h"

extern u16 g_brightness_step[32];
extern u16 g_backlight_compensation_table[256];
extern int g_cabc_percent_th;
extern u16 g_min_backlight;
extern u16 g_max_backlight;
extern u8 g_step0;
extern u8 g_step1;
extern u8 g_step2;
extern u16 g_scene_change_thr;

#endif
