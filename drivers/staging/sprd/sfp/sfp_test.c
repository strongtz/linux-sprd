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
#include <linux/major.h>
#include <linux/ip.h>
#include <net/netfilter/nf_nat.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/timer.h>
#include <linux/skbuff.h>

#include "sfp.h"
#include "sfp_ipa.h"
static struct task_struct *snfp_test_task1;
static struct task_struct *snfp_test_task2;

struct hlist_head test_fwd_entries[SFP_ENTRIES_HASH_SIZE];
static u32 saddr = 0x2A89A8C0;
static u32 daddr = 0x98965DCC;
static u16 srcport = 0x6BFC;
static u16 dstport = 0x5000;
static u32 tgt_dstip = 0x0A0A0A01;
static u32 tgt_srcip = 0x0A0A0A0A;
static u16 tgt_srcport = 5000;
static u16 tgt_dstport = 50000;
int test_count;

#define IP_1 0x0A0A0A01
#define IP_2 0x0A0A0A02
#define IP_3 0x0A0A0A03

struct timer_list ipa_timer;
struct tuple_info {
	u32 ip1;
	u32 ip2;
	u32 ip3;
	u16 s_port;
	u16 d_port;
	u8 protonum;
};

static struct tuple_info g_tuples[] = {
	{IP_1, IP_2, IP_3, 10283, 12345, 17},
	{IP_1, IP_2, IP_3, 10363, 12345, 17},
	{IP_1, IP_2, IP_3, 10819, 12345, 17},
	{IP_1, IP_2, IP_3, 10828, 12345, 17},
	{IP_1, IP_2, IP_3, 11082, 12345, 17},
	{IP_1, IP_2, IP_3, 11645, 12345, 17},
	{IP_1, IP_2, IP_3, 11816, 12345, 17},
	{IP_1, IP_2, IP_3, 11887, 12345, 17},
};

struct sfp_test_fwd_entry {
	struct hlist_node entry_lst;
	struct nf_conntrack_tuple tuple;
	struct rcu_head	 rcu;
};

static void test_sfp_fwd_entry_free(struct rcu_head *head)
{
	struct sfp_test_fwd_entry *sfp_fwd_entry;

	sfp_fwd_entry = container_of(head, struct sfp_test_fwd_entry, rcu);
	kfree(sfp_fwd_entry);
}

static void start_insert_and_print_thread(void)
{
	struct sfp_test_fwd_entry *tuple_entry;
	struct sfp_test_fwd_entry *ta_tuple_entry;
	struct sfp_test_fwd_entry *test_entry;
	u32 hash;
	int i;
	int count_x, count_y;

	while (test_count) {
		tuple_entry = kmalloc(sizeof(*tuple_entry), GFP_ATOMIC);

		memset(tuple_entry, 0, sizeof(struct sfp_test_fwd_entry));
		tuple_entry->tuple.src.u3.ip = saddr;
		tuple_entry->tuple.src.l3num = AF_INET;
		tuple_entry->tuple.src.u.all = srcport;
		tuple_entry->tuple.dst.protonum = 0x06;
		tuple_entry->tuple.dst.u.all = dstport;
		tuple_entry->tuple.dst.u3.ip = daddr;

		hash = sfp_hash_conntrack(&tuple_entry->tuple);

		hlist_add_head_rcu(
			&tuple_entry->entry_lst,
			&test_fwd_entries[hash]);

		ta_tuple_entry = kmalloc(
			sizeof(*ta_tuple_entry), GFP_ATOMIC);

		if (!ta_tuple_entry)
			break;

		ta_tuple_entry->tuple.src.u3.ip = tgt_srcip;
		ta_tuple_entry->tuple.src.l3num = htons(0x0800);
		ta_tuple_entry->tuple.src.u.all = htons(tgt_srcport);
		ta_tuple_entry->tuple.dst.protonum = 0x06;
		ta_tuple_entry->tuple.dst.u.all = htons(tgt_dstport);
		ta_tuple_entry->tuple.dst.u3.ip = tgt_dstip;

		hash = sfp_hash_conntrack(&ta_tuple_entry->tuple);

		hlist_add_head_rcu(
			&ta_tuple_entry->entry_lst,
			&test_fwd_entries[hash]);
		msleep(20);
		test_count--;
		srcport++;
		tgt_dstport++;
	}
	msleep(20);
	count_x = 0;
	count_y = 0;
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		count_y = 0;
		hlist_for_each_entry_rcu(test_entry,
					 &test_fwd_entries[i],
					entry_lst) {
			msleep(20);
			hlist_del_rcu(&test_entry->entry_lst);
			call_rcu(&test_entry->rcu, test_sfp_fwd_entry_free);
		}
	}
}

