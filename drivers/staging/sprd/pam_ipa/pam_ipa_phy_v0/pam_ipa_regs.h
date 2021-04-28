#ifndef PAM_IPA_REGS_H_
#define PAM_IPA_REGS_H_

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/bitops.h>

#define PAM_IPA_OFFSET_PRESENT	64

#define TRUE	1
#define FALSE	0

/* PCIE UL FIFO */
#define PAM_IPA_UL_CP_FREE_FIFO_BASE_ADDRL	(0x00l)
#define PAM_IPA_UL_CP_FREE_FIFO_BASE_ADDRH	(0x04l)
#define PAM_IPA_UL_CP_FILLED_FIFO_BASE_ADDRL	(0x08l)
#define PAM_IPA_UL_CP_FILLED_FIFO_BASE_ADDRH	(0x0Cl)

/* WiAP UL FIFO */
#define PAM_IPA_UL_AP_FREE_FIFO_BASE_ADDRL	(0x10l)
#define PAM_IPA_UL_AP_FREE_FIFO_BASE_ADDRH	(0x14l)
#define PAM_IPA_UL_AP_FILLED_FIFO_BASE_ADDRL	(0x18l)
#define PAM_IPA_UL_AP_FILLED_FIFO_BASE_ADDRH	(0x1Cl)

/* PCIE DL FIFO */
#define PAM_IPA_DL_CP_FREE_FIFO_BASE_ADDRL	(0x20l)
#define PAM_IPA_DL_CP_FREE_FIFO_BASE_ADDRH	(0x24l)
#define PAM_IPA_DL_CP_FILLED_FIFO_BASE_ADDRL	(0x28l)
#define PAM_IPA_DL_CP_FILLED_FIFO_BASE_ADDRH	(0x2Cl)

/* WiAP DL FIFO */
#define PAM_IPA_DL_AP_FREE_FIFO_BASE_ADDRL	(0x30l)
#define PAM_IPA_DL_AP_FREE_FIFO_BASE_ADDRH	(0x34l)
#define PAM_IPA_DL_AP_FILLED_FIFO_BASE_ADDRL	(0x38l)
#define PAM_IPA_DL_AP_FILLED_FIFO_BASE_ADDRH	(0x3Cl)

#define PAM_IPA_UL_CP_FREE_STS_BASE_ADDRL	(0x40l)
#define PAM_IPA_UL_CP_FREE_STS_BASE_ADDRH	(0x44l)
#define PAM_IPA_UL_CP_FILLED_STS_BASE_ADDRL	(0x48l)
#define PAM_IPA_UL_CP_FILLED_STS_BASE_ADDRH	(0x4Cl)

#define PAM_IPA_UL_AP_FREE_STS_BASE_ADDRL	(0x50l)
#define PAM_IPA_UL_AP_FREE_STS_BASE_ADDRH	(0x54l)
#define PAM_IPA_UL_AP_FILLED_STS_BASE_ADDRL	(0x58l)
#define PAM_IPA_UL_AP_FILLED_STS_BASE_ADDRH	(0x5Cl)

#define PAM_IPA_DL_CP_FREE_STS_BASE_ADDRL	(0x60l)
#define PAM_IPA_DL_CP_FREE_STS_BASE_ADDRH	(0x64l)
#define PAM_IPA_DL_CP_FILLED_STS_BASE_ADDRL	(0x68l)
#define PAM_IPA_DL_CP_FILLED_STS_BASE_ADDRH	(0x6Cl)

#define PAM_IPA_DL_AP_FREE_STS_BASE_ADDRL	(0x70l)
#define PAM_IPA_DL_AP_FREE_STS_BASE_ADDRH	(0x74l)
#define PAM_IPA_DL_AP_FILLED_STS_BASE_ADDRL	(0x78l)
#define PAM_IPA_DL_AP_FILLED_STS_BASE_ADDRH	(0x7Cl)

#define PAM_IPA_CFG_UL_FILLED_BUFFER_CTRL	(0x80l)

#define PAM_IPA_CFG_UL_FREE_BUFFER_CTRL		(0x84l)

#define PAM_IPA_CFG_DL_DST_FILLED_BUFFER_CTRL	(0x88l)

