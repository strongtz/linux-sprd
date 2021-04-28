/**
 * UNISOC PCIe endpoint function driver.
 *
 * Copyright (C) 2019 UNISOC Communications Inc.
 * Author: Wenping Zhou <wenping.zhou@unisoc.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SPRD_PCI_EPF_H
#define _SPRD_PCI_EPF_H

/* only support function 0 now. */
enum {
	SPRD_FUNCTION_0 = 0,
	/* SPRD_FUNCTION_1, */
	SPRD_FUNCTION_MAX
};

enum {
	SPRD_EPF_SIPC_IRQ = 0,
	SPRD_EPF_IPA_IRQ,
	SPRD_EPF_IRQ_MAX
};

enum {
	SPRD_EPF_PROBE = 0,
	SPRD_EPF_BIND,
	SPRD_EPF_LINK_UP,
	SPRD_EPF_UNBIND,
	SPRD_EPF_REMOVE
};

int sprd_pci_epf_register_notify(int function,
				 void (*notify)(int event, void *data),
				 void *data);
int sprd_pci_epf_unregister_notify(int function);
int sprd_pci_epf_set_irq_number(int function, int irq_number);
int sprd_pci_epf_raise_irq(int function, int irq);
int sprd_pci_epf_register_irq_handler(int function,
				      int irq,
				      irq_handler_t handler,
				      void *data);
int sprd_pci_epf_unregister_irq_handler(int function, int irq);
void __iomem *sprd_pci_epf_map_memory(int function,
				      phys_addr_t rc_addr, size_t size);
void sprd_pci_epf_unmap_memory(int function,
			       const void __iomem *cpu_vir_addr);

#ifdef CONFIG_SPRD_IPA_PCIE_WORKROUND
void __iomem *sprd_epf_ipa_map(phys_addr_t src, phys_addr_t dst, size_t size);
void sprd_epf_ipa_unmap(void __iomem *cpu_vir_addr);
#endif

#endif

