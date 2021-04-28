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

#include <linux/cpu_pm.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/resource.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/jiffies.h>
#include <linux/sipc.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include "sprd_mailbox_drv.h"

static struct mbox_dts_cfg_tag mbox_dts_cfg;
struct mbox_device_tag *mbox_ops;
static u8 g_mbox_inited;

static spinlock_t mbox_lock;

#define MBOX_SUPPORT_MAX_CORE_CNT 16

static u64 g_send_fifo[MBOX_SUPPORT_MAX_CORE_CNT][SEND_FIFO_LEN];
static u16 g_inptr[MBOX_SUPPORT_MAX_CORE_CNT];
static u16 g_outptr[MBOX_SUPPORT_MAX_CORE_CNT];
static u8 g_inbox_send;

static struct task_struct *g_send_thread;
static wait_queue_head_t g_send_wait;

static void mbox_put1msg(u8 dst, u64 msg)
{
	u16 rd, wt, pos;

	rd = g_outptr[dst];
	wt = g_inptr[dst];

	pos = wt % SEND_FIFO_LEN;
	/* if fifo full, return */
	if ((rd != wt) && (rd % SEND_FIFO_LEN == pos)) {
		pr_err("mbox:dst = %d, rd =%d, wt = %d\n", dst, rd, wt);
		return;
	}

	pr_debug("mbox:dst = %d, rd =%d, wt = %d, pos = %d\n",
		 dst, rd, wt, pos);

	g_send_fifo[dst][pos] = msg;
	g_inptr[dst] = wt + 1;
}

static int mbox_send_thread(void *pdata)
{
	unsigned long flag;
	u8 dst;
	u16 rd, pos;
	u64 msg;

	pr_debug("mbox:%s!\n", __func__);

	while (!kthread_should_stop()) {
		wait_event(g_send_wait, g_inbox_send);

		pr_debug("mbox:%s, wait event!\n", __func__);

		/* Event triggered, process all FIFOs. */
		spin_lock_irqsave(&mbox_lock, flag);

		for (dst = 0; dst < mbox_ops->max_cnt; dst++) {
			if (!(g_inbox_send & (1 << dst)))
				continue;

			rd = g_outptr[dst];
			while (g_inptr[dst] != rd) {
				pos = rd % SEND_FIFO_LEN;
				msg = g_send_fifo[dst][pos];

				if (mbox_ops->fops->phy_send(dst, msg) != 0)
					/* one send failed, than next */
					break;

				rd += 1;
				g_outptr[dst] = rd;
			}
		}

		g_inbox_send = 0;
		spin_unlock_irqrestore(&mbox_lock, flag);
	}

	return 0;
}

u8 mbox_get_send_fifo_mask(u8 send_bit)
{
	u8 dst_bit, dst, mask;

	spin_lock(&mbox_lock);

	mask = 0;
	for (dst = 0; dst < mbox_ops->max_cnt; dst++) {
		dst_bit = (1 << dst);
		if (send_bit & dst_bit) {
			if (g_outptr[dst] != g_inptr[dst])
				mask |= (dst_bit);
		}
	}

	spin_unlock(&mbox_lock);

	return mask;
}

void mbox_start_fifo_send(u8 send_mask)
{
	pr_debug("mbox:%s!\n", __func__);
	g_inbox_send = send_mask;
	wake_up(&g_send_wait);
}

int mbox_register_irq_handle(u8 target_id,
			     MBOX_FUNCALL irq_handler,
			     void *priv_data)
{
	unsigned long flags;
	int ret;

	pr_debug("mbox:%s,target_id =%d\n", __func__, target_id);

	if (!g_mbox_inited) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}

	spin_lock_irqsave(&mbox_lock, flags);

	ret = mbox_ops->fops->phy_register_irq_handle(target_id, irq_handler, priv_data);

	spin_unlock_irqrestore(&mbox_lock, flags);

	if (ret < 0)
		return ret;

	/* must do it, Ap may be have already revieved some msgs */
	mbox_ops->fops->process_bak_msg();

	return ret;
}
EXPORT_SYMBOL_GPL(mbox_register_irq_handle);

int mbox_unregister_irq_handle(u8 target_id)
{
	int ret;

	if (!g_mbox_inited) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}

	spin_lock(&mbox_lock);

	ret = mbox_ops->fops->phy_unregister_irq_handle(target_id);

	spin_unlock(&mbox_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mbox_unregister_irq_handle);

u32 mbox_core_fifo_full(int core_id)
{
	u32 ret;

	if (!g_mbox_inited) {
		pr_err("mbox:ERR on line %d! mbox not init\n", __LINE__);
		return 1;
	}

	ret = mbox_ops->fops->phy_core_fifo_full(core_id);

	return ret;
}
EXPORT_SYMBOL_GPL(mbox_core_fifo_full);

int mbox_raw_sent(u8 core_id, u64 msg)
{
	unsigned long flag;

	if (!g_mbox_inited) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}

	if (core_id >= mbox_ops->max_cnt) {
		pr_err("mbox:ERR core_id = %d!\n", core_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&mbox_lock, flag);

	/* if fifo is not empty, put it into the fifo,
	 * else if send failed , also put it into the fifo
	 */
	if (g_inptr[core_id] != g_outptr[core_id])
		mbox_put1msg(core_id, msg);
	else if (mbox_ops->fops->phy_send(core_id, msg) != 0)
		mbox_put1msg(core_id, msg);

	spin_unlock_irqrestore(&mbox_lock, flag);

	return 0;
}
EXPORT_SYMBOL_GPL(mbox_raw_sent);

