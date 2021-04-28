#ifndef _SIPA_HAL_PRIV_H_
#define _SIPA_HAL_PRIV_H_

#include <linux/ioport.h>
#include <linux/kfifo.h>
#include "sipa_hal.h"

#define TRUE	1
#define FALSE	0

#define IPA_TOSTR1(x) #x
#define IPA_TOSTR(x) IPA_TOSTR1(x)

#define IPA_PASTE1(x, y) x ## y
#define IPA_PASTE(x, y) IPA_PASTE1(x, y)

#define IPA_GET_LOW32(val) ((u32)(val & 0x00000000FFFFFFFF))
#define IPA_GET_HIGH32(val) ((u32)((val >> 32) & 0x00000000FFFFFFFF))
#define IPA_STI_64BIT(l_val, h_val) ((u64)(l_val | ((u64)h_val << 32)))

#define SIPA_FIFO_REG_SIZE	0x80

struct sipa_node_description_tag {
	/*soft need to set*/
	u64 address : 40;
	/*soft need to set*/
	u32 length : 20;
	/*soft need to set*/
	u16 offset : 12;
	/*soft need to set*/
	u8	net_id;
	/*soft need to set*/
	u8	src : 5;
	/*soft need to set*/
	u8	dst : 5;
	u8	prio : 3;
	u8	bear_id : 7;
	/*soft need to set*/
	u8	intr : 1;
	/*soft need to set*/
	u8	indx : 1;
	u8	err_code : 4;
	u32 reserved : 22;
} __attribute__((__packed__));

struct sipa_cmn_fifo_tag {
	u32 depth;
	u32 wr;
	u32 rd;
	u32 size;
	bool in_iram;

	u32 fifo_base_addr_l;
	u32 fifo_base_addr_h;

	void *virtual_addr;
};

struct sipa_common_fifo_cfg_tag {
	const char *fifo_name;

	enum sipa_cmn_fifo_index fifo_id;
	enum sipa_cmn_fifo_index fifo_id_dst;

	bool is_recv;
	bool is_pam;

	u32 state;
	u32 pending;
	u32 dst;
	u32 cur;

	u64 fifo_phy_addr;

	void *priv;
	void __iomem *fifo_reg_base;

	struct kfifo rx_priv_fifo;
	struct kfifo tx_priv_fifo;
	struct sipa_cmn_fifo_tag rx_fifo;
	struct sipa_cmn_fifo_tag tx_fifo;

	sipa_hal_notify_cb fifo_irq_callback;
};

