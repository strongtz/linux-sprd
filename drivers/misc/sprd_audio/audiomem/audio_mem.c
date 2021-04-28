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

#define pr_fmt(fmt) "[Audio:MEM] "fmt

#ifdef CONFIG_X86
#include <asm/cacheflush.h>
#endif
#include <linux/vmalloc.h>

#include "audio_mem.h"

enum {
	PLATFORM_SHARKL2 = 0,
	PLATFORM_WHALE2 = 1,
	PLATFORM_SHARKL5 = 2,
	PLATFORM_ORCA = 3,
};

struct smem_pool {
	struct list_head smem_head;
	/* for pool record momory list */
	spinlock_t lock;
	u32 addr;
	u32 size;
	atomic_t used;
	struct gen_pool *gen;
};

struct smem_record {
	struct list_head smem_list;
	struct task_struct *task;
	u32 size;
	u32 addr;
};

struct smem_map {
	struct list_head map_list;
	struct task_struct *task;
	const void *mem;
	void *mem_real;
	unsigned int count;
};

struct smem_map_list {
	struct list_head map_head;
	/* for memory map list operation */
	spinlock_t lock;
	u32 inited;
};

static struct smem_pool audio_mem_pool;
static struct smem_map_list mem_mp;
static struct platform_audio_mem_priv *g_priv_data;

static int audio_smem_init(u32 addr, u32 size)
{
	struct smem_pool *spool = &audio_mem_pool;
	struct smem_map_list *smem = &mem_mp;

	spool->addr = addr;
	spool->size = PAGE_ALIGN(size);
	atomic_set(&spool->used, 0);
	spin_lock_init(&spool->lock);
	INIT_LIST_HEAD(&spool->smem_head);

	/* allocator block size is times of pages */
	spool->gen = gen_pool_create(PAGE_SHIFT, -1);
	if (!spool->gen) {
		pr_err("Failed to create smem gen pool!\n");
		return -1;
	}

	if (gen_pool_add(spool->gen, spool->addr, spool->size, -1) != 0) {
		pr_err("Failed to add smem gen pool!\n");
		gen_pool_destroy(spool->gen);
		return -1;
	}

	spin_lock_init(&smem->lock);
	INIT_LIST_HEAD(&smem->map_head);
	smem->inited = 1;

	return 0;
}

static u32 audio_smem_alloc(u32 size)
{
	struct smem_pool *spool = &audio_mem_pool;
	struct smem_record *recd;
	unsigned long flags;
	u32 addr;

	recd = kzalloc(sizeof(*recd), GFP_KERNEL);
	if (!recd) {
		addr = 0;
		goto error;
	}

	size = PAGE_ALIGN(size);
	addr = gen_pool_alloc(spool->gen, size);
	if (!addr) {
		pr_err("failed to alloc smem from gen pool\n");
		kfree(recd);
		goto error;
	}

	/* record smem alloc info */
	atomic_add(size, &spool->used);
	recd->size = size;
	recd->task = current;
	recd->addr = addr;
	spin_lock_irqsave(&spool->lock, flags);
	list_add_tail(&recd->smem_list, &spool->smem_head);
	spin_unlock_irqrestore(&spool->lock, flags);

error:
	return addr;
}

static void audio_smem_free(u32 addr, u32 size)
{
	struct smem_pool *spool = &audio_mem_pool;
	struct smem_record *recd, *next;
	unsigned long flags;

	size = PAGE_ALIGN(size);
	atomic_sub(size, &spool->used);
	gen_pool_free(spool->gen, addr, size);
	/* delete record node from list */
	spin_lock_irqsave(&spool->lock, flags);
	list_for_each_entry_safe(recd, next, &spool->smem_head, smem_list) {
		if (recd->addr == addr) {
			list_del(&recd->smem_list);
			kfree(recd);
			break;
		}
	}
	spin_unlock_irqrestore(&spool->lock, flags);
}

struct AUDIO_MEM {
	u32 addr;
	u32 size;
};

static struct AUDIO_MEM audio_mem[AUDIO_MEM_TYPE_MAX];
static u32 iram_ap_base;
static u32 iram_dsp_base;
static u32 dma_offset;

static u32 audio_addr_ap2dsp_orca(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
				  bool invert)
{
	int ret_val;

	switch (mem_type) {
	case IRAM_SMS_COMMD_PARAMS:
	case IRAM_SMS:
	case DDR32_DSPMEMDUMP:
	case MEM_AUDCP_DSPBIN:
	case DDR32:
		ret_val = invert ? (addr - dma_offset) : (addr + dma_offset);
	break;
	/* ignore iram audcp aon */
	case IRAM_AUDCP_AON:
	default:
		pr_err("%s not supported mem_type %d\n", __func__, mem_type);
		ret_val = 0;
	}

	pr_debug("%s ret_val=%#x\n", __func__, ret_val);

	return ret_val;
}

