
/*
 * Spreadtrum PAM USB3 driver
 *
 * Copyright (c) 2018 Spreadtrum Co., Ltd.
 *      http://www.spreadtrum.com
 *
 * Author: Cheng Zeng <cheng.zeng@spreadtrum.com>
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
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include <linux/usb/phy.h>
#include <linux/usb/pam.h>
#include <linux/sipa.h>
#include <dt-bindings/soc/sprd,roc1-mask.h>
#include <dt-bindings/soc/sprd,roc1-regs.h>
#include "sprd-pamu3.h"

/* PAMU3 default xmit code values used for command entry */
static u32 xmit_code[20] = {
	0x0,
	0x8100,
	0x0,
	0x8200,
	0x0,
	0x9F00,
	0x0,
	0x8F00,
	0x0,
	0x8300,
	0xCAFE,
	0xCAFE,
	0x0,
	0xA000,
	0xCAFE,
	0xCAFE,
	0x0,
	0xA000,
	0xCAFE,
	0xCAFE
};

struct sprd_pamu3 {
	struct usb_phy      pam;
	void __iomem        *base;
	void __iomem        *dwc3_dma;
	void __iomem        *dwc3_base;
	struct clk		*clk;

	struct pamu3_dwc3_trb		*tx_trb_pool;
	struct pamu3_dwc3_trb		*rx_trb_pool;
	dma_addr_t					tx_trb_pool_dma;
	dma_addr_t					rx_trb_pool_dma;

	char						*rndis_header_buf;
	dma_addr_t					rndis_header_buf_dma;
	char						*rx_max_buf;
	dma_addr_t					rx_max_buf_dma;

	struct device		*dev;

	struct sipa_connect_params sipa_params;
	struct sipa_to_pam_info sipa_info;
	u8		max_dl_pkts;
	u8		max_ul_pkts;
	u8		netid;
	atomic_t    inited;     /* Pam init flag */
	atomic_t	ref;
};

static struct sprd_pamu3 *pamu3_tag;

static void pamu3_set_netid(struct sprd_pamu3 *pamu3, u8 netid)
{
	u32 value;

	sipa_rm_enable_usb_tether();
	value = readl_relaxed(pamu3->base + PAM_U3_UL_NODE_HEADER);
	value &= ~PAMU3_MASK_NETID;
	value |= netid;
	writel_relaxed(value, pamu3->base + PAM_U3_UL_NODE_HEADER);
}

static ssize_t pamu3_netid_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_pamu3 *pamu3;
	u8	netid;

	if (!x)
		return -EINVAL;

	pamu3 = container_of(x, struct sprd_pamu3, pam);
	if (kstrtou8(buf, 10, &netid) < 0)
		return -EINVAL;

	pamu3->netid = netid;
	if (atomic_read(&pamu3->ref))
		pamu3_set_netid(pamu3, netid);
	return size;
}

static ssize_t pamu3_netid_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_pamu3 *pamu3;

	if (!x)
		return -EINVAL;

	pamu3 = container_of(x, struct sprd_pamu3, pam);

	return sprintf(buf, "%d\n", pamu3->netid);
}

static DEVICE_ATTR_RW(pamu3_netid);

