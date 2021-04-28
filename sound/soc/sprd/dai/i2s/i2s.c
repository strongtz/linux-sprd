/*
 * sound/soc/sprd/dai/i2s/i2s.c
 *
 * SPRD SoC CPU-DAI -- SpreadTrum SOC DAI i2s.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" I2S ")""fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd-asoc-common.h"
#ifdef CONFIG_SND_SOC_SPRD_AUDIO_TWO_STAGE_DMAENGINE_SURPPORT
#include "sprd-2stage-dmaengine-pcm.h"
#else
#include "sprd-dmaengine-pcm.h"
#endif
#include "i2s.h"
#include "sprd-i2s.h"

/* register offset */
#define IIS_TXD			(0x0000)
#define IIS_CLKD		(0x0004)
#define IIS_CTRL0		(0x0008)
#define IIS_CTRL1		(0x000C)
#define IIS_CTRL2		(0x0010)
#define IIS_CTRL3		(0x0014)
#define IIS_INT_IEN		(0x0018)
#define IIS_INT_CLR		(0x001C)
#define IIS_INT_RAW		(0x0020)
#define IIS_INT_STS		(0x0024)
#define IIS_STS1		(0x0028)
#define IIS_STS2		(0x002C)
#define IIS_STS3		(0x0030)
#define IIS_DSPWAIT		(0x0034)
#define IIS_CTRL4		(0x0038)
#define IIS_STS4		(0x003C)
#define IIS_CTRL5		(0x0040)
#define IIS_CLKML		(0x0050)
#define IIS_CLKMH		(0x0054)
#define IIS_CLKNL		(0x0058)
#define IIS_CLKNH		(0x005C)

#define I2S_REG(i2s, offset) ((unsigned long)((i2s)->membase + (offset)))
#define I2S_PHY_REG(i2s, offset) ((phys_addr_t)((i2s)->memphys + (offset)))

static unsigned long membase;
static struct i2s_config *dup_config;
char *use_dma_name[] = {
	"iis0_tx", "iis0_rx",
	"iis1_tx", "iis1_tx",
	"iis2_tx", "iis2_tx",
	"iis3_tx", "iis3_tx",
};

#define CMD_BUFFER_LENGTH 64

struct i2s_rtx {
	int dma_no;
};

#define DAI_NAME_SIZE 20
struct i2s_priv {
	struct device *dev;
	struct list_head list;
	struct i2s_rtx rx;
	struct i2s_rtx tx;
	atomic_t open_cnt;
	char dai_name[DAI_NAME_SIZE];
	int irq_no;
	void __iomem *membase;
	unsigned int *memphys;
	unsigned int reg_size;
	struct i2s_config config;
	struct clk *i2s_clk;
	struct snd_soc_dai_driver i2s_dai_driver[1];
	int i2s_type;
};

static struct sprd_pcm_dma_params i2s_pcm_stereo_out = {
	.name = "I2S PCM Stereo out",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES,
		 .src_step = 4,
		 .des_step = 0,
		 },
};

static struct sprd_pcm_dma_params i2s_pcm_stereo_in = {
	.name = "I2S PCM Stereo in",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES,
		 .src_step = 0,
		 .des_step = 4,
		 },
};

/* default pcm config */
static const struct i2s_config def_pcm_config = {
	.hw_port = 0,
	.fs = 8000,
	.slave_timeout = 0xF11,
	.bus_type = PCM_BUS,
	.byte_per_chan = I2S_BPCH_16,
	.mode = I2S_MASTER,
	.lsb = I2S_MSB,
	.rtx_mode = I2S_RTX_MODE,
	.sync_mode = I2S_SYNC,/* I2S_SYNC better! */
	.lrck_inv = I2S_L_LEFT,
	.clk_inv = I2S_CLK_N,
	.pcm_bus_mode = I2S_SHORT_FRAME,
	.pcm_slot = 0x1,
	.pcm_cycle = 1,
	.tx_watermark = 12,
	.rx_watermark = 20,
};

/* default i2s config */
static const struct i2s_config def_i2s_config = {
	.hw_port = 0,
	.fs = 32000,
	.slave_timeout = 0xF11,
	.bus_type = I2S_BUS,
	.byte_per_chan = I2S_BPCH_16,
	.mode = I2S_SLAVE,
	.lsb = I2S_MSB,
	.rtx_mode = I2S_RX_MODE,
	.sync_mode = I2S_LRCK,
	.lrck_inv = I2S_L_LEFT,
	.clk_inv = I2S_CLK_N,
	.i2s_bus_mode = I2S_MSBJUSTFIED,
	.tx_watermark = 12,
	.rx_watermark = 20,
};

static DEFINE_SPINLOCK(i2s_lock);

static inline int i2s_reg_read(unsigned long reg)
{
	return readl_relaxed((void *__iomem)reg);
}

static inline void i2s_reg_raw_write(unsigned long reg, int val)
{
	writel_relaxed(val, (void *__iomem)reg);
}

static inline struct i2s_priv *to_info(struct snd_soc_dai *dai)
{
	return snd_soc_dai_get_drvdata(dai);
}

struct i2s_config *sprd_i2s_dai_to_config(struct snd_soc_dai *dai)
{
	struct i2s_priv *i2s = NULL;
	struct i2s_config *config = NULL;

	i2s = (struct i2s_priv *)to_info(dai);
	config = &i2s->config;
	return config;
}

