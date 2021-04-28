/*
 * PCIe host controller driver for Spreadtrum SoCs
 *
 * Copyright (C) 2018-2019 Spreadtrum corporation.
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
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pcie-rc-sprd.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

#ifdef CONFIG_SPRD_IPA_INTC
static void sprd_pcie_fix_interrupt_line(struct pci_dev *dev)
{
	struct pcie_port *pp = dev->bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	if (dev->hdr_type == PCI_HEADER_TYPE_NORMAL) {
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
				      ctrl->interrupt_line);
		dev_info(&dev->dev,
			 "The pci legacy interrupt pin is set to: %lu\n",
			 (unsigned long)ctrl->interrupt_line);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_ANY_ID, PCI_ANY_ID, sprd_pcie_fix_interrupt_line);
#endif

static irqreturn_t sprd_pcie_msi_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

	return dw_handle_msi_irq(pp);
}

static void sprd_pcie_assert_reset(struct pcie_port *pp)
{
	/* TODO */
}

static int sprd_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);

	sprd_pcie_assert_reset(pp);

	dw_pcie_setup_rc(pp);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		dw_pcie_msi_init(pp);

	if (dw_pcie_wait_for_link(pci))
		dev_warn(pci->dev,
			 "pcie ep may has not been powered yet, ignore it\n");

	return 0;
}

static const struct dw_pcie_host_ops sprd_pcie_host_ops = {
	.host_init = sprd_pcie_host_init,
};

/*
 * WAKE# (low active)from endpoint to wake up AP.
 *
 * When AP is in deep state, an endpoint can wakeup AP by pulling the wake
 * signal to low. After AP is activated, the endpoint must pull the wake signal
 * to high.
 */
static irqreturn_t sprd_pcie_wakeup_irq(int irq, void *data)
{
	struct sprd_pcie *ctrl = data;
	int value = gpiod_get_value(ctrl->gpiod_wakeup);
	u32 irq_flags = irq_get_trigger_type(irq);

	if (!value) {
		irq_flags &= ~IRQF_TRIGGER_LOW;
		irq_flags |= IRQF_TRIGGER_HIGH;
	} else {
		irq_flags &= ~IRQF_TRIGGER_HIGH;
		irq_flags |= IRQF_TRIGGER_LOW;
	}

	irq_set_irq_type(irq, irq_flags);

	return IRQ_HANDLED;
}

static int sprd_add_pcie_port(struct dw_pcie *pci, struct platform_device *pdev)
{
	struct sprd_pcie *ctrl;
	struct pcie_port *pp;
	struct device *dev = &pdev->dev;
	struct fwnode_handle *child;
	int ret;
	unsigned int irq;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base)
		return -ENOMEM;

	pp = &pci->pp;
	pp->ops = &sprd_pcie_host_ops;

	dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
			   LTSSM_EN | L1_AUXCLK_EN);

	ctrl = platform_get_drvdata(to_platform_device(pci->dev));

	device_for_each_child_node(dev, child) {
		if (fwnode_property_read_string(child, "label",
						&ctrl->label)) {
			dev_err(dev, "without interrupt property\n");
			fwnode_handle_put(child);
			return -EINVAL;
		}
		if (!strcmp(ctrl->label, "msi_int")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (!irq) {
				dev_err(dev, "cannot get msi irq\n");
				return -EINVAL;
			}

			pp->msi_irq = (int)irq;
			ret = devm_request_irq(dev, pp->msi_irq,
					       sprd_pcie_msi_irq_handler,
					       IRQF_SHARED | IRQF_NO_THREAD,
					       "sprd-pcie-msi", pp);
			if (ret) {
				dev_err(dev, "cannot request msi irq\n");
				return ret;
			}
		}

#ifdef CONFIG_SPRD_PCIE_AER
		if (!strcmp(ctrl->label, "aer_int")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (!irq) {
				dev_err(dev, "cannot get aer irq\n");
				return -EINVAL;
			}

			ctrl->aer_irq = irq;
			dev_info(dev,
				 "sprd itself defines aer irq is %d\n", irq);
		}
#endif

#ifdef CONFIG_SPRD_IPA_INTC
		if (!strcmp(ctrl->label, "ipa_int")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (!irq) {
				dev_err(dev, "cannot get legacy irq\n");
				return -EINVAL;
			}
			ctrl->interrupt_line = irq;
		}
#endif
	}

	ctrl->gpiod_wakeup =
		devm_gpiod_get_index(dev, "pcie-wakeup", 0, GPIOD_IN);
	if (IS_ERR(ctrl->gpiod_wakeup)) {
		dev_warn(dev, "Please set pcie-wakeup gpio in DTS\n");
		goto no_wakeup;
	}

	ctrl->wakeup_irq = gpiod_to_irq(ctrl->gpiod_wakeup);
	if (ctrl->wakeup_irq < 0) {
		dev_warn(dev, "cannot get wakeup irq\n");
		goto no_wakeup;
	}

	snprintf(ctrl->wakeup_label, ctrl->label_len,
		 "%s wakeup", dev_name(dev));
	ret = devm_request_threaded_irq(dev, ctrl->wakeup_irq,
					sprd_pcie_wakeup_irq, NULL,
					IRQF_TRIGGER_LOW,
					ctrl->wakeup_label, ctrl);
	if (ret < 0)
		dev_warn(dev, "cannot request wakeup irq\n");

