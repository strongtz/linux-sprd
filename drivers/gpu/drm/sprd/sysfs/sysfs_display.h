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

#ifndef _SYSFS_DISPLAY_H_
#define _SYSFS_DISPLAY_H_

#include <linux/device.h>

extern struct class *display_class;

int sprd_dpu_sysfs_init(struct device *dev);
int sprd_dsi_sysfs_init(struct device *dev);
int sprd_dphy_sysfs_init(struct device *dev);
int sprd_panel_sysfs_init(struct device *dev);

#endif
