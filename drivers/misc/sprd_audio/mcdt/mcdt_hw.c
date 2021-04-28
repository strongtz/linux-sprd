/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "[MCDT] "fmt

#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include "mcdt_hw.h"
#include "mcdt_phy_v0.h"

#define MCDT_REG_SIZE			(0x160 + 0x10)
#define MCDT_DMA_AP_CHANNEL		5

/* AGCP AHB registers doesn't defined by global header file. So
 * define them here.
 */
#define REG_AGCP_AHB_MODULE_EB0_STS	0x00
#define BIT_MCDT_EN	BIT(12)

static unsigned long membase;
static unsigned long memphys;
static unsigned long mem_dma_phys;
static unsigned int mcdt_reg_size;
static unsigned int  mcdt_irq_no;

static struct channel_status g_dac_channel[dac_channel_max];
static struct channel_status g_adc_channel[adc_channel_max];
static struct irq_desc_mcdt gadc_irq_desc[adc_channel_max];
static struct irq_desc_mcdt gdac_irq_desc[dac_channel_max];
static int mcdt_adc_dma_channel[MCDT_DMA_AP_CHANNEL];
static int mcdt_dac_dma_channel[MCDT_DMA_AP_CHANNEL];
static struct mutex mcdt_mutex;

static struct regmap *agcp_ahb_gpr;

static DEFINE_SPINLOCK(mcdt_lock);

static bool check_agcp_mcdt_clock(void)
{
	u32 val;
	int ret;

	if (agcp_ahb_gpr) {
		ret = regmap_read(agcp_ahb_gpr, REG_AGCP_AHB_MODULE_EB0_STS,
				  &val);
		if (!ret)
			return (val & BIT_MCDT_EN) ? true : false;
	}
	return true;
}

static inline int mcdt_reg_read(unsigned long reg)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return 0;
	}
	return readl_relaxed((void *__iomem)reg);
}

static inline void mcdt_reg_raw_write(unsigned long reg, int val)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	writel_relaxed(val, (void *__iomem)reg);
}

static int mcdt_reg_update(unsigned int offset, int val, int mask)
{
	unsigned long reg;
	int new, old;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return 0;
	}

	reg = membase + offset;
	spin_lock(&mcdt_lock);
	old = mcdt_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	mcdt_reg_raw_write(reg, new);
	spin_unlock(&mcdt_lock);

	return old != new;
}

static void mcdt_da_set_watermark(enum MCDT_CHAN_NUM chan_num,
	unsigned int full, unsigned int empty)
{
	unsigned int reg = MCDT_DAC0_WTMK + (chan_num * 4);

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(reg, BIT_DAC0_FIFO_AF_LVL(full) |
			BIT_DAC0_FIFO_AE_LVL(empty),
			BIT_DAC0_FIFO_AF_LVL(0x1FF) |
			BIT_DAC0_FIFO_AE_LVL(0x1FF));
}

static void mcdt_ad_set_watermark(enum MCDT_CHAN_NUM chan_num,
	unsigned int full, unsigned int empty)
{
	unsigned int reg = MCDT_ADC0_WTMK + (chan_num * 4);

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(reg, BIT_ADC0_FIFO_AF_LVL(full) |
			BIT_ADC0_FIFO_AE_LVL(empty),
			BIT_ADC0_FIFO_AF_LVL(0x1FF) |
			BIT_ADC0_FIFO_AE_LVL(0x1FF));
}

static void mcdt_da_dma_enable(enum MCDT_CHAN_NUM chan_num, unsigned int en)
{
	unsigned int shift = 16 + chan_num;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(MCDT_DMA_EN, en ? (1 << shift) : 0, 1 << shift);
}

static void mcdt_ad_dma_enable(enum MCDT_CHAN_NUM chan_num, unsigned int en)
{
	unsigned int shift = chan_num;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(MCDT_DMA_EN, en ? (1 << shift) : 0, 1 << shift);
}

static void mcdt_chan_fifo_int_enable(enum MCDT_CHAN_NUM chan_num,
				 enum MCDT_FIFO_INT int_type, unsigned int en)
{
	unsigned int reg = 0;
	unsigned int shift = 0;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_INT_EN0;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_INT_EN1;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_INT_EN2;
	else
		return;

	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + int_type;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + int_type;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + int_type;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + int_type;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, en ? (1<<shift) : 0, 1 << shift);
}

