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

#ifndef _SCENE_DETECT_H_
#define _SCENE_DETECT_H_

#include"cabc_definition.h"

void scene_detection(int *hist, int *hist_pre, int *hist_diff,
int *hist_pre_diff, int num, u8 *scene_flag,
int scene_detect_thr);

#endif
