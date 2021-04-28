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

/* reg offset define */
#define MBOX_ID			0x00
#define MBOX_MSG_L		0x04
#define MBOX_MSG_H		0x08
#define MBOX_TRI		0x0c
#define MBOX_FIFO_RST		0x10
#define MBOX_IRQ_STS		0x18
#define MBOX_IRQ_MSK		0x1c
#define MBOX_LOCK		0x20
#define MBOX_FIFO_DEPTH		0x24

#define MBOX_FIFO_INBOX_STS_1		0x14
#define MBOX_FIFO_OUTBOX_STS_1	0x14
#define MBOX_FIFO_INBOX_STS_2		0x24
#define MBOX_FIFO_OUTBOX_STS_2	0x28

#define MBOX_PRIOR_LOW  0x00
#define MBOX_PRIOR_HIGH	0x04
#define MBOX_VERSION		0x08

/* reg mask define */
#define MBOX_IRQ_INBOX_CLR_BIT		BIT(0)
#define MBOX_IRQ_OUTBOX_CLR_BIT		BIT(16)

#define MBOX_FIFO_NOT_EMPTY_BIT			BIT(0)
#define MBOX_FIFO_EMPTY_BIT			BIT(1)
#define MBOX_FIFO_FULL_BIT			BIT(2)
#define MBOX_FIFO_OVERFLOW_BIT			BIT(3)
#define MBOX_FIFO_CHANGE_BIT			BIT(4)
#define MBOX_V1_FIFO_BLOCK_BIT			BIT(5)

#define MBOX_INBOX_FIFO_BLOCK_BIT		BIT(0)
#define MBOX_INBOX_FIFO_OVERFLOW_BIT		BIT(1)
#define MBOX_INBOX_FIFO_CHANGE_BIT		BIT(2)

#define MBOX_IRQ_OUTBOX_IRQ_MASK	GENMASK(16, 0)
#define MBOX_FIFO_DELIVER_OVERLOW_CLR_MASK	GENMASK(31, 0)
#define MBOX_FIFO_DELIVER_STS_MASK	GENMASK(15, 0)
#define MBOX_FIFO_DELIVER_STS_BIT		(0)
#define MBOX_FIFO_OUTBOX_RECV_STS_BIT	(16)
#define MBOX_FIFO_BLOCK_STS_MASK	GENMASK(15, 0)
#define MBOX_FIFO_BLOCK_STS_BIT		(0)
#define MBOX_FIFO_FULL_STS_MASK		GENMASK(2, 2)

#define MBOX_UNLOCK_KEY 0x5a5a5a5a

/* enable outbox not empty */
#define MBOX_OUTBOX_IRQ_CONFIG \
	(~(u32)(MBOX_FIFO_NOT_EMPTY_BIT))
	/* (~(MBOX_FIFO_CHANGE_BIT | MBOX_FIFO_NOT_EMPTY_BIT))  */

/* only enable outbox delivered now */
#define MBOX_OUTBOX_IRQ_CONFIG_FOR_IRQ_ONLY \
	(~(u32)(MBOX_FIFO_CHANGE_BIT))

/* enable inbox block and overflow */
#define MBOX_INBOX_BLOCK_IRQ \
	(~(u32)(MBOX_INBOX_FIFO_OVERFLOW_BIT | MBOX_INBOX_FIFO_BLOCK_BIT))

/* if mailbox version is v1, the bits of inbox and outbox have the same means */
#define MBOX_INBOX_BLOCK_IRQ_V1 \
	(~(u32)(MBOX_FIFO_OVERFLOW_BIT | MBOX_V1_FIFO_BLOCK_BIT))

/* enable inbox send succ */
#define MBOX_INBOX_DELIVERED_IRQ_V2 \
	(~(u32)(MBOX_INBOX_FIFO_CHANGE_BIT))
#define MBOX_INBOX_DELIVERED_IRQ_V1 \
		(~(u32)(MBOX_FIFO_CHANGE_BIT))

#define MBOX_INBOX_DELIVERED_IRQ \
	((mbox_cfg.version > 1) ? \
	MBOX_INBOX_DELIVERED_IRQ_V2 : MBOX_INBOX_DELIVERED_IRQ_V1)

#define MBOX_IRQ_DISABLE_ALLIRQ GENMASK(31, 0)

#define MBOX_FIFO_SIZE		256
#define MBOX_MAX_CORE_CNT	16
#define MBOX_MAX_CORE_MASK	0xF
#define MAX_SMSG_BAK		64

/*
 * mbox configs define: now we had two hardware version V1 and
 * V2, the below configs are diffrence between with two version
 */

#define MBOX_INBOX_REG_BASE		0x00000
#define MBOX_OUTBOX_REG_BASE	0x10000
#define MBOX_GLOBAL_REG_BASE	0x20000

/* mbox version 2 configs */
#define MBOX_V2_INBOX_FIFO_SIZE  0x1
#define MBOX_V2_OUTBOX_FIFO_SIZE 0x80

#define MBOX_V2_READ_PT_BIT  24
#define MBOX_V2_READ_PT_MASK 0xff

#define MBOX_V2_WRITE_PT_BIT 16
#define MBOX_V2_WRITE_PT_MASK 0xff

#define MBOX_V2_INBOX_CORE_SIZE 0x1000
#define MBOX_V2_OUTBOX_CORE_SIZE 0x1000

#define MBOX_V2_INBOX_IRQ_MASK   MBOX_INBOX_BLOCK_IRQ
#define MBOX_V2_OUTBOX_IRQ_MASK  MBOX_OUTBOX_IRQ_CONFIG

#define SEND_FIFO_LEN 64

/* mbox local feature define */
/* redefine debug function, only can open it in mbox bringup phase */
/*#define MBOX_REDEFINE_DEBUG_FUNCTION*/

/* remove pr_debug, because Mbox was used so frequently, remove
 * debug info can improve the system performance
 */
#define MBOX_REMOVE_PR_DEBUG

/* remove the same msg in fifo, It also can improve the system performance */
#define MBOX_REMOVE_THE_SAME_MSG

