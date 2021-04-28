/*
 * sipa  driver debugfs support
 *
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include<linux/pm_runtime.h>
#include<linux/regmap.h>
#include <linux/sipa.h>
#include "sipa_debug.h"
#include "sipa_priv.h"

static int sipa_regdump_show(struct seq_file *s, void *unused)
{
	struct sipa_control *ctrl = s->private;
	struct sipa_plat_drv_cfg *sipa = &ctrl->params_cfg;
	struct sipa_hal_context *hal_cfg = ctrl->ctx->hdl;
	void __iomem *glbbase = hal_cfg->phy_virt_res.glb_base;
	const struct sipa_hw_data *sipa_regmap = sipa->debugfs_data;
	unsigned int i, val;

	seq_puts(s, "Sipa Ahb Register\n");

	for (i = 0; i < sipa_regmap->ahb_regnum; i++) {
		switch (sipa_regmap->ahb_reg[i].size) {
		case 8:
			regmap_read(sipa->sys_regmap,
				    sipa_regmap->ahb_reg[i].offset, &val);
			seq_printf(s, "%-12s: %02x\n",
				   sipa_regmap->ahb_reg[i].name, val);
			break;
		case 16:
			regmap_read(sipa->sys_regmap,
				    sipa_regmap->ahb_reg[i].offset, &val);
			seq_printf(s, "%-12s: %02x\n",
				   sipa_regmap->ahb_reg[i].name, val);
			break;
		case 32:
			regmap_read(sipa->sys_regmap,
				    sipa_regmap->ahb_reg[i].offset, &val);
			seq_printf(s, "%-12s:    0x%02x\n",
				   sipa_regmap->ahb_reg[i].name, val);
			break;
		}
	}
	seq_puts(s,
		 "****************Sipa Glb Register ***********************\n");

	for (i = 0; i < ARRAY_SIZE(sipa_glb_regmap); i++) {
		val = readl_relaxed(glbbase + sipa_glb_regmap[i].offset);
		seq_printf(s, "%-12s:	0x%02x\n", sipa_glb_regmap[i].name,
			   val);
	}

	return 0;
}

static int sipa_regdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_regdump_show, inode->i_private);
}

static const struct file_operations sipa_regdump_fops = {
	.open = sipa_regdump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_commonfifo_show(struct seq_file *s, void *unused)
{
	struct sipa_control *ctrl = s->private;
	struct sipa_hal_context *hal_cfg = ctrl->ctx->hdl;
	void __iomem *glbbase = hal_cfg->phy_virt_res.glb_base;
	unsigned int i, j, val;

	seq_puts(s, "Sipa commonfifo Register\n");

	for (i = 0; i < ARRAY_SIZE(sipa_common_fifo_map); i++) {
		seq_printf(s, "%-12s*******************************\n",
			   sipa_common_fifo_map[i].name);
		for (j = 0; j < ARRAY_SIZE(sipa_fifo_iterm_map); j++) {
			val = readl_relaxed(glbbase +
					    sipa_common_fifo_map[i].offset +
					    sipa_fifo_iterm_map[j].offset);
			seq_printf(s, "%-12s:	0x%02x\n",
				   sipa_fifo_iterm_map[j].name, val);
		}
	}

	return 0;
}

static int sipa_commonfifo_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_commonfifo_show, inode->i_private);
}

static const struct file_operations sipa_commonfifo_fops = {
	.open = sipa_commonfifo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_flow_ctrl_show(struct seq_file *s, void *unused)
{
	struct sipa_control *ipa = s->private;
	struct sipa_skb_sender *eth_sender =
		ipa->sender[SIPA_PKT_ETH];
	struct sipa_skb_sender *ip_sender =
		ipa->sender[SIPA_PKT_IP];
	struct sipa_skb_receiver *eth_recv =
		ipa->receiver[SIPA_PKT_ETH];
	struct sipa_skb_receiver *ip_recv =
		ipa->receiver[SIPA_PKT_IP];

	seq_printf(s, "[SEND_ETH] no_mem_cnt = %d no_free_cnt = %d\n",
		   eth_sender->no_mem_cnt, eth_sender->no_free_cnt);

	seq_printf(s, "[SEND_ETH] enter_flow_ctrl_cnt = %d\n",
		   eth_sender->enter_flow_ctrl_cnt);

	seq_printf(s, "[SEND_ETH] exit_flow_ctrl_cnt = %d\n",
		   eth_sender->exit_flow_ctrl_cnt);

	seq_printf(s, "[SEND_IP] no_mem_cnt = %d no_free_cnt = %d\n",
		   ip_sender->no_mem_cnt, ip_sender->no_free_cnt);

	seq_printf(s, "[SEND_IP] enter_flow_ctrl_cnt = %d\n",
		   ip_sender->enter_flow_ctrl_cnt);

	seq_printf(s, "[SEND_IP] exit_flow_ctrl_cnt = %d\n",
		   ip_sender->exit_flow_ctrl_cnt);

	seq_printf(s, "[RECV_ETH] tx_danger_cnt = %d rx_danger_cnt = %d\n",
		   eth_recv->tx_danger_cnt, eth_recv->rx_danger_cnt);

	seq_printf(s, "[RECV_ETH] need_fill_cnt = %d\n",
		   atomic_read(&eth_recv->need_fill_cnt));

	seq_printf(s, "[RECV_IP] tx_danger_cnt = %d rx_danger_cnt = %d\n",
		   ip_recv->tx_danger_cnt, ip_recv->rx_danger_cnt);

	seq_printf(s, "[RECV_IP] need_fill_cnt = %d\n",
		   atomic_read(&ip_recv->need_fill_cnt));

	return 0;
}

static int sipa_flow_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_flow_ctrl_show, inode->i_private);
}

static const struct file_operations sipa_flow_ctrl_fops = {
	.open = sipa_flow_ctrl_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_no_recycled_show(struct seq_file *s, void *unused)
{
	struct sipa_control *ipa = s->private;
	struct sipa_skb_sender *sender;
	struct sipa_skb_dma_addr_node *iter, *_iter;

	sender = ipa->sender[SIPA_PKT_ETH];
	if (list_empty(&sender->sending_list)) {
		seq_puts(s, "[ETH_SEND] sending list is empty\n");
	} else {
		list_for_each_entry_safe(iter, _iter,
					 &sender->sending_list, list)
			seq_printf(s,
				   "[ETH_SEND] skb = 0x%p, phy addr = 0x%llx\n",
				   iter->skb, (u64)iter->dma_addr);
	}

	sender = ipa->sender[SIPA_PKT_IP];
	if (list_empty(&sender->sending_list)) {
		seq_puts(s, "[IP_SEND] sending list is empty\n");
	} else {
		list_for_each_entry_safe(iter, _iter,
					 &sender->sending_list, list)
			seq_printf(s,
				   "[IP_SEND] skb = 0x%p, phy addr = 0x%llx\n",
				   iter->skb, (u64)iter->dma_addr);
	}

	return 0;
}

static int sipa_no_recycled_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_no_recycled_show, inode->i_private);
}

static const struct file_operations sipa_no_recycled_fops = {
	.open = sipa_no_recycled_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_nic_debug_show(struct seq_file *s, void *unused)
{
	int i;
	struct sipa_control *ipa = (struct sipa_control *)s->private;

	for (i = 0; i < SIPA_NIC_MAX; i++) {
		if (!ipa->nic[i])
			continue;

		seq_printf(s, "open = %d src_mask = %d netid = %d flow_ctrl_status = %d\n",
			   atomic_read(&ipa->nic[i]->status),
			   ipa->nic[i]->src_mask,
			   ipa->nic[i]->netid,
			   ipa->nic[i]->flow_ctrl_status);
	}

	return 0;
}

static int sipa_nic_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_nic_debug_show, inode->i_private);
}

static const struct file_operations sipa_nic_debug_fops = {
	.open = sipa_nic_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sipa_init_debugfs(struct sipa_plat_drv_cfg *sipa,
		      struct sipa_control *ctrl)
{
	struct dentry *root;
	struct dentry *file;
	int ret;

	root = debugfs_create_dir(dev_name(sipa->dev), NULL);
	if (!root)
		return -ENOMEM;

	file = debugfs_create_file("regdump", 0444, root, ctrl,
				   &sipa_regdump_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("commonfifo_reg", 0444, root, ctrl,
				   &sipa_commonfifo_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("flow_ctrl", 0444, root, ctrl,
				   &sipa_flow_ctrl_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("nic", 0444, root, ctrl,
				   &sipa_nic_debug_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	file = debugfs_create_file("no_recycled", 0444, root, ctrl,
				   &sipa_no_recycled_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	debugfs_create_symlink("sipa", NULL,
			       "/sys/kernel/debug/local_ipa");

	sipa->debugfs_root = root;

	return 0;

err1:
	debugfs_remove_recursive(root);

	return ret;
}

void sipa_exit_debugfs(struct sipa_plat_drv_cfg *sipa)
{
	debugfs_remove_recursive(sipa->debugfs_root);
}
