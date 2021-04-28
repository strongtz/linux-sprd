/*
 * Copyright (C) 2016-2017 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/tick.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/task.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

/* On-demand governor macros */
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)

#define HOTPLUG_BOOT_DELAY			(50 * HZ)

#define DEF_CPU_UP_MID_THRESHOLD		(80)
#define DEF_CPU_UP_HIGH_THRESHOLD		(90)
#define DEF_CPU_DOWN_MID_THRESHOLD		(30)
#define DEF_CPU_DOWN_HIGH_THRESHOLD		(40)

#define MAX_CPU_NUM  (4)
#define MAX_PERCPU_TOTAL_LOAD_WINDOW_SIZE  (8)
#define MAX_PLUG_AVG_LOAD_SIZE (2)
#define CLU0_CPU_NUM_MIN	(1)
#define CLU1_CPU_NUM_MIN	(0)
#define THREAD_BIND_CPU		(0)

/* default boost pulse time, units: ms */
#define DEF_BOOSTPULSE_DURATION		(500)
/* default dynamic hotplug time, units: ms */
#define DEF_CHECK_LOAD_DURATION		(40)

#define mod(n, div) ((n) % (div))

#define define_sprd_global(_name, flag)		\
static struct kobj_attribute _name =		\
__ATTR(_name, flag, show_##_name, store_##_name)

#define define_sprd_global_wo(_name, flag)		\
static struct kobj_attribute _name =		\
__ATTR(_name, flag, NULL, store_##_name)

#define for_each_cluster(clu, bitmap)					\
	for ((clu) = -1;						\
		(clu) = find_next_bit(bitmap, CLUSTER_ALL, clu+1),	\
		(clu) < CLUSTER_ALL;)

enum core_request {
	REQ_DOWN = 0,
	REQ_UP,
	REQ_ALL,
};

enum mode_type {
	NORMAL_MODE = 0,
	THREAD_MODE,
	MODE_ALL,
};

enum cluster_type {
	CLUSTER0 = 0,	/* for 2/4/8 core */
	CLUSTER1,	/* for 8 core */
	CLUSTER_ALL,
};

enum core_min {
	HOTPLUG_POWER_HINT = 0,	/* for one user at HAL */
	HOTPLUG_TEST_MIN,	/* for debug */
	HOTPLUG_FAKE_MIN,	/* for none dynamic load */
	CORE_MIN_ALL,
};

enum core_max {
	HOTPLUG_TEST_MAX = 0,	/* for debug */
	CORE_MAX_ALL,
};

struct sd_dbs_tuners {
	atomic_t request[REQ_ALL];
	unsigned int ignore_nice;
	unsigned int sampling_down_factor;
	unsigned int io_is_busy;

	unsigned int cpu_hotplug_disable;
	unsigned int qos_core_ctl;
	unsigned int hotplug_mode;
	unsigned int is_suspend;
	unsigned int cpu_num_max_limit[CLUSTER_ALL];	/* for thermal */
	unsigned int cpu_num_min_limit[CLUSTER_ALL];	/* for touch boost */
	int cluster_cpu_ids[CLUSTER_ALL];	/* cpu num for each cluster*/
	unsigned int cluster_num;
	unsigned int cpu_up_mid_threshold;
	unsigned int cpu_up_high_threshold;
	unsigned int cpu_down_mid_threshold;
	unsigned int cpu_down_high_threshold;
	unsigned int up_window_size;
	unsigned int down_window_size;
	unsigned long boostpulse_duration;
	unsigned long check_load_duration;
	struct timer_list dynamic_load_timer;
	struct pm_qos_request max_cpu_request[CLUSTER_ALL][CORE_MAX_ALL];
	struct pm_qos_request min_cpu_request[CLUSTER_ALL][CORE_MIN_ALL];
	struct cpumask cluster_mask[CLUSTER_ALL];
	bool boost_cluster[CLUSTER_ALL];
	bool passion_mode;
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_entry;
};

static struct kobject hotplug_kobj;
static struct sd_dbs_tuners *g_sd_tuners;
static unsigned long boot_done;

/* realtime thread handles cores scaling */
static struct task_struct *corechange_task;
static spinlock_t corechange_lock;
static unsigned long corechange_flag;

/*
 * This lock is used to ensure the synchronization
 * between cpu_num_max_limit and cpu_num_min_limit.
 */
static struct mutex cpu_num_lock[CLUSTER_ALL];

static u64 g_prev_cpu_wall[CONFIG_NR_CPUS] = {0};
static u64 g_prev_cpu_idle[CONFIG_NR_CPUS] = {0};

static int hotplug_ops(int cpu, enum core_request up)
{
	struct device *cpu_dev;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("hotplug: cpu[%d] device, type: %s\n",
			cpu, (up ? "up" : "down"));
		return -EINVAL;
	}

	if (up)
		return device_online(cpu_dev);
	else
		return device_offline(cpu_dev);
}

/*
 * get online cpus for each cluster
 */
static int clu_online_cpus(enum cluster_type cluster)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	struct cpumask tmp_mask;

	cpumask_clear(&tmp_mask);
	if (!sd_tuners) {
		pr_err("hotplug: g_sd_tuners not init!\n");
		return -EINVAL;
	}
	cpumask_and(&tmp_mask, &sd_tuners->cluster_mask[cluster],
		cpu_online_mask);
	return cpumask_weight(&tmp_mask);
}

static int down_proper_cpu(enum cluster_type cluster)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	struct cpumask tmp_mask;
	int cpu = 0;

	cpumask_clear(&tmp_mask);
	if (!sd_tuners) {
		pr_err("hotplug: down g_sd_tuners not init!\n");
		return -EINVAL;
	}
	cpumask_and(&tmp_mask, &sd_tuners->cluster_mask[cluster],
		cpu_online_mask);

	if (sd_tuners->hotplug_mode && clu_online_cpus(cluster) == 3) {
		cpu = cluster ? sd_tuners->cluster_cpu_ids[0] + 1 : 1;
		if (cpu_online(cpu))
			return cpu;
	}

	return find_last_bit(cpumask_bits(&tmp_mask), nr_cpu_ids);
}

