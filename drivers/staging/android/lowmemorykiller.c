/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/profile.h>
#include <linux/notifier.h>
#include <linux/swap.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/cpuset.h>
#include <linux/vmpressure.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#define CREATE_TRACE_POINTS
#include <trace/events/almk.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

/*These 3 values should be exactly the same as userspace daemon*/
#define LMK_NETLINK_PROTO NETLINK_USERSOCK
#define LMK_NETLINK_GROUP 21
#define LMK_NETLINK_MAX_NAME_LENGTH 100

#ifdef CONFIG_LOWMEM_NOTIFY_KOBJ
#include <linux/kobject.h>
#include <linux/slab.h>
#include "lowmemorykiller.h"
#endif

#ifdef CONFIG_HIGHMEM
#define _ZONE ZONE_HIGHMEM
#else
#define _ZONE ZONE_NORMAL
#endif

#define CREATE_TRACE_POINTS
#include "trace/lowmemorykiller.h"

static struct sock *nl_sk;

static uint32_t lowmem_debug_level = 1;
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;
static int lmk_fast_run = 1;

#ifdef CONFIG_E_SHOW_MEM
/*
 * when some process is killed by lmk, show system memory information
 */
static char *lowmem_proc_name;
#endif

static unsigned long lowmem_deathpending_timeout;
#ifdef CONFIG_OOM_NOTIFIER
static unsigned long oom_deathpending_timeout;
#endif

#ifdef CONFIG_LOWMEM_NOTIFY_KOBJ
static struct shrink_control lowmem_notif_sc = {GFP_KERNEL, 0};
static size_t lowmem_minfree_notif_trigger;
static struct kobject *lowmem_notify_kobj;
#endif

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

struct app_info {
	int uid;
	int pid;
	int adj;
};

struct usr_msg {
	int what;
	union {
		struct app_info app;
		int vmpressure;
	};
};
static void send_killing_app_info_to_user(int uid, int pid, int adj)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct usr_msg msg;
	int msg_size = sizeof(struct usr_msg);
	int res = 0;

	msg.what = 0;
	msg.app.uid = uid;
	msg.app.pid = pid;
	msg.app.adj = adj;

	skb = nlmsg_new(msg_size, 0);
	if (!skb) {
		lowmem_print(1, "allocation failure\n");
		return;
	}
	nlh = nlmsg_put(skb, 0, 1, NLMSG_DONE, msg_size, 0);
	memcpy(nlmsg_data(nlh), &msg, msg_size);
	res = nlmsg_multicast(nl_sk, skb, 0, LMK_NETLINK_GROUP, GFP_KERNEL);
	if (res < 0)
		lowmem_print(1, "nlmsg_multicast error:%d\n", res);
}

static void send_vmpressure_to_user(int vmpressure)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct usr_msg msg;
	int msg_size = sizeof(struct usr_msg);
	int res = 0;

	msg.what = 1;
	msg.vmpressure = vmpressure;

	skb = nlmsg_new(msg_size, 0);
	if (!skb) {
		lowmem_print(1, "allocation failure\n");
		return;
	}
	nlh = nlmsg_put(skb, 0, 1, NLMSG_DONE, msg_size, 0);
	memcpy(nlmsg_data(nlh), &msg, msg_size);
	res = nlmsg_multicast(nl_sk, skb, 0, LMK_NETLINK_GROUP, GFP_KERNEL);
	if (res < 0)
		lowmem_print(1, "nlmsg_multicast error:%d\n", res);
}

/*for testing*/
static void lmk_nl_recv_msg(struct sk_buff *skb)
{
	send_killing_app_info_to_user(100, 100, 100);
}

static unsigned long lowmem_count(struct shrinker *s,
				  struct shrink_control *sc)
{
	return global_node_page_state(NR_ACTIVE_ANON) +
		global_node_page_state(NR_ACTIVE_FILE) +
		global_node_page_state(NR_INACTIVE_ANON) +
		global_node_page_state(NR_INACTIVE_FILE);
}

static atomic_t shift_adj = ATOMIC_INIT(0);

static short adj_max_shift = 900;
static atomic_t vmpressure_mark = ATOMIC_INIT(0);
module_param_named(adj_max_shift, adj_max_shift, short,
	S_IRUGO | S_IWUSR);

/* User knob to enable/disable adaptive lmk feature */
static int enable_adaptive_lmk;
module_param_named(enable_adaptive_lmk, enable_adaptive_lmk, int,
	S_IRUGO | S_IWUSR);

