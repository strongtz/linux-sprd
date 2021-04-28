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
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <net/route.h>

#include "sfp.h"

static unsigned int proc_nfp_perms = 0666;

static struct proc_dir_entry *procdir;
static struct proc_dir_entry *sfp_proc_mgr_fwd;
static struct proc_dir_entry *sfp_proc_debug;
static struct proc_dir_entry *sfp_proc_fwd;
static struct proc_dir_entry *sfp_proc_enable;

#ifdef CONFIG_SPRD_SFP_TEST
static struct proc_dir_entry *sfp_test;
#endif

unsigned int fp_dbg_lvl = FP_PRT_ALL;

int get_sfp_enable(void)
{
	return sysctl_net_sfp_enable;
}
EXPORT_SYMBOL(get_sfp_enable);

void sfp_mgr_disable(void)
{
	if (sysctl_net_sfp_enable == 1)
		sysctl_net_sfp_enable = 0;
	FP_PRT_DBG(FP_PRT_DEBUG, "Network Fast Processing Disabled\n");
}
EXPORT_SYMBOL(sfp_mgr_disable);

void procdebugprint_ipv4addr(struct seq_file *seq, u32 ipaddr)
{
	seq_printf(seq, "%d.%d.%d.%d", ((ipaddr >> 24) & 0xFF),
		   ((ipaddr >> 16) & 0xFF), ((ipaddr >> 8) & 0xFF),
		   ((ipaddr >> 0) & 0xFF));
}

void procdebugprint_ipv6addr(struct seq_file *seq, u32 *ip6addr)
{
	seq_printf(seq, "0x%08x:0x%08x:0x%08x:0x%08x",
		   ip6addr[0], ip6addr[1],
		   ip6addr[2], ip6addr[3]);
}

void procdebugprint_mgr_fwd_info(struct seq_file *seq,
				 struct sfp_mgr_fwd_tuple *tuple)
{
	seq_puts(seq, "Original INFO:\n");
	seq_puts(seq, "\tMAC INFO:\t");
	seq_printf(seq, "dst:%02x.%02x.%02x.%02x.%02x.%02x\t",
		   tuple->orig_mac_info.dst_mac[0],
		   tuple->orig_mac_info.dst_mac[1],
		   tuple->orig_mac_info.dst_mac[2],
		   tuple->orig_mac_info.dst_mac[3],
		   tuple->orig_mac_info.dst_mac[4],
		   tuple->orig_mac_info.dst_mac[5]);
	seq_printf(seq, "src:%02x.%02x.%02x.%02x.%02x.%02x\n",
		   tuple->orig_mac_info.src_mac[0],
		   tuple->orig_mac_info.src_mac[1],
		   tuple->orig_mac_info.src_mac[2],
		   tuple->orig_mac_info.src_mac[3],
		   tuple->orig_mac_info.src_mac[4],
		   tuple->orig_mac_info.src_mac[5]);
	seq_puts(seq, "\tIP INFO:\t");
	if (tuple->orig_info.l3_proto == NFPROTO_IPV4) {
		seq_puts(seq, "dst: ");
		procdebugprint_ipv4addr(seq, ntohl(tuple->orig_info.dst_ip.ip));
		seq_puts(seq, "\tsrc: ");
		procdebugprint_ipv4addr(seq, ntohl(tuple->orig_info.src_ip.ip));
		seq_puts(seq, "\n");
	} else {
		seq_puts(seq, "dst: ");
		procdebugprint_ipv6addr(seq, tuple->orig_info.dst_ip.all);
		seq_puts(seq, "\tsrc: ");
		procdebugprint_ipv6addr(seq, tuple->orig_info.src_ip.all);
		seq_puts(seq, "\n");
	}
	seq_puts(seq, "\tL4 INFO:\t");
	seq_printf(seq, "proto:%u\t", tuple->orig_info.l4_proto);
	seq_printf(seq, "dst:%u\t", ntohs(tuple->orig_info.dst_l4_info.all));
	seq_printf(seq, "src:%u\n", ntohs(tuple->orig_info.src_l4_info.all));
	seq_puts(seq, "Transfer Info:\n");
	seq_puts(seq, "\tMAC INFO:\t");
	seq_printf(seq, "dst:%02x.%02x.%02x.%02x.%02x.%02x\t",
		   tuple->trans_mac_info.dst_mac[0],
		   tuple->trans_mac_info.dst_mac[1],
		   tuple->trans_mac_info.dst_mac[2],
		   tuple->trans_mac_info.dst_mac[3],
		   tuple->trans_mac_info.dst_mac[4],
		   tuple->trans_mac_info.dst_mac[5]);
	seq_printf(seq, "src:%02x.%02x.%02x.%02x.%02x.%02x\n",
		   tuple->trans_mac_info.src_mac[0],
		   tuple->trans_mac_info.src_mac[1],
		   tuple->trans_mac_info.src_mac[2],
		   tuple->trans_mac_info.src_mac[3],
		   tuple->trans_mac_info.src_mac[4],
		   tuple->trans_mac_info.src_mac[5]);
	seq_puts(seq, "\tIP Info:\t");
	if (tuple->trans_info.l3_proto == NFPROTO_IPV4) {
		seq_puts(seq, "dst:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.dst_ip.ip));
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.src_ip.ip));
		seq_puts(seq, "\n");
	} else {
		seq_puts(seq, "dst:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.dst_ip.all);
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.src_ip.all);
		seq_puts(seq, "\n");
	}
	seq_puts(seq, "\tL4 Info:\t");
	seq_printf(seq, "proto:%u\t", tuple->trans_info.l4_proto);
	seq_printf(seq, "dst:%u\t", ntohs(tuple->trans_info.dst_l4_info.all));
	seq_printf(seq, "src:%u\t", ntohs(tuple->trans_info.src_l4_info.all));
	seq_printf(seq, "COUNT=%u\t", tuple->count);
	seq_printf(seq, "FLAGS %d\t", tuple->fwd_flags);
	seq_printf(seq, "(IN-OUT)-(%u-%u)\n", tuple->in_ifindex,
		   tuple->out_ifindex);
}

