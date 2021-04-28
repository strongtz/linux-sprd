/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) "imsbr: " fmt

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <net/addrconf.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/ip6_checksum.h>
#include <uapi/linux/ims_bridge/ims_bridge.h>

#include "imsbr_core.h"
#include "imsbr_packet.h"
#include "imsbr_sipc.h"
#include "imsbr_test.h"

#ifdef CONFIG_SPRD_IMS_BRIDGE_TEST

static u32 pressure_level = 2 * 1000 * 1000;

static char *test_peerip4 = "123.125.114.144";
module_param(test_peerip4, charp, 0644);

static char *test_peerip6 = "2008::777";
module_param(test_peerip6, charp, 0644);

static char *test_iface = "dummy0";
module_param(test_iface, charp, 0644);

static struct dentry *debugfs_root;

static void imsbr_test_howifi2lte(unsigned long unused);
static void imsbr_test_holte2wifi(unsigned long unused);
static void imsbr_test_hofinish(unsigned long unused);
static void imsbr_test_fragsize(unsigned long unused);
static void imsbr_test_echoping(unsigned long unused);
static void imsbr_test_v4packet(unsigned long is_input);
static void imsbr_test_v6packet(unsigned long is_input);
static void imsbr_test_cptuple(unsigned long unused);
static void imsbr_test_aptuple(unsigned long unused);
static void imsbr_test_sipc(unsigned long unused);
static void imsbr_test_pressure(unsigned long unused);

static struct {
	const char *name;
	void (*doit)(unsigned long);
	unsigned long arg;
} test_suites[] = {
	{ "ho-wifi2lte",	imsbr_test_howifi2lte,	0 },
	{ "ho-lte2wifi",	imsbr_test_holte2wifi,	0 },
	{ "ho-finish",		imsbr_test_hofinish,	0 },
	{ "test-fragsize",	imsbr_test_fragsize,	0 },
	{ "test-ping",		imsbr_test_echoping,	0 },
	{ "test-v4output",	imsbr_test_v4packet,	0 },
	{ "test-v6output",	imsbr_test_v6packet,	0 },
	{ "test-v4input",	imsbr_test_v4packet,	1 },
	{ "test-v6input",	imsbr_test_v6packet,	1 },
	{ "test-cptuple",	imsbr_test_cptuple,	0 },
	{ "test-aptuple",	imsbr_test_aptuple,	0 },
	{ "test-sipc",		imsbr_test_sipc,	0 },
	{ "test-pressure",	imsbr_test_pressure,	0 },
};

static int testsuite_print(struct seq_file *s, void *p)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(test_suites); i++)
		seq_printf(s, "%s\n", test_suites[i].name);

	return 0;
}

static int testsuite_open(struct inode *inode, struct file *file)
{
	return single_open(file, testsuite_print, inode->i_private);
}

static void imsbr_test_howifi2lte(unsigned long unused)
{
	imsbr_set_callstate(IMSBR_CALLS_END, 0);
	imsbr_set_callstate(IMSBR_CALLS_VOWIFI, 0);
	imsbr_set_callstate(IMSBR_CALLS_VOLTE, 0);
}

static void imsbr_test_holte2wifi(unsigned long unused)
{
	imsbr_set_callstate(IMSBR_CALLS_END, 0);
	imsbr_set_callstate(IMSBR_CALLS_VOLTE, 0);
	imsbr_set_callstate(IMSBR_CALLS_VOWIFI, 0);
}

static void imsbr_test_hofinish(unsigned long unused)
{
	imsbr_set_callstate(IMSBR_CALLS_END, 0);
}

static void imsbr_test_fragsize(unsigned long unused)
{
	u32 fragsz = imsbr_frag_size;
	struct sblock blk;

	if (!imsbr_build_cmd("test-fragsize", &blk, &fragsz, sizeof(fragsz)))
		imsbr_sblock_send(&imsbr_ctrl, &blk, sizeof(fragsz));
}