void pamu3_load_code(struct sprd_pamu3 *pamu3, void *txep, void *rxep)
{
	u32 *addr;
	u32 value;
	int i;

	/* Set TX xmit codes depending on EP and write codes to iram */
	xmit_code[6] = (u32)((unsigned long)txep & PAMU3_MASK_LOWADDR32);
	xmit_code[2] = xmit_code[6] + 4;
	xmit_code[0] = xmit_code[2] + 4;
	xmit_code[8] = xmit_code[0] + 4;
	xmit_code[12] = xmit_code[8];
	xmit_code[16] = (xmit_code[8] & PAMU3_MASK_TRIGHDRCNT) |
			REG_DWC3_GEVNTCOUNT(1);
	addr = pamu3->base + PAM_U3_TXDBG_IRAM;
	for (i = 0; i < 20; i++) {
		writel_relaxed(xmit_code[i], addr);
		addr++;
	}

	/* Set TX command entry */
	value = PAMU3_CMDENTER_ADDR0;
	writel_relaxed(value, pamu3->base + PAM_U3_TXPCMDENTRY_ADDR0);
	value = PAMU3_CMDENTER_ADDR1;
	writel_relaxed(value, pamu3->base + PAM_U3_TXPCMDENTRY_ADDR1);

	/* Set RX xmit codes depending on EP and write codes to iram */
	xmit_code[6] = (u32)((unsigned long)rxep & PAMU3_MASK_LOWADDR32);
	xmit_code[2] = xmit_code[6] + 4;
	xmit_code[0] = xmit_code[2] + 4;
	xmit_code[8] = xmit_code[0] + 4;
	xmit_code[12] = xmit_code[8];
	xmit_code[16] = (xmit_code[8] & PAMU3_MASK_TRIGHDRCNT) |
			REG_DWC3_GEVNTCOUNT(2);
	addr = pamu3->base + PAM_U3_RXDBG_IRAM;
	for (i = 0; i < 20; i++) {
		writel_relaxed(xmit_code[i], addr);
		addr++;
	}

	/* Set RX command entry */
	value = PAMU3_CMDENTER_ADDR0;
	writel_relaxed(value, pamu3->base + PAM_U3_RXPCMDENTRY_ADDR0);
	value = PAMU3_CMDENTER_ADDR1;
	writel_relaxed(value, pamu3->base + PAM_U3_RXPCMDENTRY_ADDR1);
}

