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


#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include "gsp_interface_sharkle.h"
#include "../gsp_debug.h"
#include "../gsp_interface.h"


int gsp_interface_sharkle_parse_dt(struct gsp_interface *intf,
				  struct device_node *node)
{
	return 0;
}

int gsp_interface_sharkle_init(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_deinit(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_prepare(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_unprepare(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_reset(struct gsp_interface *intf)
{
	return 0;
}

void gsp_interface_sharkle_dump(struct gsp_interface *intf)
{

}