/* mbox test feature, will cteate a device "dev/sprd_mbox",
 * echo '1' to start,  echo '1' to stop, but we only can
 * open it in mbox bringup phase and don't care dsp state
 */

/* mbox local staruct global var define */
static struct regmap *mailbox_gpr;
static unsigned long sprd_inbox_base;
static unsigned long sprd_outbox_base;
static unsigned long sprd_global_base;

#define REGS_RECV_MBOX_SENSOR_BASE (sprd_outbox_base\
	+ (mbox_cfg.outbox_range * mbox_cfg.sensor_core))

#define MBOX_GET_FIFO_RD_PTR(val) ((val >> mbox_cfg.rd_bit) & (mbox_cfg.rd_mask))
#define MBOX_GET_FIFO_WR_PTR(val) ((val >> mbox_cfg.wr_bit) & (mbox_cfg.wr_mask))

static struct  mbox_fifo_data_tag mbox_fifo[MBOX_FIFO_SIZE];
static struct  mbox_fifo_data_tag mbox_fifo_bak[MAX_SMSG_BAK];

static u8 mbox_read_all_fifo_msg(void);
static void mbox_raw_recv(struct  mbox_fifo_data_tag *fifo);
static void mbox_cfg_printk(void);

static u8 mbox_sensor_read_all_fifo_msg(void);
static void mbox_sensor_raw_recv(struct  mbox_fifo_data_tag *fifo);
static int mbox_cfg_init(struct mbox_dts_cfg_tag *mbox_dts_cfg, u8 *mbox_inited);

static int mbox_fifo_bak_len;
static u8 g_one_time_recv_cnt;
static u8 g_inbox_send;
static u32 g_inbox_irq_mask;

static unsigned long max_total_irq_proc_time;
static unsigned long max_total_irq_cnt;

static int g_restore_cnt;
static int g_inbox_block_cnt;
static int g_outbox_full_cnt;
static int g_skip_msg;
static unsigned int g_recv_cnt[MBOX_MAX_CORE_CNT];
static unsigned int g_send_cnt[MBOX_MAX_CORE_CNT];

static struct mbox_chn_tag mbox_chns[MBOX_MAX_CORE_CNT];
static struct mbox_cfg_tag mbox_cfg;

extern int sipc_get_wakeup_flag(void);
extern void sipc_clear_wakeup_flag(void);

static irqreturn_t  mbox_src_irqhandle(int irq_num, void *dev)
{
	u32 fifo_sts_1, fifo_sts_2;
	u32 block, succ;

	/*read inbox status */
	fifo_sts_1 = readl_relaxed(
		(void __iomem *)(sprd_inbox_base + MBOX_FIFO_INBOX_STS_1));
	fifo_sts_2 = readl_relaxed(
		(void __iomem *)(sprd_inbox_base + MBOX_FIFO_INBOX_STS_2));

	pr_debug("mbox:%s,fifo_sts_1=%x, fifo_sts_2=0x%x\n",
		 __func__, fifo_sts_1, fifo_sts_2);

	/* clear inbox deliver and overlow status*/
	writel_relaxed(MBOX_FIFO_DELIVER_OVERLOW_CLR_MASK,
		     (void __iomem *)
		     (sprd_inbox_base + MBOX_FIFO_RST));
	block = (fifo_sts_2 & MBOX_FIFO_BLOCK_STS_MASK) >>
		MBOX_FIFO_BLOCK_STS_BIT;
	succ = (fifo_sts_1 & MBOX_FIFO_DELIVER_STS_MASK) >>
		MBOX_FIFO_DELIVER_STS_BIT;

	/* if It's a send irq and have some send succ bits,
	 * and the corresponding send fifo is not empty
	 */
	if (g_inbox_irq_mask == MBOX_INBOX_DELIVERED_IRQ) {
		g_inbox_send = mbox_get_send_fifo_mask(succ);
		pr_debug("mbox: succ = 0x%x, 0x%x!\n", succ, g_inbox_send);

		if (g_inbox_send)
			mbox_start_fifo_send(g_inbox_send);
	}

	/* if have some bits block, we enable delivered irq,
	 * other wise we enable block irq
	 */
	if (block) {
		g_inbox_irq_mask = MBOX_INBOX_DELIVERED_IRQ;
		pr_debug("mbox: block irq succ = 0x%x!\n", succ);
	} else {
		g_inbox_irq_mask = mbox_cfg.inbox_irq_mask;
		pr_debug("mbox: nonblock irq succ = 0x%x!\n", succ);
	}
	writel_relaxed(g_inbox_irq_mask,
		     (void __iomem *)
		     (sprd_inbox_base + MBOX_IRQ_MSK));

	/* clear irq */
	writel_relaxed(MBOX_IRQ_INBOX_CLR_BIT,
		     (void __iomem *)(sprd_inbox_base + MBOX_IRQ_STS));

	return IRQ_HANDLED;
}

