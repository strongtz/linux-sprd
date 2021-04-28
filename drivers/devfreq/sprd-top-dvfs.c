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

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/devfreq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/pm_opp.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

struct dcdc_subsys {
	u32 subsys_tune_ctl_reg;
	u32 subsys_tune_ctl_bit;
	u32 subsys_tune_eb;
};

struct dvfs_dcdc_pwr {
	u32 dvfs_ctl_reg;
	u32 dvfs_ctl_bit;
	u32 dvfs_eb;
	u32 judge_vol_sw_reg;
	u32 judge_vol_sw_bit;
	u32 judge_vol_sw_mask;
	u32 judge_vol_val;	/* real voltage needed to tell dvfs module */
	struct dcdc_subsys *subsys;
	u32 subsys_num;
};

struct topdvfs_dev {
	void __iomem *base;
	struct device_node *of_node;
	struct regmap *aon_apb_regs;
	struct dvfs_dcdc_pwr *pwr;
	u32 module_eb_reg;
	u32 module_eb_bit;
	u32 dcdc_share_reg;
	u32 dcdc_share_off;
	bool dcdc_modem_mm_shared;
	u32 device_dcdc_num;
	bool parse_done;
};

static void
top_dvfs_bits_update(struct topdvfs_dev *pdev, u32 reg, u32 mask, u32 val)
{
	u32 tmp;

	tmp = readl(pdev->base + reg) & ~mask;
	tmp |= val & mask;

	writel(tmp, pdev->base + reg);
}

static void subsys_dvfs_tune_enable(struct topdvfs_dev *pdev,
				    struct dcdc_subsys *subsys, u32 enable)
{
	u32 reg = subsys->subsys_tune_ctl_reg;
	u32 bit = 0x1 << subsys->subsys_tune_ctl_bit;

	if (enable)
		top_dvfs_bits_update(pdev, reg, bit, ~bit);
	else
		top_dvfs_bits_update(pdev, reg, bit, bit);
}

static void dcdc_pwr_dvfs_enable(struct topdvfs_dev *pdev,
				 struct dvfs_dcdc_pwr *pwr, u32 enable)
{
	u32 reg = pwr->dvfs_ctl_reg;
	u32 bit = 0x1 << pwr->dvfs_ctl_bit;

	if (enable)
		top_dvfs_bits_update(pdev, reg, bit, ~bit);
	else
		top_dvfs_bits_update(pdev, reg, bit, bit);
}

static int coordinate_dcdc_current_voltage(struct topdvfs_dev *pdev,
					   struct dvfs_dcdc_pwr *pwr)
{
	u32 addr, bit, mask;
	u32 curr_volt, val;

	if (!pwr) {
		pr_err("No DCDC Power domain found\n");
		return -ENODEV;
	}

	addr = pwr->judge_vol_sw_reg;
	bit = pwr->judge_vol_sw_bit;
	mask = pwr->judge_vol_sw_mask;
	curr_volt = pwr->judge_vol_val;

	val = readl(pdev->base + addr) & (~(mask << bit));
	val |= curr_volt << bit;

	writel(val, pdev->base + addr);

	return 0;
}

static int sprd_topdvfs_common_config(struct topdvfs_dev *pdev)
{
	struct dcdc_subsys *subsys;
	struct regmap *aon_apb;
	u32 dcdc_num, num;

	if (!pdev || !pdev->of_node) {
		pr_err("No top dvfs device found\n");
		return -ENODEV;
	}

	if (!pdev->parse_done) {
		pr_err("Not finished to parse top dvfs device\n");
		return -EPERM;
	}

	aon_apb = pdev->aon_apb_regs;
	regmap_update_bits(aon_apb, pdev->module_eb_reg,
			   1 << pdev->module_eb_bit,
			   1 << pdev->module_eb_bit);

	for (dcdc_num = 0; dcdc_num < pdev->device_dcdc_num; ++dcdc_num) {
		coordinate_dcdc_current_voltage(pdev, &pdev->pwr[dcdc_num]);
		dcdc_pwr_dvfs_enable(pdev, &pdev->pwr[dcdc_num],
				     pdev->pwr[dcdc_num].dvfs_eb);
		for (num = 0; num < pdev->pwr[dcdc_num].subsys_num; num++) {
			subsys = &pdev->pwr[dcdc_num].subsys[num];
			subsys_dvfs_tune_enable(pdev, subsys,
						subsys->subsys_tune_eb);
		}
	}

	if (pdev->dcdc_modem_mm_shared)
		top_dvfs_bits_update(pdev, pdev->dcdc_share_reg,
				     1 << pdev->dcdc_share_off,
				     1 << pdev->dcdc_share_off);

	return 0;
}

