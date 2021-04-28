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

#include "../sipa_hal.h"
#include "../sipa_priv.h"
#include "sipa_periph_ep_io.h"


void sipa_periph_recv_notify(void *priv, enum sipa_hal_evt_type evt,
							 unsigned long data)
{
	struct sipa_periph_receiver *receiver = (struct sipa_periph_receiver *)priv;

	pr_info("%s:evt:0x%x\n", __func__, evt);
	if (evt & SIPA_RECV_EVT)
		queue_work(receiver->recv_wq, &receiver->recv_work);
}

static int sipa_periph_putback(struct sipa_periph_receiver *receiver,
							   struct sipa_hal_fifo_item *item)
{
	sipa_hal_put_rx_fifo_item(receiver->ctx->hdl,
							  receiver->ep->recv_fifo.idx,
							  item);
	return 0;
}
static int do_periph_recv(struct sipa_periph_receiver *receiver)
{
	int ret, i;
	struct sk_buff *skb_ori, *skb;
	struct sipa_hal_fifo_item item;

	ret = sipa_hal_get_tx_fifo_item(receiver->ctx->hdl,
									receiver->ep->recv_fifo.idx,
									&item);
	if (ret)
		return ret;

	ret = -1;
	for (i = 0; i < SIPA_EP_MAX; i++) {
		if (receiver->senders[i]) {
			ret = sipa_periph_search_item(receiver->senders[i],
										  &item,
										  &skb_ori);
			if (!ret)
				break;
		}

	}
	if (ret) {
		pr_err("do_recv recv addr:0x%llx, but not found in sending list\n",
			   item.addr);
		return ret;
	}
	skb = skb_clone(skb_ori, GFP_KERNEL);
	if (!skb) {
		pr_err("skb_clone fail!\n");
		return -ENOMEM;
	}

	/* do offset */
	if (item.offset > skb_headroom(skb))
		skb_pull(skb, 14);
	else if (item.offset < skb_headroom(skb))
		skb_push(skb, 14);

	/* put back item to ipa */
	sipa_periph_putback(receiver, &item);


	receiver->dispatch_func(receiver, &item, skb);

	return 0;
}

static void process_recv(struct work_struct *work)
{
	struct sipa_periph_receiver *receiver = container_of(work,
											struct sipa_periph_receiver, recv_work);
	u32 recv_cnt = 0;

	while (!do_periph_recv(receiver))
		recv_cnt++;

	pr_info("%s received %d pkts\n", __func__, recv_cnt);

}


static void sipa_periph_receiver_init(struct sipa_periph_receiver *receiver)
{
	struct sipa_comm_fifo_params attr;

	attr.tx_intr_delay_us = 1;
	attr.tx_intr_threshold = 16;
	attr.flowctrl_in_tx_full = false;
	attr.flow_ctrl_cfg = flow_ctrl_tx_full;
	attr.flow_ctrl_irq_mode = enter_exit_flow_ctrl;
	attr.rx_enter_flowctrl_watermark = receiver->ep
									   ->recv_fifo.rx_fifo.fifo_depth / 4;
	attr.rx_leave_flowctrl_watermark = receiver->ep
									   ->recv_fifo.rx_fifo.fifo_depth / 2;
	attr.tx_enter_flowctrl_watermark = 0;
	attr.tx_leave_flowctrl_watermark = 0;

	pr_info("ep_id = %d fifo_id = %d rx_fifo depth = 0x%x\n",
			receiver->ep->id,
			receiver->ep->recv_fifo.idx,
			receiver->ep->recv_fifo.rx_fifo.fifo_depth);
	pr_info("recv status is %d\n",
			receiver->ep->recv_fifo.is_receiver);
	sipa_open_common_fifo(receiver->ctx->hdl,
						  receiver->ep->recv_fifo.idx,
						  &attr,
						  true,
						  sipa_periph_recv_notify, receiver);

}

void sipa_receiver_add_sender(struct sipa_periph_receiver *receiver,
							  struct sipa_periph_sender *sender)
{
	receiver->senders[sender->ep->id] = sender;
}
EXPORT_SYMBOL(sipa_receiver_add_sender);

int create_sipa_periph_receiver(struct sipa_context *ipa,
								struct sipa_endpoint *ep,
								sipa_periph_dispatch_func dispatch,
								struct sipa_periph_receiver **receiver_pp)
{
	struct sipa_periph_receiver *receiver = NULL;

	pr_info("%s ep->id = %d start\n", __func__, ep->id);
	receiver = kzalloc(sizeof(struct sipa_periph_receiver), GFP_KERNEL);
	if (!receiver) {
		pr_err("%s: kzalloc err.\n", __func__);
		return -ENOMEM;
	}

	receiver->ctx = ipa;
	receiver->ep = ep;

	spin_lock_init(&receiver->lock);
	init_waitqueue_head(&receiver->recv_waitq);
	INIT_WORK(&receiver->recv_work, process_recv);
	receiver->recv_wq = create_singlethread_workqueue("ipa_rm_wq");
	if (!receiver->recv_wq) {
		pr_err("%s: create work queue err.\n", __func__);
		kfree(receiver);
		return -ENOMEM;
	}
	receiver->dispatch_func = dispatch;

	sipa_periph_receiver_init(receiver);


	*receiver_pp = receiver;
	return 0;

}
EXPORT_SYMBOL(create_sipa_periph_receiver);

void destroy_sipa_periph_receiver(struct sipa_periph_receiver *receiver)
{
	kfree(receiver);
}
EXPORT_SYMBOL(destroy_sipa_periph_receiver);
