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
#ifndef _SIPA_PRIV_H_
#define _SIPA_PRIV_H_
#include <linux/skbuff.h>
#include <linux/sipa.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/regmap.h>
#include "sipa_hal.h"

#define SIPA_DEF_OFFSET	64

#define SIPA_RECV_EVT (SIPA_HAL_INTR_BIT | \
			SIPA_HAL_TX_FIFO_THRESHOLD_SW | SIPA_HAL_DELAY_TIMER)

#define SIPA_RECV_WARN_EVT (SIPA_HAL_TXFIFO_FULL_INT | SIPA_HAL_TXFIFO_OVERFLOW)

#define SIPA_WWAN_CONS_TIMER 100

enum sipa_nic_status_e {
	NIC_OPEN,
	NIC_CLOSE,
};

enum sipa_suspend_stage_e {
	SIPA_THREAD_SUSPEND = BIT(0),
	SIPA_EP_SUSPEND = BIT(1),
	SIPA_BACKUP_SUSPEND = BIT(2),
	SIPA_EB_SUSPEND = BIT(3),
	SIPA_FORCE_SUSPEND = BIT(4),
	SIPA_ACTION_SUSPEND = BIT(5),
};

#define SIPA_SUSPEND_MASK (SIPA_THREAD_SUSPEND | \
			   SIPA_EP_SUSPEND | \
			   SIPA_BACKUP_SUSPEND | \
			   SIPA_EB_SUSPEND | \
			   SIPA_FORCE_SUSPEND | \
			   SIPA_ACTION_SUSPEND)

enum flow_ctrl_mode_e {
	flow_ctrl_rx_empty,
	flow_ctrl_tx_full,
	flow_ctrl_rx_empty_and_tx_full,
};

enum flow_ctrl_irq_e {
	enter_flow_ctrl,
	exit_flow_ctrl,
	enter_exit_flow_ctrl,
};

struct sipa_common_fifo_info {
	const char *tx_fifo;
	const char *rx_fifo;

	enum sipa_ep_id relate_ep;
	enum sipa_term_type src_id;
	enum sipa_term_type dst_id;

	/* centered on IPA*/
	bool is_to_ipa;
	bool is_pam;
};

struct sipa_common_fifo {
	enum sipa_cmn_fifo_index idx;

	struct sipa_fifo_attrs tx_fifo;
	struct sipa_fifo_attrs rx_fifo;

	enum sipa_term_type dst_id;
	enum sipa_term_type src_id;

	bool is_receiver;
	bool is_pam;
};

struct sipa_fifo_cfg {
	bool in_iram;
	u32 fifo_size;
};

struct sipa_common_fifo_cfg {
	/*centered on CPU/PAM*/
	bool is_recv;
	bool is_pam;

	u32 src;
	u32 dst;

	struct sipa_fifo_cfg tx_fifo;
	struct sipa_fifo_cfg rx_fifo;
};

struct ipa_register_map {
	char *name;
	u32 offset;
	u32 size;
};

struct sipa_hw_data {
	const u32 ahb_regnum;
	const struct ipa_register_map *ahb_reg;
	const bool standalone_subsys;
};

struct sipa_plat_drv_cfg {
	const char *name;

	dev_t dev_num;
	struct class *class;
	struct device *dev;
	struct cdev cdev;

	bool standalone_subsys;
	spinlock_t enable_lock;
	u32 enable_cnt;
	struct regmap *sys_regmap;
	u32 enable_reg;
	u32 enable_mask;
	struct regulator *vpower;

	int ipa_intr;

	u32 ctrl_tx_intr0;
	u32 ctrl_flowctrl_intr0;
	u32 ctrl_tx_intr1;
	u32 ctrl_flowctrl_intr1;
	u32 ctrl_tx_intr2;
	u32 ctrl_flowctrl_intr2;
	u32 ctrl_tx_intr3;
	u32 ctrl_flowctrl_intr3;

	int is_bypass;
	bool tft_mode;
	bool wiap_ul_dma;
	bool need_through_pcie;

	u32 fifo_iram_size;
	u32 fifo_ddr_size;

	phys_addr_t glb_phy;
	resource_size_t glb_size;
	phys_addr_t iram_phy;
	resource_size_t iram_size;

	u32 suspend_cnt;
	u32 resume_cnt;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
	const void *debugfs_data;
#endif
	struct sipa_common_fifo_cfg common_fifo_cfg[SIPA_FIFO_MAX];
};

struct sipa_endpoint {
	enum sipa_ep_id id;

	struct sipa_context *sipa_ctx;

	/* Centered on CPU/PAM */
	struct sipa_common_fifo send_fifo;
	struct sipa_common_fifo recv_fifo;

	struct sipa_comm_fifo_params send_fifo_param;
	struct sipa_comm_fifo_params recv_fifo_param;

	sipa_notify_cb send_notify;
	sipa_notify_cb recv_notify;

	void *send_priv; /* private data for sipa_notify_cb */
	void *recv_priv;

	bool connected;
	bool suspended;
};

struct sipa_context {
	sipa_hal_hdl hdl;
	struct device *pdev;
	int is_remote;
	int bypass_mode;
};

typedef void (*sipa_sender_free_node)(void *node);

struct sipa_sender {
	struct sipa_context *ctx;
	struct sipa_endpoint *ep;
	u32 left_cnt;
	spinlock_t lock;
	struct list_head nic_list;
};

struct sipa_skb_dma_addr_node {
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct list_head list;
};

