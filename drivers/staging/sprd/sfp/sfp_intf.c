/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <linux/icmp.h>
#include <linux/ipv6.h>
#include <net/ip6_checksum.h>

#include "sfp.h"
#include "sfp_hash.h"

#define DEFAULT_BUFFER_PREFIX_OFFSET 14

static void FP_PRT_PKT(int dbg_lvl, void *data, int outif, char *iface)
{
	void *l3hdhr;
	struct iphdr *ip4hdr;
	struct ipv6hdr *ip6hdr;
	u32 l4offset;
	u8 ver, l4proto;

	if (!(fp_dbg_lvl & dbg_lvl))
		return;

	if (!data)
		return;

	l3hdhr = data;
	ver = ((struct iphdr *)l3hdhr)->version;

	switch (ver) {
	case 0x4:
	{
		u32 src, dst;

		ip4hdr = (struct iphdr *)l3hdhr;
		l4offset = ip4hdr->ihl << 2;
		l4proto = ip4hdr->protocol;
		src = ntohl(ip4hdr->saddr);
		dst = ntohl(ip4hdr->daddr);
		switch (l4proto) {
		case IP_L4_PROTO_TCP:
		{
			struct tcphdr *hp = (struct tcphdr *)(data + l4offset);

			FP_PRT_DBG(
				dbg_lvl,
			"%s:SIP=%u.%u.%u.%u, DIP=%u.%u.%u.%u, IP/TCP, " IPID
			", cksum(%x), " TCP_FMT ", len=%d, %s, out if (%d).\n",
			iface,
			IP_SFT(src, 24), IP_SFT(src, 16),
			IP_SFT(src, 8), IP_SFT(src, 0),
			IP_SFT(dst, 24), IP_SFT(dst, 16),
			IP_SFT(dst, 8), IP_SFT(dst, 0),
			ntohs(ip4hdr->id),
			ntohs(hp->check),
			ntohl(hp->seq),
			ntohl(hp->ack_seq),
			ntohs(hp->source),
			ntohs(hp->dest),
			ntohs(ip4hdr->tot_len),
			get_tcp_flag(hp),
			outif);
			return;
		}
		case IP_L4_PROTO_UDP:
		{
			struct udphdr *hp = (struct udphdr *)(data + l4offset);

			FP_PRT_DBG(
				dbg_lvl,
			"%s:SIP=%u.%u.%u.%u, DIP=%u.%u.%u.%u, IP/UDP, " IPID
			", cksum(%x), %d -> %d, len=%d, out if (%d).\n",
			iface,
			IP_SFT(src, 24), IP_SFT(src, 16),
			IP_SFT(src, 8), IP_SFT(src, 0),
			IP_SFT(dst, 24), IP_SFT(dst, 16),
			IP_SFT(dst, 8), IP_SFT(dst, 0),
			ntohs(ip4hdr->id),
			ntohs(hp->check),
			ntohs(hp->source),
			ntohs(hp->dest),
			ntohs(ip4hdr->tot_len),
			outif);
			return;
		}
		default:
			FP_PRT_DBG(
				dbg_lvl,
			"%s:SIP=%u.%u.%u.%u, DIP=%u.%u.%u.%u, IP/%d, " IPID
			", len=%d, out if (%d).\n",
			iface,
			IP_SFT(src, 24), IP_SFT(src, 16),
			IP_SFT(src, 8), IP_SFT(src, 0),
			IP_SFT(dst, 24), IP_SFT(dst, 16),
			IP_SFT(dst, 8), IP_SFT(dst, 0),
			ip4hdr->protocol,
			ntohs(ip4hdr->id),
			ntohs(ip4hdr->tot_len),
			outif);
			return;
		}
	}
	case 0x6:
	{
		struct in6_addr src;
		struct in6_addr dst;

		ip6hdr = (struct ipv6hdr *)l3hdhr;
		l4offset = sizeof(struct ipv6hdr);
		l4proto = ip6hdr->nexthdr;
		src = ip6hdr->saddr;
		dst = ip6hdr->daddr;

		switch (l4proto) {
		case IP_L4_PROTO_TCP:
		{
			struct tcphdr *hp = (struct tcphdr *)(data + l4offset);

			FP_PRT_DBG(
				dbg_lvl,
			"%s:SIP=" NIP6_FMT ",DIP=" NIP6_FMT
			", IP6/TCP, cksum(%x), " TCP_FMT
			", len=%d, %s, out if (%d).\n",
			iface,
			ntohs(src.s6_addr16[0]), ntohs(src.s6_addr16[1]),
			ntohs(src.s6_addr16[2]), ntohs(src.s6_addr16[3]),
			ntohs(src.s6_addr16[4]), ntohs(src.s6_addr16[5]),
			ntohs(src.s6_addr16[6]), ntohs(src.s6_addr16[7]),
			ntohs(dst.s6_addr16[0]), ntohs(dst.s6_addr16[1]),
			ntohs(dst.s6_addr16[2]), ntohs(dst.s6_addr16[3]),
			ntohs(dst.s6_addr16[4]), ntohs(dst.s6_addr16[5]),
			ntohs(dst.s6_addr16[6]), ntohs(dst.s6_addr16[7]),
			ntohs(hp->check),
			ntohl(hp->seq),
			ntohl(hp->ack_seq),
			ntohs(hp->source),
			ntohs(hp->dest),
			ntohs(ip6hdr->payload_len),
			get_tcp_flag(hp),
			outif);
			return;
		}
		case IP_L4_PROTO_UDP:
		{
			struct udphdr *hp = (struct udphdr *)(data + l4offset);

			FP_PRT_DBG(
				dbg_lvl,
			"%s:SIP=" NIP6_FMT ",DIP=" NIP6_FMT
			", IP6/UDP, cksum(%x), %d -> %d, len=%d, out if (%d).\n",
			iface,
			ntohs(src.s6_addr16[0]), ntohs(src.s6_addr16[1]),
			ntohs(src.s6_addr16[2]), ntohs(src.s6_addr16[3]),
			ntohs(src.s6_addr16[4]), ntohs(src.s6_addr16[5]),
			ntohs(src.s6_addr16[6]), ntohs(src.s6_addr16[7]),
			ntohs(dst.s6_addr16[0]), ntohs(dst.s6_addr16[1]),
			ntohs(dst.s6_addr16[2]), ntohs(dst.s6_addr16[3]),
			ntohs(dst.s6_addr16[4]), ntohs(dst.s6_addr16[5]),
			ntohs(dst.s6_addr16[6]), ntohs(dst.s6_addr16[7]),
			ntohs(hp->check),
			ntohs(hp->source),
			ntohs(hp->dest),
			ntohs(ip6hdr->payload_len),
			outif);
			return;
		}
		default:
			FP_PRT_DBG(
				dbg_lvl,
			"%s:SIP=" NIP6_FMT ",DIP=" NIP6_FMT
			", IP6/%d, len=%d, out if (%d).\n",
			iface,
			ntohs(src.s6_addr16[0]), ntohs(src.s6_addr16[1]),
			ntohs(src.s6_addr16[2]), ntohs(src.s6_addr16[3]),
			ntohs(src.s6_addr16[4]), ntohs(src.s6_addr16[5]),
			ntohs(src.s6_addr16[6]), ntohs(src.s6_addr16[7]),
			ntohs(dst.s6_addr16[0]), ntohs(dst.s6_addr16[1]),
			ntohs(dst.s6_addr16[2]), ntohs(dst.s6_addr16[3]),
			ntohs(dst.s6_addr16[4]), ntohs(dst.s6_addr16[5]),
			ntohs(dst.s6_addr16[6]), ntohs(dst.s6_addr16[7]),
			ip6hdr->nexthdr,
			ntohs(ip6hdr->payload_len),
			outif);
			return;
		}
	}
	default:
		FP_PRT_DBG(
			FP_PRT_DEBUG,
			"unknown pkt type.\n");
		return;
	}
}

