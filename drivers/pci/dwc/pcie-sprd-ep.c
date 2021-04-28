/*
 * PCIe Endpoint controller driver for Spreadtrum SoCs
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
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

static void sprd_pcie_ep_init(struct dw_pcie_ep *ep)
{
	dw_pcie_setup_ep(ep);
}

static int sprd_pcie_ep_raise_irq(struct dw_pcie_ep *ep,
				  enum pci_epc_irq_type type,
				  u8 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		/* TODO*/
		break;
	case  PCI_EPC_IRQ_MSI:
		return dw_pcie_ep_raise_msi_irq(ep, interrupt_num);
	default:
		dev_err(pci->dev, "UNKNOWN IRQ type\n");
	}

	return 0;
}

static struct dw_pcie_ep_ops pcie_ep_ops = {
	.ep_init = sprd_pcie_ep_init,
	.raise_irq = sprd_pcie_ep_raise_irq,
};

static int sprd_add_pcie_ep(struct sprd_pcie *sprd_pcie,
			    struct platform_device *pdev)
{
	int ret;
	struct dw_pcie_ep *ep;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci = sprd_pcie->pci;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi2");
	pci->dbi_base2 = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base2)
		return -ENOMEM;

	ep = &pci->ep;
	ep->ops = &pcie_ep_ops;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "addr_space");
	if (!res) {
		dev_err(dev, "pci can't get addr space\n");
		return -EINVAL;
	}

	ep->phys_base = res->start;
	ep->addr_size = resource_size(res);

	ret = dw_pcie_ep_init(ep);
	if (ret) {
		dev_err(dev, "failed to initialize endpoint\n");
		return ret;
	}

	return 0;
}

static int sprd_pcie_establish_link(struct dw_pcie *pci)
{
	/* TODO */
	return 0;
}

static void sprd_pcie_stop_link(struct dw_pcie *pci)
{
	/* TODO */
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.start_link = sprd_pcie_establish_link,
	.stop_link = sprd_pcie_stop_link,
};

static int sprd_pcie_ep_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct sprd_pcie *ep;
	int ret;

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-startup-syscons");
	if (ret < 0) {
		dev_err(dev, "get pcie syscons fail, return %d\n", ret);
		return ret;
	}

	ep = devm_kzalloc(dev, sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	ep->pci = pci;

	platform_set_drvdata(pdev, ep);

	ret = sprd_add_pcie_ep(ep, pdev);
	if (ret) {
		dev_err(dev, "cannot initialize ep host\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id sprd_pcie_ep_of_match[] = {
	{
		.compatible = "sprd,pcie-ep",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pcie_ep_of_match);

static struct platform_driver sprd_pcie_ep_driver = {
	.probe = sprd_pcie_ep_probe,
	.driver = {
		.name = "sprd-pcie-ep",
		.suppress_bind_attrs = true,
		.of_match_table = sprd_pcie_ep_of_match,
	},
};

module_platform_driver(sprd_pcie_ep_driver);

MODULE_DESCRIPTION("Spreadtrum pcie ep controller driver");
MODULE_LICENSE("GPL");