static void mcdt_chan_fifo_int_clr(enum MCDT_CHAN_NUM chan_num,
			      enum MCDT_FIFO_INT int_type)
{
	unsigned int reg = 0;
	unsigned int shift = 0;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_INT_CLR0;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_INT_CLR1;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_INT_CLR2;
	else
		return;

	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + int_type;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + int_type;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + int_type;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + int_type;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, 1<<shift, 1<<shift);
}

static int mcdt_is_chan_fifo_int_raw(enum MCDT_CHAN_NUM chan_num,
				enum MCDT_FIFO_INT int_type)
{
	unsigned long reg = 0;
	unsigned int shift = 0;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return 0;
	}
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_INT_RAW1;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_INT_RAW2;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_INT_RAW3;
	else
		return 0;

	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + int_type;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + int_type;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + int_type;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + int_type;
		break;
	}
	reg = reg + membase;

	return !!(mcdt_reg_read(reg)&(1<<shift));
}

static unsigned int mcdt_is_da_empty_int(enum MCDT_CHAN_NUM chan_num)
{
	return mcdt_is_chan_fifo_int_raw(chan_num, MCDT_DAC_FIFO_AE_INT);
}

static unsigned int mcdt_is_ad_full_int(enum MCDT_CHAN_NUM chan_num)
{
	return mcdt_is_chan_fifo_int_raw(chan_num, MCDT_ADC_FIFO_AF_INT);
}

static void mcdt_da_int_en(enum MCDT_CHAN_NUM chan_num, unsigned int en)
{
	mcdt_chan_fifo_int_enable(chan_num, MCDT_DAC_FIFO_AE_INT, en);
}

static void mcdt_ad_int_en(enum MCDT_CHAN_NUM chan_num, unsigned int en)
{
	mcdt_chan_fifo_int_enable(chan_num, MCDT_ADC_FIFO_AF_INT, en);
}

static void mcdt_clr_da_int(enum MCDT_CHAN_NUM chan_num)
{
	mcdt_chan_fifo_int_clr(chan_num, MCDT_DAC_FIFO_AE_INT);
}

static void mcdt_clr_ad_int(enum MCDT_CHAN_NUM chan_num)
{
	mcdt_chan_fifo_int_clr(chan_num, MCDT_ADC_FIFO_AF_INT);
}

static int mcdt_is_chan_fifo_sts(unsigned int chan_num,
	enum MCDT_FIFO_STS fifo_sts)
{
	unsigned long reg = 0;
	unsigned int shift = 0;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return 0;
	}
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_CH_FIFO_ST0;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_CH_FIFO_ST1;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_CH_FIFO_ST2;
	else
		return 0;

	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + fifo_sts;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + fifo_sts;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + fifo_sts;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + fifo_sts;
		break;
	}
	reg = reg + membase;

	return !!(mcdt_reg_read(reg) & (1<<shift));
}

static void mcdt_int_to_ap_enable(enum MCDT_CHAN_NUM chan_num, unsigned int en)
{
	unsigned int reg = MCDT_INT_MSK_CFG0;
	unsigned int shift = 0 + chan_num;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(reg, en ? (1 << shift) : 0, 1 << shift);
}

static void mcdt_ap_dac_dma_ch0_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH0_SEL0(chan_num),
			BIT_MCDT_DAC_DMA_CH0_SEL0(0xf));
}

static void mcdt_ap_dac_dma_ch1_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH1_SEL0(chan_num),
			BIT_MCDT_DAC_DMA_CH1_SEL0(0xf));
}

static void mcdt_ap_dac_dma_ch2_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH2_SEL0(chan_num),
			BIT_MCDT_DAC_DMA_CH2_SEL0(0xf));
}

static void mcdt_ap_dac_dma_ch3_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH3_SEL0(chan_num),
			BIT_MCDT_DAC_DMA_CH3_SEL0(0xf));
}

static void mcdt_ap_dac_dma_ch4_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH4_SEL0(chan_num),
			BIT_MCDT_DAC_DMA_CH4_SEL0(0xf));
}

static void mcdt_ap_adc_dma_ch0_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH0_SEL0(chan_num),
			BIT_MCDT_ADC_DMA_CH0_SEL0(0xf));
}

static void mcdt_ap_adc_dma_ch1_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH1_SEL0(chan_num),
			BIT_MCDT_ADC_DMA_CH1_SEL0(0xf));
}

static void mcdt_ap_adc_dma_ch2_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH2_SEL0(chan_num),
			BIT_MCDT_ADC_DMA_CH2_SEL0(0xf));
}

static void mcdt_ap_adc_dma_ch3_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH3_SEL0(chan_num),
			BIT_MCDT_ADC_DMA_CH3_SEL0(0xf));
}