#define PAM_IPA_CFG_DL_DST_FREE_BUFFER_CTRL	(0x8Cl)

#define PAM_IPA_CFG_DL_SRC_FILLED_BUFFER_CTRL	(0x90l)

#define PAM_IPA_CFG_DL_SRC_FREE_BUFFER_CTRL	(0x94l)

#define PAM_IPA_DDR_MAPPING_OFFSET_L		(0x98l)

#define PAM_IPA_DDR_MAPPINH_OFFSET_H		(0x9Cl)

#define PAM_IPA_AXI_MST_CFG			(0xA0l)

#define PAM_IPA_AXI_OS_CFG			(0xA4l)

#define PAM_IPA_CH_WR_PRIO			(0xA8l)

#define PAM_IPA_CH_RD_PRIO			(0xACl)

#define PAM_IPA_CFG_START			(0xB0l)

#define PAM_IPA_PCIE_RC_BASE_ADDRL		(0xB4l)

#define PAM_IPA_PCIE_RC_BASE_ADDRH		(0xB8l)

#define PAM_IPA_BUFFER_TIMEOUT_VAL		(0xBCl)

#define PAM_IPA_IP_VER				(0xC0l)

#define PAM_IPA_ID_REPLACE_PAIR1		(0xC4l)

#define PAM_IPA_ID_REPLACE_PAIR2		(0xC8l)

#define PAM_IPA_ID_REPLACE_PAIR3		(0xCCl)

#define PAM_IPA_ID_REPLACE_PAIR4		(0xD0l)

#define PAM_IPA_COMMON_FIFO_STS_UPDATE		(0xD4l)

#define PAM_IPA_PCIE_MODE1_INT			(0xD8l)

#define PAM_IPA_SW_DEBUG_MEM_BASE_ADDR		(0xDCl)

#define PAM_IPA_SW_DEBUG_INT			(0xE0l)

#define PAM_IPA_SW_DEBUG_DL_AP_RD_LEN		(0xE4l)

#define PAM_IPA_SW_DEBUG_DL_CP_RD_LEN		(0xE8l)

#define PAM_IPA_SW_DEBUG_UL_AP_RD_LEN		(0xECl)

#define PAM_IPA_SW_DEBUG_UL_CP_RD_LEN		(0xF0l)

#define PAM_IPA_SW_DEBUG_UL_PTR_CURT_STS	(0xF4l)

#define PAM_IPA_SW_DEBUG_DL_PTR_CURT_STS	(0xF8l)

#define PAM_IPA_COMMON_FIFO_OFFSET		(0xFCl)

#define PAM_IPA_BUFFER_TIMEOUT_MASK		GENMASK(31, 0)
#define PAM_IPA_BUFFER_TIMEOUT			(0x000186a0l)

#define CFG_UL_CP_FILLED_BUFFER_WATERMARK	(0xC << 6)
#define CFG_UL_AP_FILLED_BUFFER_MATERMARK	(0xC << 1)
#define CFG_UL_FILLED_BUFFER_CLR		(0x0)

#define CFG_UL_CP_FILLED_BUFFER_WATERMARK_MASK	GENMASK(10, 6)
#define CFG_UL_AP_FILLED_BUFFER_MATERMARK_MASK	GENMASK(5, 1)
#define CFG_UL_FILLED_BUFFER_CLR_MASK		GENMASK(0, 0)

#define CFG_UL_CP_FREE_BUFFER_WATERMARK		(0xF << 6)
#define CFG_UL_AP_FREE_BUFFER_MATERMARK		(0xF << 1)
#define CFG_UL_FREE_BUFFER_CLR			(0x0)

#define CFG_UL_CP_FREE_BUFFER_WATERMARK_MASK	GENMASK(10, 6)
#define CFG_UL_AP_FREE_BUFFER_MATERMARK_MASK	GENMASK(5, 1)
#define CFG_UL_FREE_BUFFER_CLR_MASK		GENMASK(0, 0)

#define CFG_DL_DST_FILLED_BUFFER_MATERMARK	(0xC << 1)
#define CFG_DL_DST_FILLED_BUFFER_CLR		(0x0)

