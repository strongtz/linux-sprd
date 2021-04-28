#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/sipa.h>

#include "pam_ipa_regs.h"
#include "../pam_ipa_core.h"

struct pam_ipa_hal_proc_tag pam_ipa_ops;

static u32 pam_ipa_hal_init_pcie_ul_fifo_base(void __iomem *reg_base,
					      u32 free_addrl, u32 free_addrh,
					      u32 filled_addrl,
					      u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_init_pcie_ul_fifo_base(reg_base,
						 free_addrl, free_addrh,
						 filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_init_pcie_dl_fifo_base(void __iomem *reg_base,
					      u32 free_addrl, u32 free_addrh,
					      u32 filled_addrl,
					      u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_init_pcie_dl_fifo_base(reg_base,
						 free_addrl, free_addrh,
						 filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_init_wiap_ul_fifo_base(void __iomem *reg_base,
					      u32 free_addrl, u32 free_addrh,
					      u32 filled_addrl,
					      u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_init_wiap_ul_fifo_base(reg_base,
						 free_addrl, free_addrh,
						 filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_init_wiap_dl_fifo_base(void __iomem *reg_base,
					      u32 free_addrl, u32 free_addrh,
					      u32 filled_addrl,
					      u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_init_wiap_dl_fifo_base(reg_base,
						 free_addrl, free_addrh,
						 filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_init_pcie_ul_fifo_sts_addr(void __iomem *reg_base,
						  u32 free_addrl,
						  u32 free_addrh,
						  u32 filled_addrl,
						  u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_pcie_ul_fifo_sts_addr(reg_base,
						    free_addrl, free_addrh,
						    filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_init_pcie_dl_fifo_sts_addr(void __iomem *reg_base,
						  u32 free_addrl,
						  u32 free_addrh,
						  u32 filled_addrl,
						  u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_pcie_dl_fifo_sts_addr(reg_base,
						    free_addrl, free_addrh,
						    filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_init_wiap_dl_fifo_sts_addr(void __iomem *reg_base,
						  u32 free_addrl,
						  u32 free_addrh,
						  u32 filled_addrl,
						  u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_wiap_dl_fifo_sts_addr(reg_base,
						    free_addrl, free_addrh,
						    filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_init_wiap_ul_fifo_sts_addr(void __iomem *reg_base,
						  u32 free_addrl,
						  u32 free_addrh,
						  u32 filled_addrl,
						  u32 filled_addrh)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_wiap_ul_fifo_sts_addr(reg_base,
						    free_addrl, free_addrh,
						    filled_addrl, filled_addrh);

	return ret;
}

static u32 pam_ipa_hal_set_ddr_mapping(void __iomem *reg_base,
				       u32 offset_l, u32 offset_h)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_ddr_mapping(reg_base, offset_l, offset_h);

	return ret;
}

static u32 pam_ipa_hal_set_pcie_rc_base(void __iomem *reg_base,
					u32 offset_l, u32 offset_h)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_pcie_rc_base(reg_base, offset_l, offset_h);

	return ret;
}

static u32 pam_ipa_hal_replace_pair1_id(void __iomem *reg_base, u32 id)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_pair1_id(reg_base, id);

	return ret;
}

static u32 pam_ipa_hal_replace_pair2_id(void __iomem *reg_base, u32 id)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_pair2_id(reg_base, id);

	return ret;
}

static u32 pam_ipa_hal_replace_pair3_id(void __iomem *reg_base, u32 id)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_pair3_id(reg_base, id);

	return ret;
}

static u32 pam_ipa_hal_replace_pair4_id(void __iomem *reg_base, u32 id)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_pair4_id(reg_base, id);

	return ret;
}

static u32 pam_ipa_hal_set_offset_present(void __iomem *reg_base, u32 offset)
{
	u32 ret = 0;

	ret = pam_ipa_phy_set_offset_present(reg_base, offset);

	return ret;
}

static u32 pam_ipa_hal_start(void __iomem *reg_base)
{
	u32 ret = 0;

	ret = pam_ipa_phy_start(reg_base);

	return ret;
}

static u32 pam_ipa_hal_stop(void __iomem *reg_base)
{
	u32 ret = 0;

	ret = pam_ipa_phy_stop(reg_base);

	return ret;
}

static u32 pam_ipa_hal_resume(void __iomem *reg_base, u32 flag)
{
	u32 ret = 0;

	ret = pam_ipa_phy_resume(reg_base, flag);

	return ret;
}

static void pam_ipa_init_speed(void __iomem *reg_base)
{
	u32 tmp;

	tmp = __raw_readl(reg_base + PAM_IPA_BUFFER_TIMEOUT_VAL);
	tmp &=  ~(PAM_IPA_BUFFER_TIMEOUT_MASK);
	tmp |= PAM_IPA_BUFFER_TIMEOUT;
	__raw_writel(tmp, reg_base + PAM_IPA_BUFFER_TIMEOUT_VAL);

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_UL_FILLED_BUFFER_CTRL);
	tmp &= ~(CFG_UL_CP_FILLED_BUFFER_WATERMARK_MASK
		| CFG_UL_AP_FILLED_BUFFER_MATERMARK_MASK
		| CFG_UL_FILLED_BUFFER_CLR_MASK);
	tmp |= CFG_UL_CP_FILLED_BUFFER_WATERMARK
		| CFG_UL_AP_FILLED_BUFFER_MATERMARK
		| CFG_UL_FILLED_BUFFER_CLR;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_UL_FILLED_BUFFER_CTRL);

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_UL_FREE_BUFFER_CTRL);
	tmp &= ~(CFG_UL_CP_FREE_BUFFER_WATERMARK_MASK
		| CFG_UL_AP_FREE_BUFFER_MATERMARK_MASK
		| CFG_UL_FREE_BUFFER_CLR_MASK);
	tmp |= CFG_UL_CP_FREE_BUFFER_WATERMARK
		| CFG_UL_AP_FREE_BUFFER_MATERMARK
		| CFG_UL_FREE_BUFFER_CLR;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_UL_FREE_BUFFER_CTRL);

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_DL_DST_FILLED_BUFFER_CTRL);
	tmp &= ~(CFG_DL_DST_FILLED_BUFFER_MATERMARK_MASK
		| CFG_DL_DST_FILLED_BUFFER_CLR_MASK);
	tmp |= CFG_DL_DST_FILLED_BUFFER_MATERMARK
		| CFG_DL_DST_FILLED_BUFFER_CLR;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_DL_DST_FILLED_BUFFER_CTRL);

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_DL_DST_FREE_BUFFER_CTRL);
	tmp &= ~(CFG_DL_DST_FREE_BUFFER_MATERMARK_MASK
		| CFG_DL_DST_FREE_BUFFER_CLR_MASK);
	tmp |= CFG_DL_DST_FREE_BUFFER_MATERMARK
		| CFG_DL_DST_FREE_BUFFER_CLR;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_DL_DST_FREE_BUFFER_CTRL);

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_DL_SRC_FILLED_BUFFER_CTRL);
	tmp &= ~(CFG_DL_SRC_FILLED_BUFFER_MATERMARK_MASK
		| CFG_DL_SRC_FILLED_BUFFER_CLR_MASK);
	tmp |= CFG_DL_SRC_FILLED_BUFFER_MATERMARK
		| CFG_DL_SRC_FILLED_BUFFER_CLR;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_DL_SRC_FILLED_BUFFER_CTRL);

	tmp = __raw_readl(reg_base + PAM_IPA_CFG_DL_SRC_FREE_BUFFER_CTRL);
	tmp &= ~(CFG_DL_SRC_FREE_BUFFER_MATERMARK_MASK
		| CFG_DL_SRC_FREE_BUFFER_CLR_MASK);
	tmp |= CFG_DL_SRC_FREE_BUFFER_MATERMARK
		| CFG_DL_SRC_FREE_BUFFER_CLR;
	__raw_writel(tmp, reg_base + PAM_IPA_CFG_DL_SRC_FREE_BUFFER_CTRL);
}