static int up_proper_cpu(enum cluster_type cluster)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	struct cpumask tmp_mask;
	int cpu = 0;

	cpumask_clear(&tmp_mask);
	if (!sd_tuners) {
		pr_err("hotplug: up g_sd_tuners not init!\n");
		return -EINVAL;
	}
	cpumask_and(&tmp_mask, &sd_tuners->cluster_mask[cluster],
		cpu_online_mask);

	if (sd_tuners->hotplug_mode && clu_online_cpus(cluster) == 1
		&& sd_tuners->cluster_cpu_ids[cluster] > 2) {
		cpu = cluster ? sd_tuners->cluster_cpu_ids[0] + 2 : 2;
		if (!cpu_online(cpu))
			return cpu;
	}

	return find_next_zero_bit(cpumask_bits(&tmp_mask), nr_cpu_ids,
			cluster ? sd_tuners->cluster_cpu_ids[cluster - 1] : 0);
}

static int get_cluster_cpu_ids(struct sd_dbs_tuners *tuners)
{
	int i = 0, j = 0, tmp_id = 0, core_nums = 0;

#if defined(CONFIG_X86)
	tuners->cluster_num = 2;
	for (i = 0; i < tuners->cluster_num; i++) {
		tuners->cluster_cpu_ids[i] = 4;
		for (j = 0; j < tuners->cluster_cpu_ids[i]; j++)
			cpumask_set_cpu(tmp_id++, &tuners->cluster_mask[i]);
		pr_info("hotplug: cluster[%d] cpus: %d\n", i,
					tuners->cluster_cpu_ids[i]);
	}
#else
	if (num_online_cpus() != nr_cpu_ids) {
		pr_err("hotplug: get cpu number may be error!\n");
		return -EINVAL;
	}
	do {
		tmp_id = cpumask_weight(topology_core_cpumask(i));
		if (tmp_id <= 0) {
			pr_err("hotplug: topology core cpumask error!\n");
			return -EINVAL;
		}
		cpumask_copy(&tuners->cluster_mask[j],
			     topology_core_cpumask(i));
		tuners->cluster_cpu_ids[j++] = tmp_id;
		pr_info("hotplug: cluster[%d] cpus: %d\n", j - 1, tmp_id);
		i += tmp_id;
	} while (i < nr_cpu_ids);
	tuners->cluster_num = j;
#endif
	/*
	 * To check summery of cores.
	 */
	for (i = 0; i < tuners->cluster_num; i++)
		core_nums += tuners->cluster_cpu_ids[i];

	if (core_nums != nr_cpu_ids) {
		pr_err("hotplug: get cpu number may be error!\n");
		return -EINVAL;
	}
	return 0;
}

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_nsecs(get_jiffies_64());

	busy_time = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = div_u64(cur_wall_time, NSEC_PER_USEC);

	return div_u64(idle_time, NSEC_PER_USEC);
}

static inline u64
get_cpu_idle_time_sprd(unsigned int cpu, u64 *wall, int io_busy)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, io_busy ? wall : NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else if (!io_busy)
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static int ops_keep_proper_cores(struct sd_dbs_tuners *sd_tuners,
			enum cluster_type cluster)
{
	int cpu, max_num, min_num, i;
	struct cpumask tmp_mask;
	int ret = 0, count = 0, recheck = 0;

reops:
	if (sd_tuners->is_suspend)
		return 0;

	cpumask_clear(&tmp_mask);
	mutex_lock(&cpu_num_lock[cluster]);
	max_num = sd_tuners->cpu_num_max_limit[cluster];
	if (!sd_tuners->cpu_hotplug_disable)
		min_num = sd_tuners->cpu_num_min_limit[cluster];
	else
		min_num = sd_tuners->cpu_num_max_limit[cluster];
	mutex_unlock(&cpu_num_lock[cluster]);

	/*
	 * To plugin cpu if online cpus number is
	 * less than cpu_num_min_limit. notice: if
	 * hotplug is disabled, it will hold
	 * cpu_num_max_limit.
	 */
	if (clu_online_cpus(cluster) < min_num) {
		for (i = 0; i < sd_tuners->cluster_cpu_ids[cluster]; i++) {
			cpu = up_proper_cpu(cluster);
			if (cpu <= 0 || cpu >= nr_cpu_ids
			  || !test_bit(cpu,
			  cpumask_bits(&sd_tuners->cluster_mask[cluster])))
				break;
			pr_info("!! %s:all gonna plugin cpu%d !!\n",
				cluster ? "big" : "lit", cpu);
			ret = hotplug_ops(cpu, REQ_UP);

			/* In order to avoid repeatedly plugin core */
			atomic_set(&sd_tuners->request[REQ_UP], 0);
			if (clu_online_cpus(cluster) >= min_num
			    || ret < 0)
				break;
		}
	}

	/*
	 * To unplug cpu from the back if online cpus number is
	 * more than cpu_num_max_limit.
	 */
	if (clu_online_cpus(cluster) > max_num) {
		for (i = 0; i < sd_tuners->cluster_cpu_ids[cluster]; i++) {
			cpu = down_proper_cpu(cluster);
			if (cpu <= 0 || cpu >= nr_cpu_ids
			  || !test_bit(cpu,
			  cpumask_bits(&sd_tuners->cluster_mask[cluster])))
				break;
			pr_info("!! %s:all gonna unplug cpu%d !!\n",
				cluster ? "big" : "lit", cpu);
			ret = hotplug_ops(cpu, REQ_DOWN);

			/* In order to avoid repeatedly unplug core */
			atomic_set(&sd_tuners->request[REQ_DOWN], 0);
			if (clu_online_cpus(cluster) <= max_num
			    || ret < 0)
				break;
		}
	}

	if (sd_tuners->cpu_hotplug_disable)
		goto ops_end;

	/*
	 * According to dynamic load, we will plug-in
	 * or plug-out one core.
	 */
	if (atomic_read(&sd_tuners->request[REQ_UP])
		&& clu_online_cpus(cluster) < max_num) {
		cpu = cpumask_next_zero(0, cpu_online_mask);
		if (cpu < sd_tuners->cpu_num_max_limit[cluster]) {
			pr_info("!! %s:we gonna plugin cpu%d !!\n",
				cluster ? "big" : "lit", cpu);
			ret = hotplug_ops(cpu, REQ_UP);
			clear_bit(cluster, &corechange_flag);
		}
	}
	atomic_set(&sd_tuners->request[REQ_UP], 0);

