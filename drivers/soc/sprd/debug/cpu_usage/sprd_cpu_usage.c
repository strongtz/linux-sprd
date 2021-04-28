/*
 * SPRD CPU USAGE TOOL:
 *    1. cpu usage per real-cpu & total
 *    2. cpu usage per thread
 *    3. cpu system info per real-cpu & total:
 *		contextswitch, pagefault, pagemajfault per cpu & total
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/hrtimer.h>
#include <linux/irqnr.h>
#include <linux/tick.h>
#include <linux/threads.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <asm/div64.h>
#include <../../../kernel/sched/sched.h>
#ifdef CONFIG_VM_EVENT_COUNTERS
#include <linux/mm.h>
#include <linux/vmstat.h>
#endif
#include "../sprd_debugfs.h"

/*
 * Macros Definitions:
 * ----------------------------------------------
 * ns_2_ms     : change ns to ms
 * bufferid    : id for thread_cpuinfo.rec[id] and g_rec[id]
 */
#define NR_REC 3
#define bufferid(index) ((index) % (NR_REC))
#define nextid(index) (((index) + 1) % (NR_REC))
#define ns_2_ms(time) (do_div(time, 1000000))
#define unused(x) ((void)(x))

/*
 * CpuInfo Structure Per Thread:
 * ----------------------------------------------
 * 1. Located at each-thread stack area, beside threadinfo struct:
 *  HIGH ADDR ---> Thread Stack End.
 *   |--------------------------
 *   |    ...
 *   |    stack
 *   |    ...
 *   |--------------------------
 *   |    thread_cpuinfo
 *   |--------------------------
 *   |    thread_info
 *   |--------------------------
 *  LOW ADDR ---> Thread Stack Start.
 *
 * 2. Cpu Threadinfo:
 *   index : for update;
 *   start : utime stime start record;
 *   rec[] : ringbuffer --> size according to NR_REC;
 */
struct thread_cpuinfo {
	ulong index;
	struct u_s_time {
		u64 ut;
		u64 st;
	} start, rec[NR_REC];
};

/*
 * Cpu Info Structure:
 * ----------------------------------------------
 *   user    : user usage, percentage;
 *   nice    : nice usage, percentage;
 *   system  : system usage, percentage;
 *   softirq : softirq usage, percentage;
 *   irq     : irq usage, percentage;
 *   idle    : idle usage, percentage;
 *   iowait  : iowait usage, percentage;
 *   steal   : steal usage, percentage;
 *   sum     : sum usage, percentage;
 *   ------------------------------------
 *   nr_cs   : context switch count;
 *   nr_pf   : page fault count;
 *   nr_pmf  : page majfault count;
 */
struct cpu_usage_info {
	u64 user;
	u64 nice;
	u64 system;
	u64 softirq;
	u64 irq;
	u64 idle;
	u64 iowait;
	u64 steal;
	u64 sum;
	u64 nr_cs;
#ifdef CONFIG_VM_EVENT_COUNTERS
	ulong nr_pf;
	ulong nr_pmf;
#endif
};

/*
 * Time Stamp Structure:
 * ----------------------------------------------
 *  1. ns_start : this record start cpu clock
 *     ns_end   : this record end cpu clock
 *  2. ts_start :
 *     ts_end   :
 *  3. tm_start :
 *     tm_end   :
 */
struct time_stamp {
	u64 ns_start;
	u64 ns_end;
	struct timespec ts_start;
	struct timespec ts_end;
	struct rtc_time tm_start;
	struct rtc_time tm_end;
};

/*
 * Recorder Structure:
 * ----------------------------------------------
 * 1. percpu : each cpu cpuinfo
 * 2. total  : total cpu_info
 * 3. ts     : timestamp for this record
 */
struct cpu_recorder {
	struct cpu_usage_info percpu[NR_CPUS];
	struct cpu_usage_info total;
	struct time_stamp ts;
};

/*
 * Global Data:
 * ----------------------------------------------
 * 1. hrtimer     : hrtimer every hrtimer_se
 * 2. global_id   : current id for g_rec and per thread
 * 3. usage_lock  : spinlock for update & print
 * 4. g_rec       : NR_REC records, each hrtimer_se cpu usage
 */
