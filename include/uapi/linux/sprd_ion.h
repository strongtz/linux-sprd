/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef _UAPI_SPRD_ION_H
#define _UAPI_SPRD_ION_H

enum sprd_ion_heap_ids {
	ION_HEAP_ID_SYSTEM = 0,
	ION_HEAP_ID_MM,
	ION_HEAP_ID_OVERLAY,
	ION_HEAP_ID_FB,
	ION_HEAP_ID_CAM,
	ION_HEAP_ID_VDSP,
};

#define ION_HEAP_ID_MASK_SYSTEM        (1<<ION_HEAP_ID_SYSTEM)
#define ION_HEAP_ID_MASK_MM            (1<<ION_HEAP_ID_MM)
#define ION_HEAP_ID_MASK_OVERLAY       (1<<ION_HEAP_ID_OVERLAY)
#define ION_HEAP_ID_MASK_FB            (1<<ION_HEAP_ID_FB)
#define ION_HEAP_ID_MASK_CAM           (1<<ION_HEAP_ID_CAM)
#define ION_HEAP_ID_MASK_VDSP          (1<<ION_HEAP_ID_VDSP)

#define ION_FLAG_SECURE  (1<<31)
#define ION_FLAG_NO_CLEAR (1 << 16)


enum SPRD_DEVICE_SYNC_TYPE {
	SPRD_DEVICE_PRIMARY_SYNC,
	SPRD_DEVICE_VIRTUAL_SYNC,
};

struct ion_phys_data {
	int fd_buffer;
	unsigned long phys;
	size_t size;
};

struct ion_msync_data {
	int fd_buffer;
	unsigned long vaddr;
	unsigned long paddr;
	size_t size;
};

struct ion_fence_data {
	uint32_t device_type;
	int life_value;
	int release_fence_fd;
	int retired_fence_fd;
};

struct ion_kmap_data {
	int fd_buffer;
	uint64_t kaddr;
	size_t size;
};

struct ion_kunmap_data {
	int fd_buffer;
};

#endif /* _UAPI_SPRD_ION_H */