	if (atomic_read(&sd_tuners->request[REQ_DOWN])
		&& clu_online_cpus(cluster) > min_num) {
		cpu = down_proper_cpu(cluster);
		if (cpu < sd_tuners->cluster_cpu_ids[cluster]
			&& cpu > CLU0_CPU_NUM_MIN - 1) {
			pr_info("!! %s:we gonna unplug cpu%d !!\n",
				cluster ? "big" : "lit", cpu);
			ret = hotplug_ops(cpu, REQ_DOWN);
			clear_bit(cluster, &corechange_flag);
		}
	}
	atomic_set(&sd_tuners->request[REQ_DOWN], 0);

ops_end:
	if (ret == -EBUSY) {
		pr_warn("hotplug ops EBUSY: %d\n", ret);
		return ret;
	} else if (ret < 0) {
		if (++count > 2) {
			pr_err("hotplug ops failed: %d\n", ret);
			return ret;
		}
		pr_err("hotplug ops err: %d\n", ret);
	} else {
		if (ret == 1) {
			pr_err("dev->offline error, ret=%d\n", ret);
			return ret;
		}
		count = 0;
	}
	/*
	 * To make sure that user needs can be processed in time,
	 * Here we use global variable in real time without lock.
	 */
	if (clu_online_cpus(cluster) < sd_tuners->cpu_num_min_limit[cluster]
	  || clu_online_cpus(cluster) > sd_tuners->cpu_num_max_limit[cluster]) {
		if (recheck++ < 10)
			goto reops;
		else
			pr_warn("hotplug continuous-reops too many times!\n");
	}
	return 0;
}

/*
 * To strictly keep the online cpus between cpu_num_min_limit
 * and cpu_num_max_limit. Also to up/down cpu while accepting
 * the request from dynamic-load timer function.
 */
static int sprd_corechange_task(void *data)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned long tmp_flag, flags;
	int clu;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&corechange_lock, flags);

		if (bitmap_empty(&corechange_flag, sd_tuners->cluster_num)) {
			spin_unlock_irqrestore(&corechange_lock,
					       flags);
			schedule();

			if (kthread_should_stop())
				break;

			spin_lock_irqsave(&corechange_lock, flags);
		}

		set_current_state(TASK_RUNNING);
		tmp_flag = corechange_flag;
		bitmap_zero(&corechange_flag, CLUSTER_ALL);
		spin_unlock_irqrestore(&corechange_lock, flags);
		for_each_cluster(clu, &tmp_flag) {
			ops_keep_proper_cores(sd_tuners, clu);
		}
	}

	return 0;
}

#define MAX_ARRAY_SIZE  (10)
#define UP_LOAD_WINDOW_SIZE  (1)
#define DOWN_LOAD_WINDOW_SIZE  (2)
static unsigned int load_array[CONFIG_NR_CPUS][MAX_ARRAY_SIZE] = { {0} };
static unsigned int window_index[CONFIG_NR_CPUS] = {0};

static unsigned int sd_avg_load(int cpu, struct sd_dbs_tuners *sd_tuners,
			unsigned int load, bool up)
{
	unsigned int window_size;
	unsigned int count;
	unsigned int scale;
	unsigned int sum_scale = 0;
	unsigned int sum_load = 0;
	unsigned int window_tail = 0, window_head = 0;

	if (up) {
		window_size = sd_tuners->up_window_size;
	} else {
		window_size = sd_tuners->down_window_size;
		goto skip_load;
	}

	load_array[cpu][window_index[cpu]] = load;
	window_index[cpu]++;
	window_index[cpu] = mod(window_index[cpu], MAX_ARRAY_SIZE);

skip_load:
	if (!window_index[cpu])
		window_tail = MAX_ARRAY_SIZE - 1;
	else
		window_tail = window_index[cpu] - 1;

	window_head = mod(MAX_ARRAY_SIZE + window_tail - window_size + 1,
			MAX_ARRAY_SIZE);
	for (scale = 1, count = 0; count < window_size;
			scale += scale, count++) {
		pr_debug("%s load_array[%d][%d]: %d, scale: %d\n",
				up ? "up" : "down", cpu, window_head,
				load_array[cpu][window_head], scale);
		sum_load += (load_array[cpu][window_head] * scale);
		/*
		 * here skip load == 0 at the first time we use load_array.
		 */
		if (load_array[cpu][window_head] != 0)
			sum_scale += scale;
		window_head++;
		window_head = mod(window_head, MAX_ARRAY_SIZE);
	}

	return sum_scale ? sum_load / sum_scale : 0;
}

static void sd_check_cpu_sprd(unsigned int load)
{
	unsigned int itself_avg_load = 0;
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned long flags;

	if (time_before(jiffies, boot_done))
		return;

	pr_debug("efficient load %d, ---- online CPUs %d ----\n",
			load, num_online_cpus());

	/* cpu plugin check */
	itself_avg_load = sd_avg_load(0, sd_tuners, load, true);
	pr_debug("up itself_avg_load %d\n", itself_avg_load);

	if (num_online_cpus() < sd_tuners->cpu_num_max_limit[CLUSTER0]) {
		int cpu_up_threshold;

		if (num_online_cpus() == 1)
			cpu_up_threshold = sd_tuners->cpu_up_mid_threshold;
		else
			cpu_up_threshold = sd_tuners->cpu_up_high_threshold;

		if (itself_avg_load > cpu_up_threshold) {
			atomic_set(&sd_tuners->request[REQ_UP], 1);
			spin_lock_irqsave(&corechange_lock, flags);
			set_bit(CLUSTER0, &corechange_flag);
			spin_unlock_irqrestore(&corechange_lock, flags);
			wake_up_process(corechange_task);
			return;
		}
	}

	/* cpu unplug check */
	if (num_online_cpus() > sd_tuners->cpu_num_min_limit[CLUSTER0]) {
		int cpu_down_threshold;

		itself_avg_load = sd_avg_load(0, sd_tuners, load, false);
		pr_debug("down itself_avg_load %d\n", itself_avg_load);

		if (num_online_cpus()
		    > (sd_tuners->cpu_num_min_limit[CLUSTER0] + 1))
			cpu_down_threshold = sd_tuners->cpu_down_high_threshold;
		else
			cpu_down_threshold = sd_tuners->cpu_down_mid_threshold;

		if (itself_avg_load < cpu_down_threshold) {
			atomic_set(&sd_tuners->request[REQ_DOWN], 1);
			spin_lock_irqsave(&corechange_lock, flags);
			set_bit(CLUSTER0, &corechange_flag);
			spin_unlock_irqrestore(&corechange_lock, flags);
			wake_up_process(corechange_task);
		}
	}
}

