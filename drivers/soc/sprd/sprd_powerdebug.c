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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/cpu_pm.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/wakeup_reason.h>

#define BIT_NUM_IN_PER_REG 0x20
#define MAX_STATES_NUM_PER_REG 8

struct reg_check {
	u32 addr_offset;
	u32 value_mask;
	u32 expect_value;
	char *preg_name;
};

struct pdm_info {
	u32 addr_offset;
	u32 pwd_bit_width;
	u32 bit_index[MAX_STATES_NUM_PER_REG];
	char *pdm_name[MAX_STATES_NUM_PER_REG];
};

struct intc_info {
	u32 addr_offset;
	char *pint_name[32];
};

struct power_debug {
	struct platform_device *pdev;
	u32 pm_thread_interval;
	u32 pm_thread_exit;
	u32 pm_log_on;
	u32 ap_intc_num;
	u32 pmu_pdm_num;
	u32 ap_ahb_reg_num;
	u32 ap_apb_reg_num;
	u32 pmu_apb_reg_num;
	u32 aon_apb_reg_num;
	u32 aon_sec_reg_num;

	struct pdm_info *ppdm_info;
	struct intc_info *pintc_info;
	struct reg_check *ap_ahb_reg;
	struct reg_check *ap_apb_reg;
	struct reg_check *pmu_apb_reg;
	struct reg_check *aon_apb_reg;
	struct reg_check *aon_sec_reg;

	struct regmap *ap_ahb;
	struct regmap *ap_apb;
	struct regmap *pmu_apb;
	struct regmap *aon_apb;
	struct regmap *aon_sec;
	struct regmap *ap_intc[0];
};

struct power_debug *p_power_debug_entry;

/*
 * sprd_pm_print_pdm_info - output the power state of each power domain
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pm_print_pdm_info(struct power_debug *pdebug_entry)
{
	u32 dbg_pwr_status;
	int i, j;
	struct pdm_info *ppdm_info;

	if (!pdebug_entry->pm_log_on)
		return;

	if (!pdebug_entry->pmu_apb || !pdebug_entry->pmu_pdm_num
		|| !pdebug_entry->ppdm_info)
		return;

	dev_info(&p_power_debug_entry->pdev->dev,
		"###--PMU submodule power states--###\n");
	for (i = 0; i < pdebug_entry->pmu_pdm_num; i++) {
		ppdm_info = &pdebug_entry->ppdm_info[i];

		regmap_read(pdebug_entry->pmu_apb, ppdm_info->addr_offset,
			&dbg_pwr_status);

		dev_info(&p_power_debug_entry->pdev->dev,
			" ##--reg offset:0x%04x value:0x%08x:\n",
			ppdm_info->addr_offset, dbg_pwr_status);

		for (j = 0; j < MAX_STATES_NUM_PER_REG; j++) {
			if (ppdm_info->bit_index[j] + ppdm_info->pwd_bit_width
					> BIT_NUM_IN_PER_REG)
				break;
			if (ppdm_info->pdm_name[j]) {
				dev_info(&p_power_debug_entry->pdev->dev,
					"  #--%s STATE:0x%X\n",
					ppdm_info->pdm_name[j],
					dbg_pwr_status >> ppdm_info->bit_index[j]
					& ((1 << ppdm_info->pwd_bit_width) - 1));
			}
		}
	}
}

/*
 * sprd_pm_print_check_reg - output the register value of the indicated
 *     register bank if the register value is not expected
 * @pregmap: the regmap indicate the specific register bank
 * @reg_num: the register number in the register in the register table
 * @pentry_tbl: the pointer of the register table which want to be checked
 * @module_name: the register bank name
 */
#define OPT_FMT " ##--offset:0x%04x value:0x%08x mask:0x%08x exp_val:0x%08x\n"
static void sprd_pm_print_check_reg(struct regmap *pregmap, u32 reg_num,
	struct reg_check *pentry_tbl, const char *module_name)
{
	u32 index;
	u32 reg_value;
	struct reg_check *pentry;

