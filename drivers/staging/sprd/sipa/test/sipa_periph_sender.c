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
#include "sipa_periph_ep_io.h"

#define SIPA_TX_INTR_EVT (SIPA_HAL_THRESHOLD | SIPA_HAL_INTR_BIT | SIPA_HAL_DELAY_TIMER)


static bool check_node_addr_same(dma_addr_t ori, dma_addr_t addr)
{
	if (ori == addr)
		return true;
	else if (ori == (addr + 14))
		return true;
	else if (ori == (addr - 14))
		return true;
	else
		return false;
}
static void free_sent_items(struct sipa_periph_sender *sender)
{
	unsigned long flags;

	struct sipa_skb_dma_addr_node *iter, *_iter;
	struct sipa_hal_fifo_item item;
	u32 fail_cnt = 0;
	int ret = 0;

	spin_lock_irqsave(&sender->lock, flags);

	while (!ret) {

		ret = sipa_hal_is_tx_fifo_empty(sender->ctx->hdl,
										sender->ep->send_fifo.idx);
		if (ret) {
			pr_info("id = %d tx fifo is empty\n",
					sender->ep->send_fifo.idx);
			break;
		}

		ret = sipa_hal_get_tx_fifo_item(sender->ctx->hdl,
										sender->ep->send_fifo.idx,
										&item);

		if (ret) {
			pr_info("get item failed index = %d\n",
					sender->ep->send_fifo.idx);
			break;
		} else {
			pr_info("id = %d tx fifo got item\n",
					sender->ep->send_fifo.idx);
		}

		sender->left_cnt++;

		if (item.err_code)
			fail_cnt++;


		list_for_each_entry_safe(iter, _iter, &sender->sending_list,
								 list) {
			if (check_node_addr_same(iter->dma_addr, item.addr)) {
				dev_kfree_skb_any(iter->skb);
				list_del(&iter->list);
				kmem_cache_free(sender->sending_pair_cache,
								iter);
				break;
			}
		}
	}

	spin_unlock_irqrestore(&sender->lock, flags);

	if (fail_cnt)
		pr_err("free_sent_items found send fail cnt:%d\n", fail_cnt);
}

void sipa_periph_sender_notify_cb(void *priv, enum sipa_hal_evt_type evt,
								  unsigned long data)
{
	struct sipa_periph_sender *sender = (struct sipa_periph_sender *)priv;

	pr_debug("sipa_periph_sender recv evt:0x%x\n", (u32)evt);

	if (evt & SIPA_HAL_TXFIFO_OVERFLOW) {
		pr_err("sipa overflow on ep:%d\n", sender->ep->id);
		BUG_ON(0);
	}

	if (evt & SIPA_HAL_EXIT_FLOW_CTRL) {
		//inform_evt_to_nics(sender, SIPA_LEAVE_FLOWCTRL);
	}

	if (evt & SIPA_HAL_ENTER_FLOW_CTRL) {
		//inform_evt_to_nics(sender, SIPA_ENTER_FLOWCTRL);
	}

	if (evt & SIPA_TX_INTR_EVT) {
		/* do clear free fifo operation */

	}
}

int sipa_periph_search_item(struct sipa_periph_sender *sender,
							struct sipa_hal_fifo_item *item,
							struct sk_buff **skb)
{
	unsigned long flags;
	struct sipa_skb_dma_addr_node *iter, *_iter;
	int ret = -1;