static void dbs_check_cpu_sprd(unsigned long data)
{
	unsigned int max_load = 0, max_freq = 0, tmp_freq = 0;
	unsigned int j;
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned long expires;

	if (sd_tuners->cpu_hotplug_disable)
		return;

	if (time_before(jiffies, boot_done) || sd_tuners->is_suspend)
		goto retry;

	/*
	 * To get the max Load between all cpus.
	 */
	for_each_cpu(j, cpu_online_mask) {
		u64 cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;
		unsigned int load;
		int io_busy = 0;
		u64 prev_cpu_wall;
		u64 prev_cpu_idle;

		prev_cpu_wall = g_prev_cpu_wall[j];
		prev_cpu_idle = g_prev_cpu_idle[j];

		/*
		 * We won't add the iowait time to the cpu idle time,
		 * because disk IO isn't the actually idle for system.
		 */
		io_busy = g_sd_tuners->io_is_busy;
		cur_idle_time = get_cpu_idle_time_sprd(j, &cur_wall_time,
					io_busy);

		wall_time = (unsigned int)
			(cur_wall_time - prev_cpu_wall);

		idle_time = (unsigned int)
			(cur_idle_time - prev_cpu_idle);

		g_prev_cpu_wall[j] = cur_wall_time;
		g_prev_cpu_idle[j] = cur_idle_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;
		if (load > max_load)
			max_load = load;
	}
	/*
	 * calculate max_load according to current frequency.(but not
	 * very precise)
	 * now dynamic hotplug is only used for sprd 4 core ARM platform.
	 * if using for other platforms, the implementation should be modified
	 * for different clusters.
	 */
	if (!sd_tuners->passion_mode) {
		max_freq = cpufreq_quick_get_max(0);
		tmp_freq = cpufreq_quick_get(0);
		if (max_freq && tmp_freq)
			max_load = max_load * tmp_freq / max_freq;
	}
	sd_check_cpu_sprd(max_load);

retry:
	if (!timer_pending(&sd_tuners->dynamic_load_timer)) {
		expires = jiffies +
			msecs_to_jiffies(sd_tuners->check_load_duration);
		mod_timer(&sd_tuners->dynamic_load_timer, expires);
	}
}

static int should_io_be_busy(void)
{
	return 1;
}

/*
 * If PM_QOS_CLUSTER0_CORE_MIN and PM_QOS_CLUSTER0_CORE_MAX request is updated,
 * cluster0_hotplug_qos_handler is called.
 */
static int cluster0_hotplug_qos_handler(struct notifier_block *b,
		unsigned long val, void *v)
{
	int online_cpu_min, online_cpu_max;
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned long flags;

	if (!sd_tuners || !sd_tuners->qos_core_ctl)
		return 0;

	online_cpu_min = min(pm_qos_request(PM_QOS_CLUSTER0_CORE_MIN),
				sd_tuners->cluster_cpu_ids[CLUSTER0]);
	online_cpu_min = max(online_cpu_min, CLU0_CPU_NUM_MIN);
	online_cpu_max = min(pm_qos_request(PM_QOS_CLUSTER0_CORE_MAX),
				sd_tuners->cluster_cpu_ids[CLUSTER0]);
	online_cpu_max = max(online_cpu_max, CLU0_CPU_NUM_MIN);
	if (online_cpu_min > online_cpu_max)
		online_cpu_min = online_cpu_max;

	mutex_lock(&cpu_num_lock[CLUSTER0]);
	sd_tuners->cpu_num_max_limit[CLUSTER0] = online_cpu_max;
	sd_tuners->cpu_num_min_limit[CLUSTER0] = online_cpu_min;

	/*
	 * make sure num of cores within min and max
	 * for PM_QOS_CLUSTER0_CORE_MIN and PM_QOS_CLUSTER0_CORE_MAX class
	 */
	if (clu_online_cpus(CLUSTER0) > online_cpu_max
		|| clu_online_cpus(CLUSTER0) < online_cpu_min) {
		spin_lock_irqsave(&corechange_lock, flags);
		set_bit(CLUSTER0, &corechange_flag);
		spin_unlock_irqrestore(&corechange_lock, flags);
		wake_up_process(corechange_task);
	}
	mutex_unlock(&cpu_num_lock[CLUSTER0]);
	pr_info("update cluster0 cpu num: %d - %d\n", online_cpu_min,
			online_cpu_max);

	return 0;
}

static struct notifier_block cluster0_qos_notifier = {
	.notifier_call = cluster0_hotplug_qos_handler,
};

/*
 * If PM_QOS_CLUSTER1_CORE_MIN and PM_QOS_CLUSTER1_CORE_MAX request is updated,
 * cluster1_hotplug_qos_handler is called.
 */
static int cluster1_hotplug_qos_handler(struct notifier_block *b,
		unsigned long val, void *v)
{
	int online_cpu_min, online_cpu_max;
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned long flags;

	if (!sd_tuners || !sd_tuners->qos_core_ctl)
		return 0;

	online_cpu_min = min(pm_qos_request(PM_QOS_CLUSTER1_CORE_MIN),
				sd_tuners->cluster_cpu_ids[CLUSTER1]);
	online_cpu_min = max(online_cpu_min, CLU1_CPU_NUM_MIN);
	online_cpu_max = min(pm_qos_request(PM_QOS_CLUSTER1_CORE_MAX),
				sd_tuners->cluster_cpu_ids[CLUSTER1]);
	online_cpu_max = max(online_cpu_max, CLU1_CPU_NUM_MIN);
	if (online_cpu_min > online_cpu_max)
		online_cpu_min = online_cpu_max;

	mutex_lock(&cpu_num_lock[CLUSTER1]);
	sd_tuners->cpu_num_max_limit[CLUSTER1] = online_cpu_max;
	sd_tuners->cpu_num_min_limit[CLUSTER1] = online_cpu_min;

	/*
	 * make sure num of cores within min and max
	 * for PM_QOS_CLUSTER1_CORE_MIN and PM_QOS_CLUSTER1_CORE_MAX class
	 */
	if (clu_online_cpus(CLUSTER1) > online_cpu_max
		|| clu_online_cpus(CLUSTER1) < online_cpu_min) {
		spin_lock_irqsave(&corechange_lock, flags);
		set_bit(CLUSTER1, &corechange_flag);
		spin_unlock_irqrestore(&corechange_lock, flags);
		wake_up_process(corechange_task);
	}
	mutex_unlock(&cpu_num_lock[CLUSTER1]);
	pr_info("update cluster1 cpu num: %d - %d\n", online_cpu_min,
			online_cpu_max);

	return 0;
}