static void start_check_thread(void)
{
	FP_PRT_DBG(FP_PRT_DEBUG,
		   "%s   start check test.\n", __func__);
}

static int snfp_test_thread_1(void *arg)
{
	start_insert_and_print_thread();
	return 0;
}

static int snfp_test_thread_2(void *arg)
{
	start_check_thread();
	return 0;
}

#ifdef CONFIG_SPRD_IPA_SUPPORT
static struct task_struct *snfp_test_task3;

static void make_ct_tuple(struct tuple_info *ti, struct nf_conn *ct)
{
	memset(ct, 0, sizeof(*ct));
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip = htonl(ti->ip1);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num = NFPROTO_IPV4;
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.all = htons(ti->s_port);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.protonum = ti->protonum;
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.all = htons(ti->d_port);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u3.ip = htonl(ti->ip3);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.dir = 0;

	ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip = htonl(ti->ip3);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.l3num = NFPROTO_IPV4;
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u.all = htons(ti->d_port);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.protonum = ti->protonum;
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.all = htons(ti->s_port);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.ip = htonl(ti->ip2);
	ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.dir = 1;
}

static void sfp_test_init_mac(struct sfp_conn *sfp_ct)
{
	u8 orig_src[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x11};
	u8 orig_dst[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x22};
	u8 reply_src[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x44};
	u8 reply_dst[ETH_ALEN] = {0x11, 0, 0, 0, 0, 0x33};
	struct sfp_mgr_fwd_tuple_hash *thptr;

	thptr = &sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.orig_mac_info.src_mac,
		orig_src);
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.orig_mac_info.dst_mac,
		orig_dst);
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.trans_mac_info.src_mac,
		reply_dst);
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.trans_mac_info.dst_mac,
		reply_src);

	thptr = &sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.orig_mac_info.src_mac,
		reply_src);
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.orig_mac_info.dst_mac,
		reply_dst);
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.trans_mac_info.src_mac,
		orig_dst);
	mac_addr_copy(
		thptr->ssfp_fwd_tuple.trans_mac_info.dst_mac,
		orig_src);
}

static void ipa_time_out_test(unsigned long ul_timer)
{
	struct fwd_entry *cur_entry;
	int i;
	int cnt = atomic_read(&fwd_tbl.entry_cnt);
	u8 *v_hash = sfp_get_hash_vtbl(sfp_tbl_id());

	FP_PRT_DBG(FP_PRT_DEBUG, "%s\n", __func__);

	cur_entry = (struct fwd_entry *)(v_hash + 2048 * 8);

	for (i = 0; i < cnt; i++) {
		cur_entry->time_stamp = htonl(jiffies + i);
		cur_entry++;
	}
}

static void setup_ipa_timer(void)
{
	FP_PRT_DBG(FP_PRT_DEBUG, "%s\n", __func__);
	setup_timer(
		&ipa_timer,
		ipa_time_out_test,
		(unsigned long)NULL);

	ipa_timer.expires = jiffies + 20 * HZ;
	add_timer(&ipa_timer);
}

static void sfp_mgr_hash_test_init(struct nf_conn *ct,
				   struct sfp_conn *new_sfp_ct)
{
	u32 in_ifindex = 2, out_ifindex = 5;
	u32 in_ipaifindex = 1, out_ipaifindex = 1;
	u32 hash, hash_inv;
	struct sfp_mgr_fwd_tuple_hash *tuple_hash;

	sfp_ct_init(ct, new_sfp_ct);

	sfp_test_init_mac(new_sfp_ct);

