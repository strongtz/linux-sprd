/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "cpufreq-dt.h"

struct sprd_dummy_cpufreq_private_data {
	struct device *cpu_dev;
	unsigned int cur_freq;
};

static struct freq_attr *sprd_dummy_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,   /* Extra space for boost-attr if required */
	NULL,
};

static int sprd_dummy_cpufreq_set_target(struct cpufreq_policy *policy,
					 unsigned int index)
{
	struct cpufreq_frequency_table *freq_table = policy->freq_table;
	struct sprd_dummy_cpufreq_private_data *priv = policy->driver_data;

	priv->cur_freq = freq_table[index].frequency;

	return 0;
}

static int sprd_dummy_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;
	struct device_node *np;
	struct sprd_dummy_cpufreq_private_data *priv;
	struct device *cpu_dev;
	unsigned int transition_latency;
	int ret, num;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", policy->cpu);
		return -ENODEV;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_err("failed to find cpu%d node\n", policy->cpu);
		return -ENOENT;
	}

	/*
	 * Initialize OPP tables for all policy->cpus. They will be shared by
	 * all CPUs which have marked their CPUs shared with OPP bindings.
	 *
	 * For platforms not using operating-points-v2 bindings, we do this
	 * before updating policy->cpus. Otherwise, we will end up creating
	 * duplicate OPPs for policy->cpus.
	 *
	 * OPPs might be populated at runtime, don't check for error here
	 */
	dev_pm_opp_of_cpumask_add_table(policy->cpus);
	/*
	 * But we need OPP table to function so if it is not there let's
	 * give platform code chance to provide it for us.
	 */
	num = dev_pm_opp_get_opp_count(cpu_dev);
	if (num <= 0) {
		pr_err("failed to init OPP table: %d\n", num);
		ret = -EPROBE_DEFER;
		goto out_free_opp;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out_free_opp;
	}

	/*
	 * Spreadtrum ZEBU cpu frequency adjustment is dummy.
	 * But we still need cpu frequency transition latency.
	 * 500000 is a typical number of transition latency.
	 */
	transition_latency = 500000;

#ifdef CONFIG_SMP
	/* CPUs in the same cluster share a clock and power domain. */
	cpumask_or(policy->cpus, policy->cpus, cpu_coregroup_mask(policy->cpu));
#endif
	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		pr_err("failed to init cpufreq table %d\n", ret);
		goto out_free_priv;
	}

	ret = cpufreq_table_validate_and_show(policy, freq_table);
	if (ret) {
		pr_err("%s: invalid frequency table %d\n", __func__, ret);
		goto out_free_cpufreq_table;
	}

	priv->cpu_dev = cpu_dev;
	priv->cur_freq = policy->freq_table[0].frequency;
	policy->driver_data = priv;
	policy->cpuinfo.transition_latency = transition_latency;
	of_node_put(np);
	return 0;

out_free_cpufreq_table:
	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table);
out_free_priv:
	kfree(priv);
out_free_opp:
	dev_pm_opp_of_cpumask_remove_table(policy->cpus);
	of_node_put(np);

	return ret;
}

static int sprd_dummy_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct sprd_dummy_cpufreq_private_data *priv = policy->driver_data;

	dev_pm_opp_free_cpufreq_table(priv->cpu_dev, &policy->freq_table);
	dev_pm_opp_of_cpumask_remove_table(policy->related_cpus);

	kfree(priv);

	return 0;
}

static unsigned int sprd_dummy_cpufreq_get(unsigned int cpu)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	struct sprd_dummy_cpufreq_private_data *priv = policy->driver_data;

	cpufreq_cpu_put(policy);
	return priv->cur_freq;
}

static struct cpufreq_driver sprd_dummy_cpufreq_driver = {
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = sprd_dummy_cpufreq_set_target,
	.get = sprd_dummy_cpufreq_get,
	.init = sprd_dummy_cpufreq_init,
	.exit = sprd_dummy_cpufreq_exit,
	.name = "dummy-cpufreq",
	.attr = sprd_dummy_cpufreq_attr,
};

static int sprd_dummy_cpufreq_probe(struct platform_device *pdev)
{
	sprd_dummy_cpufreq_driver.driver_data = dev_get_platdata(&pdev->dev);
	return cpufreq_register_driver(&sprd_dummy_cpufreq_driver);
}

static int sprd_dummy_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&sprd_dummy_cpufreq_driver);
	return 0;
}

static struct platform_driver sprd_dummy_cpufreq_platdrv = {
	.driver = {
		.name	= "dummy-cpufreq",
	},
	.probe		= sprd_dummy_cpufreq_probe,
	.remove		= sprd_dummy_cpufreq_remove,
};

module_platform_driver(sprd_dummy_cpufreq_platdrv);

static struct platform_device sprd_dummy_cpufreq_pdev = {
	.name = "dummy-cpufreq",
};

static int  __init sprd_dummy_cpufreq_init_pdev(void)
{
	return platform_device_register(&sprd_dummy_cpufreq_pdev);
}

device_initcall(sprd_dummy_cpufreq_init_pdev);

MODULE_ALIAS("platform:sprd_dummy_driver");
MODULE_DESCRIPTION("Spreadtrum dummy cpufreq driver");
MODULE_LICENSE("GPL");