/*
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int i2s_reg_update(unsigned long reg, int val, int mask)
{
	int new, old;

	spin_lock(&i2s_lock);
	old = i2s_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	i2s_reg_raw_write(reg, new);
	spin_unlock(&i2s_lock);
	sp_asoc_pr_reg("[0x%04lx] U:[0x%08x] R:[0x%08x]\n", reg & 0xFFFF, new,
		       i2s_reg_read(reg));
	return old != new;
}

static int i2s_global_disable(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s\n", __func__);
	return arch_audio_i2s_disable(i2s->config.hw_port);
}

static int i2s_global_enable(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s\n", __func__);
	arch_audio_i2s_enable(i2s->config.hw_port);
	return 0;
}

static int i2s_soft_reset(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s\n", __func__);
	return arch_audio_i2s_reset(i2s->config.hw_port);
}

static int i2s_calc_clk(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int bit_clk;
	int cycle;
	int source_clk;
	int bit;
	int val;
	struct clk *clk_parent;

	switch (config->fs) {
	case 8000:
	case 16000:
	case 32000:
		clk_parent = devm_clk_get(i2s->dev, "clk_twpll_128m");
		break;
	case 9600:
	case 12000:
	case 24000:
	case 48000:
		clk_parent = devm_clk_get(i2s->dev, "clk_twpll_153m6");
		break;
	default:
		pr_err("ERR:I2S Can't Support %d Clock\n", config->fs);
		return -ENOTSUPP;
	}

	if (IS_ERR(clk_parent)) {
		int ret = PTR_ERR(clk_parent);

		pr_err("ERR:I2S Get Clock Source Error %d!\n", ret);
		return ret;
	}
	clk_set_parent(i2s->i2s_clk, clk_parent);
	source_clk = clk_get_rate(clk_parent);
	clk_set_rate(i2s->i2s_clk, source_clk);
	clk_put(clk_parent);

	sp_asoc_pr_dbg("I2S Source Clock is %d HZ\n", source_clk);
	cycle = (config->bus_type == PCM_BUS) ? (config->pcm_cycle + 1) : 2;
	bit = 8 << config->byte_per_chan;
	bit_clk = config->fs * cycle * bit;
	sp_asoc_pr_dbg("I2S BIT Clock is %d HZ\n", bit_clk);
	if (source_clk % (bit_clk << 1))
		return -ENOTSUPP;
	val = (source_clk / bit_clk) >> 1;
	--val;
	return val;
}

static int i2s_calc_clk_m_n(struct i2s_priv *i2s, uint div_mode,
			    int *clk_m, int *clk_n, int *iis_clkd)
{
	struct i2s_config *config = &i2s->config;
	int bit_clk;
	int cycle;
	int source_clk;
	int bit;
	int iis_sck;
	struct clk *clk_parent;

	switch (config->fs) {
	case 8000:
	case 16000:
	case 32000:
		clk_parent = devm_clk_get(i2s->dev, "clk_twpll_128m");
		break;
	case 9600:
	case 12000:
	case 24000:
	case 48000:
		clk_parent = devm_clk_get(i2s->dev, "clk_twpll_153m6");
		break;
	default:
		pr_err("ERR:%s Can't Support %d Clock\n", __func__, config->fs);
		return -ENOTSUPP;
	}

	if (IS_ERR(clk_parent)) {
		int ret = PTR_ERR(clk_parent);

		pr_err("ERR:I2S Get Clock Source Error %d!\n", ret);
		return ret;
	}
	clk_set_parent(i2s->i2s_clk, clk_parent);
	source_clk = clk_get_rate(clk_parent);
	clk_set_rate(i2s->i2s_clk, source_clk);
	clk_put(clk_parent);

	sp_asoc_pr_dbg("%s Source Clock is %d HZ\n", __func__, source_clk);
	cycle = (config->bus_type == PCM_BUS) ? (config->pcm_cycle + 1) : 2;
	bit = 8 << config->byte_per_chan;
	bit_clk = config->fs * cycle * bit;
	*iis_clkd = (source_clk / bit_clk) >> 1;
	*iis_clkd = *iis_clkd - 1;

	if (div_mode == 0) {
		iis_sck = bit_clk;
		*clk_m = 2 * bit_clk / 100;
		*clk_n = source_clk / 100;
	}

	if (div_mode == 1) {
		iis_sck = (source_clk / (*iis_clkd + 1)) >> 1;

		/* iis_sck must not low than
		 * (tartget sample rate )*(channel number)*(channel length)
		 */
		if (iis_sck < bit_clk)
			return -ENOTSUPP;
		*clk_n = source_clk / 100;
		*clk_m = 2 * (*iis_clkd + 1) * bit_clk / 100;
	}
	return 0;
}

