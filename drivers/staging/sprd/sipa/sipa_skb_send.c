#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/if_arp.h>
#include <asm/byteorder.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of_device.h>
#include <linux/sipa.h>
#include <uapi/linux/sched/types.h>

#include "sipa_priv.h"
#include "sipa_hal.h"

#define SIPA_RECEIVER_BUF_LEN     1600

static void sipa_inform_evt_to_nics(struct sipa_skb_sender *sender,
				    enum sipa_evt_type evt)
{
	unsigned long flags;
	struct sipa_nic *nic;

	spin_lock_irqsave(&sender->nic_lock, flags);
	list_for_each_entry(nic, &sender->nic_list, list) {
		if (nic->flow_ctrl_status) {
			nic->flow_ctrl_status = false;
			sipa_nic_notify_evt(nic, evt);
		}
	}
	spin_unlock_irqrestore(&sender->nic_lock, flags);
}

void sipa_sender_notify_cb(void *priv, enum sipa_hal_evt_type evt,
			   unsigned long data)
{
	struct sipa_skb_sender *sender = (struct sipa_skb_sender *)priv;

	if (evt & SIPA_RECV_EVT)
		wake_up(&sender->free_waitq);

	if (evt & SIPA_HAL_TXFIFO_OVERFLOW)
		dev_err(sender->ctx->pdev,
			"sipa overflow on ep:%d\n", sender->ep->id);

	if (evt & SIPA_HAL_ENTER_FLOW_CTRL)
		sender->enter_flow_ctrl_cnt++;

	if (evt & SIPA_HAL_EXIT_FLOW_CTRL)
		sender->exit_flow_ctrl_cnt++;
}

static void sipa_free_sent_items(struct sipa_skb_sender *sender)
{
	bool status = false;
	unsigned long flags;
	u32 i, num, success_cnt = 0;
	struct sipa_skb_dma_addr_node *iter, *_iter;
	struct sipa_hal_fifo_item item;

	num = sipa_hal_get_tx_fifo_items(sender->ctx->hdl,
					 sender->ep->send_fifo.idx);

	for (i = 0; i < num; i++) {
		sipa_hal_recv_conversion_node_to_item(sender->ctx->hdl,
						      sender->ep->send_fifo.idx,
						      &item, i);
		if (item.err_code > 1)
			dev_err(sender->ctx->pdev,
				"have node transfer err = %d\n", item.err_code);

		spin_lock_irqsave(&sender->send_lock, flags);
		if (list_empty(&sender->sending_list)) {
			pr_err("fifo id %d: send list is empty\n",
			       sender->ep->send_fifo.idx);
			spin_unlock_irqrestore(&sender->send_lock, flags);
			return;
		}

		list_for_each_entry_safe(iter, _iter,
					 &sender->sending_list, list) {
			if (iter->dma_addr == item.addr) {
				list_del(&iter->list);
				list_add_tail(&iter->list,
					      &sender->pair_free_list);
				status = true;
				break;
			}
		}
		spin_unlock_irqrestore(&sender->send_lock, flags);
		if (status) {
			dma_unmap_single(sender->ctx->pdev,
					 iter->dma_addr,
					 iter->skb->len +
					 skb_headroom(iter->skb),
					 DMA_TO_DEVICE);

			dev_kfree_skb_any(iter->skb);
			success_cnt++;
			status = false;
		}
	}
	sipa_hal_set_tx_fifo_rptr(sender->ctx->hdl,
				  sender->ep->send_fifo.idx, num);
	atomic_add(success_cnt, &sender->left_cnt);
	if (sender->free_notify_net &&
	    atomic_read(&sender->left_cnt) >
	    sender->ep->send_fifo.rx_fifo.fifo_depth / 4) {
		sender->free_notify_net = false;
		sipa_inform_evt_to_nics(sender, SIPA_LEAVE_FLOWCTRL);
	}
	if (num != success_cnt)
		dev_err(sender->ctx->pdev,
			"recv num = %d release num = %d\n",
			num, success_cnt);
}

