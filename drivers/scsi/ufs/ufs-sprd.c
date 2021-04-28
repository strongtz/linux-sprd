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

#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/time.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufshci.h"
#include "ufs-sprd.h"
#include "ufs_quirks.h"
#include "unipro.h"

/**
 * ufs_sprd_rmwl - read modify write into a register
 * @base - base address
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @reg - register address
 */
static inline void ufs_sprd_rmwl(void __iomem *base, u32 mask, u32 val, u32 reg)
{
	u32 tmp;

	tmp = readl((base) + (reg));
	tmp &= ~mask;
	tmp |= (val & mask);
	writel(tmp, (base) + (reg));
}

static int ufs_sprd_get_syscon_reg(struct device_node *np,
	struct syscon_ufs *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];
	int ret;

	regmap = syscon_regmap_lookup_by_name(np, name);
	if (IS_ERR(regmap)) {
		pr_err("read ufs syscon %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		return -EINVAL;
	}

	ret = syscon_get_args_by_name(np, name, 2, syscon_args);
	if (ret < 0)
		return ret;
	else if (ret != 2) {
		pr_err("read ufs syscon %s fail,ret = %d\n", name, ret);
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

void ufs_sprd_reset(struct ufs_sprd_host *host)
{
	int val = 0, mask = 0;

	dev_info(host->hba->dev, "ufs hardware reset!\n");

	/* TODO: HW reset will be simple in next version. */
	/* Configs need strict squence. */
	regmap_update_bits(host->anlg_mphy_ufs_rst.regmap,
			   host->anlg_mphy_ufs_rst.reg,
			   host->anlg_mphy_ufs_rst.mask,
			   0);

	regmap_update_bits(host->anlg_mphy_ufs_rst.regmap,
			   host->anlg_mphy_ufs_rst.reg,
			   host->anlg_mphy_ufs_rst.mask,
			   host->anlg_mphy_ufs_rst.mask);

	regmap_update_bits(host->ap_apb_ufs_rst.regmap,
			   host->ap_apb_ufs_rst.reg,
			   host->ap_apb_ufs_rst.mask,
			   host->ap_apb_ufs_rst.mask);

	regmap_update_bits(host->ap_apb_ufs_rst.regmap,
			   host->ap_apb_ufs_rst.reg,
			   host->ap_apb_ufs_rst.mask,
			   0);

	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   host->aon_apb_ufs_rst.mask);

	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   0);

	val = mask = DL_RST;
	ufs_sprd_rmwl(host->unipro_reg, mask, val, REG_DL_0);
	ufs_sprd_rmwl(host->unipro_reg, mask, 0, REG_DL_0);

	val = mask = N_RST;
	ufs_sprd_rmwl(host->unipro_reg, mask, val, REG_N_1);
	ufs_sprd_rmwl(host->unipro_reg, mask, 0, REG_N_1);

	val = mask = T_RST;
	ufs_sprd_rmwl(host->unipro_reg, mask, val, REG_T_9);
	ufs_sprd_rmwl(host->unipro_reg, mask, 0, REG_T_9);

	val = mask = DME_RST;
	ufs_sprd_rmwl(host->unipro_reg, mask, val, REG_DME_0);
	ufs_sprd_rmwl(host->unipro_reg, mask, 0, REG_DME_0);

	val = mask = HCI_RST | HCI_CLOD_RST;
	ufshcd_rmwl(host->hba, mask, val, REG_SW_RST);

	val = mask = TX_RSTZ | RX_RSTZ;
	ufs_sprd_rmwl(host->ufsutp_reg, mask, 0, REG_UTP_MISC);
	ufs_sprd_rmwl(host->ufsutp_reg, mask, val, REG_UTP_MISC);

	mask = HCI_RST | HCI_CLOD_RST;
	ufshcd_rmwl(host->hba, mask, 0, REG_SW_RST);

	val = mask = XTAL_RST;
	ufs_sprd_rmwl(host->ufs_ao_reg, mask, val, REG_AO_SW_RST);
	ufs_sprd_rmwl(host->ufs_ao_reg, mask, 0, REG_AO_SW_RST);

	val = mask = RMMI_TX_L0_RST | RMMI_TX_L1_RST | RMMI_RX_L0_RST
			    | RMMI_RX_L1_RST | RMMI_CB_RST | RMMI_RST;
	ufs_sprd_rmwl(host->unipro_reg, mask, val, REG_PA_15);
	ufs_sprd_rmwl(host->unipro_reg, mask, 0, REG_PA_15);

	/* Exit hibernate after reset, and no need to care command's response. */
	ufshcd_writel(host->hba, UIC_CMD_DME_HIBER_EXIT, REG_UIC_COMMAND);

	/* Fix issue failing to change speed to fast gear3 mode. */
	val = mask = TX_FIFOMODE;
	ufs_sprd_rmwl(host->mphy_reg, mask, val, REG_DIG_CFG35);

	val = mask = CDR_MONITOR_BYPASS;
	ufs_sprd_rmwl(host->mphy_reg, mask, val, REG_DIG_CFG7);

	val = mask = RMMI_TX_DIRDY_SEL;
	ufs_sprd_rmwl(host->unipro_reg, mask, val, REG_PA_27);

	val = ufshcd_readl(host->hba, 0xa4);
	ufshcd_writel(host->hba, val | (1 << 29), 0xa4);
}

