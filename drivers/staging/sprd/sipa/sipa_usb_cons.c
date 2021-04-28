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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/sipa.h>
#include "sipa_priv.h"
#include "sipa_hal.h"

static void sipa_usb_rm_notify_cb(void *user_data,
				  enum sipa_rm_event event,
				  unsigned long data)
{
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	pr_debug("%s: event %d\n", __func__, event);
	switch (event) {
	case SIPA_RM_EVT_GRANTED:
		complete(&ctrl->usb_rm_comp);
		break;
	case SIPA_RM_EVT_RELEASED:
		break;
	default:
		pr_err("%s: unknown event %d\n", __func__, event);
		break;
	}
}

int sipa_rm_usb_cons_init(void)
{
	struct sipa_rm_register_params r_param;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	init_completion(&ctrl->usb_rm_comp);

	r_param.user_data = ctrl;
	r_param.notify_cb = sipa_usb_rm_notify_cb;
	return sipa_rm_register(SIPA_RM_RES_CONS_USB, &r_param);
}
EXPORT_SYMBOL(sipa_rm_usb_cons_init);

void sipa_rm_usb_cons_deinit(void)
{
	struct sipa_rm_register_params r_param;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	r_param.user_data = ctrl;
	r_param.notify_cb = sipa_usb_rm_notify_cb;
	sipa_rm_deregister(SIPA_RM_RES_CONS_USB, &r_param);
}
EXPORT_SYMBOL(sipa_rm_usb_cons_deinit);

int sipa_rm_set_usb_eth_up(void)
{
	int ret;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	reinit_completion(&ctrl->usb_rm_comp);
	ret = sipa_rm_request_resource(SIPA_RM_RES_CONS_USB);
	if (ret) {
		if (ret != -EINPROGRESS)
			return ret;
		wait_for_completion(&ctrl->usb_rm_comp);
	}

	return ret;
}
EXPORT_SYMBOL(sipa_rm_set_usb_eth_up);

void sipa_rm_set_usb_eth_down(void)
{
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	reinit_completion(&ctrl->usb_rm_comp);

	sipa_rm_release_resource(SIPA_RM_RES_CONS_USB);

	sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
				  SIPA_RM_RES_PROD_IPA);
	sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
				  SIPA_RM_RES_PROD_PAM_IPA);
	sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
				  SIPA_RM_RES_PROD_MINI_AP);
}
EXPORT_SYMBOL(sipa_rm_set_usb_eth_down);

int sipa_rm_enable_usb_tether(void)
{
	int ret;

	ret = sipa_rm_add_dependency_sync(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_IPA);
	if (ret)
		return ret;

	ret = sipa_rm_add_dependency_sync(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_PAM_IPA);
	if (ret) {
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_IPA);
		return ret;
	}

	ret = sipa_rm_add_dependency_sync(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_MINI_AP);

	if (ret) {
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_IPA);
		sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
					  SIPA_RM_RES_PROD_PAM_IPA);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(sipa_rm_enable_usb_tether);