static void imsbr_test_echoping(unsigned long unused)
{
	char *hellostr = "hello ims bridge!";
	struct sblock blk;
	int hellolen;

	hellolen = strlen(hellostr) + 1;
	if (hellolen > IMSBR_MSG_MAXLEN) {
		pr_err("%s is too large, echo ping fail!\n", hellostr);
		return;
	}

	if (!imsbr_build_cmd("echo-ping", &blk, hellostr, hellolen))
		imsbr_sblock_send(&imsbr_ctrl, &blk, hellolen);
}

static void imsbr_test_packet(struct nf_conntrack_tuple *nft,
			      struct imsbr_packet *l3pkt, int l3len,
			      struct imsbr_packet *l4pkt, int l4len,
			      bool is_input)
{
	struct imsbr_tuple tuple = {};
	struct sblock blk;
	u16 flow_type;

	if (is_input) {
		/* Volte AP video engine solution. */
		flow_type = IMSBR_FLOW_CPTUPLE;
		tuple.media_type = IMSBR_MEDIA_RTP_VIDEO;
		tuple.link_type = IMSBR_LINK_CP;
		tuple.socket_type = IMSBR_SOCKET_AP;
	} else {
		/* Vowifi CP audio engine solution. */
		flow_type = IMSBR_FLOW_APTUPLE;
		tuple.media_type = IMSBR_MEDIA_RTP_AUDIO;
		tuple.link_type = IMSBR_LINK_AP;
		tuple.socket_type = IMSBR_SOCKET_CP;
	}

	imsbr_flow_add(nft, flow_type, &tuple);

	/* Simulate "fragments" */
	blk.addr = l3pkt;
	blk.length = l3len;
	imsbr_process_packet(&imsbr_ctrl, &blk, false);

	blk.addr = l4pkt;
	blk.length = l4len;
	imsbr_process_packet(&imsbr_ctrl, &blk, false);

	imsbr_flow_del(nft, flow_type, &tuple);
}

static void imsbr_test_v4packet(unsigned long is_input)
{
	char l3buf[sizeof(struct imsbr_packet) + sizeof(struct iphdr)];
	char l4buf[sizeof(struct imsbr_packet) + sizeof(struct udphdr) + 64];
	struct imsbr_packet *l3pkt = (struct imsbr_packet *)l3buf;
	struct imsbr_packet *l4pkt = (struct imsbr_packet *)l4buf;
	struct nf_conntrack_tuple nft = {};
	struct net_device *dev;
	struct iphdr *ip;
	struct udphdr *uh;
	__be32 localip = 0;
	__be32 peerip = 0;
	u16 totlen;

	memset(l3buf, 0, sizeof(l3buf));
	memset(l4buf, 0, sizeof(l4buf));

	totlen = sizeof(struct iphdr) + sizeof(l4buf) -
		 sizeof(struct imsbr_packet);

	INIT_IMSBR_PACKET(l3pkt, totlen);

	ip = (struct iphdr *)l3pkt->packet;
	ip->version = 4;
	ip->ihl = 5;
	ip->ttl = 255;
	ip->protocol = IPPROTO_UDP;
	ip->tot_len = htons(totlen);

	dev = dev_get_by_name(&init_net, test_iface);
	if (dev) {
		localip = inet_select_addr(dev, peerip, RT_SCOPE_UNIVERSE);
		dev_put(dev);
	}

	in4_pton(test_peerip4, strlen(test_peerip4) + 1, (u8 *)&peerip,
		 '\0', NULL);

	if (is_input) {
		nft.src.u3.ip = peerip;
		ip->saddr = peerip;

		nft.dst.u3.ip = localip;
		ip->daddr = localip;
	} else {
		nft.src.u3.ip = localip;
		ip->saddr = localip;

		nft.dst.u3.ip = peerip;
		ip->daddr = peerip;
	}
	ip->check = ip_fast_csum(ip, ip->ihl);

	INIT_IMSBR_PACKET(l4pkt, totlen);
	l4pkt->frag_off = sizeof(struct iphdr);

	uh = (struct udphdr *)l4pkt->packet;
	uh->source = htons(9999);
	uh->dest = htons(9999);
	uh->len = htons(totlen - sizeof(struct iphdr));

	nft.src.l3num = AF_INET;
	nft.dst.protonum = IPPROTO_UDP;

	imsbr_test_packet(&nft, l3pkt, sizeof(l3buf), l4pkt,
			  sizeof(l4buf), is_input);
}

