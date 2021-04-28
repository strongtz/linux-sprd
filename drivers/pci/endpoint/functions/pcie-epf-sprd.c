/**
 * UNISOC PCIe endpoint function driver.
 *
 * Copyright (C) 2019 UNISOC Communications Inc.
 * Author: Ziyi Zhang <ziyi.zhang@unisoc.com>,
 * Wenping Zhou <<wenping.zhou@unisoc.com>>
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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/pci_ids.h>
#include <linux/random.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/pci-epc.h>
#include <linux/pci-epf.h>
#include <linux/pci_regs.h>
#include <linux/pcie-epf-sprd.h>

#include "../../dwc/pcie-designware.h"

#define SPRD_EPF_NAME "pci_epf_sprd"

/* doorbell reg define */
#define DOOR_BELL_BASE		0x10000
#define DOOR_BELL_ENABLE	0x10
#define DOOR_BELL_STATUS	0x14
#define REQUEST_BASE_IRQ	16

struct sprd_ep_res {
	struct list_head	list;
	phys_addr_t		rc_addr;
	phys_addr_t		cpu_phy_addr;
	void __iomem		*cpu_vir_addr;
	size_t			size;
};

struct sprd_pci_epf {
	struct pci_epf		*epf;
	struct list_head	res_list;
	spinlock_t		lock;
	int			irq_number;
};

struct sprd_pci_epf_notify {
	void	(*notify)(int event, void *data);
	void	*data;
};

struct sprd_pci_epf_irq_handler {
	irq_handler_t	handler;
	void		*data;
};

static struct sprd_pci_epf *g_epf_sprd[SPRD_FUNCTION_MAX];
static struct sprd_pci_epf_notify g_epf_notify[SPRD_FUNCTION_MAX];
static struct sprd_pci_epf_irq_handler
		g_epf_handler[SPRD_FUNCTION_MAX][SPRD_EPF_IRQ_MAX];
static int g_irq_number[SPRD_FUNCTION_MAX];
static struct pci_epf_header sprd_header[SPRD_FUNCTION_MAX];

static irqreturn_t sprd_epf_irq_handler(int irq_number, void *private);

int sprd_pci_epf_register_notify(int function,
				 void (*notify)(int event, void *data),
				 void *data)

{
	struct sprd_pci_epf_notify *epf_notify;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_notify = &g_epf_notify[function];
	epf_notify->notify = notify;
	epf_notify->data = data;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_register_notify);

int sprd_pci_epf_unregister_notify(int function)
{
	struct sprd_pci_epf_notify *epf_notify;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_notify = &g_epf_notify[function];
	epf_notify->notify = NULL;
	epf_notify->data = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_unregister_notify);

int sprd_pci_epf_register_irq_handler(int function,
				      int irq,
				      irq_handler_t handler,
				      void *data)

{
	struct sprd_pci_epf_irq_handler *epf_handler;

	if (function >= SPRD_FUNCTION_MAX || irq >= SPRD_EPF_IRQ_MAX)
		return -EINVAL;

	epf_handler = &g_epf_handler[function][irq];
	epf_handler->handler = handler;
	epf_handler->data = data;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_register_irq_handler);

int sprd_pci_epf_unregister_irq_handler(int function, int irq)
{
	struct sprd_pci_epf_irq_handler *epf_handler;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_handler = &g_epf_handler[function][irq];
	epf_handler->handler = NULL;
	epf_handler->data = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_unregister_irq_handler);

int sprd_pci_epf_set_irq_number(int function, int irq_number)
{
	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	g_irq_number[function] = irq_number;

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_set_irq_number);

int sprd_pci_epf_raise_irq(int function, int irq)
{
	u8 msi_count;
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;

	if (function >= SPRD_FUNCTION_MAX)
		return -EINVAL;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return -EINVAL;

	dev = &epf_sprd->epf->dev;
	epc = epf_sprd->epf->epc;
	msi_count = pci_epc_get_msi(epc);

	if (irq > msi_count || msi_count <= 0) {
		dev_err(dev,
			"raise irq func_no=%d, irq=%d, msi_count=%d\n",
			function, irq, msi_count);
		return -EINVAL;
	}

	irq += REQUEST_BASE_IRQ;
	/* *
	 * in dw_pcie_ep_raise_msi_irq, write to the data reg
	 * is (irq -1) , so here, we must pass (irq + 1)
	 */
	return pci_epc_raise_irq(epc, PCI_EPC_IRQ_MSI, irq + 1);
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_raise_irq);

