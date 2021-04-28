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
#define pr_fmt(fmt) "sprd_cpu_device: " fmt

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>

#include <linux/sprd_cpu_cooling.h>
#include <linux/sprd_cpu_device.h>
#include <linux/sched.h>
#include <linux/cpumask.h>

#include <linux/cpu.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>

#if defined(CONFIG_OTP_SPRD_AP_EFUSE)
#include <linux/sprd_otp.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
#include <trace/events/thermal.h>

#define MAX_SENSOR_NUMBER	8
static atomic_t in_suspend;

/*
 * Tscale = aT^3 + bT^2 + cT + d
 * Vscale = aV^3 + bV^2 + cV + d
 */
struct scale_coeff {
	int scale_a;
	int scale_b;
	int scale_c;
	int scale_d;
};

/* Pdyn = dynperghz * freq * (V/Vbase)^2 */
struct dyn_power_coeff {
	int dynperghz;
	int freq;
	int voltage_base;
};

struct cluster_power_coefficients {
	u32 hotplug_period;
	u32 min_cpufreq;
	u32 min_cpunum;
	u32 resistance_ja;
	u32 temp_point;
	u32 cpuidle_tp[MAX_SENSOR_NUMBER];
	int leak_core_base;
	int leak_cluster_base;
	struct scale_coeff core_temp_scale;
	struct scale_coeff core_voltage_scale;
	struct scale_coeff cluster_temp_scale;
	struct scale_coeff cluster_voltage_scale;
	struct dyn_power_coeff core_coeff;
	struct dyn_power_coeff cluster_coeff;
	int weight;
	void *devdata;
	int nsensor;
	const char *sensor_names[MAX_SENSOR_NUMBER];
	struct thermal_zone_device *thm_zones[MAX_SENSOR_NUMBER];
	int core_temp[MAX_SENSOR_NUMBER];
};

struct cluster_power_coefficients *cluster_data;

#define FRAC_BITS 10
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

#define SPRD_CPU_STORE(_name) \
	static ssize_t sprd_cpu_store_##_name(struct device *dev, \
			struct device_attribute *attr, \
			const char *buf, size_t count)
#define SPRD_CPU_SHOW(_name) \
	static ssize_t sprd_cpu_show_##_name(struct device *dev, \
			struct device_attribute *attr, \
			char *buf)

/* sys I/F for cooling device */
#define to_cooling_device(_dev)	\
	container_of(_dev, struct thermal_cooling_device, device)

#define SPRD_CPU_ATTR(_name) \
{ \
	.attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP,}, \
	.show = sprd_cpu_show_##_name, \
	.store = sprd_cpu_store_##_name, \
}
#define SPRD_CPU_ATTR_RO(_name) \
{ \
	.attr = { .name = #_name, .mode = S_IRUGO, }, \
	.show = sprd_cpu_show_##_name, \
}
#define SPRD_CPU_ATTR_WO(_name) \
{ \
	.attr = { .name = #_name, .mode = S_IWUSR | S_IWGRP, }, \
	.store = sprd_cpu_store_##_name, \
}

SPRD_CPU_SHOW(min_freq);
SPRD_CPU_STORE(min_freq);
SPRD_CPU_SHOW(min_core_num);
SPRD_CPU_STORE(min_core_num);
static struct device_attribute sprd_cpu_atrr[] = {
	SPRD_CPU_ATTR(min_freq),
	SPRD_CPU_ATTR(min_core_num),
};

static int sprd_cpu_creat_attr(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sprd_cpu_atrr); i++) {
		rc = device_create_file(dev, &sprd_cpu_atrr[i]);
		if (rc)
			goto sprd_attrs_failed;
	}
	goto sprd_attrs_succeed;

sprd_attrs_failed:
	while (i--)
		device_remove_file(dev, &sprd_cpu_atrr[i]);

sprd_attrs_succeed:
	return rc;
}

static int sprd_cpu_remove_attr(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_cpu_atrr); i++)
		device_remove_file(dev, &sprd_cpu_atrr[i]);
	return 0;
}