no_wakeup:

	return dw_pcie_host_init(&pci->pp);
}

static const struct of_device_id sprd_pcie_of_match[] = {
	{
		.compatible = "sprd,pcie",
	},
	{},
};

static int sprd_pcie_host_uninit(struct platform_device *pdev)
{
	int ret;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;

	sprd_pcie_save_dwc_reg(pci);

	if (ctrl->is_suspended) {
		ret = sprd_pcie_enter_pcipm_l2(pci);
		if (ret < 0)
			dev_warn(&pdev->dev, "NOTE: RC can't enter l2\n");
	}

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-suspend-syscons");
	if (ret < 0)
		dev_err(&pdev->dev,
			"set pcie uninit syscons fail, return %d\n", ret);

	return ret;
}

static struct pci_host_bridge *to_bridge_from_pdev(struct platform_device *pdev)
{
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	struct pcie_port *pp = &pci->pp;

	return pp->bridge;
}

static void sprd_pcie_rescan_bus(struct pci_bus *bus)
{
	struct pci_bus *child;

	pci_scan_child_bus(bus);

	pci_assign_unassigned_bus_resources(bus);

	list_for_each_entry(child, &bus->children, node)
		pcie_bus_configure_settings(child);

	pci_bus_add_devices(bus);
}

static void sprd_pcie_remove_bus(struct pci_bus *bus)
{
	struct pci_dev *pci_dev;

	list_for_each_entry(pci_dev, &bus->devices, bus_list) {
		struct pci_bus *child_bus = pci_dev->subordinate;

		pci_stop_and_remove_bus_device(pci_dev);

		if (child_bus) {
			dev_dbg(&bus->dev,
				"all pcie devices have been removed\n");
			return;
		}
	}
}

static int sprd_pcie_host_shutdown(struct platform_device *pdev)
{
	int ret;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	struct pci_bus *bus;
	struct pci_host_bridge *bridge;

	bridge = to_bridge_from_pdev(pdev);
	bus = bridge->bus;

	/*
	 * Before disabled pcie controller, it's better to remove pcie devices.
	 * pci_sysfs_init is called by late_initcall(fn). When it is called,
	 * pcie controller may be disabled and its EB is 0. In this case,
	 * it will cause kernel panic if a pcie device reads its owner
	 * configuration spaces.
	 */
	sprd_pcie_remove_bus(bus);
	sprd_pcie_save_dwc_reg(pci);
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-shutdown-syscons");
	if (ret < 0)
		dev_err(&pdev->dev,
			"set pcie shutdown syscons fail, return %d\n", ret);

	return ret;
}

static const struct dw_pcie_ops dw_pcie_ops = {
};

static int sprd_pcie_host_reinit(struct platform_device *pdev)
{
	int ret;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	struct pcie_port *pp = &pci->pp;

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-resume-syscons");
	if (ret < 0) {
		dev_err(&pdev->dev,
			"set pcie reinit syscons fail, return %d\n", ret);
		return ret;
	}
	dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
			   LTSSM_EN | L1_AUXCLK_EN);

	dw_pcie_setup_rc(pp);
	ret = dw_pcie_wait_for_link(pci);
	if (ret < 0) {
		dev_err(&pdev->dev, "reinit fail, pcie can't establish link\n");
		return ret;
	}

	sprd_pcie_restore_dwc_reg(pci);
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-aspml1p2-syscons");
	if (ret < 0)
		dev_err(&pdev->dev, "get pcie aspml1.2 syscons fail\n");

	return 0;
}