#define CFG_DL_DST_FILLED_BUFFER_MATERMARK_MASK	GENMASK(5, 1)
#define CFG_DL_DST_FILLED_BUFFER_CLR_MASK	GENMASK(0, 0)

#define CFG_DL_DST_FREE_BUFFER_MATERMARK	(0xF << 1)
#define CFG_DL_DST_FREE_BUFFER_CLR		(0x0)

#define CFG_DL_DST_FREE_BUFFER_MATERMARK_MASK	GENMASK(5, 1)
#define CFG_DL_DST_FREE_BUFFER_CLR_MASK		GENMASK(0, 0)

#define CFG_DL_SRC_FILLED_BUFFER_MATERMARK	(0xC << 1)
#define CFG_DL_SRC_FILLED_BUFFER_CLR		(0x0 << 0)

#define CFG_DL_SRC_FILLED_BUFFER_MATERMARK_MASK	GENMASK(5, 1)
#define CFG_DL_SRC_FILLED_BUFFER_CLR_MASK	GENMASK(0, 0)

#define CFG_DL_SRC_FREE_BUFFER_MATERMARK	(0xC << 1)
#define CFG_DL_SRC_FREE_BUFFER_CLR		(0x0)

#define CFG_DL_SRC_FREE_BUFFER_MATERMARK_MASK	GENMASK(5, 1)
#define CFG_DL_SRC_FREE_BUFFER_CLR_MASK		GENMASK(0, 0)

static inline u32 pam_ipa_phy_init_pcie_ul_fifo_base(void __iomem *reg_base,
						     u32 free_addrl,
						     u32 free_addrh,
						     u32 filled_addrl,
						     u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_UL_CP_FREE_FIFO_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_UL_CP_FREE_FIFO_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_UL_CP_FILLED_FIFO_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_UL_CP_FILLED_FIFO_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_init_pcie_dl_fifo_base(void __iomem *reg_base,
						     u32 free_addrl,
						     u32 free_addrh,
						     u32 filled_addrl,
						     u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_DL_CP_FREE_FIFO_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_DL_CP_FREE_FIFO_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_DL_CP_FILLED_FIFO_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_DL_CP_FILLED_FIFO_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_init_wiap_ul_fifo_base(void __iomem *reg_base,
						     u32 free_addrl,
						     u32 free_addrh,
						     u32 filled_addrl,
						     u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_UL_AP_FREE_FIFO_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_UL_AP_FREE_FIFO_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_UL_AP_FILLED_FIFO_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_UL_AP_FILLED_FIFO_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_init_wiap_dl_fifo_base(void __iomem *reg_base,
						     u32 free_addrl,
						     u32 free_addrh,
						     u32 filled_addrl,
						     u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_DL_AP_FREE_FIFO_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_DL_AP_FREE_FIFO_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_DL_AP_FILLED_FIFO_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_DL_AP_FILLED_FIFO_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_set_pcie_ul_fifo_sts_addr(void __iomem *reg_base,
							u32 free_addrl,
							u32 free_addrh,
							u32 filled_addrl,
							u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_UL_CP_FREE_STS_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_UL_CP_FREE_STS_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_UL_CP_FILLED_STS_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_UL_CP_FILLED_STS_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_set_pcie_dl_fifo_sts_addr(void __iomem *reg_base,
							u32 free_addrl,
							u32 free_addrh,
							u32 filled_addrl,
							u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_DL_CP_FREE_STS_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_DL_CP_FREE_STS_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_DL_CP_FILLED_STS_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_DL_CP_FILLED_STS_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_set_wiap_ul_fifo_sts_addr(void __iomem *reg_base,
							u32 free_addrl,
							u32 free_addrh,
							u32 filled_addrl,
							u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_UL_AP_FREE_STS_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_UL_AP_FREE_STS_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_UL_AP_FILLED_STS_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_UL_AP_FILLED_STS_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_set_wiap_dl_fifo_sts_addr(void __iomem *reg_base,
							u32 free_addrl,
							u32 free_addrh,
							u32 filled_addrl,
							u32 filled_addrh)
{
	__raw_writel(free_addrl,
		     reg_base + PAM_IPA_DL_AP_FREE_STS_BASE_ADDRL);
	__raw_writel(free_addrh,
		     reg_base + PAM_IPA_DL_AP_FREE_STS_BASE_ADDRH);
	__raw_writel(filled_addrl,
		     reg_base + PAM_IPA_DL_AP_FILLED_STS_BASE_ADDRL);
	__raw_writel(filled_addrh,
		     reg_base + PAM_IPA_DL_AP_FILLED_STS_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_set_ddr_mapping(void __iomem *reg_base,
					      u32 offset_l, u32 offset_h)
{
	__raw_writel(offset_l,
		     reg_base + PAM_IPA_DDR_MAPPING_OFFSET_L);
	__raw_writel(offset_h,
		     reg_base + PAM_IPA_DDR_MAPPINH_OFFSET_H);

	return TRUE;
}

static inline u32 pam_ipa_phy_set_pcie_rc_base(void __iomem *reg_base,
					       u32 offset_l, u32 offset_h)
{
	__raw_writel(offset_l,
		     reg_base + PAM_IPA_PCIE_RC_BASE_ADDRL);
	__raw_writel(offset_h,
		     reg_base + PAM_IPA_PCIE_RC_BASE_ADDRH);

	return TRUE;
}

static inline u32 pam_ipa_phy_set_pair1_id(void __iomem *reg_base, u32 id)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR1);
	tmp &= 0x000000FF;
	tmp |= (id << 8);
	__raw_writel(tmp, reg_base + PAM_IPA_ID_REPLACE_PAIR1);

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR1);
	if ((tmp >> 8) == id)
		return TRUE;
	else
		return FALSE;
}

static inline u32 pam_ipa_phy_set_pair2_id(void __iomem *reg_base,
					   u32 id)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR2);
	tmp &= 0x000000FF;
	tmp |= (id << 8);
	__raw_writel(tmp, reg_base + PAM_IPA_ID_REPLACE_PAIR2);

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR2);
	if ((tmp >> 8) == id)
		return TRUE;
	else
		return FALSE;
}

static inline u32 pam_ipa_phy_set_pair3_id(void __iomem *reg_base,
					   u32 id)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR3);
	tmp &= 0x000000FF;
	tmp |= (id << 8);
	__raw_writel(tmp, reg_base + PAM_IPA_ID_REPLACE_PAIR3);

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR3);
	if ((tmp >> 8) == id)
		return TRUE;
	else
		return FALSE;
}