/**
 * mul_frac() - multiply two fixed-point numbers
 * @x:	first multiplicand
 * @y:	second multiplicand
 *
 * Return: the result of multiplying two fixed-point numbers.  The
 * result is also a fixed-point number.
 */
static inline s64 mul_frac(s64 x, s64 y)
{
	return (x * y) >> FRAC_BITS;
}

/* return (leak * 100)  */
static int get_cpu_static_power_coeff(int cluster_id)
{
	return cluster_data[cluster_id].leak_core_base;
}

/* return (leak * 100)  */
static int get_cache_static_power_coeff(int cluster_id)
{
	return cluster_data[cluster_id].leak_cluster_base;
}

static ssize_t sprd_cpu_show_min_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpufreq_cooling_device *cpufreq_dev = cdev->devdata;

	return sprintf(buf, "%u\n",
		cluster_data[cpufreq_dev->power_model->cluster_id].min_cpufreq);
}

static ssize_t sprd_cpu_store_min_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpufreq_cooling_device *cpufreq_dev = cdev->devdata;
	unsigned long val;
	int i;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	for (i = 0; i <= cpufreq_dev->max_level; i++) {
		if (val == cpufreq_dev->freq_table[i]) {
			cluster_data[cpufreq_dev->power_model->
				cluster_id].min_cpufreq = val;
			return count;
		}
	}

	return -EINVAL;
}

static ssize_t sprd_cpu_show_min_core_num(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpufreq_cooling_device *cpufreq_dev = cdev->devdata;

	return sprintf(buf, "%u\n",
		cluster_data[cpufreq_dev->power_model->cluster_id].min_cpunum);
}

static ssize_t sprd_cpu_store_min_core_num(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpufreq_cooling_device *cpufreq_dev = cdev->devdata;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	cluster_data[cpufreq_dev->power_model->cluster_id].min_cpunum = val;
	return count;
}

static int get_all_core_temp(int cluster_id, int cpu)
{
	int i, ret;
	struct thermal_zone_device *tz = NULL;
	struct cluster_power_coefficients *cpc;

	cpc = &cluster_data[cluster_id];
	for (i = 0; i < (cpc->nsensor); i++) {
		tz = cpc->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp) {
			pr_err("get thermal zone failed\n");
			return -1;
		}

		ret = tz->ops->get_temp(tz, &(cpc->core_temp[cpu+i]));
		if (ret) {
			pr_err("get thermal %s temp failed\n", tz->type);
			return -1;
		}

		pr_debug("%s:%d\n", tz->type, cpc->core_temp[cpu+i]);
    }

	return ret;
}

static void get_core_temp(int cluster_id, int cpu, int *temp)
{
	struct cluster_power_coefficients *cpc;

	cpc = &cluster_data[cluster_id];
	*temp = cpc->core_temp[cpu];

}

#if defined(CONFIG_SPRD_CPU_COOLING_CPUIDLE) && defined(CONFIG_SPRD_CORE_CTL)
static int get_min_temp_isolated_core(int cluster_id, int cpu, int *temp)
{
	int i, ret, min_temp = 0, id = -1, first, find;
	struct thermal_zone_device *tz = NULL;
	struct cluster_power_coefficients *cpc;
	int sensor_temp[MAX_SENSOR_NUMBER];

	cpc = &cluster_data[cluster_id];
	for (i = 0; i < (cpc->nsensor); i++) {
		tz = cpc->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp) {
			pr_err("get thermal zone failed\n");
			return -1;
		}

		ret = tz->ops->get_temp(tz, &sensor_temp[cpu+i]);
		if (ret) {
			pr_err("get thermal %s temp failed\n", tz->type);
			return -1;
		}

		pr_debug("%s:%d\n", tz->type, sensor_temp[cpu+i]);
	}

	for (first = cpu; first < (cpu+cpc->nsensor); first++) {
		if (cpu_isolated(first)) {
			min_temp = sensor_temp[first];
			id = first;
			*temp = min_temp;
			find = 1;
			break;
		}
	}

	if (find) {
		for (i = cpu; i < (cpu+cpc->nsensor); i++) {
			if (cpu_isolated(i)) {
				if (sensor_temp[i] < min_temp) {
					min_temp = sensor_temp[i];
					id = i;
					*temp = min_temp;
				}
			}
		}
		pr_debug("isolated cpu%d:min_temp:%d\n", id, *temp);
	} else
		id = -1;

	return id;
}

