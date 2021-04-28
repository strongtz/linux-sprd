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
#ifndef _GSP_DEBUG_H
#define _GSP_DEBUG_H

#include <linux/printk.h>

#define GSP_TAG "sprd-gsp:"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) GSP_TAG " %s()-" fmt, __func__

#define GSP_DEBUG pr_debug

#define GSP_ERR pr_err

#define GSP_INFO pr_info

#define GSP_DUMP pr_info

#define GSP_WARN pr_warn

#define GSP_DEV_DEBUG(dev, fmt, ...) \
	dev_dbg(dev, "%s()-" fmt, __func__, ##__VA_ARGS__)

#define GSP_DEV_ERR(dev, fmt, ...) \
	dev_err(dev, "%s()-" fmt, __func__, ##__VA_ARGS__)

#define GSP_DEV_INFO(dev, fmt, ...) \
	dev_info(dev, "%s()-" fmt, __func__, ##__VA_ARGS__)

#endif
