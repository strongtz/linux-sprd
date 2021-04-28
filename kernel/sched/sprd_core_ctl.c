#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/topology.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/percpu.h>
#include <linux/math64.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "sched.h"
#include "walt.h"
#include <linux/sched/rt.h>
#include <linux/capability.h>

#ifndef pr_fmt
#define pr_fmt(fmt)	"core_ctl: " fmt
#endif

#define CORE_CTL_INFO(fmt, args...)\
	pr_info("core_ctl: " fmt, ## args)

#define CORE_CTL_ERR(fmt, args...)\
	pr_err("core_ctl_error: " fmt, ## args)

/* ========================= define data struct =========================== */
enum plug_type {
	PLUGOUT,
	PLUGIN,
};

enum set_type {
	SETMAX,
	SETMIN,
};

struct cluster_data {
	bool inited;
	bool pending;
	bool enable;
	unsigned int min_cpus;
	unsigned int max_cpus;
	unsigned int active_cpus;
	unsigned int num_cpus;
	unsigned int nr_isolated_cpus;
	unsigned int need_cpus;
	unsigned int first_cpu;
	cpumask_t latest_cpumask;
	unsigned int capacity;
	unsigned int isolate_cpu_id;
	unsigned int unisolate_cpu_id;
	cpumask_t cpu_mask;
	unsigned int id;
	enum set_type settype;
	spinlock_t pending_lock;
	struct task_struct *core_ctl_thread;
	struct list_head lru;
	struct list_head cluster_node;
	struct kobject kobj;
};

struct cpu_data {
	bool isolated_by_us;
	int cpu;
	u64 isolate_cnt;
	struct cluster_data *cluster;
	struct list_head sib;
};


static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static DEFINE_SPINLOCK(state_lock);
static LIST_HEAD(cluster_list);
static bool initialized;

static void wake_up_core_ctl_thread(struct cluster_data *state);
static unsigned int get_active_cpu_count(const struct cluster_data *cluster);

/* ========================= external call API =========================== */
/**
 * target: will be plugin or plugout target cpumask
 * maybe is one cpu, maybe >= 2cpus
 * plug_type: cpu will be plugin or plugout
 * return 0 is success,or fail.
 */
int ctrl_core_api(struct cpumask *target, int type)
{
	struct cluster_data *temp;
	unsigned int target_num = cpumask_weight(target);
	unsigned long flags;

	CORE_CTL_INFO("START %s, cpu_id=%u, cpu_num=%u\n",
		      type == 0 ? "isolate" : "unisolate",
		      cpumask_first(target), target_num);

	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry(temp, &cluster_list, cluster_node) {
		unsigned int cpuweight = temp->num_cpus;

		if (cpumask_intersects(target, &temp->cpu_mask) &&
		    temp->active_cpus == temp->need_cpus) {
			cpumask_and(&temp->latest_cpumask, target,
				    &temp->cpu_mask);
			target_num = cpumask_weight(&temp->latest_cpumask);

			if (type == PLUGIN) {
				temp->need_cpus += target_num;
				if (temp->need_cpus > temp->max_cpus) {
					temp->need_cpus -= target_num;
					goto err;
				}
			} else if (type == PLUGOUT) {
				temp->need_cpus -= target_num;
				if (temp->need_cpus < temp->min_cpus) {
					temp->need_cpus += target_num;
					goto err;
				}
			}

			if (unlikely(temp->active_cpus > cpuweight) ||
					temp->need_cpus == temp->active_cpus) {
				goto err;
			}

			spin_unlock_irqrestore(&state_lock, flags);
			CORE_CTL_INFO("start avtive_cpus=%u, need_cpus=%u\n",
					temp->active_cpus, temp->need_cpus);
			wake_up_core_ctl_thread(temp);
			return 0;
		}
	}

err:
	spin_unlock_irqrestore(&state_lock, flags);
	CORE_CTL_ERR("START %s fail\n", type == 0 ? "isolate" : "unisolate");
	return -1;
}
EXPORT_SYMBOL_GPL(ctrl_core_api);

/**
 * cpu_num: is a cpu set,all cpu will be plugin or plugout in the cpumask
 * type: set MAX or MIN cpu num in cluster_id
 * return 0 is success,or fail.
 */
int ctrl_max_min_core_api(unsigned int cpu_num, int type,
			  unsigned int cluster_id)
{
	struct cluster_data *temp;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry(temp, &cluster_list, cluster_node) {
		CORE_CTL_INFO("%s:id=%u, type=%d,cpu_num=%u\n",
			      __func__, temp->id, type, cpu_num);
		if (temp->active_cpus != temp->need_cpus ||
		    temp->id != cluster_id)
			continue;

		temp->settype = type;

		if (type == SETMIN) {
			if (temp->active_cpus < cpu_num) {
				temp->need_cpus +=
					(cpu_num - temp->active_cpus);
				spin_unlock_irqrestore(&state_lock, flags);
				wake_up_core_ctl_thread(temp);
				return 0;
			}
		} else if (type == SETMAX) {
			if (temp->active_cpus > cpu_num) {
				temp->need_cpus -=
					(temp->active_cpus - cpu_num);
				spin_unlock_irqrestore(&state_lock, flags);
				wake_up_core_ctl_thread(temp);
				return 0;
			}
		}
		/*if have error, reset it.*/
		temp->settype = -1;
	}

	spin_unlock_irqrestore(&state_lock, flags);
	CORE_CTL_ERR("%s, set fail\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(ctrl_max_min_core_api);

/* ========================= sysfs interface =========================== */
static ssize_t store_min_cpus(struct cluster_data *state,
			      const char *buf, size_t count)
{
	unsigned int val;
	unsigned long flags;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (state->id == 0 && val == 0)
		return -EINVAL;

	spin_lock_irqsave(&state_lock, flags);
	state->min_cpus = min(val, state->max_cpus);
	spin_unlock_irqrestore(&state_lock, flags);

	ctrl_max_min_core_api(state->min_cpus, SETMIN, state->id);
	return count;
}

static ssize_t show_min_cpus(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->min_cpus);
}

static ssize_t store_max_cpus(struct cluster_data *state,
			      const char *buf, size_t count)
{
	unsigned int val;
	unsigned long flags;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val == 0 && state->id == 0)
		return -EINVAL;

	spin_lock_irqsave(&state_lock, flags);
	val = min(val, state->num_cpus);
	state->max_cpus = val;
	state->min_cpus = min(state->min_cpus, state->max_cpus);
	spin_unlock_irqrestore(&state_lock, flags);

	ctrl_max_min_core_api(state->max_cpus, SETMAX, state->id);
	return count;
}

static ssize_t show_max_cpus(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->max_cpus);
}

static ssize_t show_global_state(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	ssize_t count = 0;
	int cpu;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	for_each_possible_cpu(cpu) {
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;
		if (!cluster || !cluster->inited)
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "CPU%u\n", cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tOnline: %u\n",
				  cpu_online(cpu));
		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tIsolated: %u\n",
				  cpu_isolated(cpu));
		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tIsolate cnt: %llu\n",
				  c->isolate_cnt);
		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tFirst CPU: %u\n",
				  cluster->first_cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tCLUSTER ID: %u\n",
				  cluster->id);

		if (cpu != cluster->first_cpu)
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tActive CPUs: %u\n",
				  get_active_cpu_count(cluster));
		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tNeed CPUs: %u\n",
				  cluster->need_cpus);
		count += scnprintf(buf + count, PAGE_SIZE - count,
				  "\tNr isolated CPUs: %u\n",
				  cluster->nr_isolated_cpus);
	}
	spin_unlock_irqrestore(&state_lock, flags);

	return count;
}

