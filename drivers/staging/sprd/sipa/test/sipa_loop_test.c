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
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of_device.h>
#include <linux/sipa.h>
#include <uapi/linux/sched/types.h>

#include "../sipa_priv.h"

struct sipa_loop_test {
	enum sipa_nic_id nic_id;

	struct task_struct *send_thread;
	struct task_struct *recv_thread;

	wait_queue_head_t recv_wq;

	u32 chk_index;
	u32 send_index;
	int send_stop;
};

static u32 probe_cnt;
struct sipa_loop_test s_test_ctrl;
extern void ipa_test_init_callback(void);

static int loop_test_send_pkt(void)
{
	int ret;
	struct sk_buff *skb;
	int len;

	s_test_ctrl.send_index++;

	len = s_test_ctrl.send_index % 1400 + 1;
	skb = __dev_alloc_skb(len, GFP_KERNEL);
	if (!skb) {
		pr_err("failed to alloc skb!\n");
		return 0;
	} else {
		;
	}
	skb_put(skb, len);
	memset(skb->data, len & 0xff, len);
	pr_info("skb->len = %d skb->truesize = %d skb->data_len = %d\n",
			skb->len, skb->truesize, skb->data_len);

	ret = sipa_nic_tx(s_test_ctrl.nic_id, SIPA_TERM_VAP0, 0, skb);
	if (ret) {
		s_test_ctrl.send_stop = 1;
		pr_err("loop_test7 enter flow ctrl\n");
		dev_kfree_skb_any(skb);
		return -1;
	}

	return 0;
}

static int check_data(u8 *data, u32 len)
{
	u32 i, chk_len;

	s_test_ctrl.chk_index++;
	chk_len = s_test_ctrl.chk_index % 1400 + 1;
	if (len != chk_len) {
		pr_err("loop_test check_data fail idx:%d, chk_len:%d, len:%d\n",
			   s_test_ctrl.chk_index, chk_len, len);
		return -1;
	}

	for (i = 0; i < len; i++) {
		if (data[i] != 0xE7) {
			pr_err("loop_test check_data fail i = %d data = %d\n",
				   i, data[i]);
			return -2;
		}
	}

	return 0;
}

static int loop_test_recv_pkts(void)
{
	struct sk_buff *skb;
	int ret = 0;

	while (!ret) {
		ret = sipa_nic_rx(s_test_ctrl.nic_id, &skb);

		if (ret)
			continue;

		ret = check_data(skb->data + SIPA_DEF_OFFSET, skb->data_len);
		if (!ret)
			pr_info("data check success skb->data_len = %d\n", skb->data_len);
		dev_kfree_skb_any(skb);
	}
	return 0;
}

void loop_test_notify_cb(void *priv, enum sipa_evt_type evt,
						 unsigned long data)
{
	switch (evt) {
	case SIPA_RECEIVE:
			pr_info("%s SIPA_RECEIVE\n", __func__);
		wake_up(&s_test_ctrl.recv_wq);
		break;
	case SIPA_LEAVE_FLOWCTRL:
		pr_info("%s fifo leave flowctrl\n", __func__);
		s_test_ctrl.send_stop = 0;
		break;
	case SIPA_ENTER_FLOWCTRL:
		pr_info("%s fifo enter flowctrl\n", __func__);
		s_test_ctrl.send_stop = 0;
		break;
	default:
		break;
	}
}

static int send_thread(void *data)
{
	u32 cnt = 0;

	pr_info("[IPA] start send thread\n");
	msleep(30000);

	while (!kthread_should_stop()) {
		pr_info("%s cnt = %d\n", __func__, cnt++);
		if (!s_test_ctrl.send_stop) {
			loop_test_send_pkt();
			msleep(1000);
		} else {
			msleep(100);
		}
	}

	return 0;
}

static int recv_thread(void *data)
{
	while (!kthread_should_stop()) {
		pr_info("[sipa_loop_test]%s\n", __func__);
		wait_event_interruptible(s_test_ctrl.recv_wq,
								 sipa_nic_rx_has_data(s_test_ctrl.nic_id));
		loop_test_recv_pkts();
	}

	return 0;
}

int sipa_loop_test_start(void)
{
	int ret = SIPA_NIC_MAX;

	pr_info("%s probe_cnt = %d\n", __func__, probe_cnt);

	ret = sipa_nic_open(SIPA_TERM_VCP, 0, loop_test_notify_cb, NULL);
	if (ret < 0) {
		pr_info("%s sipa nic open failed\n", __func__);
		return ret;
	}

	s_test_ctrl.nic_id = ret;
	s_test_ctrl.send_stop = 0;
	s_test_ctrl.chk_index = 0;
	s_test_ctrl.send_index = 0;
	init_waitqueue_head(&s_test_ctrl.recv_wq);

	/* create channel thread for send */
	s_test_ctrl.send_thread = kthread_create(send_thread, NULL,
							  "looptest_send");
	if (IS_ERR(s_test_ctrl.send_thread)) {
		pr_err("Failed to create kthread: looptest_send\n");
		ret = PTR_ERR(s_test_ctrl.send_thread);
		return ret;
	}
	wake_up_process(s_test_ctrl.send_thread);

	/* create channel thread for recv */
	s_test_ctrl.recv_thread = kthread_create(recv_thread, NULL,
							  "looptest_recv");
	if (IS_ERR(s_test_ctrl.recv_thread)) {
		pr_err("Failed to create kthread: looptest_recv\n");
		ret = PTR_ERR(s_test_ctrl.recv_thread);
		return ret;
	}

	wake_up_process(s_test_ctrl.recv_thread);
	ipa_test_init_callback();

	return 0;
}

int sipa_loop_test_end(void)
{
	return 0;
}

//late_initcall(sipa_loop_test_start);
