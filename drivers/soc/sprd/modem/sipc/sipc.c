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

#ifdef CONFIG_SPRD_MAILBOX
#include <linux/sprd_mailbox.h>
#else
#define SPRD_DEV_P2V(paddr)	(paddr)
#define SPRD_DEV_V2P(vaddr)	(vaddr)
#endif

#include "sipc_priv.h"

#define SMSG_TXBUF_ADDR		(0)
#define SMSG_TXBUF_SIZE		(SZ_1K)
#define SMSG_RXBUF_ADDR		(SMSG_TXBUF_SIZE)
#define SMSG_RXBUF_SIZE		(SZ_1K)

#define SMSG_RINGHDR		(SMSG_TXBUF_SIZE + SMSG_RXBUF_SIZE)
#define SMSG_TXBUF_RDPTR	(SMSG_RINGHDR + 0)
#define SMSG_TXBUF_WRPTR	(SMSG_RINGHDR + 4)
#define SMSG_RXBUF_RDPTR	(SMSG_RINGHDR + 8)
#define SMSG_RXBUF_WRPTR	(SMSG_RINGHDR + 12)

struct sipc_core sipc_ap;
EXPORT_SYMBOL_GPL(sipc_ap);

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

/* if it's upon mailbox arch, overwrite the implementation*/
#ifdef CONFIG_SPRD_MAILBOX
static u32 sipc_rxirq_status(u8 id)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	return mbox_core_fifo_full(info->core_id);
}

static void sipc_rxirq_clear(u8 id)
{
}

static void sipc_txirq_trigger(u8 id, u64 msg)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	mbox_raw_sent(info->core_id, msg);
}
#else
static u32 sipc_rxirq_status(u8 id)
{
	return 1;
}

static void sipc_rxirq_clear(u8 id)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	__raw_writel(info->ap2cp_bit_clr,
		     (__force void __iomem *)(unsigned long)
		     info->cp2ap_int_ctrl);
}

static void sipc_txirq_trigger(u8 id)
{
	struct sipc_child_node_info *info = sipc_ap.sipc_tags[id];

	writel_relaxed(info->ap2cp_bit_trig,
		     (__force void __iomem *)(unsigned long)
		     info->ap2cp_int_ctrl);
}

#endif
static int sipc_create(struct sipc_device *sipc)
{
	struct sipc_init_data *pdata = sipc->pdata;
	struct smsg_ipc *inst;
	struct sipc_child_node_info *info;
	void __iomem *base;
	u32 num;
	int ret = 0, i, j = 0;

	if (!pdata)
		return -ENODEV;

	num = pdata->chd_nr;
	inst = sipc->smsg_inst;
	info = pdata->info_table;

	for (i = 0; i < num; i++) {
		if (j < pdata->newchd_nr && info[i].is_new) {
			if (!info[i].mode) {
				base = (void __iomem *)shmem_ram_vmap_nocache(
							(u32)info[i].ring_base,
							info[i].ring_size);
				if (!base) {
					pr_info("sipc chd%d ioremap return 0\n",
						i);
					return -ENOMEM;
				}
				info[i].smem_vbase = (void *)base;

				pr_info("sipc:[tag%d] after ioremap vbase=0x%p, pbase=0x%x, size=0x%x\n",
					j, base,
					info[i].ring_base,
					info[i].ring_size);
				inst[j].txbuf_size = SMSG_TXBUF_SIZE /
					sizeof(struct smsg);
				inst[j].txbuf_addr = (uintptr_t)base +
					SMSG_TXBUF_ADDR;
				inst[j].txbuf_rdptr = (uintptr_t)base +
					SMSG_TXBUF_RDPTR;
				inst[j].txbuf_wrptr = (uintptr_t)base +
					SMSG_TXBUF_WRPTR;

				inst[j].rxbuf_size = SMSG_RXBUF_SIZE /
					sizeof(struct smsg);
				inst[j].rxbuf_addr = (uintptr_t)base +
					SMSG_RXBUF_ADDR;
				inst[j].rxbuf_rdptr = (uintptr_t)base +
					SMSG_RXBUF_RDPTR;
				inst[j].rxbuf_wrptr = (uintptr_t)base +
					SMSG_RXBUF_WRPTR;
			}

			inst[j].id = sipc_ap.sipc_tag_ids;
			sipc_ap.sipc_tags[sipc_ap.sipc_tag_ids] = &info[i];
			sipc_ap.sipc_tag_ids++;

			ret = smsg_ipc_create(inst[j].dst, &inst[j]);

			pr_info("sipc:[tag%d] created, dst = %d\n",
				j, inst[j].dst);
			j++;
			if (ret)
				break;
		}
	}
	return ret;
}

