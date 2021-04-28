/*
 * xfrm_output.c - Common IPsec encapsulation code.
 *
 * Copyright (c) 2007 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <net/dst.h>
#include <net/xfrm.h>

static int xfrm_output2(struct net *net, struct sock *sk, struct sk_buff *skb);
#ifdef CONFIG_XFRM_FRAGMENT
static int xfrm_output_resume_frag(struct sk_buff *skb, int err);
#endif

static int xfrm_skb_check_space(struct sk_buff *skb)
{
	struct dst_entry *dst = skb_dst(skb);
	int nhead = dst->header_len + LL_RESERVED_SPACE(dst->dev)
		- skb_headroom(skb);
	int ntail = dst->dev->needed_tailroom - skb_tailroom(skb);

	if (nhead <= 0) {
		if (ntail <= 0)
			return 0;
		nhead = 0;
	} else if (ntail < 0)
		ntail = 0;

	return pskb_expand_head(skb, nhead, ntail, GFP_ATOMIC);
}

/* Children define the path of the packet through the
 * Linux networking.  Thus, destinations are stackable.
 */

static struct dst_entry *skb_dst_pop(struct sk_buff *skb)
{
	struct dst_entry *child = dst_clone(skb_dst(skb)->child);

	skb_dst_drop(skb);
	return child;
}

static int xfrm_output_one(struct sk_buff *skb, int err)
{
	struct dst_entry *dst = skb_dst(skb);
	struct xfrm_state *x = dst->xfrm;
	struct net *net = xs_net(x);

	if (err <= 0)
		goto resume;

	do {
		err = xfrm_skb_check_space(skb);
		if (err) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTERROR);
			goto error_nolock;
		}

		skb->mark = xfrm_smark_get(skb->mark, x);

		err = x->outer_mode->output(x, skb);
		if (err) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTSTATEMODEERROR);
			goto error_nolock;
		}

		spin_lock_bh(&x->lock);

		if (unlikely(x->km.state != XFRM_STATE_VALID)) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTSTATEINVALID);
			err = -EINVAL;
			goto error;
		}

		err = xfrm_state_check_expire(x);
		if (err) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTSTATEEXPIRED);
			goto error;
		}

		err = x->repl->overflow(x, skb);
		if (err) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTSTATESEQERROR);
			goto error;
		}

		x->curlft.bytes += skb->len;
		x->curlft.packets++;
		x->lastused = ktime_get_real_seconds();

		spin_unlock_bh(&x->lock);

		skb_dst_force(skb);
		if (!skb_dst(skb)) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTERROR);
			err = -EHOSTUNREACH;
			goto error_nolock;
		}

		if (xfrm_offload(skb)) {
			x->type_offload->encap(x, skb);
		} else {
			/* Inner headers are invalid now. */
			skb->encapsulation = 0;

			err = x->type->output(x, skb);
			if (err == -EINPROGRESS)
				goto out;
		}

resume:
		if (err) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTSTATEPROTOERROR);
			goto error_nolock;
		}

		dst = skb_dst_pop(skb);
		if (!dst) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTERROR);
			err = -EHOSTUNREACH;
			goto error_nolock;
		}
		skb_dst_set(skb, dst);
		x = dst->xfrm;
	} while (x && !(x->outer_mode->flags & XFRM_MODE_FLAG_TUNNEL));

	return 0;

error:
	spin_unlock_bh(&x->lock);
error_nolock:
	kfree_skb(skb);
out:
	return err;
}

int xfrm_output_resume(struct sk_buff *skb, int err)
{
	struct net *net = xs_net(skb_dst(skb)->xfrm);

#ifdef CONFIG_XFRM_FRAGMENT
	/*err=0 means that hw crypto callback to call this func.
	 *err=1 the normal processing flow.
	 *author:junjie.wang@spreadtrum.com@6704
	 */
	if (net && net->xfrm.enable_xfrm_fragment)
		return xfrm_output_resume_frag(skb, err);
#endif
	while (likely((err = xfrm_output_one(skb, err)) == 0)) {
		nf_reset(skb);

		err = skb_dst(skb)->ops->local_out(net, skb->sk, skb);
		if (unlikely(err != 1))
			goto out;

		if (!skb_dst(skb)->xfrm)
			return dst_output(net, skb->sk, skb);

		err = nf_hook(skb_dst(skb)->ops->family,
			      NF_INET_POST_ROUTING, net, skb->sk, skb,
			      NULL, skb_dst(skb)->dev, xfrm_output2);
		if (unlikely(err != 1))
			goto out;
	}

	if (err == -EINPROGRESS)
		err = 0;

out:
	return err;
}
EXPORT_SYMBOL_GPL(xfrm_output_resume);

#ifdef CONFIG_XFRM_FRAGMENT

