/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include "sprd-cpufreqhw.h"

static struct cpufreq_driver sprd_hardware_cpufreq_driver;
static unsigned long boot_done_timestamp;
static int boost_mode_flag = 1;
struct sprd_cpudvfs_device *plat_dev;
/*
 * sprd_hardware_dvfs_device_register - register hw dvfs module
 * @ops: hw dvfs operations
 *
 * Return EBUSY, means another hw dvfs is on ,Not
 * surpport multi-hw dvfs.
 */
int sprd_hardware_dvfs_device_register(struct platform_device *pdev)
{
	if (!pdev)
		return -ENODEV;

	if (plat_dev)
		return -EBUSY;

	plat_dev = platform_get_drvdata(pdev);
	if (!plat_dev) {
		pr_err("Device data has not been set.\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sprd_hardware_dvfs_device_register);
/*
 * sprd_hardware_dvfs_device_unregister - register hw dvfs module
 * @ops: hw dvfs operations
 *
 * Return EINVAL, means ops is not registered yet.
 */
int sprd_hardware_dvfs_device_unregister(struct platform_device *pdev)
{
	if (!pdev || !plat_dev || plat_dev != platform_get_drvdata(pdev))
		return -EINVAL;

	plat_dev = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_hardware_dvfs_device_unregister);

/**
 * sprd_get_hardware_cpufreq_device - register hw dvfs device
 * @ops: hw dvfs operations
 *
 * Return NULL, means ops is not registered yet.
 */
struct sprd_cpudvfs_device *sprd_hardware_dvfs_device_get(void)
{
	if (plat_dev && !plat_dev->archdata) {
		pr_err("No cpu dvfs private data found\n");
		return NULL;
	}

	return plat_dev;
}
EXPORT_SYMBOL_GPL(sprd_hardware_dvfs_device_get);

/**
 * sprd_cpufreq_set_boost: set cpufreq driver to be boost mode.
 *
 * @state: 0->disable boost mode;1->enable boost mode;
 *
 * Return: zero on success, otherwise non-zero on failure.
 */
int sprd_hardware_cpufreq_set_boost(int state)
{
	boot_done_timestamp = 0;
	boost_mode_flag = state;
	return 0;
}

/**
 * sprd_cpufreq_set_target_index()  - cpufreq_set_target
 * @idx:        0 points to min freq, ascending order
 */
static
int sprd_hardware_cpufreq_set_target_index(struct cpufreq_policy *policy,
					   unsigned int idx)
{
	struct sprd_cpudvfs_device *pdev;
	struct sprd_cpudvfs_ops *driver;
	unsigned long freq;
	u32 cpu_cluster;
	int ret;

	pdev = sprd_hardware_dvfs_device_get();
	if (!pdev) {
		pr_err("Hardware dvfs device has not been registered.\n");
		return -EINVAL;
	}
	driver = &pdev->ops;

	freq = policy->freq_table[idx].frequency;

	/* Never dvfs until boot_done_timestamp */
	if ((boot_done_timestamp &&
	     time_after(jiffies, boot_done_timestamp))) {
		sprd_hardware_cpufreq_set_boost(0);
		sprd_hardware_cpufreq_driver.boost_enabled = false;

		pr_info("Disables boost it is %lu seconds after boot up\n",
			SPRD_CPUFREQ_DRV_BOOST_DURATOIN / HZ);
	}
	 /*
	  * boost_mode_flag is true and cpu is on max freq
	  * so return 0 here, reject changing freq
	  */
	if (boost_mode_flag) {
		if (policy->max >= policy->cpuinfo.max_freq)
			return 0;
		sprd_hardware_cpufreq_set_boost(0);
		sprd_hardware_cpufreq_driver.boost_enabled = false;
		pr_info("Disables boost due to policy max(%d<%d)\n",
			policy->max, policy->cpuinfo.max_freq);
	}

	cpu_cluster = topology_physical_package_id(policy->cpu);

	if (!driver->probed || !driver->probed(pdev->archdata, cpu_cluster)) {
		pr_err("Platform cpu dvfs has not been probed.\n");
		return -EINVAL;
	}

	mutex_lock(cpufreq_datas[cpu_cluster]->volt_lock);
	ret = driver->set(pdev->archdata, cpu_cluster, idx);
	mutex_unlock(cpufreq_datas[cpu_cluster]->volt_lock);

	if (!ret)
		arch_set_freq_scale(policy->related_cpus,
				    freq, policy->cpuinfo.max_freq);

	return ret;
}

static int sprd_hardware_cpufreq_init_slaves(
	struct sprd_cpufreq_driver_data *c_host,
	struct device_node *np_host)
{
	struct sprd_cpufreq_driver_data *data;
	struct sprd_cpudvfs_device *pdev;
	struct sprd_cpudvfs_ops *driver;
	struct device_node *np;
	unsigned int cluster;
	int ret, i;

	if (!np_host)
		return -ENODEV;

	pdev = sprd_hardware_dvfs_device_get();
	if (!pdev) {
		pr_err("Hardware dvfs device has not been registered.\n");
		return -EINVAL;
	}
	driver = &pdev->ops;

	for (i = 0; i < SPRD_CPUFREQ_MAX_MODULE; i++) {
		np = of_parse_phandle(np_host,
				      "cpufreq-sub-clusters", i);
		if (!np) {
			pr_info("Cluster%d does not have sub-clusters\n",
				c_host->cluster);
			return 0;
		}

		pr_info("Slave cluster%d is found: %s\n", i, np->full_name);

		if (of_property_read_u32(np,
					 "cpufreq-cluster-id", &cluster)) {
			pr_err("Cluster id is not found in sub-cluster%d\n", i);
			ret = -EINVAL;
			goto free_np;
		}

		if (cluster >= SPRD_CPUFREQ_MAX_MODULE) {
			pr_err("Cluster id %d for cluster %u is overflowed\n",
			       cluster, i);
			ret = -EINVAL;
			goto free_np;
		}

		if (!cpufreq_datas[cluster]) {
			data = kzalloc(sizeof(*data), GFP_KERNEL);
			if (!data) {
				ret = -ENOMEM;
				goto free_np;
			} else {
				cpufreq_datas[cluster] = data;
			}
		} else {
			data = cpufreq_datas[cluster];
		}

		data->cluster = cluster;
		data->online = true;
		c_host->sub_cluster_bits |= (0x1 << cluster);

		/*
		 * For HW DVFS, It's not needed to get a volt lock
		 * for slave devices.
		 */
		ret = dev_pm_opp_of_add_table_binning(cluster,
						      NULL, np, data);
		if (ret)
			goto free_np;

		if (!(driver->probed && driver->enable)) {
			pr_err("Platform cpu dvfs driver is empty\n");
			goto free_np;
		}

		if (driver->probed(pdev->archdata, data->cluster))
			driver->enable(pdev->archdata, data->cluster, true);

		of_node_put(np);
	}

	return 0;

free_np:
	for_each_set_bit(i, &c_host->sub_cluster_bits,
			 SPRD_CPUFREQ_MAX_MODULE) {
		kfree(cpufreq_datas[i]);
		cpufreq_datas[i] = NULL;
	}

	if (np)
		of_node_put(np);

	return ret;
}

static int sprd_hardware_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;
	struct sprd_cpufreq_driver_data *data;
	struct device_node *cpufreq_of_node;
	struct sprd_cpudvfs_device *pdev;
	struct sprd_cpudvfs_ops *driver;
	struct device_node *cpu_np;
	unsigned long freq_Hz = 0;
	struct device *cpu_dev;
	int ret, cpu = 0;
	int curr_cluster;

	pdev = sprd_hardware_dvfs_device_get();

	if (!pdev) {
		pr_err("Hardware dvfs device has not been registered.\n");
		return -EINVAL;
	}
	driver = &pdev->ops;

	if (!policy) {
		pr_err("Invalid cpufreq policy\n");
		return -ENODEV;
	}

	cpu = policy->cpu;
	curr_cluster = topology_physical_package_id(cpu);
	if (curr_cluster >= SPRD_CPUFREQ_MAX_CLUSTER) {
		pr_err("Invalid CPU cluster number(%d)\n", curr_cluster);
		return -EINVAL;
	}

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("No cpu%d device exist\n", cpu);
		return -ENODEV;
	}

	cpu_np = of_node_get(cpu_dev->of_node);
	if (!cpu_np) {
		pr_err("Failed to find cpu%d node\n", cpu);
		return -ENOENT;
	}

	cpufreq_of_node = of_parse_phandle(cpu_np,
					   "cpufreq-data-v1", 0);
	if (!cpufreq_of_node) {
		pr_err("Failed to find cpufreq-data for cpu%d\n", cpu);
		of_node_put(cpu_np);
		return -ENOENT;
	}

	/* Initialize the cpu cluster private data */
	if (sprd_cpufreq_data(cpu)) {
		data = sprd_cpufreq_data(cpu);
	} else {
		data = kzalloc(sizeof(*data), GFP_KERNEL);
		if (!data) {
			ret = -ENOMEM;
			goto free_np;
		}
	}
	policy->driver_data = data;

	if (!data->volt_lock) {
		data->volt_lock = kzalloc(sizeof(*data->volt_lock), GFP_KERNEL);
		if (!data->volt_lock) {
			ret = -ENOMEM;
			goto free_mem;
		} else {
			mutex_init(data->volt_lock);
		}
	}

	mutex_lock(data->volt_lock);

	data->online  = true;
	data->cpu_dev = cpu_dev;
	data->cluster = curr_cluster;

	/* TODO: need to get new temperature from thermal zone after hotplug */
	if (cpufreq_datas[0])
		data->temp_now = cpufreq_datas[0]->temp_now;

	/* Get OPP table information from dts and initialize the map */
	ret = dev_pm_opp_of_add_table_binning(is_big_cluster(cpu),
					      cpu_dev, cpufreq_of_node, data);
	if (ret < 0) {
		pr_err("Failed to init opp table (%d)\n", ret);
		goto free_mem;
	}

	/* Get the frequency table */
	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		pr_err("Error in initializing frequency table (%d)\n", ret);
		goto free_opp;
	}

	/* Verify the frequency table */
	ret = cpufreq_table_validate_and_show(policy, freq_table);
	if (ret) {
		pr_err("Invalid frequency table (%d)\n", ret);
		goto free_table;
	}

#ifdef CONFIG_SMP
	/* CPUs in the same cluster share a clock and power domain */
	cpumask_or(policy->cpus, policy->cpus,
		   cpu_coregroup_mask(policy->cpu));
#endif

	if (!cpufreq_datas[data->cluster])
		cpufreq_datas[data->cluster] = data;

	/* Initialize the slave modules whose voltage and frequency are
	 * voted by cpu clusters, so we call this kind  module as
	 * slave device, and call cpu cluster as host device.
	 */
	ret = sprd_hardware_cpufreq_init_slaves(data, cpufreq_of_node);
	if (ret)
		goto free_table;

	policy->suspend_freq = freq_table[0].frequency;

	if (!(driver->probed && driver->get && driver->enable)) {
		pr_err("platform cpu dvfs driver is empty\n");
		goto free_table;
	}

	if (driver->probed(pdev->archdata, data->cluster)) {
		policy->cur = driver->get(pdev->archdata,
					  data->cluster);
		if (policy->cur < 0)
			goto free_table;
		if (!driver->enable(pdev->archdata,
				    data->cluster, true))
			goto free_table;
	} else {
		goto free_table;
	}

	freq_Hz = policy->cur * 1000;

	policy->dvfs_possible_from_any_cpu = true;

	mutex_unlock(data->volt_lock);

	goto free_np;

free_table:
	if (policy->freq_table)
		dev_pm_opp_free_cpufreq_table(cpu_dev, &policy->freq_table);

free_opp:
	dev_pm_opp_of_remove_table(cpu_dev);

free_mem:
	if (data->volt_lock) {
		mutex_unlock(data->volt_lock);
		mutex_destroy(data->volt_lock);
		kfree(data->volt_lock);
	}
	sprd_cpufreq_data(data->cluster) = NULL;
	kfree(data);

free_np:
	if (cpufreq_of_node)
		of_node_put(cpufreq_of_node);
	if (cpu_np)
		of_node_put(cpu_np);

	return ret;
}

