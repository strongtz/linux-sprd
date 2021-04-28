#ifndef __SPRD_SYSFRT_H__
#define __SPRD_SYSFRT_H__

enum {
	SYSTEM_TIMER,
	SYSTEM_FRT,
};

#ifdef CONFIG_SPRD_SYSTIMER
extern u64 sprd_systimer_read(void);
extern u64 sprd_sysfrt_read(void);
extern u64 sprd_systimer_to_boottime(u64 counter, int src);
#else
static inline u64 sprd_sysfrt_read(void) { return 0; }
static inline u64 sprd_sysfrt_to_boottime(u64 counter) { return 0; }
static inline u64 sprd_systimer_to_boottime(u64 counter, int src) { return 0; }
#endif

#endif
