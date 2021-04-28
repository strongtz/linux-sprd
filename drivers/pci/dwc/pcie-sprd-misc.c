/*
 * PCIe host controller driver for Spreadtrum SoCs
 *
 * Copyright (C) 2019 Spreadtrum corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/msi.h>
#include <linux/of_device.h>
#include <linux/pcie-rc-sprd.h>
#include <linux/platform_device.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

#define NUM_OF_ARGS 5

static void sprd_pcie_fix_class(struct pci_dev *dev)
{
	struct pcie_port *pp = dev->bus->sysdata;

	if (dev->class != PCI_CLASS_NOT_DEFINED)
		return;

	if (dev->bus->number == pp->root_bus_nr)
		dev->class = 0x0604 << 8;
	else
		dev->class = 0x080d << 8;

	dev_info(&dev->dev,
		 "The class of device %04x:%04x is changed to: 0x%06x\n",
		 dev->device, dev->vendor, dev->class);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SYNOPSYS, 0xabcd, sprd_pcie_fix_class);

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *env)
{
	struct device_node *np = pdev->dev.of_node;
	int i, count, err, j;
	u32 type, delay, reg, mask, val, tmp_val;
	struct of_phandle_args out_args;
	struct regmap *iomap;

	if (!of_find_property(np, env, NULL)) {
		dev_info(&pdev->dev,
			 "there isn't property %s in dts\n", env);
		return 0;
	}

	/* one handle and NUM_OF_ARGS args */
	count = of_property_count_elems_of_size(np, env,
		(NUM_OF_ARGS + 1) * sizeof(u32));
	dev_dbg(&pdev->dev, "property (%s) reg count is %d :\n", env, count);

	for (i = 0; i < count; i++) {
		err = of_parse_phandle_with_fixed_args(np, env, NUM_OF_ARGS,
				i, &out_args);
		if (err < 0)
			return err;

		type = out_args.args[0];
		delay = out_args.args[1];
		reg = out_args.args[2];
		mask = out_args.args[3];
		val = out_args.args[4];

		iomap = syscon_node_to_regmap(out_args.np);
		switch (type) {
		case 0:
			regmap_update_bits(iomap, reg, mask, val);
			break;

		case 1:
			regmap_read(iomap, reg, &tmp_val);
			tmp_val &= (~mask);
			tmp_val |= (val & mask);
			regmap_write(iomap, reg, tmp_val);
			break;

		case 2:
			j = 0;
			do {
				regmap_read(iomap, reg, &tmp_val);
				if (j++ > 100)
					return -ETIMEDOUT;
				usleep_range(50, 100);
			} while ((tmp_val & mask) != val);
			break;
		}
		if (delay)
			msleep(delay);

		regmap_read(iomap, reg, &tmp_val);
		dev_dbg(&pdev->dev,
			"%2d:reg[0x%8x] mask[0x%8x] val[0x%8x] result[0x%8x]\n",
			i, reg, mask, val, tmp_val);
	}

	return i;
}

int sprd_pcie_enter_pcipm_l2(struct dw_pcie *pci)
{
	u32 reg;
	int retries;

	reg = dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_PM_CTRL);
	reg |= SPRD_PCIE_APP_CLK_PM_EN;
	dw_pcie_writel_dbi(pci, SPRD_PCIE_PE0_PM_CTRL, reg);

	reg = dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_TX_MSG_REG);
	reg |= SPRD_PCIE_PME_TURN_OFF_REQ;
	dw_pcie_writel_dbi(pci, SPRD_PCIE_PE0_TX_MSG_REG, reg);

	for (retries = 0; retries < ENTER_L2_MAX_RETRIES; retries++) {
		reg = dw_pcie_readl_dbi(pci, PCIE_PHY_DEBUG_R0);
		if ((reg & LTSSM_STATE_MASK) == LTSSM_STATE_L2_IDLE) {
			dev_info(pci->dev,
				 "enter L2, LTSSM state: 0x%x\n", reg);
			return 0;
		}
		/* TODO: now we can't sure the delay time */
		usleep_range(1000, 2000);
	}
	dev_err(pci->dev,
		"can't enter L2, [0x%x]: 0x%x, [0x%x]: 0x%x, [0x%x]: 0x%x\n",
		SPRD_PCIE_PE0_PM_CTRL,
		dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_PM_CTRL),
		SPRD_PCIE_PE0_PM_STS,
		dw_pcie_readl_dbi(pci, SPRD_PCIE_PE0_PM_STS),
		PCIE_PHY_DEBUG_R0,
		dw_pcie_readl_dbi(pci, PCIE_PHY_DEBUG_R0));

	return -ETIMEDOUT;
}

