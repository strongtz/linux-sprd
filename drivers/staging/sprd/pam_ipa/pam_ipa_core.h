#ifndef _PAM_IPA_CORE_H_
#define _PAM_IPA_CORE_H_

#include <linux/sipa.h>
#include <linux/skbuff.h>

#define PAM_AKB_BUF_SIZE	1664
#define PAM_FREE_FIFO_SIZE	128

#define PAM_IPA_DDR_MAP_OFFSET_L				0x0
#define PAM_IPA_DDR_MAP_OFFSET_H				0x2

#define PAM_IPA_GET_LOW32(val) \
			((u32)((val) & 0x00000000FFFFFFFF))
#define PAM_IPA_GET_HIGH32(val) \
			((u32)(((val) >> 32) & 0x00000000FFFFFFFF))
#define PAM_IPA_STI_64BIT(l_val, h_val) \
			((u64)((l_val) | ((u64)(h_val) << 32)))

struct pam_ipa_hal_proc_tag {
	u32 (*init_pcie_ul_fifo_base)(void __iomem *reg_base,
				      u32 free_addrl, u32 free_addrh,
				      u32 filled_addrl, u32 filled_addrh);
	u32 (*init_pcie_dl_fifo_base)(void __iomem *reg_base,
				      u32 free_addrl, u32 free_addrh,
				      u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_ul_fifo_base)(void __iomem *reg_base,
				      u32 free_addrl, u32 free_addrh,
				      u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_dl_fifo_base)(void __iomem *reg_base,
				      u32 free_addrl, u32 free_addrh,
				      u32 filled_addrl, u32 filled_addrh);
	u32 (*init_pcie_ul_fifo_sts_addr)(void __iomem *reg_base,
					  u32 free_addrl, u32 free_addrh,
					  u32 filled_addrl, u32 filled_addrh);
	u32 (*init_pcie_dl_fifo_sts_addr)(void __iomem *reg_base,
					  u32 free_addrl, u32 free_addrh,
					  u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_dl_fifo_sts_addr)(void __iomem *reg_base,
					  u32 free_addrl, u32 free_addrh,
					  u32 filled_addrl, u32 filled_addrh);
	u32 (*init_wiap_ul_fifo_sts_addr)(void __iomem *reg_base,
					  u32 free_addrl, u32 free_addrh,
					  u32 filled_addrl, u32 filled_addrh);
	u32 (*set_ddr_mapping)(void __iomem *reg_base,
			       u32 offset_l, u32 offset_h);
	u32 (*set_pcie_rc_base)(void __iomem *reg_base,
				u32 offset_l, u32 offset_h);
	u32 (*start)(void __iomem *reg_base);
	u32 (*stop)(void __iomem *reg_base);
	u32 (*resume)(void __iomem *reg_base, u32 flag);

};

struct pam_ipa_cfg_tag {
	struct platform_device *pdev;
	void __iomem *reg_base;
	struct resource pam_ipa_res;

	struct regmap *enable_regmap;
	u32 enable_reg;
	u32 enable_mask;

	bool connected;
	u64 pcie_offset;
	u64 pcie_rc_base;

	struct sipa_connect_params pam_local_param;

	struct sipa_to_pam_info local_cfg;
	struct sipa_to_pam_info remote_cfg;

	void *dl_buf;
	void *ul_buf;
	dma_addr_t dl_dma_addr;
	dma_addr_t ul_dma_addr;
	dma_addr_t dma_addr_buf[PAM_FREE_FIFO_SIZE];

	struct pam_ipa_hal_proc_tag hal_ops;
};

u32 pam_ipa_init_api(struct pam_ipa_hal_proc_tag *ops);
u32 pam_ipa_init(struct pam_ipa_cfg_tag *cfg);
int pam_ipa_set_enabled(struct pam_ipa_cfg_tag *cfg);
int pam_ipa_on_miniap_ready(struct sipa_to_pam_info *remote_cfg);

#endif
