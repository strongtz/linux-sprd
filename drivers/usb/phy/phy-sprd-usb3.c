/**
 * phy-sprd-usb3.c - Spreadtrum USB3 PHY Glue layer
 *
 * Copyright (c) 2018 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * Author: Miao Zhu <miao.zhu@spreadtrum.com>
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
#include <dt-bindings/soc/sprd,roc1-mask.h>
#include <dt-bindings/soc/sprd,roc1-regs.h>

#define PHY_REG_BASE			(phy->base)

/* PHY registers offset */
#define RST_CTRL			(PHY_REG_BASE + 0x00)
#define PWR_CTRL			(PHY_REG_BASE + 0x08)
#define PHY_TUNE1			(PHY_REG_BASE + 0xe4)
#define PHY_TUNE2			(PHY_REG_BASE + 0xe8)
#define PHY_TEST			(PHY_REG_BASE + 0xec)
#define PHY_CTRL1			(PHY_REG_BASE + 0xf0)
#define PHY_CTRL2			(PHY_REG_BASE + 0xf4)
#define PHY_DBG1			(PHY_REG_BASE + 0xf8)
#define PHY_DBG2			(PHY_REG_BASE + 0xfc)
#define PHY_CTRL3			(PHY_REG_BASE + 0x214)

/* PHY registers bits */
#define CORE_SOFT_OFFSET		4
#define CORE_SOFT_RST			BIT(1)
#define PHY_SOFT_RST			BIT(1)
#define PHY_PS_PD			BIT(18)

/* USB3.0_PHY_TEST */
#define PIPEP_POWERPRESENT_CFG_SEL	BIT(15)
#define PIPEP_POWERPRESENT_REG		BIT(14)
#define POWERDOWN_HSP			BIT(2)
#define POWERDOWN_SSP			BIT(1)

/* USB3.0_PHY_CTRL1 */
#define FSEL(x)				((x) & GENMASK(28，23))
#define MPLL_MULTIPLIER(x)		((x) & GENMASK(22，16))
#define REF_CLKDIV2			BIT(15)
#define REF_SSP_EN			BIT(14)
#define SSC_REF_CLK_SEL(x)		((x) & GENMASK(9，0))

/* USB3.0_PHY_CTRL3 */
#define DIGPWERENSSP			BIT(3)
#define DIGPWERENHSP0			BIT(2)
#define DIGOUTISOENSSP			BIT(1)
#define DIGOUTISOENHSP0			BIT(0)

struct sprd_ssphy {
	struct usb_phy		phy;
	void __iomem		*base;
	struct regmap		*ipa_ahb;
	struct regmap		*aon_apb;
	struct regmap		*ana_g3;
	struct regmap		*pmu_apb;
	struct regmap		*anatop;
	struct regmap		*anatop1;
	struct regmap           *pmic;
	struct regulator	*vdd;
	u32			vdd_vol;
	u32			phy_tune1;
	u32			phy_tune2;
	u32			revision;

#define TSMC_REVISION_28NM_HPM		0x5533286e
#define TSMC_REVISION_16NM_FFPLL	0x5533166e

	atomic_t		reset;
	atomic_t		inited;
	atomic_t		susped;
	bool			is_host;
};

/* Rest USB Core*/
static inline void sprd_ssphy_reset_core(struct sprd_ssphy *phy)
{
	u32 reg, msk;

	/* Purpose: To soft-reset USB control */
	reg = msk = MASK_IPA_AHB_USB_SOFT_RST | MASK_IPA_AHB_PAM_U3_SOFT_RST;
	regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_IPA_RST, msk, reg);

	/* Reset PHY */
	reg = msk = MASK_AON_APB_OTG_PHY_SOFT_RST |
			 MASK_AON_APB_OTG_UTMI_SOFT_RST;
	regmap_update_bits(phy->aon_apb, REG_AON_APB_APB_RST1,
		msk, reg);
	/*
	 *Reset signal should hold on for a while
	 *to issue resret process reliable.
	 */
	usleep_range(20000, 30000);
	reg = msk = MASK_IPA_AHB_USB_SOFT_RST | MASK_IPA_AHB_PAM_U3_SOFT_RST;
	regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_IPA_RST, msk, 0);
	reg = msk = MASK_AON_APB_OTG_PHY_SOFT_RST |
			 MASK_AON_APB_OTG_UTMI_SOFT_RST;
	regmap_update_bits(phy->aon_apb, REG_AON_APB_APB_RST1, msk, 0);
}