static u32 audio_addr_ap2dsp_sharkl5(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
				     bool invert)
{
	int ret_val;

	switch (mem_type) {
	case IRAM_SMS_COMMD_PARAMS:
	case IRAM_SMS:
	case IRAM_SHM_AUD_STR:
	case IRAM_SHM_DSP_VBC:
	case IRAM_SHM_NXP:
	case IRAM_SHM_REG_DUMP:
	case DDR32_DSPMEMDUMP:
	case IRAM_SHM_IVS_SMARTPA:
	case MEM_AUDCP_DSPBIN:
	case DDR32:
		ret_val = invert ? (addr - dma_offset) : (addr + dma_offset);
		break;
	case IRAM_OFFLOAD:
	case IRAM_DSPLOG:
	case IRAM_NORMAL_C_DATA:
	case IRAM_NORMAL_C_LINKLIST_NODE1:
	case IRAM_NORMAL_C_LINKLIST_NODE2:
		ret_val = invert ? (addr - iram_dsp_base + iram_ap_base) :
		(addr - iram_ap_base +
		iram_dsp_base);
		break;
	/* ignore */
	case IRAM_AUDCP_AON:
	default:
		ret_val = 0;
		pr_err("%s not supported mem_type %d\n", __func__, mem_type);
	}

	pr_debug("%s ret_val=%#x\n", __func__, ret_val);

	return ret_val;
}

u32 audio_addr_ap2dsp(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
		      bool invert)
{
	u32 ret_val;

	if (g_priv_data->platform_type == PLATFORM_SHARKL5)
		return audio_addr_ap2dsp_sharkl5(mem_type, addr, invert);

	if (g_priv_data->platform_type == PLATFORM_ORCA)
		return audio_addr_ap2dsp_orca(mem_type, addr, invert);
	if (mem_type == IRAM_SMS_COMMD_PARAMS ||
	    mem_type == IRAM_SMS ||
	    mem_type == IRAM_SHM_AUD_STR ||
	    mem_type == IRAM_SHM_DSP_VBC ||
	    mem_type == IRAM_SHM_NXP ||
	    mem_type == IRAM_SHM_REG_DUMP ||
	    mem_type == IRAM_SHM_IVS_SMARTPA ||
	    mem_type == IRAM_OFFLOAD ||
	    mem_type == IRAM_DSPLOG ||
	    mem_type == IRAM_NORMAL_C_DATA ||
	    mem_type == IRAM_NORMAL_C_LINKLIST_NODE1 ||
	    mem_type == IRAM_NORMAL_C_LINKLIST_NODE2
	    ) {
		ret_val = invert ? (addr - iram_dsp_base + iram_ap_base) :
						(addr - iram_ap_base +
						 iram_dsp_base);
	} else if (mem_type == DDR32_DSPMEMDUMP) {
		ret_val = invert ? (addr - dma_offset) : (addr + dma_offset);
	} else if (mem_type == DDR32) {
		ret_val = invert ? (addr - dma_offset) : (addr + dma_offset);
	} else if (mem_type == IRAM_AUDCP_AON) {
		pr_info("ignroe IRAM_AUDCP_AON\n");
		ret_val = 0;
	} else {
		ret_val = addr;
		pr_info("%s unknown mem_type %d,ret_val =%#x\n", __func__,
			mem_type, ret_val);
	}
	pr_debug("%s ret_val=%#x\n", __func__, ret_val);

	return ret_val;
}

/*success @@return ==address, err @@return==0*/
u32 audio_mem_alloc(enum AUDIO_MEM_TYPE_E mem_type, u32 *size_inout)
{
	u32 ret_val = 0;

	if (!size_inout) {
		pr_err("%s failed\n", __func__);
		return 0;
	}

	switch (mem_type) {
	case IRAM_SMS_COMMD_PARAMS:
	case IRAM_SMS:
	case IRAM_SHM_AUD_STR:
	case IRAM_SHM_DSP_VBC:
	case IRAM_SHM_NXP:
	case IRAM_SHM_REG_DUMP:
	case IRAM_SHM_IVS_SMARTPA:
	case IRAM_OFFLOAD:
	case DDR32_DSPMEMDUMP:
	case IRAM_DSPLOG:
	case IRAM_CM4_SHMEM:
	/* start sharkl2 */
	case IRAM_BASE:
	case IRAM_NORMAL:
	case IRAM_DEEPBUF:
	case IRAM_4ARM7:
	case IRAM_NORMAL_C_LINKLIST_NODE1:
	case IRAM_NORMAL_C_LINKLIST_NODE2:
	case IRAM_NORMAL_C_DATA:
	case MEM_AUDCP_DSPBIN:
	case IRAM_AUDCP_AON:
		ret_val = audio_mem[mem_type].addr;
		*size_inout = audio_mem[mem_type].size;
		break;
	/* end sharkl2 */
	case DDR32:
		ret_val = audio_smem_alloc(*size_inout);
		break;
	default:
		ret_val = 0;
		break;
	}
	pr_info("%s mem_type=%d addr = %#x, size=%#x\n", __func__, mem_type,
		ret_val, *size_inout);

	return ret_val;
}

void audio_mem_free(enum AUDIO_MEM_TYPE_E mem_type, u32 addr,
		    u32 size)
{
	/* to do  params check */
	switch (mem_type) {
	case IRAM_SMS_COMMD_PARAMS:
	case IRAM_SMS:
	case IRAM_SHM_AUD_STR:
	case IRAM_SHM_DSP_VBC:
	case IRAM_SHM_NXP:
	case IRAM_SHM_REG_DUMP:
	case IRAM_SHM_IVS_SMARTPA:
	case MEM_AUDCP_DSPBIN:
	case IRAM_AUDCP_AON:
	case IRAM_OFFLOAD:
	case DDR32_DSPMEMDUMP:
	case IRAM_DSPLOG:
	case IRAM_CM4_SHMEM:
	/* sharkl2 */
	case IRAM_BASE:
	case IRAM_NORMAL:
	case IRAM_DEEPBUF:
	case IRAM_4ARM7:
	case IRAM_NORMAL_C_LINKLIST_NODE1:
	case IRAM_NORMAL_C_LINKLIST_NODE2:
	case IRAM_NORMAL_C_DATA:
	/* end sharkl2 */
		/* do nothing */
		break;
	case DDR32:
		audio_smem_free(addr, size);
		break;
	default:
		/* do nothing */
		break;
	}
}

