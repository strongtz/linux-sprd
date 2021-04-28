#ifndef _IPA_CP0_FIFO_PHY_H_
#define _IPA_CP0_FIFO_PHY_H_

#include "../sipa_hal_priv.h"

/*common fifo reg offset*/
#define SIPA_FIFO_USB_UL_OFFSET			0x80l /*PAM_U3 -> IPA*/
#define SIPA_FIFO_SDIO_UL_OFFSET		0x100l
#define SIPA_FIFO_AP_IP_UL_OFFSET		0x180l/*AP -> IPA*/
#define SIPA_FIFO_PCIE_UL_OFFSET		0x200l
	/* UL PCIE 0 ~ 3*/
#define SIPA_FIFO_REMOTE_PCIE_CTRL0_UL_OFFSET	0x280l
#define SIPA_FIFO_REMOTE_PCIE_CTRL1_UL_OFFSET	0x300l
#define SIPA_FIFO_REMOTE_PCIE_CTRL2_UL_OFFSET	0x380l
#define SIPA_FIFO_REMOTE_PCIE_CTRL3_UL_OFFSET	0x400l
#define SIPA_FIFO_AP_ETH_DL_OFFSET		0x480l/*ap_dev -> IPA*/
	/* DL mAP PCIE 0 ~ 3*/
#define SIPA_FIFO_LOCAL_PCIE_CTRL0_DL_OFFSET	0x500l
#define SIPA_FIFO_LOCAL_PCIE_CTRL1_DL_OFFSET	0x580l
#define SIPA_FIFO_LOCAL_PCIE_CTRL2_DL_OFFSET	0x600l
#define SIPA_FIFO_LOCAL_PCIE_CTRL3_DL_OFFSET	0x680l
#define SIPA_FIFO_WIFI_UL_OFFSET		0x700l
#define SIPA_FIFO_CP_DL_OFFSET			0x780l/*PAM_IPA -> IPA*/
#define SIPA_FIFO_USB_DL_OFFSET			0x800l/*IPA -> PAM_U3*/
#define SIPA_FIFO_SDIO_DL_OFFSET		0x880l
#define SIPA_FIFO_AP_IP_DL_OFFSET		0x900l/*IPA -> AP*/
#define SIPA_FIFO_PCIE_DL_OFFSET		0x980l
	/* DL PCIE 0 ~ 3*/
#define SIPA_FIFO_REMOTE_PCIE_CTRL0_DL_OFFSET	0xA00l
#define SIPA_FIFO_REMOTE_PCIE_CTRL1_DL_OFFSET	0xA80l
#define SIPA_FIFO_REMOTE_PCIE_CTRL2_DL_OFFSET	0xB00l
#define SIPA_FIFO_REMOTE_PCIE_CTRL3_DL_OFFSET	0xB80l
#define SIPA_FIFO_AP_ETH_UL_OFFSET		0xC00l/*IPA -> ap_dev*/
	/* UL mAP PCIE 0 ~ 3*/