static void mcdt_ap_adc_dma_ch4_sel(unsigned int chan_num)
{
	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH4_SEL0(chan_num),
			BIT_MCDT_ADC_DMA_CH4_SEL0(0xf));
}

static void mcdt_dac_dma_chan_ack_sel(unsigned int chan_num,
				 enum MCDT_DMA_ACK ack_num)
{
	unsigned int reg;
	unsigned int shift;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_DMA_CFG2;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_DMA_CFG3;
	else
		return;

	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN8:
		shift = 0;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN9:
		shift = 4;
		break;
	case MCDT_CHAN2:
		shift = 8;
		break;
	case MCDT_CHAN3:
		shift = 12;
		break;
	case MCDT_CHAN4:
		shift = 16;
		break;
	case MCDT_CHAN5:
		shift = 20;
		break;
	case MCDT_CHAN6:
		shift = 24;
		break;
	case MCDT_CHAN7:
		shift = 28;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, ack_num<<shift, 0xf<<shift);
}

static void mcdt_adc_dma_chan_ack_sel(unsigned int chan_num,
				 enum MCDT_DMA_ACK ack_num)
{
	unsigned int reg;
	unsigned int shift;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_DMA_CFG4;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_DMA_CFG5;
	else
		return;

	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN8:
		shift = 0;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN9:
		shift = 4;
		break;
	case MCDT_CHAN2:
		shift = 8;
		break;
	case MCDT_CHAN3:
		shift = 12;
		break;
	case MCDT_CHAN4:
		shift = 16;
		break;
	case MCDT_CHAN5:
		shift = 20;
		break;
	case MCDT_CHAN6:
		shift = 24;
		break;
	case MCDT_CHAN7:
		shift = 28;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, ack_num<<shift, 0xf<<shift);
}

void mcdt_da_fifo_clr(unsigned int chan_num)
{
	unsigned int shift = 0 + chan_num;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(MCDT_FIFO_CLR, 1<<shift, 1<<shift);
}

void mcdt_ad_fifo_clr(unsigned int chan_num)
{
	unsigned int shift = 16 + chan_num;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return;
	}

	mcdt_reg_update(MCDT_FIFO_CLR, 1<<shift, 1<<shift);
}

static unsigned int mcdt_is_da_fifo_real_full(enum MCDT_CHAN_NUM chan_num)
{
	return mcdt_is_chan_fifo_sts(chan_num, MCDT_DAC_FIFO_REAL_FULL);
}

static unsigned int mcdt_is_ad_fifo_real_empty(enum MCDT_CHAN_NUM chan_num)
{
	return mcdt_is_chan_fifo_sts(chan_num, MCDT_ADC_FIFO_REAL_EMPTY);
}

static unsigned int mcdt_wait_dac_tx_fifo_valid(enum MCDT_CHAN_NUM chan_num)
{
	while (mcdt_is_da_fifo_real_full(chan_num))
		usleep_range(10, 15);

	return 0;
}

static unsigned int mcdt_wait_adc_rx_fifo_valid(enum MCDT_CHAN_NUM chan_num)
{
	while (mcdt_is_ad_fifo_real_empty(chan_num))
		usleep_range(10, 15);

	return 0;
}

static unsigned int VAL_FLG;

static unsigned int mcdt_dac_phy_write(enum MCDT_CHAN_NUM chan_num,
	unsigned int val, unsigned int *err_code)
{
	unsigned int ret = 0;
	unsigned int write = 0;
	unsigned int reg;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return 0;
	}

	*err_code = 0;
	reg = membase + MCDT_CH0_TXD + chan_num * 4;
	switch (VAL_FLG) {
	case MCDT_I2S_RW_FIFO_I2S_16:
		write = (ret = (val & 0xFFFF))<<16;
		break;
	case MCDT_I2S_RW_FIFO_DEF:
	default:
		write = ret = val;
		break;
	}

	if (mcdt_is_da_fifo_real_full(chan_num))
		*err_code = -1;
	else
		mcdt_reg_raw_write(reg, write);

	return ret;
}

static unsigned int mcdt_adc_phy_read(enum MCDT_CHAN_NUM chan_num,
		unsigned int *err_code)
{
	unsigned int read = 0;
	unsigned int ret = 0;
	unsigned long reg;

	if (!check_agcp_mcdt_clock()) {
		pr_err("%s agcp mcdt clocl not available\n", __func__);
		return 0;
	}

	*err_code = 0;
	reg = membase + MCDT_CH0_RXD + chan_num * 4;
	if (mcdt_is_ad_fifo_real_empty(chan_num))
		*err_code = -1;
	else
		read = mcdt_reg_read(reg);

	switch (VAL_FLG) {
	case MCDT_I2S_RW_FIFO_DEF:
	default:
		ret = read;
		break;
	case MCDT_I2S_RW_FIFO_I2S_16:
		ret = (read >> 16)&0xFFFF;
		break;
	}

	return ret;
}

