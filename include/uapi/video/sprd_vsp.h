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
#ifndef _SPRD_VSP_H
#define _SPRD_VSP_H

#include <linux/ioctl.h>
#include <linux/compat.h>

#define SPRD_VSP_MAP_SIZE 0xA000
#define SPRD_VSP_CLK_LEVEL_NUM 5

#define SPRD_VSP_IOCTL_MAGIC 'm'
#define VSP_CONFIG_FREQ _IOW(SPRD_VSP_IOCTL_MAGIC, 1, unsigned int)
#define VSP_GET_FREQ    _IOR(SPRD_VSP_IOCTL_MAGIC, 2, unsigned int)
#define VSP_ENABLE      _IO(SPRD_VSP_IOCTL_MAGIC, 3)
#define VSP_DISABLE     _IO(SPRD_VSP_IOCTL_MAGIC, 4)
#define VSP_ACQUAIRE    _IO(SPRD_VSP_IOCTL_MAGIC, 5)
#define VSP_RELEASE     _IO(SPRD_VSP_IOCTL_MAGIC, 6)
#define VSP_COMPLETE    _IO(SPRD_VSP_IOCTL_MAGIC, 7)
#define VSP_RESET       _IO(SPRD_VSP_IOCTL_MAGIC, 8)
#define VSP_HW_INFO     _IO(SPRD_VSP_IOCTL_MAGIC, 9)
#define VSP_VERSION     _IO(SPRD_VSP_IOCTL_MAGIC, 10)
#define VSP_GET_IOVA    _IOWR(SPRD_VSP_IOCTL_MAGIC, 11, struct vsp_iommu_map_data)
#define VSP_FREE_IOVA   _IOW(SPRD_VSP_IOCTL_MAGIC, 12, struct vsp_iommu_map_data)
#define VSP_GET_IOMMU_STATUS _IO(SPRD_VSP_IOCTL_MAGIC, 13)
#define VSP_SET_CODEC_ID    _IOW(SPRD_VSP_IOCTL_MAGIC, 14, unsigned int)
#define VSP_GET_CODEC_COUNTER    _IOWR(SPRD_VSP_IOCTL_MAGIC, 15, unsigned int)
#define VSP_SET_SCENE                _IO(SPRD_VSP_IOCTL_MAGIC, 16)
#define VSP_GET_SCENE                _IO(SPRD_VSP_IOCTL_MAGIC, 17)
#define VSP_SYNC_GSP                _IO(SPRD_VSP_IOCTL_MAGIC, 18)

#ifdef CONFIG_COMPAT
#define COMPAT_VSP_GET_IOVA    _IOWR(SPRD_VSP_IOCTL_MAGIC, 11, struct compat_vsp_iommu_map_data)
#define COMPAT_VSP_FREE_IOVA   _IOW(SPRD_VSP_IOCTL_MAGIC, 12, struct compat_vsp_iommu_map_data)

struct compat_vsp_iommu_map_data {
	compat_int_t fd;
	compat_size_t size;
	compat_ulong_t iova_addr;
};
#endif
enum sprd_vsp_frequency_e {
	VSP_FREQENCY_LEVEL_0 = 0,
	VSP_FREQENCY_LEVEL_1 = 1,
	VSP_FREQENCY_LEVEL_2 = 2,
	VSP_FREQENCY_LEVEL_3 = 3
};

enum {
	VSP_H264_DEC = 0,
	VSP_H264_ENC,
	VSP_MPEG4_DEC,
	VSP_MPEG4_ENC,
	VSP_H265_DEC,
	VSP_H265_ENC,
	VSP_VPX_DEC,
	VSP_VPX_ENC,
	VSP_ENC,
	VSP_CODEC_INSTANCE_COUNT_MAX
};

typedef enum {
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
	SHARKLJ1 = 13,
	SHARKLE = 14,
	PIKE2 = 15,
	SHARKL3 = 16,
	SHARKL5 = 17,
	ROC1 = 18,
	SHARKL5Pro  = 19,
	MAX_VERSIONS,
} VSP_VERSION_E;

struct vsp_iommu_map_data {
	int fd;
	size_t size;
	unsigned long iova_addr;
};

/*
VSP_ACQUAIRE:aquaire the hw module mutex lock
VSP_RELEASE:release the hw module mutex lock
all other commands must be sent only when the VSP user has acquaired the lock
VSP_ENABLE:enable vsp clock
VSP_DISABLE:disable vsp clock
VSP_COMPLETE:all the preparing work is done and start the vsp, the vsp finishes
its job after this command retruns.
VSP_RESET:reset vsp hardware
VSP_CONFIG_FREQ/VSP_GET_FREQ:set/get vsp frequency,the parameter is of
type sprd_vsp_frequency_e, the larger the faster
*/

#endif
