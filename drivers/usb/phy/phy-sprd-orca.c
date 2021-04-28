/**
 * phy-sprd-usb3.c - Spreadtrum USB3 PHY Glue layer
 *
 * Copyright (c) 2019 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * Author: Jiayong Yang <jiayong.yang@unsoc.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>
#include <linux/mfd/syscon.h>
#include <linux/power/sc2730-usb-charger.h>
#include <dt-bindings/soc/sprd,orca-mask.h>
#include <dt-bindings/soc/sprd,orca-regs.h>

struct sprd_ssphy {
	struct usb_phy		phy;
	struct regmap		*ipa_ahb;
	struct regmap		*aon_apb;
	struct regmap		*ana_g4;
	struct regmap           *pmic;
	struct regulator	*vdd;
	u32			vdd_vol;
	u32			phy_tune1;
	u32			phy_tune2;
	u32			revision;
	atomic_t		inited;

#define	USB_HOST_MODE	1
#define USB_DEV_MODE	0
	u32			mode;
};

#define USB20_0_TFREGRES_VALUE    (0x1 << 29)
#define USB20_0_TUNEHSAMP_VALUE   (0x11 << 23)
#define USB20_1_TFREGRES_VALUE    (0x1 << 29)
#define USB20_1_TUNEHSAMP_VALUE   (0x11 << 23)

/* Rest USB Core*/
static void sprd_ssphy_reset_core(struct sprd_ssphy *phy)
{
	u32 reg, msk;

	/* Purpose: To soft-reset USB control */
	reg = msk = MASK_AP_IPA_AHB_RF_USB1_SOFT_RST |
		    MASK_AP_IPA_AHB_RF_USB_PAM_SOFT_RST;
	regmap_update_bits(phy->ipa_ahb, REG_AP_IPA_AHB_RF_AHB_RST, msk, reg);

	/*
	 * Reset signal should hold on for a while
	 * to insure resret process reliable.
	 */
	usleep_range(20000, 30000);
	msk = MASK_AP_IPA_AHB_RF_USB1_SOFT_RST |
	      MASK_AP_IPA_AHB_RF_USB_PAM_SOFT_RST;
	regmap_update_bits(phy->ipa_ahb, REG_AP_IPA_AHB_RF_AHB_RST, msk, 0);
}

/* Reset USB Core */
static int sprd_ssphy_reset(struct usb_phy *x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);

	sprd_ssphy_reset_core(phy);
	return 0;
}