static ssize_t show_need_cpus(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->need_cpus);
}

static ssize_t show_active_cpus(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->active_cpus);
}

static ssize_t store_enable(struct cluster_data *state,
			    const char *buf, size_t count)
{
	unsigned int val;
	bool enable;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	enable = !!val;
	if (enable != state->enable)
		state->enable = enable;

	return count;
}

static ssize_t show_enable(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->enable);
}

static ssize_t store_isolate_cpu_id(struct cluster_data *state,
			     const char *buf, size_t count)
{
	unsigned int val;
	unsigned int first_cpu = cpumask_first(&state->cpu_mask);
	unsigned int cpu_weight = cpumask_weight(&state->cpu_mask);
	cpumask_var_t target_cpumask;

	if (sscanf(buf, "%u\n", &val) != 1 ||
	   (val < first_cpu || val >= first_cpu + cpu_weight))
		return -EINVAL;

	if ((val == 0 && state->id == 0) || cpu_isolated(val))
		return -EINVAL;

	if (!zalloc_cpumask_var(&target_cpumask, GFP_KERNEL))
		return -ENOMEM;

	state->isolate_cpu_id = val;
	cpumask_set_cpu(val, target_cpumask);
	CORE_CTL_INFO("isolate weight=%d\n", cpumask_weight(target_cpumask));
	ctrl_core_api(target_cpumask, PLUGOUT);
	free_cpumask_var(target_cpumask);
	return count;
}

