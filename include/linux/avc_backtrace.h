#ifndef _SELINUX_AVC_BACKTRACE_H_
#define _SELINUX_AVC_BACKTRACE_H_

#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/signal.h>

#define AVC_BACKTRACE_SIGNAL (SIGRTMIN + 3)
#define AVC_BACKTRACE_COMM_NUM 5
#define AVC_BACKTRACE_COMM_LEN 32

extern unsigned int avc_backtrace_enable;
extern unsigned int avc_dump_all;
extern char *avc_backtrace_filter_ele[AVC_BACKTRACE_COMM_NUM];

#endif