static int sipc_get_smem_info(struct sipc_init_data *pdata,
			      struct device_node *np)
{
	struct smem_item *smem_ptr;
	int i, count;
	const __be32 *list;

	list = of_get_property(np, "sprd,smem-info", &count);
	if (!list || !count) {
		pr_err("no smem-info\n");
		return -ENODEV;
	}

	count = count / sizeof(*smem_ptr);
	smem_ptr = kcalloc(count,
			   sizeof(*smem_ptr),
			   GFP_KERNEL);
	if (!smem_ptr)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		smem_ptr[i].base = be32_to_cpu(*list++);
		smem_ptr[i].mapped_base = be32_to_cpu(*list++);
		smem_ptr[i].size = be32_to_cpu(*list++);
		pr_debug("sipc:smem=%d, base=0x%x, dstbase=0x%x, size=0x%x\n",
			 i, smem_ptr[i].base,
			 smem_ptr[i].mapped_base, smem_ptr[i].size);
	}

	pdata->smem_cnt = count;
	pdata->smem_ptr = smem_ptr;

	/* default mem */
	pdata->smem_base = smem_ptr[0].base;
	pdata->mapped_smem_base = smem_ptr[0].mapped_base;
	pdata->smem_size = smem_ptr[0].size;

	return 0;
}

static int sipc_parse_dt(struct sipc_init_data **init, struct device_node *node)
{
#ifdef CONFIG_SPRD_MAILBOX
	u32 val[3];
	int ret = -1;
	struct sipc_init_data *pdata;
	struct sipc_child_node_info *info;
	struct device_node *np = node;

	pdata = kzalloc(sizeof(struct sipc_init_data) +
			     sizeof(struct sipc_child_node_info),
			     GFP_KERNEL);
	if (!pdata) {
		pr_err("sipc: failed to alloc mem for pdata\n");
		return -ENOMEM;
	}

	pdata->is_alloc = 1;
	pdata->chd_nr = 1;
	info = pdata->info_table;
	info->mode = 1;

	ret = of_property_read_string(np,
				      "sprd,name",
				      (const char **)&info->name);
	if (ret)
		goto error;
	pr_info("sipc: name=%s\n", info->name);

	ret = of_property_read_u32_array(np, "sprd,dst", val, 3);
	if (!ret)
		info->core_sensor_id = (u8)val[2];
	else
		info->core_sensor_id = (u8)RECV_MBOX_SENSOR_ID;

	pr_info("sipc: core_sensor_id = %u\n", info->core_sensor_id);

	if (ret) {
		ret = of_property_read_u32_array(np,
						 "sprd,dst", val, 2);
		if (ret) {
			pr_err("sipc: parse dst info failed.\n");
			goto error;
		}
	}

	info->dst = (u8)val[0];
	info->core_id = (u8)val[1];

	pr_info("sipc: dst = %u, core_id = %u\n", info->dst, info->core_id);
	if (info->dst >= SIPC_ID_NR) {
		pr_err("sipc: dst info is invalid.\n");
		goto error;
	}

	if (!smsg_ipcs[info->dst]) {
		pdata->newchd_nr++;
		info->is_new = 1;
	} else {
		info->is_new = 0;
	}

	ret = sipc_get_smem_info(pdata, np);
	if (ret)
		goto error;

	*init = pdata;

	return 0;

error:
	kfree(pdata);

	return ret;
#else
	return -EINVAL;
#endif
}

static void sipc_destroy_pdata(struct sipc_init_data **ppdata,
			       struct device *dev)
{
	struct sipc_init_data *pdata = *ppdata;
	struct sipc_child_node_info *info;
	int i, num;

	if (pdata) {
		num = pdata->chd_nr;
		for (i = 0; i < num; i++) {
			info = pdata->info_table;
			if (info[i].smem_vbase)
				shmem_ram_unmap(info[i].smem_vbase);
		}
		if (pdata->is_alloc)
			devm_kfree(dev, pdata);
	}
}

static const struct of_device_id sipc_match_table[] = {
	{.compatible = "sprd,sipc", .data = sipc_parse_dt, },
	{ },
};