	if (!pregmap || !reg_num || !pentry_tbl)
		return;

	dev_info(&p_power_debug_entry->pdev->dev,
		"###--PMU %s register check--###\n", module_name);
	for (index = 0; index < reg_num; index++) {
		pentry = &pentry_tbl[index];
		regmap_read(pregmap, pentry->addr_offset, &reg_value);

		if ((reg_value & pentry->value_mask) != pentry->expect_value) {
			dev_info(&p_power_debug_entry->pdev->dev,
				OPT_FMT, pentry->addr_offset, reg_value,
				pentry->value_mask, pentry->expect_value);
		}
	}
}

/*
 * sprd_pm_print_reg_check - output the register value if it's value is not
 *     expected
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pm_print_reg_check(struct power_debug *pdebug_entry)
{
	if (!pdebug_entry->pm_log_on)
		return;

	sprd_pm_print_check_reg(pdebug_entry->ap_ahb,
				pdebug_entry->ap_ahb_reg_num,
				pdebug_entry->ap_ahb_reg, "ap-ahb");
	sprd_pm_print_check_reg(pdebug_entry->ap_apb,
				pdebug_entry->ap_apb_reg_num,
				pdebug_entry->ap_apb_reg, "ap-apb");
	sprd_pm_print_check_reg(pdebug_entry->pmu_apb,
				pdebug_entry->pmu_apb_reg_num,
				pdebug_entry->pmu_apb_reg, "pmu-apb");
	sprd_pm_print_check_reg(pdebug_entry->aon_apb,
				pdebug_entry->aon_apb_reg_num,
				pdebug_entry->aon_apb_reg, "aon-apb");
	sprd_pm_print_check_reg(pdebug_entry->aon_sec,
				pdebug_entry->aon_sec_reg_num,
				pdebug_entry->aon_sec_reg, "aon-sec");
}

/*
 * sprd_pm_print_intc_state - output the value of the interrupt state register
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pm_print_intc_state(struct power_debug *pdebug_entry)
{
	u32 reg_value;
	int ret;
	int i;
	struct intc_info *pintc_info;

	if (!pdebug_entry->pm_log_on)
		return;

	if (!pdebug_entry->ap_intc_num)
		return;

	if(!pdebug_entry->pintc_info)
		return;

	for (i = 0; i < pdebug_entry->ap_intc_num; i++) {
		pintc_info = &(pdebug_entry->pintc_info[i]);

		ret = regmap_read(pdebug_entry->ap_intc[i],
				pintc_info->addr_offset, &reg_value);
		if (ret)
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s: Failed to get intc mask reg value.\n",
				__func__);
		else
			dev_info(&p_power_debug_entry->pdev->dev,
				"##--Status of intc%d :0x%08x\n", i, reg_value);
	}
}

/*
 * sprd_pm_print_wakeup_source - output the wakeup interrupt name
 * @pdebug_entry: the pointer of the core structure of this driver
 */
static void sprd_pm_print_wakeup_source(struct power_debug *pdebug_entry)
{
	u32 reg_value;
	int i, j, ret;
	struct intc_info *pintc_info;

	if (!pdebug_entry->pm_log_on)
		return;

	if (!pdebug_entry->ap_intc_num)
		return;

	if(!pdebug_entry->pintc_info)
		return;

	for (i = 0; i < pdebug_entry->ap_intc_num; i++) {
		pintc_info = &pdebug_entry->pintc_info[i];

		ret = regmap_read(pdebug_entry->ap_intc[i],
				pintc_info->addr_offset, &reg_value);
		if (ret) {
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s: Failed to get intc mask reg value.\n",
				__func__);
			continue;
		}

		if (!reg_value)
			continue;

		for (j = 0; j < BIT_NUM_IN_PER_REG; j++) {
			if (reg_value & BIT(j)) {
				log_wakeup_reason(i * BIT_NUM_IN_PER_REG + j);
				dev_info(&p_power_debug_entry->pdev->dev,
					"#--Wake up by %d(%s_%s)!\n",
					i * BIT_NUM_IN_PER_REG + j, "INT_APCPU",
					pintc_info->pint_name[j]);
			}
		}
	}
}