/*
 *  Check the L4 protocol type and
 *  return the source port and destination port
 *  from the packet.
 */
u8 sfp_check_l4proto(u16 l3proto,
		     void *iph,
		     u16 *dstport,
		     u16 *srcport)
{
	u8 proto;

	if (l3proto == NFPROTO_IPV4) {
		struct iphdr *iphdr2 = (struct iphdr *)iph;

		proto = iphdr2->protocol;
	} else {
		struct ipv6hdr *ip6hdr = (struct ipv6hdr *)iph;

		proto = ip6hdr->nexthdr;
	}
	switch (proto) {
	case IP_L4_PROTO_TCP:
	case IP_L4_PROTO_UDP:
	case IP_L4_PROTO_ICMP:
	case IP_L4_PROTO_ICMP6:
		break;
	default:
		/* Skip NAT processing at all */
		proto = IP_L4_PROTO_NULL;
	}
	return proto;
}

/* Do checksum */
void sfp_update_checksum(void *ipheader,
			 int  pkt_totlen,
			 u16 l3proto,
			 u32 l3offset,
			 u8 l4proto,
			 u32 l4offset)
{
	struct iphdr *iphhdr;
	struct ipv6hdr *ip6hdr;

	/* Do tcp checksum */
	switch (l4proto) {
	case IP_L4_PROTO_TCP:
	{
		unsigned int offset;
		unsigned int payload_csum;
		struct tcphdr *th;

		th = (struct tcphdr *)(ipheader + l4offset);
		th->check = 0;
		offset = l4offset + th->doff * 4;
		payload_csum = csum_partial(ipheader + offset,
					    pkt_totlen - offset,
					    0);
		if (l3proto == NFPROTO_IPV4) {
			iphhdr = (struct iphdr *)ipheader;
			th->check = tcp_v4_check(pkt_totlen - l4offset,
						 iphhdr->saddr,
						 iphhdr->daddr,
						 csum_partial(th,
							      th->doff << 2,
							      payload_csum));
		} else {
			ip6hdr = (struct ipv6hdr *)ipheader;
		th->check = tcp_v6_check(pkt_totlen - l4offset,
					 (struct in6_addr *)&ip6hdr->saddr,
					 (struct in6_addr *)&ip6hdr->daddr,
					 csum_partial(th,
						      th->doff << 2,
						      payload_csum));
		}
		return;
	}
	case IP_L4_PROTO_UDP:
	{
		/* Do udp checksum */
		int l4len, offset;
		struct udphdr *uh;
		unsigned int payload_csum;

		uh = (struct udphdr *)(ipheader + l4offset);
		uh->check = 0;
		l4len = 8;
		offset = l4offset + 8;
		payload_csum = csum_partial(ipheader + offset,
					    pkt_totlen - offset,
					    0);
		if (l3proto == NFPROTO_IPV4) {
			iphhdr = (struct iphdr *)ipheader;
			uh->check = udp_v4_check(pkt_totlen - l4offset,
						 iphhdr->saddr,
						 iphhdr->daddr,
						 csum_partial(uh,
							      8,
							      payload_csum));
		} else {
			ip6hdr = (struct ipv6hdr *)ipheader;

			uh->check =
				udp_v6_check(pkt_totlen - l4offset,
					     (struct in6_addr *)&ip6hdr->saddr,
					     (struct in6_addr *)&ip6hdr->daddr,
					     csum_partial(uh, 8, payload_csum));
		}
		return;
	}
	default:
		return;
	}
}