static int sipa_send_thread(void *data)
{
	int ret;
	struct sipa_skb_sender *sender = data;
	struct sched_param param = {.sched_priority = 90};

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(sender->send_waitq,
			!sipa_hal_check_rx_priv_fifo_is_empty(sender->ctx->hdl,
					sender->ep->send_fifo.idx));
		if (!ret)
			sipa_hal_put_rx_fifo_items(sender->ctx->hdl,
						   sender->ep->send_fifo.idx);

		if (sender->free_notify_net)
			wake_up(&sender->free_waitq);
	}

	return 0;
}

static int sipa_free_thread(void *data)
{
	int ret;
	struct sipa_skb_sender *sender = (struct sipa_skb_sender *)data;
	struct sched_param param = {.sched_priority = 90};

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(sender->free_waitq,
				(!sipa_hal_is_tx_fifo_empty(sender->ctx->hdl,
					sender->ep->send_fifo.idx) ||
					sender->free_notify_net));
		if (!ret)
			sipa_free_sent_items(sender);
	}

	return 0;
}

static int sipa_skb_sender_init(struct sipa_skb_sender *sender)
{
	struct sipa_comm_fifo_params attr;

	attr.tx_intr_delay_us = 500;
	attr.tx_intr_threshold = 32;
	attr.flow_ctrl_cfg = flow_ctrl_tx_full;
	attr.flowctrl_in_tx_full = true;
	attr.flow_ctrl_irq_mode = enter_exit_flow_ctrl;
	attr.rx_enter_flowctrl_watermark = 0;
	attr.rx_leave_flowctrl_watermark = 0;
	attr.tx_enter_flowctrl_watermark = 0;
	attr.tx_leave_flowctrl_watermark = 0;

	sipa_open_common_fifo(sender->ctx->hdl,
			      sender->ep->send_fifo.idx,
			      &attr,
			      NULL,
			      true,
			      sipa_sender_notify_cb, sender);
	sender->init_flag = true;

	return 0;
}

