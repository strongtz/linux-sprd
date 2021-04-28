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


#ifndef _SIPA_H_
#define _SIPA_H_

#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/if_ether.h>

/**
 * enum sipa_term_type - names for the various IPA srouce / destination ID
 * 
 */
enum sipa_term_type {

	SIPA_TERM_USB = 0x1,
	SIPA_TERM_SDIO0 = 0x8,
	SIPA_TERM_SDIO1 = 0x9,
	SIPA_TERM_SDIO2 = 0xa,
	SIPA_TERM_SDIO3 = 0xb,
	SIPA_TERM_AP_IP = 0x18, /* cp -> ap, xfer ip packets  */
	SIPA_TERM_PCIE0 = 0x10,
	SIPA_TERM_PCIE1 = 0x11,
	SIPA_TERM_PCIE2 = 0x12,
	SIPA_TERM_PCIE_REMOTE_CTRL0 = 0x14,
	SIPA_TERM_PCIE_REMOTE_CTRL1 = 0x15,
	SIPA_TERM_PCIE_REMOTE_CTRL2 = 0x16,
	SIPA_TERM_PCIE_REMOTE_CTRL3 = 0x17,
	SIPA_TERM_AP_ETH = 0x19, /* peripheral -> ap, xfer ethernet packets */
	SIPA_TERM_PCIE_LOCAL_CTRL0 = 0x1c,
	SIPA_TERM_PCIE_LOCAL_CTRL1 = 0x1d,
	SIPA_TERM_PCIE_LOCAL_CTRL2 = 0x1e,
	SIPA_TERM_PCIE_LOCAL_CTRL3 = 0x1f,
	SIPA_TERM_CP0 = 0x4,
	SIPA_TERM_CP1 = 0x5,
	SIPA_TERM_VCP = 0x6,
	SIPA_TERM_WIFI = 0x2,
	SIPA_TERM_VAP0 = 0xc,
	SIPA_TERM_VAP1 = 0xd,
	SIPA_TERM_VAP2 = 0xe,

	SIPA_TERM__MAX = 0x20, /* max 5-bit register */
};

/**
 * enum sipa_ep_type - names for the various IPA end points
 * these ids used for rx / tx data with IPA
 * NOTE: one sipa EP may related to more than one sipa_term_types
 */
enum sipa_ep_id {
    SIPA_EP_USB = 0,
    SIPA_EP_AP_IP,
    SIPA_EP_AP_ETH,
    SIPA_EP_VCP,
    SIPA_EP_PCIE,
    SIPA_EP_PCIE_CTRL0,
    SIPA_EP_PCIE_CTRL1,
    SIPA_EP_PCIE_CTRL2,
    SIPA_EP_PCIE_CTRL3,
    SIPA_EP_REMOTE_PCIE,
    SIPA_EP_REMOTE_PCIE_CTRL0,
    SIPA_EP_REMOTE_PCIE_CTRL1,
    SIPA_EP_REMOTE_PCIE_CTRL2,
    SIPA_EP_REMOTE_PCIE_CTRL3,
    SIPA_EP_SDIO,
    SIPA_EP_WIFI,

    SIPA_EP_MAX
};

enum sipa_xfer_pkt_type {
    SIPA_PKT_ETH,
    SIPA_PKT_IP,
    SIPA_PKT_TYPE_MAX
};

/**
 * enum sipa_evt_type - type of event client callback is
 * invoked for on data path
 */
enum sipa_evt_type {
	SIPA_RECEIVE,
	SIPA_ENTER_FLOWCTRL,
	SIPA_LEAVE_FLOWCTRL,
	SIPA_ERROR,
};

typedef void (*sipa_notify_cb)(void *priv, enum sipa_evt_type evt,
		       unsigned long data);


/**
 * enum sipa_rm_res_id - IPA RM clients identifications
 *
 * Add new mapping to sipa_rm_prod_index() / sipa_rm_cons_index()
 * when adding new entry to this enum.
 */
enum sipa_rm_res_id {
	SIPA_RM_RES_PRODUCER = 0,
	SIPA_RM_RES_PROD_IPA = SIPA_RM_RES_PRODUCER,
	SIPA_RM_RES_PROD_REMOTE_IPA,
	SIPA_RM_RES_PROD_PAM_IPA,
	SIPA_RM_RES_PROD_PAM_U3,
	SIPA_RM_RES_PROD_PAM_SDIO,
	SIPA_RM_RES_PROD_PAM_WIFI,
	SIPA_RM_RES_PROD_PCIE3,
	SIPA_RM_RES_PROD_USB3,
	SIPA_RM_RES_PROD_SDSLAVE,
	SIPA_RM_RES_PROD_PCIE2,
	SIPA_RM_RES_PROD_PCIE_EP,
	SIPA_RM_RES_PROD_MINI_AP,
	SIPA_RM_RES_PROD_AP,
	SIPA_RM_RES_PROD_CP,
	SIPA_RM_RES_PROD_MAX,