static int get_min_temp_unisolated_core(int cluster_id, int cpu, int *temp)
{
	int i, ret, min_temp = 0, id = -1, first, find;
	struct thermal_zone_device *tz = NULL;
	struct cluster_power_coefficients *cpc;
	int sensor_temp[MAX_SENSOR_NUMBER];

	cpc = &cluster_data[cluster_id];
	for (i = 0; i < (cpc->nsensor); i++) {
		tz = cpc->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp) {
			pr_err("get thermal zone failed\n");
			return -1;
		}

		ret = tz->ops->get_temp(tz, &sensor_temp[cpu+i]);
		if (ret) {
			pr_err("get thermal %s temp failed\n", tz->type);
			return -1;
		}

		pr_debug("%s:%d\n", tz->type, sensor_temp[cpu+i]);
	}

	for (first = cpu; first < (cpu+cpc->nsensor); first++) {
		if (!cpu_isolated(first)) {
			min_temp = sensor_temp[first];
			id = first;
			*temp = min_temp;
			find = 1;
			break;
		}
	}

	if (find) {
		for (i = cpu; i < (cpu+cpc->nsensor); i++) {
			if (!cpu_isolated(i)) {
				if (sensor_temp[i] <= min_temp) {
					min_temp = sensor_temp[i];
					id = i;
					*temp = min_temp;
				}
			}
		}
		pr_debug("unisolated cpu%d:min_temp:%d\n", id, *temp);

	} else
		id = -1;

	return id;
}
#endif

static u32 get_core_cpuidle_tp(int cluster_id,
		int first_cpu, int cpu, int *temp)
{
	int i, id = first_cpu;
	struct cluster_power_coefficients *cpc;

	cpc = &cluster_data[cluster_id];
	for (i = 0; i < cpc->nsensor; i++, id++) {
		if (id == cpu) {
			*temp = cpc->cpuidle_tp[i];
			break;
		}
	}

	return id;
}

u64 get_core_dyn_power(int cluster_id,
	unsigned int freq_mhz, unsigned int voltage_mv)
{
	u64 power = 0;
	int voltage_base = cluster_data[cluster_id].core_coeff.voltage_base;
	int dyn_base = cluster_data[cluster_id].core_coeff.dynperghz;

	power = (u64)dyn_base * freq_mhz * voltage_mv * voltage_mv;
	do_div(power, voltage_base * voltage_base);
	do_div(power, 10000);

	pr_debug("cluster:%d core_dyn_p:%u freq:%u voltage:%u voltage_base:%d\n",
		cluster_id, (u32)power, freq_mhz, voltage_mv, voltage_base);

	return power;
}

static u32 get_cpuidle_temp_point(int cluster_id)
{
	return cluster_data[cluster_id].temp_point;
}

u32 get_cluster_min_cpufreq(int cluster_id)
{
	return cluster_data[cluster_id].min_cpufreq;
}

u32 get_cluster_min_cpunum(int cluster_id)
{
	return cluster_data[cluster_id].min_cpunum;
}

u32 get_cluster_resistance_ja(int cluster_id)
{
	return cluster_data[cluster_id].resistance_ja;
}

u64 get_cluster_dyn_power(int cluster_id,
	unsigned int freq_mhz, unsigned int voltage_mv)
{
	u64 power = 0;
	int voltage_base = cluster_data[cluster_id].cluster_coeff.voltage_base;
	int dyn_base = cluster_data[cluster_id].cluster_coeff.dynperghz;

	power = (u64)dyn_base * freq_mhz * voltage_mv * voltage_mv;
	do_div(power, voltage_base * voltage_base);
	do_div(power, 10000);

	pr_debug("cluster:%d cluster_dyn_power:%u freq:%u voltage:%u voltage_base:%d\n",
		cluster_id, (u32)power, freq_mhz, voltage_mv, voltage_base);

	return power;
}

