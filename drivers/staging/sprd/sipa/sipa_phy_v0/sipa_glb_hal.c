#include "../sipa_hal_priv.h"
#include "sipa_glb_phy.h"

#define SIPA_PCIE_DL_TX_INTR_PATTERN	0
#define SIPA_PCIE_DL_RX_INTR_PATTERN	1
#define SIPA_PCIE_UL_RX_INTR_PATTERN	2
#define SIPA_PCIE_UL_TX_INTR_PATTERN	3

#define SIPA_PCIE_INTR_ADDR_LOW		0x0
#define SIPA_PCIE_INTR_ADDR_HIGH	0x2

static u32 sipa_hal_set_work_mode(void __iomem *reg_base, u32 is_bypass)
{
	u32 ret = 0;

	ret = ipa_phy_set_work_mode(reg_base, is_bypass);

	return ret;
}


static u32 sipa_hal_enable_cp_through_pcie(void __iomem *reg_base,
					   u32 enable)
{
	u32 ret = 0;

	ret = ipa_phy_enable_cp_through_pcie(reg_base, enable);

	return ret;
}

static u32 sipa_hal_get_interrupt_status(void __iomem *reg_base)
{
	u32 ret = 0;

	ret = ipa_phy_get_int_status(reg_base);

	return ret;
}

static u32 sipa_hal_ctrl_ipa_action(void __iomem *reg_base, u32 enable)
{
	u32 ret = 0;

	ret = ipa_phy_ctrl_ipa_action(reg_base, enable);

	return ret;
}

static bool sipa_hal_check_resume_status(void __iomem *reg_base)
{
	return ipa_phy_get_resume_status(reg_base);
}

static bool sipa_hal_check_pause_status(void __iomem *reg_base)
{
	return ipa_phy_get_pause_status(reg_base);
}

static u32 sipa_hal_get_hw_ready_to_check_sts(void __iomem *reg_base)
{
	u32 ret = 0;

	ret = ipa_phy_get_hw_ready_to_check_sts(reg_base);

	return ret;
}

static u32 sipa_hal_hash_table_switch(void __iomem *reg_base,
				      u32 addr_l, u32 addr_h,
				      u32 len)
{
	u32 ret = 0;

	ret = ipa_phy_hash_table_switch(reg_base, addr_l, addr_h, len);

	return ret;
}

static u32 sipa_hal_get_hash_table(void __iomem *reg_base, u32 *addr_l,
				   u32 *addr_h, u32 *len)
{
	u32 ret = 0;

	ret = ipa_phy_get_hash_table(reg_base, addr_l, addr_h, len);

	return ret;
}

static u32 sipa_hal_map_interrupt_src_en(void __iomem *reg_base,
					 u32 enable, u32 mask)
{
	u32 ret = 0;

	ret = ipa_phy_map_interrupt_src_en(reg_base, enable, mask);

	return ret;
}

static u32 sipa_hal_clear_internal_fifo(void __iomem *reg_base, u32 clr_bit)
{
	u32 ret = 0;

	ret = ipa_phy_clear_internal_fifo(reg_base, clr_bit);

	return ret;
}

static u32 sipa_hal_set_flow_ctrl_to_src_blk(void __iomem *reg_base, u32 dst,
					     u32 src)
{
	u32 ret = 0;
	u32 *dst_ptr = NULL;

	dst_ptr = (u32 *)((u64 *)(u64)dst);

	ret = ipa_phy_set_flow_ctrl_to_src_blk(reg_base, dst_ptr, src);

	return ret;
}

static u32 sipa_hal_enable_def_flowctrl_to_src_blk(void __iomem *reg_base)
{
	u32 ret = 0;

	ret = sipa_hal_set_flow_ctrl_to_src_blk(reg_base,
						IPA_WIFI_WIAP_DL_FLOWCTL_SRC,
						(u32)WIAP_DL_SRC_BLK_MAP_UL);
	ret = sipa_hal_set_flow_ctrl_to_src_blk(reg_base,
						IPA_USB_SDIO_FLOWCTL_SRC,
						(u32)USB_DL_SRC_BLK_MAP_DL);

	return ret;
}