int topdvfs_device_dt_parse(struct topdvfs_dev *pdev)
{
	struct device_node *dcdc_node, *subsys_node;
	struct dcdc_subsys *psubsys;
	struct property *prop;
	u32 nr, ix, num, len;
	u32 subsys_num, i;
	int ret = 0;

	if (!pdev || !pdev->of_node) {
		pr_err("No topdvfs device found\n");
		return -ENODEV;
	}

	if (!of_find_property(pdev->of_node, "module-enable-cfg", &len)) {
		pr_err("No 'module-enable-cfg' property found\n");
		return -EPERM;
	}

	pdev->dcdc_modem_mm_shared =
		of_property_read_bool(pdev->of_node, "dcdc-modem-mm-share");

	if (pdev->dcdc_modem_mm_shared) {
		ret = of_property_read_u32_index(pdev->of_node,
						 "dcdc-modem-mm-share-en",
						 0, &pdev->dcdc_share_reg);
		if (ret) {
			pr_err("Failed to get dcdc-modem-mm-share-en reg offset\n");
			return ret;
		}

		ret = of_property_read_u32_index(pdev->of_node,
						 "dcdc-modem-mm-share-en",
						 1, &pdev->dcdc_share_off);
		if (ret) {
			pr_err("Failed to get bit to enable dcdc-modem-mm-share-en\n");
			return ret;
		}
	}

	if (len / sizeof(u32) == 2) {
		of_property_read_u32_index(pdev->of_node, "module-enable-cfg",
					   0, &pdev->module_eb_reg);
		of_property_read_u32_index(pdev->of_node, "module-enable-cfg",
					   1, &pdev->module_eb_bit);
	} else {
		pr_err("Failed to get module enable info\n");
		return -EINVAL;
	}

	if (!of_find_property(pdev->of_node, "device-dcdc-cells", &nr)) {
		pr_err("Failed to find 'device-dcdc-cells' property\n");
		return -EINVAL;
	}

	nr = nr / sizeof(u32);
	pdev->device_dcdc_num = nr;

	if (!pdev->pwr) {
		pdev->pwr = kzalloc(nr * sizeof(struct dvfs_dcdc_pwr),
				    GFP_KERNEL);
		if (!pdev->pwr)
			return -ENOMEM;
	}

	for (ix = 0; ix < nr; ix++) {
		dcdc_node = of_parse_phandle(pdev->of_node,
					     "device-dcdc-cells", ix);

		if (!dcdc_node) {
			pr_err("Failed to find '%s' node-%d\n",
			       "device-dcdc-cells", ix);
			ret = -EINVAL;
			goto err_pwr_free;
		}

		of_property_read_u32_index(dcdc_node, "dcdc-dvfs-enable",
					   0, &pdev->pwr[ix].dvfs_ctl_reg);
		of_property_read_u32_index(dcdc_node, "dcdc-dvfs-enable",
					   1, &pdev->pwr[ix].dvfs_ctl_bit);
		of_property_read_u32_index(dcdc_node, "dcdc-dvfs-enable",
					   2, &pdev->pwr[ix].dvfs_eb);

		of_property_read_u32_index(dcdc_node, "dcdc-judge-vol-sw", 0,
					   &pdev->pwr[ix].judge_vol_sw_reg);

		of_property_read_u32_index(dcdc_node, "dcdc-judge-vol-sw", 1,
					   &pdev->pwr[ix].judge_vol_sw_bit);

		of_property_read_u32_index(dcdc_node, "dcdc-judge-vol-sw", 2,
					   &pdev->pwr[ix].judge_vol_sw_mask);

		of_property_read_u32_index(dcdc_node, "dcdc-judge-vol-sw", 3,
					   &pdev->pwr[ix].judge_vol_val);

		if (!of_find_property(dcdc_node,
				      "dcdc-subsys-cells", &subsys_num)) {
			pr_err("Failed to find 'dcdc-subsys-cells' node\n");
			ret = -EINVAL;
			goto err_pwr_free;
		}

		subsys_num = subsys_num / sizeof(u32);

		pdev->pwr[ix].subsys_num = subsys_num;

		if (!pdev->pwr[ix].subsys) {
			pdev->pwr[ix].subsys =
				kzalloc(subsys_num * sizeof(struct dcdc_subsys),
					GFP_KERNEL);
			if (!pdev->pwr[ix].subsys) {
				ret = -ENOMEM;
				goto err_pwr_free;
			}
		}

		for (i = 0; i < subsys_num; ++i) {
			subsys_node = of_parse_phandle(dcdc_node,
						       "dcdc-subsys-cells", i);
			if (!subsys_node) {
				pr_err("No subsys-cell-%d found\n", i);
				ret = -EINVAL;
				goto err_subsys_free;
			}

			prop = of_find_property(subsys_node,
						"dcdc-subsys-tune-enable",
						&num);
			if (!prop) {
				pr_err("No %s property found\n",
				       "dcdc-subsys-tune-enable");
				ret = -EINVAL;
				goto err_subsys_free;
			}

			num = num / sizeof(u32);
			if (num != 3) {
				pr_err("Invalid dts configuration\n");
				ret = -EPERM;
				goto err_subsys_free;
			}

			psubsys = &pdev->pwr[ix].subsys[i];

			of_property_read_u32_index(subsys_node,
						   "dcdc-subsys-tune-enable",
						   0,
					&psubsys->subsys_tune_ctl_reg);
			of_property_read_u32_index(subsys_node,
						   "dcdc-subsys-tune-enable",
						   1,
					&psubsys->subsys_tune_ctl_bit);
			of_property_read_u32_index(subsys_node,
						   "dcdc-subsys-tune-enable",
						   2,
					&psubsys->subsys_tune_eb);
		}
	}

	pdev->parse_done = true;

	return 0;

err_subsys_free:
	for (ix = 0; ix < nr; ix++)
		for (i = 0; i < pdev->pwr[ix].subsys_num; ++i)
			kfree(pdev->pwr[ix].subsys + i);
err_pwr_free:
	kfree(pdev->pwr);
	return ret;
}