/*
 *Tscale = 0.0000825T^3 - 0.0117T^2 + 0.608T - 8.185
 * return Tscale * 1000
 */
static u64 get_cluster_temperature_scale(int cluster_id, unsigned long temp)
{
	u64 t_scale = 0;
	struct scale_coeff *coeff =
		&cluster_data[cluster_id].cluster_temp_scale;

	t_scale = coeff->scale_a * temp * temp * temp
		+ coeff->scale_b * temp * temp
		+ coeff->scale_c * temp
		+ coeff->scale_d;

	do_div(t_scale, 10000);

	return t_scale;
}

/*
 *Tscale = 0.0000825T^3 - 0.0117T^2 + 0.608T - 8.185
 * return Tscale * 1000
 */
static u64 get_core_temperature_scale(int cluster_id, unsigned long temp)
{
	u64 t_scale = 0;
	struct scale_coeff *coeff = &cluster_data[cluster_id].core_temp_scale;

	t_scale = coeff->scale_a * temp * temp * temp
		+ coeff->scale_b * temp * temp
		+ coeff->scale_c * temp
		+ coeff->scale_d;

	do_div(t_scale, 10000);

	return t_scale;
}

/*
 * Vscale = eV^3 + fV^2 + gV + h
 * Vscale = 33.31V^3 - 73.25V^2 + 54.44V - 12.81
 * return Vscale * 1000
 */
static u64 get_cluster_voltage_scale(int cluster_id, unsigned long u_volt)
{
	unsigned long m_volt = u_volt / 1000;
	u64 v_scale = 0;
	int cubic = 0, square = 0, common = 0, data = 0;
	struct scale_coeff *coeff =
		&cluster_data[cluster_id].cluster_voltage_scale;

	/* In order to ensure accuracy of data and data does not overflow.
	 * exam: cubic = eV^3 = e * mV^3 * 10^(-9)  For return Vscale * 1000,
	 * we use div 10^6.Because of parameter 'e' is bigger(10^2) than nomal
	 * In the last divided by 10^2.
	 */
	/* In order to avoid the computational problem caused by the error.*/
	cubic = (m_volt * m_volt * m_volt) / 1000000;
	cubic = coeff->scale_a * cubic;

	square = (m_volt * m_volt) / 1000;
	square = coeff->scale_b * square;

	common = coeff->scale_c * m_volt;

	data = coeff->scale_d * 1000;

	v_scale = (u64)(cubic + square + common + data);

	do_div(v_scale, 100);

	return v_scale;
}

/*
 * Vscale = eV^3 + fV^2 + gV + h
 * Vscale = 33.31V^3 - 73.25V^2 + 54.44V - 12.81
 * return Vscale * 1000
 */
static u64 get_core_voltage_scale(int cluster_id, unsigned long u_volt)
{
	unsigned long m_volt = u_volt / 1000;
	u64 v_scale = 0;
	int cubic = 0, square = 0, common = 0, data = 0;
	struct scale_coeff *coeff =
		&cluster_data[cluster_id].core_voltage_scale;

	/* In order to ensure accuracy of data and data does not overflow.
	 * exam: cubic = eV^3 = e * mV^3 * 10^(-9)  For return Vscale * 1000,
	 * we use div 10^6.Because of parameter 'e' is bigger(10^2) than nomal
	 * In the last divided by 10^2.
	 */
	/* In order to avoid the computational problem caused by the error.*/
	cubic = (m_volt * m_volt * m_volt) / 1000000;
	cubic = coeff->scale_a * cubic;

	square = (m_volt * m_volt) / 1000;
	square = coeff->scale_b * square;

	common = coeff->scale_c * m_volt;

	data = coeff->scale_d * 1000;

	v_scale = (u64)(cubic + square + common + data);

	do_div(v_scale, 100);

	return v_scale;
}