struct sipa_hal_fifo_ops {
	u32 (*open)(enum sipa_cmn_fifo_index id,
		    struct sipa_common_fifo_cfg_tag *cfg_base,
		    void *cookie);
	u32 (*close)(enum sipa_cmn_fifo_index id,
		     struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*reset)(enum sipa_cmn_fifo_index id,
		     struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*rx_fill)(enum sipa_cmn_fifo_index id,
		       struct sipa_common_fifo_cfg_tag *cfg_base,
		       struct sipa_node_description_tag *node,
		       u32 num);
	u32 (*tx_fill)(enum sipa_cmn_fifo_index id,
		       struct sipa_common_fifo_cfg_tag *cfg_base,
		       struct sipa_node_description_tag *node,
		       u32 num);
	u32 (*set_rx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base,
			    u32 depth);
	u32 (*set_tx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base,
			    u32 depth);
	u32 (*get_rx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*get_tx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*set_rx_tx_fifo_wr_rd_ptr)(enum sipa_cmn_fifo_index id,
					struct sipa_common_fifo_cfg_tag *cfg_base,
					u32 rx_rd, u32 rx_wr,
					u32 tx_rd, u32 tx_wr);
	bool (*set_tx_fifo_wptr)(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base,
				 u32 wptr);
	bool (*set_rx_fifo_wptr)(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base,
				 u32 wptr);
	int (*update_tx_fifo_rptr)(enum sipa_cmn_fifo_index id,
				   struct sipa_common_fifo_cfg_tag *cfg_base,
				   u32 tx_rd);
	struct sipa_node_description_tag *
		(*get_tx_fifo_node)(enum sipa_cmn_fifo_index id,
				    struct sipa_common_fifo_cfg_tag *cfg_base,
				    u32 index);
	u32 (*set_interrupt_error_code)(enum sipa_cmn_fifo_index id,
					struct sipa_common_fifo_cfg_tag *cfg_base,
					u32 enable, sipa_hal_notify_cb cb);
	u32 (*set_interrupt_drop_packet)(enum sipa_cmn_fifo_index id,
					 struct sipa_common_fifo_cfg_tag *
					 cfg_base, u32 enable,
					 sipa_hal_notify_cb cb);
	u32 (*set_interrupt_timeout)(enum sipa_cmn_fifo_index id,
				     struct sipa_common_fifo_cfg_tag *cfg_base,
				     u32 enable, u32 time,
				     sipa_hal_notify_cb cb);
	u32 (*set_hw_interrupt_timeout)(enum sipa_cmn_fifo_index id,
					struct sipa_common_fifo_cfg_tag *cfg_base,
					u32 enable, u32 time,
					sipa_hal_notify_cb cb);
	u32 (*set_hw_interrupt_threshold)(enum sipa_cmn_fifo_index id,
					  struct sipa_common_fifo_cfg_tag *cfg_base,
					  u32 enable, u32 cnt,
					  sipa_hal_notify_cb cb);
	u32 (*set_interrupt_threshold)(enum sipa_cmn_fifo_index id,
				       struct sipa_common_fifo_cfg_tag *cfg_base,
				       u32 enable, u32 cnt,
				       sipa_hal_notify_cb cb);
	u32 (*set_interrupt_intr)(enum sipa_cmn_fifo_index id,
				  struct sipa_common_fifo_cfg_tag *cfg_base,
				  u32 enable, sipa_hal_notify_cb cb);
	u32 (*set_interrupt_txfifo_overflow)(enum sipa_cmn_fifo_index id,
					     struct sipa_common_fifo_cfg_tag *cfg_base,
					     u32 enable, sipa_hal_notify_cb cb);
	u32 (*set_interrupt_txfifo_full)(enum sipa_cmn_fifo_index id,
					 struct sipa_common_fifo_cfg_tag *cfg_base,
					 u32 enable, sipa_hal_notify_cb cb);
	u32 (*set_cur_dst_term)(enum sipa_cmn_fifo_index id,
				struct sipa_common_fifo_cfg_tag *cfg_base,
				u32 cur, u32 dst);
	u32 (*enable_remote_flowctrl_interrupt)(enum sipa_cmn_fifo_index id,
						struct sipa_common_fifo_cfg_tag *cfg_base,
						u32 work_mode,
						u32 tx_entry_watermark,
						u32 tx_exit_watermark,
						u32 rx_entry_watermark,
						u32 rx_exit_watermark);
	u32 (*enable_local_flowctrl_interrupt)(enum sipa_cmn_fifo_index id,
					       struct sipa_common_fifo_cfg_tag *cfg_base,
					       u32 enable, u32 irq_mode,
					       sipa_hal_notify_cb cb);
	u32 (*recv_node_from_tx_fifo)(struct device *dev,
				      enum sipa_cmn_fifo_index id,
				      struct sipa_common_fifo_cfg_tag *cfg_base,
				      struct sipa_node_description_tag *node,
				      u32 force_intr, u32 num);
	u32 (*get_node_from_rx_fifo)(struct device *dev,
				     enum sipa_cmn_fifo_index id,
				     struct sipa_common_fifo_cfg_tag *cfg_base,
				     struct sipa_node_description_tag *node,
				     u32 force_intr, u32 num);
	u32 (*get_left_cnt)(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*put_node_to_tx_fifo)(struct device *dev,
				   enum sipa_cmn_fifo_index id,
				   struct sipa_common_fifo_cfg_tag *cfg_base,
				   struct sipa_node_description_tag *node,
				   u32 force_intr, u32 num);
	u32 (*put_node_to_rx_fifo)(struct device *dev,
				   enum sipa_cmn_fifo_index id,
				   struct sipa_common_fifo_cfg_tag *cfg_base,
				   struct sipa_node_description_tag *node,
				   u32 force_intr, u32 num);
	u32 (*get_rx_fifo_wr_rd_ptr)(enum sipa_cmn_fifo_index id,
				     struct sipa_common_fifo_cfg_tag *cfg_base,
				     u32 *wr, u32 *rd);
	u32 (*get_tx_fifo_wr_rd_ptr)(enum sipa_cmn_fifo_index id,
				     struct sipa_common_fifo_cfg_tag *cfg_base,
				     u32 *wr, u32 *rd);
	u32 (*get_filled_depth)(enum sipa_cmn_fifo_index id,
				struct sipa_common_fifo_cfg_tag *cfg_base,
				u32 *rx_filled, u32 *tx_filled);
	u32 (*get_rx_full_status)(enum sipa_cmn_fifo_index id,
				  struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*get_rx_empty_status)(enum sipa_cmn_fifo_index id,
				   struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*get_tx_full_status)(enum sipa_cmn_fifo_index id,
				  struct sipa_common_fifo_cfg_tag *cfg_base);
	u32 (*get_tx_empty_status)(enum sipa_cmn_fifo_index id,
				   struct sipa_common_fifo_cfg_tag *cfg_base);

	u32 (*ctrl_receive)(enum sipa_cmn_fifo_index id,
			    struct sipa_common_fifo_cfg_tag *cfg_base,
			    bool stop);
	int (*reclaim_node_desc)(enum sipa_cmn_fifo_index id,
				 struct sipa_common_fifo_cfg_tag *cfg_base);
};