/*
 * This parameter controls the behaviour of LMK when vmpressure is in
 * the range of 90-94. Adaptive lmk triggers based on number of file
 * pages wrt vmpressure_file_min, when vmpressure is in the range of
 * 90-94. Usually this is a pseudo minfree value, higher than the
 * highest configured value in minfree array.
 */
static int vmpressure_file_min;
module_param_named(vmpressure_file_min, vmpressure_file_min, int,
	S_IRUGO | S_IWUSR);

enum {
	VMPRESSURE_NO_ADJUST = 0,
	VMPRESSURE_ADJUST_ENCROACH,
	VMPRESSURE_ADJUST_NORMAL,
};

static int vmpressure_notify_usr_min = 95;
module_param_named(vmpressure_notify_usr_min, vmpressure_notify_usr_min, int,
	S_IRUGO | S_IWUSR);

static int vmpressure_notify_usr_enable;
module_param_named(vmpressure_notify_usr_enable, vmpressure_notify_usr_enable, int,
	S_IRUGO | S_IWUSR);

int adjust_minadj(short *min_score_adj, int *pressure)
{
	int ret = VMPRESSURE_NO_ADJUST;

	if (!enable_adaptive_lmk)
		return 0;

	if (atomic_read(&shift_adj) &&
		(*min_score_adj > adj_max_shift)) {
		if (*min_score_adj == OOM_SCORE_ADJ_MAX + 1)
			ret = VMPRESSURE_ADJUST_ENCROACH;
		else
			ret = VMPRESSURE_ADJUST_NORMAL;
		*min_score_adj = adj_max_shift;
		*pressure = atomic_read(&vmpressure_mark);
	}
	atomic_set(&shift_adj, 0);
	atomic_set(&vmpressure_mark, 0);

	return ret;
}

static int lmk_vmpressure_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	int other_free, other_file;
	unsigned long pressure = action;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (!enable_adaptive_lmk)
		return 0;
	if (pressure >= vmpressure_notify_usr_min &&
		vmpressure_notify_usr_enable) {
		send_vmpressure_to_user(pressure);
	}
	if (pressure >= 95) {
		other_file = global_node_page_state(NR_FILE_PAGES) -
			global_node_page_state(NR_SHMEM) -
			total_swapcache_pages();
		other_free = global_node_page_state(NR_FREE_PAGES);

		atomic_set(&shift_adj, 1);
		atomic_set(&vmpressure_mark, pressure);
		trace_almk_vmpressure(pressure, other_free, other_file);
	} else if (pressure >= 90) {
		if (lowmem_adj_size < array_size)
			array_size = lowmem_adj_size;
		if (lowmem_minfree_size < array_size)
			array_size = lowmem_minfree_size;

		other_file = global_node_page_state(NR_FILE_PAGES) -
			global_node_page_state(NR_SHMEM) -
			total_swapcache_pages();

		other_free = global_node_page_state(NR_FREE_PAGES);

		if ((other_free < lowmem_minfree[array_size - 1]) &&
			(other_file < vmpressure_file_min)) {
				atomic_set(&shift_adj, 1);
				atomic_set(&vmpressure_mark, pressure);
				trace_almk_vmpressure(pressure, other_free,
					other_file);
		}
	}

	return 0;
}

static struct notifier_block lmk_vmpr_nb = {
	.notifier_call = lmk_vmpressure_notifier,
};

#if defined(CONFIG_ANDROID_LOW_MEMORY_KILLER_MEMINFO) || defined(CONFIG_E_SHOW_MEM)
static void dump_tasks_info(void)
{
	struct task_struct *p;
	struct task_struct *task;

	pr_info("[ pid ]   uid  tgid total_vm      rss   swap cpu oom_score_adj name\n");
	rcu_read_lock();
	for_each_process(p) {
		/* check unkillable tasks */
		if (is_global_init(p))
			continue;
		if (p->flags & PF_KTHREAD)
			continue;

		task = find_lock_task_mm(p);
		if (!task) {
			/*
			* This is a kthread or all of p's threads have already
			* detached their mm's.  There's no need to report
			* them; they can't be oom killed anyway.
			*/
			continue;
		}

		pr_info("[%5d] %5d %5d %8lu %8lu %6lu %3u         %5d %s\n",
			task->pid, from_kuid(&init_user_ns, task_uid(task)),
			task->tgid, task->mm->total_vm, get_mm_rss(task->mm),
			get_mm_counter(task->mm, MM_SWAPENTS),
			task_cpu(task),
			task->signal->oom_score_adj, task->comm);
		task_unlock(task);
	}
	rcu_read_unlock();
}
#endif

