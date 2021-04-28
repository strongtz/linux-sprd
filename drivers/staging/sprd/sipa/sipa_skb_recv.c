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
#include <uapi/linux/sched/types.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of_device.h>
#include <linux/sipa.h>
#include "sipa_priv.h"
#include "sipa_hal.h"

#define SIPA_RECV_BUF_LEN     1600
#define SIPA_RECV_RSVD_LEN     64


int put_recv_array_node(struct sipa_skb_array *p,
			struct sk_buff *skb, dma_addr_t dma_addr)
{
	u32 pos;

	if ((p->wp - p->rp) < p->depth) {
		pos = p->wp & (p->depth -1);
		p->array[pos].skb = skb;
		p->array[pos].dma_addr = dma_addr;
		p->wp++;
		return 0;
	} else {
		return -1;
	}
}

int get_recv_array_node(struct sipa_skb_array *p,
			struct sk_buff **skb, dma_addr_t *dma_addr)
{
	u32 pos;

	if (p->rp != p->wp) {
		pos = p->rp & (p->depth -1);
		*skb = p->array[pos].skb;
		*dma_addr = p->array[pos].dma_addr;
		/*
		* Ensure that we remove the item from the fifo before
		* we update the fifo rp.
		*/
		smp_wmb();
		p->rp++;
		return 0;
	} else {
		return  -1;
	}
}

struct sk_buff *alloc_recv_skb(u32 req_len, u8 rsvd)
{
	struct sk_buff *skb;
	u32 hr;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	skb = __dev_alloc_skb(req_len + rsvd, GFP_KERNEL | GFP_NOWAIT);
	if (!skb) {
		dev_err(ctrl->ctx->pdev, "failed to alloc skb!\n");
		return NULL;
	}

	/* save skb ptr to skb->data */
	hr = skb_headroom(skb);
	if (hr < rsvd)
		skb_reserve(skb, rsvd - hr);

	return skb;
}

static void sipa_prepare_free_node_init(struct sipa_skb_receiver *receiver,
					u32 cnt)
{
	struct sk_buff *skb;
	u32 fail_cnt = 0;
	int i;
	u32 success_cnt = 0;
	struct sipa_hal_fifo_item item;
	dma_addr_t dma_addr;

	for (i = 0; i < cnt; i++) {
		skb = alloc_recv_skb(SIPA_RECV_BUF_LEN, receiver->rsvd);
		if (skb) {
			skb_put(skb, SIPA_RECV_BUF_LEN);
			dma_addr = dma_map_single(receiver->ctx->pdev,
						  skb->head,
						  SIPA_RECV_BUF_LEN +
						  skb_headroom(skb),
						  DMA_FROM_DEVICE);
			if (dma_mapping_error(receiver->ctx->pdev,
					      dma_addr)) {
				dev_err(receiver->ctx->pdev,
					"prepare free node dma map err\n");
				fail_cnt++;
				break;
			}

			put_recv_array_node(&receiver->recv_array,
					    skb, dma_addr);

			item.addr = dma_addr;
			item.len = skb->len;
			item.offset = skb_headroom(skb);
			item.dst = receiver->ep->recv_fifo.dst_id;
			item.src = receiver->ep->recv_fifo.src_id;
			item.intr = 0;
			item.netid = 0;
			item.err_code = 0;
			sipa_hal_cache_rx_fifo_item(receiver->ctx->hdl,
						    receiver->ep->recv_fifo.idx,
						    &item);
			success_cnt++;
		} else {
			fail_cnt++;
			break;
		}
	}

	if (fail_cnt)
		dev_err(receiver->ctx->pdev,
			"ep->id = %d fail_cnt = %d success_cnt = %d\n",
			receiver->ep->id, fail_cnt, success_cnt);
}