static irqreturn_t mbox_recv_irqhandle(int irq_num, void *dev)
{
	u32 fifo_sts_1, irq_status;
	int i = 0;
	void *priv_data;
	u8 target_id;
	u8 fifo_len;
	unsigned long jiff, jiff_total;

	jiff_total = jiffies;

	/* get fifo status */
	fifo_sts_1 = readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_FIFO_OUTBOX_STS_1));
	irq_status =  readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_IRQ_STS));
	irq_status = irq_status & MBOX_IRQ_OUTBOX_IRQ_MASK;

	pr_debug("mbox:%s,fifo_sts_1=0x%08x,  irq_status =0x%08x\n",
		 __func__, fifo_sts_1, irq_status);

	fifo_len = mbox_sensor_read_all_fifo_msg();

	/* clear irq mask & irq after read all msg, if clear before read,
	 * it will produce a irq again
	 */
	writel_relaxed(irq_status,
		     (void __iomem *)(sprd_outbox_base + MBOX_IRQ_STS));

	/* print the id of the fist mail to know who wake up ap */
	if (sipc_get_wakeup_flag())
		pr_debug("mbox: wake up by id = %d\n",
			mbox_fifo[0].core_id);

	for (i = 0; i < fifo_len; i++) {
		target_id = mbox_fifo[i].core_id;

		if (target_id >= mbox_cfg.core_cnt) {
			pr_err("mbox:ERR on line %d, target_id >= mbox_cfg.core_cnt\n",
			       __LINE__);
			return IRQ_NONE;
		}

		if (mbox_chns[target_id].mbox_smsg_handler) {
			pr_debug("mbox: msg handle,index =%d, id = %d\n",
				 i, target_id);
			priv_data = mbox_chns[target_id].mbox_priv_data;
			/* get the jiffs before irq proc */
			jiff = jiffies;
			mbox_chns[target_id].mbox_smsg_handler(
				&mbox_fifo[i].msg,
				priv_data);
			/* update the max jiff time */
			jiff = jiffies - jiff;
			if (jiff > mbox_chns[target_id].max_irq_proc_time)
				mbox_chns[target_id].max_irq_proc_time = jiff;
		} else if (mbox_fifo_bak_len < MAX_SMSG_BAK) {
			pr_debug("mbox: msg bak hear,index =%d, id = %d\n",
				i, target_id);
			memcpy(&mbox_fifo_bak[mbox_fifo_bak_len],
			       &mbox_fifo[i],
			       sizeof(struct  mbox_fifo_data_tag));
			mbox_fifo_bak_len++;
		} else {
			pr_err("mbox: msg drop hear,index =%d, id = %d\n",
			       i, target_id);
		}
	}

	if (sipc_get_wakeup_flag())
		sipc_clear_wakeup_flag();

	/* update the max total irq time */
	jiff_total = jiffies - jiff_total;
	if (jiff_total > max_total_irq_proc_time)
		max_total_irq_proc_time = jiff_total;

	max_total_irq_cnt++;

	return IRQ_HANDLED;
}

static irqreturn_t mbox_sensor_recv_irqhandle(int irq_num, void *dev)
{
	u32 fifo_sts_1, irq_status;
	int i = 0;
	void *priv_data;
	u8 target_id;
	u8 fifo_len;
	unsigned long jiff, jiff_total;

	jiff_total = jiffies;

	/* get fifo status */
	fifo_sts_1 = readl_relaxed(
		(void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE + MBOX_FIFO_OUTBOX_STS_1));
	irq_status =  readl_relaxed(
		(void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE + MBOX_IRQ_STS));
	irq_status = irq_status & MBOX_IRQ_OUTBOX_IRQ_MASK;

	pr_debug("mbox:%s,fifo_sts_1=0x%08x,  irq_status =0x%08x\n",
		 __func__, fifo_sts_1, irq_status);

	fifo_len = mbox_read_all_fifo_msg();

	/* clear irq mask & irq after read all msg, if clear before read,
	 * it will produce a irq again
	 */
	writel_relaxed(irq_status,
		     (void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE + MBOX_IRQ_STS));

	for (i = 0; i < fifo_len; i++) {
		target_id = mbox_fifo[i].core_id;

		if (target_id >= mbox_cfg.core_cnt) {
			pr_err("mbox:ERR on line %d, target_id >= mbox_cfg.core_cnt\n",
			       __LINE__);
			return IRQ_NONE;
		}

		if (mbox_chns[target_id].mbox_smsg_handler) {
			pr_debug("mbox: msg handle,index =%d, id = %d\n",
				 i, target_id);
			priv_data = mbox_chns[target_id].mbox_priv_data;
			/* get the jiffs before irq proc */
			jiff = jiffies;
			mbox_chns[target_id].mbox_smsg_handler(
				&mbox_fifo[i].msg,
				priv_data);
			/* update the max jiff time */
			jiff = jiffies - jiff;
			if (jiff > mbox_chns[target_id].max_irq_proc_time)
				mbox_chns[target_id].max_irq_proc_time = jiff;
		} else if (mbox_fifo_bak_len < MAX_SMSG_BAK) {
			pr_debug("mbox: msg bak hear,index =%d, id = %d\n",
				i, target_id);
			memcpy(&mbox_fifo_bak[mbox_fifo_bak_len],
			       &mbox_fifo[i],
			       sizeof(struct  mbox_fifo_data_tag));
			mbox_fifo_bak_len++;
		} else {
			pr_err("mbox: msg drop hear,index =%d, id = %d\n",
			       i, target_id);
		}
	}

	/* update the max total irq time */
	jiff_total = jiffies - jiff_total;
	if (jiff_total > max_total_irq_proc_time)
		max_total_irq_proc_time = jiff_total;

	max_total_irq_cnt++;

	return IRQ_HANDLED;
}

static void mbox_sensor_raw_recv(struct  mbox_fifo_data_tag *fifo)
{
	u64 msg_l, msg_h;
	int target_id;

	msg_l = readl_relaxed(
		(void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE + MBOX_MSG_L));
	msg_h = readl_relaxed(
		(void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE + MBOX_MSG_H));
	target_id = readl_relaxed(
		(void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE + MBOX_ID));
	pr_debug("mbox:%s, id =%d, msg_l = 0x%x, msg_h = 0x%x\n",
		 __func__, target_id,
		 (unsigned int)msg_l, (unsigned int)msg_h);

	fifo->msg = (msg_h << 32) | msg_l;
	fifo->core_id = target_id & MBOX_MAX_CORE_MASK;
	g_recv_cnt[fifo->core_id]++;
	writel_relaxed(0x1, (void __iomem *)
		(REGS_RECV_MBOX_SENSOR_BASE + MBOX_TRI));
}