	SIPA_RM_RES_CONS_WWAN_UL = SIPA_RM_RES_PROD_MAX,
	SIPA_RM_RES_CONS_WWAN_DL,
	SIPA_RM_RES_CONS_WLAN,
	SIPA_RM_RES_CONS_USB,
	SIPA_RM_RES_MAX
};

/**
 * enum sipa_rm_event - IPA RM events
 *
 * Indicate the resource state change
 */
enum sipa_rm_event {
	SIPA_RM_EVT_GRANTED,
	SIPA_RM_EVT_RELEASED
};


/**
 * enum sipa_voltage_level - IPA Voltage levels
 */
enum sipa_voltage_level {
	SIPA_VOLTAGE_UNSPECIFIED,
	SIPA_VOLTAGE_L1,
	SIPA_VOLTAGE_L2,
	SIPA_VOLTAGE_L3,
	SIPA_VOLTAGE_MAX,
};

typedef void (*sipa_rm_notify_cb)(void *user_data,
		enum sipa_rm_event event,
		unsigned long data);
/**
 * struct sipa_rm_register_params - information needed to
 *      register IPA RM client with IPA RM
 *
 * @user_data: IPA RM client provided information
 *		to be passed to notify_cb callback below
 * @notify_cb: callback which is called by resource
 *		to notify the IPA RM client about its state
 *		change IPA RM client is expected to perform non
 *		blocking operations only in notify_cb and
 *		release notification context as soon as
 *		possible.
 */
struct sipa_rm_register_params {
	void *user_data;
	sipa_rm_notify_cb notify_cb;
};

struct sipa_fifo_attrs {
    dma_addr_t dma_addr;
    u32 fifo_depth;
};

struct sipa_pam_attrs {
	u64 rx_fifo_base_addr;
	u64 tx_fifo_base_addr;

	u64 fifo_sts_addr;
	u32 fifo_depth;
};

enum sipa_nic_id
{
	SIPA_NIC_USB,
	SIPA_NIC_WIFI,
	SIPA_NIC_BB0,
	SIPA_NIC_BB1,
	SIPA_NIC_BB2,
	SIPA_NIC_BB3,
	SIPA_NIC_BB4,
	SIPA_NIC_BB5,
	SIPA_NIC_BB6,
	SIPA_NIC_BB7,
	SIPA_NIC_MAX
};

enum sipa_disconnect_id {
	SIPA_DISCONNECT_START,
	SIPA_DISCONNECT_END,
};

/**
 * struct sipa_comm_fifo_params - information needed to setup an IPA 
 * common FIFO, the tx  / rx are from the perspective of IPA
 */
struct sipa_comm_fifo_params {
	u32 intr_to_ap;

	u32 tx_intr_delay_us;
	u32 tx_intr_threshold;
	bool flowctrl_in_tx_full;
	u32 flow_ctrl_cfg;
	u32 flow_ctrl_irq_mode;
	u32 tx_enter_flowctrl_watermark;
	u32 tx_leave_flowctrl_watermark;
	u32 rx_enter_flowctrl_watermark;
	u32 rx_leave_flowctrl_watermark;

	u32 data_ptr_cnt;
	u32 buf_size;
	dma_addr_t data_ptr;
};

/**
 * struct sipa_connect_params - information needed to setup an IPA terminal
 */
struct sipa_connect_params {
	enum sipa_ep_id id;

	struct sipa_comm_fifo_params recv_param;
	struct sipa_comm_fifo_params send_param;

	sipa_notify_cb send_notify;
	void *send_priv; /* private data for sipa_notify_cb */

	sipa_notify_cb recv_notify;
	void *recv_priv; /* private data for sipa_notify_cb */
};

/**
 * struct sipa_ext_fifo_params - information needed to setup an IPA
 * external specified common FIFO, the tx  / rx are from the perspective of IPA
 */
struct sipa_ext_fifo_params {
	u32 rx_depth;
	u32 rx_fifo_pal;
	u32 rx_fifo_pah;
	void *rx_fifo_va;

	u32 tx_depth;
	u32 tx_fifo_pal;
	u32 tx_fifo_pah;
	void *tx_fifo_va;
};

/**
 * struct sipa_connect_params - information needed to setup an IPA terminal
 */
struct sipa_pcie_open_params {
	enum sipa_ep_id id;
	struct sipa_comm_fifo_params recv_param;
	struct sipa_comm_fifo_params send_param;

	sipa_notify_cb send_notify;
	void *send_priv; /* private data for sipa_notify_cb */

	sipa_notify_cb recv_notify;
	void *recv_priv; /* private data for sipa_notify_cb */

	struct sipa_ext_fifo_params ext_recv_param;
	struct sipa_ext_fifo_params ext_send_param;
};

/**
 * struct sipa_to_pam_info - information needed to setup an PAM for IPA
 */
struct sipa_to_pam_info {
	enum sipa_term_type term;
	struct sipa_pam_attrs dl_fifo;
	struct sipa_pam_attrs ul_fifo;
};

