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

#include "sprd_dpu.h"
#include "cabc_definition.h"
#include "bl_decision.h"
#include "scene_detect.h"
#include "gain_compensation.h"

int s_hist_cabc[2][HIST_BIN_CABC];
int s_hist_cabc_diff[2][HIST_BIN_CABC];
int s_scene_detect_thr = 30;
struct cabc_context_tag s_cabc_ctx;
bool s_pre_is_VSP_working;
int g_cabc_percent_th = 10;
u8 s_scene_change;
int s_ui_menu_frame_flag;
u16 g_min_backlight = 408;
u16 g_max_backlight = 1020;
u8 s_smooth_final_flag;
struct bl_out_tag g_backlight;
u8 g_step0 = 2;
u8 g_step1 = 2;
u8 s_bl_cur_fix_video;
u8 s_change_cnt = 1;
u8 g_step2 = STEP2_INIT;
u16 g_scene_change_thr = 80;
static int s_vsp_working_cnt;
u8 s_step2 = 2;
int s_hist_cabc_num_diff;
u16 s_gain;

void init_cabc(int img_width, int img_height)
{
	s_cabc_ctx.width = img_width;
	s_cabc_ctx.height = img_height;
	s_cabc_ctx.pixel_total_num = s_cabc_ctx.height * s_cabc_ctx.width;
	g_backlight.cur_fix_ui = g_max_backlight;
	g_backlight.pre_fix_video = g_max_backlight;
	g_backlight.cur_fix_video = g_max_backlight;
	g_backlight.pre_fix_ui = g_max_backlight;
	g_backlight.cur = g_max_backlight;
	g_backlight.pre = g_max_backlight;
	g_backlight.pre2 = g_max_backlight;
	g_backlight.pre3 = g_max_backlight;
}

void clip(int *input, u16 bottom, u16 top)
{
	if (*input > top)
		*input = top;
	if (*input < bottom)
		*input = bottom;
}

void step_set(int step0, int step1, int step2,
	int scene_change_thr, int min_backlight)
{
	g_step0 = (u8)step0;
	g_step1 = (u8)step1;
	g_step2 = (u8)step2;
	g_min_backlight = (u16)min_backlight;
}
int cabc_trigger(struct cabc_para *para, int frame_no)
{
	u8 step0, step1, step2;
	int i, j, k;
	int *hist_pre = s_hist_cabc[PRE];
	int *hist_pre_pre = s_hist_cabc[PRE_PRE];
	int *hist_diff_pre = s_hist_cabc_diff[PRE];
	int *hist_diff_pre_pre = s_hist_cabc_diff[PRE_PRE];
	struct bl_out_tag *bl_ptr = &g_backlight;

	hist_pre[0] = para->cabc_hist[31];
	for (j = 30; j >= 0; j--)
		hist_pre[30 - (j - 1)] = hist_pre[30 - j] + para->cabc_hist[j];
	for (i = 0; i < HIST_BIN_CABC; i++)
		hist_diff_pre[i] = para->cabc_hist[HIST_BIN_CABC-1-i];

	s_hist_cabc_num_diff = hist_pre[13] - hist_pre_pre[13];
	if (para->is_VSP_working) {
		s_vsp_working_cnt++;
		if (s_vsp_working_cnt < 20)
			para->is_VSP_working = 0;
	} else {
		s_vsp_working_cnt = 0;
	}
	if (para->cur_bl <= 50)
		k = 0;
	else
		k = 1;
	step0 = k * g_step0;
	step1 = k * g_step1;
	step2 = k * g_step2;

	if (frame_no > 1) {
		scene_detection(hist_pre, hist_pre_pre, hist_diff_pre,
			hist_diff_pre_pre,
			s_cabc_ctx.pixel_total_num,
			&s_scene_change, s_scene_detect_thr);
	}

	backlight_decision(hist_pre, bl_ptr);
	if (para->is_VSP_working == 0) {
		if (s_pre_is_VSP_working == 1)
			bl_ptr->pre_fix_ui = bl_ptr->pre_fix_video;
		if (frame_no == 1) {
			bl_ptr->cur_fix_ui = g_max_backlight;
			para->bl_fix = para->cur_bl;
		} else {
			backlight_fix_ui(bl_ptr, &s_step2, step0,
				step1, step2, s_scene_change,
			s_cabc_ctx.pixel_total_num, s_hist_cabc_num_diff,
				&s_change_cnt);
		}
		clip(&bl_ptr->cur_fix_ui, g_min_backlight, g_max_backlight);
		para->bl_fix = (u16)bl_ptr->cur_fix_ui;
		compensation_gain(bl_ptr->cur_fix_ui / 4, &s_gain);
		bl_ptr->pre_fix_ui = bl_ptr->cur_fix_ui;
	} else {
		if (s_pre_is_VSP_working == 0)
			bl_ptr->pre_fix_video = bl_ptr->pre_fix_ui;
		if (frame_no == 1) {
			bl_ptr->cur_fix_video = bl_ptr->cur;
			para->bl_fix = para->cur_bl;
		} else {
			backlight_fix_video(s_scene_change,
				step0, bl_ptr);
			clip(&bl_ptr->cur_fix_video, g_min_backlight,
				g_max_backlight);
		}
		para->bl_fix = (u16)bl_ptr->cur_fix_video;
		compensation_gain(bl_ptr->cur_fix_video / 4, &s_gain);
		bl_ptr->pre_fix_video = bl_ptr->cur_fix_video;
	}
	para->gain = s_gain;
	bl_ptr->pre3 = bl_ptr->pre2;
	bl_ptr->pre2 = bl_ptr->pre;
	bl_ptr->pre = bl_ptr->cur;
	s_pre_is_VSP_working = para->is_VSP_working;
	memcpy(hist_pre_pre, hist_pre, HIST_BIN_CABC*sizeof(int));
	memcpy(hist_diff_pre_pre, hist_diff_pre, HIST_BIN_CABC*sizeof(int));
	return 0;
}