int sprd_pcie_configure_device(struct platform_device *pdev)
{
	int ret;
	struct pci_host_bridge *bridge;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	ret = sprd_pcie_host_reinit(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "pcie rescan fail\n");
		return ret;
	}

	bridge = to_bridge_from_pdev(pdev);
	sprd_pcie_rescan_bus(bridge->bus);
	ctrl->is_powered = 1;

	return 0;
}
EXPORT_SYMBOL(sprd_pcie_configure_device);

int sprd_pcie_unconfigure_device(struct platform_device *pdev)
{
	int ret;
	struct pci_bus *bus;
	struct pci_host_bridge *bridge;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	bridge = to_bridge_from_pdev(pdev);
	bus = bridge->bus;

	sprd_pcie_remove_bus(bus);

	ret = sprd_pcie_host_uninit(pdev);
	if (ret < 0)
		dev_warn(&pdev->dev,
			 "please ignore pcie unconfigure failure\n");
	ctrl->is_powered = 0;

	return 0;
}
EXPORT_SYMBOL(sprd_pcie_unconfigure_device);

static int sprd_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct sprd_pcie *ctrl;
	int ret;
	size_t len = strlen(dev_name(dev)) + 10;

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-startup-syscons");
	if (ret < 0) {
		dev_err(dev, "get pcie syscons fail, return %d\n", ret);
		return ret;
	}

	if (device_property_read_bool(dev, "no-pcie")) {
		dev_info(dev, "no pcie device\n");
		return 0;
	}

	ctrl = devm_kzalloc(dev, sizeof(*ctrl) + len, GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	ctrl->pci = pci;
	ctrl->label_len = len;

	platform_set_drvdata(pdev, ctrl);

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-aspml1p2-syscons");
	if (ret < 0)
		dev_err(&pdev->dev, "get pcie aspml1.2 syscons fail\n");

	ret = sprd_add_pcie_port(pci, pdev);
	if (ret) {
		dev_err(dev, "cannot initialize rc host\n");
		return ret;
	}

	ctrl->is_powered = 1;

	if (!dw_pcie_link_up(pci)) {
		dev_info(dev,
			 "the EP has not been ready yet, power off the RC\n");
		sprd_pcie_host_shutdown(pdev);
		ctrl->is_powered = 0;
	}

	return 0;
}

static int sprd_pcie_suspend_noirq(struct device *dev)
{
	int ret;
	struct platform_device *pdev;
	struct sprd_pcie *ctrl;

	if (device_property_read_bool(dev, "no-pcie")) {
		dev_info(dev, "no pcie device, do nothing in pcie suspend\n");
		return 0;
	}

	pdev = to_platform_device(dev);
	ctrl = platform_get_drvdata(pdev);

	if (!ctrl->is_powered)
		return 0;

	ctrl->is_suspended = 1;
	ret = sprd_pcie_host_uninit(pdev);
	if (ret < 0)
		dev_err(dev, "suspend noirq warning\n");

	return 0;
}

static int sprd_pcie_resume_noirq(struct device *dev)
{
	int ret;
	struct platform_device *pdev;
	struct sprd_pcie *ctrl;

	if (device_property_read_bool(dev, "no-pcie")) {
		dev_info(dev, "no pcie device, do nothing in pcie resume\n");
		return 0;
	}

	pdev = to_platform_device(dev);
	ctrl = platform_get_drvdata(pdev);

	if (!ctrl->is_powered)
		return 0;
	ctrl->is_suspended = 0;

	ret = sprd_pcie_host_reinit(pdev);
	if (ret < 0)
		dev_err(dev, "resume noirq warning\n");

	return 0;
}

static const struct dev_pm_ops sprd_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(sprd_pcie_suspend_noirq,
				      sprd_pcie_resume_noirq)
};

static struct platform_driver sprd_pcie_driver = {
	.probe = sprd_pcie_probe,
	.driver = {
		.name = "sprd-pcie",
		.suppress_bind_attrs = true,
		.of_match_table = sprd_pcie_of_match,
		.pm	= &sprd_pcie_pm_ops,
	},
};

builtin_platform_driver(sprd_pcie_driver);