u32 pam_ipa_init_api(struct pam_ipa_hal_proc_tag *ops)
{
	ops->init_pcie_dl_fifo_base =
		pam_ipa_hal_init_pcie_dl_fifo_base;
	ops->init_pcie_ul_fifo_base =
		pam_ipa_hal_init_pcie_ul_fifo_base;
	ops->init_wiap_dl_fifo_base =
		pam_ipa_hal_init_wiap_dl_fifo_base;
	ops->init_wiap_ul_fifo_base =
		pam_ipa_hal_init_wiap_ul_fifo_base;
	ops->init_pcie_ul_fifo_sts_addr =
		pam_ipa_hal_init_pcie_ul_fifo_sts_addr;
	ops->init_pcie_dl_fifo_sts_addr =
		pam_ipa_hal_init_pcie_dl_fifo_sts_addr;
	ops->init_wiap_ul_fifo_sts_addr =
		pam_ipa_hal_init_wiap_ul_fifo_sts_addr;
	ops->init_wiap_dl_fifo_sts_addr =
		pam_ipa_hal_init_wiap_dl_fifo_sts_addr;
	ops->set_ddr_mapping =
		pam_ipa_hal_set_ddr_mapping;
	ops->set_pcie_rc_base =
		pam_ipa_hal_set_pcie_rc_base;
	ops->start = pam_ipa_hal_start;
	ops->stop = pam_ipa_hal_stop;
	ops->resume = pam_ipa_hal_resume;

	return TRUE;
}