static struct notifier_block cluster1_qos_notifier = {
	.notifier_call = cluster1_hotplug_qos_handler,
};

static ssize_t store_io_is_busy(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;
	unsigned int j;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;
	sd_tuners->io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		g_prev_cpu_idle[j] = get_cpu_idle_time_sprd(j,
				&g_prev_cpu_wall[j], should_io_be_busy());
	}
	return count;
}

static ssize_t show_io_is_busy(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->io_is_busy);
	return strlen(buf) + 1;
}

static ssize_t store_passion_mode(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	bool input;
	int ret;

	ret = kstrtobool(buf, &input);
	if (ret < 0)
		return ret;
	sd_tuners->passion_mode = input;

	return count;
}

static ssize_t show_passion_mode(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->passion_mode);
	return strlen(buf) + 1;
}

static ssize_t store_sampling_down_factor(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;
	if (input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	sd_tuners->sampling_down_factor = input;

	return count;
}

static ssize_t show_sampling_down_factor(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->sampling_down_factor);
	return strlen(buf) + 1;
}

static ssize_t store_ignore_nice(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	if (input > 1)
		input = 1;

	if (input == sd_tuners->ignore_nice) { /* nothing to do */
		return count;
	}
	sd_tuners->ignore_nice = input;

	return count;
}

static ssize_t show_ignore_nice(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->ignore_nice);
	return strlen(buf) + 1;
}

static ssize_t store_cluster0_core_max_limit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input = 0;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;
	if (input < CLU0_CPU_NUM_MIN
		|| input > sd_tuners->cluster_cpu_ids[CLUSTER0])
		return -EINVAL;

	pr_info("hotplug: userspace ops cluster0 max: %u\n", input);
	pm_qos_update_request(
		&sd_tuners->max_cpu_request[CLUSTER0][HOTPLUG_TEST_MAX], input);
	return count;
}

static ssize_t show_cluster0_core_max_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_num_max_limit[CLUSTER0]);
	return strlen(buf) + 1;
}

static ssize_t store_cluster0_core_min_limit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input = 0;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;
	if (input < CLU0_CPU_NUM_MIN
		|| input > sd_tuners->cluster_cpu_ids[CLUSTER0])
		return -EINVAL;

	pr_info("hotplug: userspace ops cluster0 min: %u\n", input);
	pm_qos_update_request(
		&sd_tuners->min_cpu_request[CLUSTER0][HOTPLUG_TEST_MIN], input);
	return count;
}

static ssize_t show_cluster0_core_min_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_num_min_limit[CLUSTER0]);
	return strlen(buf) + 1;
}

static ssize_t store_cluster1_core_max_limit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input = 0;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;
	if (input > sd_tuners->cluster_cpu_ids[CLUSTER1])
		return -EINVAL;

	pr_info("hotplug: userspace ops cluster1 max: %u\n", input);
	pm_qos_update_request(
		&sd_tuners->max_cpu_request[CLUSTER1][HOTPLUG_TEST_MAX], input);
	return count;
}

static ssize_t show_cluster1_core_max_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_num_max_limit[CLUSTER1]);
	return strlen(buf) + 1;
}

static ssize_t store_cluster1_core_min_limit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input = 0;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;
	if (input > sd_tuners->cluster_cpu_ids[CLUSTER1])
		return -EINVAL;

	pr_info("hotplug: userspace ops cluster1 min: %u\n", input);
	pm_qos_update_request(
		&sd_tuners->min_cpu_request[CLUSTER1][HOTPLUG_TEST_MIN], input);
	return count;
}

static ssize_t show_cluster1_core_min_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_num_min_limit[CLUSTER1]);
	return strlen(buf) + 1;
}

static ssize_t store_qos_core_ctl(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	int max_num = nr_cpu_ids;
	int i = 0, ret = 0, cpu = 0;
	unsigned int input;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	if (sd_tuners->qos_core_ctl == input)
		return count;

	sd_tuners->qos_core_ctl = input;

	/*
	 * plug-in all offline cpu mandatory to max_num if we
	 * disable QOS CORE CTRL function. And we will
	 * wait for max_num of cores online.
	 */
	if (!sd_tuners->qos_core_ctl
		&& num_online_cpus() < max_num) {
qos_retry:
		for_each_possible_cpu(cpu) {
			if (!cpu_online(cpu)) {
				pr_info("qos disabled, gonna plugin cpu%d\n",
						cpu);
				if (hotplug_ops(cpu, REQ_UP))
					pr_err("plugin cpu%d err\n", cpu);
			}
		}
		msleep(20);
		pr_debug("wait for all cpu online!\n");
		if (num_online_cpus() < max_num && i++ < 2)
			goto qos_retry;
	}

	return count;
}

static ssize_t show_qos_core_ctl(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->qos_core_ctl);
	return strlen(buf) + 1;
}

static ssize_t store_hotplug_mode(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	if (input >= MODE_ALL)
		return -EINVAL;

	if (sd_tuners->hotplug_mode != input)
		sd_tuners->hotplug_mode = input;
	return count;
}

static ssize_t show_hotplug_mode(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->hotplug_mode);
	return strlen(buf) + 1;
}

static void sprd_dynamic_load_disable(unsigned int disable)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned long flags;

	if (sd_tuners->cpu_hotplug_disable == disable)
		return;
	sd_tuners->cpu_hotplug_disable = disable;
	/*
	 * Retrigger dynamic-load timer function. here we call
	 * resched timer, but the timer would defer executer.
	 */
	if (!disable)
		mod_timer(&sd_tuners->dynamic_load_timer, jiffies);

	/*
	 * The input value must be set before we really
	 * disable hotplug.
	 */
	smp_wmb();

	/*
	 * plug-in all offline cpu mandatory to max_num if we
	 * disable CPU_DYNAMIC_HOTPLUG function. And we will
	 * wait for max_num of cores online.
	 */
	do {
		int max_num = sd_tuners->cpu_num_max_limit[CLUSTER0];
		int i = 0;

		if (sd_tuners->cpu_hotplug_disable
			&& num_online_cpus() < max_num) {
			spin_lock_irqsave(&corechange_lock, flags);
			set_bit(CLUSTER0, &corechange_flag);
			spin_unlock_irqrestore(&corechange_lock, flags);
			wake_up_process(corechange_task);
			do {
				msleep(20);
				pr_debug("wait for all cpu online!\n");
			} while (num_online_cpus() < max_num && i++ < 10);
		}
	} while (0);
}

