// SPDX-License-Identifier: GPL-2.0
/*
 * xfrm4_state.c
 *
 * Changes:
 * 	YOSHIFUJI Hideaki @USAGI
 * 		Split up af-specific portion
 *
 */

#include <net/ip.h>
#include <net/xfrm.h>
#include <linux/pfkeyv2.h>
#include <linux/ipsec.h>
#include <linux/netfilter_ipv4.h>
#include <linux/export.h>

#include <linux/types.h>

#define SORT_MAX_CLASS  6
/*get tos value from ipv6 header */
static inline u8 v6_get_dsfield(const struct iphdr *iphv6)
{
	return ntohs(*(const __be16 *)iphv6) >> 4;
}

static int xfrm4_init_flags(struct xfrm_state *x)
{
	if (xs_net(x)->ipv4.sysctl_ip_no_pmtu_disc)
		x->props.flags |= XFRM_STATE_NOPMTUDISC;
	return 0;
}

static void
__xfrm4_init_tempsel(struct xfrm_selector *sel, const struct flowi *fl)
{
	const struct flowi4 *fl4 = &fl->u.ip4;

	sel->daddr.a4 = fl4->daddr;
	sel->saddr.a4 = fl4->saddr;
	sel->dport = xfrm_flowi_dport(fl, &fl4->uli);
	sel->dport_mask = htons(0xffff);
	sel->sport = xfrm_flowi_sport(fl, &fl4->uli);
	sel->sport_mask = htons(0xffff);
	sel->family = AF_INET;
	sel->prefixlen_d = 32;
	sel->prefixlen_s = 32;
	sel->proto = fl4->flowi4_proto;
	sel->ifindex = fl4->flowi4_oif;
}

static void
xfrm4_init_temprop(struct xfrm_state *x, const struct xfrm_tmpl *tmpl,
		   const xfrm_address_t *daddr, const xfrm_address_t *saddr)
{
	x->id = tmpl->id;
	if (x->id.daddr.a4 == 0)
		x->id.daddr.a4 = daddr->a4;
	x->props.saddr = tmpl->saddr;
	if (x->props.saddr.a4 == 0)
		x->props.saddr.a4 = saddr->a4;
	x->props.mode = tmpl->mode;
	x->props.reqid = tmpl->reqid;
	x->props.family = AF_INET;
}

int xfrm4_extract_header(struct sk_buff *skb)
{
	const struct iphdr *iph = ip_hdr(skb);

	XFRM_MODE_SKB_CB(skb)->ihl = sizeof(*iph);
	XFRM_MODE_SKB_CB(skb)->id = iph->id;
	XFRM_MODE_SKB_CB(skb)->frag_off = iph->frag_off;
	XFRM_MODE_SKB_CB(skb)->tos = iph->tos;
	XFRM_MODE_SKB_CB(skb)->ttl = iph->ttl;
	XFRM_MODE_SKB_CB(skb)->optlen = iph->ihl * 4 - sizeof(*iph);
	memset(XFRM_MODE_SKB_CB(skb)->flow_lbl, 0,
	       sizeof(XFRM_MODE_SKB_CB(skb)->flow_lbl));

	if (iph->version == 0x6) {
		/* maybe the inner is ipv6 and outer is ipv4,
		 * so the tos will be modified.
		 */
		XFRM_MODE_SKB_CB(skb)->tos = v6_get_dsfield(iph);
	}
	return 0;
}

/* distribution counting sort function for xfrm_state and xfrm_tmpl */
static int
__xfrm4_sort(void **dst, void **src, int n, int (*cmp)(void *p), int maxclass)
{
	int i;
	int class[XFRM_MAX_DEPTH];
	int count[SORT_MAX_CLASS];
	int size = sizeof(int) * SORT_MAX_CLASS;

	memset(count, 0, size);

	for (i = 0; i < n; i++) {
		int c;

		c = cmp(src[i]);
		class[i] = c;
		count[c]++;
	}

	for (i = 2; i < maxclass; i++)
		count[i] += count[i - 1];

	for (i = 0; i < n; i++) {
		dst[count[class[i] - 1]++] = src[i];
		src[i] = NULL;
	}

	return 0;
}

/* Rule for xfrm_tmpl:
 *
 * rule 1: select IPsec transport
 * rule 2: select MIPv6 RO or inbound trigger
 * rule 3: select IPsec tunnel
 * rule 4: others
 */
static int __xfrm4_tmpl_sort_cmp(void *p)
{
	struct xfrm_tmpl *v = p;

	switch (v->mode) {
	case XFRM_MODE_TRANSPORT:
		return 1;
	case XFRM_MODE_TUNNEL:
	case XFRM_MODE_BEET:
		return 3;
	}
	return 4;
}

static int
__xfrm4_tmpl_sort(struct xfrm_tmpl **dst, struct xfrm_tmpl **src, int n)
{
	return __xfrm4_sort((void **)dst, (void **)src, n,
			    __xfrm4_tmpl_sort_cmp, 5);
}

/* Rule for xfrm_state:
 *
 * rule 1: select IPsec transport except AH
 * rule 2: select MIPv6 RO or inbound trigger
 * rule 3: select IPsec transport AH
 * rule 4: select IPsec tunnel
 * rule 5: others
 */
static int __xfrm4_state_sort_cmp(void *p)
{
	struct xfrm_state *v = p;

	switch (v->props.mode) {
	case XFRM_MODE_TRANSPORT:
		if (v->id.proto != IPPROTO_AH)
			return 1;
		else
			return 3;
#if IS_ENABLED(CONFIG_IPV6_MIP6)
	case XFRM_MODE_ROUTEOPTIMIZATION:
	case XFRM_MODE_IN_TRIGGER:
		return 2;
#endif
	case XFRM_MODE_TUNNEL:
	case XFRM_MODE_BEET:
		return 4;
	}
	return 5;
}

static int
__xfrm4_state_sort(struct xfrm_state **dst, struct xfrm_state **src, int n)
{
	return __xfrm4_sort((void **)dst, (void **)src, n,
			    __xfrm4_state_sort_cmp, 6);
}

static struct xfrm_state_afinfo xfrm4_state_afinfo = {
	.family			= AF_INET,
	.proto			= IPPROTO_IPIP,
	.eth_proto		= htons(ETH_P_IP),
	.owner			= THIS_MODULE,
	.init_flags		= xfrm4_init_flags,
	.init_tempsel		= __xfrm4_init_tempsel,
	.init_temprop		= xfrm4_init_temprop,
	.tmpl_sort		= __xfrm4_tmpl_sort,
	.state_sort		= __xfrm4_state_sort,
	.output			= xfrm4_output,
	.output_finish		= xfrm4_output_finish,
	.extract_input		= xfrm4_extract_input,
	.extract_output		= xfrm4_extract_output,
	.transport_finish	= xfrm4_transport_finish,
	.local_error		= xfrm4_local_error,
};

void __init xfrm4_state_init(void)
{
	xfrm_state_register_afinfo(&xfrm4_state_afinfo);
}
