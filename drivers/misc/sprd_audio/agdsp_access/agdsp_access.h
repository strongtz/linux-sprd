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

#ifndef __SPRD_AGDSP_ACCESS_H__
#define __SPRD_AGDSP_ACCESS_H__
#include <linux/types.h>

int agdsp_access_enable(void);
int agdsp_access_disable(void);
int agdsp_can_access(void);
int force_on_xtl(bool on_off);
int disable_access_force(void);
int restore_access(void);

#endif