static u32 sipa_hal_get_flow_ctrl_to_src_sts(void __iomem *reg_base,
					     u32 dst, u32 src)
{
	u32 ret = 0;
	u32 *dst_ptr = NULL;

	dst_ptr = (u32 *)((u64 *)(u64)dst);

	ret = ipa_phy_get_flow_ctrl_to_src_sts(reg_base, dst_ptr, src);

	return ret;
}

static u32 sipa_hal_set_axi_mst_chn_priority(void __iomem *reg_base,
					     u32 chan, u32 prio)
{
	u32 ret = 0;

	ret = ipa_phy_set_axi_mst_chn_priority(reg_base, chan, prio);

	return ret;
}

static u32 sipa_hal_get_timestamp(void __iomem *reg_base)
{
	u32 ret = 0;

	ret = ipa_phy_get_timestamp(reg_base);

	return ret;
}

static u32 sipa_hal_set_force_to_ap_flag(void __iomem *reg_base,
					 u32 enable, u32 bit)
{
	u32 ret = 0;

	ret = ipa_phy_set_force_to_ap_flag(reg_base, enable, bit);

	return ret;
}

static u32 sipa_hal_set_wipa_ul_dma(void __iomem *reg_base, u32 enable)
{
	return ipa_phy_ctrl_wiap_ul_dma(reg_base, enable);
}

static u32 sipa_hal_enable_to_pcie_no_mac(void __iomem *reg_base, bool enable)
{
	return ipa_phy_enable_to_pcie_no_mac(reg_base, enable);
}

static u32 sipa_hal_enable_from_pcie_no_mac(void __iomem *reg_base, bool enable)
{
	return ipa_phy_enable_from_pcie_no_mac(reg_base, enable);
}

static u32 sipa_hal_set_cp_ul_pri(void __iomem *reg_base, u32 pri)
{
	return ipa_phy_set_cp_ul_pri(reg_base, pri);
}

static u32 sipa_hal_set_cp_ul_dst_num(void __iomem *reg_base, u32 dst)
{
	return ipa_phy_set_cp_ul_dst_num(reg_base, dst);
}

static u32 sipa_hal_set_cp_ul_cur_num(void __iomem *reg_base, u32 cur)
{
	return ipa_phy_set_cp_ul_cur_num(reg_base, cur);
}

static u32 sipa_hal_set_cp_ul_flow_ctrl_mode(void __iomem *reg_base, u32 mode)
{
	return ipa_phy_set_cp_ul_flow_ctrl_mode(reg_base, mode);
}

static u32 sipa_hal_set_cp_dl_pri(void __iomem *reg_base, u32 pri)
{
	return ipa_phy_set_cp_dl_pri(reg_base, pri);
}

static u32 sipa_hal_set_cp_dl_dst_num(void __iomem *reg_base, u32 dst)
{
	return ipa_phy_set_cp_dl_dst_num(reg_base, dst);
}

static u32 sipa_hal_set_cp_dl_cur_num(void __iomem *reg_base, u32 cur)
{
	return ipa_phy_set_cp_dl_cur_num(reg_base, cur);
}

static u32 sipa_hal_set_cp_dl_flow_ctrl_mode(void __iomem *reg_base, u32 mode)
{
	return ipa_phy_set_cp_dl_flow_ctrl_mode(reg_base, mode);
}

static u32 sipa_hal_ctrl_cp_work(void __iomem *reg_base, bool enable)
{
	return ipa_phy_ctrl_cp_work(reg_base, enable);
}