#define SIPA_FIFO_LOCAL_PCIE_CTRL0_UL_OFFSET	0xC80l
#define SIPA_FIFO_LOCAL_PCIE_CTRL1_UL_OFFSET	0xD00l
#define SIPA_FIFO_LOCAL_PCIE_CTRL2_UL_OFFSET	0xD80l
#define SIPA_FIFO_LOCAL_PCIE_CTRL3_UL_OFFSET	0xE00l
#define SIPA_FIFO_WIFI_DL_OFFSET		0xE80l
#define SIPA_FIFO_CP_UL_OFFSET			0xF00l/*IPA -> PAM_IPA*/
#define SIPA_CACHE_LINE_CTRL_AND_STS_OFFSET	0xF80
/*Common fifo reg*/
#define IPA_COMMON_RX_FIFO_DEPTH		0x00l
#define IPA_COMMON_RX_FIFO_WR			0x04l
#define IPA_COMMON_RX_FIFO_RD			0x08l
#define IPA_COMMON_TX_FIFO_DEPTH		0x0Cl
#define IPA_COMMON_TX_FIFO_WR			0x10l
#define IPA_COMMON_TX_FIFO_RD			0x14l
#define IPA_COMMON_RX_FIFO_ADDRL		0x18l
#define IPA_COMMON_RX_FIFO_ADDRH		0x1Cl
#define IPA_COMMON_TX_FIFO_ADDRL		0x20l
#define IPA_COMMON_TX_FIFO_ADDRH		0x24l
#define IPA_PERFETCH_FIFO_CTL			0x28l
#define IPA_INT_GEN_CTL_TX_FIFO_VALUE		0x2Cl
#define IPA_INT_GEN_CTL_EN			0x30l
#define IPA_DROP_PACKET_CNT			0x34l
#define IPA_FLOW_CTRL_CONFIG			0x38l
#define IPA_TX_FIFO_FLOW_CTRL			0x3Cl
#define IPA_RX_FIFO_FLOW_CTRL			0x40l
#define IPA_RX_FIFO_FULL_NEG_PULSE_NUM		0x44l
#define IPA_INT_GEN_CTL_CLR			0x48l
#define IPA_INTR_RX_FIFO_FULL_ADDR_HIGH		0x4Cl
#define IPA_INTR_MEM_WR_ADDR_LOW		0x50l
#define IPA_RXFIFO_FULL_MEM_WR_ADDR_LOW		0x54l
#define IPA_INTR_MEM_WR_PATTERN			0x58l
#define IPA_RX_FIFO_FULL_MEM_WR_PATTERN		0x5Cl
#define IPA_TX_FIFO_WR_INIT			0x60l
#define IPA_COMMON_RX_FIFO_AXI_STS		0x64l
#define IPA_ERRCODE_INT_ADDR_LOW		0x68l
#define IPA_ERRCODE_INT_PATTERN			0x6Cl

/* Fifo interrupt enable bit*/
#define IPA_TXFIFO_INT_THRESHOLD_ONESHOT_EN	BIT(11)
#define IPA_TXFIFO_INT_THRESHOLD_SW_EN		BIT(10)
#define IPA_TXFIFO_INT_DELAY_TIMER_SW_EN	BIT(9)
#define IPA_TXFIFO_FULL_INT_EN			BIT(8)
#define IPA_TXFIFO_OVERFLOW_EN			BIT(7)
#define IPA_ERRORCODE_IN_TX_FIFO_EN		BIT(6)
#define IPA_DROP_PACKET_OCCUR_INT_EN		BIT(5)
#define IPA_RX_FIFO_INT_EXIT_FLOW_CTRL_EN	BIT(4)
#define IPA_RX_FIFO_INT_ENTER_FLOW_CTRL_EN	BIT(3)
#define IPA_TX_FIFO_INTR_SW_BIT_EN		BIT(2)
#define IPA_TX_FIFO_THRESHOLD_EN		BIT(1)
#define IPA_TX_FIFO_DELAY_TIMER_EN		BIT(0)
#define IPA_INT_EN_BIT_GROUP			0x00000FFFl

/*Fifo interrupt status bit*/
#define IPA_INT_TX_FIFO_THRESHOLD_SW_STS	BIT(22)
#define IPA_INT_EXIT_FLOW_CTRL_STS		BIT(20)
#define IPA_INT_ENTER_FLOW_CTRL_STS		BIT(19)
#define IPA_INT_TXFIFO_FULL_INT_STS		BIT(18)
#define IPA_INT_TXFIFO_OVERFLOW_STS		BIT(17)
#define IPA_INT_ERRORCODE_IN_TX_FIFO_STS	BIT(16)
#define IPA_INT_INTR_BIT_STS			BIT(15)
#define IPA_INT_THRESHOLD_STS			BIT(14)
#define IPA_INT_DELAY_TIMER_STS			BIT(13)
#define IPA_INT_DROP_PACKT_OCCUR		BIT(12)
#define IPA_INT_INT_STS_GROUP			0x5FF000l

