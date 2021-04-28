/*
 * drivers/staging/android/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"

#define NUM_ORDERS ARRAY_SIZE(orders)

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN;
#ifdef CONFIG_ARM64
static const unsigned int orders[] = {8, 4, 0};
#endif
#ifdef CONFIG_ARM
static const unsigned int orders[] = {4, 1, 0};
#endif

static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool *uncached_pools[NUM_ORDERS];
	struct ion_page_pool *cached_pools[NUM_ORDERS];
};

struct page_info {
	struct page *page;
	bool from_pool;
	unsigned int order;
	struct list_head list;
};

/**
 * The page from page-pool are all zeroed before. We need do cache
 * clean for cached buffer. The uncached buffer are always non-cached
 * since it's allocated. So no need for non-cached pages.
 */
static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order,
				      bool *from_pool)
{
	bool cached = ion_buffer_cached(buffer);
	struct ion_page_pool *pool;
	struct page *page;

	if (!cached)
		pool = heap->uncached_pools[order_to_index(order)];
	else
		pool = heap->cached_pools[order_to_index(order)];

	page = ion_page_pool_alloc(pool, from_pool);

	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page)
{
	struct ion_page_pool *pool;
	unsigned int order = compound_order(page);
	bool cached = ion_buffer_cached(buffer);

	/* go to system */
	if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) {
		__free_pages(page, order);
		return;
	}

	if (!cached)
		pool = heap->uncached_pools[order_to_index(order)];
	else
		pool = heap->cached_pools[order_to_index(order)];

	ion_page_pool_free(pool, page);
}

static struct page_info *alloc_largest_available(struct ion_system_heap *heap,
						 struct ion_buffer *buffer,
						 unsigned long size,
						 unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;
	bool from_pool;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i], &from_pool);
		if (!page)
			continue;

		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (info) {
			info->page = page;
			info->order = orders[i];
			info->from_pool = from_pool;
		}
		return info;
	}

	return NULL;
}

void set_sg_info(struct page_info *info, struct scatterlist *sg)
{
	sg_set_page(sg, info->page, (1 << info->order) * PAGE_SIZE, 0);
	list_del(&info->list);
	kfree(info);
}
static int ion_system_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size,
				    unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct list_head pages_from_pool;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	struct timeval val_start;
	struct timeval val_end;
	u64 time_start;
	u64 time_end;
	unsigned int sz;
	struct page_info *info, *tmp_info;
	unsigned long pool_sz = 0;
	unsigned long buddy_sz = 0;
	unsigned int buddy_orders[NUM_ORDERS] = {0};

	if (size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

	do_gettimeofday(&val_start);
	time_start = val_start.tv_sec * 1000000LL + val_start.tv_usec;
	INIT_LIST_HEAD(&pages);
	INIT_LIST_HEAD(&pages_from_pool);
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining,
					       max_order);
		if (!info)
			goto free_pages;

		sz = (1 << info->order) * PAGE_SIZE;
		if (info->from_pool) {
			pool_sz += sz;
			list_add_tail(&info->list, &pages_from_pool);
		} else {
			int index;

			for (index = 0; index < NUM_ORDERS; index++) {
				if (info->order == orders[index]) {
					buddy_orders[index]++;
					break;
				}
			}
			buddy_sz += sz;
			list_add_tail(&info->list, &pages);
		}

		size_remaining -= sz;
		max_order = info->order;
		i++;
	}

	do_gettimeofday(&val_end);
	time_end = val_end.tv_sec * 1000000LL + val_end.tv_usec;
	pr_info("%s, size:%8ld, time:%lldus, pool:%ld, bud:%ld, ord 8:%d, 4:%d, 0:%d\n",
		 __func__, size, time_end - time_start, pool_sz, buddy_sz,
		 buddy_orders[0], buddy_orders[1], buddy_orders[2]);

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto free_pages;

	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_table;

	sg = table->sgl;
	do {
		info = list_first_entry_or_null(&pages, struct page_info, list);
		tmp_info = list_first_entry_or_null(&pages_from_pool,
						    struct page_info, list);
		if (info && tmp_info) {
			if (info->order >= tmp_info->order)
				set_sg_info(info, sg);
			else
				set_sg_info(tmp_info, sg);
		} else if (info) {
			set_sg_info(info, sg);
		} else if (tmp_info) {
			set_sg_info(tmp_info, sg);
		} else {
			WARN_ON(1);
		}
		sg = sg_next(sg);
	} while (sg);

	buffer->sg_table = table;
	return 0;