struct sipa_hal_global_ops {
	u32 (*set_mode)(void __iomem *reg_base, u32 is_bypass);
	u32 (*get_int_status)(void __iomem *reg_base);
	u32 (*ctrl_ipa_action)(void __iomem *reg_base, u32 enable);
	u32 (*get_hw_ready_to_check_sts)(void __iomem *reg_base);
	u32 (*hash_table_switch)(void __iomem *reg_base,
				 u32 addr_l, u32 addr_h, u32 len);
	u32 (*get_hash_table)(void __iomem *reg_base,
			      u32 *addr_l, u32 *addr_h, u32 *len);
	u32 (*map_interrupt_src_en)(void __iomem *reg_base,
				    u32 enable, u32 mask);
	u32 (*clear_internal_fifo)(void __iomem *reg_base,
				   u32 clr_bit);
	u32 (*set_flow_ctrl_to_src_blk)(void __iomem *reg_base,
					u32 dst, u32 src);
	u32 (*get_flow_ctrl_to_src_sts)(void __iomem *reg_base,
					u32 dst, u32 src);
	u32 (*set_axi_mst_chn_priority)(void __iomem *reg_base,
					u32 chan, u32 prio);
	u32 (*get_timestamp)(void __iomem *reg_base);
	u32 (*set_force_to_ap)(void __iomem *reg_base,
			       u32 enable, u32 flag);
	u32 (*enable_cp_through_pcie)(void __iomem *reg_base,
				      u32 enable);
	u32 (*enable_wiap_ul_dma)(void __iomem *reg_base,
				  u32 enable);
	u32 (*enable_def_flowctrl_to_src_blk)(void __iomem *reg_base);
	u32 (*enable_to_pcie_no_mac)(void __iomem *reg_base, bool enable);
	u32 (*enable_from_pcie_no_mac)(void __iomem *reg_base, bool enable);
	u32 (*set_cp_ul_pri)(void __iomem *reg_base, u32 pri);
	u32 (*set_cp_ul_dst_num)(void __iomem *reg_base, u32 dst);
	u32 (*set_cp_ul_cur_num)(void __iomem *reg_base, u32 cur);
	u32 (*set_cp_ul_flow_ctrl_mode)(void __iomem *reg_base, u32 mode);
	u32 (*set_cp_dl_pri)(void __iomem *reg_base, u32 pri);
	u32 (*set_cp_dl_dst_num)(void __iomem *reg_base, u32 dst);
	u32 (*set_cp_dl_cur_num)(void __iomem *reg_base, u32 cur);
	u32 (*set_cp_dl_flow_ctrl_mode)(void __iomem *reg_base, u32 mode);
	u32 (*ctrl_cp_work)(void __iomem *reg_base, bool enable);
	bool (*get_resume_status)(void __iomem *reg_base);
	bool (*get_pause_status)(void __iomem *reg_base);
	void (*enable_pcie_intr_write_reg_mode)(void __iomem *reg_base,
						bool enable);
};

struct sipa_sys_ops {
	u32 (*module_soft_rst)(void __iomem *reg_base,
			       u32 sys);
	u32 (*module_enable)(void __iomem *reg_base,
			     u32 enable, u32 sys);
};

struct sipa_rule_ops {
	int (*set_table)(void *this, u32 depth,
			 void *new_tbl, void *last_tb,
			 u32 *timel);
};

struct sipa_open_fifo_param {
	bool open_flag;
	struct sipa_comm_fifo_params *attr;
	struct sipa_ext_fifo_params *ext_attr;
	bool force_sw_intr;
	sipa_hal_notify_cb cb;
	void *priv;
};

struct sipa_hal_context {
	const char *name;
	struct device *dev;

	u32 ipa_intr;

	u32 ctrl_tx_intr0;
	u32 ctrl_flowctrl_intr0;
	u32 ctrl_tx_intr1;
	u32 ctrl_flowctrl_intr1;
	u32 ctrl_tx_intr2;
	u32 ctrl_flowctrl_intr2;
	u32 ctrl_tx_intr3;
	u32 ctrl_flowctrl_intr3;

	int is_bypass;

	u32 fifo_iram_size;
	u32 fifo_ddr_size;

	struct sipa_sys_ops sys_ops;
	struct sipa_hal_fifo_ops fifo_ops;
	struct sipa_hal_global_ops glb_ops;
	struct sipa_reg_res_tag phy_virt_res;
	struct sipa_open_fifo_param fifo_param[SIPA_FIFO_MAX];
	struct sipa_common_fifo_cfg_tag cmn_fifo_cfg[SIPA_FIFO_MAX];
};

u32 sipa_int_callback_func(int evt, void *cookie);
u32 sipa_fifo_ops_init(struct sipa_hal_fifo_ops *ops);
u32 sipa_glb_ops_init(struct sipa_hal_global_ops *ops);
u32 sipa_sys_proc_init(struct sipa_sys_ops *ops);
#endif