static unsigned int mcdt_dac_int_init(enum MCDT_CHAN_NUM id)
{
	mcdt_da_int_en(id, 1);
	if (g_adc_channel[id].int_enabled == 0) {
		mcdt_int_to_ap_enable(id, 1);
		enable_irq(mcdt_irq_no);
	}

	return 0;
}

static unsigned int mcdt_adc_int_init(enum MCDT_CHAN_NUM id)
{
	mcdt_ad_int_en(id, 1);
	if (g_dac_channel[id].int_enabled == 0) {
		mcdt_int_to_ap_enable(id, 1);
		enable_irq(mcdt_irq_no);
	}

	return 0;
}

static void mcdt_isr_tx_handler(struct irq_desc_mcdt *tx_para,
	unsigned int channel)
{
	tx_para->irq_handler(tx_para->data, channel);
	g_dac_channel[channel].int_count++;
	mcdt_clr_da_int(channel);/* TBD */
}

static void mcdt_isr_rx_handler(struct irq_desc_mcdt *rx_para,
	unsigned int channel)
{
	rx_para->irq_handler(rx_para->data, channel);
	g_adc_channel[channel].int_count++;
	mcdt_clr_da_int(channel);/* TBD */
}

static irqreturn_t mcdt_isr_mcdt_all_handler(int irq, void *dev)
{
	unsigned int i;

	for (i = MCDT_CHAN0; i <= MCDT_CHAN9; i++) {
		if (g_dac_channel[i].int_enabled &&
		    mcdt_is_da_empty_int(i)) {
			mcdt_isr_tx_handler(&gdac_irq_desc[i], i);
		}
		if (g_adc_channel[i].int_enabled &&
		    mcdt_is_ad_full_int(i)) {
			mcdt_isr_rx_handler(&gadc_irq_desc[i], i);
		}
	}

	return 0;
}

static int mcdt_send_data_use_dma(unsigned int channel,
				   enum MCDT_AP_DMA_CHAN dma_chan)
{
	int uid = -1;

	mcdt_da_dma_enable(channel, 1);
	switch (dma_chan) {
	case MCDT_AP_DMA_CH0:
		mcdt_ap_dac_dma_ch0_sel(channel);
		mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK0);
		uid = MCDT_AP_DAC_CH0_WR_REQ + 1;
		break;
	case MCDT_AP_DMA_CH1:
		mcdt_ap_dac_dma_ch1_sel(channel);
		mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK1);
		uid = MCDT_AP_DAC_CH1_WR_REQ + 1;
		break;
	case MCDT_AP_DMA_CH2:
		mcdt_ap_dac_dma_ch2_sel(channel);
		mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK2);
		uid = MCDT_AP_DAC_CH2_WR_REQ + 1;
		break;
	case MCDT_AP_DMA_CH3:
		mcdt_ap_dac_dma_ch3_sel(channel);
		mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK3);
		uid = MCDT_AP_DAC_CH3_WR_REQ + 1;
		break;
	case MCDT_AP_DMA_CH4:
		mcdt_ap_dac_dma_ch4_sel(channel);
		mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK4);
		uid = MCDT_AP_DAC_CH4_WR_REQ + 1;
		break;
	default:
		uid = -1;
		break;
	}

	return uid;
}

/*
 *return : dma request uid,err return -1.
 */
static int mcdt_rev_data_use_dma(unsigned int channel,
				  enum MCDT_AP_DMA_CHAN dma_chan)
{
	int uid = -1;

	mcdt_ad_dma_enable(channel, 1);

	switch (dma_chan) {
	case MCDT_AP_DMA_CH0:
		mcdt_ap_adc_dma_ch0_sel(channel);
		mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK0);
		uid = MCDT_AP_ADC_CH0_RD_REQ + 1;
		break;
	case MCDT_AP_DMA_CH1:
		mcdt_ap_adc_dma_ch1_sel(channel);
		mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK1);
		uid = MCDT_AP_ADC_CH1_RD_REQ + 1;
		break;
	case MCDT_AP_DMA_CH2:
		mcdt_ap_adc_dma_ch2_sel(channel);
		mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK2);
		uid = MCDT_AP_ADC_CH2_RD_REQ + 1;
		break;
	case MCDT_AP_DMA_CH3:
		mcdt_ap_adc_dma_ch3_sel(channel);
		mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK3);
		uid = MCDT_AP_ADC_CH3_RD_REQ + 1;
		break;
	case MCDT_AP_DMA_CH4:
		mcdt_ap_adc_dma_ch4_sel(channel);
		mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK4);
		uid = MCDT_AP_ADC_CH4_RD_REQ + 1;
		break;
	default:
		uid = -1;
		break;
	}

	return uid;
}