static int master_fraction_clock(struct i2s_priv *i2s, uint div_mode)
{
	unsigned long reg_ctrl5 = I2S_REG(i2s, IIS_CTRL5);
	unsigned long reg_clknh = I2S_REG(i2s, IIS_CLKNH);
	unsigned long reg_clknl = I2S_REG(i2s, IIS_CLKNL);
	unsigned long reg_clkmh = I2S_REG(i2s, IIS_CLKMH);
	unsigned long reg_clkml = I2S_REG(i2s, IIS_CLKML);
	unsigned long reg_clkd = I2S_REG(i2s, IIS_CLKD);

	int clk_m, clk_n, clkd;
	int val_clkmh;
	int val_clkml;
	int val_clknh;
	int val_clknl;
	int val;

	val = i2s_calc_clk_m_n(i2s, div_mode, &clk_m, &clk_n, &clkd);
	if (val < 0)
		return val;

	val_clkml = clk_m & 0xffff;
	val_clkmh = clk_m >> 16;

	val_clknl = clk_n & 0xffff;
	val_clknh = clk_n >> 16;

	if (div_mode == 0) {
	/* CTL5[5:4]=h01 IIS_CLK_DVDR_MOD0=1 IIS_CLKD not care*/
		i2s_reg_update(reg_ctrl5, 0x01 << 4, BIT(4) | BIT(5));
		i2s_reg_update(reg_clknh, val_clknh, 0x3f);
		i2s_reg_update(reg_clknl, val_clknl, 0xffff);
		i2s_reg_update(reg_clkmh, val_clkmh, 0x3f);
		i2s_reg_update(reg_clkml, val_clkml, 0xffff);
	} else if (div_mode == 1) {
	/* CTL5[5:4]=h02 IIS_CLK_DVDR_MOD1=1 IIS_CLK_DVDR_MOD0 must be 0*/
		i2s_reg_update(reg_ctrl5, 0x02 << 4, BIT(4) | BIT(5));
		i2s_reg_update(reg_clknh, val_clknh, 0x3f);
		i2s_reg_update(reg_clknl, val_clknl, 0xffff);
		i2s_reg_update(reg_clkmh, val_clkmh, 0x3f);
		i2s_reg_update(reg_clkml, val_clkml, 0xffff);
		i2s_reg_update(reg_clkd, clkd, 0xff);
	} else {
		pr_err("%s not supported div_mode\n", __func__);
		return -ENOTSUPP;
	}

	pr_info("%s div_mode=%d\n", __func__, div_mode);
	return 0;
}

static int i2s_set_clkd(struct i2s_priv *i2s)
{
	int shift = 0;
	int mask = 0xFFFF << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CLKD);

	sp_asoc_pr_dbg("%s\n", __func__);
	val = i2s_calc_clk(i2s);
	if (val < 0)
		return val;
	i2s_reg_update(reg, val, mask);
	return 0;
}

static int i2s_set_clk_m_n(struct i2s_priv *i2s)
{
	int val;
	uint div_mode;

	/* use fraction clock dividng mode0 */
	div_mode = 0;
	val = master_fraction_clock(i2s, div_mode);
	if (val < 0)
		pr_err("%s clk div filed!", __func__);
	return val;
}

static void i2s_set_bus_type(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(15);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (config->bus_type == PCM_BUS) ? mask : 0, mask);
}

static void i2s_set_byte_per_channal(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 4;
	int mask = 0x3 << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->byte_per_chan;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(3);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s mode %d\n", __func__, config->mode);
	i2s_reg_update(reg, (config->mode == I2S_SLAVE) ? mask : 0, mask);
}

static void i2s_set_lsb(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(2);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (config->lsb == I2S_LSB) ? mask : 0, mask);
}

static void i2s_set_rtx_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 6;
	int mask = 0x3 << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->rtx_mode;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_sync_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(9);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (config->sync_mode == I2S_SYNC) ? mask : 0, mask);
}

static void i2s_set_lrck_invert(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(10);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (config->lrck_inv == I2S_L_RIGTH) ? mask : 0, mask);
}

static void i2s_set_clk_invert(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(11);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (config->clk_inv == I2S_CLK_R) ? mask : 0, mask);
}

static void i2s_set_i2s_bus_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(8);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg, (config->i2s_bus_mode == I2S_COMPATIBLE) ? mask : 0,
		       mask);
}

static void i2s_set_pcm_bus_mode(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int mask = BIT(8);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_reg_update(reg,
		       (config->pcm_bus_mode == I2S_SHORT_FRAME) ? mask : 0,
		       mask);
}

static void i2s_set_pcm_slot(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0x7 << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CTRL2);

	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->pcm_slot;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_pcm_cycle(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 3;
	int mask = 0x7F << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CTRL2);

	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->pcm_cycle;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_rx_watermark(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0x1F1F << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CTRL3);

	sp_asoc_pr_dbg("%s\n", __func__);
	/* full watermark */
	val = config->rx_watermark;
	/* empty watermark */
	val |= (I2S_FIFO_DEPTH - config->rx_watermark) << 8;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_tx_watermark(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0x1F1F << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CTRL4);

	sp_asoc_pr_dbg("%s\n", __func__);
	/* empty watermark */
	val = config->tx_watermark << 8;
	/* full watermark */
	val |= I2S_FIFO_DEPTH - config->tx_watermark;
	i2s_reg_update(reg, val << shift, mask);
}

static void i2s_set_slave_timeout(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;
	int shift = 0;
	int mask = 0xFFF << shift;
	int val = 0;
	unsigned long reg = I2S_REG(i2s, IIS_CTRL1);

	sp_asoc_pr_dbg("%s\n", __func__);
	val = config->slave_timeout;
	i2s_reg_update(reg, val << shift, mask);
}

static int i2s_get_dma_data_width(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;

	if (config->bus_type == PCM_BUS) {
		if (config->byte_per_chan == I2S_BPCH_16)
			return DMA_SLAVE_BUSWIDTH_2_BYTES;
	}
	return DMA_SLAVE_BUSWIDTH_4_BYTES;
}

static int i2s_get_dma_step(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;

	if (config->bus_type == PCM_BUS) {
		if (config->byte_per_chan == I2S_BPCH_16)
			return 2;
	}
	return 4;
}

static int i2s_get_data_position(struct i2s_priv *i2s)
{
	struct i2s_config *config = &i2s->config;

	if (config->bus_type == PCM_BUS) {
		if (config->byte_per_chan == I2S_BPCH_16) {
			if (config->mode == I2S_SLAVE)
				return 2;
		}
	}
	return 0;
}