void sprd_pcie_save_msi_ctrls(struct dw_pcie *pci)
{
	int i;
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		ctrl->save_msi_ctrls[i][0] =
			dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_ENABLE + i * 12);
		ctrl->save_msi_ctrls[i][1] =
			dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_MASK + i * 12);
		ctrl->save_msi_ctrls[i][2] =
			dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_STATUS + i * 12);
	}
}

void sprd_pcie_save_dwc_reg(struct dw_pcie *pci)
{
	sprd_pcie_save_msi_ctrls(pci);
}

void sprd_pcie_restore_msi_ctrls(struct dw_pcie *pci)
{
	int i;
	u64 msi_target;
	struct pcie_port *pp = &pci->pp;
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	msi_target = virt_to_phys((void *)pp->msi_data);
	dw_pcie_writel_dbi(pci, PCIE_MSI_ADDR_LO,
			   (u32)(msi_target & 0xffffffff));
	dw_pcie_writel_dbi(pci, PCIE_MSI_ADDR_HI,
			   (u32)(msi_target >> 32 & 0xfffffff));

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		dw_pcie_writel_dbi(pci, PCIE_MSI_INTR0_ENABLE + i * 12,
				   ctrl->save_msi_ctrls[i][0]);
		dw_pcie_writel_dbi(pci, PCIE_MSI_INTR0_MASK + i * 12,
				   ctrl->save_msi_ctrls[i][1]);
		dw_pcie_writel_dbi(pci, PCIE_MSI_INTR0_STATUS + i * 12,
				   ctrl->save_msi_ctrls[i][2]);
	}
}

void sprd_pcie_restore_dwc_reg(struct dw_pcie *pci)
{
	sprd_pcie_restore_msi_ctrls(pci);
}

/*
 *  Orca pci asic design for normal interrupt mode is not compatible with
 *  kernel PCIE framework, the 0-3 msi irqs are handled directly by hardware.
 *  This workaround is used to disable the corresponding bits of these four irqs
 *  on the msi reqister so that AP GIC will not receive these interrupts.
 */
void sprd_pcie_teardown_msi_irq(unsigned int irq)
{
	unsigned int msi_ctrls, bit, val;
	struct irq_data *data = irq_get_irq_data(irq);
	struct msi_desc *msi = irq_data_get_msi_desc(data);
	struct pcie_port *pp = (struct pcie_port *)msi_desc_to_pci_sysdata(msi);
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);

	msi_ctrls = (data->hwirq / 32) * 12;
	bit = data->hwirq % 32;
	val = dw_pcie_readl_dbi(pci, PCIE_MSI_INTR0_ENABLE + msi_ctrls);
	val &= ~(1 << bit);
	dw_pcie_writel_dbi(pci,  PCIE_MSI_INTR0_ENABLE + msi_ctrls, val);
}
EXPORT_SYMBOL(sprd_pcie_teardown_msi_irq);

#ifdef CONFIG_SPRD_PCIE_AER
void sprd_pcie_alloc_irq_vectors(struct pci_dev *dev, int *irqs, int services)
{
	struct pcie_port *pp = dev->bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	/*
	 * Unisoc PCIe RC only supports the following port service type
	 * (please see pcieport_if.h):
	 * -1. Power Management Event (PME)
	 * -2. Advanced Error Reporting (AER)
	 *  However, only AER irq is used right now.
	 */
	irqs[1] = ctrl->aer_irq;
}
EXPORT_SYMBOL(sprd_pcie_alloc_irq_vectors);
#endif