static int sprd_ssphy_init(struct usb_phy *x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);
	u32	reg, msk;
	int	ret = 0;

	if (atomic_read(&phy->inited)) {
		dev_info(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	if (phy->vdd) {
		ret = regulator_enable(phy->vdd);
		if (ret < 0)
			return ret;
	}

	/* USB3 PHY power on */
	ret |= regmap_read(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_ANA_USB30_CTRL1, &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_USB30_PS_PD_S |
	      MASK_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_USB30_PS_PD_L |
	      MASK_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_USB30_ISO_SW_EN;
	reg &= ~msk;
	ret |= regmap_write(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_ANA_USB30_CTRL1, reg);

	/* USB2 PHY power on */
	ret |= regmap_read(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_BATTER_PLL, &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_PS_PD_S |
		    MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_PS_PD_L;
	reg &= ~msk;
	ret |= regmap_write(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_BATTER_PLL, reg);

	ret |= regmap_read(phy->ana_g4,
			REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_ISO_SW, &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_ISO_SW_EN;
	reg &= ~msk;
	ret |= regmap_write(phy->ana_g4,
			REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_ISO_SW, reg);

	/* purpose: set bvalid vbus_valid and clear vbus_valid_sel */
	reg = msk = MASK_AP_IPA_AHB_RF_UTMISRP_BVALID_REG1 |
		    MASK_AP_IPA_AHB_RF_OTG_VBUS_VALID_PHYREG1 |
		    MASK_AP_IPA_AHB_RF_PIPE3_POWERPRESENT1;
	ret |= regmap_update_bits(phy->ipa_ahb, REG_AP_IPA_AHB_RF_USB1_CTRL,
		    msk, reg);

	msk = MASK_AP_IPA_AHB_RF_OTG_VBUS_VALID_PHYREG_SEL1;
	ret |= regmap_update_bits(phy->ipa_ahb, REG_AP_IPA_AHB_RF_USB1_CTRL,
		    msk, 0);

	/* disable power management event */
	msk = MASK_AP_IPA_AHB_RF_PME_EN1;
	ret |= regmap_update_bits(phy->ipa_ahb, REG_AP_IPA_AHB_RF_USB1_CTRL,
		    msk, 0);

	/* enable USB2 PHY 16bit */
	reg = msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_DATABUS16_8 |
		    MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_VBUSVLDEXT;
	ret |= regmap_update_bits(phy->ana_g4,
		    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL1,
		    msk, reg);

	ret |= regmap_read(phy->ana_g4,
			   REG_ANLG_PHY_G4_RF_ANALOG_USB20_0_USB20_TRIMMING,
			   &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_0_USB20_TUNEHSAMP |
		MASK_ANLG_PHY_G4_RF_ANALOG_USB20_0_USB20_TFREGRES;
	reg &= ~msk;
	reg |= USB20_0_TUNEHSAMP_VALUE | USB20_0_TFREGRES_VALUE;
	ret |= regmap_write(phy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_0_USB20_TRIMMING,
			    reg);

	ret |= regmap_read(phy->ana_g4,
			   REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_TRIMMING,
			   &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_TUNEHSAMP |
		MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_TFREGRES;
	reg &= ~msk;
	reg |= USB20_1_TUNEHSAMP_VALUE | USB20_1_TFREGRES_VALUE;
	ret |= regmap_write(phy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_TRIMMING,
			    reg);

	/* Reset PHY */
	sprd_ssphy_reset_core(phy);

	atomic_set(&phy->inited, 1);

	return ret;
}

/* Turn off PHY and core */
static void sprd_ssphy_shutdown(struct usb_phy *x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);
	u32 msk, reg;

	if (!atomic_read(&phy->inited)) {
		dev_dbg(x->dev, "%s is already shut down\n", __func__);
		return;
	}

	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL1, msk, 0);

	/* purpose: set bvalid vbus_valid and clear vbus_valid_sel */
	msk = MASK_AP_IPA_AHB_RF_UTMISRP_BVALID_REG1 |
		    MASK_AP_IPA_AHB_RF_OTG_VBUS_VALID_PHYREG1 |
		    MASK_AP_IPA_AHB_RF_PIPE3_POWERPRESENT1;
	regmap_update_bits(phy->ipa_ahb, REG_AP_IPA_AHB_RF_USB1_CTRL, msk, 0);

	/* USB3 PHY power off */
	regmap_read(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_ANA_USB30_CTRL1, &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_USB30_PS_PD_S |
		MASK_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_USB30_PS_PD_L |
		MASK_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_USB30_ISO_SW_EN;
	reg |= msk;
	regmap_write(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB3_TYPEC_1_ANA_USB30_CTRL1, reg);

	/* USB2 PHY power off */
	regmap_read(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL1, &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_PS_PD_S |
	      MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_PS_PD_L;
	reg |= msk;
	regmap_write(phy->ana_g4,
		REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL1, reg);

	regmap_read(phy->ana_g4,
			REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_ISO_SW, &reg);
	msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_ISO_SW_EN;
	reg |= msk;
	regmap_write(phy->ana_g4,
			REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_ISO_SW, reg);

	if (phy->vdd)
		regulator_disable(phy->vdd);
	atomic_set(&phy->inited, 0);
}

static ssize_t phy_tune1_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);

	return sprintf(buf, "0x%x\n", phy->phy_tune1);
}

static int sprd_ssphy_id_notifier(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	struct usb_phy *usbphy = container_of(nb, struct usb_phy, id_nb);
	struct sprd_ssphy *ssphy = container_of(usbphy, struct sprd_ssphy, phy);
	u32	reg, msk;

	if (event) {
		ssphy->mode = USB_HOST_MODE;

		msk = MASK_AON_APB_RF_USB2_PHY_IDDIG;
		regmap_update_bits(ssphy->aon_apb, REG_AON_APB_RF_OTG_PHY_CTRL,
			    msk, 0);

		reg = msk =
		  MASK_ANLG_PHY_G4_RF_DBG_SEL_ANALOG_USB20_1_USB20_DPPULLDOWN |
		  MASK_ANLG_PHY_G4_RF_DBG_SEL_ANALOG_USB20_1_USB20_DMPULLDOWN;
		regmap_update_bits(ssphy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_REG_SEL_CFG_0,
			    msk, reg);

		reg = msk =
		   MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_DPPULLDOWN |
		   MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_DMPULLDOWN;
		regmap_update_bits(ssphy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL2,
			    msk, reg);

		reg = 0x200;
		msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_RESERVED;
		regmap_update_bits(ssphy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL1,
			    msk, reg);
	} else {
		reg = msk =
		  MASK_ANLG_PHY_G4_RF_DBG_SEL_ANALOG_USB20_1_USB20_DPPULLDOWN |
		  MASK_ANLG_PHY_G4_RF_DBG_SEL_ANALOG_USB20_1_USB20_DMPULLDOWN;
		regmap_update_bits(ssphy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_REG_SEL_CFG_0,
			    msk, 0);

		reg = msk =
		  MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_DPPULLDOWN |
		  MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_DMPULLDOWN;
		regmap_update_bits(ssphy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL2,
			    msk, 0);

		msk = MASK_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_RESERVED;
		regmap_update_bits(ssphy->ana_g4,
			    REG_ANLG_PHY_G4_RF_ANALOG_USB20_1_USB20_UTMI_CTL1,
			    msk, 0);

		reg = msk = MASK_AON_APB_RF_USB2_PHY_IDDIG;
		regmap_update_bits(ssphy->aon_apb, REG_AON_APB_RF_OTG_PHY_CTRL,
				msk, reg);

		ssphy->mode = USB_DEV_MODE;
	}
	return 0;
}

static ssize_t phy_tune1_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);
	if (kstrtouint(buf, 16, &phy->phy_tune1) < 0)
		return -EINVAL;

	return size;
}
static DEVICE_ATTR_RW(phy_tune1);

static ssize_t phy_tune2_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);

	return sprintf(buf, "0x%x\n", phy->phy_tune2);
}

