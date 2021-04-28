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

#ifndef _GSP_INTERFACE_PIKE2_H
#define _GSP_INTERFACE_PIKE2_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_PIKE2 "pike2"

struct gsp_interface_pike2 {
	void __iomem *gsp_qos_base;
	struct gsp_interface common;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};

int gsp_interface_pike2_parse_dt(struct gsp_interface *intf,
				  struct device_node *node);

int gsp_interface_pike2_init(struct gsp_interface *intf);
int gsp_interface_pike2_deinit(struct gsp_interface *intf);

int gsp_interface_pike2_prepare(struct gsp_interface *intf);
int gsp_interface_pike2_unprepare(struct gsp_interface *intf);

int gsp_interface_pike2_reset(struct gsp_interface *intf);

void gsp_interface_pike2_dump(struct gsp_interface *inf);

#endif