void pamu3_memory_init(struct sprd_pamu3 *pamu3)
{
	static struct pamu3_dwc3_trb *trb;
	static dma_addr_t trb_dma;
	u32 *reg;
	u32 *trb_reg;
	u32 value;
	int i;
	char *bufaddr;

	/* IPA common FIFOs IRAM addresses */
	pamu3->sipa_params.send_param.tx_intr_threshold = pamu3->max_dl_pkts;
	pamu3->sipa_params.send_param.tx_intr_delay_us = 5;
	pamu3->sipa_params.recv_param.tx_intr_threshold = pamu3->max_ul_pkts;
	pamu3->sipa_params.recv_param.tx_intr_delay_us = 5;
	sipa_get_ep_info(SIPA_EP_USB, &pamu3->sipa_info);

	value = pamu3->sipa_info.dl_fifo.tx_fifo_base_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_DLFIFOFILL_ADDRL);
	value = (pamu3->sipa_info.dl_fifo.tx_fifo_base_addr >>
			PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(value, pamu3->base + PAM_U3_DLFIFOFILL_ADDRH);

	value = pamu3->sipa_info.dl_fifo.rx_fifo_base_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_DLFIFOFREE_ADDRL);
	value = (pamu3->sipa_info.dl_fifo.rx_fifo_base_addr >>
			PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(value, pamu3->base + PAM_U3_DLFIFOFREE_ADDRH);

	value = pamu3->sipa_info.ul_fifo.rx_fifo_base_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_ULFIFOFILL_ADDRL);
	value = (pamu3->sipa_info.ul_fifo.rx_fifo_base_addr >>
			PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(value, pamu3->base + PAM_U3_ULFIFOFILL_ADDRH);

	value = pamu3->sipa_info.ul_fifo.tx_fifo_base_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_ULFIFOFREE_ADDRL);
	value = (pamu3->sipa_info.ul_fifo.tx_fifo_base_addr >>
			PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(value, pamu3->base + PAM_U3_ULFIFOFREE_ADDRH);

	/* IPA common FIFOs registers */
	value = pamu3->sipa_info.dl_fifo.fifo_sts_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_DLGETIPAFIFO_ADDRL);
	writel_relaxed(0, pamu3->base + PAM_U3_DLGETIPAFIFO_ADDRH);

	value = pamu3->sipa_info.dl_fifo.fifo_sts_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_DLPUTIPAFIFO_ADDRL);
	writel_relaxed(0, pamu3->base + PAM_U3_DLPUTIPAFIFO_ADDRH);

	value = pamu3->sipa_info.ul_fifo.fifo_sts_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_ULGETIPAFIFO_ADDRL);
	writel_relaxed(0, pamu3->base + PAM_U3_ULGETIPAFIFO_ADDRH);

	value = pamu3->sipa_info.ul_fifo.fifo_sts_addr &
			PAMU3_MASK_LOWADDR32;
	writel_relaxed(value, pamu3->base + PAM_U3_ULPUTIPAFIFO_ADDRL);
	writel_relaxed(0, pamu3->base + PAM_U3_ULPUTIPAFIFO_ADDRH);

	/* setup PAM_U3 rx buffef and rx TRBs */
	bufaddr = (char *)pamu3->rx_max_buf_dma;
	reg = pamu3->base + PAM_U3_RXBUF0_ADDRL;
	trb = (struct pamu3_dwc3_trb *)pamu3->rx_trb_pool;
	trb_dma = pamu3->rx_trb_pool_dma;
	trb_reg = pamu3->base + PAM_U3_RXTRBBUF0_ADDRL;
	for (i = 0; i < PAMU3_RX_TRBBUF_NUM; i++) {
		trb->bpl = (u64)bufaddr & PAMU3_MASK_LOWADDR32;
		trb->bph = ((u64)bufaddr >> PAMU3_BITS_LOWADDR32) &
				PAMU3_MASK_ADDR32_LSB;
		trb->size = PAMU3_RX_TRBBUF_SIZE;
		trb->ctrl = PAMU3_TRB_CTRL_DEFVAL;
		writel_relaxed(trb->bpl, reg);
		reg++;
		writel_relaxed(trb->bph, reg);
		reg++;
		value = (u32)((u64)trb_dma & PAMU3_MASK_LOWADDR32);
		writel_relaxed(value, trb_reg);
		trb_reg++;
		value = (u32)(((u64)trb_dma >> PAMU3_BITS_LOWADDR32) &
				PAMU3_MASK_ADDR32_LSB);
		writel_relaxed(value, trb_reg);
		trb_reg++;
		bufaddr += PAMU3_RX_TRBBUF_SIZE;
		trb++;
		trb_dma += sizeof(struct pamu3_dwc3_trb);
	}

	value = readl_relaxed(pamu3->base + PAM_U3_RXTRBBUF1_ADDRH);
	value &= PAMU3_MASK_RXTRBBUFSIZE_LSB;
	value = (value & PAMU3_MASK_RXTRBBUFSIZE_LSB) |
		(PAMU3_RX_TRBBUF_SIZE << PAMU3_BIT_RXTRBBUFSIZE_SHIFT);
	writel_relaxed(value, pamu3->base + PAM_U3_RXTRBBUF1_ADDRH);

	trb = (struct pamu3_dwc3_trb *)pamu3->tx_trb_pool;
	trb_dma = pamu3->tx_trb_pool_dma;
	trb_reg = pamu3->base + PAM_U3_TXTRBBUF0_ADDRL;
	for (i = 0; i < PAMU3_TX_TRBBUF_NUM; i++) {
		value = (u32)((u64)trb_dma & PAMU3_MASK_LOWADDR32);
		writel_relaxed(value, trb_reg);
		trb_reg++;
		value = (u32)(((u64)trb_dma >> PAMU3_BITS_LOWADDR32) &
				PAMU3_MASK_ADDR32_LSB);
		writel_relaxed(value, trb_reg);
		trb_reg++;
		trb_dma = pamu3->tx_trb_pool_dma +
			sizeof(struct pamu3_dwc3_trb) * PAMU3_TX_TRB_NUM;
	}

	/* Set ul node header and rnids header buf */
	value = readl_relaxed(pamu3->base + PAM_U3_UL_NODE_HEADER);
	value &= ~PAMU3_MASK_ULNODE_SRC;
	value |= (1 << PAMU3_SHIFT_ULNODE_SRC);
	writel_relaxed(value, pamu3->base + PAM_U3_UL_NODE_HEADER);
	value = (u64)pamu3->rndis_header_buf_dma;
	writel_relaxed(value, pamu3->base + PAM_U3_HEADER_ENDBASE_ADDRL);
	value = (u32)(((u64)pamu3->rndis_header_buf_dma >>
			PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB);
	writel_relaxed(value, pamu3->base + PAM_U3_HEADER_ENDBASE_ADDRH);

	/* Set MBIM NTH and NDP */
	value = PAM_U3_MBIM_DEFNTH;
	writel_relaxed(value, pamu3->base + PAM_U3_MBIM_NCM_NTH);
	value = PAM_U3_MBIM_DEFNDP;
	writel_relaxed(value, pamu3->base + PAM_U3_MBIM_NCM_NDP);
}