#ifdef CONFIG_X86
static int audio_mem_set_page_type(const struct page *page, int numpages,
				   int type)
{
	void *va;
	int ret = 0;

	pr_debug("page: %p, numpages: %d\n", page, numpages);
	va = page_address(page);
	if (!va) {
		pr_err("%s Get the virtual address of page %p error!\n",
		       __func__, page);
		return -ENOMEM;
	}

	if (type == AUDIO_PG_WC)
		ret = set_memory_wc((unsigned long)va, numpages);
	else if (type == AUDIO_PG_UC)
		ret = set_memory_uc((unsigned long)va, numpages);
	else if (type == AUDIO_PG_WB)
		ret = set_memory_wb((unsigned long)va, numpages);
	else
		pr_warn("%s invalid type: %d\n", __func__, type);

	return ret;
}
#endif

void *audio_mem_vmap(phys_addr_t start, size_t size, int writecombine)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;
	phys_addr_t addr;
	unsigned long flags;
	struct smem_map *map;
	struct smem_map_list *smem = &mem_mp;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return NULL;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	if (!writecombine)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = pgprot_writecombine(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		kfree(map);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
#ifdef CONFIG_X86
	audio_mem_set_page_type(pages[0], page_count, writecombine ?
				AUDIO_PG_WC : AUDIO_PG_UC);
#endif
	vaddr = vm_map_ram(pages, page_count, -1, prot);
	kfree(pages);

	map->count = page_count;
	map->mem = vaddr;
	map->mem_real = (void *)((size_t)vaddr + offset_in_page(start));
	map->task = current;

	if (smem->inited) {
		spin_lock_irqsave(&smem->lock, flags);
		list_add_tail(&map->map_list, &smem->map_head);
		spin_unlock_irqrestore(&smem->lock, flags);
	}

	return map->mem_real;
}
EXPORT_SYMBOL(audio_mem_vmap);

void audio_mem_unmap(const void *mem)
{
	struct smem_map *map, *next;
	unsigned long flags;
	struct smem_map_list *smem = &mem_mp;

	if (smem->inited) {
		spin_lock_irqsave(&smem->lock, flags);
		list_for_each_entry_safe(map, next, &smem->map_head, map_list) {
			if (map->mem_real == mem) {
				list_del(&map->map_list);
				spin_unlock_irqrestore(&smem->lock, flags);
				vm_unmap_ram(map->mem, map->count);
				kfree(map);
				return;
			}
		}
		spin_unlock_irqrestore(&smem->lock, flags);
	}
}
EXPORT_SYMBOL(audio_mem_unmap);