/* Reset USB Core */
static int sprd_ssphy_reset(struct usb_phy *x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);

	sprd_ssphy_reset_core(phy);
	return 0;
}

static int sprd_ssphy_set_vbus(struct usb_phy *x, int on)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);
	u32 reg, msk;
	int ret = 0;

	if (on) {
		/* set USB connector type is A-type*/
		msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->aon_apb,
			REG_AON_APB_OTG_PHY_CTRL, msk, 0);

		msk = MASK_ANLG_PHY_TOP_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_TOP_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->anatop,
			REG_ANLG_PHY_TOP_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		/* the pull down resistance on D-/D+ enable */
		msk = MASK_ANLG_PHY_TOP_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_TOP_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->anatop,
			REG_ANLG_PHY_TOP_ANALOG_USB20_USB20_UTMI_CTL2_TOP,
			msk, msk);

		ret |= regmap_read(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, &reg);
		msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_RESERVED;
		reg &= ~msk;
		reg |= 0x200;
		ret |= regmap_write(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, reg);
		phy->is_host = true;
	} else {
		reg = msk = MASK_AON_APB_USB2_PHY_IDDIG;
		ret |= regmap_update_bits(phy->aon_apb,
			REG_AON_APB_OTG_PHY_CTRL, msk, reg);

		msk = MASK_ANLG_PHY_TOP_DBG_SEL_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_TOP_DBG_SEL_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->anatop,
			REG_ANLG_PHY_TOP_ANALOG_USB20_REG_SEL_CFG_0,
			msk, msk);

		/* the pull down resistance on D-/D+ enable */
		msk = MASK_ANLG_PHY_TOP_ANALOG_USB20_USB20_DMPULLDOWN |
			MASK_ANLG_PHY_TOP_ANALOG_USB20_USB20_DPPULLDOWN;
		ret |= regmap_update_bits(phy->anatop,
			REG_ANLG_PHY_TOP_ANALOG_USB20_USB20_UTMI_CTL2_TOP,
			msk, 0);

		ret |= regmap_read(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, &reg);
		msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_RESERVED;
		reg &= ~msk;
		ret |= regmap_write(phy->ana_g3,
			REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, reg);
		phy->is_host = false;
	}

	return ret;
}

