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

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sipc.h>
#include <linux/sizes.h>

#include "sipc_priv.h"

#define MBOX_BAMK	"mbox"
#define PCIE_BAMK	"pcie"

#if defined(CONFIG_DEBUG_FS)
void sipc_debug_putline(struct seq_file *m, char c, int n)
{
	char buf[300];
	int i, max, len;

	/* buf will end with '\n' and 0 */
	max = ARRAY_SIZE(buf) - 2;
	len = (n > max) ? max : n;

	for (i = 0; i < len; i++)
		buf[i] = c;

	buf[i] = '\n';
	buf[i + 1] = 0;

	seq_puts(m, buf);
}
EXPORT_SYMBOL_GPL(sipc_debug_putline);
#endif

static u32 sipc_rxirq_status(u8 dst)
{
	return 0;
}

static void sipc_rxirq_clear(u8 dst)
{

}

static void sipc_txirq_trigger(u8 dst, u64 msg)
{
	struct smsg_ipc *ipc;

	ipc = smsg_ipcs[dst];

	if (ipc) {
#ifdef CONFIG_SPRD_MAILBOX
		if (ipc->type == SIPC_BASE_MBOX) {
			mbox_raw_sent(ipc->core_id, msg);
			return;
		}
#endif

		if (ipc->type == SIPC_BASE_PCIE) {
#ifdef CONFIG_SPRD_PCIE_EP_DEVICE
			sprd_ep_dev_raise_irq(ipc->ep_dev, PCIE_EP_SIPC_IRQ);
#endif

#ifdef CONFIG_PCIE_EPF_SPRD
			sprd_pci_epf_raise_irq(ipc->ep_fun, SPRD_EPF_SIPC_IRQ);
#endif
			return;
		}
	}
}

static int sipc_parse_dt(struct smsg_ipc *ipc,
	struct device_node *np, struct device *dev)
{
	u32 val[3];
	int ret;
	const char *type;

	/* get name */
	ret = of_property_read_string(np, "sprd,name", &ipc->name);
	if (ret)
		return ret;

	pr_debug("sipc: name=%s\n", ipc->name);

	/* get sipc type, optional */
	if (of_property_read_string(np, "sprd,type", &type) == 0) {
		pr_debug("sipc: type=%s\n", type);
		if (strcmp(MBOX_BAMK, type) == 0)
			ipc->type = SIPC_BASE_MBOX;
		else if (strcmp(PCIE_BAMK, type) == 0)
			ipc->type = SIPC_BASE_PCIE;
	}

	/* get sipc client, optional */
	if (of_property_read_u32_array(np, "sprd,client", val, 1) == 0) {
		ipc->client = (u8)val[0];
		pr_debug("sipc: client=%d\n", ipc->client);
	}

	/* get sipc dst */
	ret = of_property_read_u32_array(np, "sprd,dst", val, 1);
	if (!ret) {
		ipc->dst = (u8)val[0];
		pr_debug("sipc: dst =%d\n", ipc->dst);
	}

	if (ret || ipc->dst >= SIPC_ID_NR) {
		pr_err("sipc: dst err, ret =%d.\n", ret);
		return ret;
	}

#ifdef CONFIG_SPRD_MAILBOX
	if (ipc->type == SIPC_BASE_MBOX) {
		/* get core id */
		ipc->core_id = (u8)MBOX_INVALID_CORE;
		ret = of_property_read_u32_array(np, "sprd,core", val, 1);
		if (!ret) {
			ipc->core_id = (u8)val[0];
			pr_debug("sipc: core=%d\n", ipc->core_id);
		} else {
			pr_err("sipc: core err, ret =%d.\n", ret);
			return ret;
		}

		/* get core sensor id, optional*/
		ipc->core_sensor_id = (u8)MBOX_INVALID_CORE;
		if (of_property_read_u32_array(np, "sprd,core_sensor",
					       val, 1) == 0) {
			ipc->core_sensor_id = (u8)val[0];
			pr_debug("sipc: core_sensor=%d\n", ipc->core_sensor_id);
		}
	}
#endif

#ifdef CONFIG_SPRD_PCIE_EP_DEVICE
	if (ipc->type == SIPC_BASE_PCIE) {
		ipc->irq = PCIE_EP_SIPC_IRQ;
		ret = of_property_read_u32_array(np,
						 "sprd,ep-dev",
						 &ipc->ep_dev,
						 1);
		pr_debug("sipc: ep_dev=%d\n", ipc->ep_dev);
		if (ret || ipc->ep_dev >= PCIE_EP_NR) {
			pr_err("sipc: ep_dev err, ret =%d.\n", ret);
			return ret;
		}
	}
#endif

#ifdef CONFIG_PCIE_EPF_SPRD
	if (ipc->type == SIPC_BASE_PCIE) {
		ret = of_property_read_u32_array(np,
						"sprd,ep-fun",
						&ipc->ep_fun,
						1);
		pr_debug("sipc: ep_fun=%d\n", ipc->ep_fun);
		if (ret || ipc->ep_fun >= SPRD_FUNCTION_MAX) {
			pr_err("sipc: ep_fun err, ret =%d.\n", ret);
			return ret;
		}

		/* parse doolbell irq */
		ret = of_irq_get(np, 0);
		if (ret < 0) {
			pr_err("sipc: doorbell irq err, ret=%d\n", ret);
			return -EINVAL;
		}
		ipc->irq = ret;
		pr_debug("sipc: irq=%d\n", ipc->irq);
	}
#endif

