/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
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

#ifndef __SPRD_ROUND_CORNER_H_
#define __SPRD_ROUND_CORNER_H_

#include <linux/slab.h>
#include <linux/kernel.h>
#include "sprd_dpu.h"

#define STEP (256)
#define CORNER_ERR     -1
#define CORNER_DONE    0

extern struct sprd_dpu_layer corner_layer_top;
extern struct sprd_dpu_layer corner_layer_bottom;

void sprd_corner_destroy(void);
int sprd_corner_hwlayer_init(int panel_height, int panel_width, int corner_radius);

#endif