/**
 * ufs_sprd_init - find other essential mmio bases
 * @hba: host controller instance
 * Returns 0 on success, non-zero value on failure
 */
static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_host *host;
	struct resource *res;
	int ret = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	/* map ufsutp_reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufsutp_reg");
	if (!res) {
		dev_err(dev, "Missing ufs utp register resource\n");
		return -ENODEV;
	}
	host->ufsutp_reg = devm_ioremap_nocache(dev, res->start,
						resource_size(res));
	if (IS_ERR(host->ufsutp_reg)) {
		dev_err(dev, "%s: could not map ufsutp_reg, err %ld\n",
			__func__, PTR_ERR(host->ufsutp_reg));
		host->ufsutp_reg = NULL;
		return -ENODEV;
	}

	/* map unipro_reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "unipro_reg");
	if (!res) {
		dev_err(dev, "Missing unipro register resource\n");
		return -ENODEV;
	}
	host->unipro_reg = devm_ioremap_nocache(dev, res->start,
						resource_size(res));
	if (IS_ERR(host->unipro_reg)) {
		dev_err(dev, "%s: could not map unipro_reg, err %ld\n",
			__func__, PTR_ERR(host->unipro_reg));
		host->unipro_reg = NULL;
		return -ENODEV;
	}

	/* map ufs_ao_reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs_ao_reg");
	if (!res) {
		dev_err(dev, "Missing ufs_ao_reg register resource\n");
		return -ENODEV;
	}
	host->ufs_ao_reg = devm_ioremap_nocache(dev, res->start,
						resource_size(res));
	if (IS_ERR(host->ufs_ao_reg)) {
		dev_err(dev, "%s: could not map ufs_ao_reg, err %ld\n",
			__func__, PTR_ERR(host->ufs_ao_reg));
		host->ufs_ao_reg = NULL;
		return -ENODEV;
	}

	/* map mphy_reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mphy_reg");
	if (!res) {
		dev_err(dev, "Missing mphy_reg register resource\n");
		return -ENODEV;
	}
	host->mphy_reg = devm_ioremap_nocache(dev, res->start,
						resource_size(res));
	if (IS_ERR(host->mphy_reg)) {
		dev_err(dev, "%s: could not map mphy_reg, err %ld\n",
			__func__, PTR_ERR(host->mphy_reg));
		host->mphy_reg = NULL;
		return -ENODEV;
	}

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ap_apb_ufs_en,
				      "ap_apb_ufs_en");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ap_apb_ufs_rst,
				      "ap_apb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->anlg_mphy_ufs_rst,
				      "anlg_mphy_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->aon_apb_ufs_rst,
				      "aon_apb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	 host->rus = devm_ioremap_nocache(dev, 0x32080000, 0x10000);

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		       UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;

	return 0;
}

/**
 * ufs_sprd_hw_init - controller enable and reset
 * @hba: host controller instance
 */