static int sprd_ssphy_init(struct usb_phy *x)
{
	struct sprd_ssphy *phy = container_of(x, struct sprd_ssphy, phy);
	u32	reg, msk;
	int	ret;

	if (atomic_read(&phy->inited)) {
		dev_info(x->dev, "%s is already inited!\n", __func__);
		return 0;
	}

	/*
	 * Due to chip design, some chips may turn on vddusb by default,
	 * We MUST avoid turning it on twice.
	 */
	if (phy->vdd) {
		ret = regulator_enable(phy->vdd);
		if (ret < 0)
			return ret;
	}

	/* USB2/USB3 PHY power on */
	reg = msk = MASK_PMU_APB_USB3_PHY_PD_REG | MASK_PMU_APB_USB2_PHY_PD_REG;
	ret |= regmap_update_bits(phy->pmu_apb, REG_PMU_APB_ANALOG_PHY_PD_CFG,
			 msk, 0);

	/* disable IPA_SYS shutdown */
	reg = msk = MASK_PMU_APB_PD_IPA_SYS_FORCE_SHUTDOWN;
	ret |= regmap_update_bits(phy->pmu_apb, REG_PMU_APB_PD_IPA_SYS_CFG,
			 msk, 0);

	/* disable low power */
	reg = msk = MASK_IPA_AHB_MAIN_M7_LP_EB;
	ret |= regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_M7_LPC,
			 msk, 0);

	if (!phy->pmic) {
		/*
		 *In FPGA platform, Disable low power will take some time
		 *before the DWC3 Core register is accessible.
		 */
		usleep_range(1000, 2000);
	}

	/* select the IPA_SYS USB controller */
	reg = msk = MASK_AON_APB_USB20_CTRL_MUX_REG;
	ret |= regmap_update_bits(phy->aon_apb, REG_AON_APB_AON_SOC_USB_CTRL,
			 msk, reg);

	/* purpose: set bvalid vbus_valid and clear vbus_valid_sel */
	reg = msk = MASK_IPA_AHB_UTMISRP_BVALID_REG
		 | MASK_IPA_AHB_OTG_VBUS_VALID_PHYREG;
	ret |= regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_USB_CTL0, msk, reg);

	msk = MASK_IPA_AHB_OTG_VBUS_VALID_PHYREG_SEL;
	ret |= regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_USB_CTL0, msk, 0);

	/* disable power management event */
	msk = MASK_IPA_AHB_PME_EN;
	ret |= regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_USB_CTL0, msk, 0);

	/* set power present to default value */
	msk = MASK_AON_APB_USB30_POWER_PRESENT;
	ret |= regmap_update_bits(phy->aon_apb, REG_AON_APB_OTG_PHY_TEST,
				  msk, 0);

	/* enable USB2 PHY 16bit */
	reg = msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_DATABUS16_8;
	ret |= regmap_update_bits(phy->ana_g3,
		 REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, msk, reg);

	/* vbus valid */
	reg = msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	ret |= regmap_update_bits(phy->aon_apb, REG_AON_APB_OTG_PHY_TEST,
		 msk, reg);

	reg = msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_VBUSVLDEXT;
	ret |= regmap_update_bits(phy->ana_g3,
			 REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1,
			 msk, reg);

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
	/* vbus invalid */
	msk = MASK_AON_APB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->aon_apb, REG_AON_APB_OTG_PHY_TEST, msk, 0);

	msk = MASK_ANLG_PHY_G3_ANALOG_USB20_USB20_VBUSVLDEXT;
	regmap_update_bits(phy->ana_g3,
			 REG_ANLG_PHY_G3_ANALOG_USB20_USB20_UTMI_CTL1, msk, 0);

	/* dwc3 vbus invalid */
	reg = msk = MASK_IPA_AHB_UTMISRP_BVALID_REG
		 | MASK_IPA_AHB_OTG_VBUS_VALID_PHYREG;
	regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_USB_CTL0, msk, 0);

	reg = msk = MASK_IPA_AHB_OTG_VBUS_VALID_PHYREG_SEL;
	regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_USB_CTL0, msk, reg);

	/* USB2/USB3 PHY power off */
	reg = msk = MASK_PMU_APB_USB3_PHY_PD_REG | MASK_PMU_APB_USB2_PHY_PD_REG;
	regmap_update_bits(phy->pmu_apb, REG_PMU_APB_ANALOG_PHY_PD_CFG,
			 msk, reg);

	/*
	 * Due to chip design, some chips may turn on vddusb by default,
	 * We MUST avoid turning it off twice.
	 */
	if (phy->vdd)
		regulator_disable(phy->vdd);

	atomic_set(&phy->inited, 0);
	atomic_set(&phy->reset, 0);
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
	struct sprd_ssphy *phy = container_of(usb_phy, struct sprd_ssphy, phy);

	if (phy->is_host) {
		dev_info(usb_phy->dev, "USB PHY is host mode\n");
		return 0;
	}

	if (event) {
		usb_phy_set_charger_state(usb_phy, USB_CHARGER_PRESENT);
	} else {
		u32 msk = MASK_IPA_AHB_UTMISRP_BVALID_REG |
			MASK_IPA_AHB_OTG_VBUS_VALID_PHYREG;

		/* dwc3 vbus invalid */
		if (atomic_read(&phy->inited))
			regmap_update_bits(phy->ipa_ahb, REG_IPA_AHB_USB_CTL0,
					   msk, 0);

		usb_phy_set_charger_state(usb_phy, USB_CHARGER_ABSENT);
	}

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
	struct resource *res;
	int ret;
	u32 msk, reg;

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
			phy->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
			if (!phy->pmic)
				dev_warn(dev, "unable to get pmic regmap device\n");
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "phy_glb_regs");
	if (!res) {
		dev_err(dev, "missing USB PHY registers resource\n");
		return -ENODEV;
	}

	phy->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (IS_ERR(phy->base))
		return PTR_ERR(phy->base);

	phy->ipa_ahb = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-ipa-ahb");
	if (IS_ERR(phy->ipa_ahb)) {
		dev_err(dev, "failed to map ipa registers (via syscon)\n");
		return PTR_ERR(phy->ipa_ahb);
	}
	phy->aon_apb = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-aon_apb");
	if (IS_ERR(phy->aon_apb)) {
		dev_err(dev, "failed to map aon registers (via syscon)\n");
		return PTR_ERR(phy->aon_apb);
	}
	phy->ana_g3 = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-anag3");
	if (IS_ERR(phy->ana_g3)) {
		dev_err(dev, "failed to map anag3 registers (via syscon)\n");
		return PTR_ERR(phy->ana_g3);
	}
	phy->pmu_apb = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "sprd,syscon-pmu-apb");
	if (IS_ERR(phy->pmu_apb)) {
		dev_err(dev, "failed to map pmu registers (via syscon)\n");
		return PTR_ERR(phy->pmu_apb);
	}
	phy->anatop = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anatop");
	if (IS_ERR(phy->anatop)) {
		dev_err(&pdev->dev, "ap USB anatop syscon failed!\n");
		return PTR_ERR(phy->anatop);
	}

	phy->anatop1 = syscon_regmap_lookup_by_phandle(dev->of_node,
				 "sprd,syscon-anatop1");
	if (IS_ERR(phy->anatop1)) {
		dev_err(&pdev->dev, "ap USB anatop1 syscon failed!\n");
		return PTR_ERR(phy->anatop1);
	}

	ret = of_property_read_u32(dev->of_node, "sprd,vdd-voltage",
				   &phy->vdd_vol);
	if (ret < 0) {
		dev_warn(dev, "unable to read ssphy vdd voltage\n");
		phy->vdd_vol = 3300;
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

	if (!phy->pmic) {
		/*
		 * USB PHY must init before DWC3 phy setup in haps,
		 * otherwise dwc3 phy setting will be cleared
		 * because IPA_ATH_USB_RESET  reset dwc3 PHY setting.
		 */
		sprd_ssphy_init(&phy->phy);
	}

	/* select the IPA_SYS USB controller */
	reg = msk = MASK_AON_APB_USB20_CTRL_MUX_REG;
	ret |= regmap_update_bits(phy->aon_apb, REG_AON_APB_AON_SOC_USB_CTRL,
			 msk, reg);

	platform_set_drvdata(pdev, phy);
	phy->phy.dev				= dev;
	phy->phy.label				= "sprd-ssphy";
	phy->phy.init				= sprd_ssphy_init;
	phy->phy.shutdown			= sprd_ssphy_shutdown;
	phy->phy.reset_phy			= sprd_ssphy_reset;
	phy->phy.set_vbus			= sprd_ssphy_set_vbus;
	phy->phy.type				= USB_PHY_TYPE_USB3;
	phy->phy.vbus_nb.notifier_call		= sprd_ssphy_vbus_notify;
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
	{ .compatible = "sprd,roc1-ssphy" },
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

MODULE_ALIAS("platform:sprd-ssphy	");
MODULE_AUTHOR("Miao Zhu <miao.zhu@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 SPRD PHY Glue Layer");
