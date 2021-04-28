/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef _GSP_LITE_R3P0_CFG_H
#define _GSP_LITE_R3P0_CFG_H
/**---------------------------------------------------------------------------*
 **                             Indepence                                     *
 **---------------------------------------------------------------------------*
 */
#include <linux/types.h>
#include <uapi/drm/gsp_cfg.h>
#include <uapi/drm/gsp_lite_r3p0_cfg.h>
#include <drm/gsp_cfg.h>

struct gsp_lite_r3p0_img_layer {
	struct gsp_layer		common;
	struct gsp_lite_r3p0_img_layer_params	params;
};

struct gsp_lite_r3p0_osd_layer {
	struct gsp_layer		common;
	struct gsp_lite_r3p0_osd_layer_params	params;
};

struct gsp_lite_r3p0_des_layer {
	struct gsp_layer		common;
	struct gsp_lite_r3p0_des_layer_params	params;
};

struct gsp_lite_r3p0_misc_cfg {
	uint8_t gsp_gap;
	uint8_t core_num;
	uint8_t co_work0;
	uint8_t co_work1;
	uint8_t work_mod;
	uint8_t pmargb_en;
	struct gsp_rect workarea_src_rect;
	struct gsp_pos workarea_des_pos;
};

struct gsp_lite_r3p0_cfg {
	struct gsp_cfg common;
	struct gsp_lite_r3p0_img_layer limg[LITE_R3P0_IMGL_NUM];
	struct gsp_lite_r3p0_osd_layer losd[LITE_R3P0_OSDL_NUM];
	struct gsp_lite_r3p0_des_layer ld1;
	struct gsp_lite_r3p0_misc_cfg misc;
};

#endif