static unsigned int mcdt_sent_to_mcdt(unsigned int channel,
				      unsigned int *tx_buf)
{
	unsigned int err_code = 0;

	if (!tx_buf)
		return -1;

	mcdt_wait_dac_tx_fifo_valid(channel);
	mcdt_dac_phy_write(channel, *tx_buf, &err_code);

	return err_code;
}

int mcdt_write(unsigned int channel, char *tx_buf, unsigned int size)
{
	unsigned int i = 0;
	unsigned int size_dword = size/4;
	unsigned int *temp_buf = (unsigned int *)tx_buf;

	while (i < size_dword) {
		if (mcdt_sent_to_mcdt(channel, temp_buf + i))
			return -1;
		i++;
	}

	return 0;
}

static unsigned int mcdt_recv_from_mcdt(unsigned int channel,
					unsigned int *rx_buf)
{
	unsigned int err_code = 0;

	if (!rx_buf)
		return -1;

	mcdt_wait_adc_rx_fifo_valid(channel);
	*rx_buf = mcdt_adc_phy_read(channel, &err_code);

	return err_code;
}

int mcdt_read(unsigned int channel, char *rx_buf, unsigned int size)
{
	unsigned int i = 0;
	unsigned int size_dword = size/4;
	unsigned int *temp_buf = (unsigned int *)rx_buf;

	while (i < size_dword) {
		if (mcdt_recv_from_mcdt(channel, temp_buf + i))
			return -1;
		i++;
	}

	return 0;
}

unsigned long mcdt_adc_phy_addr(unsigned int channel)
{
	return memphys + MCDT_CH0_RXD + channel * 4;
}

unsigned long mcdt_dac_phy_addr(unsigned int channel)
{
	return memphys + MCDT_CH0_TXD + channel * 4;
}

unsigned long mcdt_adc_dma_phy_addr(unsigned int channel)
{
	return mem_dma_phys + MCDT_CH0_RXD + channel * 4;
}

unsigned long mcdt_dac_dma_phy_addr(unsigned int channel)
{
	return mem_dma_phys + MCDT_CH0_TXD + channel * 4;
}

unsigned int mcdt_is_adc_full(unsigned int channel)
{
	return mcdt_is_chan_fifo_sts(channel, MCDT_ADC_FIFO_REAL_FULL);
}

unsigned int mcdt_is_adc_empty(unsigned int channel)
{
	return mcdt_is_chan_fifo_sts(channel, MCDT_ADC_FIFO_REAL_EMPTY);
}

unsigned int mcdt_is_dac_full(unsigned int channel)
{
	return mcdt_is_chan_fifo_sts(channel, MCDT_DAC_FIFO_REAL_FULL);
}

unsigned int mcdt_is_dac_empty(unsigned int channel)
{
	return mcdt_is_chan_fifo_sts(channel, MCDT_DAC_FIFO_REAL_EMPTY);
}

unsigned int mcdt_dac_buffer_size_avail(unsigned int channel)
{
	unsigned long reg = MCDT_DAC0_FIFO_ADDR_ST + channel * 8 + membase;
	unsigned int r_addr = (mcdt_reg_read(reg)>>16)&0x3FF;
	unsigned int w_addr = mcdt_reg_read(reg)&0x3FF;

	if (w_addr > r_addr)
		return 4 * (FIFO_LENGTH - w_addr + r_addr);
	else
		return 4 * (r_addr - w_addr);
}

unsigned int mcdt_adc_data_size_avail(unsigned int channel)
{
	unsigned long reg = MCDT_ADC0_FIFO_ADDR_ST + channel * 8 + membase;
	unsigned int r_addr = (mcdt_reg_read(reg)>>16)&0x3FF;
	unsigned int w_addr = mcdt_reg_read(reg)&0x3FF;

	return (w_addr > r_addr ? (4 * (w_addr - r_addr)) :
		(4 * (FIFO_LENGTH - r_addr + w_addr)));
}