/*
 * Update the ip header and tcp header
 * and add mac header
 */
bool sfp_update_pkt_header(int ifindex,
			   void *data,
			   u32 l3offset,
			   u32 l4offset,
			   struct sfp_trans_tuple ret_info)
{
	struct tcphdr *ptcphdr;
	struct udphdr *pudphdr;
	struct icmphdr *picmphdr;
	struct icmp6hdr *picmp6hdr;
	u8 *mac_head;
	u8 l4proto;
	u8 l3proto;
	struct iphdr *iphhdr;
	struct ipv6hdr *ip6hdr;
	u8 *l3data;
	struct sk_buff *skb;

	skb = (struct sk_buff *)data;
	l3data = skb_network_header(skb);
	mac_head = (u8 *)l3data - ETH_HLEN;

	l3proto = ret_info.trans_info.l3_proto;
	l4proto = ret_info.trans_info.l4_proto;

	if (l4proto == IP_L4_PROTO_TCP) {
		ptcphdr = (struct tcphdr *)((unsigned char *)l3data + l4offset);
		if (ptcphdr->fin == 1 || ptcphdr->rst == 1) {
			FP_PRT_DBG(
				FP_PRT_DEBUG,
				"Get TCP FIN or RST pkt(%u-%u).(%d-%d)\n",
				ntohs(ptcphdr->dest),
				ntohs(ptcphdr->source),
				ptcphdr->fin,
				ptcphdr->rst);
			return false;
		}
	}

