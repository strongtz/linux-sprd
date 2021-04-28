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

#ifndef _BL_DECISION_H_
#define _BL_DECISION_H_

#include"cabc_definition.h"

void backlight_decision(int *hist_cabc, struct bl_out_tag *bl);
void backlight_fix_ui(struct bl_out_tag *bl,
	u8 *s_step2, u8 step0, u8 step1, u8 step2, u8 scene_flag,
	int hist_num, int change_num_diff, u8 *change_cnt);
void backlight_fix_video(u8 s_scene_change,
	u8 step, struct bl_out_tag *bl);

#endif
