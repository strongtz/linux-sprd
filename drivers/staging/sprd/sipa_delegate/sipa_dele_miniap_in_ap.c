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
#include <linux/soc/sprd/sprd_pcie_ep_device.h>
#include "sipa_dele_priv.h"
#include "../sipa/sipa_hal_priv.h"
#include "../pam_ipa/pam_ipa_core.h"

static struct miniap_delegator *s_miniap_delegator;

int notify_pam_ipa_miniap_ready(void)
{
	struct sipa_to_pam_info info;
	struct sipa_delegate_plat_drv_cfg *cfg;
	phys_addr_t reg_mapped;

	/* map mini ap addresses */
	cfg = s_miniap_delegator->delegator.cfg;
	sprd_ep_ipa_map(PCIE_IPA_TYPE_MEM,
			cfg->mem_base,
			cfg->mem_end - cfg->mem_base);
	reg_mapped = sprd_ep_ipa_map(PCIE_IPA_TYPE_REG,
				     cfg->reg_base,
				     cfg->reg_end - cfg->reg_base);
	/* notify pam_ipa */
	info.term = SIPA_TERM_PCIE0;
	info.dl_fifo.rx_fifo_base_addr = s_miniap_delegator->dl_free_fifo_phy;
	info.dl_fifo.tx_fifo_base_addr = s_miniap_delegator->dl_filled_fifo_phy;
	info.dl_fifo.fifo_sts_addr = reg_mapped + ((SIPA_FIFO_PCIE_DL + 1) *
						   SIPA_FIFO_REG_SIZE);

	info.ul_fifo.rx_fifo_base_addr = s_miniap_delegator->ul_filled_fifo_phy;
	info.ul_fifo.tx_fifo_base_addr = s_miniap_delegator->ul_free_fifo_phy;

	info.ul_fifo.fifo_sts_addr = reg_mapped + ((SIPA_FIFO_PCIE_UL + 1) *
						   SIPA_FIFO_REG_SIZE);

	pam_ipa_on_miniap_ready(&info);
	return 0;
}

void miniap_dele_on_event(void *priv, u16 flag, u32 data)
{
	switch (flag) {
	case SMSG_FLG_DELE_ADDR_DL_TX:
		s_miniap_delegator->dl_filled_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		break;
	case SMSG_FLG_DELE_ADDR_DL_RX:
		s_miniap_delegator->dl_free_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		break;
	case SMSG_FLG_DELE_ADDR_UL_TX:
		s_miniap_delegator->ul_free_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		break;
	case SMSG_FLG_DELE_ADDR_UL_RX:
		s_miniap_delegator->ul_filled_fifo_phy =
			PAM_IPA_STI_64BIT(data,
					  PAM_IPA_DDR_MAP_OFFSET_H);
		/* received last evt, notify pam_ipa mini_ap is ready */
		s_miniap_delegator->ready = true;
		notify_pam_ipa_miniap_ready();
		break;
	default:
		break;
	}
}

void miniap_dele_on_close(void *priv, u16 flag, u32 data)
{
	/* call base class on_close func first */
	sipa_dele_on_close(priv, flag, data);

	s_miniap_delegator->ready = false;
}

int miniap_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_miniap_delegator = devm_kzalloc(params->pdev,
					  sizeof(*s_miniap_delegator),
					  GFP_KERNEL);
	if (!s_miniap_delegator)
		return -ENOMEM;
	ret = sipa_delegator_init(&s_miniap_delegator->delegator,
				  params);
	if (ret)
		return ret;

	s_miniap_delegator->delegator.on_close = miniap_dele_on_close;
	s_miniap_delegator->delegator.on_evt = miniap_dele_on_event;

	s_miniap_delegator->ready = false;

	sipa_delegator_start(&s_miniap_delegator->delegator);

	return 0;
}