/*
 * sprd_pm_notifier - Notification call back function
 */
static int sprd_pm_notifier(struct notifier_block *self,
	unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		sprd_pm_print_reg_check(p_power_debug_entry);
		sprd_pm_print_intc_state(p_power_debug_entry);
		sprd_pm_print_pdm_info(p_power_debug_entry);
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
		break;
	case CPU_CLUSTER_PM_EXIT:
		sprd_pm_print_wakeup_source(p_power_debug_entry);
		break;
	}

	return NOTIFY_OK;
}

/* Notifier object */
static struct notifier_block sprd_pm_notifier_block = {
	.notifier_call = sprd_pm_notifier,
};

/*
 * sprd_pm_thread - the thread function
 * @data: a parameter pointer
 */
static int sprd_pm_thread(void *data)
{
	u32 cnt = 0;

	while (p_power_debug_entry) {
		if (!p_power_debug_entry->pm_thread_exit) {
			sprd_pm_print_pdm_info(p_power_debug_entry);

			if (!pm_get_wakeup_count(&cnt, false)) {
				dev_info(&p_power_debug_entry->pdev->dev,
					"PM: has wakeup events in progress:\n");
				pm_print_active_wakeup_sources();
			}
		}

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(p_power_debug_entry->pm_thread_interval * HZ);
	}

	return 0;
}

/*
 * sprd_pm_debug_init - Initialize the debug thread
 * @p_entry: the pointer of the core structure of this driver
 */
static void sprd_pm_debug_init(struct power_debug *p_entry)
{
	static struct dentry *dentry_debug_root;
	struct task_struct *task;

	if (!p_entry)
		return;

	p_entry->pm_log_on = 1;
	p_entry->pm_thread_exit = 0;
	p_entry->pm_thread_interval = 30;

	/* Debug fs config */
	dentry_debug_root = debugfs_create_dir("power", NULL);
	if (IS_ERR_OR_NULL(dentry_debug_root)) {
		dev_err(&p_power_debug_entry->pdev->dev,
			"%s, Failed to create debugfs directory\n", __func__);
		dentry_debug_root = NULL;
		return;
	}

	debugfs_create_u32("pm_log_on", 0644, dentry_debug_root,
		&(p_entry->pm_log_on));

	debugfs_create_u32("pm_thread_exit", 0644, dentry_debug_root,
		&(p_entry->pm_thread_exit));

	debugfs_create_u32("pm_thread_interval", 0644, dentry_debug_root,
		&(p_entry->pm_thread_interval));

	task = kthread_create(sprd_pm_thread, NULL, "sprd_pm_thread");
	if (!task)
		dev_err(&p_power_debug_entry->pdev->dev,
			"%s, Failed to create print thread", __func__);
	else
		wake_up_process(task);
}

/*
 * sprd_pm_parse_reg_tbl - Parse the registers information want to be checked
 * @pnode: the pointer of the dts node
 * @field_name: the register back name in the dts
 * @reg_num: the register number which want to be checked
 * @ppreg_tbl: a pointer of pointer which want to be set the new
 *     allocate space
 */
static int sprd_pm_parse_reg_tbl(const struct device_node *pnode,
	const char *field_name, u32 reg_num,
	struct reg_check **ppreg_tbl)
{
	u32 index;
	u32 temp_val;
	struct reg_check *preg_tbl;

	if (!reg_num ||
		of_property_read_u32_index(pnode, field_name, 0, &temp_val)) {
		*ppreg_tbl = NULL;
		return 0;
	}

	preg_tbl = kcalloc(reg_num, sizeof(struct reg_check), GFP_KERNEL);
	if (!preg_tbl)
		return -ENOMEM;

	*ppreg_tbl = preg_tbl;