static ssize_t show_isolate_cpu_id(const struct cluster_data *state, char *buf)
{
	int j;
	ssize_t count = 0;
	cpumask_t isolate_mask;

	cpumask_and(&isolate_mask, &state->cpu_mask, cpu_isolated_mask);

	for_each_cpu(j, &isolate_mask) {
		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "isolate cpu:%d\n", j);
	}

	return count;
}

static ssize_t store_unisolate_cpu_id(struct cluster_data *state,
			       const char *buf, size_t count)
{
	unsigned int val;
	cpumask_var_t target_cpumask;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (!cpu_isolated(val))
		return -EINVAL;

	if (!zalloc_cpumask_var(&target_cpumask, GFP_KERNEL))
		return -ENOMEM;

	state->unisolate_cpu_id = val;
	cpumask_set_cpu(val, target_cpumask);
	CORE_CTL_INFO("unisolate weight=%d\n", cpumask_weight(target_cpumask));
	ctrl_core_api(target_cpumask, PLUGIN);
	free_cpumask_var(target_cpumask);

	return count;
}

static ssize_t show_unisolate_cpu_id(const struct cluster_data *state,
				     char *buf)
{
	int j;
	ssize_t count = 0;
	cpumask_t unisolate_mask;

	cpumask_andnot(&unisolate_mask, &state->cpu_mask, cpu_isolated_mask);

	for_each_cpu(j, &unisolate_mask) {
		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "unisolate cpu:%d\n", j);
	}

	return count;
}

struct core_ctl_attr {
	struct attribute attr;
	ssize_t (*show)(const struct cluster_data *, char *);
	ssize_t (*store)(struct cluster_data *, const char *, size_t count);
};

#define core_ctl_attr_ro(_name)		\
static struct core_ctl_attr _name =	\
__ATTR(_name, 0440, show_##_name, NULL)

#define core_ctl_attr_rw(_name)			\
static struct core_ctl_attr _name =		\
__ATTR(_name, 0640, show_##_name, store_##_name)

core_ctl_attr_rw(min_cpus);
core_ctl_attr_rw(max_cpus);
core_ctl_attr_ro(need_cpus);
core_ctl_attr_ro(active_cpus);
core_ctl_attr_ro(global_state);
core_ctl_attr_rw(enable);
core_ctl_attr_rw(isolate_cpu_id);
core_ctl_attr_rw(unisolate_cpu_id);

static struct attribute *default_attrs[] = {
	&min_cpus.attr,
	&max_cpus.attr,
	&need_cpus.attr,
	&active_cpus.attr,
	&global_state.attr,
	&enable.attr,
	&isolate_cpu_id.attr,
	&unisolate_cpu_id.attr,
	NULL
};

#define to_cluster_data(k) container_of(k, struct cluster_data, kobj)
#define to_attr(a) container_of(a, struct core_ctl_attr, attr)
static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->show)
		ret = cattr->show(data, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->store)
		ret = cattr->store(data, buf, count);

	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_core_ctl = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= default_attrs,
};