	tuple_hash =	&new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL];
	tuple_hash->ssfp_fwd_tuple.in_ifindex =	in_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = in_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = out_ipaifindex;

	tuple_hash =	&new_sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	tuple_hash->ssfp_fwd_tuple.in_ifindex = out_ifindex;
	tuple_hash->ssfp_fwd_tuple.out_ifindex = in_ifindex;
	tuple_hash->ssfp_fwd_tuple.in_ipaifindex = out_ipaifindex;
	tuple_hash->ssfp_fwd_tuple.out_ipaifindex = in_ipaifindex;

	rcu_read_lock_bh();
	hash = sfp_hash_conntrack(
		&new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	new_sfp_ct->hash[IP_CT_DIR_ORIGINAL] = hash;
	hlist_add_head_rcu(
		&new_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL].entry_lst,
		&mgr_fwd_entries[hash]);

	hash_inv = sfp_hash_conntrack(
		&new_sfp_ct->tuplehash[IP_CT_DIR_REPLY].tuple);
	new_sfp_ct->hash[IP_CT_DIR_REPLY] = hash_inv;
	hlist_add_head_rcu(
		&new_sfp_ct->tuplehash[IP_CT_DIR_REPLY].entry_lst,
		&mgr_fwd_entries[hash_inv]);

	FP_PRT_DBG(
		FP_PRT_WARN,
		"add mgr tbl, orig_hash[%u], reply_hash[%u]\n",
		hash, hash_inv);
	rcu_read_unlock_bh();
}

static void sfp_ipa_timer_test(void)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	g_tuples[0].s_port = 10000;
	setup_ipa_timer();
	for (i = 0; i < 4; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		sfp_ipa_hash_add(new_sfp_ct);
	}
}

static void sfp_ipa_timer_test_2(void)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	setup_ipa_timer();
	for (i = 0; i < sizeof(g_tuples) / sizeof(struct tuple_info); i++) {
		make_ct_tuple(&g_tuples[i], &ct);
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		sfp_ipa_hash_add(new_sfp_ct);

		if (i == 3)
			msleep(60 * 1000);
	}
}

static void sfp_ipa_tbl_add_delete(void)
{
	struct sfp_conn *new_sfp_ct, *old_sfp_ct;
	struct nf_conn ct;
	int i;

	for (i = 0; i < 5; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		/*stop timer in this case*/
		del_timer(&new_sfp_ct->timeout);

		sfp_ipa_hash_add(new_sfp_ct);

		if (i == 1)
			old_sfp_ct = new_sfp_ct;

		if (i == 3) {
			sfp_ipa_fwd_delete(
				&old_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL],
				old_sfp_ct->hash[IP_CT_DIR_ORIGINAL]);
			sfp_ipa_fwd_delete(
				&old_sfp_ct->tuplehash[IP_CT_DIR_REPLY],
				old_sfp_ct->hash[IP_CT_DIR_REPLY]);
		}
	}
	msleep(10000);
	sfp_ipa_fwd_clear();
	clear_sfp_mgr_table();
}

static void sfp_ipa_tbl_hash_collision(void)
{
	struct sfp_conn *new_sfp_ct, *old_sfp_ct;
	struct nf_conn ct;
	int i;

	for (i = 0; i < sizeof(g_tuples) / sizeof(struct tuple_info); i++) {
		make_ct_tuple(&g_tuples[i], &ct);
		FP_PRT_DBG(FP_PRT_ERR, "%d\n", i);

		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		/*stop timer in this case*/
		del_timer(&new_sfp_ct->timeout);

		sfp_ipa_hash_add(new_sfp_ct);

		msleep(20);
		if (i == 3)
			old_sfp_ct = new_sfp_ct;

		if (i == 5) {
			FP_PRT_DBG(FP_PRT_ERR, "delete\n");
			sfp_ipa_fwd_delete(
				&old_sfp_ct->tuplehash[IP_CT_DIR_ORIGINAL],
				old_sfp_ct->hash[IP_CT_DIR_ORIGINAL]);
			sfp_ipa_fwd_delete(
				&old_sfp_ct->tuplehash[IP_CT_DIR_REPLY],
				old_sfp_ct->hash[IP_CT_DIR_REPLY]);
		}
	}

	msleep(10000);
	sfp_ipa_fwd_clear();
	clear_sfp_mgr_table();
}

static void sfp_ipa_tbl_add_2048(int n)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	FP_PRT_DBG(FP_PRT_ERR, "test %d\n", n);
	g_tuples[0].s_port = 10000;
	for (i = 0; i < n; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_ERR, "test new sfp %p, %d\n", new_sfp_ct, i);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		/*stop timer in this case*/
		del_timer(&new_sfp_ct->timeout);

		sfp_ipa_hash_add(new_sfp_ct);
		msleep(20);
	}
	msleep(10000);
	sfp_ipa_fwd_clear();
	clear_sfp_mgr_table();
}