/* pamu3_init will be called when controller initializes */
static int sprd_pamu3_open(struct sprd_pamu3 *pamu3)
{
	u64 temp;
	u32 reg;
	int ret;

	/* Enable ipa pamu3 */
	ret = clk_prepare_enable(pamu3->clk);
	if (ret)
		return ret;

	/* Event buffer regs */
	reg = readl_relaxed(pamu3->dwc3_base + REG_DWC3_GEVNTADRLO(1));
	writel_relaxed(reg, pamu3->base + PAM_U3_TXEVENTTBUFFER_ADDRL);
	temp = (u64)readl_relaxed(pamu3->dwc3_base +
			REG_DWC3_GEVNTADRLO(1));
	reg = (temp >> PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(reg, pamu3->base + PAM_U3_TXEVENTTBUFFER_ADDRH);

	reg = readl_relaxed(pamu3->dwc3_base + REG_DWC3_GEVNTADRLO(2));
	writel_relaxed(reg, pamu3->base + PAM_U3_RXEVENTTBUFFER_ADDRL);
	temp = (u64)readl_relaxed(pamu3->dwc3_base +
			REG_DWC3_GEVNTADRLO(2));
	reg = (temp >> PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(reg, pamu3->base + PAM_U3_RXEVENTTBUFFER_ADDRH);

	/* Event buffer size regs */
	reg = (u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTSIZ(1));
	writel_relaxed(reg, pamu3->base + PAM_U3_TXEVTBUFFER_ADDRL);
	reg = (((u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTSIZ(1)) >>
		PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB) |
		((readl_relaxed(pamu3->dwc3_base + REG_DWC3_GEVNTSIZ(1)) &
		PAMU3_MASK_EVTSZ) << PAMU3_BITS_STARTEVTSZ);
	writel_relaxed(reg, pamu3->base + PAM_U3_TXEVTBUFFER_ADDRH);

	reg = (u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTSIZ(2));
	writel_relaxed(reg, pamu3->base + PAM_U3_RXEVTBUFFER_ADDRL);
	reg = (((u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTSIZ(2)) >>
		PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB) |
		((readl_relaxed(pamu3->dwc3_base + REG_DWC3_GEVNTSIZ(2)) &
		PAMU3_MASK_EVTSZ) << PAMU3_BITS_STARTEVTSZ);
	writel_relaxed(reg, pamu3->base + PAM_U3_RXEVTBUFFER_ADDRH);

	/* Event count regs */
	reg = (u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTCOUNT(1));
	writel_relaxed(reg, pamu3->base + PAM_U3_TXEVTCOUNT_ADDRL);
	reg = ((u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTCOUNT(1)) >>
			PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(reg, pamu3->base + PAM_U3_TXEVTCOUNT_ADDRH);

	reg = (u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTCOUNT(2));
	writel_relaxed(reg, pamu3->base + PAM_U3_RXEVTCOUNT_ADDRL);
	reg = ((u64)(pamu3->dwc3_dma + REG_DWC3_GEVNTCOUNT(2)) >>
			PAMU3_BITS_LOWADDR32) & PAMU3_MASK_ADDR32_LSB;
	writel_relaxed(reg, pamu3->base + PAM_U3_RXEVTCOUNT_ADDRH);

	/* Packets per transfer */
	pamu3->max_dl_pkts = PAM_U3_MAX_DLPKTS_DEF;
	pamu3->max_ul_pkts = PAM_U3_MAX_ULPKTS_DEF;

	pamu3_memory_init(pamu3);
	pamu3_load_code(pamu3, pamu3->dwc3_dma + REG_DWC3_DEP_BASE(3),
			pamu3->dwc3_dma + REG_DWC3_DEP_BASE(2));

	atomic_set(&pamu3->inited, 1);

	return 0;
}

static void pamu3_start(struct sprd_pamu3 *pamu3)
{
	u32 value, depth;

	if (atomic_inc_return(&pamu3->ref) == 1)
		sprd_pamu3_open(pamu3);

	value = readl_relaxed(pamu3->base + PAM_U3_CTL0);
	if (value & PAMU3_CTL0_BIT_PAM_EN) {
		depth = pamu3->sipa_info.dl_fifo.fifo_depth;
		value |= PAMU3_CTL0_BIT_RX_START | PAMU3_CTL0_BIT_TX_START;
		writel_relaxed(value, pamu3->base + PAM_U3_CTL0);
		value = readl_relaxed(pamu3->base + PAM_U3_CTL0);
		value |=  PAMU3_CTL0_BIT_USB_EN;
		writel_relaxed(value, pamu3->base + PAM_U3_CTL0);
		pamu3->sipa_params.recv_param.tx_enter_flowctrl_watermark =
			depth - depth / 4;
		pamu3->sipa_params.recv_param.tx_leave_flowctrl_watermark =
			depth / 2;
		pamu3->sipa_params.recv_param.flow_ctrl_cfg = 1;
		pamu3->sipa_params.send_param.flow_ctrl_irq_mode = 2;
		sipa_pam_connect(&pamu3->sipa_params);
		return;
	}
	value = (PAMU3_INTSTS_RXEPINT << PAMU3_SHIFT_INTSTS) |
			(PAMU3_INTSTS_RXCMDERR << PAMU3_SHIFT_INTSTS);
	writel_relaxed(value, pamu3->base + PAM_U3_INR_EN);

	value = readl_relaxed(pamu3->base + PAM_U3_TRB_HEADER);
	value |= PAMU3_TRB_LAST;
	writel_relaxed(value, pamu3->base + PAM_U3_TRB_HEADER);

	value = readl_relaxed(pamu3->base + PAM_U3_SRC_MACH);
	value &= PAMU3_MASK_SRCMAC_ADDRH;
	value |= (MAX_PACKET_NUM << MAX_PACKET_NUM_SHIFT_BIT);
	writel_relaxed(value, pamu3->base + PAM_U3_SRC_MACH);

	pamu3_set_netid(pamu3, pamu3->netid);

	value = readl_relaxed(pamu3->base + PAM_U3_CTL0);
	value |= PAMU3_CTL0_BIT_PAM_EN;
	writel_relaxed(value, pamu3->base + PAM_U3_CTL0);
}

/* PAMU3 main reset */
static int sprd_pamu3_init(struct usb_phy *x)
{
	struct sprd_pamu3 *pamu3 = container_of(x, struct sprd_pamu3, pam);

	if (atomic_read(&pamu3->ref))
		dev_warn(pamu3->dev, "is already opened\n");
	return 0;
}

/* PAMU3 main reset */
static int sprd_pamu3_reset(struct usb_phy *x)
{
	return 0;
}

/* pamu3_post_init will be called when uether function setups */
static int sprd_pamu3_post_init(struct usb_phy *x)
{
	struct sprd_pamu3 *pamu3 = container_of(x, struct sprd_pamu3, pam);

	pamu3_start(pamu3);

	return 0;
}

/* pamu3_set_suspend will be called when uether function is removed */
static int sprd_pamu3_set_suspend(struct usb_phy *x, int a)
{
	struct sprd_pamu3 *pamu3 = container_of(x, struct sprd_pamu3, pam);
	u32 value;
	int timeout = 500;

	if (!atomic_read(&pamu3->inited)) {
		dev_warn(pamu3->dev, "is already disabled\n");
		return 0;
	}

	if (atomic_dec_return(&pamu3->ref)) {
		sipa_disconnect(SIPA_EP_USB, SIPA_DISCONNECT_START);
		value = readl_relaxed(pamu3->base + PAM_U3_CTL0);
		value |= PAMU3_CTL0_BIT_RELEASE;
		writel_relaxed(value, pamu3->base + PAM_U3_CTL0);
		do {
			value = readl_relaxed(pamu3->base + PAM_U3_CTL0);
			if (!timeout--) {
				dev_warn(pamu3->dev,
					 "failed to stop PAMU3!!!\n");
				break;
			}
		} while (!(value & PAMU3_CTL0_BIT_DONE));
	} else {
		value = readl_relaxed(pamu3->base + PAM_U3_CTL0);
		value &= ~(PAMU3_CTL0_BIT_USB_EN | PAMU3_CTL0_BIT_PAM_EN |
			   PAMU3_CTL0_BIT_RELEASE);
		writel_relaxed(value, pamu3->base + PAM_U3_CTL0);
		clk_disable_unprepare(pamu3->clk);
		atomic_set(&pamu3->inited, 0);
		sipa_disconnect(SIPA_EP_USB, SIPA_DISCONNECT_END);
	}
	return 0;
}

/* PAMU3 attributes */
static ssize_t max_dl_pkts_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_pamu3 *pamu3;

	if (!x)
		return -EINVAL;

	pamu3 = container_of(x, struct sprd_pamu3, pam);

	return sprintf(buf, "%d\n", pamu3->max_dl_pkts);
}

static ssize_t max_dl_pkts_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_pamu3 *pamu3;
	u8 max_dl_pkts;

	if (!x)
		return -EINVAL;

	pamu3 = container_of(x, struct sprd_pamu3, pam);
	if (kstrtou8(buf, 10, &max_dl_pkts) < 0)
		return -EINVAL;

	if (max_dl_pkts > 10) {
		dev_err(dev, "Invalid max_dl_pkts value %d\n", max_dl_pkts);
		return -EINVAL;
	}
	pamu3->max_dl_pkts = max_dl_pkts;

	pamu3->sipa_params.send_param.tx_intr_threshold = pamu3->max_dl_pkts;
	sipa_pam_connect(&pamu3->sipa_params);
	return size;
}
static DEVICE_ATTR_RW(max_dl_pkts);

static ssize_t max_ul_pkts_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_pamu3 *pamu3;

	if (!x)
		return -EINVAL;

	pamu3 = container_of(x, struct sprd_pamu3, pam);

	return sprintf(buf, "%d\n", pamu3->max_ul_pkts);
}

static ssize_t max_ul_pkts_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct usb_phy *x = dev_get_drvdata(dev);
	struct sprd_pamu3 *pamu3;
	u8 max_ul_pkts;

	if (!x)
		return -EINVAL;

	pamu3 = container_of(x, struct sprd_pamu3, pam);
	if (kstrtou8(buf, 10, &max_ul_pkts) < 0)
		return -EINVAL;

	if (max_ul_pkts > 10) {
		dev_err(dev, "Invalid max_ul_pkts value %d\n", max_ul_pkts);
		return -EINVAL;
	}
	pamu3->max_ul_pkts = max_ul_pkts;

	return size;
}
static DEVICE_ATTR_RW(max_ul_pkts);

static struct attribute *usb_pamu3_attrs[] = {
	&dev_attr_max_dl_pkts.attr,
	&dev_attr_max_ul_pkts.attr,
	&dev_attr_pamu3_netid.attr,
	NULL
};
ATTRIBUTE_GROUPS(usb_pamu3);

static int sprd_pamu3_probe(struct platform_device *pdev)
{
	struct sprd_pamu3 *pamu3;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	pamu3 = devm_kzalloc(dev, sizeof(*pamu3), GFP_KERNEL);
	if (!pamu3)
		return -ENOMEM;
	pamu3->dev = dev;
	pamu3_tag = pamu3;

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "pamu3_glb_regs");
	if (!res) {
		dev_err(dev, "missing pamu3 global registers resource\n");
		return -ENODEV;
	}

	pamu3->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!pamu3->base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "dwc3_core_regs");
	if (!res) {
		dev_err(dev, "missing DWC3 core registers resource\n");
		return -ENODEV;
	}

	pamu3->clk = devm_clk_get(dev, "pamu3_clk");
	if (IS_ERR(pamu3->clk)) {
		dev_err(dev, "failed to get ipa pamu3 clock\n");
		ret = PTR_ERR(pamu3->clk);
		pamu3->clk = NULL;
		return ret;
	}

	pamu3->dwc3_dma = (void __iomem *)res->start;
	pamu3->dwc3_base = devm_ioremap_nocache(dev,
			res->start, resource_size(res));
	if (!pamu3->dwc3_base)
		return -ENOMEM;

	pamu3->tx_trb_pool = (struct pamu3_dwc3_trb *)dma_alloc_coherent(
			pamu3->dev, sizeof(struct pamu3_dwc3_trb) *
			PAMU3_TX_TRB_NUM * 2,
			&pamu3->tx_trb_pool_dma, GFP_KERNEL);
	if (!pamu3->tx_trb_pool)
		return -ENOMEM;

	pamu3->rx_trb_pool = (struct pamu3_dwc3_trb *)dma_alloc_coherent(
			pamu3->dev, sizeof(struct pamu3_dwc3_trb) *
			PAMU3_RX_TRB_NUM,
			&pamu3->rx_trb_pool_dma, GFP_KERNEL);
	if (!pamu3->rx_trb_pool)  {
		ret = -ENOMEM;
		goto err;
	}

	pamu3->rndis_header_buf = dma_alloc_coherent(pamu3->dev,
			PAMU3_RNDIS_HEADER_SIZE,
			&pamu3->rndis_header_buf_dma, GFP_KERNEL);
	if (!pamu3->rndis_header_buf) {
		ret = -ENOMEM;
		goto err;
	}

	pamu3->rx_max_buf = dma_alloc_coherent(pamu3->dev,
			PAMU3_RX_TRBBUF_SIZE * PAMU3_RX_TRBBUF_NUM,
			&pamu3->rx_max_buf_dma, GFP_KERNEL);
	if (!pamu3->rx_max_buf) {
		ret = -ENOMEM;
		goto err;
	}

	platform_set_drvdata(pdev, pamu3);
	pamu3->pam.dev = dev;
	pamu3->pam.label = "sprd-pamu3";

	pamu3->pam.init = sprd_pamu3_init;
	pamu3->pam.reset_phy = sprd_pamu3_reset;
	pamu3->pam.post_init = sprd_pamu3_post_init;
	pamu3->pam.set_suspend = sprd_pamu3_set_suspend;

	pamu3->pam.type = USB_PAM_TYPE_USB3;

	ret = usb_add_phy_dev(&pamu3->pam);
	if (ret) {
		dev_err(dev, "fail to add usb pam\n");
		goto err;
	}

	ret = sysfs_create_groups(&dev->kobj, usb_pamu3_groups);
	if (ret)
		dev_warn(dev, "failed to create usb pamu3 attributes\n");

	return 0;

