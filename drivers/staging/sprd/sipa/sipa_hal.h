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


#ifndef _SIPA_HAL_H_
#define _SIPA_HAL_H_

#include <linux/sipa.h>

#define SIPA_CMN_FIFO_TX_CTRL_OFFSET 0xc
#define SIPA_CMN_FIFO_RX_CTRL_OFFSET 0x0

#define SIPA_LOCAL	0
#define SIPA_REMOTE	1

enum fifo_status {
	fifo_empty	= 1,
	fifo_full	= 2,
	fifo_status_end,
};

enum sipa_hal_evt_type {
	SIPA_HAL_TX_FIFO_THRESHOLD_SW	= 0x00400000,
	SIPA_HAL_EXIT_FLOW_CTRL		= 0x00100000,
	SIPA_HAL_ENTER_FLOW_CTRL	= 0x00080000,
	SIPA_HAL_TXFIFO_FULL_INT	= 0x00040000,
	SIPA_HAL_TXFIFO_OVERFLOW	= 0x00020000,
	SIPA_HAL_ERRORCODE_IN_TX_FIFO	= 0x00010000,
	SIPA_HAL_INTR_BIT		= 0x00008000,
	SIPA_HAL_THRESHOLD		= 0x00004000,
	SIPA_HAL_DELAY_TIMER		= 0x00002000,
	SIPA_HAL_DROP_PACKT_OCCUR	= 0x00001000,
	SIPA_HAL_ERROR			= 0x0,
};

enum sipa_sys_type {
	IPA_PCIE2		= 0x00000100,
	IPA_PCIE3		= 0x00000080,
	IPA_PAM_WIFI		= 0x00000040,
	IPA_PAM_IPA		= 0x00000020,
	IPA_PAM_USB		= 0x00000010,
	IPA_IPA			= 0x00000008,
	IPA_USB_REF		= 0x00000004,
	IPA_USB_SUSPEND		= 0x00000002,
	IPA_USB			= 0x00000001,
};

enum sipa_cmn_fifo_index {
	SIPA_FIFO_USB_UL = 0, /*PAM_U3 -> IPA*/
	SIPA_FIFO_SDIO_UL,
	SIPA_FIFO_AP_IP_UL, /*AP -> IPA*/
	SIPA_FIFO_PCIE_UL,
	/* UL PCIE 0 ~ 3*/
	SIPA_FIFO_REMOTE_PCIE_CTRL0_UL,
	SIPA_FIFO_REMOTE_PCIE_CTRL1_UL,
	SIPA_FIFO_REMOTE_PCIE_CTRL2_UL,
	SIPA_FIFO_REMOTE_PCIE_CTRL3_UL,
	SIPA_FIFO_AP_ETH_DL, /*ap_dev -> IPA*/
	/* DL mAP PCIE 0 ~ 3*/
	SIPA_FIFO_LOCAL_PCIE_CTRL0_DL,
	SIPA_FIFO_LOCAL_PCIE_CTRL1_DL,
	SIPA_FIFO_LOCAL_PCIE_CTRL2_DL,
	SIPA_FIFO_LOCAL_PCIE_CTRL3_DL,
	SIPA_FIFO_WIFI_UL,
	SIPA_FIFO_CP_DL, /*PAM_IPA -> IPA*/
	SIPA_FIFO_USB_DL, /*IPA -> PAM_U3*/
	SIPA_FIFO_SDIO_DL,
	SIPA_FIFO_AP_IP_DL, /*IPA -> AP*/
	SIPA_FIFO_PCIE_DL,
	/* DL PCIE 0 ~ 3*/
	SIPA_FIFO_REMOTE_PCIE_CTRL0_DL,
	SIPA_FIFO_REMOTE_PCIE_CTRL1_DL,
	SIPA_FIFO_REMOTE_PCIE_CTRL2_DL,
	SIPA_FIFO_REMOTE_PCIE_CTRL3_DL,
	SIPA_FIFO_AP_ETH_UL, /*IPA -> ap_dev*/
	/* UL mAP PCIE 0 ~ 3*/
	SIPA_FIFO_LOCAL_PCIE_CTRL0_UL,
	SIPA_FIFO_LOCAL_PCIE_CTRL1_UL,
	SIPA_FIFO_LOCAL_PCIE_CTRL2_UL,
	SIPA_FIFO_LOCAL_PCIE_CTRL3_UL,
	SIPA_FIFO_WIFI_DL,
	SIPA_FIFO_CP_UL, /*IPA -> PAM_IPA*/
	SIPA_FIFO_MAX
};

struct sipa_interrupt_table_tag {
	enum sipa_cmn_fifo_index id;
	u32 int_owner;
};


typedef void *sipa_hal_hdl;

typedef void (*sipa_hal_notify_cb)(void *priv,
				   enum sipa_hal_evt_type evt,
				   unsigned long data);

struct sipa_hal_fifo_item {
	dma_addr_t addr;
	u32 len;
	u32 offset;
	u8 netid;
	u8 dst;
	u8 src;
	u8 intr;
	u32 err_code;
	u32 reserved;
};