static long hrtimer_se = 10;
static struct hrtimer timer;
static ulong global_id;
static DEFINE_SPINLOCK(usage_lock);
static struct cpu_recorder g_rec[NR_REC];
static struct cpu_usage_info info_saved[NR_CPUS];
static struct timespec ts_saved;
static u64 ns_saved;

/*
 * Functions Start
 * ----------------------------------------------
 * now divider is clock_t or ms, need u64?
 */
static void _ratio_calc(const u64 dividend, const u64 divider, ulong *result)
{
	u64 tmp;

	if (divider == 0) {
		result[0] = result[1] = 0;
		return;
	}

	/*save result as xx.xx% */
	tmp = 10000 * dividend;
	do_div(tmp, divider);

	result[1] = (ulong)(do_div(tmp, 100));
	result[0] = (ulong)tmp;
}

/* collect new info of the cpu base on kernel_stat, rq, and vm_event_states */
static void get_curr_percpu_data(struct cpu_usage_info *new, int cpu)
{
#ifdef CONFIG_VM_EVENT_COUNTERS
	struct vm_event_state *this = &per_cpu(vm_event_states, cpu);

	new->nr_pf = this->event[PGFAULT];
	new->nr_pmf = this->event[PGMAJFAULT];
#endif

	new->nr_cs = cpu_rq(cpu)->nr_switches;

#define CPU_STAT(cpu, stat) (kcpustat_cpu(cpu).cpustat[CPUTIME_##stat])
	new->sum = new->user = CPU_STAT(cpu, USER);
	new->sum += new->system = CPU_STAT(cpu, SYSTEM);
	new->sum += new->nice = CPU_STAT(cpu, NICE);
	new->sum += new->idle = CPU_STAT(cpu, IDLE);
	new->sum += new->iowait = CPU_STAT(cpu, IOWAIT);
	new->sum += new->irq = CPU_STAT(cpu, IRQ);
	new->sum += new->softirq = CPU_STAT(cpu, SOFTIRQ);
	new->sum += new->steal = CPU_STAT(cpu, STEAL);

	/* kernel_cpustat keep nano secord in kernel 4.14
	 * convert to clock_t base on USER_HZ, which always 100
	 */
	new->user = nsec_to_clock_t(new->user);
	new->system = nsec_to_clock_t(new->system);
	new->nice = nsec_to_clock_t(new->nice);
	new->idle = nsec_to_clock_t(new->idle);
	new->iowait = nsec_to_clock_t(new->iowait);
	new->irq = nsec_to_clock_t(new->irq);
	new->softirq = nsec_to_clock_t(new->softirq);
	new->steal = nsec_to_clock_t(new->steal);
	new->sum = nsec_to_clock_t(new->sum);
}

/* calc the delta base on new and saved.
 * and update to the percpu data of global_id.
 */
static void update_cpu_record(struct cpu_recorder *record,
			      struct cpu_usage_info *saved,
			      struct cpu_usage_info *new, int cpu)
{
	struct cpu_usage_info *pcpu = &record->percpu[cpu];
	struct cpu_usage_info *ptotal = &record->total;

	ptotal->user += pcpu->user = new->user - saved->user;
	ptotal->system += pcpu->system = new->system - saved->system;
	ptotal->nice += pcpu->nice = new->nice - saved->nice;
	/*
	 * FIXME begin: idle from system maybe wrong!!!
	 *      in our multi-core system idle may not monotone increasing!!!
	 *      SO: here we walk-around, but need modify in future!!!
	 */
	if (new->idle < saved->idle) {
		saved->sum -= saved->idle;
		saved->idle = (new->idle > 200) ? (new->idle - 200) : 0;
		saved->sum += saved->idle;
	}
	ptotal->idle += pcpu->idle = new->idle - saved->idle;
	ptotal->iowait += pcpu->iowait = new->iowait - saved->iowait;
	ptotal->irq += pcpu->irq = new->irq - saved->irq;
	ptotal->softirq += pcpu->softirq = new->softirq - saved->softirq;
	ptotal->steal += pcpu->steal = new->steal - saved->steal;
	ptotal->sum += pcpu->sum = new->sum - saved->sum;

#ifdef CONFIG_VM_EVENT_COUNTERS
	/*
	 * FIXME begin: pagefault & pagemajfault maybe wrong!!!
	 *       in our multi-core system pagefault & pagemajfault
	 *       may not monotone increasing!!!
	 *       SO: here we walk-around, but need modify in future!!!
	 */
	if (new->nr_pf < saved->nr_pf)
		saved->nr_pf = new->nr_pf;
	ptotal->nr_pf += pcpu->nr_pf = new->nr_pf - saved->nr_pf;