int mcdt_dac_int_enable(unsigned int channel,
			void (*callback)(void*, unsigned int),
			void *data, unsigned int emptymark)
{
	if (g_dac_channel[channel].int_enabled == 1 ||
	    g_dac_channel[channel].dma_enabled == 1)
		return -1;

	mcdt_da_fifo_clr(channel);
	mcdt_da_set_watermark(channel, FIFO_LENGTH - 1, emptymark);

	if (callback != NULL) {
		gdac_irq_desc[channel].irq_handler = callback;
		gdac_irq_desc[channel].data = data;
		mcdt_dac_int_init(channel);
	}
	g_dac_channel[channel].int_enabled = 1;

	return 0;
}

int mcdt_adc_int_enable(unsigned int channel,
			void (*callback)(void*, unsigned int),
			void *data, unsigned int fullmark)
{
	if (g_adc_channel[channel].int_enabled == 1 ||
	    g_adc_channel[channel].dma_enabled == 1)
		return -1;

	mcdt_ad_fifo_clr(channel);
	mcdt_ad_set_watermark(channel, fullmark, FIFO_LENGTH - 1);

	if (callback != NULL) {
		gadc_irq_desc[channel].irq_handler = callback;
		gadc_irq_desc[channel].data = data;
		mcdt_adc_int_init(channel);
	}

	g_adc_channel[channel].int_enabled = 1;

	return 0;
}

static int mcdt_dma_channel_get(unsigned int channel)
{
	int dma_channel;

	switch (channel) {
	case MCDT_CHAN0:
		dma_channel = MCDT_AP_DMA_CH0;
		break;
	case MCDT_CHAN1:
		dma_channel = MCDT_AP_DMA_CH1;
		break;
	case MCDT_CHAN2:
		dma_channel = MCDT_AP_DMA_CH2;
		break;
	case MCDT_CHAN3:
		dma_channel = MCDT_AP_DMA_CH3;
		break;
	case MCDT_CHAN4:
		dma_channel = MCDT_AP_DMA_CH4;
		break;
	default:
		dma_channel = -EINVAL;
		pr_err("adc no dma id for :%d", channel);
		break;
	}
	return dma_channel;
}

/* return: uid, error if < 0 */
int mcdt_dac_dma_enable(unsigned int channel, unsigned int emptymark)
{
	int uid = -1;
	int dma_channel;

	if (g_dac_channel[channel].int_enabled == 1 ||
	    g_dac_channel[channel].dma_enabled == 1)
		return -1;

	dma_channel = mcdt_dma_channel_get(channel);
	if (dma_channel < 0 ||
		dma_channel >= MCDT_DMA_AP_CHANNEL) {
		pr_err("dma_channel error:%d", dma_channel);
		return -EINVAL;
	}
	mutex_lock(&mcdt_mutex);
	if (mcdt_dac_dma_channel[dma_channel]) {
		pr_err("dma_channel is already used:%d", dma_channel);
		mutex_unlock(&mcdt_mutex);
		return -EBUSY;
	}
	mcdt_dac_dma_channel[dma_channel] = 1;
	mutex_unlock(&mcdt_mutex);

	mcdt_da_fifo_clr(channel);
	mcdt_da_set_watermark(channel, FIFO_LENGTH - 1, emptymark);
	uid = mcdt_send_data_use_dma(channel, dma_channel);
	if (uid > 0) {
		pr_info("%s %d mcdt_dma_ap_channel=%d\n",
			__func__, __LINE__, dma_channel);
		g_dac_channel[channel].dma_enabled = 1;
		g_dac_channel[channel].dma_channel = dma_channel;
	}

	return uid;
}

/*
 *return: uid, error if < 0
 */
int mcdt_adc_dma_enable(unsigned int channel, unsigned int fullmark)
{
	int uid = -1;
	int dma_channel;

	if (g_adc_channel[channel].int_enabled == 1 ||
	    g_adc_channel[channel].dma_enabled == 1)
		return -1;
	dma_channel = mcdt_dma_channel_get(channel);
	if (dma_channel < 0 ||
		dma_channel >= MCDT_DMA_AP_CHANNEL) {
		pr_err("adc dma_channel error:%d", dma_channel);
		return -EINVAL;
	}
	mutex_lock(&mcdt_mutex);
	if (mcdt_adc_dma_channel[dma_channel]) {
		pr_err("adc dma_channel is already used:%d", dma_channel);
		mutex_unlock(&mcdt_mutex);
		return -EBUSY;
	}
	mcdt_adc_dma_channel[dma_channel] = 1;
	mutex_unlock(&mcdt_mutex);

	mcdt_ad_fifo_clr(channel);
	mcdt_ad_set_watermark(channel, fullmark, FIFO_LENGTH - 1);
	uid = mcdt_rev_data_use_dma(channel, dma_channel);
	if (uid > 0) {
		pr_info("%s %d mcdt_dma_ap_channel=%d\n",
			__func__, __LINE__, dma_channel);
		g_adc_channel[channel].dma_enabled = 1;
		g_adc_channel[channel].dma_channel = dma_channel;
	}

	return uid;
}