static bool is_active(const struct cpu_data *state)
{
	return cpu_online(state->cpu) && !cpu_isolated(state->cpu);
}

static unsigned int get_active_cpu_count(const struct cluster_data *cluster)
{
	return cluster->num_cpus -
				sched_isolate_count(&cluster->cpu_mask, true);
}

/* ======================= enforcement core control ====================== */
static void wake_up_core_ctl_thread(struct cluster_data *cluster)
{
	unsigned long flags;

	spin_lock_irqsave(&cluster->pending_lock, flags);
	cluster->pending = true;
	spin_unlock_irqrestore(&cluster->pending_lock, flags);

	wake_up_process(cluster->core_ctl_thread);
}

static void move_cpu_lru(struct cpu_data *state, bool forward)
{
	list_del(&state->sib);
	if (forward)
		list_add_tail(&state->sib, &state->cluster->lru);
	else
		list_add(&state->sib, &state->cluster->lru);
}

static void try_to_isolate(struct cluster_data *cluster, unsigned int need)
{
	struct cpu_data *c, *tmp;
	unsigned long flags;
	unsigned int nr_isolated = 0;
	/*
	 * Protect against entry being removed (and added at tail) by other
	 * thread (hotplug).
	 */
	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
		if (!cluster->max_cpus && cluster->id == 0)
			break;

		if (cluster->active_cpus == need)
			break;

		if (!is_active(c))
			continue;
		/**
		 * Use the CPU mask to indicate the cpu sets that
		 * need to be isolated, because it is possible to
		 * isolate multiple cpus within a cluster at one time.
		 * lastest_cpumask is newest isolate cpu request.
		 */
		if (cluster->settype == -1) {
			const struct cpumask *ready_isolate_cpumask =
						get_cpu_mask(c->cpu);

			if (!cpumask_subset(ready_isolate_cpumask,
			    &cluster->latest_cpumask))
				continue;
		}
		spin_unlock_irqrestore(&state_lock, flags);

		CORE_CTL_INFO("Trying to isolate CPU%u\n", c->cpu);
		if (!sched_isolate_cpu(c->cpu)) {
			c->isolated_by_us = true;
			c->isolate_cnt++;
			move_cpu_lru(c, true);
			nr_isolated++;
			CORE_CTL_INFO("isolate_success: CPU%u\n", c->cpu);
		} else {
			CORE_CTL_ERR("isolate_fail: CPU%u\n", c->cpu);
		}

		spin_lock_irqsave(&state_lock, flags);

		cluster->active_cpus = get_active_cpu_count(cluster);
	}

	cluster->nr_isolated_cpus += nr_isolated;
	cluster->settype = -1;

	/*reset core ctrl conditon if active_cpus!=need_cpus*/
	if (unlikely(cluster->need_cpus != cluster->active_cpus))
		cluster->need_cpus = cluster->active_cpus;

	spin_unlock_irqrestore(&state_lock, flags);

	CORE_CTL_INFO("END %s:active_cpus=%u,need_cpus=%u\n",
		      __func__, cluster->active_cpus, cluster->need_cpus);
}

