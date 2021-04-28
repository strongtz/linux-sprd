/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#ifndef __SPRD_THM_COMM_H__
#define __SPRD_THM_COMM_H__

#include <linux/thermal.h>
#include <linux/types.h>

#define THM_NAME_LENGTH	20
#define THM_SENSOR_NUMBER	8

struct sprd_thermal_zone {
	struct thermal_zone_device *therm_dev;
	struct mutex th_lock;
	struct device *dev;
	struct delayed_work resume_delay_work;
	struct thm_handle_ops *ops;
	char name[THM_NAME_LENGTH];
	int id;
};

struct thm_handle_ops {
	void (*hw_init)(struct sprd_thermal_zone *);
	int (*read_temp)(struct sprd_thermal_zone *, int *);
	int (*suspend)(struct sprd_thermal_zone *);
	int (*resume)(struct sprd_thermal_zone *);
};

int sprd_thermal_init(struct sprd_thermal_zone *pzone);
void sprd_thermal_remove(struct sprd_thermal_zone *pzone);

#endif