	for (index = 0; index < reg_num; index++) {
		of_property_read_u32_index(pnode, field_name, (index * 3 + 0),
					&preg_tbl[index].addr_offset);
		of_property_read_u32_index(pnode, field_name, (index * 3 + 1),
					&preg_tbl[index].value_mask);
		of_property_read_u32_index(pnode, field_name, (index * 3 + 2),
					&preg_tbl[index].expect_value);
		preg_tbl[index].preg_name = NULL;
	}

	return 0;
}

/*
 * sprd_pm_parse_intc_info - Parse the intc information want to be output
 * @pnode: the pointer of the dts node
 * @intc_num: the intc number
 * @ppintc_info: a pointer of pointer which want to be set the new
 *     allocate space
 */
static int sprd_pm_parse_intc_info(const struct device_node *pnode,
	u32 intc_num, struct intc_info **ppintc_info)
{
	u32 index, i;
	const char *cur;
	struct intc_info *pintc_info;
	struct device_node *psub_node;
	struct property *prop;

	if (!intc_num)
		return -ENODEV;

	pintc_info = kcalloc(intc_num, sizeof(struct intc_info), GFP_KERNEL);
	if (!pintc_info)
		return -ENOMEM;

	*ppintc_info = pintc_info;

	for (index = 0; index < intc_num; index++) {
		psub_node = of_parse_phandle(pnode, "sprd,ap-intc", index);
		if (!psub_node) {
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s, Failed find intc[%d]\n", __func__, index);
			kzfree(pintc_info);
			return -ENODEV;
		}

		if (of_property_read_u32_index(psub_node, "reg",
					0, &(pintc_info[index].addr_offset))) {
			dev_err(&p_power_debug_entry->pdev->dev, "can't read reg!\n");
			of_node_put(psub_node);
			kzfree(pintc_info);
			return -ENODATA;
		}

		prop = of_find_property(psub_node, "sprd,int-names", NULL);
		if (!prop) {
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s, Failed to find int-names\n", __func__);
			of_node_put(psub_node);
			kzfree(pintc_info);
			return -ENODATA;
		}
		cur = NULL;
		for (i = 0; i < BIT_NUM_IN_PER_REG; i++) {
			cur = of_prop_next_string(prop, cur);
			pintc_info[index].pint_name[i] = (char *)cur;
		}
		of_node_put(psub_node);
	}

	return 0;
}

/*
 * sprd_pm_parse_pdm_info - Parse the power state information want to be output
 * @pnode: the pointer of the dts node
 * @pdm_state_num: the number of power state register
 * @pppdm_info: a pointer of pointer which want to be set the new
 *     allocate space
 */
static int sprd_pm_parse_pdm_info(const struct device_node *pnode,
	u32 pdm_state_num, struct pdm_info **pppdm_info)
{
	u32 index, i;
	const char *cur;
	struct pdm_info *ppdm_info;
	struct device_node *psub_node;
	struct property *prop;

	if (!pdm_state_num)
		return -ENODEV;

	ppdm_info = kcalloc(pdm_state_num, sizeof(struct pdm_info),
			GFP_KERNEL);
	if (!ppdm_info)
		return -ENOMEM;

	*pppdm_info = ppdm_info;

	for (index = 0; index < pdm_state_num; index++) {
		psub_node = of_parse_phandle(pnode, "sprd,pdm-name", index);
		if (!psub_node) {
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s,Failed find sprd_pwr_status[%d]\n",
				__func__, index);
			kzfree(ppdm_info);
			return -ENODEV;
		}

		if (of_property_read_u32_index(psub_node, "reg", 0,
					&ppdm_info[index].addr_offset)) {
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s, Failed to find reg\n", __func__);
			of_node_put(psub_node);
			kzfree(ppdm_info);
			return -ENODATA;
		}
		if (of_property_read_u32_index(psub_node, "sprd,bit-width", 0,
					&ppdm_info[index].pwd_bit_width)) {
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s, Failed to find sprd,bit-width\n",
				__func__);
			of_node_put(psub_node);
			kzfree(ppdm_info);
			return -ENODATA;
		}

		for (i = 0; i < MAX_STATES_NUM_PER_REG; i++) {
			if (of_property_read_u32_index(psub_node, "sprd,bit-index", i,
						&ppdm_info[index].bit_index[i]))
				ppdm_info[index].bit_index[i] = BIT_NUM_IN_PER_REG;
		}

		prop = of_find_property(psub_node, "sprd,pdm-names", NULL);
		if (!prop) {
			dev_err(&p_power_debug_entry->pdev->dev,
				"%s, Failed to find sprd,pdm-names\n",
				__func__);
			of_node_put(psub_node);
			kzfree(ppdm_info);
			return -ENODATA;
		}
		cur = NULL;
		for (i = 0; i < MAX_STATES_NUM_PER_REG; i++) {
			cur = of_prop_next_string(prop, cur);
			if (!cur || !of_compat_cmp(cur, "null", 4))
				ppdm_info[index].pdm_name[i] = NULL;
			else
				ppdm_info[index].pdm_name[i] = (char *)cur;
		}
		of_node_put(psub_node);
	}

	return 0;
}