	switch (l3proto) {
	case NFPROTO_IPV4:
		iphhdr = (struct iphdr *)l3data;
		iphhdr->daddr = ret_info.trans_info.dst_ip.ip;
		iphhdr->saddr = ret_info.trans_info.src_ip.ip;
		iphhdr->ttl--;
		ip_send_check(iphhdr);
		*((u16 *)(mac_head + 12)) =  htons(0x0800);
		break;
	case NFPROTO_IPV6:
		ip6hdr = (struct ipv6hdr *)l3data;
		ip6hdr->hop_limit--;
		*((u16 *)(mac_head + 12)) =  htons(0x86dd);
		break;
	default:
		break;
	}
	switch (l4proto) {
	case IP_L4_PROTO_TCP:
		ptcphdr = (struct tcphdr *)((unsigned char *)l3data + l4offset);
		ptcphdr->dest = ret_info.trans_info.dst_l4_info.all;
		ptcphdr->source = ret_info.trans_info.src_l4_info.all;
		mac_addr_copy(mac_head, ret_info.trans_mac_info.dst_mac);
		mac_addr_copy(mac_head + 6, ret_info.trans_mac_info.src_mac);
		break;
	case IP_L4_PROTO_UDP:
		pudphdr = (struct udphdr *)((unsigned char *)l3data + l4offset);
		pudphdr->dest = ret_info.trans_info.dst_l4_info.all;
		pudphdr->source = ret_info.trans_info.src_l4_info.all;
		mac_addr_copy(mac_head, ret_info.trans_mac_info.dst_mac);
		mac_addr_copy(mac_head + 6, ret_info.trans_mac_info.src_mac);
		break;
	case IP_L4_PROTO_ICMP:
		picmphdr =
			(struct icmphdr *)((unsigned char *)l3data + l4offset);
		/*icmp id in dst_l4_info*/
		picmphdr->un.echo.id = ret_info.trans_info.dst_l4_info.all;
		mac_addr_copy(mac_head, ret_info.trans_mac_info.dst_mac);
		mac_addr_copy(mac_head + 6, ret_info.trans_mac_info.src_mac);
		break;
	case IP_L4_PROTO_ICMP6:
		picmp6hdr =
			(struct icmp6hdr *)((unsigned char *)l3data + l4offset);
		picmp6hdr->icmp6_identifier =
			ret_info.trans_info.dst_l4_info.all;
		mac_addr_copy(mac_head, ret_info.trans_mac_info.dst_mac);
		mac_addr_copy(mac_head + 6, ret_info.trans_mac_info.src_mac);
		break;
	default:
		FP_PRT_DBG(FP_PRT_DEBUG, "Unexpected IP protocol.\n");
		return false;
	}
	return true;
}