static int sprd_hardware_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct sprd_cpufreq_driver_data *data;
	struct sprd_cpudvfs_device *pdev;
	struct sprd_cpudvfs_ops *driver;
	int ret;

	pdev = sprd_hardware_dvfs_device_get();
	if (!pdev) {
		pr_err("hardware dvfs device has not been registered.\n");
		return  -EINVAL;
	}
	driver = &pdev->ops;

	if (!policy)
		return -ENODEV;

	data = policy->driver_data;
	if (!data) {
		pr_err("the private data for current cluster is empty\n");
		return  -EINVAL;
	}

	mutex_lock(data->volt_lock);
	if (policy->freq_table)
		dev_pm_opp_free_cpufreq_table(data->cpu_dev,
					      &policy->freq_table);
	ret = driver->enable(pdev->archdata,
		       topology_physical_package_id(policy->cpu), false);
	policy->driver_data = NULL;
	data->online  = false;
	mutex_unlock(data->volt_lock);

	return ret;
}

static int sprd_hardware_cpufreq_table_verify(struct cpufreq_policy *policy)
{
	return cpufreq_generic_frequency_table_verify(policy);
}

static unsigned int sprd_hardware_cpufreq_get(unsigned int cpu)
{
	int cluster = topology_physical_package_id(cpu);
	struct sprd_cpudvfs_device *pdev;
	struct sprd_cpudvfs_ops *driver;

	pdev = sprd_hardware_dvfs_device_get();
	if (!pdev) {
		pr_err("Hardware dvfs device has not been registered.\n");
		return  -EINVAL;
	}
	driver = &pdev->ops;

	if (driver->probed && driver->probed(pdev->archdata, cluster))
		return driver->get(pdev->archdata, cluster);
	else
		return cpufreq_generic_get(cpu);
}