void mbox_just_sent(u8 core_id, u64 msg)
{
	mbox_ops->fops->phy_just_sent(core_id, msg);
}
EXPORT_SYMBOL_GPL(mbox_just_sent);

static int mbox_parse_dts(void)
{
	int ret;
	struct device_node *np;
	u32 syscon_args[2];

	np = of_find_compatible_node(NULL, NULL, "sprd,mailbox");
	if (!np) {
		pr_err("mbox: can't find %s!\n", "sprd,mailbox");
		return(-EINVAL);
	}

	mbox_dts_cfg.gpr = syscon_regmap_lookup_by_name(np, "clk");
	if (IS_ERR(mbox_dts_cfg.gpr)) {
		pr_err("mbox: failed to map mailbox_gpr\n");
		return PTR_ERR(mbox_dts_cfg.gpr);
	}

	ret = syscon_get_args_by_name(np, "clk", 2, (u32 *)syscon_args);
	if (ret == 2) {
		mbox_dts_cfg.enable_reg = syscon_args[0];
		mbox_dts_cfg.mask_bit = syscon_args[1];
		pr_debug("mbox:%s!reg=0x%x,mask=0x%x\n", __func__, mbox_dts_cfg.enable_reg, mbox_dts_cfg.mask_bit);
	} else {
		pr_err("mbox: failed to map mailbox reg\n");
		return(-EINVAL);
	}

	/* parse inbox base */
	of_address_to_resource(np, 0, &mbox_dts_cfg.inboxres);

	/* parse out base */
	of_address_to_resource(np, 1, &mbox_dts_cfg.outboxres);

	/* parse inbox irq */
	ret = of_irq_get(np, 0);
	if (ret < 0) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}
	mbox_dts_cfg.inbox_irq = ret;

	/* parse outbox irq */
	ret = of_irq_get(np, 1);
	if (ret < 0) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}
	mbox_dts_cfg.outbox_irq = ret;

	/* parse outbox sensor irq, in dt config */
	ret = of_irq_get(np, 2);
	if (ret < 0) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
	} else {
		mbox_dts_cfg.outbox_sensor_irq = ret;

		/* parse sensor core */
		ret = of_property_read_u32(np, "sprd,sensor", &mbox_dts_cfg.sensor_core);
		if (ret) {
			pr_debug("mbox:no sprd,sensor!\n");
			mbox_dts_cfg.sensor_core = MBOX_INVALID_CORE;
		}
	}

	/* parse core cnt */
	ret = of_property_read_u32(np, "sprd,core-cnt", &mbox_dts_cfg.core_cnt);
	if (ret) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}

	/* parse mbox version */
	ret = of_property_read_u32(np, "sprd,version", &mbox_dts_cfg.version);
	if (ret) {
		pr_debug("mbox: not found version!\n");
		mbox_dts_cfg.version = 2;
	}

	return 0;
}

static int mbox_pm_notifier(struct notifier_block *self,
			    unsigned long cmd, void *v)
{
	if (cmd == CPU_CLUSTER_PM_EXIT && mbox_ops->fops->outbox_has_irq())
		sipc_set_wakeup_flag();

	return NOTIFY_OK;
}

static struct notifier_block mbox_pm_notifier_block = {
	.notifier_call = mbox_pm_notifier,
};

static int __init mbox_init(void)
{
	int ret;

	pr_info("mbox:%s!\n", __func__);

	ret = mbox_parse_dts();
	if (ret != 0)
		return -EINVAL;

	mbox_get_phy_device(&mbox_ops);

	spin_lock_init(&mbox_lock);

	ret = mbox_ops->fops->cfg_init(&mbox_dts_cfg, &g_mbox_inited);
	if (ret != 0) {
		return -EINVAL;
	}

	ret = cpu_pm_register_notifier(&mbox_pm_notifier_block);
	if (ret)
		return ret;

	init_waitqueue_head(&g_send_wait);
	g_send_thread = kthread_create(mbox_send_thread,
				       NULL,
				       "mbox-send-thread");
	wake_up_process(g_send_thread);

	return 0;
}
subsys_initcall(mbox_init);

#if defined(CONFIG_DEBUG_FS)
void mbox_check_all_send_fifo(struct seq_file *m)
{
	u8 dst;
	u16 rd, wt, len;
	unsigned long flag;

	spin_lock_irqsave(&mbox_lock, flag);
	for (dst = 0; dst < mbox_ops->max_cnt; dst++) {
		rd = g_outptr[dst];
		wt = g_inptr[dst];

		if (rd <= wt)
			len = wt - rd;
		else
			len = (wt % SEND_FIFO_LEN)
				+ (SEND_FIFO_LEN - rd % SEND_FIFO_LEN);

		if (len > 0)
			seq_printf(m,
				   "	inbox fifo %d len is %d!\n",
				   dst, len);
	}
	spin_unlock_irqrestore(&mbox_lock, flag);
}


int  mbox_init_debugfs(void *root)
{
	if (!root)
		return -ENXIO;

	if (!mbox_ops) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EPROBE_DEFER;
	}

	debugfs_create_file("mbox", S_IRUGO,
			    (struct dentry *)root,
			    NULL,
			    mbox_ops->debug_fops);
	return 0;
}
EXPORT_SYMBOL_GPL(mbox_init_debugfs);
#endif /* CONFIG_DEBUG_FS */

MODULE_AUTHOR("zhou wenping");
MODULE_DESCRIPTION("SIPC/mailbox driver");
MODULE_LICENSE("GPL");