static int audio_mem_whale2_parse_dt(struct device_node *np)
{
	u32 val_arr[2] = { 0 };
	struct resource res;
	struct device_node *memnp;
	int ret = 0;
	u32 ddr32_size;

	if (!np) {
		pr_err("%s, np is NULL!\n", __func__);
		return -EINVAL;
	}

	/* DDR32 memory */
	memnp = of_parse_phandle(np, "memory-region", 0);
	if (!memnp) {
		pr_err("get phandle 'memory-region' failed!\n");
		return -ENODEV;
	}
	ret = of_address_to_resource(memnp, 0, &res);
	if (ret != 0) {
		pr_err("of_address_to_resource failed!(%d)\n", ret);
		return -EINVAL;
	}
	of_node_put(memnp);
	ddr32_size = (u32)(res.end - res.start) + 1;
	pr_info("%s res.start=%#lx, res.end=%#lx\n", __func__,
		(unsigned long)res.start, (unsigned long)res.end);

	/* DDR32 memory for DMA */
	ret = of_property_read_u32(np, "sprd,ddr32-dma", val_arr);
	if (ret) {
		pr_err("read 'sprd,ddr32-dma' failed!(%d)\n", ret);
		return ret;
	}
	if (val_arr[0] > ddr32_size) {
		pr_warn("the size(%#x) of 'sprd,ddr32-dma' is greater than the total size(%#x)\n",
			val_arr[0], ddr32_size);
		val_arr[0] = ddr32_size;
	}
	audio_mem[DDR32].addr = (u32)res.start;
	audio_mem[DDR32].size = val_arr[0];

	/* DDR32 memory for DSP memory dump */
	ret = of_property_read_u32(np, "sprd,ddr32-dspmemdump", val_arr + 1);
	if (ret) {
		pr_err("read 'sprd,ddr32-dspmemdump' failed!(%d)\n", ret);
		return ret;
	}
	if (val_arr[1] + val_arr[0] > ddr32_size) {
		pr_warn("the requested size(%#x = %#x + %#x) is greater than the total size(%#x)\n",
			val_arr[0] + val_arr[1], val_arr[0], val_arr[1],
			ddr32_size);
		val_arr[1] = ddr32_size - val_arr[0];
	}
	audio_mem[DDR32_DSPMEMDUMP].addr = res.start + val_arr[0];
	audio_mem[DDR32_DSPMEMDUMP].size = val_arr[1];

	/* ap address base for iram */
	if (!of_property_read_u32_array(np, "sprd,iram-ap-base",
					&val_arr[0], 1)) {
		iram_ap_base = val_arr[0];
		pr_info("%s iram_ap_base=%#x\n", __func__, iram_ap_base);
	} else {
		pr_err("%s, ERR:Must give me the ap_base!\n", __func__);
		return -EINVAL;
	}

	/* dsp address base for iram */
	if (!of_property_read_u32_array(np, "sprd,iram-dsp-base",
					&val_arr[0], 1)) {
		iram_dsp_base = val_arr[0];
		pr_info("%s iram_dsp_base=%#x\n", __func__, iram_dsp_base);
	} else {
		pr_err("%s, ERR:Must give me the sprd,dsp_base!\n", __func__);
		return -EINVAL;
	}

	/* IRAM_SMS_COMMD_PARAMS */
	if (!of_property_read_u32_array(np, "sprd,cmdaddr", &val_arr[0], 2)) {
		audio_mem[IRAM_SMS_COMMD_PARAMS].addr = val_arr[0];
		audio_mem[IRAM_SMS_COMMD_PARAMS].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the smsg cmdaddr!\n", __func__);
		return -EINVAL;
	}

	/* IRAM_SMS */
	if (!of_property_read_u32_array(np, "sprd,smsg-addr", &val_arr[0], 2)) {
		audio_mem[IRAM_SMS].addr = val_arr[0];
		audio_mem[IRAM_SMS].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the smsg addr!\n", __func__);
		return -EINVAL;
	}

	/* IRAM_SHM_AUD_STR */
	if (!of_property_read_u32_array(np, "sprd,shmaddr-aud-str",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_AUD_STR].addr = val_arr[0];
		audio_mem[IRAM_SHM_AUD_STR].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_a addr!\n", __func__);
	}

	/* IRAM_SHM_AUD_STR */
	if (!of_property_read_u32_array(np, "sprd,shmaddr-dsp-vbc",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_DSP_VBC].addr = val_arr[0];
		audio_mem[IRAM_SHM_DSP_VBC].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_b addr!\n", __func__);
		return -EINVAL;
	}

	/* IRAM_SHM_NXP */
	if (!of_property_read_u32_array(np, "sprd,shmaddr-nxp",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_NXP].addr = val_arr[0];
		audio_mem[IRAM_SHM_NXP].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_c addr!\n", __func__);
	}

	/* IRAM shared with */
	if (!of_property_read_u32_array(np, "sprd,cm4-shmem", &val_arr[0], 2)) {
		audio_mem[IRAM_CM4_SHMEM].addr = val_arr[0];
		audio_mem[IRAM_CM4_SHMEM].size = val_arr[1];
	} else {
		pr_warn("%s, warn: parse the cm4 share memory addr failed!\n",
			__func__);
	}

	/* IRAM_SHM_REG_DUMP */
	if (!of_property_read_u32_array(np, "sprd,shmaddr-reg-dump",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_REG_DUMP].addr = val_arr[0];
		audio_mem[IRAM_SHM_REG_DUMP].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_d addr!\n", __func__);
	}

	/* IRAM_OFFLOAD */
	if (!of_property_read_u32_array(np, "sprd,offload-addr",
					&val_arr[0], 2)) {
		audio_mem[IRAM_OFFLOAD].addr = val_arr[0];
		audio_mem[IRAM_OFFLOAD].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the offload addr!\n", __func__);
	}

	/* IRAM DSPLOG */
	if (!of_property_read_u32_array(np, "sprd,dsplog", &val_arr[0], 2)) {
		audio_mem[IRAM_DSPLOG].addr = val_arr[0];
		audio_mem[IRAM_DSPLOG].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the offload addr!\n", __func__);
	}
	/* IRAM NORMAL CAPTURE */
	if (!of_property_read_u32_array(np,
					"sprd,normal-captue-linklilst-node1",
					&val_arr[0], 2)) {
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE1].addr = val_arr[0];
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE1].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the iram sprd,normal-captue-linklilst-node1 addr!\n",
		       __func__);
	}
	if (!of_property_read_u32_array(np,
					"sprd,normal-captue-linklilst-node2",
					&val_arr[0], 2)) {
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE2].addr = val_arr[0];
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE2].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the iram sprd,normal-captue-linklilst-node2 addr!\n",
		       __func__);
	}
	if (!of_property_read_u32_array(np, "sprd,normal-captue-data",
					&val_arr[0], 2)) {
		audio_mem[IRAM_NORMAL_C_DATA].addr = val_arr[0];
		audio_mem[IRAM_NORMAL_C_DATA].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the iram normal capture data addr!\n",
		       __func__);
	}

	if (!of_property_read_u32_array
	    (np, "sprd,dma-offset", &val_arr[0], 1)) {
		dma_offset = val_arr[0];
	} else {
		pr_err("%s, ERR:Must give me the dma offset addr!\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int audio_mem_sharkl2_parse_dt(struct device_node *np)
{
	u32 val_arr[2] = { 0 };

	if (!np) {
		pr_err("%s, np is NULL!\n", __func__);
		return -EINVAL;
	}

	if (!of_property_read_u32_array(np, "sprd,iram_phy_addr",
					&val_arr[0], 2)) {
		audio_mem[IRAM_BASE].addr = val_arr[0];
		audio_mem[IRAM_BASE].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the sprd,iram-phy-addr!\n",
		       __func__);
		return -EINVAL;
	}

	if (!of_property_read_u32_array(np, "sprd,iram_normal",
					&val_arr[0], 2)) {
		audio_mem[IRAM_NORMAL].addr = val_arr[0];
		audio_mem[IRAM_NORMAL].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the sprd,iram_normal!\n",
		       __func__);
		return -EINVAL;
	}

	if (!of_property_read_u32_array(np, "sprd,iram_deepbuf",
					&val_arr[0], 2)) {
		audio_mem[IRAM_DEEPBUF].addr = val_arr[0];
		audio_mem[IRAM_DEEPBUF].size = val_arr[1];
	} else {
		pr_err("%s, sprd,iram_deepbuf!\n", __func__);
		return -EINVAL;
	}

	if (!of_property_read_u32_array(np, "sprd,iram_4arm7",
					&val_arr[0], 2)) {
		audio_mem[IRAM_4ARM7].addr = val_arr[0];
		audio_mem[IRAM_4ARM7].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the sprd,iram_4arm7!\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int audio_mem_sharkl5_parse_dt(struct device_node *np)
{
	u32 val_arr[2] = { 0 };
	struct resource res;
	struct device_node *memnp;
	int ret;
	u32 ddr32_size;

	if (!np) {
		pr_err("%s, np is NULL!\n", __func__);
		return -EINVAL;
	}

	memnp = of_parse_phandle(np, "memory-region", 1);
	if (!memnp) {
		pr_err("get phandle 'memory-region' failed!\n");
		return -ENODEV;
	}
	ret = of_address_to_resource(memnp, 0, &res);
	if (ret) {
		pr_err("of_address_to_resource failed!(%d)\n", ret);
		of_node_put(memnp);
		return ret;
	}
	of_node_put(memnp);
	audio_mem[MEM_AUDCP_DSPBIN].addr = (u32)res.start;
	audio_mem[MEM_AUDCP_DSPBIN].size = (u32)resource_size(&res);
	pr_info("dsp_bin (addr, size): (%#x, %#x)\n",
		audio_mem[MEM_AUDCP_DSPBIN].addr,
		audio_mem[MEM_AUDCP_DSPBIN].size);
	/* DDR32 memory */
	memnp = of_parse_phandle(np, "memory-region", 0);
	if (!memnp) {
		pr_err("get phandle 'memory-region' failed!\n");
		return -ENODEV;
	}
	ret = of_address_to_resource(memnp, 0, &res);
	if (ret != 0) {
		pr_err("of_address_to_resource failed!(%d)\n", ret);
		return -EINVAL;
	}
	of_node_put(memnp);
	ddr32_size = (u32)(res.end - res.start) + 1;
	if (!of_property_read_u32_array
		(np, "ddr32-ap-dsp-map-offset", &val_arr[0], 1)) {
		dma_offset = val_arr[0];
	} else {
		pr_err("%s, ERR:Must give me the dma offset addr!\n", __func__);
		return -EINVAL;
	}

	/* DDR32 memory for DMA, DSP LOG, DSP PCM etc */
	ret = of_property_read_u32_array(np, "sprd,ddr32-dma", &val_arr[0], 2);
	if (ret) {
		pr_err("read 'sprd,ddr32-dma' failed!(%d)\n", ret);
		return ret;
	}
	if (val_arr[1] > ddr32_size) {
		pr_warn("the size(%#x) of 'sprd,ddr32-dma' is greater than the total size(%#x)\n",
			val_arr[0], ddr32_size);
		return -ENOMEM;
	}
	audio_mem[DDR32].addr = val_arr[0];
	audio_mem[DDR32].size = val_arr[1];
	/* DDR32 memory for DSP memory dump */
	ret = of_property_read_u32_array(np,
					 "sprd,ddr32-dspmemdump",
					 &val_arr[0], 2);
	if (ret) {
		pr_err("read 'sprd,ddr32-dspmemdump' failed!(%d)\n", ret);
		return ret;
	}
	if (val_arr[1] + audio_mem[DDR32].size > ddr32_size) {
		pr_warn("the requested size(%#x = %#x + %#x) is greater than the total size(%#x)\n",
			audio_mem[DDR32].size + val_arr[1],
			audio_mem[DDR32].size, val_arr[1], ddr32_size);
		return -ENOMEM;
	}
	audio_mem[DDR32_DSPMEMDUMP].addr = val_arr[0];
	audio_mem[DDR32_DSPMEMDUMP].size = val_arr[1];

	/* DDR32 command params */
	if (!of_property_read_u32_array(np, "sprd,cmdaddr", &val_arr[0], 2)) {
		audio_mem[IRAM_SMS_COMMD_PARAMS].addr = val_arr[0];
		audio_mem[IRAM_SMS_COMMD_PARAMS].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the smsg cmdaddr!\n", __func__);
		return -EINVAL;
	}

	/* DDR32 SMSG */
	if (!of_property_read_u32_array(np, "sprd,smsg-addr", &val_arr[0], 2)) {
		audio_mem[IRAM_SMS].addr = val_arr[0];
		audio_mem[IRAM_SMS].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the smsg addr!\n", __func__);
		return -EINVAL;
	}

	/* DDR32 AUD_STRUCT */
	if (!of_property_read_u32_array(np,
					"sprd,shmaddr-aud-str",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_AUD_STR].addr = val_arr[0];
		audio_mem[IRAM_SHM_AUD_STR].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_a addr!\n", __func__);
	}

	/* DDR32 SHM_DSP_VBC */
	if (!of_property_read_u32_array(np,
					"sprd,shmaddr-dsp-vbc",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_DSP_VBC].addr = val_arr[0];
		audio_mem[IRAM_SHM_DSP_VBC].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_b addr!\n", __func__);
		return -EINVAL;
	}

	/* DDR32 SHM_NXP */
	if (!of_property_read_u32_array(np,
					"sprd,shmaddr-nxp", &val_arr[0], 2)) {
		audio_mem[IRAM_SHM_NXP].addr = val_arr[0];
		audio_mem[IRAM_SHM_NXP].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_c addr!\n", __func__);
	}
	/* DDR32 SHM_IVS_SMARTPA */
	if (!of_property_read_u32_array(np,
					"sprd,shmaddr-dsp-smartpa",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_IVS_SMARTPA].addr = val_arr[0];
		audio_mem[IRAM_SHM_IVS_SMARTPA].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_c addr!\n", __func__);
	}

	/* DDR32 SHM_REG_DUMP */
	if (!of_property_read_u32_array(np,
					"sprd,shmaddr-reg-dump",
					&val_arr[0], 2)) {
		audio_mem[IRAM_SHM_REG_DUMP].addr = val_arr[0];
		audio_mem[IRAM_SHM_REG_DUMP].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the shmaddr_d addr!\n", __func__);
	}
	pr_info("%s ddr32 (COMMD_PARAMS), (SMS), (AUD_STRUCT), (DSP_VBC), (SHM_NXP), (SHM_IVS_SMARTPA) (SHM_REG_DUMP)\n",
		__func__);
	pr_info("ddr32 (addr, size) (%#x, %#x), (%#x, %#x), (%#x, %#x), (%#x, %#x), (%#x, %#x), (%#x, %#x), (%#x, %#x)\n",
		audio_mem[IRAM_SMS_COMMD_PARAMS].addr,
		audio_mem[IRAM_SMS_COMMD_PARAMS].size,
		audio_mem[IRAM_SMS].addr,
		audio_mem[IRAM_SMS].size,
		audio_mem[IRAM_SHM_AUD_STR].addr,
		audio_mem[IRAM_SHM_AUD_STR].size,
		audio_mem[IRAM_SHM_DSP_VBC].addr,
		audio_mem[IRAM_SHM_DSP_VBC].size,
		audio_mem[IRAM_SHM_NXP].addr,
		audio_mem[IRAM_SHM_NXP].size,
		audio_mem[IRAM_SHM_IVS_SMARTPA].addr,
		audio_mem[IRAM_SHM_IVS_SMARTPA].size,
		audio_mem[IRAM_SHM_REG_DUMP].addr,
		audio_mem[IRAM_SHM_REG_DUMP].size);

	/* IRAM */
	/* ap address base for iram */
	if (!of_property_read_u32_array(np, "sprd,iram-ap-base",
					&val_arr[0], 1)) {
		iram_ap_base = val_arr[0];
		pr_info("%s iram_ap_base=%#x\n", __func__, iram_ap_base);
	} else {
		pr_err("%s, ERR:Must give me the ap_base!\n", __func__);
		return -EINVAL;
	}

	/* dsp address base for iram */
	if (!of_property_read_u32_array(np, "sprd,iram-dsp-base",
					&val_arr[0], 1)) {
		iram_dsp_base = val_arr[0];
		pr_info("%s iram_dsp_base=%#x\n", __func__, iram_dsp_base);
	} else {
		pr_err("%s, ERR:Must give me the sprd,dsp_base!\n", __func__);
		return -EINVAL;
	}

	/* IRAM_OFFLOAD */
	if (!of_property_read_u32_array(np, "sprd,offload-addr",
					&val_arr[0], 2)) {
		audio_mem[IRAM_OFFLOAD].addr = val_arr[0];
		audio_mem[IRAM_OFFLOAD].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the offload addr!\n", __func__);
	}

	/* IRAM NORMAL CAPTURE */
	if (!of_property_read_u32_array(np,
					"sprd,normal-captue-linklilst-node1",
					&val_arr[0], 2)) {
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE1].addr = val_arr[0];
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE1].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the iram sprd,normal-captue-linklilst-node1 addr!\n",
		       __func__);
	}

	if (!of_property_read_u32_array(np,
					"sprd,normal-captue-linklilst-node2",
					&val_arr[0], 2)) {
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE2].addr = val_arr[0];
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE2].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the iram sprd,normal-captue-linklilst-node2 addr!\n",
		       __func__);
	}

	if (!of_property_read_u32_array(np, "sprd,normal-captue-data",
					&val_arr[0], 2)) {
		audio_mem[IRAM_NORMAL_C_DATA].addr = val_arr[0];
		audio_mem[IRAM_NORMAL_C_DATA].size = val_arr[1];
	} else {
		pr_err("%s, ERR:Must give me the iram normal capture data addr!\n",
		       __func__);
	}

	pr_info("%s audio iram(OFFLOAD), (NORMAL_CAP NODE1), (NORMAL_CAP NODE2), (NORMAL_CAP DATA)\n",
		__func__);
	pr_info("iram(addr size), (%#x,%#x), (%#x,%#x), (%#x,%#x), (%#x,%#x)\n",
		audio_mem[IRAM_OFFLOAD].addr,
		audio_mem[IRAM_OFFLOAD].size,
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE1].addr,
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE1].size,
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE2].addr,
		audio_mem[IRAM_NORMAL_C_LINKLIST_NODE2].size,
		audio_mem[IRAM_NORMAL_C_DATA].addr,
		audio_mem[IRAM_NORMAL_C_DATA].size);

	/* audio cp aon iram */
	if (!of_property_read_u32_array(np, "sprd,audcp-aon-iram",
					&val_arr[0], 2)) {
		audio_mem[IRAM_AUDCP_AON].addr = val_arr[0];
		audio_mem[IRAM_AUDCP_AON].size = val_arr[1];
		pr_info("iram audcp aon (addr size), (%#x, %#x)\n",
			audio_mem[IRAM_AUDCP_AON].addr,
			audio_mem[IRAM_AUDCP_AON].size);
	} else {
		pr_err("%s, ERR:Must give me the offload addr!\n", __func__);
	}

	return 0;
}

