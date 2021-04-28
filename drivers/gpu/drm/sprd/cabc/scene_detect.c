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

#include"scene_detect.h"
#include"cabc_global.h"

void scene_detection(int *hist, int *hist_pre, int *hist_diff,
	int *hist_pre_diff,  int num, u8 *scene_flag,
	int scene_detect_thr)
{
	int i;
	int hist_diff_diff, sum_hist_diff = 0;

	for (i = 0; i < HIST_BIN_CABC; i++) {
		hist_diff_diff = abs(hist_diff[i] - hist_pre_diff[i]);
		sum_hist_diff += hist_diff_diff;
	}
	if ((sum_hist_diff * 100) >= (num * g_scene_change_thr))
		*scene_flag = 0;
	else
		*scene_flag = 1;
}
