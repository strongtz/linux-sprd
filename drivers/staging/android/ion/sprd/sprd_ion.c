/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
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

#include <asm/cacheflush.h>
#include <linux/compat.h>
#include <linux/dma-buf.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include "../ion.h"

static int num_heaps;
struct ion_heap **heaps;

static struct ion_buffer *get_ion_buffer(int fd, struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd,
		       dmabuf);
		return ERR_PTR(-EINVAL);
	}

	if (fd >= 0) {
		dmabuf = dma_buf_get(fd);
		if (IS_ERR_OR_NULL(dmabuf)) {
			pr_err("%s, dmabuf=%p dma_buf_get error!\n", __func__,
			       dmabuf);
			return ERR_PTR(-EBADF);
		}
		buffer = dmabuf->priv;
		dma_buf_put(dmabuf);
	} else {
		buffer = dmabuf->priv;
	}

	return buffer;
}

void get_ion_user_info(int fd, bool map)
{
	int i;
	struct ion_buffer *buffer;
	struct task_struct *task = current->group_leader;
	pid_t pid = task_pid_nr(task);
	struct dma_buf *dmabuf = dma_buf_get(fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		return;
	}

	if (strcmp(dmabuf->exp_name, "ion"))
		goto out;

	buffer = dmabuf->priv;
	for (i = 0; i < MAX_MAP_USER; i++) {
		if (map) {
			if (!(buffer->mappers[i].fd)) {
				buffer->mappers[i].pid = pid;
				get_task_comm(buffer->mappers[i].task_name, task);
				buffer->mappers[i].fd = fd;
				buffer->mappers[i].valid = true;
				do_gettimeofday(&(buffer->mappers[i].map_time));
				buffer->mappers[i].map_time.tv_sec -= sys_tz.tz_minuteswest * 60;
				break;
			}
		} else {
			if (buffer->mappers[i].pid == pid && buffer->mappers[i].fd == fd && buffer->mappers[i].valid) {
				memset((void *)(&buffer->mappers[i]), 0x0, sizeof(buffer->mappers[i]));
				break;
			}
		}
	}
	if (map && i == MAX_MAP_USER)
		pr_err("%s: pid %d fd %d\n", __func__, pid, fd);

out:
	dma_buf_put(dmabuf);
}

int sprd_ion_is_reserved(int fd, struct dma_buf *dmabuf, bool *reserved)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	/* The range of system reserved memory is in 0x5 xxxxxxxx,*/
	/* so master must use IOMMU to access it*/

	if (buffer->heap->type == ION_HEAP_TYPE_CARVEOUT &&
	    buffer->heap->id != ION_HEAP_ID_SYSTEM)
		*reserved = true;
	else
		*reserved = false;

	return 0;
}
EXPORT_SYMBOL(sprd_ion_is_reserved);

int sprd_ion_get_sg_table(int fd, struct dma_buf *dmabuf,
			  struct sg_table **table, size_t *size)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*table = buffer->sg_table;
	*size = buffer->size;

	return 0;
}

int sprd_ion_get_buffer(int fd, struct dma_buf *dmabuf,
			void **buf, size_t *size)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->size;

	return 0;
}
EXPORT_SYMBOL(sprd_ion_get_buffer);

int sprd_ion_get_sg(void *buf, struct sg_table **table)
{
	struct ion_buffer *buffer;

	if (!buf) {
		pr_err("%s, buf==NULL", __func__);
		return -EINVAL;
	}

	buffer = (struct ion_buffer *)buf;
	*table = buffer->sg_table;

	return 0;
}

void sprd_ion_set_dma(void *buf, int id)
{
	struct ion_buffer *buffer = (struct ion_buffer *)buf;

	if (id < 0 || id >= SPRD_IOMMU_MAX) {
		pr_err("%s: iommu id %d out of range!", __func__, id);
		return;
	}

	if (buffer->iomap_cnt[id] == 0x6b6b6b6b) {
		pr_err("%s: iommu id %d memory corruption!", __func__, id);
		dump_stack();
	}
	buffer->iomap_cnt[id]++;
}

void sprd_ion_put_dma(void *buf, int id)
{
	struct ion_buffer *buffer = (struct ion_buffer *)buf;

	if (id < 0 || id >= SPRD_IOMMU_MAX) {
		pr_err("%s: iommu id %d out of range!", __func__, id);
		return;
	}

	if (buffer->iomap_cnt[id] == 0x6b6b6b6b) {
		pr_err("%s: iommu id %d memory corruption!", __func__, id);
		dump_stack();
	}
	buffer->iomap_cnt[id]--;
}

