/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#ifndef _SPRD_CP_H_
#define _SPRD_CP_H_

#include <linux/ioctl.h>

#define SPRD_CP_IOC_MAGIC 'c'

/* Query MODEM image load status. */
#define SPRD_CP_IOCGLDSTAT _IOR(SPRD_CP_IOC_MAGIC, 0, int)
/* Clear MODEM image load status. */
#define SPRD_CP_IOCCLDSTAT _IO(SPRD_CP_IOC_MAGIC, 1)

#endif  /*!_SPRD_CP_H_ */