void procdebugprint_fwd_info_2(struct seq_file *seq,
			       struct sfp_trans_tuple *tuple)
{
	seq_puts(seq, "Transfer Info:\n");
	seq_puts(seq, "\tMAC INFO:\t");
	seq_printf(seq, "dst:%02x.%02x.%02x.%02x.%02x.%02x\t",
		   tuple->trans_mac_info.dst_mac[0],
		   tuple->trans_mac_info.dst_mac[1],
		   tuple->trans_mac_info.dst_mac[2],
		   tuple->trans_mac_info.dst_mac[3],
		   tuple->trans_mac_info.dst_mac[4],
		   tuple->trans_mac_info.dst_mac[5]);
	seq_printf(seq, "src:%02x.%02x.%02x.%02x.%02x.%02x\n",
		   tuple->trans_mac_info.src_mac[0],
		   tuple->trans_mac_info.src_mac[1],
		   tuple->trans_mac_info.src_mac[2],
		   tuple->trans_mac_info.src_mac[3],
		   tuple->trans_mac_info.src_mac[4],
		   tuple->trans_mac_info.src_mac[5]);
	seq_puts(seq, "\tIP Info:\t");
	if (tuple->trans_info.l3_proto == NFPROTO_IPV4) {
		seq_puts(seq, "dst:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.dst_ip.ip));
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.src_ip.ip));
		seq_puts(seq, "\n");
	} else {
		seq_puts(seq, "dst:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.dst_ip.all);
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.src_ip.all);
		seq_puts(seq, "\n");
	}
	seq_puts(seq, "\tL4 Info:\t");
	seq_printf(seq, "proto:%u\t", tuple->trans_info.l4_proto);
	seq_printf(seq, "dst:%u\t", ntohs(tuple->trans_info.dst_l4_info.all));
	seq_printf(seq, "src:%u\t", ntohs(tuple->trans_info.src_l4_info.all));
	seq_printf(seq, "COUNT=%u\t", tuple->count);
	seq_printf(seq, "FLAGS: %d\t", tuple->fwd_flags);
	seq_printf(seq, "(IN-OUT)-(%u-%u)\n", tuple->in_ifindex,
		   tuple->out_ifindex);
}