/**
 * struct sipa_hash_table - hash table for IPA
 */
struct sipa_hash_table {
	u32 depth;
	u64 tbl_phy_addr;
};

/**
 * struct ipa_rm_create_params - information needed to initialize
 *				the resource
 * @name: resource name
 * @floor_voltage: floor voltage needed for client to operate in maximum
 *		bandwidth.
 * @reg_params: register parameters, contains are ignored
 *		for consumer resource NULL should be provided
 *		for consumer resource
 * @request_resource: function which should be called to request resource,
 *			NULL should be provided for consumer resource
 * @release_resource: function which should be called to release resource,
 *			NULL should be provided for consumer resource
 *
 * IPA RM client is expected to perform non blocking operations only
 * in request_resource and release_resource functions and
 * release notification context as soon as possible.
 */
struct sipa_rm_create_params {
	enum sipa_rm_res_id name;
	enum sipa_voltage_level floor_voltage;
	struct sipa_rm_register_params reg_params;
	int (*request_resource)(void *);
	int (*release_resource)(void *);
};

/**
 * struct sipa_rm_perf_profile - information regarding IPA RM client performance
 * profile
 *
 * @max_bandwidth_mbps: maximum bandwidth need of the client in Mbps
 */
struct sipa_rm_perf_profile {
	u32 max_supported_bandwidth_mbps;
};

int sipa_register_ipa_ready_cb(void (*sipa_ready_cb)(void *user_data),
			      void *user_data);

bool sipa_rm_is_initialized(void);

int sipa_set_enabled(bool enable);

/*
 * IPA terminal managment
 */
int sipa_get_ep_info(enum sipa_ep_id id,
    struct sipa_to_pam_info *out);

int sipa_pam_connect(const struct sipa_connect_params *in);

int sipa_sw_connect(const struct sipa_connect_params *in);

int sipa_ext_open_pcie(struct sipa_pcie_open_params *in);

int sipa_disconnect(enum sipa_ep_id ep, enum sipa_disconnect_id stage);

int sipa_enable_receive(enum sipa_ep_id ep_id, bool enabled);


/*
 * SIPA NIC interface
 */

int sipa_nic_open(enum sipa_term_type src, int netid,
        sipa_notify_cb cb, void* priv);

void sipa_nic_close(enum sipa_nic_id nic_id);

int sipa_nic_tx(enum sipa_nic_id nic_id, enum sipa_term_type dst,
        int netid, struct sk_buff *skb);

int sipa_nic_rx(enum sipa_nic_id nic_id, struct sk_buff **out_skb);

int sipa_nic_trigger_flow_ctrl_work(enum sipa_nic_id, int err);
/*
 * IPA hash table managment
 */

/**
 * sipa_swap_hash_table() - swap old hash table and new one. 
 * @term: specified term
 * @net_id: specified net id, -1 for all net id
 * @out_skb: received packet
 *
 */
int sipa_swap_hash_table(struct sipa_hash_table *new_tbl,
		struct sipa_hash_table *old_tbl);

/*
 * Resource manager
 */
int sipa_rm_create_resource(struct sipa_rm_create_params *create_params);

int sipa_rm_delete_resource(enum sipa_rm_res_id res_id);

int sipa_rm_register(enum sipa_rm_res_id res_id,
			struct sipa_rm_register_params *reg_params);

int sipa_rm_deregister(enum sipa_rm_res_id res_id,
			struct sipa_rm_register_params *reg_params);

/*
 * @res_id: name of dependent producer resource
 * @depends_on_res: name of its dependency consumer
 */
int sipa_rm_add_dependency(enum sipa_rm_res_id cons,
			enum sipa_rm_res_id prod);

int sipa_rm_add_dependency_sync(enum sipa_rm_res_id cons,
			enum sipa_rm_res_id prod);

int sipa_rm_delete_dependency(enum sipa_rm_res_id cons,
			enum sipa_rm_res_id prod);

int sipa_rm_request_resource(enum sipa_rm_res_id res_id);

int sipa_rm_release_resource(enum sipa_rm_res_id res_id);

int sipa_rm_notify_completion(enum sipa_rm_event event,
		enum sipa_rm_res_id res_id);

int sipa_rm_inactivity_timer_init(enum sipa_rm_res_id res_id,
				 unsigned long ms);

int sipa_rm_inactivity_timer_destroy(enum sipa_rm_res_id res_id);

int sipa_rm_inactivity_timer_request_resource(
				enum sipa_rm_res_id res_id);

int sipa_rm_inactivity_timer_release_resource(
				enum sipa_rm_res_id res_id);

/*
 * SIPA USB RM interface
 */

int sipa_rm_usb_cons_init(void);

void sipa_rm_usb_cons_deinit(void);

int sipa_rm_set_usb_eth_up(void);

void sipa_rm_set_usb_eth_down(void);

int sipa_rm_enable_usb_tether(void);

#endif /* _SIPA_H_ */