void fill_free_fifo(struct sipa_skb_receiver *receiver, u32 cnt)
{
	struct sk_buff *skb;
	u32 fail_cnt = 0;
	int i;
	u32 success_cnt = 0, depth;
	struct sipa_hal_fifo_item item;
	dma_addr_t dma_addr;

	depth = receiver->ep->recv_fifo.rx_fifo.fifo_depth;
	if (cnt > (depth - depth / 4)) {
		dev_warn(receiver->ctx->pdev,
			 "ep id = %d free node is not enough,need fill %d\n",
			 receiver->ep->id, cnt);
		receiver->rx_danger_cnt++;
	}

	for (i = 0; i < cnt; i++) {
		skb = alloc_recv_skb(SIPA_RECV_BUF_LEN, receiver->rsvd);
		if (skb) {
			skb_put(skb, SIPA_RECV_BUF_LEN);
			dma_addr = dma_map_single(receiver->ctx->pdev,
						  skb->head,
						  SIPA_RECV_BUF_LEN +
						  skb_headroom(skb),
						  DMA_FROM_DEVICE);

			put_recv_array_node(&receiver->recv_array,
					    skb, dma_addr);

			item.addr = dma_addr;
			item.len = skb->len;
			item.offset = skb_headroom(skb);
			item.dst = receiver->ep->recv_fifo.dst_id;
			item.src = receiver->ep->recv_fifo.src_id;
			item.intr = 0;
			item.netid = 0;
			item.err_code = 0;
			sipa_hal_cache_rx_fifo_item(receiver->ctx->hdl,
						    receiver->ep->recv_fifo.idx,
						    &item);
			success_cnt++;
		} else {
			fail_cnt++;
			break;
		}
	}
	if (success_cnt) {
		sipa_hal_put_rx_fifo_items(receiver->ctx->hdl,
					   receiver->ep->recv_fifo.idx);
		if (atomic_read(&receiver->need_fill_cnt) > 0)
			atomic_sub(success_cnt,
				   &receiver->need_fill_cnt);
	}
	if (fail_cnt)
		dev_err(receiver->ctx->pdev,
			"fill free fifo fail_cnt = %d\n", fail_cnt);
}

void sipa_fill_free_node(struct sipa_skb_receiver *receiver, u32 cnt)
{
	sipa_hal_put_rx_fifo_items(receiver->ctx->hdl,
				   receiver->ep->recv_fifo.idx);
	if (atomic_read(&receiver->need_fill_cnt) > 0)
		atomic_sub(cnt, &receiver->need_fill_cnt);
}
EXPORT_SYMBOL(sipa_fill_free_node);

void sipa_receiver_notify_cb(void *priv, enum sipa_hal_evt_type evt,
			     unsigned long data)
{
	struct sipa_skb_receiver *receiver = (struct sipa_skb_receiver *)priv;

	if (evt & SIPA_RECV_EVT)
		wake_up(&receiver->recv_waitq);

	if (evt & SIPA_RECV_WARN_EVT) {
		dev_err(receiver->ctx->pdev,
			"sipa maybe poor resources evt = 0x%x\n", evt);
		receiver->tx_danger_cnt++;
		wake_up(&receiver->recv_waitq);
	}
}

static void trigger_nics_recv(struct sipa_skb_receiver *receiver)
{
	int i;

	for (i = 0; i < receiver->nic_cnt; i++)
		sipa_nic_try_notify_recv(receiver->nic_array[i]);
}

static int dispath_to_nic(struct sipa_skb_receiver *receiver,
			  struct sipa_hal_fifo_item *item,
			  struct sk_buff *skb)
{
	u32 i;
	struct sipa_nic *nic;
	struct sipa_nic *dst_nic = NULL;

	for (i = 0; i < receiver->nic_cnt; i++) {
		nic = receiver->nic_array[i];
		if (atomic_read(&nic->status) != NIC_OPEN)
			continue;
		if ((nic->src_mask & BIT(item->src)) &&
		    (nic->netid == -1 || nic->netid == item->netid)) {
			dst_nic = nic;
			break;
		}
	}

	if (dst_nic) {
		sipa_nic_push_skb(dst_nic, skb);
	} else {
		dev_err(receiver->ctx->pdev,
			"dispath to nic src:0x%x, netid:%d no nic matched\n",
			item->src, item->netid);
		dev_kfree_skb_any(skb);
	}

	return 0;
}

static int do_recv(struct sipa_skb_receiver *receiver)
{
	int i, ret;
	u32 num = 0, depth = 0;
	dma_addr_t addr;
	struct sk_buff *recv_skb = NULL;
	struct sipa_hal_fifo_item item;
	enum sipa_cmn_fifo_index id = receiver->ep->recv_fifo.idx;

	depth = receiver->ep->recv_fifo.tx_fifo.fifo_depth;
	num = sipa_hal_get_tx_fifo_items(receiver->ctx->hdl,
					 receiver->ep->recv_fifo.idx);

	if (num > (depth - depth / 4)) {
		dev_warn(receiver->ctx->pdev,
			 "ep id %d tx fifo not read in time num = %d\n",
			 receiver->ep->id, num);
		receiver->tx_danger_cnt++;
	}

	for (i = 0; i < num; i++) {
		sipa_hal_recv_conversion_node_to_item(receiver->ctx->hdl, id,
						      &item, i);

		ret = get_recv_array_node(&receiver->recv_array,
					  &recv_skb, &addr);
		if (!item.addr) {
			dev_err(receiver->ctx->pdev,
				"phy addr is null = %llx\n", item.addr);
			continue;
		}
		if (ret) {
			dev_err(receiver->ctx->pdev,
				"recv addr:0x%llx, butrecv_array is empty\n",
				item.addr);
			continue;
		} else if (addr != item.addr) {
			dev_err(receiver->ctx->pdev,
				"recv addr:0x%llx, but recv_array addr:0x%llx not equal\n",
				item.addr, addr);
			continue;
		}

		dma_unmap_single(receiver->ctx->pdev,
				 addr,
				 SIPA_RECV_BUF_LEN + skb_headroom(recv_skb),
				 DMA_FROM_DEVICE);

		skb_trim(recv_skb, item.len);

		skb_reset_network_header(recv_skb);

		dispath_to_nic(receiver, &item, recv_skb);
	}

	sipa_hal_set_tx_fifo_rptr(receiver->ctx->hdl, id, num);

	return num;
}

