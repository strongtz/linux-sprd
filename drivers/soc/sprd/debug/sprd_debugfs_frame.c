#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/stat.h>
#include "sprd_debugfs.h"

struct sprd_debug_dir {
	char *name;
	struct dentry *debugfs_entry;
};

struct sprd_debug_dir sprd_debug_dir_table[TYPE_COUNT] = {
	{.name = "sche", .debugfs_entry = NULL},
	{.name = "mem", .debugfs_entry = NULL},
	{.name = "comm", .debugfs_entry = NULL},
	{.name = "timer", .debugfs_entry = NULL},
	{.name = "irq", .debugfs_entry = NULL},
	{.name = "io", .debugfs_entry = NULL},
	{.name = "cpu", .debugfs_entry = NULL},
	{.name = "misc", .debugfs_entry = NULL},
};

struct dentry *sprd_debugfs_entry(enum sprd_debug_type type)
{
	if (type < SCHED || type >= TYPE_COUNT)
		return NULL;
	return sprd_debug_dir_table[type].debugfs_entry;
}


static struct dentry *debugfs_sprd_debug_root;

static void _sprd_dir_init(void)
{
	int i;

	debugfs_sprd_debug_root = debugfs_create_dir("sprd_debug", NULL);
	for (i = 0; i < TYPE_COUNT; i++) {
		struct dentry *ientry;

		ientry = debugfs_create_dir(sprd_debug_dir_table[i].name,
					    debugfs_sprd_debug_root);
		sprd_debug_dir_table[i].debugfs_entry = ientry;
	}
}

static int _sprd_debug_module_init(void)
{
	_sprd_dir_init();
	return 0;
}

arch_initcall(_sprd_debug_module_init);

