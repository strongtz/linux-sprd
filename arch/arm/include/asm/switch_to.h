/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_ARM_SWITCH_TO_H
#define __ASM_ARM_SWITCH_TO_H

#include <linux/thread_info.h>

/*
 * For v7 SMP cores running a preemptible kernel we may be pre-empted
 * during a TLB maintenance operation, so execute an inner-shareable dsb
 * to ensure that the maintenance completes in case we migrate to another
 * CPU.
 */
#if defined(CONFIG_PREEMPT) && defined(CONFIG_SMP) && defined(CONFIG_CPU_V7)
#define __complete_pending_tlbi()	dsb(ish)
#else
#define __complete_pending_tlbi()
#endif

/*
 * switch_to(prev, next) should switch from task `prev' to `next'
 * `prev' will never be the same as `next'.  schedule() itself
 * contains the memory barrier to tell GCC not to cache `current'.
 */
extern struct task_struct *__switch_to(struct task_struct *, struct thread_info *, struct thread_info *);

#if defined(CONFIG_SPRD_CPU_USAGE) && defined(CONFIG_SPRD_DEBUG)
extern void sprd_update_cpu_usage(struct task_struct *prev,
				  struct task_struct *next);
#else
#define sprd_update_cpu_usage(prev, next)
#endif

#define switch_to(prev,next,last)					\
do {									\
	__complete_pending_tlbi();					\
	sprd_update_cpu_usage(prev, next);				\
	last = __switch_to(prev,task_thread_info(prev), task_thread_info(next));	\
} while (0)

#endif /* __ASM_ARM_SWITCH_TO_H */