void ufs_sprd_hw_init(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	regmap_update_bits(host->ap_apb_ufs_en.regmap,
			   host->ap_apb_ufs_en.reg,
			   host->ap_apb_ufs_en.mask,
			   host->ap_apb_ufs_en.mask);
	ufs_sprd_reset(host);
}

static void ufs_sprd_exit(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	devm_kfree(dev, host);
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	return UFSHCI_VERSION_21;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		/* Do hardware reset before host controller enable. */
		ufs_sprd_hw_init(hba);
		break;
	case POST_CHANGE:
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		/*
		 * Some UFS devices (and may be host) have issues if LCC is
		 * enabled. So we are setting PA_Local_TX_LCC_Enable to 0
		 * before link startup which will make sure that both host
		 * and device TX LCC are disabled once link startup is
		 * completed.
		 */
		if (ufshcd_get_local_unipro_ver(hba) != UFS_UNIPRO_VER_1_41)
			err = ufshcd_dme_set(hba,
					UIC_ARG_MIB(PA_LOCAL_TX_LCC_ENABLE),
					0);

		break;
	case POST_CHANGE:
		break;
	default:
		break;
	}

	return err;
}

static int ufs_sprd_pwr_change_notify(struct ufs_hba *hba,
				enum ufs_notify_change_status status,
				struct ufs_pa_layer_attr *dev_max_params,
				struct ufs_pa_layer_attr *dev_req_params)
{
	int err = 0;

	if (!dev_req_params) {
		pr_err("%s: incoming dev_req_params is NULL\n", __func__);
		err = -EINVAL;
		goto out;
	}

	switch (status) {
	case PRE_CHANGE:
		dev_req_params->gear_rx = UFS_PWM_G2;
		dev_req_params->gear_tx = UFS_PWM_G2;
		dev_req_params->lane_rx = 1;
		dev_req_params->lane_tx = 1;
		dev_req_params->pwr_rx = FAST_MODE;
		dev_req_params->pwr_tx = FAST_MODE;
		dev_req_params->hs_rate = PA_HS_MODE_A;
		break;
	case POST_CHANGE:
		break;
	default:
		err = -EINVAL;
		break;
	}

out:
	return err;
}

static void ufs_sprd_hibern8_notify(struct ufs_hba *hba,
				    enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	int val = 0, mask = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER) {
			/*
			 * Fix UIC_CMD_DME_HIBER_ENTER command fail.
			 * Set the configuration before entry,
			 * clear it after exit.
			 * Only tested on samsung device.
			 */
			val = mask = REDESKEW_MASK;
			ufs_sprd_rmwl(host->unipro_reg, mask, val, REG_PA_7);
		}
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT) {
			mask = REDESKEW_MASK;
			ufs_sprd_rmwl(host->unipro_reg, mask, 0, REG_PA_7);
		}
		break;
	default:
		break;
	}
}

/**
 * struct ufs_hba_sprd_vops - UFS sprd specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_sprd_vops = {
	.name = "sprd",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.link_startup_notify = ufs_sprd_link_startup_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
	.hibern8_notify = ufs_sprd_hibern8_notify,
};

/**
 * ufs_sprd_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_sprd_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;

	/* Perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_sprd_vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

/**
 * ufs_sprd_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always returns 0
 */
static int ufs_sprd_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);
	ufshcd_remove(hba);
	return 0;
}

static const struct of_device_id ufs_sprd_of_match[] = {
	{ .compatible = "sprd,ufshc"},
	{},
};

static const struct dev_pm_ops ufs_sprd_pm_ops = {
	.suspend = ufshcd_pltfrm_suspend,
	.resume = ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume = ufshcd_pltfrm_runtime_resume,
	.runtime_idle = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_sprd_pltform = {
	.probe = ufs_sprd_probe,
	.remove = ufs_sprd_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver = {
		.name = "ufshcd-sprd",
		.pm = &ufs_sprd_pm_ops,
		.of_match_table = of_match_ptr(ufs_sprd_of_match),
	},
};
module_platform_driver(ufs_sprd_pltform);

MODULE_DESCRIPTION("SPRD Specific UFSHCI driver");
MODULE_LICENSE("GPL v2");
