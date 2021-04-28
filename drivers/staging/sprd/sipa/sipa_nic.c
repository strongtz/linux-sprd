/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "sipa_priv.h"
#include "sipa_hal.h"

#define SIPA_CP_SRC ((1 << SIPA_TERM_VAP0) | (1 << SIPA_TERM_VAP1) |\
		(1 << SIPA_TERM_VAP2) | (1 << SIPA_TERM_CP0) | \
		(1 << SIPA_TERM_CP1) | (1 << SIPA_TERM_VCP))

#define SIPA_NIC_RM_INACTIVE_TIMER	1000

struct sipa_nic_statics_info {
	enum sipa_ep_id send_ep;
	enum sipa_xfer_pkt_type pkt_type;
	u32 src_mask;
	int netid;
	enum sipa_rm_res_id cons;
};

static struct sipa_nic_statics_info s_spia_nic_statics[SIPA_NIC_MAX] = {
	{
		.send_ep = SIPA_EP_AP_ETH,
		.pkt_type = SIPA_PKT_ETH,
		.src_mask = (1 << SIPA_TERM_USB),
		.netid = -1,
		.cons = SIPA_RM_RES_CONS_USB,
	},
	{
		.send_ep = SIPA_EP_AP_ETH,
		.pkt_type = SIPA_PKT_ETH,
		.src_mask = (1 << SIPA_TERM_WIFI),
		.netid = -1,
		.cons = SIPA_RM_RES_CONS_WLAN,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 0,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 1,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 2,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 3,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 4,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 5,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 6,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 7,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
};

static void sipa_nic_rm_res_release(struct sipa_nic *nic);

static void sipa_nic_rm_res_granted(struct sipa_nic *nic)
{
	unsigned long flags;
	struct sipa_nic_cons_res *res = &nic->rm_res;

	spin_lock_irqsave(&res->lock, flags);
	res->request_in_progress = false;
	spin_unlock_irqrestore(&res->lock, flags);
}

static void sipa_nic_rm_notify_cb(void *user_data,
				  enum sipa_rm_event event,
				  unsigned long data)
{
	struct sipa_nic *nic = user_data;

	pr_debug("%s: event %d\n", __func__, event);
	switch (event) {
	case SIPA_RM_EVT_GRANTED:
		sipa_nic_rm_res_granted(nic);
		if (atomic_read(&nic->status) == NIC_OPEN)
			nic->cb(nic->cb_priv, SIPA_LEAVE_FLOWCTRL, 0);

		sipa_nic_rm_res_release(nic);
		break;
	case SIPA_RM_EVT_RELEASED:
		break;
	default:
		pr_err("%s: unknown event %d\n", __func__, event);
		break;
	}
}

static int sipa_nic_register_rm(struct sipa_nic *nic,
				enum sipa_nic_id nic_id)
{
	struct sipa_rm_register_params r_param;

	if (s_spia_nic_statics[nic_id].cons != SIPA_RM_RES_CONS_WWAN_UL)
		return 0;

	r_param.user_data = nic;
	r_param.notify_cb = sipa_nic_rm_notify_cb;

	return sipa_rm_register(SIPA_RM_RES_CONS_WWAN_UL, &r_param);
}

static int sipa_nic_deregister_rm(struct sipa_nic *nic,
				  enum sipa_nic_id nic_id)
{
	struct sipa_rm_register_params r_param;

	if (s_spia_nic_statics[nic_id].cons != SIPA_RM_RES_CONS_WWAN_UL)
		return 0;

	r_param.user_data = nic;
	r_param.notify_cb = sipa_nic_rm_notify_cb;