void procdebugprint_macaddr(struct seq_file *seq, const u8 *macaddr)
{
	int i;

	seq_printf(seq, "%02x", (unsigned int)macaddr[0]);
	for (i = 1; i < MAC_ADDR_SIZE; i++)
		seq_printf(seq, ":%02x", macaddr[i]);
}

void procdebugprint_fwd_info(struct seq_file *seq, struct sfp_fwd_entry *tuple)
{
	struct sfp_trans_tuple *sfp_tuple;

	sfp_tuple = &tuple->ssfp_trans_tuple;
	procdebugprint_fwd_info_2(seq, sfp_tuple);
}

/********sfp mgr fwd show********************************/
static int sfp_fwd_proc_show(struct seq_file *seq, void *v)
{
	struct hlist_node *n;
	struct sfp_fwd_entry *curr_entry;
	struct sfpfwd_iter_state *st = seq->private;

	n = (struct hlist_node *)v;
	seq_printf(seq, "SFP Forward Entry index[%d]-hash[%u]:\n",
		   st->count++, st->bucket);
	curr_entry = hlist_entry(n, struct sfp_fwd_entry, entry_lst);
	procdebugprint_fwd_info(seq, curr_entry);

	return 0;
}

static void *sfp_fwd_get_next(struct seq_file *s, void *v)
{
	struct sfp_mgr_fwd_iter_state *st = s->private;
	struct hlist_node *n;

	n = (struct hlist_node *)v;
	n = hlist_next_rcu(n);
	if (!n) {
		while (!n && (++st->bucket) < SFP_ENTRIES_HASH_SIZE) {
			n = rcu_dereference(hlist_first_rcu(
				    &sfp_fwd_entries[st->bucket]));
			if (n)
				return n;
		}
	}
	return n;
}

static void *sfp_fwd_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return sfp_fwd_get_next(s, v);
}

static void *sfp_fwd_get_first(struct seq_file *seq, loff_t *pos)
{
	struct sfp_mgr_fwd_iter_state *st = seq->private;
	struct hlist_node *n;

	if (st->bucket == SFP_ENTRIES_HASH_SIZE)
		return NULL;

	for (; st->bucket < SFP_ENTRIES_HASH_SIZE; st->bucket++) {
		n = rcu_dereference(
			hlist_first_rcu(&sfp_fwd_entries[st->bucket]));
		if (n)
			return n;
	}
	return NULL;
}

static void *sfp_fwd_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return sfp_fwd_get_first(seq, pos);
}

static void sfp_fwd_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static const struct seq_operations sfp_fwd_ops = {
	.start = sfp_fwd_seq_start,
	.next  = sfp_fwd_seq_next,
	.stop  = sfp_fwd_seq_stop,
	.show  = sfp_fwd_proc_show
};

static int sfp_fwd_opt_proc_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file,
				&sfp_fwd_ops,
				sizeof(struct sfpfwd_iter_state));
}

static const struct file_operations proc_sfp_file_fwd_ops = {
	.owner = THIS_MODULE,
	.open = sfp_fwd_opt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

static ssize_t sysctl_net_sfp_enable_proc_write(struct file *file,
						const char __user *buffer,
						size_t count,
						loff_t *pos)
{
	char mode;
	int status = 0;

	if (count > 0) {
		if (get_user(mode, buffer))
			return -EFAULT;

		status = (mode != '0');
		if (status == 1 && sysctl_net_sfp_enable == 0) {
			sfp_mgr_proc_enable();
			sysctl_net_sfp_enable = 1;
		} else if (status == 0 && sysctl_net_sfp_enable == 1) {
			sysctl_net_sfp_enable = 0;
			sfp_mgr_proc_disable();
		}
	}
	return count;
}

static int sysctl_net_sfp_enable_proc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, sysctl_net_sfp_enable ? "1\n" : "0\n");
	return 0;
}