static void sfp_ipa_tbl_add_2048_timer(void)
{
	struct sfp_conn *new_sfp_ct;
	struct nf_conn ct;
	int i;

	setup_ipa_timer();
	g_tuples[0].s_port = 10000;
	for (i = 0; i < SFP_ENTRIES_HASH_SIZE; i++) {
		make_ct_tuple(&g_tuples[0], &ct);
		g_tuples[0].s_port++;
		new_sfp_ct = kmalloc(sizeof(*new_sfp_ct), GFP_ATOMIC);
		if (!new_sfp_ct)
			return;

		FP_PRT_DBG(FP_PRT_WARN, "test new sfp %p\n", new_sfp_ct);
		memset(new_sfp_ct, 0, sizeof(struct sfp_conn));

		sfp_mgr_hash_test_init(&ct, new_sfp_ct);

		sfp_ipa_hash_add(new_sfp_ct);
		msleep(20);
	}
	msleep(100 * 1000);

	sfp_ipa_swap_tbl();
}

int snfp_test_thread_3(void *arg)
{
	sfp_ipa_tbl_add_delete();
	return 0;
}

int snfp_test_thread_4(void *arg)
{
	sfp_ipa_timer_test();
	return 0;
}

int snfp_test_thread_5(void *arg)
{
	sfp_ipa_tbl_add_2048(2048);

	return 0;
}

int snfp_test_thread_6(void *arg)
{
	sfp_ipa_tbl_hash_collision();
	return 0;
}

int snfp_test_thread_7(void *arg)
{
	sfp_ipa_timer_test_2();
	return 0;
}

int snfp_test_thread_8(void *arg)
{
	sfp_ipa_tbl_add_2048_timer();
	return 0;
}

int snfp_test_thread_n(void *arg)
{
	int n = *((int *)arg);

	sfp_ipa_tbl_add_2048(n);
	return 0;
}
#endif

int sfp_test_init(int count)
{
	test_count = count;
	if (count == 1) {
		snfp_test_task1 = kthread_run(
			snfp_test_thread_1,
			NULL, "snfp_test_1");
		if (IS_ERR(snfp_test_task1))
			return 1;

		snfp_test_task2 = kthread_run(
			snfp_test_thread_2,
			NULL, "snfp_test_2");
		if (IS_ERR(snfp_test_task2))
			return 1;
	}

#ifdef CONFIG_SPRD_IPA_SUPPORT
	if (count == 10) {
		snfp_test_task3 = kthread_run(
			snfp_test_thread_3,
			NULL, "ipa_hash_init_delete");
		if (IS_ERR(snfp_test_task3))
			return 1;
	}

	if (count == 11) {
		snfp_test_task3 = kthread_run(
			snfp_test_thread_4,
			NULL, "ipa_hash_timer");
		if (IS_ERR(snfp_test_task3))
			return 1;
	}

	if (count == 12) {
		snfp_test_task3 = kthread_run(
			snfp_test_thread_5,
			NULL, "ipa_hash_tbl_2048");
		if (IS_ERR(snfp_test_task3))
			return 1;
	}

	if (count == 13) {
		snfp_test_task3 = kthread_run(
			snfp_test_thread_6,
			NULL, "sfp_ipa_tbl_hash_collision");
		if (IS_ERR(snfp_test_task3))
			return 1;
	}

	if (count == 14) {
		snfp_test_task3 = kthread_run(
			snfp_test_thread_7,
			NULL, "sfp_ipa_timer_test_2");
		if (IS_ERR(snfp_test_task3))
			return 1;
	}

	if (count == 15) {
		snfp_test_task3 = kthread_run(
			snfp_test_thread_8,
			NULL, "sfp_ipa_tbl_add_2048_timer");
		if (IS_ERR(snfp_test_task3))
			return 1;
	}

	if (count > 15) {
		snfp_test_task3 = kthread_run(
			snfp_test_thread_n,
			&count, "ipa_hash_tbl_2048");
		if (IS_ERR(snfp_test_task3))
			return 1;
	}
#endif
	return 0;
}