/*Fifo interrupt clear bit*/
#define IPA_TX_FIFO_TIMER_CLR_BIT		BIT(0)
#define IPA_TX_FIFO_THRESHOLD_CLR_BIT		BIT(1)
#define IPA_TX_FIFO_INTR_CLR_BIT		BIT(2)
#define IPA_ENTRY_FLOW_CONTROL_CLR_BIT		BIT(3)
#define IPA_EXIT_FLOW_CONTROL_CLR_BIT		BIT(4)
#define IPA_DROP_PACKET_INTR_CLR_BIT		BIT(5)
#define IPA_ERROR_CODE_INTR_CLR_BIT		BIT(6)
#define IPA_TX_FIFO_OVERFLOW_CLR_BIT		BIT(7)
#define IPA_TX_FIFO_FULL_INT_CLR_BIT		BIT(8)
#define IPA_INT_STS_CLR_GROUP			(0x000001FFl)

#define NODE_DESCRIPTION_SIZE			128l

/**
 * Description: set rx fifo total depth.
 * Input:
 *   @fifo_base: Need to set total depth of the fifo,
 *				the base address of the FIFO.
 *   @depth: the size of depth.
 * return:
 *   TRUE: set successfully.
 *   FALSE: set failed.
 * Note:
 */
static inline u32 ipa_phy_set_rx_fifo_total_depth(void __iomem *fifo_base,
						  u32 depth)
{
	u32 tmp;

	if (depth > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);
	tmp &= 0x0000FFFF;
	tmp |= (depth << 16);
	writel_relaxed(tmp, fifo_base + IPA_COMMON_RX_FIFO_DEPTH);
	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);
	if ((tmp & 0xFFFF0000l) == tmp)
		return TRUE;
	else
		return FALSE;

}

/**
 * Description: get rx fifo total depth.
 * Input:
 *   @fifo_base: Need to get total depth of the fifo, the base address of the
 *              FIFO.
 * return: The size of toal depth.
 * Note:
 */
static inline u32 ipa_phy_get_rx_fifo_total_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);

	return (tmp >> 16) & 0x0000FFFF;
}

/**
 * Description: get rx fifo filled depth.
 * Input:
 *   @fifo_base: Need to get filled depth of the FIFO, the base address of the
 *              FIFO.
 * return:
 *   TRUE: The size of rx filled depth
 * Note:
 */
static inline u32 ipa_phy_get_rx_fifo_filled_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);

	return tmp & 0x0000FFFF;
}

/**
 * Description: get rx fifo full status.
 * Input:
 *   @fifo_base: Need to get rx fifo full status of the FIFO, the base address
 *              of the FIFO.
 * return:
 *   1: rx fifo full.
 *   0: rx fifo not full.
 * Note:
 */
static inline u32 ipa_phy_get_rx_fifo_full_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_WR);

	return tmp & 0x00000001;
}

/**
 * Description: update rx fifo write pointer.
 * Input:
 *   @fifo_base: Need to update rx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: update rx fifo write pointer successfully,
 *   FALSE: update rx fifo write pointer failed.
 * Note:
 */
static inline u32 ipa_phy_update_rx_fifo_wptr(void __iomem *fifo_base,
					      u32 wptr)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr =
		fifo_base + IPA_COMMON_RX_FIFO_WR;

	if (wptr > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (wptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) == wptr)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: get rx fifo write pointer.
 * Input:
 *   @fifo_base: Need to get rx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The write pointer of rx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_rx_fifo_wptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_WR);

	return (tmp >> 16);
}

