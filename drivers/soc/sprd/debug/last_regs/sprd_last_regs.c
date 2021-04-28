#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/swiotlb.h>
#include <linux/slab.h>
#include "regs_debug.h"
#include "../sprd_debugfs.h"

struct sprd_debug_regs_access *sprd_debug_last_regs_access;
EXPORT_SYMBOL(sprd_debug_last_regs_access);

static int _last_regs_info_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned int ncores =  num_possible_cpus();

	for (i = 0; i < ncores; i++) {
		seq_printf(m, "cpu  %d\n", i);
		seq_printf(m, "vaddr:%lx value:%x pc:%lx time:%ld status:%d\n",
				sprd_debug_last_regs_access[i].vaddr,
				sprd_debug_last_regs_access[i].value,
				sprd_debug_last_regs_access[i].pc,
				sprd_debug_last_regs_access[i].time,
				sprd_debug_last_regs_access[i].status);
	}

	return 0;
}

static int _last_regs_info_proc_open(struct inode *inode, struct file *file)
{
	single_open(file, _last_regs_info_proc_show, NULL);
	return 0;
}

static const struct file_operations last_regs_info_proc_fops = {
	.open    = _last_regs_info_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static u64 sprd_debug_dma_mask = ~0ULL;

static struct device_dma_parameters sprd_debug_ddp = {
		.segment_boundary_mask = ~0UL,
	};
static struct device sprd_debug_device;

static __init int sprd_debug_last_regs_init(void)
{
	dma_addr_t addr;
	int size = sizeof(struct sprd_debug_regs_access);
	unsigned int ncores =  num_possible_cpus();
	struct sprd_debug_regs_access *tmp;

	memset(&sprd_debug_device, 0, sizeof(struct device));

	sprd_debug_device.coherent_dma_mask = ~0ULL;
	sprd_debug_device.dma_mask = &sprd_debug_dma_mask;
	sprd_debug_device.dma_parms = &sprd_debug_ddp;

	arch_setup_dma_ops(&sprd_debug_device, 0, 0, NULL, false);

	tmp = (struct sprd_debug_regs_access *)dma_alloc_coherent(&sprd_debug_device,
								  size*ncores,
								  &addr,
								  GFP_KERNEL);
	sprd_debug_last_regs_access = tmp;

	pr_info("*** %s, size:%u, sprd_debug_last_regs_access:%p ***\n",
		__func__, size*ncores, sprd_debug_last_regs_access);
	debugfs_create_file("last_regs",
			    0644,
			    sprd_debugfs_entry(IO),
			    NULL,
			    &last_regs_info_proc_fops);

	return 0;
}
subsys_initcall(sprd_debug_last_regs_init);
