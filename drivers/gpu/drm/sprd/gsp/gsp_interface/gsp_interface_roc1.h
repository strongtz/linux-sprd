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

#ifndef _GSP_INTERFACE_ROC1_H
#define _GSP_INTERFACE_ROC1_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_ROC1 "roc1"

#define ROC1_AP_AHB_DISP_EB_NAME	  ("clk_ap_ahb_disp_eb")

struct gsp_interface_roc1 {
	struct gsp_interface common;

	struct clk *clk_ap_ahb_disp_eb;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};

int gsp_interface_roc1_parse_dt(struct gsp_interface *intf,
				  struct device_node *node);

int gsp_interface_roc1_init(struct gsp_interface *intf);
int gsp_interface_roc1_deinit(struct gsp_interface *intf);

int gsp_interface_roc1_prepare(struct gsp_interface *intf);
int gsp_interface_roc1_unprepare(struct gsp_interface *intf);

int gsp_interface_roc1_reset(struct gsp_interface *intf);

void gsp_interface_roc1_dump(struct gsp_interface *intf);

#endif