static int sipc_probe(struct platform_device *pdev)
{
	struct sipc_init_data *pdata = pdev->dev.platform_data;
	struct sipc_device *sipc;
	struct sipc_child_node_info *info;
	struct smsg_ipc *smsg;
	const struct of_device_id *of_id;
	int (*parse)(struct sipc_init_data **, struct device_node *);
	u32 num, dst;
	int i, j = 0;
	struct device_node *np, *chd;
	int segnr;
	struct smem_item *smem_ptr;

	if (!pdata && pdev->dev.of_node) {
		of_id = of_match_node(sipc_match_table, pdev->dev.of_node);
		if (!of_id) {
			pr_err("sipc: failed to get of_id\n");
			return -ENODEV;
		}

		np = pdev->dev.of_node;
		segnr = of_get_child_count(np);
		pr_info("%s: segnr = %d\n", __func__, segnr);
		parse = (int(*)(struct sipc_init_data **,
					struct device_node *))of_id->data;
		for_each_child_of_node(np, chd) {
			if (parse && parse(&pdata, chd)) {
				pr_err("sipc: failed to parse dt, parse(0x%p)\n",
						parse);
				return -ENODEV;
			}

			sipc = devm_kzalloc(&pdev->dev, sizeof(struct sipc_device), GFP_KERNEL);
			if (!sipc) {
				sipc_destroy_pdata(&pdata, &pdev->dev);
				return -ENOMEM;
			}

			num = pdata->chd_nr;
			smsg = devm_kzalloc(&pdev->dev,
					    pdata->newchd_nr * sizeof(struct smsg_ipc),
					    GFP_KERNEL);
			if (!smsg) {
				sipc_destroy_pdata(&pdata, &pdev->dev);
				devm_kfree(&pdev->dev, sipc);
				return -ENOMEM;
			}
			pr_info("sipc: tag count = %d\n", num);
			info = pdata->info_table;
			j = 0;
			for (i = 0; i < num; i++) {
				if (j < pdata->newchd_nr && info[i].is_new) {
					smsg[j].name = info[i].name;
					smsg[j].dst = info[i].dst;
#ifdef CONFIG_SPRD_MAILBOX
					smsg[j].core_id = info[i].core_id;
					smsg[j].core_sensor_id = info[i].core_sensor_id;
#else
					smsg[j].irq = info[i].irq;
#endif
					smsg[j].rxirq_status = sipc_rxirq_status;
					smsg[j].rxirq_clear = sipc_rxirq_clear;
					smsg[j].txirq_trigger = sipc_txirq_trigger;

#ifdef CONFIG_SPRD_MAILBOX
					pr_info("sipc:[tag%d] smsg name=%s, dst=%u, core_id=%d\n",
						j, smsg[j].name, smsg[j].dst, smsg[j].core_id);
#else
					pr_info("sipc:[tag%d] smsg name=%s, dst=%u, irq=%d\n",
						j, smsg[j].name, smsg[j].dst, smsg[j].irq);
#endif
					j++;
				}
			}

			sipc->pdata = pdata;
			sipc->smsg_inst = smsg;
			dst = pdata->info_table->dst;
			sipc_ap.sipc_dev[dst] = sipc;

			pr_info("sipc: smem_init smem_base=0x%x, smem_size=0x%x\n",
				pdata->smem_base, pdata->smem_size);
			if (dst == SIPC_ID_LTE ||
			    dst == SIPC_ID_CPW)
				smem_set_default_pool(pdata->smem_base);

			smem_ptr = pdata->smem_ptr;
			for (i = 0; i < pdata->smem_cnt; i++)
				smem_init(smem_ptr[i].base,
					  smem_ptr[i].size, dst);

			smsg_suspend_init();

			sipc_create(sipc);

			platform_set_drvdata(pdev, sipc);
		}
	}
	return 0;
}

static int sipc_remove(struct platform_device *pdev)
{
	struct sipc_device *sipc = platform_get_drvdata(pdev);

	sipc_destroy_pdata(&sipc->pdata, &pdev->dev);
	kfree(sipc->pdata->smem_ptr);
	devm_kfree(&pdev->dev, sipc->smsg_inst);
	devm_kfree(&pdev->dev, sipc);
	return 0;
}

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
	return platform_driver_register(&sipc_driver);
}

static void __exit sipc_exit(void)
{
	platform_driver_unregister(&sipc_driver);
}

subsys_initcall_sync(sipc_init);
module_exit(sipc_exit);

MODULE_AUTHOR("Qiu Yi");
MODULE_DESCRIPTION("SIPC module driver");
MODULE_LICENSE("GPL");
