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

#ifndef _SPRD_CPROC_H
#define _SPRD_CPROC_H
#define INVALID_REG (0xff)
enum {
	CPROC_CTRL_SHUT_DOWN = 0,
	CPROC_CTRL_DEEP_SLEEP = 1,
	CPROC_CTRL_CORE_RESET = 2,
	CPROC_CTRL_SYS_RESET = 3,
	CPROC_CTRL_LOAD_STATUS = 4,
	CPROC_CTRL_DSPCORE_RESET = 5,
	CPROC_CTRL_EXT0,
	CPROC_CTRL_EXT1,
	CPROC_CTRL_EXT2,
	CPROC_CTRL_EXT3,
	CPROC_CTRL_EXT4,
	CPROC_CTRL_EXT5,
	CPROC_CTRL_EXT6,
	CPROC_CTRL_NR,
};

#define CPROC_IRAM_DATA_NR 3

enum {
	CPROC_REGION_CP_MEM = 0,
	CPROC_REGION_IRAM_MEM = 1,
	CPROC_REGION_CTRL_REG = 1,
	CPROC_REGION_NR,
};

enum {
	CPROC_REGTYPE_AON_APB = 0,
	CPROC_REGTYPE_PMU_APB = 1,
	CPROC_REGTYPE_NR,
};

struct cproc_segments {
	char			*name;
	u32		base;		/* segment addr */
	u32		maxsz;		/* segment size */
};

#define MAX_CPROC_NODE_NAME_LEN	0x20
struct load_node {
	char name[MAX_CPROC_NODE_NAME_LEN];
	u32 base;
	u32 size;
};

#define MAX_IRAM_DATA_NUM	0x40
struct cproc_ctrl {
	phys_addr_t iram_addr;
	u32 iram_loaded;
	u32 iram_size;
	u32 iram_data[MAX_IRAM_DATA_NUM];
	u32 ctrl_reg[CPROC_CTRL_NR]; /* offset */
	u32 ctrl_mask[CPROC_CTRL_NR];
	struct regmap *rmap[CPROC_CTRL_NR];
	u32 reg_nr;
};

struct cproc_init_data {
	char			*devname;
	phys_addr_t		base;		/* CP base addr */
	u32		maxsz;		/* CP max size */
	int			(*start)(void *arg);
	int			(*stop)(void *arg);
	int			(*suspend)(void *arg);
	int			(*resume)(void *arg);

	struct cproc_ctrl	*ctrl;
	void		*mini_mem;
	phys_addr_t	 mini_mem_paddr;
	u32	mini_mem_size;
	int			wdtirq;
	int			type;
	u32		segnr;
	struct cproc_segments	segs[];
};

#endif