	if (new->nr_pmf < saved->nr_pmf)
		saved->nr_pmf = new->nr_pmf;
	ptotal->nr_pmf += pcpu->nr_pmf = new->nr_pmf - saved->nr_pmf;
#endif

	if (new->nr_cs < saved->nr_cs)
		saved->nr_cs = new->nr_cs;
	ptotal->nr_cs += pcpu->nr_cs = new->nr_cs - saved->nr_cs;
}

static void cpu_threadinfo_clean(struct thread_cpuinfo *buffer)
{
	ulong clr_id = nextid(buffer->index);
	ulong clr_cnt = global_id - buffer->index;

	/*
	 * No records from (buffer->index + 1) to global_id,
	 * then the entry must clear, or previous data will be used.
	 */
	if (clr_cnt >= NR_REC)
		clr_cnt = NR_REC;

	while (clr_cnt--) {
		buffer->rec[clr_id].ut = 0;
		buffer->rec[clr_id].st = 0;

		clr_id = nextid(clr_id);
	}

	/* update index */
	buffer->index = global_id;
}

/* thread buffer. Skip SCHED_STACK_END_CHECK by end add 8 */
#define T_OFFSET (sizeof(struct thread_info) + 8)
#define T_BUFF(task) ((struct thread_cpuinfo *)((task)->stack + T_OFFSET))

static void update_prev(struct task_struct *prev)
{
	struct thread_cpuinfo *buffer = T_BUFF(prev);
	struct u_s_time *pcurr;

	if (buffer->index != global_id)
		cpu_threadinfo_clean(buffer);

	/* task->utime changed to nano secord in kernel 4.14 */
	pcurr = &buffer->rec[bufferid(buffer->index)];
	pcurr->ut += (prev->utime - buffer->start.ut);
	pcurr->st += (prev->stime - buffer->start.st);
}

static void update_next(struct task_struct *next)
{
	struct thread_cpuinfo *buffer = T_BUFF(next);

	buffer->start.ut = next->utime;
	buffer->start.st = next->stime;
}

void sprd_update_cpu_usage(struct task_struct *prev, struct task_struct *next)
{
	ulong flags;

	spin_lock_irqsave(&usage_lock, flags);
	update_prev(prev);
	update_next(next);
	spin_unlock_irqrestore(&usage_lock, flags);
}

/* update: if update ns_saved, ts_saved, info_saved, and global_id
 *         true: when hrtimer expire
 *         false: when cat "cpu_usage"
 *
 * record cpu usage of global_id
 */
static void record_cpu_usage(bool update)
{
	int i, id;
	struct cpu_usage_info info_new;
	struct time_stamp *pts;

	id = bufferid(global_id);
	memset(&g_rec[id].total, 0, sizeof(struct cpu_usage_info));
	pts = &g_rec[id].ts;

	/* record start ns/ts/tm */
	pts->ns_start = ns_saved;
	memcpy(&pts->ts_start, &ts_saved, sizeof(struct timespec));
	rtc_time_to_tm(pts->ts_start.tv_sec, &pts->tm_start);

	/* record end ns/ts/tm */
	pts->ns_end = cpu_clock(0);
	getnstimeofday(&pts->ts_end);
	rtc_time_to_tm(pts->ts_end.tv_sec, &pts->tm_end);

	/* update global_id cpu_usage_info */
	for_each_possible_cpu(i) {
		get_curr_percpu_data(&info_new, i);
		update_cpu_record(&g_rec[id], &info_saved[i], &info_new, i);

		if (update)
			memcpy(&info_saved[i], &info_new, sizeof(info_new));
	}

	if (update) {
		ns_saved = pts->ns_end;
		memcpy(&ts_saved, &pts->ts_end, sizeof(struct timespec));
		global_id++;
	}
}

