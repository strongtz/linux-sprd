#ifndef __SPRD_PLATFORM_SYSDUMP_H
#define __SPRD_PLATFORM_SYSDUMP_H

#ifndef CONFIG_X86_64
#define SPRD_SYSDUMP_MAGIC      0x85500000
#else
#define SPRD_SYSDUMP_MAGIC      0x3B800000
#endif

struct sysdump_mem {
	unsigned long paddr;
	unsigned long vaddr;
	unsigned long soff;
	unsigned long size;
	unsigned long type;
};

enum sysdump_type {
	SYSDUMP_RAM,
	SYSDUMP_MODEM,
	SYSDUMP_IOMEM,
};

#ifdef CONFIG_ARM
#include "sysdump32.h"
#endif

#ifdef CONFIG_ARM64
#include "sysdump64.h"
#endif

#ifdef CONFIG_X86_64
#include "sysdump_x86_64.h"
#endif

#endif
