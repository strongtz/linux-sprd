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

#ifndef _SPRD_DRM_GSP_H_
#define _SPRD_DRM_GSP_H_

#include <drm/drm.h>
#include "gsp_cfg.h"

#define DRM_SPRD_GSP_GET_CAPABILITY	0
#define DRM_SPRD_GSP_TRIGGER	1

struct drm_gsp_cfg_user {
	bool async;
	__u32 size;
	__u32 num;
	bool split;
	void *config;
};

struct drm_gsp_capability {
	__u32 size;
	void *cap;
};

#define DRM_IOCTL_SPRD_GSP_GET_CAPABILITY \
	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_SPRD_GSP_GET_CAPABILITY, \
		struct drm_gsp_capability)

#define DRM_IOCTL_SPRD_GSP_TRIGGER	\
	DRM_IOWR(DRM_COMMAND_BASE + DRM_SPRD_GSP_TRIGGER, \
		struct drm_gsp_cfg_user)

#endif	/* SPRD_DRM_GSP_H_ */