static u8 mbox_sensor_read_all_fifo_msg(void)
{
	u32 fifo_sts;
	u8 fifo_depth;
	u8 rd, wt, cnt, i;
#ifdef MBOX_REMOVE_THE_SAME_MSG
	struct smsg *msg;
	u8 j, skip;
	struct  mbox_fifo_data_tag fifo_data;
#endif

	fifo_sts = readl_relaxed(
		(void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE
		+ MBOX_FIFO_INBOX_STS_1));
	wt = MBOX_GET_FIFO_WR_PTR(fifo_sts);
	rd = MBOX_GET_FIFO_RD_PTR(fifo_sts);

	/* if fifo is full or empty, when the read ptr == write ptr */
	if (rd == wt) {
		if (fifo_sts & MBOX_FIFO_FULL_STS_MASK) {
			g_outbox_full_cnt++;
			cnt = mbox_cfg.outbox_fifo_size;
		} else {
			cnt = 0;
		}
	} else {
		if (wt > rd)
			cnt = wt - rd;
		else
			cnt = mbox_cfg.outbox_fifo_size - rd + wt;
	}
	pr_debug("mbox: %s, rd = %d, wt = %d, cnt = %d\n",
		 __func__, rd, wt, cnt);

	fifo_depth = 0;
	for (i = 0; i < cnt; i++) {
#ifdef MBOX_REMOVE_THE_SAME_MSG
		mbox_sensor_raw_recv(&fifo_data);

		skip = 0;
		/* skip the same msg to reduce the irq proc time */
		for (j = 0; j < fifo_depth; j++) {
			if (fifo_data.core_id == mbox_fifo[j].core_id &&
			    fifo_data.msg == mbox_fifo[j].msg) {
				msg = (struct smsg *)&fifo_data.msg;
				/* only skip the event type msg */
				if (msg->type == SMSG_TYPE_EVENT) {
					skip = 1;
					g_skip_msg++;
					break;
				}
			}
		}

		if (skip)
			continue;

		/* copy to fifo */
		memcpy(&mbox_fifo[fifo_depth],
		       &fifo_data,
		       sizeof(struct  mbox_fifo_data_tag));
		fifo_depth++;
		pr_debug("mbox: %s, fifo_depth=%d\n", __func__, fifo_depth);
#else
		mbox_sensor_raw_recv(&mbox_fifo[i]);
		fifo_depth++;
#endif
	}

	g_one_time_recv_cnt = cnt;

	return fifo_depth;
}

static void mailbox_process_bak_msg(void)
{
	int i;
	int cnt = 0;
	int target_id = 0;
	void *priv_data;

	for (i = 0; i < mbox_fifo_bak_len; i++) {
		target_id = mbox_fifo_bak[i].core_id;
		/* has been procced */
		if (target_id == MBOX_MAX_CORE_CNT) {
			cnt++;
			continue;
		}
		if (mbox_chns[target_id].mbox_smsg_handler) {
			pr_debug("mbox: bak msg pass to handler,index = %d, id = %d\n",
				 i, target_id);

			priv_data = mbox_chns[target_id].mbox_priv_data;
			mbox_chns[target_id].mbox_smsg_handler(
				&mbox_fifo_bak[i].msg,
				priv_data);
			/* set a mask indicate the bak msg is been procced*/
			mbox_fifo_bak[i].core_id = MBOX_MAX_CORE_CNT;
			cnt++;
		} else {
			pr_debug("mbox_smsg_handler is NULL,index = %d, id = %d\n",
				 i, target_id);
		}
	}

	/* reset  mbox_fifo_bak_len*/
	if (mbox_fifo_bak_len == cnt)
		mbox_fifo_bak_len = 0;
}

static int mbox_phy_register_irq_handle(u8 target_id,
			     MBOX_FUNCALL irq_handler,
			     void *priv_data)
{
	if (target_id >= mbox_cfg.core_cnt || mbox_chns[target_id].mbox_smsg_handler)
		return -EINVAL;

	mbox_chns[target_id].mbox_smsg_handler = irq_handler;
	mbox_chns[target_id].mbox_priv_data = priv_data;

	return 0;
}

static int mbox_phy_unregister_irq_handle(u8 target_id)
{
	u32 reg_val;

	if (target_id >= mbox_cfg.core_cnt || !mbox_chns[target_id].mbox_smsg_handler)
		return -EINVAL;

	mbox_chns[target_id].mbox_smsg_handler = NULL;

	/*clean the the corresponding regist core  irq status*/
	reg_val = readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_IRQ_STS));
	reg_val |= (0x1 << target_id) << 8;
	writel_relaxed(reg_val,
		     (void __iomem *)(sprd_outbox_base + MBOX_IRQ_STS));

	return 0;
}

static void mbox_raw_recv(struct  mbox_fifo_data_tag *fifo)
{
	u64 msg_l, msg_h;
	int target_id;

	msg_l = readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_MSG_L));
	msg_h = readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_MSG_H));
	target_id = readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_ID));
	pr_debug("mbox:%s, id =%d, msg_l = 0x%x, msg_h = 0x%x\n",
		 __func__, target_id,
		 (unsigned int)msg_l, (unsigned int)msg_h);

	fifo->msg = (msg_h << 32) | msg_l;
	fifo->core_id = target_id & MBOX_MAX_CORE_MASK;
	g_recv_cnt[fifo->core_id]++;
	writel_relaxed(0x1, (void __iomem *)(sprd_outbox_base + MBOX_TRI));
}

static u8 mbox_read_all_fifo_msg(void)
{
	u32 fifo_sts;
	u8 fifo_depth;
	u8 rd, wt, cnt, i;
#ifdef MBOX_REMOVE_THE_SAME_MSG
	struct smsg *msg;
	u8 j, skip;
	struct  mbox_fifo_data_tag fifo_data;
#endif

	fifo_sts = readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_FIFO_INBOX_STS_1));
	wt = MBOX_GET_FIFO_WR_PTR(fifo_sts);
	rd = MBOX_GET_FIFO_RD_PTR(fifo_sts);

    /* if fifo is full or empty, when the read ptr == write ptr */
	if (rd == wt) {
		if (fifo_sts & MBOX_FIFO_FULL_STS_MASK) {
			g_outbox_full_cnt++;
			cnt = mbox_cfg.outbox_fifo_size;
		} else {
			cnt = 0;
		}
	} else {
		if (wt > rd)
			cnt = wt - rd;
		else
			cnt = mbox_cfg.outbox_fifo_size - rd + wt;
	}

	if (cnt == 0 || cnt > mbox_cfg.outbox_fifo_size)
		pr_err("mbox: %s, rd = %d, wt = %d, cnt = %d\n",
		       __func__, rd, wt, cnt);

	pr_debug("mbox: %s, rd = %d, wt = %d, cnt = %d\n",
		 __func__, rd, wt, cnt);

	fifo_depth = 0;
	for (i = 0; i < cnt; i++) {
#ifdef MBOX_REMOVE_THE_SAME_MSG
		mbox_raw_recv(&fifo_data);

		skip = 0;
		/* skip the same msg to reduce the irq proc time */
		for (j = 0; j < fifo_depth; j++) {
			if (fifo_data.core_id == mbox_fifo[j].core_id &&
			    fifo_data.msg == mbox_fifo[j].msg) {
				msg = (struct smsg *)&fifo_data.msg;
				/* only skip the event type msg */
				if (msg->type == SMSG_TYPE_EVENT) {
					skip = 1;
					g_skip_msg++;
					break;
				}
			}
		}

		if (skip)
			continue;

		/* copy to fifo */
		memcpy(&mbox_fifo[fifo_depth],
		       &fifo_data,
		       sizeof(struct  mbox_fifo_data_tag));
		fifo_depth++;
		pr_debug("mbox: %s, fifo_depth=%d\n", __func__, fifo_depth);
#else
		mbox_raw_recv(&mbox_fifo[i]);
		fifo_depth++;
#endif
	}

	g_one_time_recv_cnt = cnt;

	return fifo_depth;
}

