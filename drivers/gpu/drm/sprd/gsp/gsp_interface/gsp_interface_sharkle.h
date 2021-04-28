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

#ifndef _GSP_INTERFACE_SHARKLE_H
#define _GSP_INTERFACE_SHARKLE_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_SHARKLE "sharkle"

struct gsp_interface_sharkle {
	struct gsp_interface common;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};



int gsp_interface_sharkle_parse_dt(struct gsp_interface *intf,
				  struct device_node *node);

int gsp_interface_sharkle_init(struct gsp_interface *intf);
int gsp_interface_sharkle_deinit(struct gsp_interface *intf);

int gsp_interface_sharkle_prepare(struct gsp_interface *intf);
int gsp_interface_sharkle_unprepare(struct gsp_interface *intf);

int gsp_interface_sharkle_reset(struct gsp_interface *intf);

void gsp_interface_sharkle_dump(struct gsp_interface *inf);

#endif
