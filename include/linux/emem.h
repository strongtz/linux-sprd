/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KERNEL_EMEM__
#define __KERNEL_EMEM__

extern int sysctl_emem_trigger;

int sysctl_emem_trigger_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *length, loff_t *ppos);

#endif