static u32 mbox_phy_core_fifo_full(int core_id)
{
	u32 fifo_sts = 0;
	u32 mail_box_offset;

	mail_box_offset = mbox_cfg.outbox_range * core_id;

	fifo_sts = readl_relaxed((void __iomem *)(
		sprd_outbox_base + mail_box_offset + MBOX_FIFO_INBOX_STS_1));

	if (fifo_sts & MBOX_FIFO_FULL_STS_MASK) {
		pr_debug("mbox: is full,core_id = %d, fifo_sts=0x%x\n",
			core_id, fifo_sts);
		return 1;
	}

	return 0;
}

static int mbox_phy_send(u8 core_id, u64 msg)
{
	u32 l_msg = (u32)msg;
	u32 h_msg = (u32)(msg >> 32);
	u32 fifo_sts_1, fifo_sts_2, block, recv_flag;
	unsigned long recv_flag_cnt;

	pr_debug("mbox:%s, core_id=%d\n", __func__, (u32)core_id);

	fifo_sts_1 = readl_relaxed(
		(void __iomem *)(sprd_inbox_base + MBOX_FIFO_INBOX_STS_1));
	fifo_sts_2 = readl_relaxed(
		(void __iomem *)(sprd_inbox_base + MBOX_FIFO_INBOX_STS_2));
	block = (fifo_sts_2 & MBOX_FIFO_BLOCK_STS_MASK) >>
		MBOX_FIFO_BLOCK_STS_BIT;

	/* if dst bit inbox block,  we dont't send it */
	if (block & (1 << core_id))
		goto block_exit;

	/* wait outbox recv flag, until flag is 0
	 * (mail be send to  outbox will clear it)
	 */
	recv_flag_cnt = 0;
	recv_flag = 1 << (core_id + MBOX_FIFO_OUTBOX_RECV_STS_BIT);
	while (fifo_sts_1 & recv_flag) {
		recv_flag_cnt++;
		fifo_sts_1 =
		readl_relaxed(
			    (void __iomem *)
			    (sprd_inbox_base + MBOX_FIFO_INBOX_STS_1));
		fifo_sts_2 = readl_relaxed(
			(void __iomem *)(sprd_inbox_base
			+ MBOX_FIFO_INBOX_STS_2));
		block = (fifo_sts_2 & MBOX_FIFO_BLOCK_STS_MASK) >>
			MBOX_FIFO_BLOCK_STS_BIT;

		/* if block, outbox recv flag will always be 1,
		 * because mail cat't be send to outbox
		 */
		if (block & (1 << core_id))
			goto block_exit;
	}

	if (mbox_chns[core_id].max_recv_flag_cnt < recv_flag_cnt)
		mbox_chns[core_id].max_recv_flag_cnt = recv_flag_cnt;

	writel_relaxed(l_msg,
		     (void __iomem *)
		     (sprd_inbox_base + MBOX_MSG_L));
	writel_relaxed(h_msg,
		     (void __iomem *)
		     (sprd_inbox_base + MBOX_MSG_H));
	writel_relaxed(core_id,
		     (void __iomem *)
		     (sprd_inbox_base + MBOX_ID));
	writel_relaxed(0x1,
		     (void __iomem *)
		     (sprd_inbox_base + MBOX_TRI));

	g_send_cnt[core_id]++;

	return 0;

block_exit:

	pr_debug("mbox: is block,core_id = %d, sts_1=0x%x, sts2=0x%x",
		core_id, fifo_sts_1, fifo_sts_2);
	g_inbox_block_cnt++;

	return -EBUSY;
}

static void mbox_phy_just_sent(u8 core_id, u64 msg)
{
	u32 l_msg = (u32)msg;
	u32 h_msg = (u32)(msg >> 32);

	writel_relaxed(l_msg, (void __iomem *)(sprd_inbox_base + MBOX_MSG_L));
	writel_relaxed(h_msg, (void __iomem *)(sprd_inbox_base + MBOX_MSG_H));
	writel_relaxed(core_id, (void __iomem *)(sprd_inbox_base + MBOX_ID));
	writel_relaxed(0x1, (void __iomem *)(sprd_inbox_base + MBOX_TRI));

	g_send_cnt[core_id]++;
}

static bool mbox_phy_outbox_has_irq(void)
{
	u32 irq_sts;

	irq_sts = readl_relaxed(
		(void __iomem *)(sprd_outbox_base + MBOX_IRQ_STS));

	return (irq_sts & MBOX_IRQ_OUTBOX_CLR_BIT);
}