int sfp_check_mod_pkts(u32 ifindex,
		       void *data,
		       u16 l3proto,
		       void *iph)
{
	struct sk_buff *skb;
	u8  proto;
	u16 srcport, dstport;
	struct sfp_trans_tuple ret_info;
	int out_ifindex;
	struct iphdr *iphdr2;
	u32 dip, sip;
	struct ipv6hdr *ip6hdr;
	struct nf_conntrack_tuple tuple;
	u32 l3offset;
	u32 l4offset;
	u8 *ip_header;
	u32 totlen;
	int ret = -SFP_FAIL;
	bool sfp_ret = false;

	if (l3proto == NFPROTO_IPV4) {
		iphdr2 = (struct iphdr *)iph;
		dip = iphdr2->daddr;
		sip = iphdr2->saddr;
		proto = sfp_check_l4proto(l3proto, (void *)iphdr2,
					  &dstport, &srcport);
	} else {
		ip6hdr = (struct ipv6hdr *)iph;
		proto = sfp_check_l4proto(l3proto, (void *)ip6hdr,
					  &dstport, &srcport);
	}

	if (proto == IP_L4_PROTO_NULL) {
		/* NAT not supported for this protocol */
		FP_PRT_DBG(FP_PRT_DEBUG, "proto [%d] no supported.\n", proto);
		return ret; /* No support protocol */
	}

	memset(&tuple, 0, sizeof(struct nf_conntrack_tuple));
	skb = (struct sk_buff *)data;
	sfp_ret = sfp_pkt_to_tuple(skb->data,
				   skb_network_offset(skb),
				   &tuple,
				   &l4offset);
	l3offset = skb_network_offset(skb);
	ip_header = skb->data + skb_network_offset(skb);
	totlen = skb->len;

	if (!sfp_ret)
		return ret;

	/* Lookup fwd entries accordingly with 5 tuple key */
	ret = check_sfp_fwd_table(&tuple, &ret_info);
	if (ret != SFP_OK) {
		FP_PRT_DBG(FP_PRT_DEBUG, "SFP_MGR: no fwd entries.\n");
		return -ret;
	}
	out_ifindex = ret_info.out_ifindex;

	sfp_ret = sfp_update_pkt_header(ifindex,
					data, l3offset,
					l4offset, ret_info);
	if (!sfp_ret)
		return -SFP_FAIL;

	sfp_update_checksum((void *)ip_header,
			    totlen,
			    l3proto,
			    l3offset,
			    proto,
			    l4offset);

	skb_push(skb, ETH_HLEN);
	return out_ifindex;
}

/*
 * 1.check the data if it will be forward. If it will be,
 * modify the header according to spread fast path forwarding
 * table sfpRuleDB and the return value is zero.
 * If it will be not, the return value is not zero.
 * Parameter:
 *    1.IN int in_if: SFP_INTERFACE_LTE or SFP_INTERFACE_USB
 *    2.IN unsigned char *pDataHeader: From USB, the pDataHeader is skb pointer,
 *      and From LTE,the pDataHeader is head pointer.
 *    3.INOUT int *offset:From USB, *offset is null,and from LTE,
 *      the offset is the interval between header which is ip header or
 *      mac header and head pointer.
 *    4.INOUT int *pDataLen: the pkt total length
 *    5.OUT int *out_if: the interface is which the pkt will be sent to.
 *    Return: 0. the pkt will  be forward and the ip header and udp or
 *                tcp header is ready.
 *            >0. the pkt should be sent to network subsystem.
 */
int soft_fastpath_process(int in_if,
			  void *data_header,
			  unsigned short *offset,
			  int *plen,
			  int *out_if)
{
	struct iphdr *piphdr;
	struct sk_buff *skb;
	char *link_dir;

	if (get_sfp_enable() == 0)
		return 1;

	if (in_if == SFP_INTERFACE_LTE)
		link_dir = "LTE IN";
	else
		link_dir = "USB IN";

	/* Check whether is ip or ip6 header */
	skb = (struct sk_buff *)data_header;
	piphdr = ip_hdr(skb);
	if (piphdr->version == 0x04) {
		*out_if = sfp_check_mod_pkts(
			in_if,
			(void *)skb,
			NFPROTO_IPV4,
			piphdr);

		FP_PRT_PKT(FP_PRT_INFO, piphdr, *out_if, link_dir);
		if (*out_if < 0)
			return 1;
		else
			return 0;
	} else {
		struct ipv6hdr *ip6hdr = (struct ipv6hdr *)piphdr;
		*out_if = sfp_check_mod_pkts(
			in_if,
			(void *)skb,
			NFPROTO_IPV6,
			ip6hdr);

		FP_PRT_PKT(FP_PRT_INFO, piphdr, *out_if, link_dir);
		if (*out_if < 0)
			return 1;
		else
			return 0;
	}

	return 1;
}
EXPORT_SYMBOL(soft_fastpath_process);

struct net_device *netdev_get_by_index(int ifindex)
{
	struct net_device *dev;

	if (!ifindex)
		return NULL;
	rcu_read_lock();
	dev = dev_get_by_index_rcu(&init_net, ifindex);
	if (dev)
		dev_hold(dev);
	rcu_read_unlock();
	return dev;
}
EXPORT_SYMBOL(netdev_get_by_index);