/*
 * sprd_pm_parse_regmap - Parse the register base address regmap
 * @pnode: the pointer of the dts node
 */
static int sprd_pm_parse_regmap(struct device_node *pnode)
{
	u32 i;
	struct device_node *psub_node;

	p_power_debug_entry->ap_ahb = syscon_regmap_lookup_by_phandle(pnode,
							"sprd,sys-ap-ahb");
	if (IS_ERR(p_power_debug_entry->ap_ahb)) {
		dev_err(&p_power_debug_entry->pdev->dev,
			"%s, Failed to get ap-ahb regmap\n", __func__);
		return PTR_ERR(p_power_debug_entry->ap_ahb);
	}

	p_power_debug_entry->ap_apb = syscon_regmap_lookup_by_phandle(pnode,
							"sprd,sys-ap-apb");
	if (IS_ERR(p_power_debug_entry->ap_apb)) {
		dev_err(&p_power_debug_entry->pdev->dev,
			"%s, Failed to get ap-apb regmap\n", __func__);
		return PTR_ERR(p_power_debug_entry->ap_apb);
	}

	p_power_debug_entry->pmu_apb = syscon_regmap_lookup_by_phandle(pnode,
							"sprd,sys-pmu-apb");
	if (IS_ERR(p_power_debug_entry->pmu_apb)) {
		dev_err(&p_power_debug_entry->pdev->dev,
			"%s, Failed to get pmu-apb regmap\n", __func__);
		return PTR_ERR(p_power_debug_entry->pmu_apb);
	}

	p_power_debug_entry->aon_apb = syscon_regmap_lookup_by_phandle(pnode,
							"sprd,sys-aon-apb");
	if (IS_ERR(p_power_debug_entry->aon_apb)) {
		dev_err(&p_power_debug_entry->pdev->dev,
			"%s, Failed to get aon-apb regmap\n", __func__);
		return PTR_ERR(p_power_debug_entry->aon_apb);;
	}

	p_power_debug_entry->aon_sec = syscon_regmap_lookup_by_phandle(pnode,
							"sprd,sys-aon-sec");
	if (IS_ERR(p_power_debug_entry->aon_sec))
		dev_warn(&p_power_debug_entry->pdev->dev,
			"%s, Warned to get aon-sec regmap\n", __func__);

	for (i = 0; i < p_power_debug_entry->ap_intc_num; i++) {
		psub_node = of_parse_phandle(pnode, "sprd,sys-ap-intc", i);
		if (psub_node) {
			p_power_debug_entry->ap_intc[i] =
					syscon_node_to_regmap(psub_node);
			of_node_put(psub_node);
			if (IS_ERR(p_power_debug_entry->ap_intc[i])) {
				dev_err(&p_power_debug_entry->pdev->dev,
					"%s,Failed to get ap-intc[%d] regmap]\n",
					__func__, i);
				return PTR_ERR(p_power_debug_entry->ap_intc[i]);
			}
		} else {
			p_power_debug_entry->ap_intc[i] = NULL;
		}
	}

	return 0;
}