static int fill_recv_thread(void *data)
{
	int ret;
	struct sipa_skb_receiver *receiver = (struct sipa_skb_receiver *)data;
	struct sched_param param = {.sched_priority = 92};

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(receiver->fill_recv_waitq,
				(atomic_read(&receiver->need_fill_cnt) > 0));
		if (!ret)
			fill_free_fifo(receiver,
				       atomic_read(&receiver->need_fill_cnt));
	}

	return 0;
}

static int recv_thread(void *data)
{
	struct sipa_skb_receiver *receiver = (struct sipa_skb_receiver *)data;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		u32 recv_cnt = 0;

		wait_event_interruptible(receiver->recv_waitq,
					 !sipa_hal_is_tx_fifo_empty(receiver->ctx->hdl,
								    receiver->ep->recv_fifo.idx));

		recv_cnt = do_recv(receiver);
		atomic_add(recv_cnt, &receiver->need_fill_cnt);
		if (atomic_read(&receiver->need_fill_cnt) > 0x30)
			wake_up(&receiver->fill_recv_waitq);

		trigger_nics_recv(receiver);
	}

	return 0;
}

int sipa_receiver_prepare_suspend(struct sipa_skb_receiver *receiver)
{
	if (!sipa_hal_is_tx_fifo_empty(receiver->ctx->hdl,
				       receiver->ep->recv_fifo.idx)) {
		pr_err("sipa recv fifo %d tx fifo is not empty\n",
		       receiver->ep->recv_fifo.idx);
		wake_up(&receiver->recv_waitq);
		return -EAGAIN;
	}

	if (atomic_read(&receiver->need_fill_cnt)) {
		pr_err("sipa recv fifo %d need_fill_cnt = %d\n",
		       receiver->ep->recv_fifo.idx,
		       atomic_read(&receiver->need_fill_cnt));
		wake_up(&receiver->fill_recv_waitq);
		return -EAGAIN;
	}

	return sipa_hal_cmn_fifo_set_receive(receiver->ctx->hdl,
					     receiver->ep->recv_fifo.idx,
					     true);
}
EXPORT_SYMBOL(sipa_receiver_prepare_suspend);

int sipa_receiver_prepare_resume(struct sipa_skb_receiver *receiver)
{
	if (unlikely(receiver->init_flag)) {
		dev_info(receiver->ctx->pdev, "receiver %d wake up thread\n",
			 receiver->ep->id);
		wake_up_process(receiver->thread);
		wake_up_process(receiver->fill_thread);
		receiver->init_flag = false;
	}

	return sipa_hal_cmn_fifo_set_receive(receiver->ctx->hdl,
					     receiver->ep->recv_fifo.idx,
					     false);
}
EXPORT_SYMBOL(sipa_receiver_prepare_resume);

void sipa_receiver_init(struct sipa_skb_receiver *receiver, u32 rsvd)
{
	u32 depth;
	struct sipa_comm_fifo_params attr;

	/* timeout = 1 / ipa_sys_clk * 1024 * value */
	attr.tx_intr_delay_us = 0x64;
	attr.tx_intr_threshold = 0x30;
	attr.flowctrl_in_tx_full = true;
	attr.flow_ctrl_cfg = flow_ctrl_rx_empty;
	attr.flow_ctrl_irq_mode = enter_exit_flow_ctrl;
	attr.rx_enter_flowctrl_watermark =
		receiver->ep->recv_fifo.rx_fifo.fifo_depth / 4;
	attr.rx_leave_flowctrl_watermark =
		receiver->ep->recv_fifo.rx_fifo.fifo_depth / 2;
	attr.tx_enter_flowctrl_watermark = 0;
	attr.tx_leave_flowctrl_watermark = 0;

	dev_info(receiver->ctx->pdev,
		 "ep_id = %d fifo_id = %d rx_fifo depth = 0x%x\n",
		 receiver->ep->id,
		 receiver->ep->recv_fifo.idx,
		 receiver->ep->recv_fifo.rx_fifo.fifo_depth);
	dev_info(receiver->ctx->pdev,
		 "recv status is %d\n", receiver->ep->recv_fifo.is_receiver);
	sipa_open_common_fifo(receiver->ctx->hdl,
			      receiver->ep->recv_fifo.idx,
			      &attr,
			      NULL,
			      true,
			      sipa_receiver_notify_cb, receiver);

	/* reserve space for dma flushing cache issue */
	receiver->rsvd = rsvd;
	receiver->init_flag = true;
	depth = receiver->ep->recv_fifo.rx_fifo.fifo_depth;

	sipa_prepare_free_node_init(receiver, depth);
}