static ssize_t phy_tune2_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);
	if (kstrtouint(buf, 16, &phy->phy_tune2) < 0)
		return -EINVAL;

	return size;
}
static DEVICE_ATTR_RW(phy_tune2);

static ssize_t vdd_voltage_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);

	return sprintf(buf, "%d\n", phy->vdd_vol);
}

static ssize_t vdd_voltage_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_ssphy *phy;
	u32 vol;

	if (!x)
		return -EINVAL;

	phy = container_of(x, struct sprd_ssphy, phy);
	if (kstrtouint(buf, 16, &vol) < 0)
		return -EINVAL;

	if (vol < 1200000 || vol > 3750000)
		return -EINVAL;

	phy->vdd_vol = vol;

	return size;
}
static DEVICE_ATTR_RW(vdd_voltage);

static struct attribute *usb_ssphy_attrs[] = {
	&dev_attr_phy_tune1.attr,
	&dev_attr_phy_tune2.attr,
	&dev_attr_vdd_voltage.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_ssphy);

static int sprd_ssphy_vbus_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);

	if (event)
		usb_phy_set_charger_state(usb_phy, USB_CHARGER_PRESENT);
	else
		usb_phy_set_charger_state(usb_phy, USB_CHARGER_ABSENT);

	return 0;
}

static enum usb_charger_type sprd_ssphy_charger_detect(struct usb_phy *x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);

	if (!phy->pmic)
		return UNKNOWN_TYPE;
	return sc27xx_charger_detect(phy->pmic);
}