	/* get smem type */
	ret = of_property_read_u32_array(np,
					 "sprd,smem-type",
					 &ipc->smem_type,
					 1);
	if (ret)
		ipc->smem_type = SMEM_LOCAL;

	pr_debug("sipc: smem_type = %d, ret =%d\n", ipc->smem_type, ret);

	/* get smem info */
	ret = of_property_read_u32_array(np,
					 "sprd,smem-info",
					 val,
					 3);
	if (ret) {
		pr_err("sipc: parse smem info failed.\n");
		return ret;
	}
	ipc->smem_base = val[0];
	ipc->dst_smem_base = val[1];
	ipc->smem_size = val[2];
	pr_debug("sipc: smem_base=0x%x, dst_smem_base=0x%x, smem_size=0x%x\n",
		ipc->smem_base, ipc->dst_smem_base, ipc->smem_size);

	/* try to get high_offset */
	ret = of_property_read_u32_array(np,
					 "sprd,high-offset",
					 val,
					 2);
	if (!ret) {
		ipc->high_offset = val[0];
		ipc->dst_high_offset = val[1];
	}
	pr_debug("sipc:  high_offset=0x%x, dst_high_offset=0x%x\n",
		ipc->high_offset, ipc->dst_high_offset);

	if (ipc->type == SIPC_BASE_PCIE) {
		/* need wait pcie linkup */
		ipc->suspend = 1;
		/* pcie sipc, the host must use loacal SMEM_LOCAL */
		if (!ipc->client && ipc->smem_type != SMEM_LOCAL) {
			pr_err("sipc: host must use local smem!");
			return -EINVAL;
		}
	}

	return 0;
}

static int sipc_probe(struct platform_device *pdev)
{
	struct smsg_ipc *ipc;
	struct device_node *np;

	if (pdev->dev.of_node) {
		np = pdev->dev.of_node;
		ipc = devm_kzalloc(&pdev->dev,
			sizeof(struct smsg_ipc),
			GFP_KERNEL);
		if (!ipc)
			return -ENOMEM;

		if (sipc_parse_dt(ipc, np, &pdev->dev)) {
			pr_err("%s: failed to parse dt!\n", __func__);
			return -ENODEV;
		}

		ipc->rxirq_status = sipc_rxirq_status;
		ipc->rxirq_clear = sipc_rxirq_clear;
		ipc->txirq_trigger = sipc_txirq_trigger;
		init_waitqueue_head(&ipc->suspend_wait);
		spin_lock_init(&ipc->suspend_pinlock);
		spin_lock_init(&ipc->txpinlock);

		smsg_ipc_create(ipc);
		platform_set_drvdata(pdev, ipc);
	}
	return 0;
}

static int sipc_remove(struct platform_device *pdev)
{
	struct smsg_ipc *ipc = platform_get_drvdata(pdev);

	smsg_ipc_destroy(ipc);

	devm_kfree(&pdev->dev, ipc);
	return 0;
}

static const struct of_device_id sipc_match_table[] = {
	{ .compatible = "sprd,sipc", },
	{ },
};

static struct platform_driver sipc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sipc",
		.of_match_table = sipc_match_table,
	},
	.probe = sipc_probe,
	.remove = sipc_remove,
};

static int __init sipc_init(void)
{
	smsg_init_channel2index();
	return platform_driver_register(&sipc_driver);
}

static void __exit sipc_exit(void)
{
	platform_driver_unregister(&sipc_driver);
}

subsys_initcall_sync(sipc_init);
module_exit(sipc_exit);

MODULE_AUTHOR("Wenping Zhou");
MODULE_DESCRIPTION("SIPC module driver");
MODULE_LICENSE("GPL v2");