static int sysctl_net_sfp_enable_proc_open(struct inode *inode,
					   struct file *file)
{
	return single_open(file, sysctl_net_sfp_enable_proc_show, NULL);
}

static const struct file_operations proc_sfp_file_switch_ops = {
	.open  = sysctl_net_sfp_enable_proc_open,
	.read  = seq_read,
	.write  = sysctl_net_sfp_enable_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int sfp_debug_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "now debug level=0x%02x\n", fp_dbg_lvl);
	return 0;
}

static ssize_t sfp_debug_proc_write(struct file *file,
				    const char __user *buffer,
				    size_t count,
				    loff_t *pos)
{
	unsigned int level2;
	int ret;

	ret = kstrtouint_from_user(buffer, count, 16, &level2);
	if (ret < 0)
		return -EFAULT;
	else if (level2 <= 0xFF)
		fp_dbg_lvl = level2;
	return count;
}

static int sfp_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sfp_debug_proc_show, NULL);
}

static const struct file_operations proc_sfp_file_debug_ops = {
	.open  = sfp_debug_proc_open,
	.read  = seq_read,
	.write  = sfp_debug_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_SPRD_SFP_TEST
static int sfp_test_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "do test=0x%02x\n", test_count);
	return 0;
}

static ssize_t sfp_test_proc_write(struct file *file,
				   const char __user *buffer,
				   size_t count,
				   loff_t *pos)
{
	int level;
	int ret;

	if (count > 0) {
		ret = kstrtouint_from_user(buffer, count, 10, &level);
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "test_proc = %d, ret %d\n", level, ret);
		if (ret < 0)
			return -EFAULT;
		if (level > 0)
			sfp_test_init(level);
	}
	return count;
}

static int sfp_test_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sfp_test_proc_show, NULL);
}

static const struct file_operations proc_sfp_file_test_ops = {
	.open  = sfp_test_proc_open,
	.read  = seq_read,
	.write  = sfp_test_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};
#endif

/********Mgr_fp fwd show********************************/
static int sfp_mgr_fwd_proc_show(struct seq_file *seq, void *v)
{
	struct hlist_node *n;
	struct sfp_mgr_fwd_tuple_hash *entry_info1, *entry_info2;
	struct sfpfwd_iter_state *st = seq->private;
	struct sfp_conn *sfp_ct;

	n = (struct hlist_node *)v;
	entry_info1 = hlist_entry(n, struct sfp_mgr_fwd_tuple_hash, entry_lst);

	if (entry_info1->tuple.dst.dir != IP_CT_DIR_ORIGINAL)
		return 0;

	sfp_ct = sfp_ct_tuplehash_to_ctrack(entry_info1);
	seq_printf(seq, "SFP MGR Forward Entry index[%d]-hash[%u]:\n",
		   st->count++, st->bucket);
	procdebugprint_mgr_fwd_info(seq, &entry_info1->ssfp_fwd_tuple);
	entry_info2 = &sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	procdebugprint_mgr_fwd_info(seq, &entry_info2->ssfp_fwd_tuple);
	seq_printf(seq, "\t time=%ld",
		   (sfp_ct->timeout.expires - jiffies) / HZ);
	seq_puts(seq, "\n");
	return 0;
}

static void *sfp_mgr_fwd_get_next(struct seq_file *s, void *v)
{
	struct sfpfwd_iter_state *st = s->private;
	struct hlist_node *n;

	n = (struct hlist_node *)v;
	n = hlist_next_rcu(n);
	if (!n) {
		while (!n && (++st->bucket) < SFP_ENTRIES_HASH_SIZE) {
			n = rcu_dereference(hlist_first_rcu(
				    &mgr_fwd_entries[st->bucket]));
			if (n)
				return n;
		}
	}
	return n;
}

static void *sfp_mgr_fwd_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return sfp_mgr_fwd_get_next(s, v);
}

static void *sfp_mgr_fwd_get_first(struct seq_file *seq, loff_t *pos)
{
	struct sfpfwd_iter_state *st = seq->private;
	struct hlist_node *n;

	if (st->bucket == SFP_ENTRIES_HASH_SIZE)
		return NULL;

	for (; st->bucket < SFP_ENTRIES_HASH_SIZE; st->bucket++) {
		n = rcu_dereference(
			hlist_first_rcu(&mgr_fwd_entries[st->bucket]));
		if (n)
			return n;
	}
	return NULL;
}

