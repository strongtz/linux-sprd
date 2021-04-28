/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifndef _GSP_INTERFACE_SHARKL5_H
#define _GSP_INTERFACE_SHARKL5_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_SHARKL5 "sharkl5"

#define SHARKL5_AP_AHB_DISP_EB_NAME	  ("clk_ap_ahb_disp_eb")

struct gsp_interface_sharkl5 {
	struct gsp_interface common;

	struct clk *clk_ap_ahb_disp_eb;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};

int gsp_interface_sharkl5_parse_dt(struct gsp_interface *intf,
				  struct device_node *node);

int gsp_interface_sharkl5_init(struct gsp_interface *intf);
int gsp_interface_sharkl5_deinit(struct gsp_interface *intf);

int gsp_interface_sharkl5_prepare(struct gsp_interface *intf);
int gsp_interface_sharkl5_unprepare(struct gsp_interface *intf);

int gsp_interface_sharkl5_reset(struct gsp_interface *intf);

void gsp_interface_sharkl5_dump(struct gsp_interface *inf);

#endif
