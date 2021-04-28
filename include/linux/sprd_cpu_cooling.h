/*
 * Copyright (C) 2012-2015 Spreadtrum Communications Inc.
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
#ifndef __SPRD_CPU_COOLING_H__
#define __SPRD_CPU_COOLING_H__

#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/cpumask.h>
#include <linux/sprd_cpu_device.h>
#include <linux/pm_qos.h>

typedef int (*get_static_t)(cpumask_t *cpumask, int interval,
			unsigned long voltage, u32 *power, int temperature);
typedef int (*get_core_static_t)(cpumask_t *cpumask, int interval,
			unsigned long voltage, u32 *power, int temperature);
#define HOTPLUG_CLUSTER_NUM(cluster_id) \
	(PM_QOS_CLUSTER0_CORE_MAX+(cluster_id*2))
/*
 * Cooling state <-> CPUFreq frequency
 *
 * Cooling states are translated to frequencies throughout this driver and this
 * is the relation between them.
 *
 * Highest cooling state corresponds to lowest possible frequency.
 *
 * i.e.
 *	level 0 --> 1st Max Freq
 *	level 1 --> 2nd Max Freq
 *	...
 */

/**
 * struct power_table - frequency to power conversion
 * @frequency:	frequency in KHz
 * @power:	power in mW
 *
 * This structure is built when the cooling device registers and helps
 * in translating frequency to power and viceversa.
 */

struct power_table {
	u32 frequency;
	u32 power;
};

struct online_data {
	u32 target_online_cpus;
	u32 rounds;
};

/**
 * struct cpufreq_cooling_device - data for cooling device with cpufreq
 * @id: unique integer value corresponding to each cpufreq_cooling_device
 *  registered.
 * @cool_dev: thermal_cooling_device pointer to keep track of the
 *  registered cooling device.
 * @cpufreq_state: integer value representing the current state of cpufreq
 *  cooling devices.
 * @cpufreq_val: integer value representing the absolute value of the clipped
 *  frequency.
 * @max_level: maximum cooling level. One less than total number of valid
 *  cpufreq frequencies.
 * @allowed_cpus: all the cpus involved for this cpufreq_cooling_device.
 * @node: list_head to link all cpufreq_cooling_device together.
 * @last_load: load measured by the latest call to cpufreq_get_actual_power()
 * @time_in_idle: previous reading of the absolute time that this cpu was idle
 * @time_in_idle_timestamp: wall time of the last invocation of
 *  get_cpu_idle_time_us()
 * @dyn_power_table: array of struct power_table for frequency to power
 *  conversion, sorted in ascending order.
 * @dyn_power_table_entries: number of entries in the @dyn_power_table array
 * @cpu_dev: the first cpu_device from @allowed_cpus that has OPPs registered
 * @plat_get_static_power: callback to calculate the static power
 *
 * This structure is required for keeping information of each registered
 * cpufreq_cooling_device.
 */
struct cpufreq_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	unsigned int cpufreq_state;
	unsigned int clipped_freq;
	unsigned int max_level;
	unsigned int *freq_table;	/* In descending order */
	struct cpumask allowed_cpus;
#ifdef CONFIG_SPRD_CPU_COOLING_CPUIDLE
	struct cpumask idle_cpus;
	struct cpumask active_cpus;
	struct delayed_work idle_work;
#endif
	struct list_head node;
	u32 last_load;
	u64 *time_in_idle;
	u64 *time_in_idle_timestamp;
	struct power_table *dyn_power_table;
	struct power_table *dyn_l2_power_table;
	int dyn_power_table_entries;
	struct device *cpu_dev;
	u32 hotplug_refractory_period;
	get_static_t plat_get_static_power;
	get_core_static_t plat_get_core_static_power;
	struct online_data online_data;
	struct cpu_power_model_t *power_model;
	unsigned int qos_cur_cpu;
	struct pm_qos_request max_cpu_request;
	unsigned int curr_max_freq;
};

#ifdef CONFIG_SPRD_CPU_COOLING
/**
 * cpufreq_cooling_register - function to create cpufreq cooling device.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
struct thermal_cooling_device *
cpufreq_cooling_register(const struct cpumask *clip_cpus);

struct thermal_cooling_device *
cpufreq_power_cooling_register(const struct cpumask *clip_cpus,
				u32 hotplug_period,
				struct cpu_power_model_t *power_model);
#ifdef CONFIG_THERMAL_OF
/**
 * of_cpufreq_cooling_register - create cpufreq cooling device based on DT.
 * @np: a valid struct device_node to the cooling device device tree node.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
struct thermal_cooling_device *
of_cpufreq_cooling_register(struct device_node *np,
			    const struct cpumask *clip_cpus);

struct thermal_cooling_device *
of_cpufreq_power_cooling_register(struct device_node *np,
				  const struct cpumask *clip_cpus,
				  u32 hotplug_period,
				  struct cpu_power_model_t *power_model);
#else
static inline struct thermal_cooling_device *
of_cpufreq_cooling_register(struct device_node *np,
			    const struct cpumask *clip_cpus)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct thermal_cooling_device *
of_cpufreq_power_cooling_register(struct device_node *np,
				  const struct cpumask *clip_cpus,
				  u32 hotplug_period,
				  struct cpu_power_model_t *power_model)
{
	return NULL;
}
#endif

/**
 * cpufreq_cooling_unregister - function to remove cpufreq cooling device.
 * @cdev: thermal cooling device pointer.
 */
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev);

unsigned long cpufreq_cooling_get_level(unsigned int cpu, unsigned int freq);
struct cpufreq_cooling_device *
cpufreq_cooling_get_dev_by_name(const char *name);
#else /* !CONFIG_SPRD_CPU_COOLING */
static inline struct thermal_cooling_device *
cpufreq_cooling_register(const struct cpumask *clip_cpus)
{
	return ERR_PTR(-ENOSYS);
}
static inline struct thermal_cooling_device *
cpufreq_power_cooling_register(const struct cpumask *clip_cpus,
			       u32 hotplug_period,
			       struct cpu_power_model_t *power_model)
{
	return NULL;
}

static inline struct thermal_cooling_device *
of_cpufreq_cooling_register(struct device_node *np,
			    const struct cpumask *clip_cpus)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct thermal_cooling_device *
of_cpufreq_power_cooling_register(struct device_node *np,
				  const struct cpumask *clip_cpus,
				  u32 hotplug_period,
				  struct cpu_power_model_t *power_model)
{
	return NULL;
}

static inline
void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
static inline
unsigned long cpufreq_cooling_get_level(unsigned int cpu, unsigned int freq)
{
	return THERMAL_CSTATE_INVALID;
}
#endif	/* CONFIG_SPRD_CPU_COOLING */

#endif /* __SPRD_CPU_COOLING_H__ */