void __iomem *sprd_pci_epf_map_memory(int function,
				      phys_addr_t rc_addr,
				      size_t size)
{
	int ret;
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;
	unsigned long flags;
	struct sprd_ep_res *res;
	phys_addr_t cpu_phys_addr;
	void __iomem *cpu_vir_addr;

	if (function >= SPRD_FUNCTION_MAX)
		return NULL;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return NULL;

	dev = &epf_sprd->epf->dev;
	res = devm_kzalloc(dev, sizeof(*res), GFP_KERNEL);
	if (!res)
		return NULL;

	epc = epf_sprd->epf->epc;
	size = PAGE_ALIGN(size);

	cpu_vir_addr = pci_epc_mem_alloc_addr(epc, &cpu_phys_addr, size);
	if (!cpu_vir_addr) {
		dev_err(dev,
			"failed to alloc epc memory,  size = 0x%lx!\n",
			size);
		devm_kfree(dev, res);
		return NULL;
	}

	dev_dbg(dev, "map: func_no=%d, ep_addr=0x%llx, rc_addr=0x%llx\n",
		function, cpu_phys_addr, rc_addr);

	ret = pci_epc_map_addr(epc, cpu_phys_addr, rc_addr, size);
	if (ret) {
		dev_err(dev, "failed to map address!\n");
		pci_epc_mem_free_addr(epc, cpu_phys_addr, cpu_vir_addr, size);
		devm_kfree(dev, res);
		return NULL;
	}

	res->rc_addr = rc_addr;
	res->cpu_phy_addr = cpu_phys_addr;
	res->cpu_vir_addr = cpu_vir_addr;
	res->size = size;

	spin_lock_irqsave(&epf_sprd->lock, flags);
	list_add_tail(&res->list, &epf_sprd->res_list);
	spin_unlock_irqrestore(&epf_sprd->lock, flags);

	return cpu_vir_addr;
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_map_memory);

#ifdef CONFIG_SPRD_IPA_PCIE_WORKROUND
void __iomem *sprd_epf_ipa_map(phys_addr_t src_addr,
			       phys_addr_t dst_addr, size_t size)
{
	int ret;
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;
	void __iomem *cpu_vir_addr;

	epf_sprd = g_epf_sprd[SPRD_FUNCTION_0];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return NULL;

	dev = &epf_sprd->epf->dev;
	epc = epf_sprd->epf->epc;
	size = PAGE_ALIGN(size);
	ret = pci_epc_map_addr(epc, src_addr, dst_addr, size);
	if (ret) {
		dev_err(dev, "failed to map ipa address!\n");
		return NULL;
	}

	cpu_vir_addr = ioremap_nocache(src_addr, size);

	return cpu_vir_addr;
}
EXPORT_SYMBOL_GPL(sprd_epf_ipa_map);

void sprd_epf_ipa_unmap(void __iomem *cpu_vir_addr)
{
	iounmap(cpu_vir_addr);
}
EXPORT_SYMBOL_GPL(sprd_epf_ipa_unmap);
#endif

void sprd_pci_epf_unmap_memory(int function, const void __iomem *cpu_vir_addr)
{
	struct pci_epc *epc;
	struct sprd_pci_epf *epf_sprd;
	struct device *dev;
	struct sprd_ep_res *res, *next;
	unsigned long flags;

	if (function >= SPRD_FUNCTION_MAX)
		return;

	epf_sprd = g_epf_sprd[function];
	if (!epf_sprd || IS_ERR(epf_sprd->epf->epc))
		return;

	dev = &(epf_sprd->epf->dev);
	epc = epf_sprd->epf->epc;

	dev_dbg(dev, "map: unpap func_no=%d, cpu_vir_addr=0x%p\n",
		function, cpu_vir_addr);

	/* find the res from list */
	spin_lock_irqsave(&epf_sprd->lock, flags);
	list_for_each_entry_safe(res, next, &epf_sprd->res_list, list) {
		if (res->cpu_vir_addr == cpu_vir_addr) {
			pci_epc_unmap_addr(epc, res->rc_addr);
			pci_epc_mem_free_addr(epc,
					      res->cpu_phy_addr,
					      res->cpu_vir_addr,
					      res->size);
			list_del(&res->list);
			devm_kfree(dev, res);
			break;
		}
	}
	spin_unlock_irqrestore(&epf_sprd->lock, flags);
}
EXPORT_SYMBOL_GPL(sprd_pci_epf_unmap_memory);