static void *sfp_mgr_fwd_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return sfp_mgr_fwd_get_first(seq, pos);
}

static void sfp_mgr_fwd_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static const struct seq_operations sfp_mgr_fwd_ops = {
	.start = sfp_mgr_fwd_seq_start,
	.next  = sfp_mgr_fwd_seq_next,
	.stop  = sfp_mgr_fwd_seq_stop,
	.show  = sfp_mgr_fwd_proc_show
};

static int sfp_mgr_fwd_opt_proc_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file,
				&sfp_mgr_fwd_ops,
				sizeof(struct sfpfwd_iter_state));
}

static const struct file_operations proc_mgr_sfp_file_fwd_ops = {
	.owner = THIS_MODULE,
	.open = sfp_mgr_fwd_opt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

int sfp_proc_create(void)
{
#ifdef CONFIG_PROC_FS
	int ret;

	procdir = proc_mkdir("sfp", init_net.proc_net);
	if (!procdir) {
		pr_err("failed to create proc/.../sfp\n");
		ret = -ENOMEM;
		goto no_dir;
	}
	sfp_proc_mgr_fwd = proc_create_data("mgr_fwd_entries", proc_nfp_perms,
					    procdir,
					    &proc_mgr_sfp_file_fwd_ops,
					    NULL);
	if (!sfp_proc_mgr_fwd) {
		pr_err("nfp: failed to create sfp/nat file\n");
		ret = -ENOMEM;
		goto no_mgr_fwd_entry;
	}
	sfp_proc_fwd = proc_create_data("sfp_fwd_entries", proc_nfp_perms,
					procdir,
					&proc_sfp_file_fwd_ops,
					NULL);
	if (!sfp_proc_fwd) {
		pr_err("nfp: failed to create sfp/fwd_entries file\n");
		ret = -ENOMEM;
		goto no_fwd_entry;
	}
	sfp_proc_enable = proc_create_data("enable", proc_nfp_perms,
					   procdir,
					   &proc_sfp_file_switch_ops,
					   NULL);
	if (!sfp_proc_enable) {
		pr_err("nfp: failed to create sfp/enable file\n");
		ret = -ENOMEM;
		goto no_enable_entry;
	}
	sfp_proc_debug = proc_create_data("debug", proc_nfp_perms,
					  procdir,
					  &proc_sfp_file_debug_ops,
					  NULL);
	if (!sfp_proc_debug) {
		pr_err("nfp: failed to create sfp/debug file\n");
		ret = -ENOMEM;
		goto no_debug_entry;
	}
#ifdef CONFIG_SPRD_SFP_TEST
	sfp_test = proc_create_data("test", proc_nfp_perms,
				    procdir,
				    &proc_sfp_file_test_ops,
				    NULL);

	if (!sfp_test) {
		pr_err("nfp: failed to create sfp/test file\n");
		ret = -ENOMEM;
		goto no_test_entry;
	}
#endif
	return 0;
#ifdef CONFIG_SPRD_SFP_TEST
no_test_entry:
	remove_proc_entry("debug", procdir);
#endif
no_enable_entry:
	remove_proc_entry("enable", procdir);
no_debug_entry:
	remove_proc_entry("sfp_fwd_entries", procdir);
no_fwd_entry:
	remove_proc_entry("mgr_fwd_entries", procdir);
no_mgr_fwd_entry:
	remove_proc_entry("sfp", NULL);
no_dir:
	return ret;
#endif
}
EXPORT_SYMBOL(sfp_proc_create);

int nfp_proc_exit(void)
{
	remove_proc_entry("test", procdir);
	remove_proc_entry("debug", procdir);
	remove_proc_entry("enable", procdir);
	remove_proc_entry("sfp_fwd_entries", procdir);
	remove_proc_entry("mgr_fwd_entries", procdir);
	remove_proc_entry("sfp", NULL);
	return 0;
}