static int sprd_ssphy_probe(struct platform_device *pdev)
{
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sprd_ssphy *phy;
	struct device *dev = &pdev->dev;
	u32 reg, msk;
	int ret;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_warn(dev, "unable to get syscon node\n");
	} else {
		regmap_pdev = of_find_device_by_node(regmap_np);
		if (!regmap_pdev) {
			of_node_put(regmap_np);
			dev_warn(dev, "unable to get syscon platform device\n");
			phy->pmic = NULL;
		} else {
			phy->pmic =
				 dev_get_regmap(regmap_pdev->dev.parent, NULL);
			if (!phy->pmic)
				dev_warn(dev, "unable to get pmic device\n");
		}
	}

	phy->ipa_ahb = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-ipa-ahb");
	if (IS_ERR(phy->ipa_ahb)) {
		dev_err(dev, "failed to map ipa registers (via syscon)\n");
		return PTR_ERR(phy->ipa_ahb);
	}
	phy->aon_apb = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-aon-apb");
	if (IS_ERR(phy->aon_apb)) {
		dev_err(dev, "failed to map aon registers (via syscon)\n");
		return PTR_ERR(phy->aon_apb);
	}
	phy->ana_g4 = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-anag4");
	if (IS_ERR(phy->ana_g4)) {
		dev_err(dev, "failed to map anag4 registers (via syscon)\n");
		return PTR_ERR(phy->ana_g4);
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_warn(dev, "unable to read ssphy vdd voltage\n");
		phy->vdd_vol = 330000;
	}

	phy->vdd = devm_regulator_get_optional(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_warn(dev, "unable to get ssphy vdd supply\n");
		phy->vdd = NULL;
	} else {
		ret = regulator_set_voltage(phy->vdd, phy->vdd_vol,
				 phy->vdd_vol);
		if (ret < 0) {
			dev_warn(dev, "fail to set ssphy vdd voltage:%dmV\n",
				phy->vdd_vol);
		}
	}

	/* vbus valid */
	reg = msk = MASK_AON_APB_RF_OTG_VBUS_VALID_PHYREG;
	ret = regmap_update_bits(phy->aon_apb, REG_AON_APB_RF_OTG_PHY_TEST,
		    msk, reg);
	if (ret) {
		dev_err(dev, "fail to write phy register\n");
		return ret;
	}

	if (!phy->pmic) {
		/*
		 * USB PHY must init before DWC3 phy setup in haps,
		 * otherwise dwc3 phy setting will be cleared
		 * because IPA_ATH_USB_RESET  reset dwc3 PHY setting.
		 */
		sprd_ssphy_init(&phy->phy);
	} else {
		/*
		 * USB PHY must reset before DWC3 phy setup,
		 * otherwise dwc3 controller registers are empty.
		 */
		sprd_ssphy_reset(&phy->phy);
	}

	/* enable otg utmi and analog */

	platform_set_drvdata(pdev, phy);
	phy->phy.dev				= dev;
	phy->phy.label				= "sprd-ssphy";
	phy->phy.init				= sprd_ssphy_init;
	phy->phy.shutdown			= sprd_ssphy_shutdown;
	phy->phy.reset_phy			= sprd_ssphy_reset;
	phy->phy.type				= USB_PHY_TYPE_USB3;
	phy->phy.vbus_nb.notifier_call		= sprd_ssphy_vbus_notify;
	phy->phy.id_nb.notifier_call 		= sprd_ssphy_id_notifier;
	phy->phy.charger_detect			= sprd_ssphy_charger_detect;
	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	ret = sysfs_create_groups(&dev->kobj, usb_ssphy_groups);
	if (ret)
		dev_warn(dev, "failed to create usb ssphy attributes\n");

	if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0)
		usb_phy_set_charger_state(&phy->phy, USB_CHARGER_PRESENT);

	return 0;
}

static int sprd_ssphy_remove(struct platform_device *pdev)
{
	struct sprd_ssphy *phy = platform_get_drvdata(pdev);

	sysfs_remove_groups(&pdev->dev.kobj, usb_ssphy_groups);
	usb_remove_phy(&phy->phy);
	regulator_disable(phy->vdd);
	return 0;
}

static const struct of_device_id sprd_ssphy_match[] = {
	{ .compatible = "sprd,orca1-ssphy1" },
	{},
};

MODULE_DEVICE_TABLE(of, sprd_ssphy_match);

static struct platform_driver sprd_ssphy_driver = {
	.probe		= sprd_ssphy_probe,
	.remove		= sprd_ssphy_remove,
	.driver		= {
		.name	= "sprd-ssphy",
		.of_match_table = sprd_ssphy_match,
	},
};

static int __init sprd_ssphy_driver_init(void)
{
	return platform_driver_register(&sprd_ssphy_driver);
}

static void __exit sprd_ssphy_driver_exit(void)
{
	platform_driver_unregister(&sprd_ssphy_driver);
}

late_initcall(sprd_ssphy_driver_init);
module_exit(sprd_ssphy_driver_exit);

MODULE_ALIAS("platform:sprd-ssphy");
MODULE_AUTHOR("Jiayong Yang <jiayong.yang@unisoc.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 SPRD PHY Glue Layer");