static int sprd_hardware_cpufreq_suspend(struct cpufreq_policy *policy)
{
	if (policy && !strcmp(policy->governor->name, "userspace")) {
		pr_info("Do nothing for governor-%s\n",
			policy->governor->name);
		return 0;
	}

	if (boost_mode_flag) {
		sprd_hardware_cpufreq_set_boost(0);
		sprd_hardware_cpufreq_driver.boost_enabled = false;
	}

	return cpufreq_generic_suspend(policy);
}

static int sprd_hardware_cpufreq_resume(struct cpufreq_policy *policy)
{
	if (policy && !strcmp(policy->governor->name, "userspace")) {
		pr_info("Do nothing for governor-%s\n", policy->governor->name);
		return 0;
	}

	return cpufreq_generic_suspend(policy);
}

static struct cpufreq_driver sprd_hardware_cpufreq_driver = {
	.name = "sprd-cpufreq",
	.flags = CPUFREQ_STICKY
			| CPUFREQ_NEED_INITIAL_FREQ_CHECK
			| CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.init = sprd_hardware_cpufreq_init,
	.exit = sprd_hardware_cpufreq_exit,
	.verify = sprd_hardware_cpufreq_table_verify,
	.target_index = sprd_hardware_cpufreq_set_target_index,
	.get = sprd_hardware_cpufreq_get,
	.suspend = sprd_hardware_cpufreq_suspend,
	.resume = sprd_hardware_cpufreq_resume,
	.attr = cpufreq_generic_attr,
	.boost_enabled = true,
	.set_boost = sprd_hardware_cpufreq_set_boost,
};