/**
 * Description: update rx fifo read pointer.
 * Input:
 *   @fifo_base: Need to update rx fifo read pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: update rx fifo read pointer successfully,
 *   FALSE: update rx fifo read pointer failed.
 * Note:
 */
static inline u32 ipa_phy_update_rx_fifo_rptr(void __iomem *fifo_base,
					      u32 rptr)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_RX_FIFO_RD;

	if (rptr > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (rptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp >> 16) == rptr)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: get rx fifo read pointer.
 * Input:
 *   @fifo_base: Need to get rx fifo read pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The read pointer of rx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_rx_fifo_rptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_RD);

	return (tmp >> 16);
}

/**
 * Description: get rx fifo empty status.
 * Input:
 *   @fifo_base: Need to get rx fifo empty status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The empty status of rx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_rx_fifo_empty_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_RD);

	return tmp & 0x00000001;
}

/**
 * Description: set tx fifo total depth.
 * Input:
 *   @fifo_base: Need to set tx fifo empty status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: set tx fifo total depth successfully.
 *   FALSE: set tx fifo total_depth failed.
 * Note:
 */
static inline u32 ipa_phy_set_tx_fifo_total_depth(void __iomem *fifo_base,
						  u32 depth)
{
	u32 tmp;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_TX_FIFO_DEPTH;

	if (depth > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (depth << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) == depth)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: get tx fifo total depth.
 * Input:
 *   @fifo_base: Need to get tx fifo empty status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The total depth of tx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_tx_fifo_total_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_DEPTH);

	return ((tmp >> 16) & 0x0000FFFF);
}

/**
 * Description: get tx fifo filled depth.
 * Input:
 *   @fifo_base: Need to get tx fifo filled depth of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The tx fifo filled depth.
 * Note:
 */
static inline u32 ipa_phy_get_tx_fifo_filled_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_DEPTH);

	return (tmp & 0x0000FFFF);
}

/**
 * Description: get tx fifo full status.
 * Input:
 *   @fifo_base: Need to get tx fifo full status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The full status of tx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_tx_fifo_full_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_WR);

	return (tmp & 0x00000001);
}

/**
 * Description: get tx fifo empty status.
 * Input:
 *   @fifo_base: Need to get tx fifo empty status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The empty status of tx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_tx_fifo_empty_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_RD);

	return (tmp & 0x00000001);
}

/**
 * Description: update tx fifo write pointer.
 * Input:
 *   @fifo_base: Need to update tx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: update tx fifo write pointer successfully.
 *   FALSE: update tx fifo write pointer failed.
 * Note:
 */
static inline u32 ipa_phy_update_tx_fifo_wptr(void __iomem *fifo_base,
					      u32 wptr)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_TX_FIFO_WR_INIT;

	if (tmp > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (wptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= 0x2;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFFFFD;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp >> 16) == wptr)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: get tx fifo write pointer.
 * Input:
 *   @fifo_base: Need to get tx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The write pointer of tx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_tx_fifo_wptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_WR);

	return (tmp >> 16);
}

/**
 * Description: update tx fifo read pointer.
 * Input:
 *   @fifo_base: Need to update tx fifo read pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: update tx fifo read pointer successfully.
 *   FALSE: update tx fifo read pointer failed.
 * Note:
 */
static inline u32 ipa_phy_update_tx_fifo_rptr(void __iomem *fifo_base,
					      u32 rptr)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_TX_FIFO_RD;

	if (tmp > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (rptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp >> 16) == rptr)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: get tx fifo write pointer.
 * Input:
 *   @fifo_base: Need to get tx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The write pointer of rx fifo.
 * Note:
 */
static inline u32 ipa_phy_get_tx_fifo_rptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_RD);

	return (tmp >> 16);
}

/**
 * Description: set rx fifo address of iram.
 * Input:
 *   @fifo_base: Need to set rx fifo address of the FIFO, the base
 *              address of the FIFO.
 *   @addr_l: low 32 bit.
 *   @addr_h: high 8 bit.
 * return:
 *   TRUE: update rx fifo address of iram successfully.
 *   FALSE: update rx fifo address of iram failed.
 * Note:
 */