static void ipa_phy_set_dl_tx_intr(void __iomem *reg_base,
				   u32 addr_low,
				   u32 addr_high,
				   u32 pattern)
{
	u32 tmp;

	writel_relaxed(addr_low, reg_base + IPA_PCIE_DL_TX_FIFO_INT_ADDR_LOW);

	tmp = readl_relaxed(reg_base + IPA_PCIE_DL_TX_FIFO_INT_ADDR_HIGH);
	tmp &= ~(u32)IPA_PCIE_DL_TX_FIFO_INT_ADDR_HIGH_MASK;
	tmp |= addr_high & IPA_PCIE_DL_TX_FIFO_INT_ADDR_HIGH_MASK;
	writel_relaxed(tmp, reg_base + IPA_PCIE_DL_TX_FIFO_INT_ADDR_HIGH);
	writel_relaxed(pattern, reg_base + IPA_PCIE_DL_TX_FIFO_INT_PATTERN);
}

static void ipa_phy_set_dl_rx_intr(void __iomem *reg_base,
				   u32 addr_low,
				   u32 addr_high,
				   u32 pattern)
{
	u32 tmp;

	writel_relaxed(addr_low, reg_base + IPA_PCIE_DL_RX_FIFO_INT_ADDR_LOW);

	tmp = readl_relaxed(reg_base + IPA_PCIE_DL_RX_FIFO_INT_ADDR_HIGH);
	tmp &= ~(u32)IPA_PCIE_DL_RX_FIFO_INT_ADDR_HIGH_MASK;
	tmp |= addr_high & IPA_PCIE_DL_RX_FIFO_INT_ADDR_HIGH_MASK;
	writel_relaxed(tmp, reg_base + IPA_PCIE_DL_RX_FIFO_INT_ADDR_HIGH);
	writel_relaxed(pattern, reg_base + IPA_PCIE_DL_RX_FIFO_INT_PATTERN);
}

static void ipa_phy_set_ul_rx_intr(void __iomem *reg_base,
				   u32 addr_low,
				   u32 addr_high,
				   u32 pattern)
{
	u32 tmp;

	writel_relaxed(addr_low, reg_base + IPA_PCIE_UL_RX_FIFO_INT_ADDR_LOW);

	tmp = readl_relaxed(reg_base + IPA_PCIE_UL_RX_FIFO_INT_ADDR_HIGH);
	tmp &= ~(u32)IPA_PCIE_UL_RX_FIFO_INT_ADDR_HIGH_MASK;
	tmp |= addr_high & IPA_PCIE_UL_RX_FIFO_INT_ADDR_HIGH_MASK;
	writel_relaxed(tmp, reg_base + IPA_PCIE_UL_RX_FIFO_INT_ADDR_HIGH);
	writel_relaxed(pattern, reg_base + IPA_PCIE_UL_RX_FIFO_INT_PATTERN);
}

static void ipa_phy_set_ul_tx_intr(void __iomem *reg_base,
				   u32 addr_low,
				   u32 addr_high,
				   u32 pattern)
{
	u32 tmp;

	writel_relaxed(addr_low, reg_base + IPA_PCIE_UL_TX_FIFO_INT_ADDR_LOW);

	tmp = readl_relaxed(reg_base + IPA_PCIE_UL_TX_FIFO_INT_ADDR_HIGH);
	tmp &= ~(u32)IPA_PCIE_UL_TX_FIFO_INT_ADDR_HIGH_MASK;
	tmp |= addr_high & IPA_PCIE_UL_TX_FIFO_INT_ADDR_HIGH_MASK;
	writel_relaxed(tmp, reg_base + IPA_PCIE_UL_TX_FIFO_INT_ADDR_HIGH);
	writel_relaxed(pattern, reg_base + IPA_PCIE_UL_TX_FIFO_INT_PATTERN);
}