static int i2s_config_rate(struct i2s_priv *i2s)
{
	int ret = 0;
	struct i2s_config *config = &i2s->config;

	if (config->mode == I2S_MASTER)
		ret = i2s_set_clkd(i2s);
	if (ret)
		ret = i2s_set_clk_m_n(i2s);
	return ret;
}

static int i2s_config_apply(struct i2s_priv *i2s)
{
	int ret = 0;
	struct i2s_config *config = &i2s->config;

	sp_asoc_pr_dbg("%s\n", __func__);

	if (config->byte_per_chan == I2S_BPCH_8) {
		pr_err("ERR:The I2S can't Support 8byte Mode in this code\n");
		return -ENOTSUPP;
	}

	i2s_set_bus_type(i2s);
	i2s_set_mode(i2s);
	i2s_set_byte_per_channal(i2s);
	i2s_set_rtx_mode(i2s);
	i2s_set_sync_mode(i2s);
	i2s_set_lsb(i2s);
	i2s_set_lrck_invert(i2s);
	i2s_set_clk_invert(i2s);
	i2s_set_rx_watermark(i2s);
	i2s_set_tx_watermark(i2s);
	if (config->mode == I2S_SLAVE)
		i2s_set_slave_timeout(i2s);
	if (config->bus_type == I2S_BUS)
		i2s_set_i2s_bus_mode(i2s);
	if (config->bus_type == PCM_BUS) {
		i2s_set_pcm_bus_mode(i2s);
		i2s_set_pcm_slot(i2s);
		i2s_set_pcm_cycle(i2s);
	}

	ret = i2s_config_rate(i2s);

	return ret;
}

static void i2s_dma_ctrl(struct i2s_priv *i2s, int enable)
{
	int mask = BIT(14);
	unsigned long reg = I2S_REG(i2s, IIS_CTRL0);

	sp_asoc_pr_dbg("%s Enable = %d\n", __func__, enable);
	if (!enable) {
		if (atomic_read(&i2s->open_cnt) <= 0)
			i2s_reg_update(reg, 0, mask);
	} else {
		i2s_reg_update(reg, mask, mask);
	}
}

static int i2s_close(struct i2s_priv *i2s)
{
	sp_asoc_pr_dbg("%s %d\n", __func__, atomic_read(&i2s->open_cnt));
	if (atomic_dec_and_test(&i2s->open_cnt)) {
		i2s_soft_reset(i2s);
		i2s_global_disable(i2s);
		if (!IS_ERR(i2s->i2s_clk)) {
			clk_disable_unprepare(i2s->i2s_clk);
			clk_put(i2s->i2s_clk);
		}
	}
	return 0;
}

static int i2s_open(struct i2s_priv *i2s)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s %d\n", __func__, atomic_read(&i2s->open_cnt));

	atomic_inc(&i2s->open_cnt);
	if (atomic_read(&i2s->open_cnt) == 1) {
		i2s->i2s_clk = devm_clk_get(i2s->dev,
			arch_audio_i2s_clk_name(i2s->config.hw_port));
		if (IS_ERR(i2s->i2s_clk)) {
			ret = PTR_ERR(i2s->i2s_clk);
			pr_err("ERR:I2S Get clk Error %d!\n", ret);
			return ret;
		}
		i2s_global_enable(i2s);
		i2s_soft_reset(i2s);
		i2s_dma_ctrl(i2s, 0);
		ret = i2s_config_apply(i2s);
		clk_prepare_enable(i2s->i2s_clk);
	}

	return ret;
}

static int i2s_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	int ret;
	struct i2s_priv *i2s = to_info(dai);

	sp_asoc_pr_info("i2s config hw_port %d\n", i2s->config.hw_port);
	ret = i2s_open(i2s);
	if (ret < 0) {
		pr_err("ERR:I2S Open Error!\n");
		return ret;
	}

	return 0;
}

static void i2s_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	struct i2s_priv *i2s = to_info(dai);

	sp_asoc_pr_dbg("%s\n", __func__);
	i2s_close(i2s);
}

static int i2s_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	int ret = 0;
	struct sprd_pcm_dma_params *dma_data;
	struct i2s_priv *i2s = to_info(dai);
	int port = i2s->config.hw_port;

	sp_asoc_pr_dbg("%s Port %d\n", __func__, i2s->config.hw_port);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_data = &i2s_pcm_stereo_out;
		dma_data->channels[0] = i2s->tx.dma_no;
		dma_data->used_dma_channel_name[0] = use_dma_name[2 * port];
		dma_data->desc.fragmens_len = I2S_FIFO_DEPTH -
			i2s->config.tx_watermark;
	} else {
		dma_data = &i2s_pcm_stereo_in;
		dma_data->channels[0] = i2s->rx.dma_no;
		dma_data->used_dma_channel_name[0] = use_dma_name[2 * port + 1];
		dma_data->desc.fragmens_len = i2s->config.rx_watermark;
	}
	dma_data->desc.datawidth = i2s_get_dma_data_width(i2s);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->desc.src_step = i2s_get_dma_step(i2s);
	else
		dma_data->desc.des_step = i2s_get_dma_step(i2s);

	dma_data->dev_paddr[0] =
	    I2S_PHY_REG(i2s, IIS_TXD) + i2s_get_data_position(i2s);

	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	i2s->config.fs = params_rate(params);
	ret = i2s_config_rate(i2s);
	if (ret)
		return ret;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	default:
		ret = -ENOTSUPP;
		pr_err("ERR:I2S Only Supports format S16_LE now!\n");
		break;
	}

	if (params_channels(params) > 2) {
		ret = -ENOTSUPP;
		pr_err("ERR:I2S Can not Supports Grate 2 Channels\n");
	}

	return ret;
}