static void imsbr_test_v6packet(unsigned long is_input)
{
	char l3buf[sizeof(struct imsbr_packet) + sizeof(struct ipv6hdr)];
	char l4buf[sizeof(struct imsbr_packet) + sizeof(struct udphdr) + 64];
	struct imsbr_packet *l3pkt = (struct imsbr_packet *)l3buf;
	struct imsbr_packet *l4pkt = (struct imsbr_packet *)l4buf;
	struct nf_conntrack_tuple nft = {};
	struct net_device *dev;
	struct ipv6hdr *ip6;
	struct udphdr *uh;
	struct in6_addr localip = {};
	struct in6_addr peerip = {};
	u16 dlen, totlen;

	memset(l3buf, 0, sizeof(l3buf));
	memset(l4buf, 0, sizeof(l4buf));

	dlen = sizeof(l4buf) - sizeof(struct imsbr_packet);
	totlen = dlen + sizeof(struct ipv6hdr);

	INIT_IMSBR_PACKET(l3pkt, totlen);

	ip6 = (struct ipv6hdr *)l3pkt->packet;
	ip6->version = 6;
	ip6->nexthdr = IPPROTO_UDP;
	ip6->payload_len = htons(dlen);
	ip6->hop_limit = 255;

	in6_pton(test_peerip6, strlen(test_peerip6) + 1, (u8 *)&peerip,
		 '\0', NULL);

	dev = dev_get_by_name(&init_net, test_iface);
	if (dev) {
		ipv6_dev_get_saddr(&init_net, dev, &peerip, 0, &localip);
		dev_put(dev);
	}

	if (is_input) {
		nft.src.u3.in6 = peerip;
		ip6->saddr = peerip;

		nft.dst.u3.in6 = localip;
		ip6->daddr = localip;
	} else {
		nft.src.u3.in6 = localip;
		ip6->saddr = localip;

		nft.dst.u3.in6 = peerip;
		ip6->daddr = peerip;
	}

	INIT_IMSBR_PACKET(l4pkt, totlen);
	l4pkt->frag_off = sizeof(struct ipv6hdr);

	uh = (struct udphdr *)l4pkt->packet;
	uh->source = htons(9999);
	uh->dest = htons(9999);
	uh->len = htons(dlen);
	uh->check = csum_ipv6_magic(&ip6->saddr, &ip6->daddr, dlen,
				    ip6->nexthdr, csum_partial(uh, dlen, 0));

	nft.src.l3num = AF_INET6;
	nft.dst.protonum = IPPROTO_UDP;

	imsbr_test_packet(&nft, l3pkt, sizeof(l3buf), l4pkt,
			  sizeof(l4buf), is_input);
}

static void imsbr_test_cptuple(unsigned long unused)
{
	struct nf_conntrack_tuple nft = {};
	struct imsbr_tuple tuple = {};

	nft.src.l3num = AF_INET;
	nft.dst.protonum = IPPROTO_TCP;
	nft.dst.u.all = htons(123);

	tuple.media_type = IMSBR_MEDIA_SIP;
	tuple.link_type = IMSBR_LINK_CP;
	tuple.socket_type = IMSBR_SOCKET_CP;

	imsbr_flow_add(&nft, IMSBR_FLOW_CPTUPLE, &tuple);
	imsbr_flow_add(&nft, IMSBR_FLOW_CPTUPLE, &tuple);

	nft.src.l3num = AF_INET6;
	tuple.media_type = IMSBR_MEDIA_RTP_AUDIO;
	imsbr_flow_add(&nft, IMSBR_FLOW_CPTUPLE, &tuple);
	imsbr_flow_add(&nft, IMSBR_FLOW_CPTUPLE, &tuple);
}