void tune_lmk_zone_param(struct zonelist *zonelist, int classzone_idx,
					int *other_free, int *other_file)
{
	struct zone *zone;
	struct zoneref *zoneref;
	int zone_idx;

	for_each_zone_zonelist(zone, zoneref, zonelist, MAX_NR_ZONES) {
		zone_idx = zonelist_zone_idx(zoneref);
		if (zone_idx == ZONE_MOVABLE)
			continue;

		if (zone_idx > classzone_idx) {
			if (other_free != NULL)
				*other_free -= zone_page_state(zone,
							       NR_FREE_PAGES);
			if (other_file != NULL)
				*other_file -= zone_page_state(zone,
							       NR_FILE_PAGES)
					      - zone_page_state(zone, NR_SHMEM);
		} else if (zone_idx < classzone_idx) {
			if (zone_watermark_ok(zone, 0, 0, classzone_idx, 0))
				*other_free -=
					zone->lowmem_reserve[classzone_idx];
			else
				*other_free -=
					zone_page_state(zone, NR_FREE_PAGES);
		}
	}
}

#ifdef CONFIG_HIGHMEM
void adjust_gfp_mask(gfp_t *gfp_mask)
{
	struct zone *preferred_zone;
	struct zonelist *zonelist;
	enum zone_type high_zoneidx;
	struct zoneref *z;

	if (current_is_kswapd()) {
		zonelist = node_zonelist(0, *gfp_mask);
		high_zoneidx = gfp_zone(*gfp_mask);
		z = first_zones_zonelist(zonelist, high_zoneidx, NULL);
		preferred_zone = z->zone;

		if (high_zoneidx == ZONE_NORMAL) {
			if (zone_watermark_ok_safe(
					preferred_zone, 0,
					high_wmark_pages(preferred_zone), 0))
				*gfp_mask |= __GFP_HIGHMEM;
		} else if (high_zoneidx == ZONE_HIGHMEM) {
			*gfp_mask |= __GFP_HIGHMEM;
		}
	}
}
#else
void adjust_gfp_mask(gfp_t *unused)
{
}
#endif

void tune_lmk_param(int *other_free, int *other_file, struct shrink_control *sc)
{
	gfp_t gfp_mask;
	struct zone *preferred_zone;
	struct zonelist *zonelist;
	enum zone_type high_zoneidx, classzone_idx;
	struct zoneref *z;

	gfp_mask = sc->gfp_mask;
	adjust_gfp_mask(&gfp_mask);

	zonelist = node_zonelist(0, gfp_mask);
	high_zoneidx = gfp_zone(gfp_mask);
	z = first_zones_zonelist(zonelist, high_zoneidx, NULL);
	preferred_zone = z->zone;

	classzone_idx = zone_idx(preferred_zone);

	if (likely(current_is_kswapd() && zone_watermark_ok(preferred_zone, 0,
			  high_wmark_pages(preferred_zone) + SWAP_CLUSTER_MAX,
			  0, 0))) {
		if (lmk_fast_run)
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       other_file);
		else
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       NULL);

		if (zone_watermark_ok(preferred_zone, 0, 0, _ZONE, 0))
			*other_free -=
				preferred_zone->lowmem_reserve[_ZONE];
		else
			*other_free -= zone_page_state(preferred_zone,
						      NR_FREE_PAGES);
		lowmem_print(4, "lowmem_scan of kswapd tunning for highmem "
			"ofree %d, %d\n", *other_free, *other_file);
	} else {
		tune_lmk_zone_param(zonelist, classzone_idx, other_free,
			       other_file);

		lowmem_print(4, "lowmem_scan tunning for others ofree %d,%d\n",
				*other_free, *other_file);
	}
}

#ifdef CONFIG_E_SHOW_MEM
static int process_need_show_memory(char *cmdline)
{
	if (!cmdline)
		return false;

	lowmem_print(4, "need show memory process name:%s\n", lowmem_proc_name);

	if (lowmem_proc_name && strlen(lowmem_proc_name) > 0)
		if (strstr(lowmem_proc_name, cmdline))
			return true;

	return false;
}

#endif