static int audio_mem_orca_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;
	u32 val_arr[2];
	struct resource res;
	struct device_node *memnp;
	u32 ddr32_size;

	if (!np) {
		pr_err("%s, np is NULL!\n", __func__);
		return -EINVAL;
	}

	memnp = of_parse_phandle(np, "memory-region", 1);
	if (!memnp) {
		pr_err("get phandle 'memory-region' failed!\n");
		return -ENODEV;
	}
	ret = of_address_to_resource(memnp, 0, &res);
	if (ret != 0) {
		pr_err("of_address_to_resource failed!(%d)\n", ret);
		return -EINVAL;
	}
	of_node_put(memnp);
	audio_mem[MEM_AUDCP_DSPBIN].addr = (u32)res.start;
	audio_mem[MEM_AUDCP_DSPBIN].size = (u32)resource_size(&res);
	pr_info("dsp_bin (addr, size): (%#x, %#x)\n",
		audio_mem[MEM_AUDCP_DSPBIN].addr,
		audio_mem[MEM_AUDCP_DSPBIN].size);

	/* DDR32 memory */
	memnp = of_parse_phandle(np, "memory-region", 0);
	if (!memnp) {
		pr_err("get phandle 'memory-region' failed!\n");
		return -ENODEV;
	}
	ret = of_address_to_resource(memnp, 0, &res);
	if (ret) {
		pr_err("of_address_to_resource failed!(%d)\n", ret);
		return ret;
	}
	of_node_put(memnp);
	ddr32_size = resource_size(&res);
	if (!of_property_read_u32_array
		(np, "ddr32-ap-dsp-map-offset", &val_arr[0], 1)) {
		dma_offset = val_arr[0];
	} else {
		pr_err("%s, ERR:Must give me the dma offset addr!\n", __func__);
		return -EINVAL;
	}
	/* DDR32 memory for, DSP LOG, DSP PCM */
	ret = of_property_read_u32_array(np, "sprd,ddr32-dma", &val_arr[0], 2);
	if (ret) {
		pr_err("read 'sprd,ddr32-dma' failed!(%d)\n", ret);
		return ret;
	}
	audio_mem[DDR32].addr = val_arr[0];
	audio_mem[DDR32].size = val_arr[1];
	if (audio_mem[DDR32].addr && audio_mem[DDR32].size) {
		ret = audio_smem_init(audio_mem[DDR32].addr,
				      audio_mem[DDR32].size);
		if (ret) {
			pr_info("%s audio_smem_init failed ret =%d\n", __func__,
				ret);
			return ret;
		}
	}

	/* DDR32 memory for DSP memory dump */
	ret = of_property_read_u32_array(np, "sprd,ddr32-dspmemdump",
					 &val_arr[0], 2);
	if (ret) {
		pr_err("read 'sprd,ddr32-dspmemdump' failed!(%d)\n", ret);
		return ret;
	}
	audio_mem[DDR32_DSPMEMDUMP].addr = val_arr[0];
	audio_mem[DDR32_DSPMEMDUMP].size = val_arr[1];

	/* DDR32 command params */
	ret = of_property_read_u32_array(np, "sprd,cmdaddr", &val_arr[0], 2);
	if (ret) {
		pr_err("%s, ERR:Must give me the smsg cmdaddr!\n", __func__);
		return ret;
	}
	audio_mem[IRAM_SMS_COMMD_PARAMS].addr = val_arr[0];
	audio_mem[IRAM_SMS_COMMD_PARAMS].size = val_arr[1];

	/* DDR32 SMSG */
	ret = of_property_read_u32_array(np, "sprd,smsg-addr", &val_arr[0], 2);
	if (ret) {
		pr_err("%s, ERR:Must give me the smsg addr!\n", __func__);
		return -ret;
	}
	audio_mem[IRAM_SMS].addr = val_arr[0];
	audio_mem[IRAM_SMS].size = val_arr[1];
	pr_info("%s ddr32 (COMMD_PARAMS), (SMS), (DSPMEMDUMP), (DDR32)\n",
		__func__);
	pr_info("ddr32 (addr, size) (%#x, %#x), (%#x, %#x), (%#x, %#x), (%#x, %#x)\n",
		audio_mem[IRAM_SMS_COMMD_PARAMS].addr,
		audio_mem[IRAM_SMS_COMMD_PARAMS].size,
		audio_mem[IRAM_SMS].addr,
		audio_mem[IRAM_SMS].size,
		audio_mem[DDR32_DSPMEMDUMP].addr,
		audio_mem[DDR32_DSPMEMDUMP].size,
		audio_mem[DDR32].addr, audio_mem[DDR32].size);

	/* audio cp aon iram */
	if (!of_property_read_u32_array(np, "sprd,audcp-aon-iram",
					&val_arr[0], 2)) {
		audio_mem[IRAM_AUDCP_AON].addr = val_arr[0];
		audio_mem[IRAM_AUDCP_AON].size = val_arr[1];
		pr_info("iram audcp aon (addr size), (%#x, %#x)\n",
			audio_mem[IRAM_AUDCP_AON].addr,
			audio_mem[IRAM_AUDCP_AON].size);
	} else {
		pr_err("%s, ERR:Must give me the offload addr!\n", __func__);
	}

	return 0;
}

