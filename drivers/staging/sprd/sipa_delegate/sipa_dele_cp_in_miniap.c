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
#include "sipa_dele_priv.h"
#include "../pam_ipa/pam_ipa_core.h"

static struct cp_delegator *s_cp_delegator;

static void cp_dele_on_commad(void *priv, u16 flag, u32 data)
{
	struct sipa_delegator *delegator = priv;

	pr_debug("prod_id:%d\n", delegator->prod_id);
	switch (flag) {
	case SMSG_FLG_DELE_ENABLE:
		sipa_set_enabled(true);
		sipa_dele_start_done_work(delegator,
					  SMSG_FLG_DELE_ENABLE,
					  SMSG_VAL_DELE_REQ_SUCCESS);
		break;
	case SMSG_FLG_DELE_DISABLE:
		/* do release operation */
		sipa_set_enabled(false);
		break;
	default:
		break;
	}
	/* do default operations */
	sipa_dele_on_commad(priv, flag, data);
}

static int cp_dele_local_req_r_prod(void *user_data)
{
	/* do enable ipa  operation */

	return sipa_dele_local_req_r_prod(user_data);
}

int cp_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_cp_delegator = devm_kzalloc(params->pdev,
				      sizeof(*s_cp_delegator),
				      GFP_KERNEL);
	if (!s_cp_delegator)
		return -ENOMEM;
	ret = sipa_delegator_init(&s_cp_delegator->delegator,
				  params);
	if (ret)
		return ret;

	s_cp_delegator->delegator.on_cmd = cp_dele_on_commad;
	s_cp_delegator->delegator.local_request_prod = cp_dele_local_req_r_prod;
	sipa_delegator_start(&s_cp_delegator->delegator);

	return 0;
}