static inline u32 ipa_phy_set_rx_fifo_addr(void __iomem *fifo_base,
					   u32 addr_l, u32 addr_h)
{
	u32 tmp_l, tmp_h;

	writel_relaxed(addr_l, fifo_base + IPA_COMMON_RX_FIFO_ADDRL);
	writel_relaxed(addr_h, fifo_base + IPA_COMMON_RX_FIFO_ADDRH);

	tmp_l = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_ADDRL);
	tmp_h = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_ADDRH);

	if ((tmp_l == addr_l) && (tmp_h == addr_h))
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: get rx fifo address of iram.
 * Input:
 *   @fifo_base: Need to get rx fifo address of the FIFO, the base
 *              address of the FIFO.
 *   @addr_l: low 32 bit.
 *   @addr_h: high 8 bit.
 * return:
 *   void.
 * Note:
 */
static inline void ipa_phy_get_rx_fifo_addr(void __iomem *fifo_base,
					    u32 *addr_l, u32 *addr_h)
{
	*addr_l = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_ADDRL);
	*addr_h = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_ADDRH);
}

/**
 * Description: set tx fifo address of iram.
 * Input:
 *   @fifo_base: Need to set tx fifo address of the FIFO, the base
 *              address of the FIFO.
 *   @addr_l: low 32 bit.
 *   @addr_h: high 8 bit.
 * return:
 *   TRUE: update tx fifo address of iram successfully.
 *   FALSE: update tx fifo address of iram failed.
 * Note:
 */
static inline u32 ipa_phy_set_tx_fifo_addr(void __iomem *fifo_base,
					   u32 addr_l, u32 addr_h)
{
	u32 tmp_l, tmp_h;

	writel_relaxed(addr_l, fifo_base + IPA_COMMON_TX_FIFO_ADDRL);
	writel_relaxed(addr_h, fifo_base + IPA_COMMON_TX_FIFO_ADDRH);

	tmp_l = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_ADDRL);
	tmp_h = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_ADDRH);

	if ((tmp_l == addr_l) && (tmp_h == addr_h))
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: get tx fifo address of iram.
 * Input:
 *   @fifo_base: Need to get tx fifo address of the FIFO, the base
 *              address of the FIFO.
 *   @addr_l: low 32 bit.
 *   @addr_h: high 8 bit.
 * return:
 *   void.
 * Note:
 */
static inline void ipa_phy_get_tx_fifo_addr(void __iomem *fifo_base,
					    u32 *addr_l, u32 *addr_h)
{
	*addr_l = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_ADDRL);
	*addr_h = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_ADDRH);
}

/**
 * Description: Enable interrupt bit.
 * Input:
 *   @fifo_base: Need to enable interrupr bit of the FIFO, the base
 *              address of the FIFO.
 *   @int_bit: The interrupt bit that need to enable.
 * return:
 *   TRUE: Enable successfully.
 *   FALSE: Enable successfully.
 * Note:
**/
static inline u32 ipa_phy_enable_int_bit(void __iomem *fifo_base,
					 u32 int_bit)
{
	u32 tmp;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= int_bit;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp & int_bit) == int_bit) {
		tmp = TRUE;
	} else {
		tmp = FALSE;
	}

	return tmp;
}

/**
 * Description: Disable interrupt bit.
 * Input:
 *   @fifo_base: Need to Disable interrupr bit of the FIFO, the base
 *              address of the FIFO.
 *   @int_bit: The interrupt bit that need to disable.
 * return:
 *   TRUE: Disable successfully.
 *   FALSE: Disable failed.
 * Note:
 */