static int get_current_ram(int *other_free_p, int *other_file_orig_p,
			   int *other_file_p, struct shrink_control *sc)
{
	int other_free, other_file_orig, other_file;
	int unevictable_anon;

	other_free = global_node_page_state(NR_FREE_PAGES);
	other_file_orig = global_node_page_state(NR_FILE_PAGES) -
			  global_node_page_state(NR_SHMEM) -
			  total_swapcache_pages();
	if (other_file_orig < 0)
		other_file_orig = 0;

	unevictable_anon = global_node_page_state(NR_ANON_MAPPED) +
			    global_node_page_state(NR_SHMEM) +
			    total_swapcache_pages() / 2 -
			    global_node_page_state(NR_INACTIVE_ANON) -
			    global_node_page_state(NR_ACTIVE_ANON);
	if (unevictable_anon < 0)
		unevictable_anon = 0;

	other_file = other_file_orig - global_node_page_state(NR_UNEVICTABLE) +
			unevictable_anon;
	if (other_file < 0)
		other_file = 0;

	tune_lmk_param(&other_free, &other_file, sc);

	if (other_free < 0)
		other_free = 0;

	*other_free_p = other_free;
	*other_file_orig_p = other_file_orig;
	*other_file_p = other_file;

	return 1;
}

#ifdef CONFIG_LOWMEM_NOTIFY_KOBJ
static void lowmem_notify_killzone_approach(void);

static int get_free_ram(int *other_free, int *other_file_orig,
			int *other_file, struct shrink_control *sc)
{
	get_current_ram(other_free, other_file_orig,
			other_file, sc);

	if (*other_free < lowmem_minfree_notif_trigger &&
	    *other_file < lowmem_minfree_notif_trigger)
		return 1;
	else
		return 0;
}
#endif

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t;

	for_each_thread(p, t) {
		task_lock(t);
		if (test_tsk_thread_flag(t, flag)) {
			task_unlock(t);
			return 1;
		}
		task_unlock(t);
	}

	return 0;
}

static int test_task_state(struct task_struct *p, int state)
{
	struct task_struct *t;

	for_each_thread(p, t) {
		task_lock(t);
		if (t->state & state) {
			task_unlock(t);
			return 1;
		}
		task_unlock(t);
	}

	return 0;
}

static DEFINE_MUTEX(scan_mutex);