void sprd_ion_unmap_dma(void *buffer)
{
	int i;
	struct ion_buffer *buf = (struct ion_buffer *)buffer;
	struct sprd_iommu_unmap_data data;

	for (i = 0; i < SPRD_IOMMU_MAX; i++) {
		if (buf->iomap_cnt[i]) {
			buf->iomap_cnt[i] = 0;
			data.buf = buffer;
			data.table = buf->sg_table;
			data.iova_size = buf->size;
			data.dev_id = i;
			sprd_iommu_unmap_orphaned(&data);
		}
	}
}

int sprd_ion_get_phys_addr(int fd, struct dma_buf *dmabuf,
			   unsigned long *phys_addr, size_t *size)
{
	int ret = 0;
	struct ion_buffer *buffer;
	struct sg_table *table = NULL;
	struct scatterlist *sgl = NULL;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	if (buffer->heap->type == ION_HEAP_TYPE_CARVEOUT) {
		table = buffer->sg_table;
		if (table && table->sgl) {
			sgl = table->sgl;
		} else {
			if (!table)
				pr_err("invalid table\n");
			else if (!table->sgl)
				pr_err("invalid table->sgl\n");
			return -EINVAL;
		}

		*phys_addr = sg_phys(sgl);
		*size = buffer->size;
	} else {
		pr_err("%s, buffer heap type:%d error\n", __func__,
		       buffer->heap->type);
		return -EPERM;
	}

	return ret;
}
EXPORT_SYMBOL(sprd_ion_get_phys_addr);

int sprd_ion_get_phys_addr_by_db(struct dma_buf *dmabuf,
				 unsigned long *phys_addr,
				 size_t *size)
{
	struct ion_buffer *buffer;
	struct sg_table *table = NULL;
	struct scatterlist *sgl = NULL;

	if (!dmabuf)
		return -EINVAL;

	buffer = dmabuf->priv;
	if (IS_ERR_OR_NULL(buffer))
		return PTR_ERR(buffer);

	if (buffer->heap->type == ION_HEAP_TYPE_CARVEOUT) {
		table = buffer->sg_table;
		if (table && table->sgl) {
			sgl = table->sgl;
		} else {
			if (!table)
				pr_err("invalid table\n");
			else if (!table->sgl)
				pr_err("invalid table->sgl\n");
			return -EINVAL;
		}

		*phys_addr = sg_phys(sgl);
		*size = buffer->size;
	} else {
		pr_err("%s, buffer heap type:%d error\n", __func__,
		       buffer->heap->type);
		return -EPERM;
	}

	return 0;
}
EXPORT_SYMBOL(sprd_ion_get_phys_addr_by_db);

void *sprd_ion_map_kernel(struct dma_buf *dmabuf, unsigned long offset)
{
	void *vaddr;

	if (!dmabuf)
		return ERR_PTR(-EINVAL);

	dmabuf->ops->begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	vaddr = dmabuf->ops->map(dmabuf, offset);

	return vaddr;
}
EXPORT_SYMBOL(sprd_ion_map_kernel);

int sprd_ion_unmap_kernel(struct dma_buf *dmabuf, unsigned long offset)
{
	if (!dmabuf)
		return -EINVAL;

	dmabuf->ops->unmap(dmabuf, offset, NULL);
	dmabuf->ops->end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);

	return 0;
}
EXPORT_SYMBOL(sprd_ion_unmap_kernel);