static irqreturn_t sprd_epf_irq_handler(int irq_number, void *private)
{
	struct sprd_pci_epf_irq_handler *epf_handler;
	struct sprd_pci_epf *epf_sprd = (struct sprd_pci_epf *)private;
	struct device *dev = &(epf_sprd->epf->dev);
	struct dw_pcie_ep *ep = epc_get_drvdata(epf_sprd->epf->epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	int irq, function;
	u32 enable, status;

	function = epf_sprd->epf->func_no;
	if (function >= SPRD_FUNCTION_MAX)
		return IRQ_NONE;

	/*read irq status and clear irq */
	enable = dw_pcie_readl_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_ENABLE);
	status = dw_pcie_readl_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_STATUS);
	dw_pcie_writel_dbi(pci, DOOR_BELL_BASE + DOOR_BELL_STATUS, 0x0);

	dev_dbg(dev, "func_no=%d, enable=%#x, status=%#x\n",
		function, enable, status);
	status &= enable;

	for (irq = 0; irq < SPRD_EPF_IRQ_MAX; irq++) {
		if (status & BIT(irq)) {
			dev_dbg(dev, "func_no=%d, irq=%d\n", function, irq);
			epf_handler = &g_epf_handler[function][irq];
			if (epf_handler->handler)
				epf_handler->handler(irq, epf_handler->data);
		}
	}
	return IRQ_HANDLED;
}

static int sprd_epf_bind(struct pci_epf *epf)
{
#ifdef CONFIG_SPRD_EPF_ENABLE_EPCONF
	int ret;
#endif
	void  (*notify)(int event, void *data);
	struct pci_epc *epc = epf->epc;
	struct device *dev = &epf->dev;
	struct dw_pcie_ep *ep;
	struct dw_pcie *pci;

	if (WARN_ON_ONCE(!epc))
		return -EINVAL;

	dev_info(dev, "bind: func_no = %d, epf->func_no = =%d\n",
		epf->func_no, epf->msi_interrupts);

	/*
	 * if the feature CONFIG_SPRD_EPF_ENABLE_EPCONF
	 * (default closed) is not opend, we can't set the header
	 * and the msi, just use the default configuration.
	 */
#ifdef CONFIG_SPRD_EPF_ENABLE_EPCONF
	ret = pci_epc_write_header(epc, epf->header);
	if (ret) {
		dev_err(dev, "bind: header write failed, ret=%d\n", ret);
		return ret;
	}

	ret = pci_epc_set_msi(epc, epf->msi_interrupts);
	if (ret) {
		dev_err(dev, "bind: set msi failed, ret=%d\n", ret);
		return ret;
	}
#endif

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_BIND, g_epf_notify[epf->func_no].data);

		/* enable door bell irq */
		ep = epc_get_drvdata(epf->epc);
		pci = to_dw_pcie_from_ep(ep);
		dw_pcie_writel_dbi(pci,
				   DOOR_BELL_BASE + DOOR_BELL_ENABLE,
				   BIT(SPRD_FUNCTION_MAX) - 1);
	}

	return 0;
}

static void sprd_epf_unbind(struct pci_epf *epf)
{
	void (*notify)(int event, void *data);
	struct device *dev = &epf->dev;
	struct pci_epc *epc = epf->epc;

	dev_info(dev, "unbind: func_no = %d\n", epf->func_no);

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_UNBIND,
			       g_epf_notify[epf->func_no].data);
	}

	pci_epc_stop(epc);
}

static void sprd_epf_linkup(struct pci_epf *epf)
{
	struct device *dev = &epf->dev;
	void (*notify)(int event, void *data);

	dev_info(dev, "linkup: func_no = %d\n", epf->func_no);

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_LINK_UP,
			       g_epf_notify[epf->func_no].data);
	}
}

static const struct pci_epf_device_id sprd_epf_ids[] = {
	{
		.name = SPRD_EPF_NAME,
	},
	{},
};