void mcdt_dac_dma_disable(unsigned int channel)
{
	if (!g_dac_channel[channel].dma_enabled)
		return;

	mcdt_da_dma_enable(channel, 0);
	mcdt_da_fifo_clr(channel);
	mutex_lock(&mcdt_mutex);
	mcdt_dac_dma_channel[g_dac_channel[channel].dma_channel] = 0;
	mutex_unlock(&mcdt_mutex);
	g_dac_channel[channel].dma_enabled = 0;
	g_dac_channel[channel].dma_channel = 0;
}

void mcdt_adc_dma_disable(unsigned int channel)
{
	if (!g_adc_channel[channel].dma_enabled)
		return;

	mcdt_ad_dma_enable(channel, 0);
	mcdt_ad_fifo_clr(channel);
	mutex_lock(&mcdt_mutex);
	mcdt_adc_dma_channel[g_adc_channel[channel].dma_channel] = 0;
	mutex_unlock(&mcdt_mutex);
	g_adc_channel[channel].dma_enabled = 0;
	g_adc_channel[channel].dma_channel = 0;
}

int mcdt_dac_disable(unsigned int channel)
{
	if (g_dac_channel[channel].int_enabled) {
		if (gdac_irq_desc[channel].irq_handler) {
			mcdt_da_int_en(channel, 0);
			mcdt_clr_da_int(channel);
			gdac_irq_desc[channel].irq_handler = NULL;
			gdac_irq_desc[channel].data = NULL;
			if (g_adc_channel[channel].int_enabled == 0) {
				mcdt_int_to_ap_enable(channel, 0);
				disable_irq_nosync(mcdt_irq_no);
			}
		}
		g_dac_channel[channel].int_enabled = 0;
	}

	if (g_dac_channel[channel].dma_enabled) {
		mcdt_da_dma_enable(channel, 0);
		mutex_lock(&mcdt_mutex);
		mcdt_dac_dma_channel[g_dac_channel[channel].dma_channel] = 0;
		mutex_unlock(&mcdt_mutex);
		g_dac_channel[channel].dma_enabled = 0;
	}
	mcdt_da_fifo_clr(channel);

	return 0;
}

int mcdt_adc_disable(unsigned int channel)
{
	if (g_adc_channel[channel].int_enabled) {
		if (gadc_irq_desc[channel].irq_handler) {
			mcdt_ad_int_en(channel, 0);
			mcdt_clr_ad_int(channel);
			gadc_irq_desc[channel].irq_handler = NULL;
			gadc_irq_desc[channel].data = NULL;
			if (g_dac_channel[channel].int_enabled == 0) {
				mcdt_int_to_ap_enable(channel, 0);
				disable_irq_nosync(mcdt_irq_no);
			}
		}
		g_adc_channel[channel].int_enabled = 0;
	}

	if (g_adc_channel[channel].dma_enabled) {
		mcdt_ad_dma_enable(channel, 0);
		mutex_lock(&mcdt_mutex);
		mcdt_adc_dma_channel[g_adc_channel[channel].dma_channel] = 0;
		mutex_unlock(&mcdt_mutex);
		g_adc_channel[channel].dma_enabled = 0;
	}
	mcdt_da_fifo_clr(channel);

	return 0;

}