	return sipa_rm_deregister(SIPA_RM_RES_CONS_WWAN_UL, &r_param);
}

static void sipa_nic_rm_timer_func(struct work_struct *work)
{
	struct sipa_nic_cons_res *res = container_of(to_delayed_work(work),
						     struct sipa_nic_cons_res,
						     work);
	unsigned long flags;

	pr_debug("timer expired for resource %d!\n",
		 res->cons);

	spin_lock_irqsave(&res->lock, flags);
	/* need check resource not used any more */
	if (res->reschedule_work || !res->chk_func(res->chk_priv)) {
		pr_debug("setting delayed work\n");
		res->reschedule_work = false;
		queue_delayed_work(system_unbound_wq,
				   &res->work,
				   res->jiffies);
	} else if (res->resource_requested) {
		pr_debug("not calling release\n");
		res->release_in_progress = false;
	} else {
		pr_debug("calling release_resource on resource %d!\n",
			 res->cons);
		sipa_rm_release_resource(res->cons);
		res->need_request = true;
		res->release_in_progress = false;
	}
	spin_unlock_irqrestore(&res->lock, flags);
}

static int sipa_nic_rm_init(struct sipa_nic_cons_res *res,
			    struct sipa_skb_sender *sender,
			    enum sipa_rm_res_id cons,
			    unsigned long msecs)
{
	if (res->initied)
		return -EEXIST;

	res->initied = true;
	res->cons = cons;
	spin_lock_init(&res->lock);
	res->chk_func = (sipa_check_send_completed)
		sipa_skb_sender_check_send_complete;
	res->chk_priv = sender;
	res->jiffies = msecs_to_jiffies(msecs);
	res->resource_requested = false;
	res->reschedule_work = false;
	res->release_in_progress = false;
	res->need_request = true;
	res->request_in_progress = false;

	INIT_DELAYED_WORK(&res->work,
			  sipa_nic_rm_timer_func);

	return 0;
}

static int sipa_nic_rm_res_request(struct sipa_nic *nic)
{
	int ret = 0;
	unsigned long flags;
	struct sipa_nic_cons_res *res = &nic->rm_res;

	spin_lock_irqsave(&res->lock, flags);
	res->resource_requested = true;
	if (res->need_request) {
		res->need_request = false;
		ret = sipa_rm_request_resource(nic->rm_res.cons);
		if (ret == -EINPROGRESS)
			res->request_in_progress = true;
		else
			res->request_in_progress = false;
	} else {
		ret = res->request_in_progress ? -EINPROGRESS : 0;
	}
	spin_unlock_irqrestore(&res->lock, flags);

	return ret;
}

static void sipa_nic_rm_res_release(struct sipa_nic *nic)
{
	unsigned long flags;
	struct sipa_nic_cons_res *res = &nic->rm_res;

	spin_lock_irqsave(&res->lock, flags);
	res->resource_requested = false;
	if (res->release_in_progress) {
		res->reschedule_work = true;
		spin_unlock_irqrestore(&res->lock, flags);
		return;
	}

	res->release_in_progress = true;
	res->reschedule_work = false;
	queue_delayed_work(system_unbound_wq,
			   &res->work,
			   res->jiffies);
	spin_unlock_irqrestore(&res->lock, flags);
}

int sipa_nic_open(enum sipa_term_type src, int netid,
		  sipa_notify_cb cb, void *priv)
{
	int i, ret;
	struct sipa_nic *nic = NULL;
	struct sk_buff *skb;
	enum sipa_nic_id nic_id = SIPA_NIC_MAX;
	struct sipa_skb_receiver *receiver;
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	for (i = 0; i < SIPA_NIC_MAX; i++) {
		if ((s_spia_nic_statics[i].src_mask & (1 << src)) &&
			netid == s_spia_nic_statics[i].netid) {
			nic_id = i;
			break;
		}
	}
	dev_info(ctrl->ctx->pdev, "%s nic_id = %d\n", __func__, nic_id);
	if (nic_id == SIPA_NIC_MAX)
		return -EINVAL;

	if (ctrl->nic[nic_id]) {
		nic = ctrl->nic[nic_id];
		if  (atomic_read(&nic->status) == NIC_OPEN)
			return -EBUSY;
		while ((skb = skb_dequeue(&nic->rx_skb_q)) != NULL)
			dev_kfree_skb_any(skb);
	} else {
		nic = kzalloc(sizeof(*nic), GFP_KERNEL);
		if (!nic)
			return -ENOMEM;
		ctrl->nic[nic_id] = nic;
		skb_queue_head_init(&nic->rx_skb_q);
	}

	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];

	/* sipa rm operations */
	sipa_nic_rm_init(&nic->rm_res,
			 sender,
			 s_spia_nic_statics[nic_id].cons,
			 SIPA_NIC_RM_INACTIVE_TIMER);
	ret = sipa_nic_register_rm(nic, nic_id);
	if (ret)
		return ret;

	atomic_set(&nic->status, NIC_OPEN);
	nic->nic_id = nic_id;
	nic->send_ep = ctrl->eps[s_spia_nic_statics[nic_id].send_ep];
	nic->need_notify = 0;
	nic->src_mask = s_spia_nic_statics[i].src_mask;
	nic->netid = netid;
	nic->cb = cb;
	nic->cb_priv = priv;