static void try_to_unisolate(struct cluster_data *cluster, unsigned int need)
{
	struct cpu_data *c, *tmp;
	unsigned long flags;
	unsigned int nr_unisolated = 0;

	/*
	 * Protect against entry being removed (and added at tail) by other
	 * thread (hotplug).
	 */
	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
		if (!cluster->max_cpus)
			break;

		if (!c->isolated_by_us)
			continue;

		if (cluster->settype == -1) {
			const struct cpumask *ready_unisolate_cpumask =
							get_cpu_mask(c->cpu);

			if (!cpumask_subset(ready_unisolate_cpumask,
			    &cluster->latest_cpumask))
				continue;
		}

		if (cluster->active_cpus == need)
			break;

		spin_unlock_irqrestore(&state_lock, flags);

		CORE_CTL_INFO("Trying to unisolate CPU%u\n", c->cpu);
		if (!sched_unisolate_cpu(c->cpu)) {
			c->isolated_by_us = false;
			c->isolate_cnt--;
			move_cpu_lru(c, false);
			nr_unisolated++;
			CORE_CTL_INFO("Unisolate CPU%u success\n", c->cpu);
		} else {
			CORE_CTL_INFO("Unable to unisolate CPU%u\n", c->cpu);
		}

		spin_lock_irqsave(&state_lock, flags);

		cluster->active_cpus = get_active_cpu_count(cluster);
	}

	cluster->nr_isolated_cpus -= nr_unisolated;
	cluster->settype = -1;

	/*reset core ctrl conditon if active_cpus!=need_cpus*/
	if (unlikely(cluster->need_cpus != cluster->active_cpus))
		cluster->need_cpus = cluster->active_cpus;

	spin_unlock_irqrestore(&state_lock, flags);

	CORE_CTL_INFO("END %s:active_cpus=%u,need_cpus=%u\n",
		      __func__, cluster->active_cpus, cluster->need_cpus);
}

static void __ref do_core_ctl(struct cluster_data *cluster)
{
	unsigned int need = cluster->need_cpus;

	if (cluster->active_cpus > need) {
		CORE_CTL_INFO("start exec try_to_isolate func\n");
		try_to_isolate(cluster, need);
	} else if (cluster->active_cpus < need) {
		CORE_CTL_INFO("start exec try_to_unisolate func\n");
		try_to_unisolate(cluster, need);
	}
}

static int __ref try_core_ctl(void *data)
{
	struct cluster_data *cluster = data;
	unsigned long flags;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&cluster->pending_lock, flags);
		if (!cluster->pending) {
			spin_unlock_irqrestore(&cluster->pending_lock, flags);
			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&cluster->pending_lock, flags);
		}
		set_current_state(TASK_RUNNING);
		cluster->pending = false;
		spin_unlock_irqrestore(&cluster->pending_lock, flags);
		do_core_ctl(cluster);
	}

	return 0;
}

static int __ref cpuhp_core_ctl_online(unsigned int cpu)
{
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = state->cluster;
	unsigned long flags;

	if (unlikely(!cluster))
		return -ENODEV;

	if (unlikely(!cluster->inited))
		return 0;

	/*
	 * Moving to the end of the list should only happen in
	 * CPU_ONLINE and not on CPU_UP_PREPARE to prevent an
	 * infinite list traversal when thermal (or other entities)
	 * reject trying to online CPUs.
	 */
	spin_lock_irqsave(&state_lock, flags);
	move_cpu_lru(state, false);
	cluster->active_cpus = get_active_cpu_count(cluster);
	cluster->need_cpus = cluster->active_cpus;
	spin_unlock_irqrestore(&state_lock, flags);

	return 0;
}

static int __ref cpuhp_core_ctl_offline(unsigned int cpu)
{
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = state->cluster;
	unsigned long flags;

	if (unlikely(!cluster))
		return -ENODEV;

	if (unlikely(!cluster->inited))
		return 0;

	/*
	 * We don't want to have a CPU both offline and isolated.
	 * So unisolate a CPU that went down if it was isolated by us.
	 */
	if (state->isolated_by_us) {
		sched_unisolate_cpu_unlocked(cpu);
		state->isolated_by_us = false;
		cluster->nr_isolated_cpus--;
	} else {
		/* Move a CPU to the end of the LRU when it goes offline. */
		move_cpu_lru(state, true);
	}

	spin_lock_irqsave(&state_lock, flags);
	cluster->active_cpus = get_active_cpu_count(cluster);
	cluster->need_cpus = cluster->active_cpus;
	spin_unlock_irqrestore(&state_lock, flags);

	return 0;
}