static int sprd_epf_probe(struct pci_epf *epf)
{
	int ret;
	void (*notify)(int event, void *data);
	struct sprd_pci_epf *epf_sprd;
	struct device *dev = &epf->dev;

	epf_sprd = devm_kzalloc(dev, sizeof(*epf_sprd), GFP_KERNEL);
	if (!epf_sprd)
		return -ENOMEM;

	if (epf->func_no >= SPRD_FUNCTION_MAX) {
		dev_err(dev, "probe: func_no = %d\n", epf->func_no);
		return -EINVAL;
	}

	epf->header = &sprd_header[epf->func_no];
	epf->header->vendorid = PCI_ANY_ID,
	epf->header->deviceid = PCI_ANY_ID,
	epf->header->baseclass_code = PCI_CLASS_OTHERS,
	epf->header->interrupt_pin  = PCI_INTERRUPT_UNKNOWN,

	epf_sprd->epf = epf;
	spin_lock_init(&epf_sprd->lock);
	INIT_LIST_HEAD(&epf_sprd->res_list);

	g_epf_sprd[epf->func_no] = epf_sprd;
	notify = g_epf_notify[epf->func_no].notify;
	if (notify)
		notify(SPRD_EPF_PROBE, g_epf_notify[epf->func_no].data);

	if (g_irq_number[epf->func_no] > 0) {
		epf_sprd->irq_number = g_irq_number[epf->func_no];
		ret = request_irq(epf_sprd->irq_number,
			   sprd_epf_irq_handler,
			   IRQF_NO_SUSPEND,
			   SPRD_EPF_NAME,
			   epf_sprd);
		if (ret)
			dev_warn(dev,
				 "request irq number=%d, ret=%d\n",
				 epf_sprd->irq_number,
				 ret);
	}

	epf_set_drvdata(epf, epf_sprd);
	return 0;
}

static int sprd_epf_remove(struct pci_epf *epf)
{
	unsigned long flags;
	struct sprd_ep_res *res, *next;
	void  (*notify)(int event, void *data);
	struct device *dev = &epf->dev;
	struct pci_epc *epc = epf->epc;
	struct sprd_pci_epf *epf_sprd = epf_get_drvdata(epf);

	spin_lock_irqsave(&epf_sprd->lock, flags);
	list_for_each_entry_safe(res, next, &epf_sprd->res_list, list) {
		pci_epc_unmap_addr(epc, res->rc_addr);
		pci_epc_mem_free_addr(epc, res->cpu_phy_addr,
				      res->cpu_vir_addr,
				      res->size);
		list_del(&res->list);
		devm_kfree(dev, res);
	}
	spin_unlock_irqrestore(&epf_sprd->lock, flags);

	if (epf->func_no < SPRD_FUNCTION_MAX) {
		notify = g_epf_notify[epf->func_no].notify;
		if (notify)
			notify(SPRD_EPF_REMOVE,
				 g_epf_notify[epf->func_no].data);

		g_epf_sprd[epf->func_no] = NULL;
	}

	devm_kfree(dev, epf_sprd);
	epf_set_drvdata(epf, NULL);

	return 0;
}

static struct pci_epf_ops ops = {
	.unbind	= sprd_epf_unbind,
	.bind	= sprd_epf_bind,
	.linkup = sprd_epf_linkup,
};

static struct pci_epf_driver sprd_epf_driver = {
	.driver.name	= SPRD_EPF_NAME,
	.probe		= sprd_epf_probe,
	.remove		= sprd_epf_remove,
	.id_table	= sprd_epf_ids,
	.ops		= &ops,
	.owner		= THIS_MODULE,
};

static int __init pci_epf_sprd_init(void)
{
	int ret;

	ret = pci_epf_register_driver(&sprd_epf_driver);
	if (ret) {
		pr_err("%s: failed to register SPRD PCIe epf driver, ret=%d\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}
module_init(pci_epf_sprd_init);

static void __exit pci_epf_sprd_exit(void)
{
	pci_epf_unregister_driver(&sprd_epf_driver);
}
module_exit(pci_epf_sprd_exit);

MODULE_DESCRIPTION("UNISOC SPRD PCIe endpoint function driver");
MODULE_AUTHOR("Ziyi Zhang <ziyi.zhang@unisoc.com>");
MODULE_LICENSE("GPL v2");