static inline u32 pam_ipa_phy_set_pair4_id(void __iomem *reg_base, u32 id)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR4);
	tmp &= 0x000000FF;
	tmp |= (id << 8);
	__raw_writel(tmp, reg_base + PAM_IPA_ID_REPLACE_PAIR4);

	tmp = __raw_readl(reg_base + PAM_IPA_ID_REPLACE_PAIR4);
	if ((tmp >> 8) == id)
		return TRUE;
	else
		return FALSE;
}

static inline u32 pam_ipa_phy_set_offset_present(void __iomem *reg_base,
						 u32 offset)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_START);
	tmp &= 0x0000FFFF;
	tmp |= (PAM_IPA_CFG_START << 16);
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_START);

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_START);

	if ((tmp >> 16) == offset)
		return TRUE;
	else
		return FALSE;
}

static inline u32 pam_ipa_phy_start(void __iomem *reg_base)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_START);
	tmp |= 1;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_START);

	return TRUE;
}

static inline u32 pam_ipa_phy_stop(void __iomem *reg_base)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_START);
	tmp |= 2;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_START);

	return TRUE;
}

static inline u32 pam_ipa_phy_resume(void __iomem *reg_base, u32 flag)
{
	u32 tmp = 0;

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_START);
	if (!flag) {
		tmp |= 4;
		__raw_writel(tmp, reg_base + PAM_IPA_CFG_START);
		while ((__raw_readl(reg_base + PAM_IPA_CFG_START)
				& 0x08l) == 0x0l)
			;
	} else {
		tmp &= 0xFFFFFFFB;
		__raw_writel(tmp, reg_base + PAM_IPA_CFG_START);
	}

	return TRUE;
}

#endif /* !PAM_IPA_REGS_H_ */