static void sipa_hal_enable_pcie_intr_write_reg_mode(void __iomem *reg_base,
						     bool enable)
{
	ipa_phy_enable_pcie_mem_intr(reg_base, enable);
	if (!enable)
		return;

	ipa_phy_set_dl_tx_intr(reg_base,
			       SIPA_PCIE_INTR_ADDR_LOW,
			       SIPA_PCIE_INTR_ADDR_HIGH,
			       SIPA_PCIE_DL_TX_INTR_PATTERN);
	ipa_phy_set_dl_rx_intr(reg_base,
			       SIPA_PCIE_INTR_ADDR_LOW,
			       SIPA_PCIE_INTR_ADDR_HIGH,
			       SIPA_PCIE_DL_RX_INTR_PATTERN);
	ipa_phy_set_ul_rx_intr(reg_base,
			       SIPA_PCIE_INTR_ADDR_LOW,
			       SIPA_PCIE_INTR_ADDR_HIGH,
			       SIPA_PCIE_UL_RX_INTR_PATTERN);
	ipa_phy_set_ul_tx_intr(reg_base,
			       SIPA_PCIE_INTR_ADDR_LOW,
			       SIPA_PCIE_INTR_ADDR_HIGH,
			       SIPA_PCIE_UL_TX_INTR_PATTERN);
}

u32 sipa_glb_ops_init(
	struct sipa_hal_global_ops *ops)
{
	ops->set_mode				=
		sipa_hal_set_work_mode;
	ops->clear_internal_fifo		=
		sipa_hal_clear_internal_fifo;
	ops->ctrl_ipa_action			=
		sipa_hal_ctrl_ipa_action;
	ops->get_flow_ctrl_to_src_sts		=
		sipa_hal_get_flow_ctrl_to_src_sts;
	ops->get_hw_ready_to_check_sts		=
		sipa_hal_get_hw_ready_to_check_sts;
	ops->get_int_status			=
		sipa_hal_get_interrupt_status;
	ops->hash_table_switch			=
		sipa_hal_hash_table_switch;
	ops->get_hash_table			=
		sipa_hal_get_hash_table;
	ops->map_interrupt_src_en		=
		sipa_hal_map_interrupt_src_en;
	ops->set_axi_mst_chn_priority		=
		sipa_hal_set_axi_mst_chn_priority;
	ops->set_flow_ctrl_to_src_blk		=
		sipa_hal_set_flow_ctrl_to_src_blk;
	ops->get_timestamp			=
		sipa_hal_get_timestamp;
	ops->set_force_to_ap			=
		sipa_hal_set_force_to_ap_flag;
	ops->enable_cp_through_pcie		=
		sipa_hal_enable_cp_through_pcie;
	ops->enable_wiap_ul_dma			=
		sipa_hal_set_wipa_ul_dma;
	ops->enable_def_flowctrl_to_src_blk	=
		sipa_hal_enable_def_flowctrl_to_src_blk;
	ops->enable_to_pcie_no_mac		=
		sipa_hal_enable_to_pcie_no_mac;
	ops->enable_from_pcie_no_mac		=
		sipa_hal_enable_from_pcie_no_mac;
	ops->set_cp_ul_pri			=
		sipa_hal_set_cp_ul_pri;
	ops->set_cp_ul_dst_num			=
		sipa_hal_set_cp_ul_dst_num;
	ops->set_cp_ul_cur_num			=
		sipa_hal_set_cp_ul_cur_num;
	ops->set_cp_ul_flow_ctrl_mode		=
		sipa_hal_set_cp_ul_flow_ctrl_mode;
	ops->set_cp_dl_pri			=
		sipa_hal_set_cp_dl_pri;
	ops->set_cp_dl_dst_num			=
		sipa_hal_set_cp_dl_dst_num;
	ops->set_cp_dl_cur_num			=
		sipa_hal_set_cp_dl_cur_num;
	ops->set_cp_dl_flow_ctrl_mode		=
		sipa_hal_set_cp_dl_flow_ctrl_mode;
	ops->ctrl_cp_work			=
		sipa_hal_ctrl_cp_work;
	ops->enable_pcie_intr_write_reg_mode	=
		sipa_hal_enable_pcie_intr_write_reg_mode;
	ops->get_resume_status			=
		sipa_hal_check_resume_status;
	ops->get_pause_status			=
		sipa_hal_check_pause_status;

	return TRUE;
}