static void imsbr_test_aptuple(unsigned long unused)
{
	struct nf_conntrack_tuple nft = {};
	struct imsbr_tuple tuple = {};

	nft.src.l3num = AF_INET;
	nft.dst.protonum = IPPROTO_UDP;
	nft.dst.u.all = htons(8888);

	tuple.media_type = IMSBR_MEDIA_RTCP_VIDEO;
	tuple.link_type = IMSBR_LINK_AP;
	tuple.socket_type = IMSBR_SOCKET_AP;

	imsbr_flow_add(&nft, IMSBR_FLOW_APTUPLE, &tuple);
	imsbr_flow_add(&nft, IMSBR_FLOW_APTUPLE, &tuple);

	nft.src.l3num = AF_INET6;
	imsbr_flow_add(&nft, IMSBR_FLOW_APTUPLE, &tuple);
	imsbr_flow_add(&nft, IMSBR_FLOW_APTUPLE, &tuple);
}

static void imsbr_test_sblock(struct imsbr_sipc *sipc, int size)
{
	struct sblock *blk;
	int cnt, i;

	blk = kmalloc_array(sipc->blknum, sizeof(struct sblock), GFP_KERNEL);
	if (!blk)
		return;

	for (cnt = 0; cnt < sipc->blknum; cnt++) {
		if (imsbr_sblock_get(sipc, &blk[cnt], size))
			break;
	}

	pr_debug("%s alloc %d sblocks\n", sipc->desc, cnt);

	for (i = 0; i < cnt; i++)
		imsbr_sblock_put(sipc, &blk[i]);

	kfree(blk);
}

static void imsbr_test_sipc(unsigned long unused)
{
	imsbr_test_sblock(&imsbr_ctrl, IMSBR_MSG_MAXLEN);
	imsbr_test_sblock(&imsbr_data, IMSBR_PACKET_MAXSZ);
}

static int imsbr_test_kthread(void *arg)
{
	struct nf_conntrack_tuple tuple = {};
	int i;

	for (i = 0; i < pressure_level; i++) {
		imsbr_test_cptuple(0);
		imsbr_test_aptuple(0);

		rcu_read_lock();
		imsbr_flow_match(&tuple);
		imsbr_flow_match(&tuple);
		imsbr_flow_match(&tuple);
		rcu_read_unlock();

		imsbr_flow_reset(IMSBR_FLOW_CPTUPLE, 0, true);
		imsbr_flow_reset(IMSBR_FLOW_APTUPLE, 0, true);
	}

	return 0;
}

static void imsbr_test_pressure(unsigned long unused)
{
	const int nthread = 4;
	int i;

	for (i = 0; i < nthread; i++)
		kthread_run(imsbr_test_kthread, NULL, "imsbr-test%d", i);
}

static ssize_t testsuite_write(struct file *file,
			       const char __user *user_buf, size_t count,
			       loff_t *ppos)
{
	char buff[64];
	int val, i;

	val = strncpy_from_user(buff, user_buf,
				min_t(long, sizeof(buff) - 1, count));
	if (val < 0)
		return -EFAULT;

	buff[val] = '\0';
	strim(buff); /* Skip leading & tailing space */

	pr_debug("testsuite: %s\n", buff);

	for (i = 0; i < ARRAY_SIZE(test_suites); i++) {
		if (!strcmp(buff, test_suites[i].name)) {
			test_suites[i].doit(test_suites[i].arg);
			break;
		}
	}

	if (i == ARRAY_SIZE(test_suites)) {
		pr_err("[%s] is invalid\n", buff);
		return -EINVAL;
	}

	return count;
}

static const struct file_operations testsuite_fops = {
	.open		= testsuite_open,
	.write		= testsuite_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};

int __init imsbr_test_init(void)
{
	debugfs_root = debugfs_create_dir("ims_bridge", NULL);
	if (!debugfs_root)
		return -ENOMEM;

	debugfs_create_file("test-suite", 0644, debugfs_root, NULL,
			    &testsuite_fops);
	debugfs_create_u32("pressure_level", 0644, debugfs_root,
			   &pressure_level);

	return 0;
}

void imsbr_test_exit(void)
{
	debugfs_remove_recursive(debugfs_root);
}

#else
int __init imsbr_test_init(void)
{
	return 0;
}

void imsbr_test_exit(void)
{
}

#endif /* CONFIG_SPRD_IMS_BRIDGE_TEST */