/*
 * sprd_pm_parse_reg_num - Parse the number of variable parameter in the dts,
 *     such as xxx_num.
 * @pnode: the pointer of the dts node
 */
static int sprd_pm_parse_reg_num(struct device_node *pnode)
{
	int result;

	result = of_property_count_elems_of_size(pnode,
					"sprd,pdm-name", sizeof(u32));
	if (result < 0) {
		dev_err(&p_power_debug_entry->pdev->dev,
			"no power state information!\n");
		return result;
	}

	p_power_debug_entry->pmu_pdm_num = result;

	result = of_property_count_elems_of_size(pnode,
				"sprd,ap-ahb-reg-tbl", 3 * sizeof(u32));
	p_power_debug_entry->ap_ahb_reg_num = result > 0 ? result : 0;

	result = of_property_count_elems_of_size(pnode,
				"sprd,ap-apb-reg-tbl", 3 * sizeof(u32));
	p_power_debug_entry->ap_apb_reg_num = result > 0 ? result : 0;

	result = of_property_count_elems_of_size(pnode,
				"sprd,pmu-apb-reg-tbl", 3 * sizeof(u32));
	p_power_debug_entry->pmu_apb_reg_num = result > 0 ? result : 0;

	result = of_property_count_elems_of_size(pnode,
				"sprd,aon-apb-reg-tbl", 3 * sizeof(u32));
	p_power_debug_entry->aon_apb_reg_num = result > 0 ? result : 0;

	result = of_property_count_elems_of_size(pnode,
				"sprd,aon-sec-reg-tbl", 3 * sizeof(u32));
	p_power_debug_entry->aon_sec_reg_num = result > 0 ? result : 0;

	dev_info(&p_power_debug_entry->pdev->dev,
		"power-dbg-parameter:%d %d %d %d %d %d %d\n",
		p_power_debug_entry->ap_intc_num,
		p_power_debug_entry->pmu_pdm_num,
		p_power_debug_entry->ap_ahb_reg_num,
		p_power_debug_entry->ap_apb_reg_num,
		p_power_debug_entry->pmu_apb_reg_num,
		p_power_debug_entry->aon_apb_reg_num,
		p_power_debug_entry->aon_sec_reg_num);

	return 0;
}

/*
 * sprd_pm_parse_node - Parse the dts node information of this driver, and
 *     construct the core structure used in this driver.
 */