static int get_cluster_id(int cpu)
{
#if defined(CONFIG_X86_SPRD_ISOC)
	return ((cpu) < 4 ? 0 : 1);
#else
	return topology_physical_package_id((cpu));
#endif
}

/* voltage in uV and temperature in mC */
static int get_static_power(cpumask_t *cpumask, int interval,
		unsigned long u_volt, u32 *power, int temperature)
{
	unsigned long core_t_scale, core_v_scale;
	unsigned long cluster_t_scale = 0, cluster_v_scale = 0;
	u32 cpu_coeff;
	u32 tmp_power = 0;
	int nr_cpus = cpumask_weight(cpumask);
	int cache_coeff = 0;
	int cluster_id =
		get_cluster_id(cpumask_any(cpumask));

	/* get coeff * 100 */
	cpu_coeff = get_cpu_static_power_coeff(cluster_id);
	/* get Tscale * 1000 */
	core_t_scale =
		get_core_temperature_scale(cluster_id, temperature / 1000);
	/* get Vscale * 1000 */
	core_v_scale =
		get_core_voltage_scale(cluster_id, u_volt);

	/* In order to avoid the computational problem caused by the error.*/
	if ((core_t_scale * core_v_scale) > 1000000) {
		tmp_power = (core_t_scale * core_v_scale) / 1000000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 100;
	} else {
		tmp_power = (core_t_scale * core_v_scale) / 100000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 1000;
	}

	if (nr_cpus) {
		/* get cluster-Tscale * 1000 */
		cluster_t_scale = get_cluster_temperature_scale(cluster_id,
							temperature / 1000);
		/* get cluster-Vscale * 1000 */
		cluster_v_scale = get_cluster_voltage_scale(cluster_id, u_volt);
		/* get coeff * 100 */
		cache_coeff = get_cache_static_power_coeff(cluster_id);
		if ((cluster_v_scale * cluster_t_scale) > 1000000) {
			tmp_power =
				(cluster_v_scale * cluster_t_scale) / 1000000;
			*power += (cache_coeff * tmp_power) / 100;
		} else {
			tmp_power =
				(cluster_v_scale * cluster_t_scale) / 100000;
			*power += (cache_coeff * tmp_power) / 1000;
		}
	}

	pr_debug("cluster:%d cpus:%d m_volt:%lu static_power:%u\n",
		cluster_id, nr_cpus, u_volt / 1000, *power);
	pr_debug("-->cpu_coeff:%d core_t_scale:%lu core_v_scale:%lu\n",
		cpu_coeff, core_t_scale, core_v_scale);
	pr_debug("-->cache_coeff:%d cluster_t_scale:%lu cluster_v_scale:%lu\n",
		cache_coeff, cluster_t_scale, cluster_v_scale);

	return 0;
}

/* voltage in uV and temperature in mC */
static int get_core_static_power(cpumask_t *cpumask, int interval,
		unsigned long u_volt, u32 *power, int temperature)
{
	unsigned long core_t_scale, core_v_scale;
	u32 cpu_coeff;
	u32 tmp_power = 0;
	int nr_cpus = cpumask_weight(cpumask);
	int cluster_id =
		get_cluster_id(cpumask_any(cpumask));

	/* get coeff * 100 */
	cpu_coeff = get_cpu_static_power_coeff(cluster_id);
	/* get Tscale * 1000 */
	core_t_scale =
		get_core_temperature_scale(cluster_id, temperature / 1000);
	/* get Vscale * 1000 */
	core_v_scale =
		get_core_voltage_scale(cluster_id, u_volt);

	/* In order to avoid the computational problem caused by the error.*/
	if ((core_t_scale * core_v_scale) > 1000000) {
		tmp_power = (core_t_scale * core_v_scale) / 1000000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 100;
	} else {
		tmp_power = (core_t_scale * core_v_scale) / 100000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 1000;
	}

	pr_debug("cluster:%d cpus:%d m_volt:%lu core_static_power:%u\n",
		cluster_id, nr_cpus, u_volt / 1000, *power);
	pr_debug("-->cpu_coeff:%d core_t_scale:%lu core_v_scale:%lu\n",
		cpu_coeff, core_t_scale, core_v_scale);

	return 0;
}
/* return (leakage * 10) */
static u64 get_leak_base(int cluster_id, int val, int *coeff)
{
	int i;
	u64 leak_base;

	if (cluster_id)
		leak_base = ((val>>16) & 0x1F) + 1;
	else
		leak_base = ((val>>11) & 0x1F) + 1;

	/* (LIT_LEAK[4:0]+1) x 2mA x 0.85V x 18.69% */
	for (i = 0; i < 3; i++)
		leak_base = leak_base * coeff[i];
	do_div(leak_base, 100000);

	return leak_base;
}