static inline u32 ipa_phy_disable_int_bit(void __iomem *fifo_base,
					  u32 int_bit)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= (~int_bit);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= int_bit;

	if (!tmp) {
		tmp = TRUE;
	} else {
		tmp = FALSE;
		pr_err("Disable interrupt bit = 0x%x set failed!\n",
		       int_bit);
	}

	return tmp;
}

static inline u32 ipa_phy_get_all_intr_enable_status(void __iomem *fifo_base)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);

	tmp &= IPA_INT_EN_BIT_GROUP;

	return tmp;
}

/**
 * Description: Get specified interrupt bit status.
 * Input:
 *   @fifo_base: Need to get interrupt bit of the FIFO, the base
 *              address of the FIFO.
 *   @int_bit: The specified interrupt bit that need to get.
 * return:
 *   TRUE: interrupt bit enable.
 *   FALSE: interrupt bit disable.
 * Note:
 */
static inline u32 ipa_phy_get_fifo_int_sts(void __iomem *fifo_base, u32 sts)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_INT_GEN_CTL_EN);

	if (tmp & sts)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get interrupt group status.
 * Input:
 *   @fifo_base: Need to get interrupt group status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Interrupt group status.
 * Note:
 */
static inline u32 ipa_phy_get_fifo_all_int_sts(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_INT_GEN_CTL_EN);

	return (tmp & IPA_INT_INT_STS_GROUP);
}

/**
 * Description: Clear interrupt flag, need to write 1, then write 0.
 * Input:
 *   @fifo_base: Need to clear interrupt flag of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   void.
 * Note:
 */
static inline void ipa_phy_clear_int(void __iomem *fifo_base, u32 clr_bit)
{
	writel_relaxed(clr_bit, fifo_base + IPA_INT_GEN_CTL_CLR);
}

/**
 * Description: Get drop packet count.
 * Input:
 *   @fifo_base: Need to get drop packet count of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Drop packet count.
 * Note:
 */
static inline u32 ipa_phy_get_drop_packet_cnt(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_DROP_PACKET_CNT);

	return tmp;
}

/**
 * Description: Get tx fifo threshold interrupt.
 * Input:
 *   @fifo_base: Need to get threshold interrupt of the FIFO, the base
 *              address of the FIFO.
 * OUTPUT:
 *   threshold value.
 * Note:
 */
static inline u32 ipa_phy_get_tx_fifo_interrupt_threshold(void __iomem
							  *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_INT_GEN_CTL_TX_FIFO_VALUE);

	return (tmp >> 16);
}

/**
 * Description: Set tx fifo interrupt threshold of value.
 * Input:
 *   @fifo_base: Need to get threshold interrupt value of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: set successfully.
 *   FALSE: set failed.
 * Note:
 */
static inline u32
ipa_phy_set_tx_fifo_interrupt_threshold(void __iomem *fifo_base, u32 threshold)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_TX_FIFO_VALUE;

	if (threshold > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (threshold << 16);
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp >> 16) == threshold)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get tx fifo interrupt of delay timer value.
 * Input:
 *   @fifo_base: Need to get delay timer interrupt of the FIFO, the base
 *              address of the FIFO.
 * OUTPUT:
 *   delay timer value.
 * Note:
 */
static inline u32
ipa_phy_get_tx_fifo_interrupt_delay_timer(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_INT_GEN_CTL_TX_FIFO_VALUE);

	return (tmp & 0x0000FFFF);
}

/**
 * Description: Set tx fifo interrupt of delay timer value.
 * Input:
 *   @fifo_base: Need to set delay timer interrupt of the FIFO, the base
 *              address of the FIFO.
 *   @threshold: The overflow value that need to set.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: set failed.
 * Note:
 */
static inline u32
ipa_phy_set_tx_fifo_interrupt_delay_timer(void __iomem *fifo_base,
					  u32 threshold)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_TX_FIFO_VALUE;

	if (threshold > 0xFFFF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFF0000;
	tmp |= threshold;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp & 0x0000FFFF) == threshold)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get current term number.
 * Input:
 *   @fifo_base: Need to get current term number of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Current term number.
 * Note:
 */
