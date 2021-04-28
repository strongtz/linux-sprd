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

#ifndef __AUDIO_MEMALLOC_H
#define __AUDIO_MEMALLOC_H
#include <linux/debugfs.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

/* We need to put the new item to the end of enum,
 * because we will use the serial num to get the addr from dt.
 */
enum AUDIO_MEM_TYPE_E {
	/* iram for share memary */
	IRAM_SHM_AUD_STR = 0,
	IRAM_SHM_DSP_VBC = 1,
	IRAM_SHM_NXP = 2,
	IRAM_SHM_REG_DUMP = 3,
	IRAM_SHM_IVS_SMARTPA = 4,
	/* DDR32 reserved */
	DDR32,
	/* iram for sms command parameter */
	IRAM_SMS_COMMD_PARAMS,
	/* iram for sms */
	IRAM_SMS,
	/* iram for offload */
	IRAM_OFFLOAD,
	DDR32_DSPMEMDUMP,
	IRAM_DSPLOG,
	IRAM_DSPPCM,
	/* IRAM_BASE_ADDR for sharkl2 */
	IRAM_BASE,
	IRAM_NORMAL,
	IRAM_DEEPBUF,
	IRAM_4ARM7,
	IRAM_CM4_SHMEM,
	IRAM_NORMAL_C_LINKLIST_NODE1,
	IRAM_NORMAL_C_LINKLIST_NODE2,
	IRAM_NORMAL_C_DATA,
	MEM_AUDCP_DSPBIN,
	IRAM_AUDCP_AON,
	AUDIO_MEM_TYPE_MAX,
};

struct platform_audio_mem_priv {
	u32 platform_type;
};

#ifdef CONFIG_X86
enum {
	AUDIO_PG_WC, /* write-combine */
	AUDIO_PG_UC, /* noncached */
	AUDIO_PG_WB, /* write-back */
	AUDIO_PG_MAX,
};
#endif

u32 audio_addr_ap2dsp(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
			   bool invert);
u32 audio_mem_alloc(enum AUDIO_MEM_TYPE_E mem_type, u32 *size_inout);
u32 audio_mem_alloc_dsp(enum AUDIO_MEM_TYPE_E mem_type,
			     u32 *size_inout);
void audio_mem_free(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
		    u32 size);
void *audio_mem_vmap(phys_addr_t start, size_t size, int noncached);
void audio_mem_unmap(const void *mem);
#endif