static ssize_t store_dynamic_load_disable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	sprd_dynamic_load_disable(input);

	return count;
}

static ssize_t show_dynamic_load_disable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_hotplug_disable);
	return strlen(buf) + 1;
}

static ssize_t store_cpu_up_mid_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	sd_tuners->cpu_up_mid_threshold = input;
	return count;
}

static ssize_t show_cpu_up_mid_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_up_mid_threshold);
}

static ssize_t store_cpu_up_high_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	sd_tuners->cpu_up_high_threshold = input;
	return count;
}

static ssize_t show_cpu_up_high_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_up_high_threshold);
}

static ssize_t store_cpu_down_mid_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	sd_tuners->cpu_down_mid_threshold = input;
	return count;
}

static ssize_t show_cpu_down_mid_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_down_mid_threshold);
}

static ssize_t store_cpu_down_high_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	sd_tuners->cpu_down_high_threshold = input;
	return count;
}

static ssize_t show_cpu_down_high_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->cpu_down_high_threshold);
}

static ssize_t store_up_window_size(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	if (input > MAX_ARRAY_SIZE || input < 1)
		return -EINVAL;

	sd_tuners->up_window_size = input;
	return count;
}

static ssize_t show_up_window_size(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->up_window_size);
}

static ssize_t store_down_window_size(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret < 0)
		return ret;

	if (input > MAX_ARRAY_SIZE || input < 1)
		return -EINVAL;

	sd_tuners->down_window_size = input;
	return count;
}

static ssize_t show_down_window_size(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", g_sd_tuners->down_window_size);
}

static ssize_t store_boostpulse_duration(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	sd_tuners->boostpulse_duration = val;
	return count;
}

static ssize_t show_boostpulse_duration(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%lu\n", g_sd_tuners->boostpulse_duration);
}

static ssize_t store_boostpulse(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val < CLU0_CPU_NUM_MIN || val > nr_cpu_ids
			|| sd_tuners->cpu_hotplug_disable)
		return -EINVAL;

	pm_qos_update_request_timeout(
		&sd_tuners->min_cpu_request[CLUSTER0][HOTPLUG_POWER_HINT],
		val, sd_tuners->boostpulse_duration * USEC_PER_MSEC);
	return count;
}

static ssize_t show_check_load_duration(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%lu\n", g_sd_tuners->check_load_duration);
}

static ssize_t store_check_load_duration(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	sd_tuners->check_load_duration = val;
	return count;
}

define_sprd_global(io_is_busy, 0660);
define_sprd_global(passion_mode, 0660);
define_sprd_global(sampling_down_factor, 0660);
define_sprd_global(ignore_nice, 0660);
define_sprd_global(dynamic_load_disable, 0660);
define_sprd_global(cpu_up_mid_threshold, 0660);
define_sprd_global(cpu_up_high_threshold, 0660);
define_sprd_global(cpu_down_mid_threshold, 0660);
define_sprd_global(cpu_down_high_threshold, 0660);
define_sprd_global(up_window_size, 0660);
define_sprd_global(down_window_size, 0660);
define_sprd_global(boostpulse_duration, 0660);
define_sprd_global_wo(boostpulse, 0220);
define_sprd_global(check_load_duration, 0660);

static const struct attribute *dynamic_hotplug[] = {
	&io_is_busy.attr,
	&passion_mode.attr,
	&sampling_down_factor.attr,
	&ignore_nice.attr,
	&dynamic_load_disable.attr,
	&cpu_up_mid_threshold.attr,
	&cpu_up_high_threshold.attr,
	&cpu_down_mid_threshold.attr,
	&cpu_down_high_threshold.attr,
	&up_window_size.attr,
	&down_window_size.attr,
	&boostpulse_duration.attr,
	&boostpulse.attr,
	&check_load_duration.attr,
	NULL,
};

define_sprd_global(cluster0_core_max_limit, 0660);
define_sprd_global(cluster0_core_min_limit, 0660);
define_sprd_global(cluster1_core_max_limit, 0660);
define_sprd_global(cluster1_core_min_limit, 0660);
define_sprd_global(qos_core_ctl, 0660);
define_sprd_global(hotplug_mode, 0660);

/*
 * NOTICE: ATTR_NUM should be modify if add a new attr.
 */
#define ATTR_NUM	(5)
static const struct attribute *qos_hotplug[CLUSTER_ALL][ATTR_NUM] = {
	{
		&cluster0_core_max_limit.attr,
		&cluster0_core_min_limit.attr,
		&qos_core_ctl.attr,
		&hotplug_mode.attr,
		NULL,
	},
	{
		&cluster1_core_max_limit.attr,
		&cluster1_core_min_limit.attr,
		NULL,
	},
};

static struct kobj_type hotplug_dir_ktype = {
	.sysfs_ops	= &kobj_sysfs_ops,
};

static int
add_qos_files(struct sd_dbs_tuners *tuners, struct kobject *kobj)
{
	int i, ret = 0;

	for (i = 0; i < tuners->cluster_num && !ret; i++)
		ret = sysfs_create_files(kobj, qos_hotplug[i]);

	return ret;
}

/*
 * BOOT_MIN_CPUHOTPLUG allow cores to up during the device
 * being boot-up, while it will Increase power consumption.
 */