static inline u32 ipa_phy_get_cur_term_num(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_PERFETCH_FIFO_CTL);

	return ((tmp & 0x0003E000) >> 13);
}

/**
 * Description: Set current term number.
 * Input:
 *   @fifo_base: Need to set current term number of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32 ipa_phy_set_cur_term_num(void __iomem *fifo_base,
					   u32 num)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_PERFETCH_FIFO_CTL;

	if (num > 0x1F)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFC1FFF;
	tmp |= (num << 13);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (((tmp & 0x0003E000) >> 13) == num)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get dst term number.
 * Input:
 *   @fifo_base: Need to get dst term number of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Dst term number.
 * Note:
 */
static inline u32 ipa_phy_get_dst_term_num(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_PERFETCH_FIFO_CTL);

	return ((tmp & 0x00001F00) >> 8);
}

/**
 * Description: Set dst term number.
 * Input:
 *   @fifo_base: Need to set dst term number of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32 ipa_phy_set_dst_term_num(void __iomem *fifo_base,
					   u32 num)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_PERFETCH_FIFO_CTL;
	if (num > 0x1F)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFE0FF;
	tmp |= (num << 8);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (((tmp & 0x00001F00) >> 8) == num)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get prefetch fifo priority.
 * Input:
 *   @fifo_base: Need to get prefetch fifo priority of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Prefetch fifo priority.
 * Note:
 */
static inline u32 ipa_phy_get_prefetch_fifo_priority(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_PERFETCH_FIFO_CTL);

	return ((tmp & 0x000000F0) >> 4);
}

/**
 * Description: Set prefetch fifo priority.
 * Input:
 *   @fifo_base: Need to set prefetch fifo priority of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32 ipa_phy_set_prefetch_fifo_priority(void __iomem *fifo_base,
						     u32 pri)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_base;

	fifo_reg_base = fifo_base + IPA_PERFETCH_FIFO_CTL;

	if (pri > 0xF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_base);
	tmp &= 0xFFFFFF0F;
	tmp |= (pri << 4);
	writel_relaxed(tmp, fifo_reg_base);

	tmp = readl_relaxed(fifo_reg_base);
	if (((tmp & 0x000000F0) >> 4) == pri)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get prefetch threshold.
 * Input:
 *   @fifo_base: Need to get prefetch threshold of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Prefetch threshold.
 * Note:
 */
static inline u32 ipa_phy_get_prefetch_threshold(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_PERFETCH_FIFO_CTL);

	return (tmp & 0x0000000F);
}

/**
 * Description: Set prefetch threshold.
 * Input:
 *   @fifo_base: Need to get threshold of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32 ipa_phy_set_prefetch_threshold(void __iomem *fifo_base,
						 u32 threshold)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_PERFETCH_FIFO_CTL;

	if (threshold > 0xF)
		return FALSE;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFFFF0;
	tmp |= threshold;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp & 0x0000000F) == threshold)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Set stop receive bit.
 * Input:
 *   @fifo_base: Need to set stop receive bit of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32 ipa_phy_stop_receive(void __iomem *fifo_base)
{
	u32 tmp;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= 0x8;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (tmp & 0x8)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Clear stop receive bit.
 * Input:
 *   @fifo_base: Need to clear stop receive bit of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: clear successfully.
 *   FALSE: clear failed.
 * Note:
 */
static inline u32 ipa_phy_clear_stop_receive(void __iomem *fifo_base)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFFFF7;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if (!(tmp & 0x8))
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: recover fifo work.
 * Input:
 *   @fifo_base: Need to be recovered of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: Recover successfully.
 *   FALSE: Recover failed.
 * Note:
 */