static int audio_mem_sharkl5_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;
	int i;

	ret = audio_mem_sharkl5_parse_dt(np);
	if (ret) {
		pr_err("%s audio_mem_parse_dt failed\n", __func__);
		return -1;
	}

	if (0 != audio_mem[DDR32].addr && 0 != audio_mem[DDR32].size) {
		ret = audio_smem_init(audio_mem[DDR32].addr,
				      audio_mem[DDR32].size);
		if (ret) {
			pr_info("%s audio_smem_init failed ret =%d\n", __func__,
				ret);
			return ret;
		}
	}

	for (i = 0; i < AUDIO_MEM_TYPE_MAX; i++) {
		pr_info("%s addr= %#x, size=%#x\n", __func__,
			audio_mem[i].addr, audio_mem[i].size);
	}

	return 0;
}

static int audio_mem_sharkl2_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	ret = audio_mem_sharkl2_parse_dt(np);
	if (ret) {
		pr_err("%s audio_mem_parse_dt failed\n", __func__);
		return -1;
	}

	return 0;
}

static int audio_mem_whale2_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct device_node *np = pdev->dev.of_node;

	ret = audio_mem_whale2_parse_dt(np);
	if (ret) {
		pr_err("%s audio_mem_parse_dt failed\n", __func__);
		return -1;
	}

	if (0 != audio_mem[DDR32].addr && 0 != audio_mem[DDR32].size) {
		ret = audio_smem_init(audio_mem[DDR32].addr,
				      audio_mem[DDR32].size);
		if (ret) {
			pr_info("%s audio_smem_init failed ret =%d\n", __func__,
				ret);
			return ret;
		}
	}

	for (i = 0; i < AUDIO_MEM_TYPE_MAX; i++) {
		pr_info("%s addr= %#x, size=%#x\n", __func__,
			audio_mem[i].addr, audio_mem[i].size);
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id audio_mem_match_table[] = {
	{.compatible = "sprd,sharkl3-audio-mem",
	 .data = (void *)PLATFORM_SHARKL2},
	{.compatible = "sprd,audio-mem-whale2",
	 .data = (void *)PLATFORM_WHALE2},
	{.compatible = "sprd,audio-mem-sharkl5",
	 .data = (void *)PLATFORM_SHARKL5},
	{.compatible = "sprd,audio-mem-roc1",
	 .data = (void *)PLATFORM_SHARKL5},
	{.compatible = "sprd,audio-mem-orca",
	 .data = (void *)PLATFORM_ORCA},
	{},
};

MODULE_DEVICE_TABLE(of, audio_mem_match_table);
#endif

static int audio_mem_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id;

	if (!np) {
		pr_err("ERR: device_node is NULL!\n");
		return -1;
	}

	of_id = of_match_node(audio_mem_match_table, pdev->dev.of_node);
	if (!of_id) {
		pr_err("%s line[%d] Get the pcm of device id failed!\n",
		       __func__, __LINE__);
		return -ENODEV;
	}

	g_priv_data =
	    devm_kzalloc(&pdev->dev, sizeof(*g_priv_data), GFP_KERNEL);
	if (!g_priv_data)
		return -ENOMEM;

	g_priv_data->platform_type = (long)of_id->data;
	if (g_priv_data->platform_type == PLATFORM_WHALE2)
		return audio_mem_whale2_probe(pdev);
	if (g_priv_data->platform_type == PLATFORM_SHARKL2)
		return audio_mem_sharkl2_probe(pdev);
	if (g_priv_data->platform_type == PLATFORM_SHARKL5)
		return audio_mem_sharkl5_probe(pdev);
	if (g_priv_data->platform_type == PLATFORM_ORCA)
		return audio_mem_orca_probe(pdev);

	pr_err("%s %d unknown platform_type[%d]\n",
	       __func__, __LINE__, g_priv_data->platform_type);

	return -1;
}

static int audio_mem_remove(struct platform_device *pdev)
{
	if (g_priv_data) {
		devm_kfree(&pdev->dev, g_priv_data);
		g_priv_data = NULL;
	}

	return 0;
}

static struct platform_driver audio_mem_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "audio_mem",
		.of_match_table = audio_mem_match_table,
	},
	.probe = audio_mem_probe,
	.remove = audio_mem_remove,
};

static int __init audio_mem_init(void)
{
	return platform_driver_register(&audio_mem_driver);
}

static void __exit audio_mem_exit(void)
{
	platform_driver_unregister(&audio_mem_driver);
}

arch_initcall(audio_mem_init);
module_exit(audio_mem_exit);

MODULE_DESCRIPTION("audio memory manager driver");
MODULE_LICENSE("GPL v2");