free_table:
	kfree(table);
free_pages:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page);
		kfree(info);
	}
	list_for_each_entry_safe(info, tmp_info, &pages_from_pool, list) {
		free_buffer_page(sys_heap, buffer, info->page);
		kfree(info);
	}
	return -ENOMEM;
}

static void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_system_heap *sys_heap = container_of(buffer->heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;

	/* zero the buffer before goto page pool */
	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE))
		ion_heap_buffer_zero(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg));
	sg_free_table(table);
	kfree(table);
}

static int ion_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
				  int nr_to_scan)
{
	struct ion_page_pool *uncached_pool;
	struct ion_page_pool *cached_pool;
	struct ion_system_heap *sys_heap;
	int nr_total = 0;
	int i, nr_freed;
	int only_scan = 0;

	sys_heap = container_of(heap, struct ion_system_heap, heap);

	if (!nr_to_scan)
		only_scan = 1;

#ifdef CONFIG_ARM64
	for (i = NUM_ORDERS - 1; i >= 0 ; i--) {
#else
	for (i = 0; i < NUM_ORDERS; i++) {
#endif
		uncached_pool = sys_heap->uncached_pools[i];
		cached_pool = sys_heap->cached_pools[i];

		if (only_scan) {
			nr_total += ion_page_pool_shrink(uncached_pool,
							 gfp_mask,
							 nr_to_scan);

			nr_total += ion_page_pool_shrink(cached_pool,
							 gfp_mask,
							 nr_to_scan);
		} else {
			nr_freed = ion_page_pool_shrink(uncached_pool,
							gfp_mask,
							nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
			nr_freed = ion_page_pool_shrink(cached_pool,
							gfp_mask,
							nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
	}
	return nr_total;
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = ion_system_heap_shrink,
};

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

#ifdef CONFIG_E_SHOW_MEM
	unsigned long *pool_used = (unsigned long *)unused;
	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool = sys_heap->uncached_pools[i];

		if (s) {
			seq_printf(s, "%d order %u highmem pages \
					in uncached pool = %lu total\n",
				   pool->high_count, pool->order,
				 (PAGE_SIZE << pool->order) * pool->high_count);
			seq_printf(s, "%d order %u lowmem pages \
				in uncached pool = %lu total\n",
				pool->low_count, pool->order,
				(PAGE_SIZE << pool->order) * pool->low_count);
		} else {
			if (!pool_used) {
				pr_info("%3d order %u highmem pages \
					in uncached pool = %12lu total\n",
					pool->high_count, pool->order,
					(1 << pool->order) * PAGE_SIZE
					* pool->high_count);
				pr_info("%3d order %u  lowmem pages \
					in uncached pool = %12lu total\n",
					pool->low_count, pool->order,
					(1 << pool->order) * PAGE_SIZE
					* pool->low_count);
			} else {
				*pool_used += (1 << pool->order) * PAGE_SIZE
						* pool->high_count;
				*pool_used += (1 << pool->order) * PAGE_SIZE
						* pool->low_count;
			}

		}
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool = sys_heap->cached_pools[i];

		if (s) {
			seq_printf(s, "%d order %u highmem pages \
					in cached pool = %lu total\n",
				   pool->high_count, pool->order,
				 (PAGE_SIZE << pool->order) * pool->high_count);
			seq_printf(s, "%d order %u lowmem pages \
					in cached pool = %lu total\n",
				   pool->low_count, pool->order,
				   (PAGE_SIZE << pool->order) * pool->low_count);
		} else {
			if (!pool_used) {
				pr_info("%3d order %u highmem pages \
					in cached pool = %12lu total\n",
					pool->high_count, pool->order,
					(1 << pool->order) * PAGE_SIZE
					* pool->high_count);
				pr_info("%3d order %u  lowmem pages \
					in cached pool = %12lu total\n",
					pool->low_count, pool->order,
					(1 << pool->order) * PAGE_SIZE
					* pool->low_count);
			} else {
				*pool_used += (1 << pool->order) * PAGE_SIZE
						* pool->high_count;
				*pool_used += (1 << pool->order) * PAGE_SIZE
						* pool->low_count;
			}
		}
	}
#else
	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool = sys_heap->uncached_pools[i];

		seq_printf(s, "%d order %u highmem pages in uncached pool = %lu total\n",
			   pool->high_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in uncached pool = %lu total\n",
			   pool->low_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->low_count);
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool = sys_heap->cached_pools[i];

		seq_printf(s, "%d order %u highmem pages in cached pool = %lu total\n",
			   pool->high_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in cached pool = %lu total\n",
			   pool->low_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->low_count);
	}
#endif
	return 0;
}

static void ion_system_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (pools[i])
			ion_page_pool_destroy(pools[i]);
}

