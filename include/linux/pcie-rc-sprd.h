#ifndef _PCIE_RC_SPRD_H
#define _PCIE_RC_SPRD_H

#include  <linux/platform_device.h>
/*
 * SPRD PCIe root complex (e.g. UD710 SoC) can't support PCI hotplug
 * capability. Therefore, the standard hotplug driver can't be used.
 *
 * Whenever one endpoint is plugged or powered on, the EP driver must
 * call sprd_pcie_configure_device() in order to add EP device to system
 * and probe EP driver. If one endpoint is unplugged or powered off,
 * the EP driver must call sprd_pcie_unconfigure_device() in order to
 * remove all PCI devices on PCI bus.
 *
 * return 0 on success, otherwise return a negative number.
 */

extern int sprd_pcie_configure_device(struct platform_device *pdev);
extern int sprd_pcie_unconfigure_device(struct platform_device *pdev);
extern void sprd_pcie_teardown_msi_irq(unsigned int irq);

#ifdef CONFIG_SPRD_PCIE_AER
void sprd_pcie_alloc_irq_vectors(struct pci_dev *dev, int *irqs, int services);
#else
static inline void sprd_pcie_alloc_irq_vectors(struct pci_dev *dev, int *irqs,
					       int services)
{
}
#endif

#endif