static int i2s_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	struct i2s_priv *i2s = to_info(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		i2s_dma_ctrl(i2s, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		i2s_dma_ctrl(i2s, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct snd_soc_dai_ops sprd_i2s_dai_ops = {
	.startup = i2s_startup,
	.shutdown = i2s_shutdown,
	.hw_params = i2s_hw_params,
	.trigger = i2s_trigger,
};

static void i2s_config_setting(int index, int value, struct i2s_config *config)
{
	pr_debug("%s i2s index %d,value %d\n", __func__, index, value);
	if (!config) {
		pr_err("%s config is NULL,error!\n", __func__);
		return;
	}
	switch (index) {
	case FS:
		if (value >= SAMPLATE_MIN && value <= SAMPLATE_MAX)
			config->fs = value;
		break;
	case HW_PROT:
		if (value >= 0 && value <= 3)
			config->hw_port = value;
		break;
	case SLAVE_TIMEOUT:
		config->slave_timeout = value;
		break;
	case BUS_TYPE:
		if (value == I2S_BUS || value == PCM_BUS)
			config->bus_type = value;
		break;
	case BYTE_PER_CHAN:
		if (value == I2S_BPCH_8 ||
		    value == I2S_BPCH_16 ||
			value == I2S_BPCH_32)
			config->byte_per_chan = value;
		break;
	case MODE:
		if (value == I2S_MASTER || value == I2S_SLAVE)
			config->mode = value;
		break;
	case LSB:
		if (value == I2S_MSB || value == I2S_LSB)
			config->lsb = value;
		break;
	case TRX_MODE:
		if (value >= I2S_RTX_DIS && value <= I2S_RTX_MODE)
			config->rtx_mode = value;
		break;
	case LRCK_INV:
		if (value == I2S_L_LEFT || value == I2S_L_RIGTH)
			config->lrck_inv = value;
		break;
	case SYNC_MODE:
		if (value == I2S_SYNC || value == I2S_LRCK)
			config->sync_mode = value;
		break;
	case CLK_INV:
		if (value == I2S_CLK_N || value == I2S_CLK_R)
			config->clk_inv = value;
		break;
	case I2S_BUS_MODE:
		if (value == I2S_MSBJUSTFIED || value == I2S_COMPATIBLE)
			config->i2s_bus_mode = value;
		break;
	case PCM_BUS_MODE:
		if (value == I2S_SHORT_FRAME || value == I2S_LONG_FRAME)
			config->pcm_bus_mode = value;
		break;
	case PCM_SLOT:
		if (value >= 1 && value <= 4)
			config->pcm_slot = value;
		break;
	case PCM_CYCLE:
		if (value >= 0 && value <= 127)
			config->pcm_cycle = value;
		break;
	case TX_WATERMARK:
		if (value >= 0 && value <= I2S_FIFO_DEPTH)
			config->tx_watermark = value;
		break;
	case RX_WATERMARK:
		if (value >= 0 && value <= I2S_FIFO_DEPTH)
			config->rx_watermark = value;
		break;
	default:
		pr_err("i2s echo cmd is invalid\n");
		break;
	}
}

static int i2s_config_getting(int index, struct i2s_config *config)
{
	pr_debug("%s i2s index %d\n", __func__, index);
	if (!config) {
		pr_err("%s config is NULL,error\n", __func__);
		return -1;
	}
	switch (index) {
	case FS:
		return config->fs;
	case HW_PROT:
		return config->hw_port;
	case SLAVE_TIMEOUT:
		return config->slave_timeout;
	case BUS_TYPE:
		return config->bus_type;
	case BYTE_PER_CHAN:
		return config->byte_per_chan;
	case MODE:
		return config->mode;
	case LSB:
		return config->lsb;
	case TRX_MODE:
		return config->rtx_mode;
	case LRCK_INV:
		return config->lrck_inv;
	case SYNC_MODE:
		return config->sync_mode;
	case CLK_INV:
		return config->clk_inv;
	case I2S_BUS_MODE:
		return config->i2s_bus_mode;
	case PCM_BUS_MODE:
		return config->pcm_bus_mode;
	case PCM_SLOT:
		return config->pcm_slot;
	case PCM_CYCLE:
		return config->pcm_cycle;
	case TX_WATERMARK:
		return config->tx_watermark;
	case RX_WATERMARK:
		return config->rx_watermark;
	default:
		pr_err("i2s config index is invalid\n");
	return -1;
	}
}

static int get_index(char *line_first, char *line_end)
{
	char *line_index = line_first;
	char *line_first_dummy = line_first;
	unsigned long long index;

	if (!line_first || !line_end)
		return -1;
	for (; line_first < line_end; line_first++) {
		if (*line_first >= '0' && *line_first <= '9') {
			*line_index = *line_first;
			line_index++;
		}
	}
	*line_index = '\0';
	return kstrtoull(line_first_dummy, 10, &index) > 0 ? (int)index : -1;
}

static int get_index_value(char *line_first)
{
	char *line_index_value = line_first;
	char *line_first_dummy = line_first;
	unsigned long long index;

	if (!line_first)
		return -1;
	pr_debug("i2s %s %s\n", __func__, line_first);
	while (line_first++) {
		if (*line_first == '\0')
			break;
		if (*line_first >= '0' && *line_first <= '9') {
			*line_index_value = *line_first;
			line_index_value++;
		}
	}
	*line_index_value = '\0';
	return kstrtoull(line_first_dummy, 10, &index) > 0 ? (int)index : -1;
}

void i2s_debug_write(struct snd_info_entry *entry,
		     struct snd_info_buffer *buffer)
{
	int index;
	int index_value;
	char *line = NULL;
	char *sym_eq = NULL;

	if (!dup_config) {
		pr_err("%s failed!\n", __func__);
		return;
	}
	line = kzalloc(CMD_BUFFER_LENGTH, GFP_KERNEL);
	if (!line)
		return;
	if (!snd_info_get_line(buffer, line, CMD_BUFFER_LENGTH)) {
		pr_debug("i2s: input line %s\n", line);
		sym_eq = strchr(line, '=');
		if (sym_eq) {
			index = get_index(line, sym_eq);
			pr_debug("i2s: index %d\n", index);
			index_value = get_index_value(++sym_eq);
			pr_debug("i2s: index_value %d\n", index_value);
			if (index >= 0 && index_value >= 0)
				i2s_config_setting(index,
						   index_value, dup_config);
		} else if (strstr(line, "pcm")) {
			memcpy(dup_config,
			       &def_pcm_config, sizeof(def_pcm_config));
		} else if (strstr(line, "i2s")) {
			memcpy(dup_config,
			       &def_i2s_config, sizeof(def_i2s_config));
		} else {
			pr_err("i2s:echo str fmt not right\n");
		}
	} else {
		pr_err("i2s echo error\n");
	}
	kfree(line);
}

void i2s_debug_read(struct snd_info_entry *entry,
		    struct snd_info_buffer *buffer)
{
	if (!dup_config) {
		snd_iprintf(buffer, "\n\n dup_config is NUll,error!\n");
		return;
	}
	snd_iprintf(buffer, "\n\n using examples :\n");
	snd_iprintf(buffer, " 1)echo 0=33 > i2s-debug\n");
	snd_iprintf(buffer, " 2)echo i2s > i2s-debug\n");
	snd_iprintf(buffer, " 3)echo pcm >i2s-debug\n\n");
	snd_iprintf(buffer,
		    " 0     fs 0d:%d\n 1     hw_port 0d:%d\n 2     slave_timeout 0x%x\n",
		dup_config->fs, dup_config->hw_port, dup_config->slave_timeout);
	snd_iprintf(buffer,
		    " 3     bus_type(****tip:bus_type 0 is iis,1 is pcm****) 0x%x\n 4     byte_per_chan 0x%x\n",
		dup_config->bus_type, dup_config->byte_per_chan);
	snd_iprintf(buffer,
		    " 5     mode(****tip:mode 0 is master,1 is slave****) 0x%x\n 6     lsb 0x%x\n",
		dup_config->mode, dup_config->lsb);
	snd_iprintf(buffer, " 7     rtx_mode 0x%x\n 8     lrck_inv 0x%x\n",
		    dup_config->rtx_mode, dup_config->lrck_inv);
	snd_iprintf(buffer, " 9     sync_mode 0x%x\n 10    clk_inv 0x%x\n",
		    dup_config->sync_mode, dup_config->clk_inv);
	snd_iprintf(buffer,
		    " 11    i2s_bus_mode 0x%x\n 12    pcm_bus_mode 0x%x\n",
		dup_config->i2s_bus_mode, dup_config->pcm_bus_mode);
	snd_iprintf(buffer, " 13    pcm_slot 0x%x\n 14    pcm_cycle 0x%x\n",
		    dup_config->pcm_slot, dup_config->pcm_cycle);
	snd_iprintf(buffer,
		    " 15    tx_watermark 0d:%d\n 16    rx_watermark 0d:%d\n\n",
		dup_config->tx_watermark, dup_config->rx_watermark);
}

static inline int iis_reg_read(unsigned long reg)
{
	return readl_relaxed((void *__iomem)reg);
}

void i2s_register_proc_read(struct snd_info_entry *entry,
			    struct snd_info_buffer *buffer)
{
	unsigned long reg;

	if (membase == 0) {
		snd_iprintf(buffer, "\n\n i2s membase is NULL\n");
		return;
	}
	pr_debug("i2s membase 0x%lx\n", membase);
	snd_iprintf(buffer, "i2s register dump\n");
	for (reg = IIS_TXD + membase; reg <= IIS_CLKNH + membase; reg += 0x10) {
		snd_iprintf(buffer, "0x%08lx | 0x%08x 0x%08x 0x%08x 0x%08x\n",
			    (reg - IIS_TXD - membase)
			, iis_reg_read(reg + 0x00)
			, iis_reg_read(reg + 0x04)
			, iis_reg_read(reg + 0x08)
			, iis_reg_read(reg + 0x0C)
			);
	}
}

int i2s_config_get(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	if (!dup_config) {
		pr_err("%s return\n", __func__);
		return 0;
	}
	ucontrol->value.integer.value[0] = i2s_config_getting(id, dup_config);
	pr_debug("%s return value %ld,id %d\n", __func__,
		 ucontrol->value.integer.value[0], id);
	return 0;
}

int i2s_config_set(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	if (!dup_config) {
		pr_err("%s return\n", __func__);
		return 0;
	}
	i2s_config_setting(id, ucontrol->value.integer.value[0], dup_config);
	return 0;
}

static const struct snd_soc_component_driver sprd_i2s_component = {
	.name = "i2s",
};

static int i2s_drv_probe(struct platform_device *pdev)
{
	int ret;
	struct i2s_priv *i2s;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;
	const char *dai_name  = NULL;
	const char *config_type = NULL;
	struct regmap *ap_apb_gpr;

	sp_asoc_pr_dbg("%s\n", __func__);

	i2s = devm_kzalloc(&pdev->dev, sizeof(struct i2s_priv), GFP_KERNEL);
	if (!i2s)
		return -ENOMEM;
	i2s->dev = &pdev->dev;
	if (node) {
		u32 val[2];

		if (!arch_audio_get_ap_apb_gpr()) {
			ap_apb_gpr = syscon_regmap_lookup_by_phandle(
				pdev->dev.of_node, "sprd,syscon-ap-apb");
			if (IS_ERR(ap_apb_gpr)) {
				pr_err("ERR: Get the i2s ap apb syscon failed!\n");
				ap_apb_gpr = NULL;
				goto out;
			}
			arch_audio_set_ap_apb_gpr(ap_apb_gpr);
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res) {
			pr_err("ERR:Must give me the base address!\n");
			goto out;
		}
		i2s->memphys = (unsigned int *)res->start;
		i2s->reg_size = (unsigned int)resource_size(res);
		i2s->membase = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(i2s->membase)) {
			pr_err("ERR:i2s reg address ioremap_nocache error !\n");
			return -EINVAL;
		}

		if (of_property_read_string(node, "sprd,dai_name", &dai_name)) {
			pr_err("ERR:Must give me the dai_name!\n");
			/* goto out; */
		}
		if (dai_name)
			strcpy(i2s->dai_name, dai_name);
		sp_asoc_pr_info("dt version i2s->dai_name %s\n", i2s->dai_name);

		if (of_property_read_string(node,
					    "sprd,config_type", &config_type)) {
			i2s->config = def_pcm_config;
			i2s->i2s_type = 0;
			sp_asoc_pr_info(
				"warning:Must give me the config_type! default is pcm\n");
		}
		sp_asoc_pr_info("i2s config_type %s\n", config_type);

		if (config_type) {
			if (strcmp(config_type, "i2s") == 0) {
				i2s->config = def_i2s_config;
				i2s->i2s_type = 1;
			} else if (strcmp(config_type, "pcm") == 0) {
				i2s->config = def_pcm_config;
				i2s->i2s_type = 0;
			} else {
				pr_err("ERR:config_type must be pcm or i2s !\n");
			}
		}

		if (of_property_read_u32(node, "sprd,hw_port", &val[0])) {
			pr_err("ERR:Must give me the hw_port!\n");
			goto out;
		}
		i2s->config.hw_port = val[0];

		if (!of_property_read_u32
			(node, "sprd,fs", &val[0])) {
			i2s->config.fs = val[0];
			sp_asoc_pr_dbg("Change fs to %d!\n", val[0]);
		}
		if (!of_property_read_u32
			(node, "sprd,bus_type", &val[0])) {
			if (val[0])
				i2s->config.bus_type = PCM_BUS;
			else
				i2s->config.bus_type = I2S_BUS;
			sp_asoc_pr_dbg("Change bus_type to %d!\n", val[0]);
		}

		if (!of_property_read_u32
			(node, "sprd,rtx_mode", &val[0])) {
			i2s->config.rtx_mode = val[0];
			sp_asoc_pr_dbg("Change rtx_mode to %d!\n", val[0]);
		}

		if (!of_property_read_u32
			(node, "sprd,slave_timeout", &val[0])) {
			i2s->config.slave_timeout = val[0];
			sp_asoc_pr_dbg("Change slave_timeout to %d!\n", val[0]);
		}
		if (!of_property_read_u32
			(node, "sprd,byte_per_chan", &val[0])) {
			i2s->config.byte_per_chan = val[0];
			sp_asoc_pr_dbg("Change byte per channal to %d!\n",
				       val[0]);
		}
		if (!of_property_read_u32(node, "sprd,slave_mode", &val[0])) {
			if (val[0])
				i2s->config.mode = I2S_SLAVE;
			else
				i2s->config.mode = I2S_MASTER;
			sp_asoc_pr_dbg("Change to %s!\n",
				       val[0] ? "Slave" : "Master");
		}
		if (!of_property_read_u32(node, "sprd,lsb", &val[0])) {
			if (val[0])
				i2s->config.lsb = I2S_LSB;
			else
				i2s->config.lsb = I2S_MSB;
			sp_asoc_pr_dbg("Change to %s!\n",
				       val[0] ? "LSB" : "MSB");
		}
		if (!of_property_read_u32(node, "sprd,lrck", &val[0])) {
			if (val[0])
				i2s->config.sync_mode = I2S_LRCK;
			else
				i2s->config.sync_mode = I2S_SYNC;
			sp_asoc_pr_dbg("Change to %s!\n",
				       val[0] ? "LRCK" : "SYNC");
		}
		if (!of_property_read_u32
		    (node, "sprd,low_for_left", &val[0])) {
			if (val[0])
				i2s->config.lrck_inv = I2S_L_LEFT;
			else
				i2s->config.lrck_inv = I2S_L_RIGTH;
			sp_asoc_pr_dbg("Change to %s!\n",
				       val[0] ?
				       "Low for Left" : "Low for Right");
		}
		if (!of_property_read_u32(node, "sprd,clk_inv", &val[0])) {
			if (val[0])
				i2s->config.clk_inv = I2S_CLK_R;
			else
				i2s->config.clk_inv = I2S_CLK_N;
			sp_asoc_pr_dbg("Change to %s!\n",
				       val[0] ? "CLK INV" : "CLK Normal");
		}
		if (i2s->i2s_type) {
			if (!of_property_read_u32
				(node, "sprd,i2s_compatible", &val[0])) {
				if (val[0])
					/* I2S_SHORT_FRAME */
					i2s->config.i2s_bus_mode =
						I2S_COMPATIBLE;
				else
					/* I2S_LONG_FRAME */
					i2s->config.i2s_bus_mode =
						I2S_MSBJUSTFIED;
				sp_asoc_pr_dbg("Change to %s!\n",
					       val[0] ?
					       "Compatible" : "MSBJustfied");
			}
		}
		if (!of_property_read_u32
			(node, "sprd,pcm_short_frame", &val[0])) {
			if (val[0])
				i2s->config.pcm_bus_mode = I2S_SHORT_FRAME;
			else
				i2s->config.pcm_bus_mode = I2S_LONG_FRAME;
			sp_asoc_pr_dbg("Change to %s!\n",
				       val[0] ? "Short Frame" : "Long Frame");
		}
		if (!i2s->i2s_type) {
			if (!of_property_read_u32(
				node, "sprd,pcm_slot", &val[0])) {
				i2s->config.pcm_slot = val[0];
				sp_asoc_pr_dbg(
					"Change PCM Slot to 0x%x!\n", val[0]);
			}
			if (!of_property_read_u32(
				node, "sprd,pcm_cycle", &val[0])) {
				i2s->config.pcm_cycle = val[0];
				sp_asoc_pr_dbg(
					"Change PCM Cycle to %d!\n", val[0]);
			}
		}
		if (!of_property_read_u32
			(node, "sprd,tx_watermark", &val[0])) {
			i2s->config.tx_watermark = val[0];
			sp_asoc_pr_dbg("Change TX Watermark to %d!\n", val[0]);
		}
		if (!of_property_read_u32
			(node, "sprd,rx_watermark", &val[0])) {
			i2s->config.rx_watermark = val[0];
			sp_asoc_pr_dbg("Change RX Watermark to %d!\n", val[0]);
		}
	} else {
		i2s->config =
		    *((struct i2s_config *)(dev_get_platdata(&pdev->dev)));
		snprintf(i2s->dai_name, DAI_NAME_SIZE, "%s%d",
			 "i2s_bt_sco", pdev->id);
		sp_asoc_pr_info("not bt version i2s->dai_name %s\n",
				i2s->dai_name);
		sp_asoc_pr_dbg("i2s.%d(%d) default fs is (%d)\n", pdev->id,
			       i2s->config.hw_port, i2s->config.fs);

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		i2s->membase = (void __iomem *)res->start;
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		i2s->memphys = (unsigned int *)res->start;

		res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		i2s->tx.dma_no = res->start;
		i2s->rx.dma_no = res->end;
	}

	i2s->tx.dma_no = arch_audio_i2s_tx_dma_info(i2s->config.hw_port);
	i2s->rx.dma_no = arch_audio_i2s_rx_dma_info(i2s->config.hw_port);

	i2s->i2s_dai_driver[0].id = I2S_MAGIC_ID;
	i2s->i2s_dai_driver[0].name = i2s->dai_name;
	i2s->i2s_dai_driver[0].playback.channels_min = 1;
	i2s->i2s_dai_driver[0].playback.channels_max = 2;
	i2s->i2s_dai_driver[0].playback.rates = SNDRV_PCM_RATE_CONTINUOUS;
	i2s->i2s_dai_driver[0].playback.rate_max = 96000;
	i2s->i2s_dai_driver[0].playback.formats = SNDRV_PCM_FMTBIT_S16_LE;

	i2s->i2s_dai_driver[0].capture.channels_min = 1;
	i2s->i2s_dai_driver[0].capture.channels_max = 2;
	i2s->i2s_dai_driver[0].capture.rates = SNDRV_PCM_RATE_CONTINUOUS;
	i2s->i2s_dai_driver[0].capture.rate_max = 96000;
	i2s->i2s_dai_driver[0].capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	i2s->i2s_dai_driver[0].ops = &sprd_i2s_dai_ops;

	sp_asoc_pr_dbg("membase = %p memphys = %p\n", i2s->membase,
		       i2s->memphys);
	sp_asoc_pr_dbg("DMA Number tx = %d rx = %d\n", i2s->tx.dma_no,
		       i2s->rx.dma_no);

	platform_set_drvdata(pdev, i2s);

	atomic_set(&i2s->open_cnt, 0);

	ret = snd_soc_register_component(&pdev->dev,
					 &sprd_i2s_component,
					i2s->i2s_dai_driver,
					ARRAY_SIZE(i2s->i2s_dai_driver));
	sp_asoc_pr_dbg("return %i\n", ret);

	if (strcmp(i2s->dai_name, "i2s_bt_sco0") == 0) {
		dup_config = &i2s->config;
		membase = (unsigned long)i2s->membase;
	}
	return ret;
out:
	return -EINVAL;
}

static int i2s_drv_remove(struct platform_device *pdev)
{
	struct i2s_priv *i2s;

	i2s = platform_get_drvdata(pdev);

	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2s_of_match[] = {
	{.compatible = "sprd,i2s",},
	{},
};

MODULE_DEVICE_TABLE(of, i2s_of_match);
#endif

static struct platform_driver i2s_driver = {
	.driver = {
		.name = "i2s",
		.owner = THIS_MODULE,
		.of_match_table = i2s_of_match,
	},

	.probe = i2s_drv_probe,
	.remove = i2s_drv_remove,
};

module_platform_driver(i2s_driver);

MODULE_DESCRIPTION("SPRD ASoC I2S CUP-DAI driver");
MODULE_AUTHOR("Ken Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("cpu-dai:i2s");