static void _print_cpu_rate(struct seq_file *p, struct cpu_recorder *record)
{
	ulong idle_ratio[2], user_ratio[2], system_ratio[2], nice_ratio[2];
	ulong iowait_ratio[2], irq_ratio[2], softirq_ratio[2];
	ulong steal_ratio[2], sum_ratio[2];
	int i;
	struct cpu_usage_info *pt;

	for_each_possible_cpu(i) {
		pt = &record->percpu[i];
		/* compute each cpu */
		_ratio_calc(pt->idle, pt->sum, idle_ratio);
		_ratio_calc(pt->user, pt->sum, user_ratio);
		_ratio_calc(pt->system, pt->sum, system_ratio);
		_ratio_calc(pt->nice, pt->sum, nice_ratio);
		_ratio_calc(pt->iowait, pt->sum, iowait_ratio);
		_ratio_calc(pt->irq, pt->sum, irq_ratio);
		_ratio_calc(pt->softirq, pt->sum, softirq_ratio);
		_ratio_calc(pt->steal, pt->sum, steal_ratio);
		_ratio_calc(pt->sum, pt->sum, sum_ratio);
		/* print each cpu */
#ifdef CONFIG_VM_EVENT_COUNTERS
		seq_printf(p,
			   " cpu%d(%d): %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% | %15llu %15lu %15lu\n",
			   i, cpu_online(i), idle_ratio[0], idle_ratio[1],
			   user_ratio[0], user_ratio[1], system_ratio[0],
			   system_ratio[1], nice_ratio[0], nice_ratio[1],
			   iowait_ratio[0], iowait_ratio[1], irq_ratio[0],
			   irq_ratio[1], softirq_ratio[0], softirq_ratio[1],
			   steal_ratio[0], steal_ratio[1], sum_ratio[0],
			   sum_ratio[1], pt->nr_cs, pt->nr_pf, pt->nr_pmf);
#else
		seq_printf(p,
			   " cpu%d(%d): %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% | %15llu\n",
			   i, cpu_online(i), idle_ratio[0], idle_ratio[1],
			   user_ratio[0], user_ratio[1], system_ratio[0],
			   system_ratio[1], nice_ratio[0], nice_ratio[1],
			   iowait_ratio[0], iowait_ratio[1], irq_ratio[0],
			   irq_ratio[1], softirq_ratio[0], softirq_ratio[1],
			   steal_ratio[0], steal_ratio[1], sum_ratio[0],
			   sum_ratio[1], pt->nr_cs);
#endif
	}

	if (num_possible_cpus() > 1) {
		pt = &record->total;
		/* compute total */
		_ratio_calc(pt->idle, pt->sum, idle_ratio);
		_ratio_calc(pt->user, pt->sum, user_ratio);
		_ratio_calc(pt->system, pt->sum, system_ratio);
		_ratio_calc(pt->nice, pt->sum, nice_ratio);
		_ratio_calc(pt->iowait, pt->sum, iowait_ratio);
		_ratio_calc(pt->irq, pt->sum, irq_ratio);
		_ratio_calc(pt->softirq, pt->sum, softirq_ratio);
		_ratio_calc(pt->steal, pt->sum, steal_ratio);
		_ratio_calc(pt->sum, pt->sum, sum_ratio);
		/* print total */
		seq_puts(p, " ------------------\n");
#ifdef CONFIG_VM_EVENT_COUNTERS
		seq_printf(p,
			   " Total:   %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% | %15llu %15lu %15lu\n",
			   idle_ratio[0], idle_ratio[1], user_ratio[0],
			   user_ratio[1], system_ratio[0], system_ratio[1],
			   nice_ratio[0], nice_ratio[1], iowait_ratio[0],
			   iowait_ratio[1], irq_ratio[0], irq_ratio[1],
			   softirq_ratio[0], softirq_ratio[1], steal_ratio[0],
			   steal_ratio[1], sum_ratio[0], sum_ratio[1],
			   pt->nr_cs, pt->nr_pf, pt->nr_pmf);
#else
		seq_printf(p,
			   " Total:   %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% %4lu.%02lu%% | %15llu\n",
			   idle_ratio[0], idle_ratio[1], user_ratio[0],
			   user_ratio[1], system_ratio[0], system_ratio[1],
			   nice_ratio[0], nice_ratio[1], iowait_ratio[0],
			   iowait_ratio[1], irq_ratio[0], irq_ratio[1],
			   softirq_ratio[0], softirq_ratio[1], steal_ratio[0],
			   steal_ratio[1], sum_ratio[0], sum_ratio[1],
			   pt->nr_cs);
#endif
	}
}

