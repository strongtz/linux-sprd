/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <misc/wcn_bus.h>

#include "bufring.h"
#ifdef CONFIG_WCN_PCIE
#include "edma_engine.h"
#endif
#include "wcn_glb.h"
#include "wcn_procfs.h"
#include "mdbg_type.h"
#include "../include/wcn_dbg.h"

#define	LOG_BUF_NUM	16
#define	LOG_BUF_SIZE	1500

#ifdef CONFIG_WCN_PCIE
int mdbg_log_cb(int channel, struct mbuf_t *head, struct mbuf_t *tail, int num);
int mdbg_log_push(int chn, struct mbuf_t **head,
		  struct mbuf_t **tail, int *num);
#else
int mdbg_log_read(int channel, struct mbuf_t *head,
		  struct mbuf_t *tail, int num);
#endif

static struct ring_device *ring_dev;
static unsigned long long rx_count;
static unsigned long long rx_count_last;
static atomic_t ring_reg_flag;

#ifdef CONFIG_WCN_PCIE
static struct mchn_ops_t mdbg_ringc_ops = {
	.channel = WCN_RING_RX,
	.inout = WCNBUS_RX,
	.hif_type = 1,
	/* bt cp log buffer size: 360*4, so setting as 1500 */
	.buf_size = 1500,
	/* bt log buf num + pld buf num = 4 + 8 */
	.pool_size = 16,
	.cb_in_irq = 0,
	.pop_link = mdbg_log_cb,
	.push_link = mdbg_log_push,
};
#else
static struct mchn_ops_t mdbg_ringc_ops = {
	.channel = WCN_RING_RX,
	.inout = WCNBUS_RX,
	.pool_size = 1,
	.pop_link = mdbg_log_read,
};
#endif

#ifdef CONFIG_WCN_PCIE
int mdbg_log_push(int chn, struct mbuf_t **head, struct mbuf_t **tail, int *num)
{
	WCN_INFO("%s enter num=%d, chn=%d,mbuf used done", __func__, *num, chn);
#ifdef CONFIG_WCN_PCIE
	edma_dump_glb_reg();
	edma_dump_chn_reg(chn);
#endif

	return 0;
}
#endif

bool mdbg_rx_count_change(void)
{
	rx_count = sprdwcn_bus_get_rx_total_cnt();

	WCN_DBG("rx_count:0x%llx rx_count_last:0x%llx\n",
		rx_count, rx_count_last);

	if ((rx_count == 0) && (rx_count_last == 0)) {
		return true;
	} else if (rx_count != rx_count_last) {
		rx_count_last = rx_count;
		return true;
	} else {
		return false;
	}
}

int mdbg_read_release(unsigned int fifo_id)
{
	return 0;
}

long mdbg_content_len(void)
{
	if (unlikely(!ring_dev))
		return 0;

	return mdbg_ring_readable_len(ring_dev->ring);
}

static long int mdbg_comm_write(char *buf,
				long int len, unsigned int subtype)
{
	unsigned char *send_buf = NULL;
	char *str = NULL;
	struct mbuf_t *head, *tail;
	int num = 1;

	if (unlikely(marlin_get_module_status() != true)) {
		WCN_WARN("WCN module have not open\n");
		return -EIO;
	}
	send_buf = kzalloc(len + PUB_HEAD_RSV + 1, GFP_KERNEL);
	if (!send_buf)
		return -ENOMEM;
	memcpy(send_buf + PUB_HEAD_RSV, buf, len);

	str = strstr(send_buf + PUB_HEAD_RSV, SMP_HEAD_STR);
	if (!str)
		str = strstr(send_buf + PUB_HEAD_RSV + ARMLOG_HEAD,
			     SMP_HEAD_STR);

	if (str) {
		int ret;

		/* for arm log to pc */
		WCN_INFO("smp len:%ld,str:%s\n", len, str);
		str[sizeof(SMP_HEAD_STR)] = 0;
		ret = kstrtol(&str[sizeof(SMP_HEAD_STR) - 1], 10,
							&ring_dev->flag_smp);
		WCN_INFO("smp ret:%d, flag_smp:%ld\n", ret,
			 ring_dev->flag_smp);
		kfree(send_buf);
	} else {
		if (!sprdwcn_bus_list_alloc(
				mdbg_proc_ops[MDBG_AT_TX_OPS].channel,
				&head, &tail, &num)) {
			head->buf = send_buf;
			head->len = len;
			head->next = NULL;
			sprdwcn_bus_push_list(
				mdbg_proc_ops[MDBG_AT_TX_OPS].channel,
				head, tail, num);
		}
	}

	return len;
}