	/* every receiver may receive cp packets */
	receiver = ctrl->receiver[s_spia_nic_statics[nic_id].pkt_type];
	sipa_receiver_add_nic(receiver, nic);

	if (SIPA_PKT_IP == s_spia_nic_statics[nic_id].pkt_type) {
		receiver = ctrl->receiver[SIPA_PKT_ETH];
		sipa_receiver_add_nic(receiver, nic);
	}

	sipa_skb_sender_add_nic(sender, nic);

	return nic_id;
}
EXPORT_SYMBOL(sipa_nic_open);

void sipa_nic_close(enum sipa_nic_id nic_id)
{
	struct sipa_nic *nic = NULL;
	struct sk_buff *skb;
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return;
	}
	if (nic_id == SIPA_NIC_MAX || !ctrl->nic[nic_id])
		return;

	nic = ctrl->nic[nic_id];

	sipa_nic_deregister_rm(nic, nic_id);
	atomic_set(&nic->status, NIC_CLOSE);
	/* free all  pending skbs */
	while ((skb = skb_dequeue(&nic->rx_skb_q)) != NULL)
		dev_kfree_skb_any(skb);

	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];
	sipa_skb_sender_remove_nic(sender, nic);
}
EXPORT_SYMBOL(sipa_nic_close);

void sipa_nic_notify_evt(struct sipa_nic *nic, enum sipa_evt_type evt)
{
	if (nic->cb)
		nic->cb(nic->cb_priv, evt, 0);

}
EXPORT_SYMBOL(sipa_nic_notify_evt);

void sipa_nic_try_notify_recv(struct sipa_nic *nic)
{
	int need_notify = 0;

	if (atomic_read(&nic->status) == NIC_CLOSE)
		return;

	if (nic->need_notify) {
		nic->need_notify = 0;
		need_notify = 1;
	}

	if (need_notify && nic->cb)
		nic->cb(nic->cb_priv, SIPA_RECEIVE, 0);
}
EXPORT_SYMBOL(sipa_nic_try_notify_recv);

void sipa_nic_push_skb(struct sipa_nic *nic, struct sk_buff *skb)
{
	skb_queue_tail(&nic->rx_skb_q, skb);
	if (nic->rx_skb_q.qlen == 1)
		nic->need_notify = 1;
}
EXPORT_SYMBOL(sipa_nic_push_skb);

int sipa_nic_tx(enum sipa_nic_id nic_id, enum sipa_term_type dst,
		int netid, struct sk_buff *skb)
{
	int ret;
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];
	if (!sender)
		return -ENODEV;

	ret = sipa_nic_rm_res_request(ctrl->nic[nic_id]);
	if (ret)
		return ret;

	ret = sipa_skb_sender_send_data(sender, skb, dst, netid);
	if (ret == -EAGAIN)
		ctrl->nic[nic_id]->flow_ctrl_status = true;

	sipa_nic_rm_res_release(ctrl->nic[nic_id]);

	return ret;
}
EXPORT_SYMBOL(sipa_nic_tx);

int sipa_nic_rx(enum sipa_nic_id nic_id, struct sk_buff **out_skb)
{
	struct sk_buff *skb;
	struct sipa_nic *nic;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	if (!ctrl->nic[nic_id] ||
	    atomic_read(&ctrl->nic[nic_id]->status) == NIC_CLOSE)
		return -ENODEV;

	nic = ctrl->nic[nic_id];
	skb = skb_dequeue(&nic->rx_skb_q);

	*out_skb = skb;

	return (skb) ? 0 : -ENODATA;
}
EXPORT_SYMBOL(sipa_nic_rx);

int sipa_nic_rx_has_data(enum sipa_nic_id nic_id)
{
	struct sipa_nic *nic;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	if (!ctrl->nic[nic_id] ||
	    atomic_read(&ctrl->nic[nic_id]->status) == NIC_CLOSE)
		return 0;

	nic = ctrl->nic[nic_id];

	return (!!nic->rx_skb_q.qlen);
}
EXPORT_SYMBOL(sipa_nic_rx_has_data);

int sipa_nic_trigger_flow_ctrl_work(enum sipa_nic_id nic_id, int err)
{
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];
	if (!sender)
		return -ENODEV;

	switch (err) {
	case -EAGAIN:
		sender->free_notify_net = true;
		schedule_work(&ctrl->flow_ctrl_work);
		break;
	default:
		dev_warn(ctrl->ctx->pdev,
			 "don't have this flow ctrl err type\n");
		break;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_nic_trigger_flow_ctrl_work);