static inline u32 ipa_phy_flow_ctrl_recover(void __iomem *fifo_base)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= 0x00000004;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (tmp & 0x00000004) {
		tmp &= 0xFFFFFFFB;
		writel_relaxed(tmp, fifo_reg_addr);

		tmp = readl_relaxed(fifo_reg_addr);
		if (!(tmp & 0x00000004))
			return TRUE;
		else
			return FALSE;
	} else {
		return FALSE;
	}
}

/**
 * Description: Set flow ctrl mode.
 * Input:
 *   @fifo_base: Need to set flow ctrl mode of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32 ipa_phy_set_flow_ctrl_config(void __iomem *fifo_base,
					       u32 config)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFFFFC;
	tmp |= config;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp & 0x00000003) == config)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get flow ctrl mode.
 * Input:
 *   @fifo_base: Need to get flow ctrl mode of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Flow ctrl config
 * Note:
 */
static inline u32 ipa_phy_get_flow_ctrl_config(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_FLOW_CTRL_CONFIG);

	return (tmp & 0x00000003);
}

/**
 * Description: Set tx fifo exit flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The need to be set.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32 ipa_phy_set_tx_fifo_exit_flow_ctrl_watermark(void __iomem
							       *fifo_base,
							       u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_TX_FIFO_FLOW_CTRL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (watermark << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) == watermark)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get tx fifo exit flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be get of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   Tx fifo exit watermark.
 * Note:
 */
static inline u32
ipa_phy_get_tx_fifo_exit_flow_ctrl_watermark(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_TX_FIFO_FLOW_CTRL);

	return (tmp >> 16);
}

/**
 * Description: Set tx fifo entry flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The need to be set.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32
ipa_phy_set_tx_fifo_entry_flow_ctrl_watermark(void __iomem *fifo_base,
					      u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_TX_FIFO_FLOW_CTRL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFF0000;
	tmp |= watermark;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp & 0x0000FFFF) == watermark)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get tx fifo entry flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be get of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   @The value of tx fifo entry watermark.
 * Note:
 */
static inline u32
ipa_phy_get_tx_fifo_entry_flow_ctrl_watermark(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_TX_FIFO_FLOW_CTRL);

	return (tmp & 0x0000FFFF);
}

/**
 * Description: Set rx fifo exit flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The value of rx fifo exit watermark.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32
ipa_phy_set_rx_fifo_exit_flow_ctrl_watermark(void __iomem *fifo_base,
					     u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_RX_FIFO_FLOW_CTRL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (watermark << 16);
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp >> 16) == watermark)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get rx fifo exit flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be get of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The value of rx fifo exit watermark.
 * Note:
 */
static inline u32
ipa_phy_get_rx_fifo_exit_flow_ctrl_watermark(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_RX_FIFO_FLOW_CTRL);

	return (tmp >> 16);
}

/**
 * Description: Set rx fifo entry flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The value of rx fifo entry watermark.
 * return:
 *   TRUE: Set successfully.
 *   FALSE: Set failed.
 * Note:
 */
static inline u32
ipa_phy_set_rx_fifo_entry_flow_ctrl_watermark(void __iomem *fifo_base,
					      u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_RX_FIFO_FLOW_CTRL;
	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFF0000;
	tmp |= watermark;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp & 0x0000FFFF) == watermark)
		return TRUE;
	else
		return FALSE;
}

/**
 * Description: Get rx fifo entry flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be get of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The value of rx fifo entry watermark.
 * Note:
 */
static inline u32
ipa_phy_get_rx_fifo_entry_flow_ctrl_watermark(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_RX_FIFO_FLOW_CTRL);

	return (tmp & 0x0000FFFF);
}

/**
 * Description: Get rx_axi_read_cmd_sts
 * return:
 *   rx_axi_read_cmd_sts.
 * Note:
 */
static inline u32 ipa_phy_get_rx_fifo_axi_sts(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_AXI_STS);

	return (tmp & 0x00000003);
}
#endif
