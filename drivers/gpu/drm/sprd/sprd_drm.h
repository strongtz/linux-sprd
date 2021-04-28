/*
 *Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#ifndef _SPRD_DRM_H_
#define _SPRD_DRM_H_

#include <drm/drmP.h>
#include <drm/drm_atomic.h>

struct sprd_drm {
	struct drm_atomic_state *state;
	struct device *dpu_dev;
	struct device *gsp_dev;
};

int sprd_atomic_wait_for_fences(struct drm_device *dev,
				      struct drm_atomic_state *state,
				      bool pre_swap);

int sprd_atomic_commit(struct drm_device *dev,
			     struct drm_atomic_state *state,
			     bool nonblock);

#endif /* _SPRD_DRM_H_ */