static unsigned long lowmem_scan(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	unsigned long rem = 0;
	int selected_process_uid = 0;
	int selected_process_pid = 0;
	int selected_process_adj = 0;
	int tasksize;
	int i;
	int ret = 0;
	int pressure = 0;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free;
	int other_file;
	struct sysinfo si;
	int other_file_orig;

	/* work around for antutu */
	struct task_struct *selected_antutu = NULL;
	int selected_antutu_tasksize = 0;
	short selected_antutu_adj = -1000;
	bool has_antutu_3D = false;

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_MEMINFO
	static DEFINE_RATELIMIT_STATE(lmk_rs, DEFAULT_RATELIMIT_INTERVAL, 1);
#endif

#ifdef CONFIG_E_SHOW_MEM
	/* 600s */
	static DEFINE_RATELIMIT_STATE(lmk_mem_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12 * 10, 1);
	static DEFINE_RATELIMIT_STATE(lmk_meminfo_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12, 1);
#endif
	if (!mutex_trylock(&scan_mutex))
		return 0;

#ifdef CONFIG_LOWMEM_NOTIFY_KOBJ
	lowmem_notif_sc.gfp_mask = sc->gfp_mask;
	if (get_free_ram(&other_free, &other_file_orig, &other_file, sc)) {
		if (mutex_is_locked(&kernfs_mutex))
			msleep(1);

		if (!mutex_is_locked(&kernfs_mutex))
			lowmem_notify_killzone_approach();
		else
			lowmem_print(1, "skip as kernfs_mutex is locked.");
	}
#else
	get_current_ram(&other_free, &other_file_orig, &other_file, sc);
#endif

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	ret = adjust_minadj(&min_score_adj, &pressure);

	lowmem_print(3, "lowmem_scan %lu, %x, ofree %d %d, ma %hd\n",
			sc->nr_to_scan, sc->gfp_mask, other_free,
			other_file, min_score_adj);

	if (min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		trace_almk_shrink(0, ret, other_free, other_file, 0);
		lowmem_print(5, "lowmem_scan %lu, %x, return 0\n",
			     sc->nr_to_scan, sc->gfp_mask);
		mutex_unlock(&scan_mutex);
		return 0;
	}

	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (time_before_eq(jiffies, lowmem_deathpending_timeout)) {
			if (test_task_flag(tsk, TIF_MEMDIE)) {
				rcu_read_unlock();
				mutex_unlock(&scan_mutex);
				return 0;
			}
		}

		/* workaround for cts case:CtsMediaTestCases
		 * vmpressure is disable in GMS version when run cts.
		 * so this is for gsi version.
		 */
		if (pressure > 0 && strstr(tsk->comm, "decTestProcess"))
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (p->signal->flags & SIGNAL_GROUP_EXIT) {
			lowmem_print(2, "'%s' (%d:%d) group exit, skip.\n",
				     p->comm, p->pid, p->tgid);
			task_unlock(p);
			continue;
		}

		/* workaround for antutu */
		if (strstr("com.antutu.benchmark.full", p->comm))
			has_antutu_3D = true;

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		/* workaround for antutu */
		if (!selected_antutu &&
		    strstr("com.antutu.ABenchMark", p->comm)) {
			selected_antutu = p;
			selected_antutu_tasksize = tasksize;
			selected_antutu_adj = oom_score_adj;
			continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %hd, size %d, to kill\n",
			     p->comm, p->pid, oom_score_adj, tasksize);
	}
	/* workaround for antutu:
	 * if 3D task is not exist, check if the antutu task is more suited
	 * to be killed
	 */
	if (selected && selected_antutu && !has_antutu_3D) {
		if (selected_antutu_adj > selected_oom_score_adj ||
		    (selected_antutu_adj == selected_oom_score_adj &&
		    selected_antutu_tasksize > selected_tasksize)) {
			selected = selected_antutu;
			selected_tasksize = selected_antutu_tasksize;
			selected_oom_score_adj = selected_antutu_adj;
		}
	}
	if (selected) {
		long cache_size = other_file * (long)(PAGE_SIZE / 1024);
		long cache_size_orig = other_file_orig * (long)(PAGE_SIZE / 1024);
		long cache_limit = minfree * (long)(PAGE_SIZE / 1024);
		long free = other_free * (long)(PAGE_SIZE / 1024);

		if (test_task_flag(selected, TIF_MEMDIE) &&
		    (test_task_state(selected, TASK_UNINTERRUPTIBLE))) {
			lowmem_print(2, "'%s' (%d) is already killed\n",
				     selected->comm,
				     selected->pid);
			rcu_read_unlock();
			mutex_unlock(&scan_mutex);
			return 0;
		}

		task_lock(selected);
		/* add for lmfs */
		selected_process_uid = from_kuid(&init_user_ns,
						 selected->cred->uid);
		selected_process_pid = selected->pid;
		selected_process_adj = selected_oom_score_adj;

		send_sig(SIGKILL, selected, 0);
		/*
		 * FIXME: lowmemorykiller shouldn't abuse global OOM killer
		 * infrastructure. There is no real reason why the selected
		 * task should have access to the memory reserves.
		 */
		if (selected->mm)
			mark_oom_victim(selected);
		task_unlock(selected);
		trace_lowmemory_kill(selected, cache_size, cache_limit, free);
		si_swapinfo(&si);
		lowmem_print(1, "Killing '%s' (%d:%d), adj %hd,\n"
			"   to free %ldkB on behalf of '%s' (%d) because\n"
			"   cache is %ldkB , limit is %ldkB for oom_score_adj %hd\n"
			"   Free memory is %ldkB above reserved\n"
			"   swaptotal is %ldkB, swapfree is %ldkB, pressure is %d\n"
			"   cache_orig is %ldkB\n",
			     selected->comm, selected->pid, selected->tgid,
			     selected_oom_score_adj,
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     current->comm, current->pid,
			     cache_size, cache_limit,
			     min_score_adj, free,
			     si.totalswap * (long)(PAGE_SIZE / 1024),
			     si.freeswap * (long)(PAGE_SIZE / 1024),
			     pressure, cache_size_orig);
		lowmem_deathpending_timeout = jiffies + HZ;
		rem += selected_tasksize;
		trace_almk_shrink(selected_tasksize, ret,
			other_free, other_file, selected_oom_score_adj);
	} else {
	    trace_almk_shrink(1, ret, other_free, other_file, 0);
	}
	rcu_read_unlock();
	mutex_unlock(&scan_mutex);

	if (selected) {
		send_killing_app_info_to_user(selected_process_uid,
					      selected_process_pid,
					      selected_process_adj);

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_MEMINFO
		if (__ratelimit(&lmk_rs))
			dump_tasks_info();
#endif

#ifdef CONFIG_E_SHOW_MEM
		if ((0 == min_score_adj)
			&& (__ratelimit(&lmk_meminfo_rs))) {
			enhanced_show_mem(E_SHOW_MEM_ALL);
		} else if (__ratelimit(&lmk_mem_rs)) {
			if ((!si.freeswap)
				|| ((si.totalswap / (si.freeswap + 1)) >= 10))
				enhanced_show_mem(E_SHOW_MEM_CLASSIC);
			else
				enhanced_show_mem(E_SHOW_MEM_BASIC);
		} else if (process_need_show_memory(selected->comm)) {
			enhanced_show_mem(E_SHOW_MEM_ALL);
		}
#endif
	}
	lowmem_print(4, "lowmem_scan %lu, %x, return %lu\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	return rem;
}
#ifdef CONFIG_OOM_NOTIFIER
#ifdef CONFIG_MULTIPLE_OOM_KILLER
#define OOM_DEPTH CONFIG_MULTIPLE_OOM_KILL_COUNT
static int select_max_oom_idx(const int *oom_score_adj, const int *tasksize,
				int num)
{
	int i;
	int s_idx = 0;

	if (num > OOM_DEPTH || !oom_score_adj || !tasksize)
		return 0;

	for (i = 1; i < num; i++) {
		if (*(oom_score_adj + i) < *(oom_score_adj + s_idx))
			s_idx = i;
		else if (*(oom_score_adj + i) == *(oom_score_adj + s_idx) &&
			*(tasksize + i) < *(tasksize + s_idx))
			s_idx = i;
	}
	return s_idx;
}