static inline int ipv4v6_skb_dst_mtu(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);

	if (iph->version == 0x04) {
		struct inet_sock *inet = skb->sk ? inet_sk(skb->sk) : NULL;

		return (inet && inet->pmtudisc == IP_PMTUDISC_PROBE) ?
			skb_dst(skb)->dev->mtu : dst_mtu(skb_dst(skb));
	} else{
		struct ipv6_pinfo *np = skb->sk ? inet6_sk(skb->sk) : NULL;

		return (np && np->pmtudisc == IPV6_PMTUDISC_PROBE) ?
			skb_dst(skb)->dev->mtu : dst_mtu(skb_dst(skb));
	}
}

/*only do an encryption process.*/
static int __xfrm_output_resume_ss_once(struct sk_buff *skb, int err)
{
	struct net *net = xs_net(skb_dst(skb)->xfrm);

	err = xfrm_output_one(skb, err);
	if (likely(err == 0)) {
		nf_reset(skb);

		err = skb_dst(skb)->ops->local_out(net, skb->sk, skb);
		/*local_out ->__ip_local_out*/
		if (unlikely(err != 1))
			goto out;

		if (!skb_dst(skb)->xfrm)
			return dst_output(net, skb->sk, skb);

		err = nf_hook(skb_dst(skb)->ops->family,
			      NF_INET_POST_ROUTING, net, skb->sk, skb,
			      NULL, skb_dst(skb)->dev, xfrm_output2);
		/*nf_hook  !->xfrm_output2*/

		if (unlikely(err != 1))
			goto out;
	}

	if (err == -EINPROGRESS)
		err = 0;
out:
	return err;
}

static int __xfrm_output_resume_ss_after_frag(struct net *net,
					      struct sock *sock,
					      struct sk_buff *skb)
{
	return  __xfrm_output_resume_ss_once(skb, 1);
}

/*xfrm_output_resume_sub:
 * parameter:
 *  pmtu:the minimal mtu in data path.
 */
static int xfrm_output_resume_frag_sub(struct sk_buff *skb,
				       int pmtu,
				       int err,
				       int *exit)
{
	struct dst_entry *dst = skb_dst(skb);
	struct xfrm_state *x = dst->xfrm;
	struct net *net = xs_net(skb_dst(skb)->xfrm);
	*exit = 0;

	if (skb_dst(skb)->child && !((skb_dst(skb)->child)->xfrm)) {
		/*it is external xfrm,when done,exit the loop while*/
		*exit = 1;
	}
	/*do the fragment,must meet the condition
	 *1.tunnel mode
	 *2.len > mtu
	 *3.no gso
	 *4.the return err is bigger than 0.
	 */
	pmtu = pmtu - x->props.header_len - x->props.trailer_len;
	if (unlikely(pmtu < 0)) {
		printk_ratelimited(KERN_ERR
	"The mtu is too small,pmtu=%d,to keep communication,set the pmtu quals to 1400\n",
				   pmtu);
		pmtu = (XFRM_FRAG_DEFAULT_MTU
			- x->props.header_len
			- x->props.trailer_len);
	}
	if ((x->props.mode == XFRM_MODE_TUNNEL &&
	     (skb->len > pmtu && !skb_is_gso(skb))) &&
	     err > 0 &&
	    (*exit == 1)) {
		struct iphdr *iph = ip_hdr(skb);
		int segs = 0;
		int seg_pmtu = 0;
		/* According to ip headr type v4 or v6
		 * to choose which fragement to be done.
		 */
		/* Bug 707083 avoid too small pkt,
		 * so be average divided into serval parts.
		 */
		segs = skb->len / pmtu;
		segs++;
		seg_pmtu = skb->len / segs;
		seg_pmtu = (seg_pmtu / 4 + 1) * 4;
		if (iph->version == 0x04) {
			/* If skb payload is esp,do fragment.
			 * even the esp payload is tcp.
			 */
			if (iph->protocol == IPPROTO_ESP &&
			    skb->ignore_df == 0)
				skb->ignore_df = 1;

			seg_pmtu += iph->ihl * 4;
			if (seg_pmtu > pmtu)
				seg_pmtu = pmtu;
			printk_ratelimited(KERN_ERR
		"IPv4:The pkt is average divided into %d parts with mtu %d.\n",
			segs, seg_pmtu);
			return ip4_do_xfrm_frag(net, skb->sk, skb, seg_pmtu,
					__xfrm_output_resume_ss_after_frag);
		} else {
			struct ipv6hdr *ip6h = ipv6_hdr(skb);
			unsigned int hlen = 0;
			u8 *prevhdr = NULL;
			/* If skb payload is esp,do fragment.
			 * even tht esp payload is tcp.
			 */
			if (ip6h->nexthdr == NEXTHDR_FRAGMENT)
				goto  dont_frag;
			hlen = ip6_find_1stfragopt(skb, &prevhdr);
			seg_pmtu += sizeof(struct frag_hdr) + hlen;
			if (seg_pmtu > pmtu)
				seg_pmtu = pmtu;
			printk_ratelimited(KERN_ERR
		"IPv6:The pkt is average divided into %d parts with mtu %d.\n",
			segs, seg_pmtu);
			if (ip6h->nexthdr == IPPROTO_ESP && skb->ignore_df == 0)
				skb->ignore_df = 1;

			return  ip6_do_xfrm_frag(net, skb->sk, skb, seg_pmtu,
				       __xfrm_output_resume_ss_after_frag);
		}
	}
dont_frag:
	return __xfrm_output_resume_ss_once(skb, err);
}

