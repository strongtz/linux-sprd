#ifndef _REGS_DEBUG_
#define _REGS_DEBUG_

#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/jiffies.h>
#include <linux/smp.h>

struct sprd_debug_regs_access {
	unsigned long vaddr;
	unsigned long stack;
	unsigned long pc;
	unsigned long time;
	unsigned int status;
	u32 value;
};
#ifdef CONFIG_ARM64
#define sprd_debug_regs_read_start(a) ({	\
	unsigned long cpu_id, stack, lr;	\
	asm volatile(				\
		"	mov %1, sp\n"		\
		"	ldr %0, [x29, #8]\n"	\
		: "=&r" (lr),			\
		  "=&r" (stack)			\
		:				\
		: "memory", "cc");		\
	if (sprd_debug_last_regs_access) {	\
		cpu_id = raw_smp_processor_id();			\
		sprd_debug_last_regs_access[cpu_id].value = 0;		\
		sprd_debug_last_regs_access[cpu_id].vaddr = (unsigned long)a; \
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	barrier();							\
	})

#define sprd_debug_regs_write_start(v, a)	({	\
	unsigned long cpu_id, stack, lr;		\
	asm volatile(					\
		"	mov %1, sp\n"			\
		"	ldr %0, [x29, #8]\n"		\
		: "=&r" (lr),				\
		  "=&r" (stack)				\
		:					\
		: "memory", "cc");			\
	if (sprd_debug_last_regs_access) {		\
		cpu_id = raw_smp_processor_id();	\
		sprd_debug_last_regs_access[cpu_id].value = (unsigned long)(v);\
		sprd_debug_last_regs_access[cpu_id].vaddr = (unsigned long)(a);\
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	barrier();							\
	})

#define sprd_debug_regs_access_done()	({			\
	unsigned long cpu_id, lr;				\
	asm volatile(						\
		"	ldr %0, [x29, #8]\n"			\
		: "=&r" (lr)					\
		:						\
		: "memory", "cc");				\
	if (sprd_debug_last_regs_access) {			\
		cpu_id = raw_smp_processor_id();			\
		sprd_debug_last_regs_access[cpu_id].time = jiffies;     \
		sprd_debug_last_regs_access[cpu_id].status = 1; }	\
	barrier();							\
	})

#else
#ifdef CONFIG_ARM
#define sprd_debug_regs_read_start(a)	({u32 cpu_id, stack, lr;	\
	asm volatile(						\
		"	mov %1, r13\n"				\
		"	mov %0, pc\n"				\
		: "=&r" (lr), "=&r" (stack)			\
		:						\
		: "memory", "cc");				\
	if (sprd_debug_last_regs_access) {			\
		cpu_id = raw_smp_processor_id();		\
		sprd_debug_last_regs_access[cpu_id].value = 0;		\
		sprd_debug_last_regs_access[cpu_id].vaddr = (u32)a;	\
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	barrier();							\
	})

#define sprd_debug_regs_write_start(v, a)	({		\
	u32 cpu_id, stack, lr;					\
	asm volatile(						\
		"	mov %1, r13\n"				\
		"	mov %0, pc\n"				\
		: "=&r" (lr), "=&r" (stack)			\
		:						\
		: "memory", "cc");				\
	if (sprd_debug_last_regs_access) {			\
		cpu_id = raw_smp_processor_id();                \
		sprd_debug_last_regs_access[cpu_id].value = (u32)(v);	\
		sprd_debug_last_regs_access[cpu_id].vaddr = (u32)(a);	\
		sprd_debug_last_regs_access[cpu_id].stack = stack;	\
		sprd_debug_last_regs_access[cpu_id].pc = lr;		\
		sprd_debug_last_regs_access[cpu_id].status = 0; }	\
	barrier();							\
	})

#define sprd_debug_regs_access_done()	({u32 cpu_id, lr;	\
	asm volatile(						\
		"	mov %0, pc\n"				\
		: "=&r" (lr)					\
		:						\
		: "memory", "cc");				\
	if (sprd_debug_last_regs_access) {			\
		cpu_id = raw_smp_processor_id();		\
		sprd_debug_last_regs_access[cpu_id].time = jiffies;     \
		sprd_debug_last_regs_access[cpu_id].status = 1; }	\
	barrier();							\
	})

#else
#define sprd_debug_regs_read_start(a)
#define sprd_debug_regs_write_start(v, a)
#define sprd_debug_regs_access_done()
#endif
#endif
#endif
