/*
 * Copyright (C) 2012--2015 Spreadtrum Communications Inc.
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
#ifndef _SPRD_JPG_H
#define _SPRD_JPG_H

#include <linux/ioctl.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define SPRD_JPG_IOCTL_MAGIC 'm'
#define JPG_CONFIG_FREQ _IOW(SPRD_JPG_IOCTL_MAGIC, 1, unsigned int)
#define JPG_GET_FREQ    _IOR(SPRD_JPG_IOCTL_MAGIC, 2, unsigned int)
#define JPG_ENABLE      _IO(SPRD_JPG_IOCTL_MAGIC, 3)
#define JPG_DISABLE     _IO(SPRD_JPG_IOCTL_MAGIC, 4)
#define JPG_ACQUAIRE    _IO(SPRD_JPG_IOCTL_MAGIC, 5)
#define JPG_RELEASE     _IO(SPRD_JPG_IOCTL_MAGIC, 6)
#define JPG_START       _IO(SPRD_JPG_IOCTL_MAGIC, 7)
#define JPG_RESET       _IO(SPRD_JPG_IOCTL_MAGIC, 8)
#define JPG_ACQUAIRE_MBIO_VLC_DONE _IOR(SPRD_JPG_IOCTL_MAGIC, 9, unsigned int)
#define JPG_GET_IOVA    _IOWR(SPRD_JPG_IOCTL_MAGIC, 11, struct jpg_iommu_map_data)
#define JPG_FREE_IOVA   _IOW(SPRD_JPG_IOCTL_MAGIC, 12, struct jpg_iommu_map_data)
#define JPG_GET_IOMMU_STATUS _IO(SPRD_JPG_IOCTL_MAGIC, 13)
#define JPG_VERSION _IO(SPRD_JPG_IOCTL_MAGIC, 14)

#ifdef CONFIG_COMPAT
#define COMPAT_JPG_GET_IOVA    _IOWR(SPRD_JPG_IOCTL_MAGIC, 11, struct compat_jpg_iommu_map_data)
#define COMPAT_JPG_FREE_IOVA   _IOW(SPRD_JPG_IOCTL_MAGIC, 12, struct compat_jpg_iommu_map_data)

struct compat_jpg_iommu_map_data {
	compat_int_t fd;
	compat_size_t size;
	compat_ulong_t iova_addr;
};
#endif

struct jpg_iommu_map_data {
	int fd;
	size_t size;
	unsigned long iova_addr;
};

enum jpg_version_e {
	SHARK = 0,
	DOLPHIN = 1,
	TSHARK = 2,
	SHARKL = 3,
	PIKE = 4,
	PIKEL = 5,
	SHARKL64 = 6,
	SHARKLT8 = 7,
	WHALE = 8,
	WHALE2 = 9,
	IWHALE2 = 10,
	ISHARKL2 = 11,
	SHARKL2 = 12,
	SHARKLE = 13,
	PIKE2 = 14,
	SHARKL3 = 15,
	SHARKL5 = 16,
	ROC1 = 17,
	SHARKL5PRO = 18,
	MAX_VERSIONS,
};

enum sprd_jpg_frequency_e {
	JPG_FREQENCY_LEVEL_0 = 0,
	JPG_FREQENCY_LEVEL_1 = 1,
	JPG_FREQENCY_LEVEL_2 = 2,
	JPG_FREQENCY_LEVEL_3 = 3
};

#define INTS_MBIO 3
#define INTS_VLD 2
#define INTS_VLC 1
#define INTS_BSM 0

/*
*ioctl command description
the JPG module user must mmap the jpg module address space to user-space
to access the hardware.
JPG_ACQUAIRE:aquaire the jpg module lock
JPG_RELEASE:release the jpg module lock
all other commands must be sent only when the JPG user posses the lock
JPG_ENABLE:enable jpg module clock
JPG_DISABLE:disable jpg module clock
JPG_START:all the preparing work is done and start the jpg module,
	the jpg module finishes
its job after this command retruns.
JPG_RESET:reset jpg module hardware
JPG_CONFIG_FREQ/JPG_GET_FREQ:set/get jpg module frequency,the parameter is of
type sprd_jpg_frequency_e, the smaller the faster
*/

#endif
