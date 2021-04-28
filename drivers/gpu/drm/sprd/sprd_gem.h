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

#ifndef _SPRD_GEM_H_
#define _SPRD_GEM_H_

#include <drm/drm_gem.h>

struct sprd_gem_obj {
	struct drm_gem_object base;
	dma_addr_t dma_addr;
	struct sg_table *sgtb;
	void *vaddr;
	bool fb_reserved;
	bool need_iommu;
};

#define to_sprd_gem_obj(x)	container_of(x, struct sprd_gem_obj, base)

void sprd_gem_free_object(struct drm_gem_object *gem);
int sprd_gem_cma_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			    struct drm_mode_create_dumb *args);
int sprd_gem_cma_mmap(struct file *filp, struct vm_area_struct *vma);
int sprd_gem_cma_prime_mmap(struct drm_gem_object *obj,
			 struct vm_area_struct *vma);
struct sg_table *sprd_gem_cma_prime_get_sg_table(struct drm_gem_object *obj);
struct drm_gem_object *sprd_gem_prime_import_sg_table(struct drm_device *dev,
		struct dma_buf_attachment *attach, struct sg_table *sgtb);

#endif