/* ============================ init code ============================== */
static void insert_cluster_by_cap(struct cluster_data *cluster)
{
	struct cluster_data *temp;

	/* bigger capacity first */
	list_for_each_entry(temp, &cluster_list, cluster_node) {
		if (temp->capacity < cluster->capacity) {
			list_add_tail(&cluster->cluster_node,
				      &temp->cluster_node);
			return;
		}
	}

	list_add_tail(&cluster->cluster_node, &cluster_list);
}

static struct cluster_data *find_cluster_by_first_cpu(unsigned int first_cpu)
{
	struct cluster_data *temp;

	list_for_each_entry(temp, &cluster_list, cluster_node) {
		if (temp->first_cpu == first_cpu)
			return temp;
	}

	return NULL;
}

static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	struct task_struct *thread;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	int cpu;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;

	/* cluster data has been inited */
	if (find_cluster_by_first_cpu(first_cpu))
		return 0;

	dev = get_cpu_device(first_cpu);
	if (!dev) {
		CORE_CTL_ERR("core_ctl: fail to get cpu device\n");
		return -ENODEV;
	}

	CORE_CTL_INFO("Creating CPU group %d\n", first_cpu);

	cluster = devm_kzalloc(dev, sizeof(*cluster), GFP_KERNEL);
	if (!cluster) {
		CORE_CTL_ERR("core_ctl: alloc cluster err\n");
		return -ENOMEM;
	}

	cpumask_copy(&cluster->cpu_mask, mask);
	cluster->num_cpus = cpumask_weight(mask);
	cluster->first_cpu = first_cpu;
	cluster->max_cpus = cluster->num_cpus;
	cluster->need_cpus = cluster->num_cpus;
	cluster->nr_isolated_cpus = 0;
	cluster->enable = true;
	cluster->id = topology_physical_package_id(first_cpu);
	cluster->min_cpus = (cluster->id == 0) ? 1 : 0;
	cluster->settype = -1;
	cluster->isolate_cpu_id = -1;
	cluster->unisolate_cpu_id = -1;
	cluster->capacity = capacity_orig_of(first_cpu);
	INIT_LIST_HEAD(&cluster->lru);
	spin_lock_init(&cluster->pending_lock);

	for_each_cpu(cpu, mask) {
		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
		list_add(&state->sib, &cluster->lru);
	}
	cluster->active_cpus = get_active_cpu_count(cluster);

	thread = kthread_run(try_core_ctl, (void *) cluster,
			     "core_ctl/%d", first_cpu);
	if (IS_ERR(thread)) {
		CORE_CTL_ERR("core_ctl: thread create err\n");
		return PTR_ERR(thread);
	}

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		CORE_CTL_ERR("failed set SCHED_FIFO in cluster%u\n",
			     cluster->id);
		return ret;
	}

	cluster->core_ctl_thread = thread;

	wake_up_process(thread);

	cluster->inited = true;

	insert_cluster_by_cap(cluster);

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

static int __init core_ctl_init(void)
{
	int cpu;

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "core_ctl:online",
				  cpuhp_core_ctl_online, NULL);

	cpuhp_setup_state_nocalls(CPUHP_CORE_CTL_ISOLATION_DEAD,
				  "core_ctl:dead", NULL,
				  cpuhp_core_ctl_offline);
	CORE_CTL_INFO("sprd_core_ctl feature start init\n");
	cpu_maps_update_begin();
	for_each_online_cpu(cpu) {
		int ret;

		ret = cluster_init(topology_core_cpumask(cpu));
		if (ret)
			CORE_CTL_ERR("create core ctl group%d failed: %d\n",
			       cpu, ret);
	}
	cpu_maps_update_done();
	initialized = true;
	return 0;
}

late_initcall(core_ctl_init);