	spin_lock_irqsave(&sender->lock, flags);
	list_for_each_entry_safe(iter, _iter, &sender->sending_list,
							 list) {
		if (check_node_addr_same(iter->dma_addr, item->addr)) {
			*skb = iter->skb;

			ret = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&sender->lock, flags);

	return ret;
}
EXPORT_SYMBOL(sipa_periph_search_item);


int sipa_periph_send_data(struct sipa_periph_sender *sender,
						  struct sk_buff *skb,
						  enum sipa_term_type dst,
						  u8 netid)
{
	unsigned long flags;
	int can_send = 0;
	dma_addr_t dma_addr;
	struct sipa_skb_dma_addr_node *node;
	struct sipa_hal_fifo_item item;

	/* free sent items first */
	free_sent_items(sender);

	node = kmem_cache_zalloc(sender->sending_pair_cache, GFP_ATOMIC);
	if (!node)
		return -ENOMEM;

	spin_lock_irqsave(&sender->lock, flags);
	if (sender->left_cnt) {
		sender->left_cnt--;
		can_send = 1;
	}
	spin_unlock_irqrestore(&sender->lock, flags);

	if (!can_send) {
		kmem_cache_free(sender->sending_pair_cache, node);
		return -EAGAIN;
	}

	dma_addr = dma_map_single(sender->ctx->pdev,
							  skb->head,
							  skb->len + skb_headroom(skb),
							  DMA_TO_DEVICE);

	node->skb = skb;
	node->dma_addr = dma_addr;

	memset(&item, 0, sizeof(item));
	item.addr = dma_addr;
	item.len = skb->len;
	item.offset = skb_headroom(skb);
	item.netid = netid;
	item.dst = dst;
	item.src = sender->ep->send_fifo.src_id;

	pr_debug("sipa_periph_send_data :%d, item addr:0x%x, len:%d, offset:%d, src:%d, dst:%d\n",
				sender->ep->send_fifo.idx,
				((u32)(dma_addr & 0x00000000FFFFFFFF)),
				item.len,
				item.offset,
				item.src,
				item.dst);

	spin_lock_irqsave(&sender->lock, flags);

	list_add_tail(&node->list, &sender->sending_list);

	sipa_hal_put_rx_fifo_item(sender->ctx->hdl,
							  sender->ep->send_fifo.idx,
							  &item);
	spin_unlock_irqrestore(&sender->lock, flags);

	return 0;
}


int sipa_periph_sender_init(struct sipa_periph_sender *sender)
{
	struct sipa_comm_fifo_params attr;

	spin_lock_init(&sender->lock);
	INIT_LIST_HEAD(&sender->sending_list);

	sender->left_cnt = sender->ep->send_fifo.tx_fifo.fifo_depth;

	sender->sending_pair_cache = kmem_cache_create("SIPA_PERIPH_PAIR",
								 sizeof(struct sipa_skb_dma_addr_node), 0, 0, NULL);
	if (!sender->sending_pair_cache) {
		pr_err("sipa_skb_sender_init:sending_pair_cache create failed\n");
		return -ENOMEM;
	}

	attr.tx_intr_delay_us = 0;
	attr.tx_intr_threshold = sender->left_cnt / 4;
	attr.flow_ctrl_cfg = 0;
	attr.flowctrl_in_tx_full = true;
	attr.flow_ctrl_irq_mode = enter_exit_flow_ctrl;
	attr.rx_enter_flowctrl_watermark = 0;
	attr.rx_leave_flowctrl_watermark = 0;
	attr.tx_enter_flowctrl_watermark = 0;
	attr.tx_leave_flowctrl_watermark = 0;

	sipa_open_common_fifo(sender->ctx->hdl,
						  sender->ep->send_fifo.idx,
						  &attr,
						  true,
						  sipa_periph_sender_notify_cb, sender);

	return 0;
}

int create_sipa_periph_sender(struct sipa_context *ipa,
							  struct sipa_endpoint *ep,
							  enum sipa_xfer_pkt_type type,
							  struct sipa_periph_sender **sender_pp)
{
	struct sipa_periph_sender *sender = NULL;

	pr_info("%s ep->id = %d start\n", __func__, ep->id);
	sender = kzalloc(sizeof(*sender), GFP_KERNEL);
	if (!sender) {
		pr_err("create_sipa_skb_sender: kzalloc err.\n");
		return -ENOMEM;
	}

	sender->ctx = ipa;
	sender->ep = ep;
	sender->type = type;
	sender->left_cnt = ep->send_fifo.tx_fifo.fifo_depth;

	/* reigster sender ipa event callback */
	sipa_periph_sender_init(sender);

	*sender_pp = sender;
	return 0;
}
EXPORT_SYMBOL(create_sipa_periph_sender);


void destroy_sipa_periph_sender(struct sipa_periph_sender *sender)
{
	free_sent_items(sender);
	if (sender->sending_pair_cache)
		kmem_cache_destroy(sender->sending_pair_cache);
	kfree(sender);
}
EXPORT_SYMBOL(destroy_sipa_periph_sender);