struct sipa_skb_sender {
	struct sipa_context *ctx;
	struct sipa_endpoint *ep;
	enum sipa_xfer_pkt_type type;
	atomic_t left_cnt;
	spinlock_t nic_lock;
	spinlock_t send_lock;
	struct list_head nic_list;
	struct list_head sending_list;
	struct list_head pair_free_list;
	struct sipa_skb_dma_addr_node *pair_cache;

	bool free_notify_net;
	bool send_notify_net;

	wait_queue_head_t send_waitq;
	wait_queue_head_t free_waitq;

	struct task_struct *free_thread;
	struct task_struct *send_thread;

	bool init_flag;
	u32 no_mem_cnt;
	u32 no_free_cnt;
	u32 enter_flow_ctrl_cnt;
	u32 exit_flow_ctrl_cnt;
};

struct sipa_receiver {
	struct sipa_context *ctx;
	struct sipa_endpoint *ep;
	struct mutex mutex;
	struct task_struct *thread;
};

struct sipa_skb_dma_addr_pair {
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct list_head list;
};

struct sipa_skb_array {

	struct sipa_skb_dma_addr_pair *array;
	u32 rp;
	u32 wp;
	u32 depth;
};

typedef bool (*sipa_check_send_completed)(void *priv);

struct sipa_nic_cons_res {
	bool initied;
	enum sipa_rm_res_id cons;
	sipa_check_send_completed chk_func;
	void *chk_priv;
	spinlock_t lock;
	struct delayed_work work;
	bool resource_requested;
	bool reschedule_work;
	bool release_in_progress;
	bool need_request;
	bool request_in_progress;
	unsigned long jiffies;
};

struct sipa_nic {
	enum sipa_nic_id nic_id;
	struct sipa_endpoint *send_ep;
	struct sk_buff_head	rx_skb_q;
	int need_notify;
	u32 src_mask;
	int netid;
	struct list_head list;
	sipa_notify_cb cb;
	void *cb_priv;
	atomic_t status;
	bool flow_ctrl_status;
	struct sipa_nic_cons_res rm_res;
};

struct sipa_skb_receiver {
	struct sipa_context *ctx;
	struct sipa_endpoint *ep;
	u32 rsvd;
	struct sipa_skb_array recv_array;
	wait_queue_head_t recv_waitq;
	wait_queue_head_t fill_recv_waitq;
	spinlock_t lock;
	u32 nic_cnt;
	atomic_t need_fill_cnt;
	struct sipa_nic *nic_array[SIPA_NIC_MAX];

	struct task_struct *fill_thread;
	struct task_struct *thread;

	bool init_flag;
	u32 tx_danger_cnt;
	u32 rx_danger_cnt;
};

struct sipa_control {
	/* IPA hw contxt */
	struct sipa_context *ctx;
	struct sipa_endpoint *eps[SIPA_EP_MAX];

	struct sipa_plat_drv_cfg params_cfg;

	/* avoid pam connect and power_wq race */
	struct mutex resume_lock;
	struct work_struct flow_ctrl_work;

	/* ipa low power*/
	bool power_flag;
	struct delayed_work suspend_work;
	struct delayed_work resume_work;
	struct workqueue_struct *power_wq;

	/* IPA NIC interface */
	struct sipa_nic *nic[SIPA_NIC_MAX];

	/* sender & receiver */
	struct sipa_skb_sender *sender[SIPA_PKT_TYPE_MAX];
	struct sipa_skb_receiver *receiver[SIPA_PKT_TYPE_MAX];

	/* usb rm */
	struct completion usb_rm_comp;
	struct sipa_rm_create_params ipa_rm;

	u32 suspend_stage;
};

int create_sipa_skb_sender(struct sipa_context *ipa,
			   struct sipa_endpoint *ep,
			   enum sipa_xfer_pkt_type type,
			   struct sipa_skb_sender **sender_pp);

void destroy_sipa_skb_sender(struct sipa_skb_sender *sender);

u32 sipa_get_suspend_status(void);

struct sipa_control *sipa_get_ctrl_pointer(void);

int sipa_skb_sender_send_data(struct sipa_skb_sender *sender,
			      struct sk_buff *skb,
			      enum sipa_term_type dst,
			      u8 netid);

bool sipa_skb_sender_check_send_complete(struct sipa_skb_sender *sender);

void sipa_skb_sender_add_nic(struct sipa_skb_sender *sender,
			     struct sipa_nic *nic);

void sipa_skb_sender_remove_nic(struct sipa_skb_sender *sender,
				struct sipa_nic *nic);

int create_sipa_skb_receiver(struct sipa_context *ipa,
			     struct sipa_endpoint *ep,
			     struct sipa_skb_receiver **receiver_pp);

void destroy_sipa_skb_receiver(struct sipa_skb_receiver *receiver);

void sipa_receiver_add_nic(struct sipa_skb_receiver *receiver,
			   struct sipa_nic *nic);

void sipa_nic_try_notify_recv(struct sipa_nic *nic);

void sipa_nic_notify_evt(struct sipa_nic *nic, enum sipa_evt_type evt);

void sipa_nic_push_skb(struct sipa_nic *nic, struct sk_buff *skb);

int sipa_nic_rx_has_data(enum sipa_nic_id nic_id);

int sipa_sender_prepare_suspend(struct sipa_skb_sender *sender);
int sipa_sender_prepare_resume(struct sipa_skb_sender *sender);

int sipa_receiver_prepare_suspend(struct sipa_skb_receiver *receiver);

int sipa_receiver_prepare_resume(struct sipa_skb_receiver *receiver);

void sipa_fill_free_node(struct sipa_skb_receiver *receiver, u32 cnt);
#endif /* _SIPA_PRIV_H_ */