static int xfrm_output_resume_frag(struct sk_buff *skb, int err)
{
	int exit = 0;
	int pmtu = XFRM_FRAG_DEFAULT_MTU;

	pmtu = ipv4v6_skb_dst_mtu(skb);
	while (1) {
		err = xfrm_output_resume_frag_sub(skb, pmtu, err, &exit);
		if (err == 1 && !exit) {
			pmtu = ipv4v6_skb_dst_mtu(skb);
			continue;
		}
		break;
	}
	return err;
}
#endif
static int xfrm_output2(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	return xfrm_output_resume(skb, 1);
}

static int xfrm_output_gso(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	struct sk_buff *segs;

	BUILD_BUG_ON(sizeof(*IPCB(skb)) > SKB_SGO_CB_OFFSET);
	BUILD_BUG_ON(sizeof(*IP6CB(skb)) > SKB_SGO_CB_OFFSET);
	segs = skb_gso_segment(skb, 0);
	kfree_skb(skb);
	if (IS_ERR(segs))
		return PTR_ERR(segs);
	if (segs == NULL)
		return -EINVAL;

	do {
		struct sk_buff *nskb = segs->next;
		int err;

		segs->next = NULL;
		err = xfrm_output2(net, sk, segs);

		if (unlikely(err)) {
			kfree_skb_list(nskb);
			return err;
		}

		segs = nskb;
	} while (segs);

	return 0;
}

int xfrm_output(struct sock *sk, struct sk_buff *skb)
{
	struct net *net = dev_net(skb_dst(skb)->dev);
	struct xfrm_state *x = skb_dst(skb)->xfrm;
	int err;

	secpath_reset(skb);

	if (xfrm_dev_offload_ok(skb, x)) {
		struct sec_path *sp;

		sp = secpath_dup(skb->sp);
		if (!sp) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTERROR);
			kfree_skb(skb);
			return -ENOMEM;
		}
		if (skb->sp)
			secpath_put(skb->sp);
		skb->sp = sp;
		skb->encapsulation = 1;

		sp->olen++;
		sp->xvec[skb->sp->len++] = x;
		xfrm_state_hold(x);

		if (skb_is_gso(skb)) {
			skb_shinfo(skb)->gso_type |= SKB_GSO_ESP;

			return xfrm_output2(net, sk, skb);
		}

		if (x->xso.dev && x->xso.dev->features & NETIF_F_HW_ESP_TX_CSUM)
			goto out;
	}

	if (skb_is_gso(skb))
		return xfrm_output_gso(net, sk, skb);

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		err = skb_checksum_help(skb);
		if (err) {
			XFRM_INC_STATS(net, LINUX_MIB_XFRMOUTERROR);
			kfree_skb(skb);
			return err;
		}
	}

out:
	return xfrm_output2(net, sk, skb);
}
EXPORT_SYMBOL_GPL(xfrm_output);

int xfrm_inner_extract_output(struct xfrm_state *x, struct sk_buff *skb)
{
	struct xfrm_mode *inner_mode;
	if (x->sel.family == AF_UNSPEC)
		inner_mode = xfrm_ip2inner_mode(x,
				xfrm_af2proto(skb_dst(skb)->ops->family));
	else
		inner_mode = x->inner_mode;

	if (inner_mode == NULL)
		return -EAFNOSUPPORT;
	return inner_mode->afinfo->extract_output(x, skb);
}
EXPORT_SYMBOL_GPL(xfrm_inner_extract_output);

void xfrm_local_error(struct sk_buff *skb, int mtu)
{
	unsigned int proto;
	struct xfrm_state_afinfo *afinfo;

	if (skb->protocol == htons(ETH_P_IP))
		proto = AF_INET;
	else if (skb->protocol == htons(ETH_P_IPV6))
		proto = AF_INET6;
	else
		return;

	afinfo = xfrm_state_get_afinfo(proto);
	if (afinfo) {
		afinfo->local_error(skb, mtu);
		rcu_read_unlock();
	}
}
EXPORT_SYMBOL_GPL(xfrm_local_error);