void sipa_receiver_add_nic(struct sipa_skb_receiver *receiver,
			   struct sipa_nic *nic)
{
	int i;
	unsigned long flags;

	for (i = 0; i < receiver->nic_cnt; i++)
		if (receiver->nic_array[i] == nic)
			return;
	spin_lock_irqsave(&receiver->lock, flags);
	if (receiver->nic_cnt < SIPA_NIC_MAX)
		receiver->nic_array[receiver->nic_cnt++] = nic;
	spin_unlock_irqrestore(&receiver->lock, flags);
}
EXPORT_SYMBOL(sipa_receiver_add_nic);

int create_recv_array(struct sipa_skb_array *p, u32 depth)
{
	p->array = kzalloc(sizeof(struct sipa_skb_dma_addr_pair) * depth,
			   GFP_KERNEL);
	if (!p->array)
		return -ENOMEM;
	p->rp = 0;
	p->wp = 0;
	p->depth = depth;

	return 0;
}

void destroy_recv_array(struct sipa_skb_array *p)
{
	if (p->array)
		kfree(p->array);

	p->array = NULL;
	p->rp = 0;
	p->wp = 0;
	p->depth = 0;
}

int create_sipa_skb_receiver(struct sipa_context *ipa,
			     struct sipa_endpoint *ep,
			     struct sipa_skb_receiver **receiver_pp)
{
	int ret;
	struct sipa_skb_receiver *receiver = NULL;

	dev_info(ipa->pdev, "ep->id = %d start\n", ep->id);
	receiver = kzalloc(sizeof(struct sipa_skb_receiver), GFP_KERNEL);
	if (!receiver)
		return -ENOMEM;

	receiver->ctx = ipa;
	receiver->ep = ep;
	receiver->rsvd = SIPA_RECV_RSVD_LEN;

	atomic_set(&receiver->need_fill_cnt, 0);

	ret = create_recv_array(&receiver->recv_array,
				receiver->ep->recv_fifo.rx_fifo.fifo_depth);
	if (ret) {
		dev_err(ipa->pdev,
			"create_sipa_sipa_receiver: recv_array kzalloc err.\n");
		kfree(receiver);
		return -ENOMEM;
	}

	spin_lock_init(&receiver->lock);
	init_waitqueue_head(&receiver->recv_waitq);
	init_waitqueue_head(&receiver->fill_recv_waitq);

	sipa_receiver_init(receiver, SIPA_RECV_RSVD_LEN);
	/* create sender thread */
	receiver->thread = kthread_create(recv_thread, receiver,
					  "sipa-recv-%d", ep->id);
	if (IS_ERR(receiver->thread)) {
		dev_err(ipa->pdev,
			"Failed to create kthread: ipa-recv-%d\n",
			ep->id);
		ret = PTR_ERR(receiver->thread);
		kfree(receiver->recv_array.array);
		kfree(receiver);
		return ret;
	}

	receiver->fill_thread = kthread_create(fill_recv_thread, receiver,
					       "sipa-fill-%d", ep->id);
	if (IS_ERR(receiver->fill_thread)) {
		kthread_stop(receiver->thread);
		dev_err(ipa->pdev, "Failed to create kthread: ipa-fill-%d\n",
			ep->id);
		ret = PTR_ERR(receiver->fill_thread);
		kfree(receiver->recv_array.array);
		kfree(receiver);
		return ret;
	}

	*receiver_pp = receiver;
	return 0;
}
EXPORT_SYMBOL(create_sipa_skb_receiver);

void destroy_sipa_skb_receiver(struct sipa_skb_receiver *receiver)
{
	if (receiver->recv_array.array)
		destroy_recv_array(&receiver->recv_array);

	kfree(receiver);
}
EXPORT_SYMBOL(destroy_sipa_skb_receiver);
