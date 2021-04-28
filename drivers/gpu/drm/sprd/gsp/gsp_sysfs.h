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


int gsp_core_sysfs_init(struct gsp_core *core);

void gsp_core_sysfs_destroy(struct gsp_core *core);

int gsp_dev_sysfs_init(struct gsp_dev *gsp);

void gsp_dev_sysfs_destroy(struct gsp_dev *gsp);