static unsigned long multiple_oom_killer(unsigned long *freed)
{
	struct task_struct *tsk;
	struct task_struct *selected[OOM_DEPTH] = {NULL,};

	int task_mm_size;
	int i;
	int min_score_adj = 0;
	unsigned long rem = 0;
	int tasksize[OOM_DEPTH] = {0,};
	int oom_score_adj[OOM_DEPTH] = {OOM_ADJUST_MAX,};
	int all_oom = 0;
	int max_oom_idx = 0;
	bool has_kill = false;

	for (i = 0; i < OOM_DEPTH; i++)
		oom_score_adj[i] = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		int adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (time_before_eq(jiffies, oom_deathpending_timeout)) {
			if (test_task_flag(tsk, TIF_MEMDIE)) {
				rcu_read_unlock();
				return 0;
			}
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		adj = p->signal->oom_score_adj;
		if (adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		task_mm_size = get_mm_rss(p->mm);
		task_unlock(p);
		if (task_mm_size <= 0)
			continue;

		lowmem_print(2, "oom: ------ %d (%s), adj %d, size %d\n",
			     p->pid, p->comm, adj, task_mm_size);

		if (all_oom < OOM_DEPTH) {
			selected[all_oom] = p;
			tasksize[all_oom] = task_mm_size;
			oom_score_adj[all_oom] = adj;
			all_oom++;

			max_oom_idx = select_max_oom_idx(oom_score_adj,
								tasksize,
								all_oom);
			lowmem_print(2, "oom: select %d (%s), adj %d, size %d"
					" to kill. cur_idx %d, max_oom_idx %d\n",
					p->pid, p->comm, adj,
					task_mm_size, all_oom-1, max_oom_idx);

			continue;
		}

		if (all_oom == OOM_DEPTH &&
			(oom_score_adj[max_oom_idx] < adj ||
			(oom_score_adj[max_oom_idx] == adj &&
			tasksize[max_oom_idx] < task_mm_size))) {

			selected[max_oom_idx] = p;
			tasksize[max_oom_idx] = task_mm_size;
			oom_score_adj[max_oom_idx] = adj;

			lowmem_print(2, "oom: select %d (%s), adj %d, size %d"
					" to kill, overwrite idx %d.\n",
					p->pid, p->comm,
					adj, task_mm_size, max_oom_idx);

			max_oom_idx = select_max_oom_idx(oom_score_adj,
								tasksize,
								all_oom);
		}


	}

	for (i = 0; i < OOM_DEPTH; i++) {
		if (selected[i]) {
			if (test_task_flag(selected[i], TIF_MEMDIE) &&
			    test_task_state(selected[i], TASK_UNINTERRUPTIBLE))
				continue;

			task_lock(selected[i]);
			send_sig(SIGKILL, selected[i], 0);
			if (selected[i]->mm)
				mark_oom_victim(selected[i]);
			task_unlock(selected[i]);

			lowmem_print(1, "oom: send sigkill to %d (%s) adj %d,"
					" size %d\n",
				     selected[i]->pid, selected[i]->comm,
				     oom_score_adj[i],
				     tasksize[i]);

			rem += tasksize[i];
			has_kill = true;
			*freed += (unsigned long)tasksize[i];
		}
	}
	if (has_kill)
		oom_deathpending_timeout = jiffies + HZ;

	rcu_read_unlock();

	lowmem_print(2, "oom: get memory %lu", *freed);
	return rem;
}
#else
static unsigned long oom_killer(unsigned long *freed)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	int tasksize;
	int min_score_adj = 0;
	unsigned long rem = 0;
	int selected_tasksize = 0;
	int selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		int oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (time_before_eq(jiffies, oom_deathpending_timeout)) {
			if (test_task_flag(tsk, TIF_MEMDIE)) {
				rcu_read_unlock();
				return 0;
			}
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;

		lowmem_print(2, "oom: ------ %d (%s), adj %d, size %d\n",
			     p->pid, p->comm, oom_score_adj, tasksize);

		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "oom: select %d (%s),adj %d,size %d, to kill\n",
			     p->pid, p->comm, oom_score_adj, tasksize);

	}

	if (selected) {
		if (test_task_flag(selected, TIF_MEMDIE) &&
		    test_task_state(selected, TASK_UNINTERRUPTIBLE)) {
			rcu_read_unlock();
			return 0;
		}
		task_lock(selected);
		send_sig(SIGKILL, selected, 0);
		if (selected->mm)
			mark_oom_victim(selected);
		task_unlock(selected);

		lowmem_print(1, "oom: send sigkill to %d (%s),adj %d,size %d\n",
			     selected->pid, selected->comm,
			     selected_oom_score_adj, selected_tasksize);
		rem += selected_tasksize;
		*freed += (unsigned long)selected_tasksize;
		oom_deathpending_timeout = jiffies + HZ;
	}

	rcu_read_unlock();

	lowmem_print(2, "oom: get memory %lu", *freed);
	return rem;
}
#endif
/*
 * CONFIG_OOM_NOTIFIER
 *
 * Android does the final attempt to reclaim memory before oom selects the
 * victim.That's mean, oom doesn't work as long as one task which oom_score_adj
 * above 0 is still alive.This is useful.In special case, system has many file
 * cache which is difficult to reduce,if file cache always above the minfree,
 * lmk doesn't run.so we need this mechanism to solve the problem.
 *
*/
static int android_oom_handler(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	unsigned long rem = 0;
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_MEMINFO
	static DEFINE_RATELIMIT_STATE(oom_rs, DEFAULT_RATELIMIT_INTERVAL/5, 1);
#endif
#ifdef CONFIG_E_SHOW_MEM
	/* 600s */
	static DEFINE_RATELIMIT_STATE(oom_mem_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12 * 10, 1);
	/* 60s */
	static DEFINE_RATELIMIT_STATE(oom_meminfo_rs,
		DEFAULT_RATELIMIT_INTERVAL * 12, 1);
#endif

	unsigned long *freed = data;

	lowmem_print(1, "enter android_oom_handler\n");

	/* show status */
	pr_warn("%s invoked Android-oom-killer: oom_score_adj=%d\n",
		current->comm, current->signal->oom_score_adj);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_MEMINFO
	dump_stack();
	show_mem(SHOW_MEM_FILTER_NODES, NULL);
	if (__ratelimit(&oom_rs))
		dump_tasks_info();
#endif

#ifdef CONFIG_MULTIPLE_OOM_KILLER
	rem = multiple_oom_killer(freed);
#else
	rem = oom_killer(freed);
#endif

#ifdef CONFIG_E_SHOW_MEM
	if (__ratelimit(&oom_meminfo_rs))
		enhanced_show_mem(E_SHOW_MEM_CLASSIC);
	else if (__ratelimit(&oom_mem_rs))
		enhanced_show_mem(E_SHOW_MEM_BASIC);
#endif
	return NOTIFY_DONE;
}