static void mdbg_ring_rx_task(unsigned long data)
{
	struct ring_rx_data *rx = NULL;
	struct mdbg_ring_t *ring = NULL;
	struct mbuf_t *mbuf_node;
	int i;
#ifndef CONFIG_WCN_PCIE
	struct bus_puh_t *puh = NULL;
#endif

	if (unlikely(!ring_dev)) {
		WCN_ERR("ring_dev is NULL\n");
		return;
	}

	spin_lock_bh(&ring_dev->rw_lock);
	rx = list_first_entry_or_null(&ring_dev->rx_head,
				      struct ring_rx_data, entry);
	if (rx) {
		list_del(&rx->entry);
	} else {
		WCN_WARN("tasklet something err\n");
		spin_unlock_bh(&ring_dev->rw_lock);
		return;
	}
	if (!list_empty(&ring_dev->rx_head))
		tasklet_schedule(&ring_dev->rx_task);
	ring = ring_dev->ring;
	spin_unlock_bh(&ring_dev->rw_lock);

	for (i = 0, mbuf_node = rx->head; i < rx->num; i++,
		mbuf_node = mbuf_node->next) {
#ifndef CONFIG_WCN_PCIE
		rx->addr = mbuf_node->buf + PUB_HEAD_RSV;
		puh = (struct bus_puh_t *)mbuf_node->buf;
		mdbg_ring_write(ring, rx->addr, puh->len);
#else
		mdbg_ring_write(ring, mbuf_node->buf, mbuf_node->len);
#endif
	}
	sprdwcn_bus_push_list(mdbg_ringc_ops.channel,
			      rx->head, rx->tail, rx->num);
	wake_up_log_wait();
	kfree(rx);
}

int mdbg_log_read(int channel, struct mbuf_t *head,
		  struct mbuf_t *tail, int num)
{
	struct ring_rx_data *rx;

	if (ring_dev) {
		mutex_lock(&ring_dev->mdbg_read_mutex);
		rx = kmalloc(sizeof(*rx), GFP_KERNEL);
		if (!rx) {
			WCN_ERR("mdbg ring low memory\n");
			mutex_unlock(&ring_dev->mdbg_read_mutex);
			sprdwcn_bus_push_list(channel, head, tail, num);
			return 0;
		}
		mutex_unlock(&ring_dev->mdbg_read_mutex);
		spin_lock_bh(&ring_dev->rw_lock);
		rx->channel = channel;
		rx->head = head;
		rx->tail = tail;
		rx->num = num;
		list_add_tail(&rx->entry, &ring_dev->rx_head);
		spin_unlock_bh(&ring_dev->rw_lock);
		tasklet_schedule(&ring_dev->rx_task);
	}

	return 0;
}

/*
 * PCIE's log callback in Interrupt context, can not use interface(kmalloc,
 * mutex) that may cause sleep. So handle it apart.
 * SDIO's log callback not in interrupt context, it in Kthread
 */
#ifdef CONFIG_WCN_PCIE
int mdbg_log_cb(int channel, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct mbuf_t *mbuf_node;
	int i;

	WCN_INFO("%s:type=0x%x,seq=0x%x, num=%d\n", __func__,
		 *(head->buf + 7), *((u32 *)(head->buf + 12)), num);

	if ((atomic_read(&ring_reg_flag)) == 0) {
		WCN_INFO("mdbg ring has do unreg, so discard it\n");
		return 0;
	}

	for (i = 0, mbuf_node = head; i < num; i++, mbuf_node = mbuf_node->next)
		mdbg_ring_write(ring_dev->ring, mbuf_node->buf, mbuf_node->len);

	sprdwcn_bus_push_list(channel, head, tail, num);
	wake_up_log_wait();

	return 0;
}
#endif

long int mdbg_send(char *buf, long int len, unsigned int subtype)
{
	long int sent_size = 0;

	WCN_DEBUG("BYTE MODE");
	__pm_stay_awake(&ring_dev->rw_wake_lock);
	sent_size = mdbg_comm_write(buf, len, subtype);
	__pm_relax(&ring_dev->rw_wake_lock);

	return sent_size;
}
EXPORT_SYMBOL_GPL(mdbg_send);

long int mdbg_receive(void *buf, long int len)
{
	return mdbg_ring_read(ring_dev->ring, buf, len);
}

int mdbg_tx_cb(int channel, struct mbuf_t *head,
	       struct mbuf_t *tail, int num)
{
#ifndef CONFIG_WCN_PCIE
	struct mbuf_t *mbuf_node;
	int i;

