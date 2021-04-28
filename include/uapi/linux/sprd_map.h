/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2019 Unisoc Communications, Inc.
 * Author: Sheng Xu <sheng.xu@unisoc.com>
 */

#ifndef SPRD_MAP_H
#define SPRD_MAP_H

#define SPRD_MAP_IOCTRL_MAGIC        'o'

/**
 * DOC: MAP_USER_VIR
 *
 * Takes a pmem_info struct
 */
#define MAP_USER_VIR  _IOWR(SPRD_MAP_IOCTRL_MAGIC, 0, struct sprd_pmem_info)

struct sprd_pmem_info {
	unsigned long phy_addr;
	unsigned int phys_offset;
	size_t size;
};

#endif
