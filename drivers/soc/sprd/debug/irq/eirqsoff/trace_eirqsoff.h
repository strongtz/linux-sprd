#ifndef __SPRD_EIRQSOFF_H
#define __SPRD_EIRQSOFF_H

#ifdef CONFIG_SPRD_EIRQSOFF
extern void notrace
start_eirqsoff_timing(unsigned long ip, unsigned long parent_ip);
extern void notrace
stop_eirqsoff_timing(unsigned long ip, unsigned long parent_ip);
#ifdef CONFIG_PREEMPT_TRACER
extern void notrace
start_epreempt_timing(unsigned long ip, unsigned long parent_ip);
extern void notrace
stop_epreempt_timing(unsigned long ip, unsigned long parent_ip);
#else
#define start_epreempt_timing(ip, parent_ip)  \
do { } while (0)
#define stop_epreempt_timing(ip, parent_ip)   \
do { } while (0)
#endif
#else
#define start_eirqsoff_timing(ip, parent_ip)  \
do { } while (0)
#define stop_eirqsoff_timing(ip, parent_ip)   \
do { } while (0)
#define start_epreempt_timing(ip, parent_ip)  \
do { } while (0)
#define stop_epreempt_timing(ip, parent_ip)   \
do { } while (0)
#endif

#endif
