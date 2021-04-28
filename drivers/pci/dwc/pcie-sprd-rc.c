/*
 * SPRD PCIe root complex driver
 * Author:	Billows.Wu
 * Purpose:	PCI Express root complex driver.
 *		1. PCI Express Port Bus Driver can deal with AER/PME/...
 *		   SPRD PCI Express controllers don't have AER/PME/...
 *		   like standard pcie port driver. Therefore, we add this file
 *		   to deal with AER/PME/...
 *		2. When system suspend, PCIe controller will power off because
 *		   ipa sys power off. So PCIe link must be established when
 *		   system resume. This file will operate some registers about
 *		   PCI capabilities. On the other, the other registers about
 *		   PCI controller or designware will be operated in controller
 *		   driver.
 *
 * Copyright (C) 2019 Unisoc Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

static void sprd_pcie_rc_enter_l0s(struct pci_dev *pdev)
{
	u16 reg16;
	u32 reg32;

	pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &reg32);
	reg32 |= PCI_EXP_LNKCAP_ASPMS;
	pcie_capability_write_dword(pdev, PCI_EXP_LNKCAP, reg32);

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &reg16);
	reg16 |= PCI_EXP_LNKCTL_ASPM_L0S;
	pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, reg16);
}

static void sprd_pcie_rc_enter_l1(struct pci_dev *pdev)
{
	u16 reg16;
	u32 reg32;

	pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &reg32);
	reg32 |= PCI_EXP_LNKCAP_ASPMS;
	pcie_capability_write_dword(pdev, PCI_EXP_LNKCAP, reg32);

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &reg16);
	reg16 |= PCI_EXP_LNKCTL_ASPM_L1;
	pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, reg16);
}

static void sprd_pcie_rc_enter_l1_1(struct pci_dev *pdev)
{
	u16 reg16;
	u32 reg32, l1ss_cap_ptr;

	l1ss_cap_ptr = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_ptr) {
		dev_err(&pdev->dev,
			"cannot find extended capabilities about L1.1\n");
		return;
	}

	pci_read_config_dword(pdev, l1ss_cap_ptr + PCI_L1SS_CTL1, &reg32);
	reg32 |= PCI_L1SS_CTL1_ASPM_L1_1;
	pci_write_config_dword(pdev, l1ss_cap_ptr + PCI_L1SS_CTL1, reg32);

	pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &reg32);
	reg32 |= PCI_EXP_LNKCAP_ASPMS;
	pcie_capability_write_dword(pdev, PCI_EXP_LNKCAP, reg32);

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &reg16);
	reg16 |= PCI_EXP_LNKCTL_ASPM_L1;
	pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, reg16);
}

/* Note: only ASPM L1_2 need to set global registers in PCIe RC controller */
static void sprd_pcie_rc_enter_l1_2(struct pci_dev *pdev)
{
	u16 reg16;
	u32 reg32, l1ss_cap_ptr;

	l1ss_cap_ptr = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_ptr) {
		dev_err(&pdev->dev,
			"cannot find extended capabilities about L1.2\n");
		return;
	}

	pci_read_config_dword(pdev, l1ss_cap_ptr + PCI_L1SS_CTL2, &reg32);
	reg32 |= (PCI_L1SS_CTL2_T_POWER_ON_VALUE_MASK & (0x1 << 3));
	reg32 &= ~PCI_L1SS_CTL2_T_POWER_ON_SCALE_MASK;
	pci_write_config_dword(pdev, l1ss_cap_ptr + PCI_L1SS_CTL2, reg32);

	pci_read_config_dword(pdev, l1ss_cap_ptr + PCI_L1SS_CTL1, &reg32);
	reg32 |= PCI_L1SS_CTL1_ASPM_L1_2;
	reg32 |= (PCI_L1SS_COMMON_MODE_RESTORE_TIME_MASK & (0x2 << 8));
	reg32 |= (PCI_L1SS_LTR_L1_2_THRESHOLD_VALUE_MASK & (0x80 << 16));
	pci_write_config_dword(pdev, l1ss_cap_ptr + PCI_L1SS_CTL1, reg32);

	pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &reg32);
	reg32 |= PCI_EXP_LNKCAP_ASPMS;
	pcie_capability_write_dword(pdev, PCI_EXP_LNKCAP, reg32);

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &reg16);
	reg16 |= PCI_EXP_LNKCTL_ASPM_L1;
	pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, reg16);

	pcie_capability_read_word(pdev, PCI_EXP_DEVCTL2, &reg16);
	reg16 |= PCI_EXP_DEVCTL2_LTR_EN;
	pcie_capability_write_word(pdev, PCI_EXP_DEVCTL2, reg16);
}

static void sprd_pcie_aspm_init(struct pci_dev *pdev)
{
	sprd_pcie_rc_enter_l0s(pdev);
	sprd_pcie_rc_enter_l1(pdev);
	sprd_pcie_rc_enter_l1_1(pdev);
	sprd_pcie_rc_enter_l1_2(pdev);
}

static int sprd_pcie_rc_probe(struct pci_dev *pdev,
			      const struct pci_device_id *id)
{
	sprd_pcie_aspm_init(pdev);

	return 0;
}

static int sprd_pcie_rc_suspend(struct device *dev)
{
	/*
	 * When RC enter suspend, RC pcie controller will power off.
	 * ASPM capabilities will be setted to default values by hardware.
	 * Therefore, we do nothing here.
	 */

	return 0;
}

static int sprd_pcie_rc_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	sprd_pcie_aspm_init(pdev);

	return 0;
}

static const struct dev_pm_ops sprd_pcie_rc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_pcie_rc_suspend, sprd_pcie_rc_resume)
};

static const struct pci_device_id sprd_pcie_rc_ids[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_SYNOPSYS, PCI_DEVICE_ID_SPRD_RC),
		.class = (PCI_CLASS_BRIDGE_PCI << 8),
		.class_mask = ~0,
	}, {}
};
static struct pci_driver sprd_pcie_rc_driver = {
	.name = "sprd-pcie-rc",
	.id_table = &sprd_pcie_rc_ids[0],
	.probe = sprd_pcie_rc_probe,
	.driver = {
		.pm = &sprd_pcie_rc_pm_ops,
	},
};

static int __init sprd_pcie_rc_init(void)
{
	return pci_register_driver(&sprd_pcie_rc_driver);
}

static void __exit sprd_pcie_rc_exit(void)
{
	pci_unregister_driver(&sprd_pcie_rc_driver);
}

device_initcall(sprd_pcie_rc_init);