#ifdef CONFIG_BOOT_MIN_CPUHOTPLUG
static struct pm_qos_request boot_min_cpu_hotplug_request[CLUSTER_ALL];
#define BOOT_MIN_CPUHOTPLUG_TIME	50
#endif
static void
cpu_hotplug_pm_qos_init(struct sd_dbs_tuners *tuners, unsigned int disabled)
{
	/* Register PM QoS notifier handler */
	pm_qos_add_notifier(PM_QOS_CLUSTER0_CORE_MIN,
		&cluster0_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER0_CORE_MAX,
		&cluster0_qos_notifier);
	if (tuners->cluster_num > 1) {
		pm_qos_add_notifier(PM_QOS_CLUSTER1_CORE_MIN,
			&cluster1_qos_notifier);
		pm_qos_add_notifier(PM_QOS_CLUSTER1_CORE_MAX,
			&cluster1_qos_notifier);
	}

#ifdef CONFIG_BOOT_MIN_CPUHOTPLUG
	/* Guarantee all CPUs running during booting time */
	pm_qos_add_request(
		&boot_min_cpu_hotplug_request[CLUSTER0],
		PM_QOS_CLUSTER0_CORE_MIN, CLU0_CPU_NUM_MIN);
	pm_qos_update_request_timeout(
		&boot_min_cpu_hotplug_request[CLUSTER0],
		tuners->cluster_cpu_ids[CLUSTER0],
		BOOT_MIN_CPUHOTPLUG_TIME * USEC_PER_SEC);
	if (tuners->cluster_num > 1) {
		pm_qos_add_request(
			&boot_min_cpu_hotplug_request[CLUSTER1],
			PM_QOS_CLUSTER1_CORE_MIN, CLU1_CPU_NUM_MIN);
		pm_qos_update_request_timeout(
			&boot_min_cpu_hotplug_request[CLUSTER1],
			tuners->cluster_cpu_ids[CLUSTER1],
			BOOT_MIN_CPUHOTPLUG_TIME * USEC_PER_SEC);
	}
#endif
	/* Add PM QoS cluster0 for debug test */
	pm_qos_add_request(
		&tuners->max_cpu_request[CLUSTER0][HOTPLUG_TEST_MAX],
		PM_QOS_CLUSTER0_CORE_MAX,
		tuners->cluster_cpu_ids[CLUSTER0]);
	pm_qos_add_request(
		&tuners->min_cpu_request[CLUSTER0][HOTPLUG_TEST_MIN],
		PM_QOS_CLUSTER0_CORE_MIN,
		CLU0_CPU_NUM_MIN);
	if (disabled)
		pm_qos_add_request(
			&tuners->min_cpu_request[CLUSTER0][HOTPLUG_FAKE_MIN],
			PM_QOS_CLUSTER0_CORE_MIN,
			tuners->cluster_cpu_ids[CLUSTER0]);
	/* Add PM QoS cluster0 for one user at HAL */
	pm_qos_add_request(
		&tuners->min_cpu_request[CLUSTER0][HOTPLUG_POWER_HINT],
		PM_QOS_CLUSTER0_CORE_MIN,
		CLU0_CPU_NUM_MIN);
	if (tuners->cluster_num > 1) {
		/* Add PM QoS cluster1 for debug test */
		pm_qos_add_request(
			&tuners->max_cpu_request[CLUSTER1][HOTPLUG_TEST_MAX],
			PM_QOS_CLUSTER1_CORE_MAX,
			tuners->cluster_cpu_ids[CLUSTER1]);
		pm_qos_add_request(
			&tuners->min_cpu_request[CLUSTER1][HOTPLUG_TEST_MIN],
			PM_QOS_CLUSTER1_CORE_MIN,
			CLU1_CPU_NUM_MIN);
		if (disabled)
			pm_qos_add_request(
			&tuners->min_cpu_request[CLUSTER1][HOTPLUG_FAKE_MIN],
				PM_QOS_CLUSTER1_CORE_MIN,
				tuners->cluster_cpu_ids[CLUSTER1]);
	}
}

static void cpu_hotplug_pm_qos_exit(struct sd_dbs_tuners *tuners)
{
	/* Register PM QoS notifier handler */
	pm_qos_remove_notifier(PM_QOS_CLUSTER0_CORE_MIN,
		&cluster0_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_CLUSTER0_CORE_MAX,
		&cluster0_qos_notifier);
	if (tuners->cluster_num > 1) {
		pm_qos_remove_notifier(PM_QOS_CLUSTER1_CORE_MIN,
			&cluster1_qos_notifier);
		pm_qos_remove_notifier(PM_QOS_CLUSTER1_CORE_MAX,
			&cluster1_qos_notifier);
	}
}

static int hotplug_pm_notifier_block(struct notifier_block *nb,
				unsigned long mode, void *dummy)
{
	struct sd_dbs_tuners *sd_tuners = g_sd_tuners;
	unsigned long flags;
	int i = 0;

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		sd_tuners->is_suspend = true;
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		sd_tuners->is_suspend = false;
		if (!sd_tuners->qos_core_ctl)
			break;
		spin_lock_irqsave(&corechange_lock, flags);
		for (i = 0; i < sd_tuners->cluster_num; i++)
			if (clu_online_cpus(i) < sd_tuners->cpu_num_min_limit[i]
			|| clu_online_cpus(i) > sd_tuners->cpu_num_max_limit[i])
				set_bit(i, &corechange_flag);
		spin_unlock_irqrestore(&corechange_lock, flags);
		if (!bitmap_empty(&corechange_flag,
				  sd_tuners->cluster_num))
			wake_up_process(corechange_task);
		break;
	default:
		pr_err("%s: Unknown PM request type!\n", __func__);
		break;
	}

	return 0;
}

static struct notifier_block hotplug_pm_notifer = {
	.notifier_call = hotplug_pm_notifier_block,
};

static int sprd_hotplug_parse_dt(struct sd_dbs_tuners *tuners)
{
	struct device_node *np;
	int retval = 0;

	np = of_find_node_by_name(NULL, "sprd-hotplug");
	if (!np) {
		pr_warn("DT: not find sprd-hotplug\n");
		goto parse_end;
	}

	if (of_property_read_bool(np, "enable-dynamic-hotplug")) {
		pr_info("enable dynamic hotplug func\n");
		tuners->cpu_hotplug_disable = false;
	}

parse_end:
	return retval;
}

/*
 * now support two clusters.
 */
