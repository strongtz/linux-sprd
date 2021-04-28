/**
 * SPRD ep device driver in host side for Spreadtrum SoCs
 *
 * Copyright (C) 2019 Spreadtrum Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is used to control ep device driver in host side for
 * Spreadtrum SoCs.
 */

#ifndef __SPRD_PCIE_EP_DEVICE_H
#define __SPRD_PCIE_EP_DEVICE_H

#include <linux/interrupt.h>

enum {
	PCIE_EP_SIPC_IRQ = 0,
	PCIE_EP_OTHER_IRQ,
	PCIE_EP_MAX_IRQ
};

enum {
	PCIE_EP_MODEM = 0,
	/* PCIE_EP_WCN, */
	PCIE_EP_NR
};

enum {
	PCIE_EP_PROBE = 0,
	PCIE_EP_REMOVE
};

#ifdef CONFIG_SPRD_SIPA
enum {
	PCIE_IPA_TYPE_MEM = 0,
	PCIE_IPA_TYPE_REG
};
#endif

#define MINI_REGION_SIZE 0x10000 /*64 K default */

int sprd_ep_dev_register_notify(int ep,
				void (*notify)(int event, void *data),
				void *data);
int sprd_ep_dev_unregister_notify(int ep);
int sprd_ep_dev_register_irq_handler(int ep,
				     int irq,
				     irq_handler_t handler,
				     void *data);
int sprd_ep_dev_raise_irq(int ep, int irq);
int sprd_ep_dev_unregister_irq_handler(int ep, int irq);
void __iomem *sprd_ep_map_memory(int ep,
				 phys_addr_t cpu_addr,
				 size_t size);
void sprd_ep_unmap_memory(int ep, const void __iomem *bar_addr);

#ifdef CONFIG_SPRD_SIPA
phys_addr_t sprd_ep_ipa_map(int type, phys_addr_t target_addr, size_t size);
int sprd_ep_ipa_unmap(int type, phys_addr_t cpu_addr);
#endif
#endif