int sipa_sender_prepare_suspend(struct sipa_skb_sender *sender)
{
	if (!list_empty(&sender->sending_list)) {
		pr_err("pkt_type = %d sending list have unsend node\n",
		       sender->type);
		wake_up(&sender->free_waitq);
		return -EAGAIN;
	}

	if (!sipa_hal_check_rx_priv_fifo_is_empty(sender->ctx->hdl,
						  sender->ep->send_fifo.idx)) {
		pr_err("pkt_type = %d rx priv fifo is not empty\n",
		       sender->type);
		wake_up(&sender->send_waitq);
		return -EAGAIN;
	}

	if (!sipa_hal_check_send_cmn_fifo_com(sender->ctx->hdl,
					      sender->ep->send_fifo.idx)) {
		pr_err("pkt_type = %d sender have something to handle\n",
		       sender->type);
		return -EAGAIN;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_sender_prepare_suspend);

int sipa_sender_prepare_resume(struct sipa_skb_sender *sender)
{
	if (unlikely(sender->init_flag)) {
		wake_up_process(sender->send_thread);
		wake_up_process(sender->free_thread);
		sender->init_flag = false;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_sender_prepare_resume);

int create_sipa_skb_sender(struct sipa_context *ipa,
			   struct sipa_endpoint *ep,
			   enum sipa_xfer_pkt_type type,
			   struct sipa_skb_sender **sender_pp)
{
	int i, ret;
	struct sipa_skb_sender *sender = NULL;

	dev_info(ipa->pdev, "%s ep->id = %d start\n", __func__, ep->id);
	sender = kzalloc(sizeof(*sender), GFP_KERNEL);
	if (!sender)
		return -ENOMEM;

	sender->pair_cache = kcalloc(ep->send_fifo.rx_fifo.fifo_depth,
				     sizeof(struct sipa_skb_dma_addr_node),
				     GFP_KERNEL);
	if (!sender->pair_cache) {
		kfree(sender);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&sender->nic_list);
	INIT_LIST_HEAD(&sender->sending_list);
	INIT_LIST_HEAD(&sender->pair_free_list);
	spin_lock_init(&sender->nic_lock);
	spin_lock_init(&sender->send_lock);

	for (i = 0; i < ep->send_fifo.rx_fifo.fifo_depth; i++)
		list_add_tail(&((sender->pair_cache + i)->list),
			      &sender->pair_free_list);

	sender->ctx = ipa;
	sender->ep = ep;
	sender->type = type;

	atomic_set(&sender->left_cnt, ep->send_fifo.rx_fifo.fifo_depth);

	/* reigster sender ipa event callback */
	sipa_skb_sender_init(sender);

	init_waitqueue_head(&sender->send_waitq);
	init_waitqueue_head(&sender->free_waitq);

	/* create sender thread */
	sender->send_thread = kthread_create(sipa_send_thread, sender,
					     "sipa-send-%d", ep->id);
	if (IS_ERR(sender->send_thread)) {
		dev_err(ipa->pdev, "Failed to create kthread: ipa-send-%d\n",
			ep->id);
		ret = PTR_ERR(sender->send_thread);
		kfree(sender->pair_cache);
		kfree(sender);
		return ret;
	}

	sender->free_thread = kthread_create(sipa_free_thread, sender,
					     "sipa-free-%d", ep->id);
	if (IS_ERR(sender->free_thread)) {
		kthread_stop(sender->send_thread);
		dev_err(ipa->pdev, "Failed to create kthread: ipa-free-%d\n",
			ep->id);
		ret = PTR_ERR(sender->free_thread);
		kfree(sender->pair_cache);
		kfree(sender);
		return ret;
	}

	*sender_pp = sender;
	return 0;
}
EXPORT_SYMBOL(create_sipa_skb_sender);

void destroy_sipa_skb_sender(struct sipa_skb_sender *sender)
{
	kfree(sender->pair_cache);
	kfree(sender);
}
EXPORT_SYMBOL(destroy_sipa_skb_sender);

void sipa_skb_sender_add_nic(struct sipa_skb_sender *sender,
			     struct sipa_nic *nic)
{
	unsigned long flags;

	spin_lock_irqsave(&sender->nic_lock, flags);
	list_add_tail(&nic->list, &sender->nic_list);
	spin_unlock_irqrestore(&sender->nic_lock, flags);
}
EXPORT_SYMBOL(sipa_skb_sender_add_nic);

void sipa_skb_sender_remove_nic(struct sipa_skb_sender *sender,
				struct sipa_nic *nic)
{
	unsigned long flags;

	spin_lock_irqsave(&sender->nic_lock, flags);
	list_del(&nic->list);
	spin_unlock_irqrestore(&sender->nic_lock, flags);
}
EXPORT_SYMBOL(sipa_skb_sender_remove_nic);

int sipa_skb_sender_send_data(struct sipa_skb_sender *sender,
			      struct sk_buff *skb,
			      enum sipa_term_type dst,
			      u8 netid)
{
	unsigned long flags;
	dma_addr_t dma_addr;
	struct sipa_skb_dma_addr_node *node;
	struct sipa_hal_fifo_item item;

	if (!atomic_read(&sender->left_cnt)) {
		sender->no_free_cnt++;
		return -EAGAIN;
	}
	atomic_dec(&sender->left_cnt);

	dma_addr = dma_map_single(sender->ctx->pdev,
				  skb->head,
				  skb->len + skb_headroom(skb),
				  DMA_TO_DEVICE);

	memset(&item, 0, sizeof(item));
	item.addr = dma_addr;
	item.len = skb->len;
	item.offset = skb_headroom(skb);
	item.netid = netid;
	item.dst = dst;
	item.src = sender->ep->send_fifo.src_id;

	spin_lock_irqsave(&sender->send_lock, flags);
	node = list_first_entry(&sender->pair_free_list,
				struct sipa_skb_dma_addr_node,
				list);
	list_del(&node->list);
	node->skb = skb;
	node->dma_addr = dma_addr;
	list_add_tail(&node->list, &sender->sending_list);
	sipa_hal_cache_rx_fifo_item(sender->ctx->hdl,
				    sender->ep->send_fifo.idx,
				    &item);
	spin_unlock_irqrestore(&sender->send_lock, flags);

	wake_up(&sender->send_waitq);

	return 0;
}
EXPORT_SYMBOL(sipa_skb_sender_send_data);

bool sipa_skb_sender_check_send_complete(struct sipa_skb_sender *sender)
{
	return sipa_hal_check_send_cmn_fifo_com(sender->ctx->hdl,
						sender->ep->send_fifo.idx);
}
EXPORT_SYMBOL(sipa_skb_sender_check_send_complete);