struct sipa_reg_res_tag {
	void __iomem *glb_base;
	void *iram_base;
	phys_addr_t glb_phy;
	phys_addr_t iram_phy;
	resource_size_t glb_size;
	resource_size_t iram_size;

	u64 iram_allocated_size;
};

struct sipa_plat_drv_cfg;

sipa_hal_hdl sipa_hal_init(struct device *dev, struct sipa_plat_drv_cfg *cfg);

int sipa_hal_set_enabled(struct sipa_plat_drv_cfg *cfg, bool enable);

int sipa_force_wakeup(struct sipa_plat_drv_cfg *cfg, bool wake);

int sipa_open_common_fifo(sipa_hal_hdl hdl,
			  enum sipa_cmn_fifo_index fifo,
			  struct sipa_comm_fifo_params *attr,
			  struct sipa_ext_fifo_params *ext_attr,
			  bool force_sw_intr,
			  sipa_hal_notify_cb cb,
			  void *priv);

/*
 * stop : true : stop recv false : start receive
 */
int sipa_hal_cmn_fifo_set_receive(sipa_hal_hdl hdl,
				  enum sipa_cmn_fifo_index fifo,
				  bool stop);

int sipa_hal_init_set_tx_fifo(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo,
			      struct sipa_hal_fifo_item *items,
			      u32 num);

int sipa_hal_get_tx_fifo_item(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo,
			      struct sipa_hal_fifo_item *item);

u32 sipa_hal_get_tx_fifo_items(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id);

int sipa_hal_conversion_node_to_item(sipa_hal_hdl hdl,
				     enum sipa_cmn_fifo_index fifo_id,
				     struct sipa_hal_fifo_item *item);

int sipa_hal_recv_conversion_node_to_item(sipa_hal_hdl hdl,
					  enum sipa_cmn_fifo_index fifo_id,
					  struct sipa_hal_fifo_item *item,
					  u32 index);

int sipa_hal_put_rx_fifo_item(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo,
			      struct sipa_hal_fifo_item *item);

int sipa_hal_put_rx_fifo_items(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id);

int sipa_hal_cache_rx_fifo_item(sipa_hal_hdl hdl,
				enum sipa_cmn_fifo_index fifo_id,
				struct sipa_hal_fifo_item *item);

bool sipa_hal_is_rx_fifo_empty(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo);

bool sipa_hal_is_rx_fifo_full(sipa_hal_hdl hdl,
			      enum sipa_cmn_fifo_index fifo);

bool sipa_hal_is_tx_fifo_empty(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo);

int sipa_hal_init_pam_param(enum sipa_cmn_fifo_index dl_idx,
			    enum sipa_cmn_fifo_index ul_idx,
			    struct sipa_to_pam_info *out);

int sipa_hal_get_cmn_fifo_filled_depth(sipa_hal_hdl hdl,
				       enum sipa_cmn_fifo_index fifo_id,
				       u32 *rx_filled, u32 *tx_filled);

int sipa_hal_enable_wiap_dma(sipa_hal_hdl hdl, bool dma);

int sipa_tft_mode_init(sipa_hal_hdl hdl);

int sipa_close_common_fifo(sipa_hal_hdl hdl,
			   enum sipa_cmn_fifo_index fifo);

int sipa_hal_reclaim_unuse_node(sipa_hal_hdl hdl,
				enum sipa_cmn_fifo_index fifo_id);

int sipa_resume_common_fifo(sipa_hal_hdl hdl, struct sipa_plat_drv_cfg *cfg);

bool sipa_hal_check_rx_priv_fifo_is_empty(sipa_hal_hdl hdl,
					  enum sipa_cmn_fifo_index fifo_id);

bool sipa_hal_check_rx_priv_fifo_is_full(sipa_hal_hdl hdl,
					 enum sipa_cmn_fifo_index fifo_id);

int sipa_hal_set_tx_fifo_rptr(sipa_hal_hdl hdl, enum sipa_cmn_fifo_index id,
			      u32 tmp);

bool sipa_hal_bk_fifo_node(sipa_hal_hdl hdl,
			   enum sipa_cmn_fifo_index fifo_id);

bool sipa_hal_resume_fifo_node(sipa_hal_hdl hdl,
			       enum sipa_cmn_fifo_index fifo_id);

bool sipa_hal_cmn_fifo_open_status(sipa_hal_hdl hdl,
				   enum sipa_cmn_fifo_index fifo_id);

void sipa_resume_glb_reg_cfg(struct sipa_plat_drv_cfg *cfg);

bool sipa_hal_check_send_cmn_fifo_com(sipa_hal_hdl hdl,
				      enum sipa_cmn_fifo_index fifo_id);

int sipa_hal_ctrl_action(u32 enable);
bool sipa_hal_get_pause_status(void);
bool sipa_hal_get_resume_status(void);
#endif /* !_SIPA_HAL_H_ */