static void _print_a_thread_rate(struct seq_file *p, int pid, char *name,
					u64 u_ms, u64 s_ms, u64 total_ms)
{
	ulong u_rto[2] = { 0, 0 };
	ulong s_rto[2] = { 0, 0 };
	ulong t_rto[2] = { 0, 0 };

	/* compute ratio */
	_ratio_calc(u_ms, total_ms, u_rto);
	_ratio_calc(s_ms, total_ms, s_rto);
	_ratio_calc((u_ms + s_ms), total_ms, t_rto);

	if (name != NULL) {
		seq_printf(p,
			   " %-6d  %4lu.%02lu%%  %4lu.%02lu%%  %4lu.%02lu%%    %-15s\n",
			   pid, u_rto[0], u_rto[1], s_rto[0], s_rto[1],
			   t_rto[0], t_rto[1], name);
	} else {
		seq_printf(p,
			   " %-6s  %4lu.%02lu%%  %4lu.%02lu%%  %4lu.%02lu%%\n",
			   "Total:", u_rto[0], u_rto[1], s_rto[0], s_rto[1],
			   t_rto[0], t_rto[1]);
	}
}

static void print_summary(struct seq_file *p, ulong id)
{
	u64 longth_ms;
	u64 start_ms = 0;
	u64 end_ms = 0;

	/* start_ms & end_ms & length */
	start_ms = g_rec[id].ts.ns_start;
	end_ms = g_rec[id].ts.ns_end;
	longth_ms = end_ms - start_ms;

	/* change ns to ms */
	ns_2_ms(start_ms);
	ns_2_ms(end_ms);
	ns_2_ms(longth_ms);

	/* print cpu core count */
	seq_printf(p, "\n\nCpu Core Count: %-6d\n", num_possible_cpus());

	/* from start_ms to end_ms */
	seq_printf(p, "Timer Circle: %-llums.\n", longth_ms);
	seq_printf(p,
		   "  From time %llums(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC) to %llums(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC).\n\n",
		   start_ms,
		   g_rec[id].ts.tm_start.tm_year + 1900,
		   g_rec[id].ts.tm_start.tm_mon + 1,
		   g_rec[id].ts.tm_start.tm_mday,
		   g_rec[id].ts.tm_start.tm_hour,
		   g_rec[id].ts.tm_start.tm_min,
		   g_rec[id].ts.tm_start.tm_sec,
		   g_rec[id].ts.ts_start.tv_nsec, end_ms,
		   g_rec[id].ts.tm_end.tm_year + 1900,
		   g_rec[id].ts.tm_end.tm_mon + 1,
		   g_rec[id].ts.tm_end.tm_mday,
		   g_rec[id].ts.tm_end.tm_hour,
		   g_rec[id].ts.tm_end.tm_min,
		   g_rec[id].ts.tm_end.tm_sec,
		   g_rec[id].ts.ts_end.tv_nsec);
}

static void print_cpu_usage(struct seq_file *p, ulong id)
{
	/* print tile */
	seq_printf(p, "%-87s   %-s\n", " * CPU USAGE:", " | * OTHER COUNTS:");
#ifdef CONFIG_VM_EVENT_COUNTERS
	seq_printf(p,
		   " -%lu-      %8s %8s %8s %8s %8s %8s %8s %8s %8s | %15s %15s %15s\n",
		   id, "IDLE", "USER", "SYSTEM", "NICE", "IOWAIT", "IRQ",
		   "SOFTIRQ", "STEAL", "TOTAL", "CTXT_SWITCH",
		   "FG_FAULT", "FG_MAJ_FAULT");
#else
	seq_printf(p,
		   " -%lu-      %8s %8s %8s %8s %8s %8s %8s %8s %8s | %15s\n",
		   id, "IDLE", "USER", "SYSTEM", "NICE", "IOWAIT", "IRQ",
		   "SOFTIRQ", "STEAL", "TOTAL", "CTXT_SWITCH");
#endif

	/* compute & print */
	_print_cpu_rate(p, &g_rec[id]);
}

