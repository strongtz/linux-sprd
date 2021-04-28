/*
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

#define pr_fmt(fmt) "sipa_dele: %s " fmt, __func__

#include <linux/sipa.h>
#include <linux/sipc.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sipc.h>
#include <linux/sipa.h>
#include "sipa_dele_priv.h"
#include "../pam_ipa/pam_ipa_core.h"

#define FIFO_NODE_BYTES	16

static struct ap_delegator *s_ap_delegator;

static int sipa_alloc_fifo_mem(struct sipa_ext_fifo_params *param)
{
	param->rx_fifo_pal = smem_alloc(SIPC_ID_PSCP,
					param->rx_depth * FIFO_NODE_BYTES);
	if (!param->rx_fifo_pal)
		return -ENOMEM;
	param->rx_fifo_pah = 0;
	param->rx_fifo_va =
		shmem_ram_vmap_nocache(SIPC_ID_PSCP,
				       param->rx_fifo_pal,
				       param->rx_depth * FIFO_NODE_BYTES);
	if (!param->rx_fifo_va)
		goto mem_fail;

	param->tx_fifo_pal = smem_alloc(SIPC_ID_PSCP,
					param->tx_depth * FIFO_NODE_BYTES);
	if (!param->tx_fifo_pal)
		goto mem_fail;
	param->tx_fifo_pah = 0;
	param->tx_fifo_va =
		shmem_ram_vmap_nocache(SIPC_ID_PSCP,
				       param->tx_fifo_pal,
				       param->tx_depth * FIFO_NODE_BYTES);
	if (!param->tx_fifo_va)
		goto mem_fail;

	return 0;

mem_fail:
	if (param->rx_fifo_va)
		shmem_ram_unmap(SIPC_ID_PSCP, param->rx_fifo_va);
	if (param->rx_fifo_pal)
		smem_free(SIPC_ID_PSCP,
			  param->rx_fifo_pal,
			  param->rx_depth * FIFO_NODE_BYTES);
	if (param->tx_fifo_pal)
		smem_free(SIPC_ID_PSCP,
			  param->tx_fifo_pal,
			  param->tx_depth * FIFO_NODE_BYTES);

	return -ENOMEM;
}

void ap_dele_on_open(void *priv, u16 flag, u32 data)
{
	int ret;
	struct smsg msg;
	struct sipa_pcie_open_params param = {};
	struct sipa_ext_fifo_params *recv;
	struct sipa_ext_fifo_params *send;
	struct sipa_delegator *delegator = priv;

	/* call base class on_open func first */
	sipa_dele_on_open(priv, flag, data);

	/* alloc memory for pcie sipa ep */
	recv = &param.ext_recv_param;
	recv->rx_depth = delegator->cfg->dl_fifo_depth;
	recv->tx_depth = delegator->cfg->dl_fifo_depth;
	ret = sipa_alloc_fifo_mem(recv);
	if (ret)
		pr_err("alloc recv fifo mem fail\n");
	send = &param.ext_send_param;
	send->rx_depth = delegator->cfg->ul_fifo_depth;
	send->tx_depth = delegator->cfg->ul_fifo_depth;
	ret = sipa_alloc_fifo_mem(send);
	if (ret)
		pr_err("alloc send fifo mem fail\n");

	/* open pcie ep */
	param.id = SIPA_EP_PCIE;
	param.recv_param.intr_to_ap = 0;
	param.recv_param.tx_intr_delay_us = 5;
	param.recv_param.tx_intr_threshold = 5;
	param.send_param.intr_to_ap = 0;
	param.send_param.tx_intr_delay_us = 5;
	param.send_param.tx_intr_threshold = 5;

	ret = sipa_ext_open_pcie(&param);
	if (ret)
		pr_err("open pcie ip fail\n");

	/* dl tx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_DL_TX,
		 recv->tx_fifo_pal);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("dl tx smsg send fail %d\n", ret);

	/* dl rx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_DL_RX,
		 recv->rx_fifo_pal);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("dl rx smsg send fail %d\n", ret);
	/* ul tx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_UL_TX,
		 send->tx_fifo_pal);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("ul tx smsg send fail %d\n", ret);
	/* ul rx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_UL_RX,
		 send->rx_fifo_pal);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("ul rx smsg send fail %d\n", ret);
}

int ap_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_ap_delegator = devm_kzalloc(params->pdev,
				      sizeof(*s_ap_delegator),
				      GFP_KERNEL);
	if (!s_ap_delegator)
		return -ENOMEM;
	ret = sipa_delegator_init(&s_ap_delegator->delegator,
				  params);
	if (ret)
		return ret;

	s_ap_delegator->delegator.on_open = ap_dele_on_open;

	sipa_delegator_start(&s_ap_delegator->delegator);

	return 0;
}
