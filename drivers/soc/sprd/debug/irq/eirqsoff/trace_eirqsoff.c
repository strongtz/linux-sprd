#define pr_fmt(fmt) "Warning: sprd_dbg: " fmt
#include <linux/debugfs.h>
#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/uaccess.h>
#include "../../sprd_debugfs.h"

#define DEFAULT_WARNING_INTERVAL (30*NSEC_PER_MSEC)

static int __read_mostly trace_ready;
static DEFINE_PER_CPU(unsigned int, eirqsoff_is_tracing);
static DEFINE_PER_CPU(unsigned long long, eirqsoff_start_timestamp);

static DEFINE_PER_CPU(pid_t, eirqsoff_pid);
static DEFINE_PER_CPU(unsigned long, eirqsoff_ip);
static DEFINE_PER_CPU(unsigned long, eirqsoff_parent_ip);

static unsigned long long __read_mostly warning_interval;

#ifdef CONFIG_PREEMPT_TRACER
static DEFINE_PER_CPU(unsigned int, epreempt_is_tracing);
static DEFINE_PER_CPU(unsigned long long, epreempt_start_timestamp);

static DEFINE_PER_CPU(pid_t, epreempt_pid);
static DEFINE_PER_CPU(unsigned long, epreempt_ip);
static DEFINE_PER_CPU(unsigned long, epreempt_parent_ip);

static unsigned long long __read_mostly epreempt_interval;
#endif

void notrace start_eirqsoff_timing(unsigned long ip, unsigned long parent_ip)
{

	if (!irqs_disabled())
		return;

	if (current->pid == 0)
		return;

	if (__this_cpu_read(eirqsoff_is_tracing))
		return;

	if (__this_cpu_read(eirqsoff_start_timestamp))
		return;

	if (oops_in_progress)
		return;

	__this_cpu_write(eirqsoff_pid, current->pid);
	__this_cpu_write(eirqsoff_ip, ip);
	__this_cpu_write(eirqsoff_parent_ip, parent_ip);
	__this_cpu_write(eirqsoff_start_timestamp, sched_clock());

	__this_cpu_write(eirqsoff_is_tracing, 1);

}

void notrace stop_eirqsoff_timing(unsigned long ip, unsigned long parent_ip)
{
	unsigned long long stop_timestamp;
	unsigned long long start_timestamp;
	unsigned long long start_timestamp_ms;
	unsigned long long interval;
	unsigned long long interval_us;

	if (!irqs_disabled())
		return;

	if (unlikely(!trace_ready))
		return;

	if (!__this_cpu_read(eirqsoff_is_tracing))
		return;

	__this_cpu_write(eirqsoff_is_tracing, 0);

	if (!oops_in_progress) {

		stop_timestamp = sched_clock();
		start_timestamp = __this_cpu_read(eirqsoff_start_timestamp);

		interval = stop_timestamp - start_timestamp;

		if (interval > warning_interval) {

			start_timestamp_ms = do_div(start_timestamp, NSEC_PER_SEC);
			interval_us = do_div(interval, NSEC_PER_MSEC);

			pr_warn("irqsoff: Process %d detected Process %d disable interrupt "
				"%lld.%06lldms from %lld.%09llds\n",
				current->pid,
				__this_cpu_read(eirqsoff_pid),
				interval,
				interval_us,
				start_timestamp,
				start_timestamp_ms);
			pr_warn("disable at:\n");
			print_ip_sym(__this_cpu_read(eirqsoff_parent_ip));
			print_ip_sym(__this_cpu_read(eirqsoff_ip));
			dump_stack();
		}
	}
	__this_cpu_write(eirqsoff_start_timestamp, 0);
}