static int ion_system_heap_create_pools(struct ion_page_pool **pools,
					bool cached)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i])
			gfp_flags = high_order_gfp_flags;
#ifdef CONFIG_ARM64
		if (orders[i] == 4)
			gfp_flags |= __GFP_KSWAPD_RECLAIM;
#endif

		pool = ion_page_pool_create(gfp_flags, orders[i], cached);
		if (!pool)
			goto err_create_pool;
		pools[i] = pool;
	}
	return 0;

err_create_pool:
	ion_system_heap_destroy_pools(pools);
	return -ENOMEM;
}

static struct ion_heap *__ion_system_heap_create(void)
{
	struct ion_system_heap *heap;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->heap.id = 0;

	if (ion_system_heap_create_pools(heap->uncached_pools, false))
		goto free_heap;

	if (ion_system_heap_create_pools(heap->cached_pools, true))
		goto destroy_uncached_pools;

	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;

destroy_uncached_pools:
	ion_system_heap_destroy_pools(heap->uncached_pools);

free_heap:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

extern struct ion_heap **heaps;
static int ion_system_heap_create(void)
{
	struct ion_heap *heap;

	heap = __ion_system_heap_create();
	if (IS_ERR(heap))
		return PTR_ERR(heap);
	heap->name = "system";

	if (heaps)
		heaps[0] = heap;
	ion_device_add_heap(heap);

	return 0;
}
device_initcall(ion_system_heap_create);
/*
static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long flags)
{
	int order = get_order(len);
	struct page *page;
	struct sg_table *table;
	unsigned long i;
	int ret;

	page = alloc_pages(low_order_gfp_flags | __GFP_NOWARN, order);
	if (!page)
		return -ENOMEM;

	split_page(page, order);

	len = PAGE_ALIGN(len);
	for (i = len >> PAGE_SHIFT; i < (1 << order); i++)
		__free_page(page + i);

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto free_pages;
	}

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_table;

	sg_set_page(table->sgl, page, len, 0);

	buffer->sg_table = table;

	return 0;

free_table:
	kfree(table);
free_pages:
	for (i = 0; i < len >> PAGE_SHIFT; i++)
		__free_page(page + i);

	return ret;
}

static void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	unsigned long pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;
	unsigned long i;

	for (i = 0; i < pages; i++)
		__free_page(page + i);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
};

static struct ion_heap *__ion_system_contig_heap_create(void)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	heap->name = "ion_system_contig_heap";
	return heap;
}

static int ion_system_contig_heap_create(void)
{
	struct ion_heap *heap;

	heap = __ion_system_contig_heap_create();
	if (IS_ERR(heap))
		return PTR_ERR(heap);

	ion_device_add_heap(heap);
	return 0;
}
device_initcall(ion_system_contig_heap_create);
*/