static int sprd_get_power_model_coeff(struct device_node *np,
		struct cluster_power_coefficients *power_coeff, int cluster_id)
{
	int ret;
	int val = 0;
	int efuse_block = -1;
	int efuse_switch = 0;
	int coeff[3];
	int count, i;

	if (!np) {
		pr_err("device node not found\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,efuse-block15", &efuse_block);
	if (ret) {
		pr_err("fail to get cooling devices efuse_block\n");
		efuse_block = -1;
	}

	ret = of_property_read_u32(np, "sprd,efuse-switch", &efuse_switch);
	if (ret)
		pr_err("fail to get cooling devices efuse_switch\n");

	if (efuse_switch) {
#if defined(CONFIG_OTP_SPRD_AP_EFUSE)
		if (efuse_block >= 0)
			val = sprd_ap_efuse_read(efuse_block);
#endif

		pr_debug("sci_efuse_leak --val : %x\n", val);
		if (val) {
			ret = of_property_read_u32_array(np,
					"sprd,leak-core", coeff, 3);
			if (ret) {
				pr_err("fail to get cooling devices leak-core-coeff\n");
				return -EINVAL;
			}

			power_coeff->leak_core_base =
				get_leak_base(cluster_id, val, coeff);

			ret = of_property_read_u32_array(np,
				"sprd,leak-cluster", coeff, 3);
			if (ret) {
				pr_err("fail to get cooling devices leak-cluster-coeff\n");
				return -EINVAL;
			}

			power_coeff->leak_cluster_base =
				get_leak_base(cluster_id, val, coeff);
		}
	}

	if (!val) {
		ret = of_property_read_u32(np, "sprd,core-base",
				&power_coeff->leak_core_base);
		if (ret) {
			pr_err("fail to get default cooling devices leak-core-base\n");
			return -EINVAL;
		}

		ret = of_property_read_u32(np, "sprd,cluster-base",
				&power_coeff->leak_cluster_base);
		if (ret) {
			pr_err("fail to get default cooling devices leak-cluster-base\n");
			return -EINVAL;
		}
	}

	ret = of_property_read_u32_array(np, "sprd,core-temp-scale",
			(int *)&power_coeff->core_temp_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices core-temp-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,core-voltage-scale",
			(int *)&power_coeff->core_voltage_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices core-voltage-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,cluster-temp-scale",
			(int *)&power_coeff->cluster_temp_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices cluster-temp-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,cluster-voltage-scale",
			(int *)&power_coeff->cluster_voltage_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices cluster_voltage-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,dynamic-core",
			(int *)&power_coeff->core_coeff,
			sizeof(struct dyn_power_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices dynamic-core-coeff\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,dynamic-cluster",
			(int *)&power_coeff->cluster_coeff,
			sizeof(struct dyn_power_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices dynamic-cluster-coeff\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np,
		"sprd,hotplug-period", &power_coeff->hotplug_period);
	if (ret)
		pr_err("fail to get cooling devices efuse_block\n");

	ret = of_property_read_u32(np,
		"sprd,min-cpufreq", &power_coeff->min_cpufreq);
	if (ret)
		pr_err("fail to get cooling devices min_cpufreq\n");

	ret = of_property_read_u32(np,
		"sprd,min-cpunum", &power_coeff->min_cpunum);
	if (ret)
		pr_err("fail to get cooling devices min_cpunum\n");

	ret = of_property_read_u32(np,
		"sprd,resistance-ja", &power_coeff->resistance_ja);
	if (ret)
		pr_err("fail to get cooling devices resistance-ja\n");

	count = of_property_count_strings(np, "sprd,sensor-names");
	if (count < 0) {
		pr_err("sensor names not found\n");
		return 0;
	}

	power_coeff->nsensor = count;
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sprd,sensor-names",
			i, &power_coeff->sensor_names[i]);
		if (ret)
			pr_err("fail to get sensor-names\n");
	}

	for (i = 0; i < power_coeff->nsensor; i++) {
		power_coeff->thm_zones[i] =
		thermal_zone_get_zone_by_name(
			power_coeff->sensor_names[i]);
		if (IS_ERR(power_coeff->thm_zones[i])) {
			pr_err("get thermal zone %s failed\n",
					power_coeff->sensor_names[i]);
		}
	}

	ret = of_property_read_u32_array(np, "sprd,cii-per-core-tp",
			power_coeff->cpuidle_tp, power_coeff->nsensor);
	if (ret)
		pr_err("fail to get cooling devices per-core-tp\n");

	ret = of_property_read_u32(np, "sprd,cii-max-tp-core",
			&power_coeff->temp_point);
	if (ret)
		pr_err("fail to get cooling devices max-tp-core\n");

	return 0;
}

static int cpu_cooling_pm_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		atomic_set(&in_suspend, 1);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		atomic_set(&in_suspend, 0);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block cpu_cooling_pm_nb = {
	.notifier_call = cpu_cooling_pm_notify,
};

static struct power_model_callback pm_call = {
		.get_core_dyn_power_p = get_core_dyn_power,
		.get_cluster_dyn_power_p = get_cluster_dyn_power,
		.get_static_power_p = get_static_power,
		.get_core_static_power_p = get_core_static_power,
		.get_cluster_min_cpufreq_p = get_cluster_min_cpufreq,
		.get_cluster_min_cpunum_p = get_cluster_min_cpunum,
		.get_cluster_resistance_ja_p = get_cluster_resistance_ja,

#if defined(CONFIG_SPRD_CPU_COOLING_CPUIDLE) && defined(CONFIG_SPRD_CORE_CTL)
		.get_min_temp_unisolated_core_p = get_min_temp_unisolated_core,
		.get_min_temp_isolated_core_p = get_min_temp_isolated_core,
#endif
		.get_core_temp_p = get_core_temp,
		.get_all_core_temp_p = get_all_core_temp,
		.get_core_cpuidle_tp_p = get_core_cpuidle_tp,
		.get_cpuidle_temp_point_p = get_cpuidle_temp_point,
};

int create_cpu_cooling_device(void)
{
	struct device_node *np, *child;
	int ret = 0;
	int cpu = 0;
	int result = 0;
	int cluster_count;
	struct cpumask cpu_online_check;
	struct device *dev = NULL;
	struct thermal_cooling_device *cool_dev = NULL;
	struct cpufreq_cooling_device *cfd = NULL;

	np = of_find_node_by_name(NULL, "cooling-devices");
	if (!np) {
		pr_err("unable to find thermal zones\n");
		return 0; /* Run successfully on systems without thermal DT */
	}

	cluster_count = of_get_child_count(np);

	cluster_data = kcalloc(cluster_count,
		sizeof(struct cluster_power_coefficients), GFP_KERNEL);
	if (cluster_data == NULL) {
		ret = -ENOMEM;
		goto ERR_RET;
	}

	for_each_child_of_node(np, child) {
		struct cpu_power_model_t *power_model_n;
		int cluster_id_n;

		/* Check whether child is enabled or not */
		if (!of_device_is_available(child))
			continue;

		cluster_id_n = of_alias_get_id(child, "cooling-device");
		if (cluster_id_n == -ENODEV) {
			pr_err("fail to get cooling devices id\n");
			goto free_cluster;
		}

		power_model_n = kzalloc(sizeof(*power_model_n), GFP_KERNEL);
		if (power_model_n == NULL) {
			ret = -ENOMEM;
			goto free_cluster;
		}

		for_each_possible_cpu(cpu) {
			int cluster_id;
			cluster_id = get_cluster_id(cpu);
			if (cluster_id > cluster_count) {
				pr_warn("cluster_id id: %d > %d\n",
					cluster_id, cluster_count);
				kfree(power_model_n);
				power_model_n = NULL;
				ret = -ENODEV;
				goto free_cluster;
			} else if (cluster_id == cluster_id_n)
				cpumask_set_cpu(cpu, &power_model_n->clip_cpus);
		}


		if (!cpumask_and(&cpu_online_check,
				&power_model_n->clip_cpus, cpu_online_mask)) {
			pr_warn("%s cpu offline unnormal\n", __func__);
			kfree(power_model_n);
			power_model_n = NULL;
			continue;
		}

		ret = sprd_get_power_model_coeff(child,
			&cluster_data[cluster_id_n], cluster_id_n);
		if (ret) {
			pr_err("fail to get power model coeff !\n");
			kfree(power_model_n);
			power_model_n = NULL;
			goto free_cluster;
		}

		strlcpy(power_model_n->type,
			child->name ? : "", sizeof(power_model_n->type));

		power_model_n->cluster_id = cluster_id_n;
		power_model_n->cab = &pm_call;

		cool_dev =
			of_cpufreq_power_cooling_register(child,
					&power_model_n->clip_cpus,
				cluster_data[cluster_id_n].hotplug_period,
			power_model_n);

		power_model_n->cdev = cool_dev;

		if (IS_ERR(power_model_n->cdev)) {
			pr_err("Error registering cooling device %d\n",
						cluster_id_n);
			kfree(power_model_n);
			power_model_n = NULL;
			continue;
		}

		for_each_cpu(cpu, &power_model_n->clip_cpus) {
			dev = get_cpu_device(cpu);
			if (!dev) {
				pr_err("No cpu device for cpu %d\n", cpu);
				continue;
			}
			if (dev_pm_opp_get_opp_count(dev) > 0)
				break;
		}

		if (cool_dev->devdata != NULL) {
			cfd = cool_dev->devdata;
			cfd->cpu_dev =  dev;
			sprd_cpu_creat_attr(&cool_dev->device);
		} else {
			pr_err("No cpufreq devices!\n");
			kfree(power_model_n);
			power_model_n = NULL;
			continue;
		}
	}

	result = register_pm_notifier(&cpu_cooling_pm_nb);
	if (result)
		pr_warn("Thermal: Can not register suspend notifier, return %d\n",
			result);

	return ret;

free_cluster:
	kfree(cluster_data);
	cluster_data = NULL;
ERR_RET:
	return ret;
}

int destroy_cpu_cooling_device(void)
{
	struct device_node *np, *child;

	unregister_pm_notifier(&cpu_cooling_pm_nb);

	np = of_find_node_by_name(NULL, "cooling-devices");
	if (!np) {
		pr_err("unable to find thermal zones\n");
		return -ENODEV;
	}

	for_each_child_of_node(np, child) {
		struct cpufreq_cooling_device *cpufreq_dev;

		cpufreq_dev = cpufreq_cooling_get_dev_by_name(child->name);
		if (IS_ERR(cpufreq_dev))
			continue;

		sprd_cpu_remove_attr(&cpufreq_dev->cool_dev->device);

		kfree(cpufreq_dev->power_model);
		cpufreq_cooling_unregister(cpufreq_dev->cool_dev);
	}

	kfree(cluster_data);
	cluster_data = NULL;

	return 0;
}

static int __init sprd_cpu_cooling_device_init(void)
{
	return create_cpu_cooling_device();
}

static void __exit sprd_cpu_cooling_device_exit(void)
{
	destroy_cpu_cooling_device();
}

late_initcall(sprd_cpu_cooling_device_init);
module_exit(sprd_cpu_cooling_device_exit);