#ifdef CONFIG_PREEMPT_TRACER
void notrace start_epreempt_timing(unsigned long ip, unsigned long parent_ip)
{

	if (current->pid == 0)
		return;

	if (__this_cpu_read(eirqsoff_is_tracing))
		return;

	if (!preempt_count())
		return;

	if (__this_cpu_read(epreempt_is_tracing))
		return;

	if (__this_cpu_read(epreempt_start_timestamp))
		return;

	if (oops_in_progress)
		return;

	__this_cpu_write(epreempt_pid, current->pid);
	__this_cpu_write(epreempt_ip, ip);
	__this_cpu_write(epreempt_parent_ip, parent_ip);
	__this_cpu_write(epreempt_start_timestamp, sched_clock());

	__this_cpu_write(epreempt_is_tracing, 1);

}


void notrace stop_epreempt_timing(unsigned long ip, unsigned long parent_ip)
{

	unsigned long long stop_timestamp;
	unsigned long long start_timestamp;
	unsigned long long start_timestamp_ms;
	unsigned long long interval;
	unsigned long long interval_us;

	if (unlikely(!trace_ready))
		return;

	if (!__this_cpu_read(epreempt_is_tracing))
		return;

	if (!preempt_count())
		return;

	__this_cpu_write(epreempt_is_tracing, 0);

	if (!oops_in_progress) {
		stop_timestamp = sched_clock();
		start_timestamp = __this_cpu_read(epreempt_start_timestamp);

		interval = stop_timestamp - start_timestamp;

		if (interval > epreempt_interval) {

			start_timestamp_ms = do_div(start_timestamp, NSEC_PER_SEC);
			interval_us = do_div(interval, NSEC_PER_MSEC);

			pr_warn("irqsoff: Process %d detected Process %d disable preempt "
				"%lld.%06lldms from %lld.%09llds\n",
				current->pid,
				 __this_cpu_read(epreempt_pid),
				interval,
				interval_us,
				start_timestamp,
				start_timestamp_ms);
			pr_warn("disable at:\n");
			print_ip_sym(__this_cpu_read(epreempt_parent_ip));
			print_ip_sym(__this_cpu_read(epreempt_ip));
			dump_stack();
		}
	}
	__this_cpu_write(epreempt_start_timestamp, 0);

}

#endif

static int notrace eirqsoff_interval_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lld\n", warning_interval);
	return 0;
}

static int notrace eirqsoff_interval_open(struct inode *inodep,
					  struct file *filep)
{
	single_open(filep, eirqsoff_interval_show, NULL);
	return 0;
}

static ssize_t notrace eirqsoff_interval_write(struct file *filep,
					       const char __user *buf,
					       size_t len,
					       loff_t *ppos)
{
	unsigned long long interval;
	int err;

	if (len <= 5 || len >= 11)
		return -EINVAL;

	err = kstrtoull_from_user(buf, len, 0, &interval);

	if (err)
		return -EINVAL;

	warning_interval = interval;
#ifdef CONFIG_PREEMPT_TRACER
	epreempt_interval = interval<<2;
#endif

	return len;
}

const struct file_operations eirqsoff_interval_fops = {
	.open    = eirqsoff_interval_open,
	.read    = seq_read,
	.write   = eirqsoff_interval_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init trace_eirqsoff_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		per_cpu(eirqsoff_start_timestamp, cpu) = 0;
		per_cpu(eirqsoff_is_tracing, cpu) = 0;
#ifdef CONFIG_PREEMPT_TRACER
		per_cpu(epreempt_start_timestamp, cpu) = 0;
		per_cpu(epreempt_is_tracing, cpu) = 0;
#endif
	}
	warning_interval = DEFAULT_WARNING_INTERVAL;
#ifdef CONFIG_PREEMPT_TRACER
	epreempt_interval = DEFAULT_WARNING_INTERVAL<<2;
#endif
	trace_ready = 1;

	debugfs_create_file("warning_interval",
			    0644,
			    sprd_debugfs_entry(IRQ),
			    NULL,
			    &eirqsoff_interval_fops);
	return 0;
}

fs_initcall(trace_eirqsoff_init);