static void mbox_cfg_printk(void)
{
	pr_debug("mbox:inbox_base = 0x%x, outbox_base = 0x%x\n",
		mbox_cfg.inbox_base, mbox_cfg.outbox_base);

	pr_debug("mbox:inbox_range = 0x%x, outbox_range = 0x%x\n",
		mbox_cfg.inbox_range, mbox_cfg.outbox_range);

	pr_debug("mbox:inbox_fifo = %d, outbox_fifo = %d\n",
		mbox_cfg.inbox_fifo_size, mbox_cfg.outbox_fifo_size);

	pr_debug("mbox:inbox_irq = %d, outbox_irq = %d\n",
		mbox_cfg.inbox_irq, mbox_cfg.outbox_irq);

	pr_debug("mbox:inbox_irq_mask = 0x%x, outbox_irq_mask = 0x%x\n",
		mbox_cfg.inbox_irq_mask, mbox_cfg.outbox_irq_mask);

	pr_debug("mbox:outbox_sensor_irq = %d\n",
		mbox_cfg.outbox_sensor_irq);

	pr_debug("mbox:sensor_core = %d\n",
		mbox_cfg.sensor_core);

	pr_debug("mbox:rd_bit = %d, rd_mask = %d\n",
		mbox_cfg.rd_bit, mbox_cfg.rd_mask);

	pr_debug("mbox:wr_bit = %d, wr_mask = %d\n",
		mbox_cfg.wr_bit, mbox_cfg.wr_mask);

	pr_debug("mbox:enable_reg = 0x%x, mask_bit = 0x%x\n",
		mbox_cfg.enable_reg, mbox_cfg.mask_bit);

	pr_debug("mbox:prior_low = %d, version = %d\n",
		mbox_cfg.core_cnt, mbox_cfg.prior_low);

	pr_debug("mbox:prior_high = %d, version = %d\n",
		mbox_cfg.core_cnt, mbox_cfg.prior_high);
}