#if defined(CONFIG_DEBUG_FS)
static int mcdt_debug_info_show(struct seq_file *m, void *private)
{
	int i = 0;

	seq_printf(m, "\nmembase=0x%lx, memphys=0x%lx, mem_dma_phys=0x%lx, mcdt_reg_size=0x%x, mcdt_irq_no=%d\n",
		membase, memphys, mem_dma_phys, mcdt_reg_size, mcdt_irq_no);
	seq_puts(m, "\n");

	for (i = 0; i < dac_channel_max; i++) {
		seq_printf(m, "g_dac_channel[%d]: int_enabled=%d, dma_enabled=%d, dma_channel=%d\n",
			   i, g_dac_channel[i].int_enabled,
			   g_dac_channel[i].dma_enabled,
			   g_dac_channel[i].dma_channel);
	}
	seq_puts(m, "\n");

	for (i = 0; i < adc_channel_max; i++) {
		seq_printf(m, "g_adc_channel[%d]: int_enabled=%d, dma_enabled=%d, dma_channel=%d\n",
			   i, g_adc_channel[i].int_enabled,
			   g_adc_channel[i].dma_enabled,
			   g_adc_channel[i].dma_channel);
	}
	seq_puts(m, "\n");

	seq_puts(m, "mcdt_dac_dma_channel:\n");
	for (i = 0; i < MCDT_DMA_AP_CHANNEL; i++)
		seq_printf(m, "%d ", mcdt_dac_dma_channel[i]);
	seq_puts(m, "\n");

	seq_puts(m, "mcdt_adc_dma_channel:\n");
	for (i = 0; i < MCDT_DMA_AP_CHANNEL; i++)
		seq_printf(m, "%d ", mcdt_adc_dma_channel[i]);
	seq_puts(m, "\n");

	return 0;
}

static int mcdt_debug_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, mcdt_debug_info_show, inode->i_private);
}

static const struct file_operations mcdt_debug_info_fops = {
	.open = mcdt_debug_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int mcdt_init_debugfs_info(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("info", 0444, (struct dentry *)root, NULL,
			    &mcdt_debug_info_fops);

	return 0;
}

static int mcdt_debug_reg_show(struct seq_file *m, void *private)
{
	/*int i = 0;*/
	int val = 0, *pval;

	pval = &val;
	seq_printf(m, "MCDT register dump: phy_addr=0x%lx\n", memphys);

	return 0;
}

static int mcdt_debug_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mcdt_debug_reg_show, inode->i_private);
}

static const struct file_operations sblock_debug_reg_fops = {
	.open = mcdt_debug_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int mcdt_init_debugfs_reg(void *root)
{
	if (!root)
		return -ENXIO;

	debugfs_create_file("reg", 0444, (struct dentry *)root, NULL,
		&sblock_debug_reg_fops);

	return 0;
}

static int __init mcdt_init_debugfs(void)
{
	struct dentry *root = debugfs_create_dir("mcdt", NULL);

	if (!root)
		return -ENXIO;

	mcdt_init_debugfs_info(root);
	mcdt_init_debugfs_reg(root);

	return 0;
}

device_initcall(mcdt_init_debugfs);
#endif /* CONFIG_DEBUG_FS */

static int mcdt_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;
	u32 val = 0;

	if (!node) {
		pr_err("ERR: %s, node is NULL!\n", __func__);
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("mcdt reg parse error!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "sprd,ap-addr-offset", &val);
	if (ret) {
		pr_err("ERR: %s :no property of 'reg'\n", __func__);
		return -EINVAL;
	}

	agcp_ahb_gpr = syscon_regmap_lookup_by_phandle(node,
						       "sprd,syscon-agcp-ahb");
	if (IS_ERR(agcp_ahb_gpr)) {
		pr_warn("ERR: [%s] Get the agcp ahb syscon failed!\n",
			__func__);
		agcp_ahb_gpr = NULL;
	}

	memphys = res->start;
	mem_dma_phys = memphys - val;
	mcdt_reg_size = (unsigned int)(res->end - res->start) + 1;

	membase = (unsigned long)devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_VALUE(membase)) {
		pr_err("ERR:mcdt reg address ioremap_nocache error!\n");
		return -ENOMEM;
	}

	mcdt_irq_no = platform_get_irq(pdev, 0);
	mutex_init(&mcdt_mutex);
	ret = devm_request_irq(&pdev->dev, mcdt_irq_no,
		mcdt_isr_mcdt_all_handler, 0, "sprd_mcdt_ap", NULL);
	if (ret) {
		pr_err("mcdt ERR:Request irq ap failed!\n");
		return ret;
	}
	disable_irq_nosync(mcdt_irq_no);

	return 0;
}

static const struct of_device_id mcdt_of_match[] = {
	{.compatible = "sprd,sharkl5-mcdt", },
	{.compatible = "sprd,roc1-mcdt", },
	{ }
};
static struct platform_driver mcdt_driver = {
	.driver = {
		.name = "mcdt_sprd",
		.owner = THIS_MODULE,
		.of_match_table = mcdt_of_match,
	},
	.probe = mcdt_probe,
};

module_platform_driver(mcdt_driver);

MODULE_DESCRIPTION("mcdt driver v1");
MODULE_AUTHOR("Zuo Wang <zuo.wang@spreadtrum.com>");
MODULE_LICENSE("GPL");