static int sd_tuners_init(struct sd_dbs_tuners *tuners)
{
	int i = 0, ret = 0;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	ret = get_cluster_cpu_ids(tuners);
	if (ret) {
		pr_err("%s: Failed to get core_num cluster\n", __func__);
		return ret;
	}

	for (i = 0; i < tuners->cluster_num; i++) {
		tuners->cpu_num_max_limit[i] = tuners->cluster_cpu_ids[i];
		tuners->cpu_num_min_limit[i] = CLU1_CPU_NUM_MIN;
	}

	for (i = 0; i < REQ_ALL; i++)
		atomic_set(&tuners->request[i], 0);

	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice = 0;
	tuners->io_is_busy = should_io_be_busy();

	tuners->cpu_hotplug_disable = true;
	tuners->qos_core_ctl = true;
	tuners->hotplug_mode = false;
	tuners->is_suspend = false;
	tuners->cpu_up_mid_threshold = DEF_CPU_UP_MID_THRESHOLD;
	tuners->cpu_up_high_threshold = DEF_CPU_UP_HIGH_THRESHOLD;
	tuners->cpu_down_mid_threshold = DEF_CPU_DOWN_MID_THRESHOLD;
	tuners->cpu_down_high_threshold = DEF_CPU_DOWN_HIGH_THRESHOLD;
	tuners->up_window_size = UP_LOAD_WINDOW_SIZE;
	tuners->down_window_size = DOWN_LOAD_WINDOW_SIZE;
	tuners->boostpulse_duration = DEF_BOOSTPULSE_DURATION;
	tuners->check_load_duration = DEF_CHECK_LOAD_DURATION;
	tuners->cpu_num_min_limit[CLUSTER0] = CLU0_CPU_NUM_MIN;

	ret = sprd_hotplug_parse_dt(g_sd_tuners);
	if (ret) {
		pr_err("%s: Failed to parse dt data\n", __func__);
		return ret;
	}

	for (i = 0; i < tuners->cluster_num; i++)
		mutex_init(&cpu_num_lock[i]);

	spin_lock_init(&corechange_lock);
	corechange_task =
		kthread_create(sprd_corechange_task, NULL, "sprdhotplug");
	if (IS_ERR(corechange_task))
		return PTR_ERR(corechange_task);
	sched_setscheduler_nocheck(corechange_task, SCHED_FIFO, &param);
	get_task_struct(corechange_task);
	kthread_bind(corechange_task, THREAD_BIND_CPU);

	/* NB: wake up so the thread does not look hung to the freezer */
	wake_up_process(corechange_task);

	/*
	 * Here is a deferrable timer, anyway it will save power with
	 * respect to a normal timer because of cpuidle.
	 */
	init_timer_deferrable(&tuners->dynamic_load_timer);
	tuners->dynamic_load_timer.function = dbs_check_cpu_sprd;
	tuners->dynamic_load_timer.data = 0;
	tuners->dynamic_load_timer.expires = jiffies + HOTPLUG_BOOT_DELAY;

	/*
	 * cpu0 will not be shutdown generally, so a cpu0-pinned timer
	 * will prevent the timer from being migrated.
	 */
	add_timer_on(&tuners->dynamic_load_timer, THREAD_BIND_CPU);

	return 0;
}

static int sprd_add_sysfs_files(struct sd_dbs_tuners *tuners)
{
	int ret = 0;

	if (!tuners->cpu_hotplug_disable) {
		ret = sysfs_create_files(&hotplug_kobj, dynamic_hotplug);
		if (ret)
			pr_err("%s: Failed to add sys file for dyn-plug\n",
				__func__);
	}

	ret = add_qos_files(g_sd_tuners, &hotplug_kobj);
	if (ret)
		pr_err("%s: Failed to add sys file for qos\n", __func__);

	return ret;
}
static int sprd_hotplug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", !g_sd_tuners->cpu_hotplug_disable);
	return 0;
}

static ssize_t sprd_hotplug_proc_write(struct file *file,
const char *buffer, size_t len, loff_t *off)
{
	char input;

	if (len != 2)
		return -EFAULT;
	if (copy_from_user(&input, buffer, len - 1) != 0)
		return -EFAULT;
	sprd_dynamic_load_disable(!(input - '0'));

	return len;
}

static int sprd_hotplug_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, sprd_hotplug_proc_show, NULL);
}

static const struct file_operations sprd_hotplug_proc_fileops = {
	.owner = THIS_MODULE,
	.open = sprd_hotplug_proc_open,
	.read = seq_read,
	.write = sprd_hotplug_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_add_proc_files(struct sd_dbs_tuners *tuners)
{
	if (!tuners->cpu_hotplug_disable) {
		tuners->proc_dir = proc_mkdir("hps", NULL);
		if (!tuners->proc_dir)
			return -ENOMEM;
		tuners->proc_entry = proc_create("enabled", 0644, tuners->proc_dir, &sprd_hotplug_proc_fileops);
		if (!tuners->proc_entry) {
			remove_proc_entry("hps", NULL);
			return -ENOMEM;
		}
	}
	return 0;
}

static int __init sprd_hotplug_init(void)
{
	int ret = 0;
	unsigned int hotplug_disable_save = true;

	boot_done = jiffies + HOTPLUG_BOOT_DELAY;

	g_sd_tuners = kzalloc(sizeof(struct sd_dbs_tuners), GFP_KERNEL);
	if (!g_sd_tuners)
		return ret;

	pr_info("%s: Hotplug and qos init!!!\n", __func__);

	ret = sd_tuners_init(g_sd_tuners);
	if (ret) {
		pr_err("%s: Failed to init tuners\n", __func__);
		goto out_free_mem;
	}
	hotplug_disable_save = g_sd_tuners->cpu_hotplug_disable;

	ret = kobject_init_and_add(&hotplug_kobj, &hotplug_dir_ktype,
		&(cpu_subsys.dev_root->kobj), "cpuhotplug");
	if (ret) {
		pr_err("%s: Failed to add kobject for hotplug\n", __func__);
		goto out_free_mem;
	}

	ret = sprd_add_sysfs_files(g_sd_tuners);
	if (ret) {
		pr_err("%s: Failed to add sys files\n", __func__);
		goto out_free_mem;
	}

	ret = sprd_add_proc_files(g_sd_tuners);
	if (ret) {
		pr_err("%s: Failed to add proc files\n", __func__);
		goto out_free_mem;
	}

	/* Initialize pm_qos request and handler */
	cpu_hotplug_pm_qos_init(g_sd_tuners, hotplug_disable_save);
	ret = register_pm_notifier(&hotplug_pm_notifer);
	if (ret) {
		pr_err("%s: Failed to register pm notify\n", __func__);
		goto out_free_mem;
	}
	return ret;

out_free_mem:
	kfree(g_sd_tuners);
	return ret;
}

static void __exit sprd_hotplug_exit(void)
{
	struct sd_dbs_tuners *tuners = g_sd_tuners;

	unregister_pm_notifier(&hotplug_pm_notifer);
	del_timer_sync(&tuners->dynamic_load_timer);
	cpu_hotplug_pm_qos_exit(tuners);
	kthread_stop(corechange_task);
	put_task_struct(corechange_task);

	kfree(g_sd_tuners);
}

module_init(sprd_hotplug_init);
module_exit(sprd_hotplug_exit);

MODULE_AUTHOR("sprd");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPRD CPU DYNAMIC HOTPLUG DRIVER");