static struct ion_platform_heap *sprd_ion_parse_dt(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	const struct device_node *parent = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct ion_platform_heap *ion_heaps = NULL;
	struct platform_device *new_dev = NULL;
	u32 val = 0, type = 0;
	const char *name;
	u32 out_values[4];
	struct device_node *np_memory;

	for_each_child_of_node(parent, child)
		num_heaps++;

	pr_info("%s: num_heaps=%d\n", __func__, num_heaps);

	if (!num_heaps)
		return NULL;

	ion_heaps = kcalloc(num_heaps, sizeof(struct ion_platform_heap),
			    GFP_KERNEL);
	if (!ion_heaps)
		return ERR_PTR(-ENOMEM);

	for_each_child_of_node(parent, child) {
		new_dev = of_platform_device_create(child, NULL, &pdev->dev);
		if (!new_dev) {
			pr_err("Failed to create device %s\n", child->name);
			goto out;
		}

		ion_heaps[i].priv = &new_dev->dev;

		ret = of_property_read_u32(child, "reg", &val);
		if (ret) {
			pr_err("%s: Unable to find reg key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].id = val;

		ret = of_property_read_string(child, "label", &name);
		if (ret) {
			pr_err("%s: Unable to find label key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].name = name;

		ret = of_property_read_u32(child, "type", &type);
		if (ret) {
			pr_err("%s: Unable to find type key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].type = type;

		np_memory = of_parse_phandle(child, "memory-region", 0);

		if (!np_memory) {
			ion_heaps[i].base = 0;
			ion_heaps[i].size = 0;
		} else {
#ifdef CONFIG_64BIT
			ret = of_property_read_u32_array(np_memory, "reg",
							 out_values, 4);
			if (!ret) {
				ion_heaps[i].base = out_values[0];
				ion_heaps[i].base = ion_heaps[i].base << 32;
				ion_heaps[i].base |= out_values[1];

				ion_heaps[i].size = out_values[2];
				ion_heaps[i].size = ion_heaps[i].size << 32;
				ion_heaps[i].size |= out_values[3];
			} else {
				ion_heaps[i].base = 0;
				ion_heaps[i].size = 0;
			}
#else
			ret = of_property_read_u32_array(np_memory, "reg",
							 out_values, 2);
			if (!ret) {
				ion_heaps[i].base = out_values[0];
				ion_heaps[i].size = out_values[1];
			} else {
				ion_heaps[i].base = 0;
				ion_heaps[i].size = 0;
			}
#endif
		}

		pr_info("%s: heaps[%d]: id: %u %s type: %d base: 0x%llx size 0x%zx\n",
			__func__, i,
			ion_heaps[i].id,
			ion_heaps[i].name,
			ion_heaps[i].type,
			(u64)(ion_heaps[i].base),
			ion_heaps[i].size);
		++i;
	}
	return ion_heaps;
out:
	kfree(ion_heaps);
	return ERR_PTR(ret);
}

#ifdef CONFIG_E_SHOW_MEM
static int ion_e_show_mem_handler(struct notifier_block *nb,
				unsigned long val, void *data)
{
	int i;
	enum e_show_mem_type type = (enum e_show_mem_type)val;
	unsigned long total_used = 0;

	if (!heaps)
		return -EINVAL;

	pr_info("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	pr_info("Enhanced Mem-info :ION\n");
	for (i = 0; i <= num_heaps; i++) {
		if ((E_SHOW_MEM_BASIC != type) ||
		    (ION_HEAP_TYPE_SYSTEM == heaps[i]->type ||
		     ION_HEAP_TYPE_SYSTEM_CONTIG == heaps[i]->type)) {
			pr_info("%s: heap_id %d\n", __func__, heaps[i]->id);
			ion_debug_heap_show_printk(heaps[i], type, &total_used);
		}
	}

	pr_info("Total allocated from Buddy: %lu kB\n", total_used / 1024);
	return 0;
}

static struct notifier_block ion_e_show_mem_notifier = {
	.notifier_call = ion_e_show_mem_handler,
};
#endif

static int sprd_ion_probe(struct platform_device *pdev)
{
	int i = 0, ret = -1;
	struct ion_platform_heap *ion_heaps = NULL;

	ion_heaps = sprd_ion_parse_dt(pdev);
	if (IS_ERR(ion_heaps)) {
		pr_err("%s: parse dt failed with err %ld\n",
			__func__, PTR_ERR(ion_heaps));
		return PTR_ERR(ion_heaps);
	}

	heaps = kcalloc(num_heaps + 1, sizeof(struct ion_heap *), GFP_KERNEL);
	if (!heaps) {
		ret = -ENOMEM;
		goto out1;
	}

	/* create the heaps as specified in the board file */
	for (i = 1; i <= num_heaps; i++) {
		struct ion_platform_heap *heap_data = &ion_heaps[i - 1];

		if (!pdev->dev.of_node)
			heap_data->priv = &pdev->dev;
		heaps[i] = ion_carveout_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			pr_err("%s,heaps is null, i:%d\n", __func__, i);
			ret = PTR_ERR(heaps[i]);
			goto out;
		}
		ion_device_add_heap(heaps[i]);
	}
#ifdef CONFIG_E_SHOW_MEM
	register_e_show_mem_notifier(&ion_e_show_mem_notifier);
#endif

	if (ion_heaps)
		kfree(ion_heaps);
	return 0;

out:
	kfree(heaps);
out1:
	kfree(ion_heaps);

	return ret;
}

static int sprd_ion_remove(struct platform_device *pdev)
{
#ifdef CONFIG_E_SHOW_MEM
	unregister_e_show_mem_notifier(&ion_e_show_mem_notifier);
#endif
	kfree(heaps);

	return 0;
}

static const struct of_device_id sprd_ion_ids[] = {
	{ .compatible = "sprd,ion"},
	{},
};

static struct platform_driver ion_driver = {
	.probe = sprd_ion_probe,
	.remove = sprd_ion_remove,
	.driver = {
		.name = "ion",
		.of_match_table = of_match_ptr(sprd_ion_ids),
	}
};

static int __init sprd_ion_init(void)
{
	int result = 0;

	result = platform_driver_register(&ion_driver);
	pr_info("%s,result:%d\n", __func__, result);
	return result;
}

static void __exit sprd_ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

subsys_initcall(sprd_ion_init);