static void print_threads_usage(struct seq_file *p, ulong id)
{
	u64 total_usr = 0;
	u64 total_sys = 0;
	u64 u_time, s_time;
	struct task_struct *gp, *pp;
	struct thread_cpuinfo *buffer;
	u64 total_ms = g_rec[id].ts.ns_end - g_rec[id].ts.ns_start;

	ns_2_ms(total_ms);

	/* print tile */
	seq_puts(p, "\n* USAGE PER THREAD:\n");
	seq_printf(p, " %-6s  %8s  %8s  %8s    %-15s\n", "PID", "USER",
			"SYSTEM", "TOTAL", "NAME");

	read_lock(&tasklist_lock);
	do_each_thread(gp, pp) {
		void *stack = try_get_task_stack(pp);

		if (stack == NULL)
			continue;

		buffer = (struct thread_cpuinfo *)(stack + T_OFFSET);
		if (buffer->index != global_id)
			cpu_threadinfo_clean(buffer);

		u_time = buffer->rec[id].ut;
		s_time = buffer->rec[id].st;

		put_task_stack(pp);

		if (0 != (u_time + s_time)) {
			total_usr += u_time;
			total_sys += s_time;

			/* print none-zero thread */
			ns_2_ms(u_time);
			ns_2_ms(s_time);
			_print_a_thread_rate(p, pp->pid, pp->comm,
					u_time, s_time, total_ms);
		}
	} while_each_thread(gp, pp);
	read_unlock(&tasklist_lock);

	/* print total */
	seq_puts(p, " ------------------\n");

	ns_2_ms(total_usr);
	ns_2_ms(total_sys);
	_print_a_thread_rate(p, 0, NULL, total_usr, total_sys, total_ms);
}

static void print_usage(struct seq_file *p)
{
	ulong cnt = NR_REC;
	ulong i;

	record_cpu_usage(false);

	/* print from the earliest time */
	i = nextid(global_id);
	while (cnt--) {
		print_summary(p, i);
		print_cpu_usage(p, i);
		print_threads_usage(p, i);

		i = nextid(i);
	}
}

static enum hrtimer_restart hrtimer_handler(struct hrtimer *timer)
{
	ulong flags;

	spin_lock_irqsave(&usage_lock, flags);
	record_cpu_usage(true);
	spin_unlock_irqrestore(&usage_lock, flags);

	hrtimer_forward_now(timer, ms_to_ktime(hrtimer_se * 1000));
	return HRTIMER_RESTART;
}

static int show_cpu_usage(struct seq_file *p, void *v)
{
	unused(v);
	print_usage(p);

	return 0;
}

static ssize_t cpu_usage_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	char flag[8];
	int ret;

	/* >= 10000 seconds? */
	if (len >= 5)
		return -EFAULT;

	memset(flag, 0, sizeof(flag));
	if (copy_from_user(flag, buf, len))
		return -EFAULT;

	ret = kstrtol(flag, 10, &hrtimer_se);
	if (ret < 0)
		return ret;

	return len;
}

static int cpu_usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_cpu_usage, NULL);
}

const struct file_operations cpu_usage_fops = {
	.open = cpu_usage_open,
	.read = seq_read,
	.write = cpu_usage_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sprd_cpu_usage_init(void)
{
	/* int static */
	memset(g_rec, 0, sizeof(g_rec));
	memset(info_saved, 0, sizeof(info_saved));
	memset(&ts_saved, 0, sizeof(struct timespec));

	/* init timer */
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = hrtimer_handler;
	hrtimer_start(&timer, ms_to_ktime(hrtimer_se * 1000), HRTIMER_MODE_REL);

	/* create debugfs */
	debugfs_create_file("cpu_usage", 0444, sprd_debugfs_entry(CPU),
						NULL, &cpu_usage_fops);

	return 0;
}

static void __exit sprd_cpu_usage_exit(void)
{
	hrtimer_cancel(&timer);
}

subsys_initcall(sprd_cpu_usage_init);
module_exit(sprd_cpu_usage_exit);