static int sprd_pm_parse_node(struct platform_device *pdev)
{
	struct device_node *pnode;
	int result;

	pnode = pdev->dev.of_node;
	if (!pnode) {
		dev_err(&pdev->dev,
			"%s, Failed to find power-debug node\n", __func__);
		return -ENODEV;
	}

	result = of_property_count_elems_of_size(pnode,
					"sprd,sys-ap-intc", sizeof(u32));
	if (result < 0) {
		dev_err(&pdev->dev, "no intc information!\n");
		return -ENODEV;
	}

	p_power_debug_entry = kzalloc((sizeof(struct power_debug) +
			result * sizeof(struct regmap *)), GFP_KERNEL);
	if (!p_power_debug_entry)
		return -ENOMEM;

	p_power_debug_entry->pdev = pdev;
	p_power_debug_entry->ap_intc_num = (u32)result;

	result = sprd_pm_parse_reg_num(pnode);
	if (result < 0)
		goto free_debug_entry;

	result = sprd_pm_parse_regmap(pnode);
	if (result < 0)
		goto free_debug_entry;

	result = sprd_pm_parse_reg_tbl(pnode, "sprd,ap-ahb-reg-tbl",
					p_power_debug_entry->ap_ahb_reg_num,
					&p_power_debug_entry->ap_ahb_reg);
	if (result)
		goto free_debug_entry;

	result = sprd_pm_parse_reg_tbl(pnode, "sprd,ap-apb-reg-tbl",
					p_power_debug_entry->ap_apb_reg_num,
					&p_power_debug_entry->ap_apb_reg);
	if (result)
		goto free_ap_ahb_reg_check;

	result = sprd_pm_parse_reg_tbl(pnode, "sprd,pmu-apb-reg-tbl",
					p_power_debug_entry->pmu_apb_reg_num,
					&p_power_debug_entry->pmu_apb_reg);
	if (result)
		goto free_ap_apb_reg_check;

	result = sprd_pm_parse_reg_tbl(pnode, "sprd,aon-apb-reg-tbl",
					p_power_debug_entry->aon_apb_reg_num,
					&p_power_debug_entry->aon_apb_reg);
	if (result)
		goto free_pmu_apb_reg_check;

	result = sprd_pm_parse_reg_tbl(pnode, "sprd,aon-sec-reg-tbl",
					p_power_debug_entry->aon_sec_reg_num,
					&p_power_debug_entry->aon_sec_reg);
	if (result)
		goto free_aon_apb_reg_check;

	result = sprd_pm_parse_intc_info(pnode,
					p_power_debug_entry->ap_intc_num,
					&p_power_debug_entry->pintc_info);
	if (result)
		goto free_aon_sec_reg_check;

	result = sprd_pm_parse_pdm_info(pnode,
					p_power_debug_entry->pmu_pdm_num,
				&p_power_debug_entry->ppdm_info);
	if (result)
		goto free_intc_info;

	return 0;

free_intc_info:
	kfree(p_power_debug_entry->pintc_info);
free_aon_sec_reg_check:
	kfree(p_power_debug_entry->aon_sec_reg);
free_aon_apb_reg_check:
	kfree(p_power_debug_entry->aon_apb_reg);
free_pmu_apb_reg_check:
	kfree(p_power_debug_entry->pmu_apb_reg);
free_ap_apb_reg_check:
	kfree(p_power_debug_entry->ap_apb_reg);
free_ap_ahb_reg_check:
	kfree(p_power_debug_entry->ap_ahb_reg);
free_debug_entry:
	kzfree(p_power_debug_entry);
	p_power_debug_entry = NULL;

	return result;
}

/*
 * sprd_powerdebug_probe - add the power debug driver
 */
static int sprd_powerdebug_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "##### Power debug log init start #####\n");

	/* Init register base address regmap */
	ret = sprd_pm_parse_node(pdev);
	if (ret) {
		dev_err(&pdev->dev, "##### Power debug log init failed #####\n");
		return ret;
	}

	/* Init sprd pm debug thread */
	sprd_pm_debug_init(p_power_debug_entry);

	/* Register the callback function when system suspend or resume */
	cpu_pm_register_notifier(&sprd_pm_notifier_block);

	dev_info(&pdev->dev, "##### Power debug log init successfully #####\n");

	return 0;
}

/*
 * sprd_powerdebug_remove - remove the power debug driver
 */
static int sprd_powerdebug_remove(struct platform_device *pdev)
{
	if (!p_power_debug_entry)
		return -ENOMEM;

	kfree(p_power_debug_entry->ppdm_info);
	kfree(p_power_debug_entry->pintc_info);
	kfree(p_power_debug_entry->aon_sec_reg);
	kfree(p_power_debug_entry->aon_apb_reg);
	kfree(p_power_debug_entry->pmu_apb_reg);
	kfree(p_power_debug_entry->ap_apb_reg);
	kfree(p_power_debug_entry->ap_ahb_reg);
	kzfree(p_power_debug_entry);
	p_power_debug_entry = NULL;

	return 0;
}

static const struct of_device_id sprd_powerdebug_of_match[] = {
	{
		.compatible = "sprd,power-debug",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_powerdebug_of_match);

static struct platform_driver sprd_powerdebug_driver = {
	.probe = sprd_powerdebug_probe,
	.remove = sprd_powerdebug_remove,
	.driver = {
		.name = "sprd-powerdebug",
		.of_match_table = sprd_powerdebug_of_match,
	},
};

module_platform_driver(sprd_powerdebug_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jamesj Chen<Jamesj.Chen@unisoc.com>");
MODULE_DESCRIPTION("sprd power debug driver");