static int mbox_pm_event(struct notifier_block *notifier,
			 unsigned long pm_event, void *unused)
{
	if (mbox_cfg.sensor_core == MBOX_MAX_CORE_CNT)
		return NOTIFY_DONE;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pr_debug("mbox:PM_SUSPEND_PREPARE\n");
		/* disable all outbox irq mask */
		writel_relaxed(MBOX_IRQ_DISABLE_ALLIRQ,
			     (void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE
			     + MBOX_IRQ_MSK));

		break;

	case PM_POST_SUSPEND:
		pr_debug("mbox:PM_POST_SUSPEND\n");
		/* restore outbox irq mask */
		writel_relaxed(mbox_cfg.outbox_irq_mask,
			     (void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE
			     + MBOX_IRQ_MSK));
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mailbox_pm_notifier_block = {
	.notifier_call = mbox_pm_event,
};

#if defined(CONFIG_DEBUG_FS)
static void mbox_check_all_inbox(struct seq_file *m)
{
	int i;
	unsigned long inbox;
	u32 status, block;

	for (i = 0; i < mbox_cfg.core_cnt; i++) {
		inbox = sprd_inbox_base + i * mbox_cfg.inbox_range;
		status = readl_relaxed(
			(void __iomem *)(sprd_inbox_base + MBOX_FIFO_INBOX_STS_2));
		block = (status & MBOX_FIFO_BLOCK_STS_MASK) >>
			MBOX_FIFO_BLOCK_STS_BIT;

		/* find the block inbox*/
		if (block)
			seq_printf(m, "    inbox %d is block!\n", i);
	}
}

static void mbox_check_all_outbox(struct seq_file *m)
{
	int i;
	unsigned long outbox;
	u32 status;

	for (i = 0; i < mbox_cfg.core_cnt; i++) {
		outbox = sprd_outbox_base + i * mbox_cfg.outbox_range;
		status = readl_relaxed((void __iomem *)(outbox + MBOX_FIFO_INBOX_STS_1));

		/* find the full outbox */
		if (status & MBOX_FIFO_FULL_STS_MASK)
			seq_printf(m, "    outbox %d is full!\n", i);
	}
}

extern void sipc_debug_putline(struct seq_file *m, char c, int n);

static int mbox_debug_show(struct seq_file *m, void *private)
{
	int i;
	unsigned long box;

	/* mbox */
	sipc_debug_putline(m, '*', 120);
	seq_puts(m, "1. mailbox config:\n");
	sipc_debug_putline(m, '-', 110);

	seq_printf(m, "\n    enable_reg:  0x%08x,     reg_mask:    0x%08x\n",
		   mbox_cfg.enable_reg + 0x402e0000, mbox_cfg.mask_bit);
	seq_printf(m, "    inbox_base:  0x%08x,     outbox_base:  0x%08x\n",
		   mbox_cfg.inbox_base, mbox_cfg.outbox_base);
	seq_printf(m, "    inbox_range: 0x%08x,     outbox_range: 0x%08x\n",
		   mbox_cfg.inbox_range, mbox_cfg.outbox_range);
	seq_printf(m, "    inbox_mask:  0x%08x,     outbox_mask:  0x%08x\n",
		   mbox_cfg.inbox_irq_mask, mbox_cfg.outbox_irq_mask);
	seq_printf(m, "\n    inbox_fifo: 0x%02x,     outbox_fifo: 0x%02x\n",
		   mbox_cfg.inbox_fifo_size, mbox_cfg.outbox_fifo_size);
	seq_printf(m, "    rd_pt_bit:  0x%02x,     wr_pt_bit:   0x%02x\n",
		   mbox_cfg.rd_bit, mbox_cfg.wr_bit);
	seq_printf(m, "    core_cnt:   0x%02x,     version:     0x%02x\n",
		   mbox_cfg.core_cnt, mbox_cfg.prior_low);
	seq_printf(m, "    core_cnt:   0x%02x,     version:     0x%02x\n",
		   mbox_cfg.core_cnt, mbox_cfg.prior_high);
	seq_printf(m, "    inbox_irq:  0x%02x,     outbox_irq:  0x%02x\n",
		   mbox_cfg.inbox_irq, mbox_cfg.outbox_irq);
	seq_printf(m, "    sensor_id:  0x%02x,	   sensor_irq:	0x%02x\n",
		   mbox_cfg.sensor_core, mbox_cfg.outbox_sensor_irq);

	sipc_debug_putline(m, '-', 110);
	seq_puts(m, "\n2. mailbox outbox reg:\n");
	for (i = 0; i < mbox_cfg.core_cnt; i++) {
		box = sprd_outbox_base + i * mbox_cfg.outbox_range;
		sipc_debug_putline(m, '-', 105);
		seq_printf(m, "outbox %d reg start:\n", i);
		seq_printf(m, "\n  core_id:     0x%08x",
			   readl_relaxed((void __iomem *)(box + 0x0)));

		seq_printf(m, "  msg_low:     0x%08x  msg_high:    0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x4)),
			   readl_relaxed((void __iomem *)(box + 0x8)));

		seq_printf(m, "  fifo_reset:  0x%08x  fifo_status: 0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x10)),
			   readl_relaxed((void __iomem *)(box + 0x14)));

		seq_printf(m, "  irq_status:  0x%08x  irq_mask:    0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x18)),
			   readl_relaxed((void __iomem *)(box + 0x1c)));

		seq_printf(m, "  fifo_depth:     0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x24)));
	}

	sipc_debug_putline(m, '-', 110);
	seq_puts(m, "\n3. mailbox inbox reg:\n");

	for (i = 0; i < mbox_cfg.core_cnt; i++) {
		sipc_debug_putline(m, '-', 105);
		seq_printf(m, "inbox %d reg start:\n", i);

		box = sprd_inbox_base + i * mbox_cfg.inbox_range;

		seq_printf(m, "\n  core_id:     0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x0)));

		seq_printf(m, "  msg_low:     0x%08x  msg_high:    0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x4)),
			   readl_relaxed((void __iomem *)(box + 0x8)));

		seq_printf(m, "  fifo_reset:  0x%08x  fifo_status: 0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x10)),
			   readl_relaxed((void __iomem *)(box + 0x14)));

		seq_printf(m, "  irq_status:  0x%08x  irq_mask:    0x%08x\n",
			   readl_relaxed((void __iomem *)(box + 0x18)),
			   readl_relaxed((void __iomem *)(box + 0x1c)));
	}

	sipc_debug_putline(m, '-', 110);
	seq_puts(m, "\n3. mailbox global reg:\n");
	box = sprd_global_base;
	seq_printf(m, "  version: 0x%08x\n",
		 mbox_cfg.version);
	seq_printf(m, "  prior_low:	0x%08x	prior_high: 0x%08x\n",
		 mbox_cfg.prior_low,
		 mbox_cfg.prior_low);

	sipc_debug_putline(m, '-', 110);

	seq_puts(m, "\n4. mailbox data:\n");

	seq_printf(m, "\n    restore_cnt: %d,     block_cnt: %d\n",
		   g_restore_cnt, g_inbox_block_cnt);
	seq_printf(m, "    oboxfull_cnt:%d,     skip_msg:  %d\n",
		   g_outbox_full_cnt, g_skip_msg);
	seq_printf(m, "    max_total_irq_time: %lu\n",
		   max_total_irq_proc_time);
	seq_printf(m, "    max_total_irq_cnt: %lu\n\n",
		   max_total_irq_cnt);

	for (i = 0; i < mbox_cfg.core_cnt; i++) {
		if (mbox_chns[i].mbox_smsg_handler)
			seq_printf(m, "    mbox core %d,        max_irq_time: %lu\n",
				   i, mbox_chns[i].max_irq_proc_time);

		seq_printf(m, "    mbox core %d,        max_recv_flag_cnt: %lu\n",
			   i, mbox_chns[i].max_recv_flag_cnt);
	}

	sipc_debug_putline(m, '-', 110);

	seq_puts(m, "\n5. mailbox total:\n");

	for (i = 0; i < mbox_cfg.core_cnt; i++) {
		seq_printf(m, "    mbox core %d,        recv_mail_cnt: %u\n",
			   i, g_recv_cnt[i]);
		seq_printf(m, "    mbox core %d,        send_mail_cnt: %u\n",
			   i, g_send_cnt[i]);
	}

	mbox_check_all_inbox(m);
	mbox_check_all_outbox(m);
	mbox_check_all_send_fifo(m);

	return 0;
}

static int mbox_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mbox_debug_show, inode->i_private);
}

static const struct file_operations mbox_debug_fops = {
	.open = mbox_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif /* CONFIG_DEBUG_FS */

static const struct mbox_operations_tag mbox_r2p0_operation = {
	.cfg_init = mbox_cfg_init,
	.phy_register_irq_handle = mbox_phy_register_irq_handle,
	.phy_unregister_irq_handle = mbox_phy_unregister_irq_handle,
	.src_irqhandle = mbox_src_irqhandle,
	.recv_irqhandle = mbox_recv_irqhandle,
	.sensor_recv_irqhandle = mbox_sensor_recv_irqhandle,
	.phy_send = mbox_phy_send,
	.process_bak_msg = mailbox_process_bak_msg,
	.phy_core_fifo_full = mbox_phy_core_fifo_full,
	.phy_just_sent = mbox_phy_just_sent,
	.outbox_has_irq = mbox_phy_outbox_has_irq
};

static struct mbox_device_tag mbox_r2p0_device = {
	.version = 0x200,
	.max_cnt = MBOX_MAX_CORE_CNT,
	.fops = &mbox_r2p0_operation,
#if defined(CONFIG_DEBUG_FS)
	.debug_fops = &mbox_debug_fops,
#endif
};

static int mbox_cfg_init(struct mbox_dts_cfg_tag *mbox_dts_cfg, u8 *mbox_inited)
{
	int ret;
	unsigned long base;

	mailbox_gpr = mbox_dts_cfg->gpr;

	/* init enable reg and mask bit */
	mbox_cfg.enable_reg = mbox_dts_cfg->enable_reg;
	mbox_cfg.mask_bit = mbox_dts_cfg->mask_bit;
	mbox_cfg.version = mbox_dts_cfg->version;

	/* init inbox base */
	mbox_cfg.inbox_base = mbox_dts_cfg->inboxres.start;
	base = (unsigned long)ioremap_nocache(mbox_dts_cfg->inboxres.start,
							 resource_size(&mbox_dts_cfg->inboxres));
	if (!base) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}

	sprd_inbox_base  = base + MBOX_INBOX_REG_BASE;
	sprd_outbox_base = base + MBOX_OUTBOX_REG_BASE;
	sprd_global_base = base + MBOX_GLOBAL_REG_BASE;

	/* init irq */
	mbox_cfg.inbox_irq = mbox_dts_cfg->inbox_irq;
	mbox_cfg.outbox_irq = mbox_dts_cfg->outbox_irq;
	mbox_cfg.outbox_sensor_irq = mbox_dts_cfg->outbox_sensor_irq;

	/* init sensor core */
	if (mbox_dts_cfg->sensor_core == MBOX_INVALID_CORE) {
		mbox_cfg.sensor_core = MBOX_MAX_CORE_CNT;
	} else {
		if (mbox_dts_cfg->sensor_core >= MBOX_MAX_CORE_CNT) {
				pr_err("mbox:ERR on line %d!\n", __LINE__);
				return -EINVAL;
		}
		mbox_cfg.sensor_core = mbox_dts_cfg->sensor_core;
	}

	/* init core cnt*/
	if (mbox_dts_cfg->core_cnt > MBOX_MAX_CORE_CNT) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}
	mbox_cfg.core_cnt = mbox_dts_cfg->core_cnt;

	/* init fifo size */
	mbox_cfg.inbox_fifo_size  = MBOX_V2_INBOX_FIFO_SIZE;
	mbox_cfg.outbox_fifo_size = MBOX_V2_OUTBOX_FIFO_SIZE;

	/* init fifo read ptr */
	mbox_cfg.rd_bit  = MBOX_V2_READ_PT_BIT;
	mbox_cfg.rd_mask = MBOX_V2_READ_PT_MASK;

	/* init fifo write ptr */
	mbox_cfg.wr_bit  = MBOX_V2_WRITE_PT_BIT;
	mbox_cfg.wr_mask = MBOX_V2_WRITE_PT_MASK;

	/* init core range */
	mbox_cfg.inbox_range  = MBOX_V2_INBOX_CORE_SIZE;
	mbox_cfg.outbox_range = MBOX_V2_OUTBOX_CORE_SIZE;

	/* init irq mask */
	mbox_cfg.inbox_irq_mask  = MBOX_V2_INBOX_IRQ_MASK;
	mbox_cfg.outbox_irq_mask = MBOX_V2_OUTBOX_IRQ_MASK;

	/* 0x402e0004, bit 21 , enable mailbox clk */
	if (mailbox_gpr) {
		pr_debug("mbox:enable mailbox clk!\n");

		regmap_update_bits(mailbox_gpr,
				   mbox_cfg.enable_reg,
				   mbox_cfg.mask_bit,
				   mbox_cfg.mask_bit);
	}

	/* read fifo depth and version form reg, must + 1 */
	mbox_cfg.outbox_fifo_size = readl_relaxed(
			(void __iomem *)(sprd_outbox_base + MBOX_FIFO_DEPTH)) + 1;
	mbox_cfg.version = readl_relaxed(
			(void __iomem *)(sprd_global_base + MBOX_VERSION));
	mbox_cfg.prior_low = readl_relaxed(
			(void __iomem *)(sprd_global_base + MBOX_PRIOR_LOW));
	mbox_cfg.prior_high = readl_relaxed(
			(void __iomem *)(sprd_global_base + MBOX_PRIOR_HIGH));

	mbox_cfg_printk();

	/* normal mode */
	writel_relaxed(0x0,
		     (void __iomem *)(sprd_outbox_base +  MBOX_FIFO_RST));

	/* set inbox irq mask */
	g_inbox_irq_mask = mbox_cfg.inbox_irq_mask;
	writel_relaxed(mbox_cfg.inbox_irq_mask,
		     (void __iomem *)(sprd_inbox_base + MBOX_IRQ_MSK));

	/* set outbox irq mask */
	writel_relaxed(mbox_cfg.outbox_irq_mask,
		     (void __iomem *)(sprd_outbox_base + MBOX_IRQ_MSK));

	/* set outbox irq mask */
	if ((mbox_cfg.outbox_sensor_irq) && (mbox_cfg.sensor_core <= MBOX_MAX_CORE_CNT)) {
		writel_relaxed(mbox_cfg.outbox_irq_mask,
			     (void __iomem *)(REGS_RECV_MBOX_SENSOR_BASE
			     + MBOX_IRQ_MSK));

		ret = register_pm_notifier(&mailbox_pm_notifier_block);
		if (ret) {
			pr_err("mbox:ERR on line %d!\n", __LINE__);
			return -EINVAL;
		}
	}

	/* lock irq here until g_mbox_inited =1,
	 * it make sure that when irq handle come,
	 * g_mboX_inited is 1
	 */
	*mbox_inited = 1;

	/* request inbox arm irq ,and enable wake up */
	ret = request_irq(mbox_cfg.inbox_irq,
			  mbox_src_irqhandle,
			  IRQF_NO_SUSPEND,
			  "sprd-mailbox_source",
			  NULL);
	if (ret) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}

	enable_irq_wake(mbox_cfg.inbox_irq);

	/* request outbox arm irq, and enable wake up */
	ret = request_irq(mbox_cfg.outbox_irq,
			  mbox_recv_irqhandle,
			  IRQF_NO_SUSPEND,
			  "sprd-mailbox_target",
			  NULL);
	if (ret) {
		pr_err("mbox:ERR on line %d!\n", __LINE__);
		return -EINVAL;
	}
	enable_irq_wake(mbox_cfg.outbox_irq);

	if ((mbox_cfg.outbox_sensor_irq) && (mbox_cfg.sensor_core <= MBOX_MAX_CORE_CNT)) {
		ret = request_irq(mbox_cfg.outbox_sensor_irq,
				  mbox_sensor_recv_irqhandle,
				  0,
				  "sprd-sensorbox_target",
				  NULL);
		if (ret) {
			pr_err("mbox:ERR on line %d!\n", __LINE__);
			return -EINVAL;
		}
	}

#ifdef SPRD_MAILBOX_TEST
	mbox_test_init();
#endif

	return 0;
}

void mbox_get_phy_device(struct mbox_device_tag **mbox_dev)
{
	*mbox_dev = &mbox_r2p0_device;
}

MODULE_AUTHOR("zhou wenping");
MODULE_DESCRIPTION("SIPC/mailbox driver");
MODULE_LICENSE("GPL");