static int sprd_topdvfs_probe(struct platform_device *pdev)
{
	struct topdvfs_dev *ptopdvfs;
	struct resource *res;
	int ret = 0;

	ptopdvfs = devm_kzalloc(&pdev->dev, sizeof(struct topdvfs_dev),
				GFP_KERNEL);
	if (!ptopdvfs)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	ptopdvfs->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ptopdvfs->base)) {
		ret = PTR_ERR(ptopdvfs->base);
		goto err_dev_free;
	}

	ptopdvfs->of_node = pdev->dev.of_node;

	ptopdvfs->aon_apb_regs =
			syscon_regmap_lookup_by_phandle(ptopdvfs->of_node,
							"sprd,syscon-enable");
	if (IS_ERR(ptopdvfs->aon_apb_regs)) {
		pr_err("Failed to get aon apb control register map.\n");
		ret = -ENODEV;
		goto err_mem_unmap;
	}

	ret = topdvfs_device_dt_parse(ptopdvfs);
	if (ret)
		goto err_mem_unmap;

	ret = sprd_topdvfs_common_config(ptopdvfs);
	if (ret)
		goto err_mem_unmap;

	pr_info("Succeeded to register a top dvfs device\n");

	return 0;

err_mem_unmap:
	devm_iounmap(&pdev->dev, ptopdvfs->base);
err_dev_free:
	devm_kfree(&pdev->dev, ptopdvfs);

	return ret;
}

static int sprd_topdvfs_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id sprd_topdvfs_dev_of_match[] = {
	{ .compatible = "sprd,sharkl5-topdvfs"},
	{ .compatible = "sprd,roc1-topdvfs"},
	{ .compatible = "sprd,sharkl5pro-topdvfs"},
	{ .compatible = "sprd,orca-topdvfs"},
	{},
};

static struct platform_driver sprd_topdvfs_driver = {
	.probe = sprd_topdvfs_probe,
	.remove = sprd_topdvfs_remove,
	.driver = {
		.name = "sharkl5-topdvfs-drv",
		.of_match_table = of_match_ptr(sprd_topdvfs_dev_of_match),
	},
};

static int __init top_dvfs_init(void)
{
	return platform_driver_register(&sprd_topdvfs_driver);
}
device_initcall(top_dvfs_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jack Liu<Jack.Liu@unisoc.com>");
MODULE_DESCRIPTION("sprd hardware dvfs driver");