static struct notifier_block android_oom_notifier = {
	.notifier_call = android_oom_handler,
};
#endif /* CONFIG_OOM_NOTIFIER */

#ifdef CONFIG_E_SHOW_MEM
static int tasks_e_show_mem_handler(struct notifier_block *nb,
			unsigned long val, void *data)
{
	enum e_show_mem_type type = val;
	struct sysinfo si;

	si_swapinfo(&si);
	pr_info("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	pr_info("Enhanced Mem-info :TASK\n");
	pr_info("Detail:\n");
	if (E_SHOW_MEM_CLASSIC == type || E_SHOW_MEM_ALL == type)
		dump_tasks_info();
	pr_info("Total used:\n");
	pr_info("anon: %lu kB\n",
	       ((global_node_page_state(NR_ACTIVE_ANON) +
		global_node_page_state(NR_INACTIVE_ANON)) << PAGE_SHIFT)
		 / 1024);
	pr_info("swaped: %lu kB\n", ((si.totalswap - si.freeswap)
		<< PAGE_SHIFT) / 1024);
	return 0;
}

static struct notifier_block tasks_e_show_mem_notifier = {
	.notifier_call = tasks_e_show_mem_handler,
};
#endif

static struct shrinker lowmem_shrinker = {
	.scan_objects = lowmem_scan,
	.count_objects = lowmem_count,
	.seeks = DEFAULT_SEEKS * 16,
	.flags = SHRINKER_LMK
};

static struct netlink_kernel_cfg cfg  = {
	.input = lmk_nl_recv_msg,
};

#ifdef CONFIG_LOWMEM_NOTIFY_KOBJ
static void lowmem_notify_killzone_approach(void)
{
	lowmem_print(3, "notification trigger activated\n");
	sysfs_notify(lowmem_notify_kobj, NULL,
		     "notify_trigger_active");
}

static ssize_t lowmem_notify_trigger_active_show(struct kobject *k,
						 struct kobj_attribute *attr,
						 char *buf)
{
	int other_free, other_file_orig, other_file;

	if (get_free_ram(&other_free, &other_file_orig,
			 &other_file, &lowmem_notif_sc))
		return snprintf(buf, 3, "1\n");
	else
		return snprintf(buf, 3, "0\n");
}

static struct kobj_attribute lowmem_notify_trigger_active_attr =
	__ATTR(notify_trigger_active, S_IRUGO,
	       lowmem_notify_trigger_active_show, NULL);

static struct attribute *lowmem_notify_default_attrs[] = {
	&lowmem_notify_trigger_active_attr.attr, NULL,
};

static ssize_t lowmem_show(struct kobject *k, struct attribute *attr, char *buf)
{
	struct kobj_attribute *kobj_attr;

	kobj_attr = container_of(attr, struct kobj_attribute, attr);
	return kobj_attr->show(k, kobj_attr, buf);
}

static const struct sysfs_ops lowmem_notify_ops = {
	.show = lowmem_show,
};

static void lowmem_notify_kobj_release(struct kobject *kobj)
{
	/* Nothing to be done here */
}

static struct kobj_type lowmem_notify_kobj_type = {
	.release = lowmem_notify_kobj_release,
	.sysfs_ops = &lowmem_notify_ops,
	.default_attrs = lowmem_notify_default_attrs,
};
#endif

static int __init lowmem_init(void)
{
#ifdef CONFIG_LOWMEM_NOTIFY_KOBJ
	int rc;

	lowmem_notify_kobj = kzalloc(sizeof(*lowmem_notify_kobj), GFP_KERNEL);
	if (!lowmem_notify_kobj)
		return -ENOMEM;

	rc = kobject_init_and_add(lowmem_notify_kobj, &lowmem_notify_kobj_type,
				  mm_kobj, "lowmemkiller");
	if (rc) {
		kfree(lowmem_notify_kobj);
		return rc;
	}
#endif

	register_shrinker(&lowmem_shrinker);
#ifdef CONFIG_OOM_NOTIFIER
	register_oom_notifier(&android_oom_notifier);
#endif
#ifdef CONFIG_E_SHOW_MEM
	register_e_show_mem_notifier(&tasks_e_show_mem_notifier);
#endif
	vmpressure_notifier_register(&lmk_vmpr_nb);
	lowmem_print(1, "entering:%s\n", __func__);
	nl_sk = netlink_kernel_create(&init_net, LMK_NETLINK_PROTO, &cfg);
	if (!nl_sk)
		lowmem_print(1, "error createing nl socket.\n");

	return 0;
}
device_initcall(lowmem_init);

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	short oom_adj;
	short oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_short,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

/*
 * not really modular, but the easiest way to keep compat with existing
 * bootargs behaviour is to continue using module_param here.
 */
module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
module_param_cb(adj, &lowmem_adj_array_ops,
		.arr = &__param_arr_adj,
		S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);
module_param_named(lmk_fast_run, lmk_fast_run, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_E_SHOW_MEM
module_param_named(proc_name, lowmem_proc_name, charp, S_IRUGO | S_IWUSR);
#endif
#ifdef CONFIG_LOWMEM_NOTIFY_KOBJ
module_param_named(notify_trigger, lowmem_minfree_notif_trigger, uint,
		   S_IRUGO | S_IWUSR);
#endif