static int sprd_hardware_cpufreq_probe(struct platform_device *pdev)
{
	struct device *cpu_dev = NULL;
	struct device_node *np = NULL;
	struct device_node *cpu_np;
	struct nvmem_cell *cell;
	int ret;
	int cpu = 0; /* just core0 do probe */

	boot_done_timestamp = jiffies + SPRD_CPUFREQ_DRV_BOOST_DURATOIN;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		dev_err(&pdev->dev, "Failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	cpu_np = of_node_get(cpu_dev->of_node);
	if (!cpu_np) {
		dev_err(&pdev->dev, "Failed to find cpu node\n");
		return -ENODEV;
	}

	np = of_parse_phandle(cpu_np, "cpufreq-data-v1", 0);
	if (!np) {
		dev_err(&pdev->dev, "No cpufreq-data found for cpu%d\n", cpu);
		of_node_put(cpu_np);
		return -ENOENT;
	}

	cell = of_nvmem_cell_get(np, "dvfs_bin");
	ret = PTR_ERR(cell);
	if (!IS_ERR(cell))
		nvmem_cell_put(cell);
	else if (ret == -EPROBE_DEFER)
		goto exit;

	sprd_cpufreq_cpuhp_setup();

	ret = cpufreq_register_driver(&sprd_hardware_cpufreq_driver);
	if (ret)
		dev_err(&pdev->dev, "Failed to reigister cpufreq driver\n");
	else
		dev_info(&pdev->dev, "Succeeded to register cpufreq driver\n");

exit:
	of_node_put(np);
	of_node_put(cpu_np);

	return ret;
}

static int sprd_hardware_cpufreq_remove(struct platform_device *pdev)
{
	return cpufreq_unregister_driver(&sprd_hardware_cpufreq_driver);
}

static struct platform_driver sprd_hardware_cpufreq_platdrv = {
	.driver = {
		.name	= "sprd-hardware-cpufreq",
		.owner	= THIS_MODULE,
	},
	.probe		= sprd_hardware_cpufreq_probe,
	.remove		= sprd_hardware_cpufreq_remove,
};

module_platform_driver(sprd_hardware_cpufreq_platdrv);

static struct platform_device sprd_hardware_cpufreq_pdev = {
	.name = "sprd-hardware-cpufreq",
};

static int  __init sprd_hardware_cpufreq_init_pdev(void)
{
	return platform_device_register(&sprd_hardware_cpufreq_pdev);
}

device_initcall(sprd_hardware_cpufreq_init_pdev);

MODULE_DESCRIPTION("sprd hardware cpufreq driver");
MODULE_LICENSE("GPL v2");
