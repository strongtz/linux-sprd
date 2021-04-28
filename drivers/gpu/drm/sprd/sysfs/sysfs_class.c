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

#include <linux/kernel.h>
#include <linux/module.h>

#include "sysfs_display.h"

struct class *display_class;

static int __init display_class_init(void)
{
	pr_info("[drm] display class register\n");

	display_class = class_create(THIS_MODULE, "display");
	if (IS_ERR(display_class)) {
		pr_err("[drm] Unable to create display class\n");
		return PTR_ERR(display_class);
	}

	return 0;
}

postcore_initcall(display_class_init);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Provide display class for hardware driver");
MODULE_LICENSE("GPL v2");