int pam_ipa_set_enabled(struct pam_ipa_cfg_tag *cfg)
{
	int ret = 0;

	if (cfg->enable_regmap) {
		ret = regmap_update_bits(cfg->enable_regmap,
					 cfg->enable_reg,
					 cfg->enable_mask,
					 cfg->enable_mask);
		if (ret < 0)
			pr_warn("%s: regmap update bits failed", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(pam_ipa_set_enabled);

u32 pam_ipa_init(struct pam_ipa_cfg_tag *cfg)
{
	u32 ret;

	pam_ipa_set_enabled(cfg);

	pam_ipa_hal_init_pcie_dl_fifo_base(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->remote_cfg.dl_fifo.rx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.dl_fifo.rx_fifo_base_addr),
		PAM_IPA_GET_LOW32(cfg->remote_cfg.dl_fifo.tx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.dl_fifo.tx_fifo_base_addr));

	pam_ipa_hal_init_pcie_ul_fifo_base(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->remote_cfg.ul_fifo.tx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.ul_fifo.tx_fifo_base_addr),
		PAM_IPA_GET_LOW32(cfg->remote_cfg.ul_fifo.rx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.ul_fifo.rx_fifo_base_addr));

	pam_ipa_hal_init_wiap_dl_fifo_base(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->local_cfg.dl_fifo.tx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.dl_fifo.tx_fifo_base_addr),
		PAM_IPA_GET_LOW32(cfg->local_cfg.dl_fifo.rx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.dl_fifo.rx_fifo_base_addr));

	pam_ipa_hal_init_wiap_ul_fifo_base(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->local_cfg.ul_fifo.rx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.ul_fifo.rx_fifo_base_addr),
		PAM_IPA_GET_LOW32(cfg->local_cfg.ul_fifo.tx_fifo_base_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.ul_fifo.tx_fifo_base_addr));

	pam_ipa_hal_init_pcie_dl_fifo_sts_addr(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->remote_cfg.dl_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.dl_fifo.fifo_sts_addr),
		PAM_IPA_GET_LOW32(cfg->remote_cfg.dl_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.dl_fifo.fifo_sts_addr));

	pam_ipa_hal_init_pcie_ul_fifo_sts_addr(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->remote_cfg.ul_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.ul_fifo.fifo_sts_addr),
		PAM_IPA_GET_LOW32(cfg->remote_cfg.ul_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->remote_cfg.ul_fifo.fifo_sts_addr));

	pam_ipa_hal_init_wiap_dl_fifo_sts_addr(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->local_cfg.dl_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.dl_fifo.fifo_sts_addr),
		PAM_IPA_GET_LOW32(cfg->local_cfg.dl_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.dl_fifo.fifo_sts_addr));

	pam_ipa_hal_init_wiap_ul_fifo_sts_addr(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->local_cfg.ul_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.ul_fifo.fifo_sts_addr),
		PAM_IPA_GET_LOW32(cfg->local_cfg.ul_fifo.fifo_sts_addr),
		PAM_IPA_GET_HIGH32(cfg->local_cfg.ul_fifo.fifo_sts_addr));

	pam_ipa_hal_set_ddr_mapping(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->pcie_offset),
		PAM_IPA_GET_HIGH32(cfg->pcie_offset));

	pam_ipa_hal_set_pcie_rc_base(
		cfg->reg_base,
		PAM_IPA_GET_LOW32(cfg->pcie_rc_base),
		PAM_IPA_GET_HIGH32(cfg->pcie_rc_base));

	pam_ipa_hal_set_offset_present(cfg->reg_base, PAM_IPA_OFFSET_PRESENT);
	pam_ipa_hal_replace_pair1_id(cfg->reg_base, SIPA_TERM_PCIE0);
	pam_ipa_hal_replace_pair2_id(cfg->reg_base, SIPA_TERM_PCIE1);
	pam_ipa_hal_replace_pair3_id(cfg->reg_base, SIPA_TERM_PCIE2);
	pam_ipa_hal_replace_pair4_id(cfg->reg_base, SIPA_TERM_PCIE0);

	pam_ipa_init_speed(cfg->reg_base);
	ret = pam_ipa_hal_start(cfg->reg_base);

	if (ret)
		return 0;
	else
		return -1;
}
