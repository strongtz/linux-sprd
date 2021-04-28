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

#include <linux/err.h>
#include <linux/thermal.h>
#include "sprd_thm_comm.h"

static int sprd_sys_temp_read(void *devdata, int *temp)
{
	struct sprd_thermal_zone *pzone = devdata;
	int ret = 0;

	if (!pzone->ops->read_temp)
		return -1;
	ret = pzone->ops->read_temp(pzone, temp);
	return ret;
}

void sprd_thermal_remove(struct sprd_thermal_zone *pzone)
{
	thermal_zone_device_unregister(pzone->therm_dev);
	cancel_delayed_work_sync(&pzone->resume_delay_work);
	mutex_destroy(&pzone->th_lock);
}

static void sprd_thermal_resume_delay_work(struct work_struct *work)
{
	struct sprd_thermal_zone *pzone;

	pzone =
	    container_of(work, struct sprd_thermal_zone,
			 resume_delay_work.work);
	dev_dbg(&pzone->therm_dev->device,
		"thermal resume delay work Started.\n");
	pzone->ops->resume(pzone);
	dev_dbg(&pzone->therm_dev->device,
		"thermal resume delay work finished.\n");
}

static const struct thermal_zone_of_device_ops sprd_of_thermal_ops = {
	.get_temp = sprd_sys_temp_read,
};

int sprd_thermal_init(struct sprd_thermal_zone *pzone)
{
	INIT_DELAYED_WORK(&pzone->resume_delay_work,
			  sprd_thermal_resume_delay_work);
	pzone->therm_dev =
	    thermal_zone_of_sensor_register(pzone->dev, pzone->id, pzone,
					    &sprd_of_thermal_ops);
	if (IS_ERR_OR_NULL(pzone->therm_dev)) {
		pr_err("Register thermal zone device failed.\n");
		return PTR_ERR(pzone->therm_dev);
	}
	thermal_zone_device_update(pzone->therm_dev, THERMAL_EVENT_UNSPECIFIED);
	return 0;
}