err:
	if (pamu3->tx_trb_pool)
		dma_free_coherent(pamu3->dev,
			sizeof(struct pamu3_dwc3_trb) * PAMU3_TX_TRB_NUM * 2,
			pamu3->tx_trb_pool, pamu3->tx_trb_pool_dma);
	if (pamu3->rx_trb_pool)
		dma_free_coherent(pamu3->dev,
			sizeof(struct pamu3_dwc3_trb) * PAMU3_RX_TRB_NUM * 2,
			pamu3->rx_trb_pool, pamu3->rx_trb_pool_dma);
	if (pamu3->rndis_header_buf)
		dma_free_coherent(pamu3->dev, PAMU3_RNDIS_HEADER_SIZE,
			pamu3->rndis_header_buf, pamu3->rndis_header_buf_dma);

	return ret;
}

static int sprd_pamu3_remove(struct platform_device *pdev)
{
	struct sprd_pamu3 *pamu3 = platform_get_drvdata(pdev);

	dma_free_coherent(pamu3->dev,
			sizeof(struct pamu3_dwc3_trb) * PAMU3_TX_TRB_NUM * 2,
			pamu3->tx_trb_pool, pamu3->tx_trb_pool_dma);
	dma_free_coherent(pamu3->dev,
			sizeof(struct pamu3_dwc3_trb) * PAMU3_RX_TRB_NUM * 2,
			pamu3->rx_trb_pool, pamu3->rx_trb_pool_dma);
	dma_free_coherent(pamu3->dev, PAMU3_RNDIS_HEADER_SIZE,
			pamu3->rndis_header_buf, pamu3->rndis_header_buf_dma);
	dma_free_coherent(pamu3->dev,
			PAMU3_RX_TRBBUF_SIZE * PAMU3_RX_TRBBUF_NUM,
			pamu3->rx_max_buf, pamu3->rx_max_buf_dma);

	sysfs_remove_groups(&pdev->dev.kobj, usb_pamu3_groups);

	return 0;
}

static const struct of_device_id sprd_pamu3_match[] = {
	{ .compatible = "sprd,roc1-pamu3" },
	{ .compatible = "sprd,orca-pamu3" },
	{ },
};

MODULE_DEVICE_TABLE(of, sprd_pamu3_match);

static struct platform_driver sprd_pamu3_driver = {
	.probe      = sprd_pamu3_probe,
	.remove     = sprd_pamu3_remove,
	.driver     = {
		.name   = "sprd-pamu3",
		.of_match_table = sprd_pamu3_match,
	},
};

module_platform_driver(sprd_pamu3_driver);

MODULE_ALIAS("platform:sprd-pamu3");
MODULE_AUTHOR("Cheng Zeng <cheng.zeng@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SPREADTRUM USB PAMU3 driver");
