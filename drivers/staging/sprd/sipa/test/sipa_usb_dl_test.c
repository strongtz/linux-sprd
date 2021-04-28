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

#include "../sipa_priv.h"
#include "sipa_test.h"
#include "sipa_periph_ep_io.h"


extern struct sipa_control s_sipa_ctrl;


struct sipa_usb_dl_test {
	struct sipa_periph_sender *cp_sender;
	struct sipa_periph_receiver *usb_receiver;

	struct task_struct *send_thread;
	int send_stop;
	u32 send_index;
};

static struct sipa_usb_dl_test s_usb_dl_test;

static int vcp_send_test_pkt(void)
{
	int ret;
	struct sk_buff *skb;
	int len;

	s_usb_dl_test.send_index++;

	len = s_usb_dl_test.send_index % 1400 + 1;
	skb = __dev_alloc_skb(len, GFP_KERNEL);
	if (!skb) {
		pr_err("failed to alloc skb!\n");
		return 0;
	} else {
		pr_info("len = %d skb->truesize = %d\n", len, skb->truesize);
	}
	skb_put(skb, len);
	memset(skb->data, len & 0xff, len);

	ret = sipa_periph_send_data(s_usb_dl_test.cp_sender,
								skb,
								SIPA_TERM_USB,
								0);
	if (ret) {
		s_usb_dl_test.send_stop = 1;
		pr_err("loop_test7 enter flow ctrl\n");
		dev_kfree_skb_any(skb);
		return -1;
	}

	return 0;
}

static int send_thread(void *data)
{
	u32 cnt  = 0;

	pr_info("[IPA] start send thread\n");
	msleep(30000);

	while (!kthread_should_stop()) {
		pr_info("%s cnt = %d\n", __func__, cnt++);
		if (!s_usb_dl_test.send_stop) {
			vcp_send_test_pkt();
			msleep(1000);
		} else {
			s_usb_dl_test.send_stop = 0;
			msleep(100);
		}
	}

	return 0;
}

static int dispatch_recv(struct sipa_periph_receiver *receiver,
						 struct sipa_hal_fifo_item *item,
						 struct sk_buff *skb)
{
	pr_info("%s received skb->len = %d\n", __func__, skb->len);
	dev_kfree_skb_any(skb);
	return 0;
}

int sipa_usb_dl_test_start(void)
{
	int ret = 0;

	pr_info("%s start......\n", __func__);

	sipa_test_enable_periph_int_to_sw();

	ret = create_sipa_periph_sender(s_sipa_ctrl.ctx,
									s_sipa_ctrl.eps[SIPA_EP_VCP],
									SIPA_PKT_IP,
									&s_usb_dl_test.cp_sender);
	if (ret) {
		ret = -EFAULT;
		goto sender_fail;
	}


	ret = create_sipa_periph_receiver(s_sipa_ctrl.ctx,
									  s_sipa_ctrl.eps[SIPA_EP_USB],
									  dispatch_recv,
									  &s_usb_dl_test.usb_receiver);

	if (ret) {
		ret = -EFAULT;
		goto receiver_fail;
	}

	sipa_receiver_add_sender(s_usb_dl_test.usb_receiver,
							 s_usb_dl_test.cp_sender);

	/* create channel thread for send */
	s_usb_dl_test.send_thread = kthread_create(send_thread, NULL,
								"usb_dl_test_send");
	if (IS_ERR(s_usb_dl_test.send_thread)) {
		pr_err("Failed to create kthread: usb_dl_test\n");
		ret = PTR_ERR(s_usb_dl_test.send_thread);
		return ret;
	}
	wake_up_process(s_usb_dl_test.send_thread);

	return 0;
receiver_fail:
	pr_err("%s receiver_fail ret:%d......\n", __func__, ret);
	if (s_usb_dl_test.usb_receiver) {
		destroy_sipa_periph_receiver(s_usb_dl_test.usb_receiver);
		s_usb_dl_test.usb_receiver = NULL;
	}

sender_fail:
	pr_err("%s sender_fail ret:%d......\n", __func__, ret);
	if (s_usb_dl_test.cp_sender) {
		destroy_sipa_periph_sender(s_usb_dl_test.cp_sender);
		s_usb_dl_test.cp_sender = NULL;
	}

	return ret;
}

int sipa_usb_dl_test_end(void)
{
	return 0;
}

late_initcall(sipa_usb_dl_test_start);