	mbuf_node = head;
	for (i = 0; i < num; i++, mbuf_node = mbuf_node->next) {
		kfree(mbuf_node->buf);
		mbuf_node->buf = NULL;
	}
#endif
	/* PCIe buf is witebuf[], not kmalloc, no need to free */
	sprdwcn_bus_list_free(channel, head, tail, num);

	return 0;
}

#ifdef CONFIG_WCN_PCIE
static struct dma_buf log_buf[LOG_BUF_NUM];

static int free_prepare_buf(struct dma_buf *dm)
{
	struct wcn_pcie_info *pcie_dev;

	pcie_dev = get_wcn_device_info();
	if (!pcie_dev) {
		WCN_ERR("%s:PCIE device link error\n", __func__);
		return -1;
	}

	if (dm->vir && dm->phy)
		dmfree(pcie_dev, dm);

	return 0;
}

int prepare_free_buf_for_log(int chn, int size, int num)
{
	int ret, i;
	struct mbuf_t *mbuf, *head, *tail;
	struct wcn_pcie_info *pcie_dev;

	pcie_dev = get_wcn_device_info();
	if (!pcie_dev) {
		WCN_ERR("%s:PCIE device link error\n", __func__);
		return -1;
	}
	ret = sprdwcn_bus_list_alloc(chn, &head, &tail, &num);
	if (ret != 0)
		return -1;
	for (i = 0, mbuf = head; i < num; i++) {
		ret = dmalloc(pcie_dev, &log_buf[i], size);
		if (ret != 0)
			return -1;
		mbuf->buf = (unsigned char *)(log_buf[i].vir);
		mbuf->phy = (unsigned long)(log_buf[i].phy);
		mbuf->len = log_buf[i].size;
		memset(mbuf->buf, 0x0, mbuf->len);
		mbuf = mbuf->next;
		WCN_DBG("dma_alloc_coherent(0x%x) vir=0x%lx, phy=0x%lx\n",
			 log_buf[i].size, log_buf[i].vir, log_buf[i].phy);
	}

	ret = sprdwcn_bus_push_list(chn, head, tail, num);

	return ret;
}
#endif

void mdbg_pt_ring_reg(void)
{
	WCN_INFO("%s\n", __func__);
	atomic_set(&ring_reg_flag, 0x1);
	sprdwcn_bus_chn_init(&mdbg_ringc_ops);
#ifdef CONFIG_WCN_PCIE
	prepare_free_buf_for_log(15, LOG_BUF_SIZE, LOG_BUF_NUM);
#endif
}

void mdbg_pt_ring_unreg(void)
{
#ifdef CONFIG_WCN_PCIE
	int i;
#endif

	WCN_INFO("%s\n", __func__);
	atomic_set(&ring_reg_flag, 0x0);
	sprdwcn_bus_chn_deinit(&mdbg_ringc_ops);
#ifdef CONFIG_WCN_PCIE
	for (i = 0; i < LOG_BUF_NUM; i++)
		free_prepare_buf(&log_buf[i]);
#endif
}

int mdbg_ring_init(void)
{
	int err = 0;

	ring_dev = kmalloc(sizeof(struct ring_device), GFP_KERNEL);
	if (!ring_dev)
		return -ENOMEM;

	ring_dev->ring = mdbg_ring_alloc(MDBG_RX_RING_SIZE);
	if (!(ring_dev->ring)) {
		WCN_ERR("Ring malloc error.");
		kfree(ring_dev);
		return -MDBG_ERR_MALLOC_FAIL;
	}

	wakeup_source_init(&ring_dev->rw_wake_lock, "mdbg_wake_lock");
	spin_lock_init(&ring_dev->rw_lock);
	mutex_init(&ring_dev->mdbg_read_mutex);
	INIT_LIST_HEAD(&ring_dev->rx_head);
	tasklet_init(&ring_dev->rx_task, mdbg_ring_rx_task,
		(unsigned long int)ring_dev);
	ring_dev->flag_smp = 0;
#ifndef CONFIG_WCN_PCIE
	mdbg_pt_ring_reg();
#endif
	WCN_INFO("success!");

	mdbg_dev->ring_dev = ring_dev;

	return err;
}

void mdbg_ring_remove(void)
{
	struct ring_rx_data *pos, *next;

	MDBG_FUNC_ENTERY;
#ifndef CONFIG_WCN_PCIE
	mdbg_pt_ring_unreg();
#endif
	wakeup_source_trash(&ring_dev->rw_wake_lock);
	mdbg_ring_destroy(ring_dev->ring);
	tasklet_kill(&ring_dev->rx_task);
	list_for_each_entry_safe(pos, next, &ring_dev->rx_head, entry) {
		list_del(&pos->entry);
		kfree(pos);
	}
	kfree(ring_dev);
	ring_dev = NULL;
}
