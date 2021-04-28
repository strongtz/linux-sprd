/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-phy-r3p0.c
 *
 * SPRD SoC VBC -- SpreadTrum SOC for VBC driver function.
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
#define pr_fmt(fmt) pr_sprd_fmt(" VBC ") ""fmt

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sound/soc.h>

#include "audio-sipc.h"
#include "sprd-asoc-common.h"
#include "vbc-phy-v4.h"

int sprd_get_codec_dp_base(void)
	__attribute__((weak, alias("__sprd_get_codec_dp_base")));
int __sprd_get_codec_dp_base(void)
{
	pr_info("warning: %s, no codec dp base\n", __func__);
	return 0;
}

int sprd_codec_virt_mclk_mixer_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
	__attribute__((weak, alias("__sprd_codec_virt_mclk_mixer_put")));

int __sprd_codec_virt_mclk_mixer_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_info("warning: %s, no codec virt mclock\n", __func__);
	return 0;
}

#define TO_STRING(e) #e

static void *sprd_ap_vbc_virt_base;
static u32 sprd_ap_vbc_phy_base;
static u32 vbc_ap_dsp_offset;

/* vbc reg read/write/update */
u32 vbc_phy_ap2dsp(u32 addr)
{
	u32 ret_val = 0;

	ret_val = addr - vbc_ap_dsp_offset;
	return ret_val;
}

u32 get_vbc_dsp_ap_offset(void)
{
	return vbc_ap_dsp_offset;
}

void set_vbc_dsp_ap_offset(u32 offset)
{
	vbc_ap_dsp_offset = offset;
}

u32 get_ap_vbc_phy_base(void)
{
	return sprd_ap_vbc_phy_base;
}

void set_ap_vbc_phy_base(u32 phy_addr)
{
	sprd_ap_vbc_phy_base = phy_addr;
}

void *get_ap_vbc_virt_base(void)
{
	return sprd_ap_vbc_virt_base;
}

void set_ap_vbc_virt_base(void *virt_addr)
{
	sprd_ap_vbc_virt_base = virt_addr;
}

static DEFINE_SPINLOCK(vbc_reg_lock);

int ap_vbc_reg_update(u32 reg, u32 val, u32 mask)
{
	u32 new_v, old_v, updated;

	spin_lock(&vbc_reg_lock);
	old_v = readl_relaxed((void *__iomem)(reg + get_ap_vbc_virt_base()));
	new_v = (old_v & ~mask) | (val & mask);
	writel_relaxed(new_v, (void *__iomem)(reg + get_ap_vbc_virt_base()));
	updated = readl_relaxed((void *__iomem)(reg + get_ap_vbc_virt_base()));
	spin_unlock(&vbc_reg_lock);
	sp_asoc_pr_reg("[0x%04x] U:[0x%08x] R:[0x%08x]\n",
		reg & 0xFFFFFFFF, new_v,
		updated);

	return old_v != new_v;
}

u32 ap_vbc_reg_read(u32 reg)
{
	u32 ret;

	spin_lock(&vbc_reg_lock);
	ret = readl_relaxed((void *__iomem)(reg + get_ap_vbc_virt_base()));
	spin_unlock(&vbc_reg_lock);

	return ret;
}

int ap_vbc_reg_write(u32 reg, u32 val)
{
	u32 updated_v;

	spin_lock(&vbc_reg_lock);
	writel_relaxed(val, (void *__iomem)(reg + get_ap_vbc_virt_base()));
	updated_v = readl_relaxed((void *__iomem)(reg +
		get_ap_vbc_virt_base()));
	spin_unlock(&vbc_reg_lock);
	sp_asoc_pr_reg("AP-VBC:[0x%04x] W:[0x%08x] R:[0x%08x]\n",
		reg & 0xFFFFFFFF, val, updated_v);

	return 0;
}

/*********************************************************
 *********************************************************
 * ap phy define
 *********************************************************
 *********************************************************/

const char *ap_vbc_fifo_id2name(int id)
{
	const char * const vbc_fifo_name[AP_FIFO_MAX] = {
		[AP01_PLY_FIFO] = TO_STRING(AP01_PLY_FIFO),
		[AP01_REC_FIFO] = TO_STRING(AP01_REC_FIFO),
		[AP23_PLY_FIFO] = TO_STRING(AP23_PLY_FIFO),
		[AP23_REC_FIFO] = TO_STRING(AP23_REC_FIFO),
	};

	if (id >= AP_FIFO_MAX) {
		pr_err("invalid id %s %d\n", __func__, id);
		return "";
	}
	if (!vbc_fifo_name[id]) {
		pr_err("null string =%d\n", id);
		return "";
	}

	return vbc_fifo_name[id];
}

const char *ap_vbc_watermark_type2name(int type)
{
	const char * const vbc_watermark_type_name[WATERMARK_TYPE_MAX] = {
		[FULL_WATERMARK] = TO_STRING(FULL_WATERMARK),
		[EMPTY_WATERMARK] = TO_STRING(EMPTY_WATERMARK),
	};

	if (type >= WATERMARK_TYPE_MAX) {
		pr_err("invalid id %s %d\n", __func__, type);
		return "";
	}
	if (!vbc_watermark_type_name[type]) {
		pr_err("null string =%d\n", type);
		return "";
	}

	return vbc_watermark_type_name[type];
}

/* AP FIFO CTRL WATERMARK */
int ap_vbc_set_watermark(int fifo_id, int watermark_type, u32 watermark)
{
	u32 reg;
	u32 mask;
	u32 val;

	if (watermark_type != FULL_WATERMARK &&
		watermark_type != EMPTY_WATERMARK) {
		pr_err("invalid watermark_type = %d\n", watermark_type);
		return 0;
	}
	switch (fifo_id) {
	case AP01_PLY_FIFO:
		reg = REG_VBC_AUDPLY_FIFO_CTRL;
		if (watermark_type == FULL_WATERMARK) {
			val = BITS_RF_AUDPLY_FIFO_FULL_LVL(watermark);
			mask = AUDPLY01_FIFO_FULL_LVL_MASK;
		} else {
			val = BITS_RF_AUDPLY_FIFO_EMPTY_LVL(watermark);
			mask = AUDPLY01_FIFO_EMPTY_LVL_MASK;
		}
		break;
	case AP01_REC_FIFO:
		reg = REG_VBC_AUDRCD_FIFO_CTRL;
		if (watermark_type == FULL_WATERMARK) {
			val = BITS_RF_AUDRCD_FIFO_FULL_LVL(watermark);
			mask = AUDRCD01_FIFO_FULL_LVL_MASK;
		} else {
			val = BITS_RF_AUDRCD_FIFO_EMPTY_LVL(watermark);
			mask = AUDRCD01_FIFO_EMPTY_LVL_MASK;
		}
		break;
	case AP23_PLY_FIFO:
		reg = REG_VBC_AUDPLY_FIFO23_CTRL;
		if (watermark_type == FULL_WATERMARK) {
			val = BITS_RF_AUDPLY_FIFO23_FULL_LVL(watermark);
			mask = AUDPLY23_FIFO_FULL_LVL_MASK;
		} else {
			val = BITS_RF_AUDPLY_FIFO23_EMPTY_LVL(watermark);
			mask = AUDPLY23_FIFO_EMPTY_LVL_MASK;
		}
		break;
	case AP23_REC_FIFO:
		reg = REG_VBC_AUDRCD_FIFO23_CTRL;
		if (watermark_type == FULL_WATERMARK) {
			val = BITS_RF_AUDRCD_FIFO23_FULL_LVL(watermark);
			mask = AUDRCD23_FIFO_FULL_LVL_MASK;
		} else {
			val = BITS_RF_AUDRCD_FIFO23_EMPTY_LVL(watermark);
			mask = AUDRCD23_FIFO_EMPTY_LVL_MASK;
		}
		break;
	default:
		pr_err("%s unknown fifo_id=%d, watermark_type = %s, val = %u\n",
			__func__, fifo_id,
			ap_vbc_watermark_type2name(watermark_type), watermark);
		return 0;
	}
	ap_vbc_reg_update(reg, val, mask);
	pr_info("%s fifo_id=%s, watermark_type = %s, watermark = %u\n",
			__func__, ap_vbc_fifo_id2name(fifo_id),
			ap_vbc_watermark_type2name(watermark_type), watermark);

	return 0;
}

const char *ap_vbc_datafmt2name(int fmt)
{
	const char * const vbc_datafmt_name[VBC_DAT_FMT_MAX] = {
		[VBC_DAT_H24] = TO_STRING(VBC_DAT_H24),
		[VBC_DAT_L24] = TO_STRING(VBC_DAT_L24),
		[VBC_DAT_H16] = TO_STRING(VBC_DAT_H16),
		[VBC_DAT_L16] = TO_STRING(VBC_DAT_L16),
	};

	if (fmt >= VBC_DAT_FMT_MAX) {
		pr_err("invalid id %s %d\n", __func__, fmt);
		return "";
	}
	if (!vbc_datafmt_name[fmt]) {
		pr_err("null string =%d\n", fmt);
		return "";
	}

	return vbc_datafmt_name[fmt];
}

/* AP FIFO CTRL DATA FORMAT */
void ap_vbc_data_format_set(int fifo_id, enum VBC_DAT_FORMAT dat_fmt)
{
	u32 reg;
	u32 mask;
	u32 val;
	u32 shift;

	switch (fifo_id) {
	case AP01_PLY_FIFO:
	case AP23_PLY_FIFO:
		reg = REG_VBC_AUDPLY_FIFO_CTRL;
		shift = AUDPLY_DAT_FORMAT_CTL_SHIFT;
		mask = AUDPLY_DAT_FORMAT_MASK;
		break;
	case AP01_REC_FIFO:
	case AP23_REC_FIFO:
		reg = REG_VBC_AUDRCD_FIFO_CTRL;
		shift = AUDRCD_DAT_FORMAT_CTL_SHIFT;
		mask = AUDRCD_DAT_FORMAT_MASK;
		break;
	default:
		pr_err("%s unknown fifo_id=%d, dat_fmt = %s\n",
			__func__, fifo_id,
			ap_vbc_datafmt2name(dat_fmt));
		return;
	}
	val = dat_fmt << shift;
	ap_vbc_reg_update(reg, val, mask);
	pr_info("fifo_id =%s dat_fmt =%s\n",
		ap_vbc_fifo_id2name(fifo_id), ap_vbc_datafmt2name(dat_fmt));
}

/* AP FIFO ENABLE */
const char *ap_vbc_chan_id2name(int chan_id)
{
	const char * const vbc_chan_name[VBC_CHAN_MAX] = {
		[VBC_LEFT] = TO_STRING(VBC_LEFT),
		[VBC_RIGHT] = TO_STRING(VBC_RIGHT),
		[VBC_ALL_CHAN] = TO_STRING(VBC_ALL_CHAN),
	};

	if (chan_id >= VBC_CHAN_MAX) {
		pr_err("invalid chan_id %s %d\n", __func__, chan_id);
		return "";
	}
	if (!vbc_chan_name[chan_id]) {
		pr_err("null string =%d\n", chan_id);
		return "";
	}

	return vbc_chan_name[chan_id];
}

int ap_vbc_fifo_enable(int fifo_id, int chan, int enable)
{
	u32 reg;
	u32 mask;
	u32 bit;
	u32 val;

	reg = REG_VBC_AUD_EN;
	switch (fifo_id) {
	case AP01_PLY_FIFO:
		switch (chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDPLY_AP_FIFO0_EN;
			mask = BIT_RF_AUDPLY_AP_FIFO0_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDPLY_AP_FIFO1_EN;
			mask = BIT_RF_AUDPLY_AP_FIFO1_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDPLY_AP_FIFO0_EN |
				BIT_RF_AUDPLY_AP_FIFO1_EN;
			mask = BIT_RF_AUDPLY_AP_FIFO0_EN |
				BIT_RF_AUDPLY_AP_FIFO1_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, chan);
			return 0;
		}
		break;
	case AP23_PLY_FIFO:
		switch (chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDPLY_AP_FIFO2_EN;
			mask = BIT_RF_AUDPLY_AP_FIFO2_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDPLY_AP_FIFO3_EN;
			mask = BIT_RF_AUDPLY_AP_FIFO3_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDPLY_AP_FIFO2_EN |
				BIT_RF_AUDPLY_AP_FIFO3_EN;
			mask = BIT_RF_AUDPLY_AP_FIFO2_EN |
				BIT_RF_AUDPLY_AP_FIFO3_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, chan);
			return 0;
		}
		break;
	case AP01_REC_FIFO:
		switch (chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDRCD_AP_FIFO0_EN;
			mask = BIT_RF_AUDRCD_AP_FIFO0_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDRCD_AP_FIFO1_EN;
			mask = BIT_RF_AUDRCD_AP_FIFO1_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDRCD_AP_FIFO0_EN |
				BIT_RF_AUDRCD_AP_FIFO1_EN;
			mask = BIT_RF_AUDRCD_AP_FIFO0_EN |
				BIT_RF_AUDRCD_AP_FIFO1_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, chan);
			return 0;
		}
		break;
	case AP23_REC_FIFO:
		switch (chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDRCD_AP_FIFO2_EN;
			mask = BIT_RF_AUDRCD_AP_FIFO2_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDRCD_AP_FIFO3_EN;
			mask = BIT_RF_AUDRCD_AP_FIFO3_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDRCD_AP_FIFO2_EN |
				BIT_RF_AUDRCD_AP_FIFO3_EN;
			mask = BIT_RF_AUDRCD_AP_FIFO2_EN |
				BIT_RF_AUDRCD_AP_FIFO3_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, chan);
			return 0;
		}
		break;
	default:
		pr_err("%s unknown fifo_id=%d, vbc_chan =%s, enable=%d\n",
		__func__, fifo_id,
		ap_vbc_chan_id2name(chan), enable);
		return -EINVAL;
	}
	if (enable)
		val = bit;
	else
		val = ~bit;
	ap_vbc_reg_update(reg, bit, mask);
	pr_info("%s fifo_id=%s, vbc_chan =%s, enable=%d\n",
		__func__, ap_vbc_fifo_id2name(fifo_id),
		ap_vbc_chan_id2name(chan), enable);

	return 0;
}

/* AP FIFO CLEAR */
void ap_vbc_fifo_clear(int fifo_id)
{
	u32 reg;
	u32 bit;
	u32 mask;

	reg = REG_VBC_AUD_CLR;
	switch (fifo_id) {
	case AP01_PLY_FIFO:
		bit = BIT_RF_AUDPLY_FIFO_CLR;
		mask = BIT_RF_AUDPLY_FIFO_CLR;
		break;
	case AP23_PLY_FIFO:
		bit = BIT_RF_AUDPLY23_FIFO_CLR;
		mask = BIT_RF_AUDPLY23_FIFO_CLR;
		break;
	case AP01_REC_FIFO:
		bit = BIT_RF_AUDRCD_FIFO_CLR;
		mask = BIT_RF_AUDRCD_FIFO_CLR;
		break;
	case AP23_REC_FIFO:
		bit = BIT_RF_AUDRCD23_FIFO_CLR;
		mask = BIT_RF_AUDRCD23_FIFO_CLR;
		break;
	default:
		pr_err("%s unknown fifo_id=%d\n", __func__, fifo_id);
		return;
	}
	ap_vbc_reg_update(reg, bit, mask);
	pr_info("%s fifo=%s\n", __func__, ap_vbc_fifo_id2name(fifo_id));
}

/* AP FIFO DMA ENABLE */
void ap_vbc_aud_dma_chn_en(int fifo_id, int vbc_chan, int enable)
{
	u32 reg;
	u32 mask;
	u32 bit;
	u32 val;

	reg = REG_VBC_AUD_DMA_EN;
	switch (fifo_id) {
	case AP01_PLY_FIFO:
		switch (vbc_chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDPLY_DMA_DA0_EN;
			mask = BIT_RF_AUDPLY_DMA_DA0_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDPLY_DMA_DA1_EN;
			mask = BIT_RF_AUDPLY_DMA_DA1_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDPLY_DMA_DA0_EN |
				BIT_RF_AUDPLY_DMA_DA1_EN;
			mask = BIT_RF_AUDPLY_DMA_DA0_EN |
				BIT_RF_AUDPLY_DMA_DA1_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, vbc_chan);
			return;
		}
		break;
	case AP23_PLY_FIFO:
		switch (vbc_chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDPLY_DMA_DA2_EN;
			mask = BIT_RF_AUDPLY_DMA_DA2_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDPLY_DMA_DA3_EN;
			mask = BIT_RF_AUDPLY_DMA_DA3_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDPLY_DMA_DA2_EN |
				BIT_RF_AUDPLY_DMA_DA3_EN;
			mask = BIT_RF_AUDPLY_DMA_DA2_EN |
				BIT_RF_AUDPLY_DMA_DA3_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, vbc_chan);
			return;
		}
		break;
	case AP01_REC_FIFO:
		switch (vbc_chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDRCD_DMA_AD0_EN;
			mask = BIT_RF_AUDRCD_DMA_AD0_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDRCD_DMA_AD1_EN;
			mask = BIT_RF_AUDRCD_DMA_AD1_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDRCD_DMA_AD0_EN |
				BIT_RF_AUDRCD_DMA_AD1_EN;
			mask = BIT_RF_AUDRCD_DMA_AD0_EN |
				BIT_RF_AUDRCD_DMA_AD1_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, vbc_chan);
			return;
		}
		break;
	case AP23_REC_FIFO:
		switch (vbc_chan) {
		case VBC_LEFT:
			bit = BIT_RF_AUDRCD_DMA_AD2_EN;
			mask = BIT_RF_AUDRCD_DMA_AD2_EN;
			break;
		case VBC_RIGHT:
			bit = BIT_RF_AUDRCD_DMA_AD3_EN;
			mask = BIT_RF_AUDRCD_DMA_AD3_EN;
			break;
		case VBC_ALL_CHAN:
			bit = BIT_RF_AUDRCD_DMA_AD2_EN |
				BIT_RF_AUDRCD_DMA_AD3_EN;
			mask = BIT_RF_AUDRCD_DMA_AD2_EN |
				BIT_RF_AUDRCD_DMA_AD3_EN;
			break;
		default:
			pr_err("%s invalid chan=%d\n", __func__, vbc_chan);
			return;
		}
		break;
	default:
		pr_err("%s unknown fifo_id=%d, vbc_chan =%s, enable=%d\n",
		__func__, fifo_id,
		ap_vbc_chan_id2name(vbc_chan), enable);
		return;
	}
	if (enable)
		val = bit;
	else
		val = ~bit;
	ap_vbc_reg_update(reg, bit, mask);
	pr_info("%s fifo_id=%s, vbc_chan =%s, enable=%d\n",
		__func__, ap_vbc_fifo_id2name(fifo_id),
		ap_vbc_chan_id2name(vbc_chan), enable);
}

void vbc_phy_audply_set_src_mode(int en, int mode)
{
	u32 reg;
	u32 val;
	u32 mask = AUDPLY_AP01_SRC_MODE_MASK;

	reg = REG_VBC_AUD_SRC_CTRL;
	val = AUDPLY_AP_SRC_MODE(mode);
	ap_vbc_reg_update(reg, val, mask);
	reg = REG_VBC_AUD_EN;
	mask = BIT_AP01_RCD_SRC_EN_0 | BIT_AP01_RCD_SRC_EN_1;
	if (en)
		val = mask;
	else
		val = 0;

	ap_vbc_reg_update(reg, val, mask);
}

/*********************************************************
 *********************************************************
 * dsp phy define
 *********************************************************
 *********************************************************/

/*********************************************************
 * cmd for SND_VBC_DSP_IO_KCTL_SET
 *********************************************************/

/* SND_KCTL_TYPE_REG */
int dsp_vbc_reg_write(u32 reg, int val, u32 mask)
{
	int ret;
	struct sprd_vbc_kcontrol vbc_reg = { };

	vbc_reg.reg = reg;
	vbc_reg.value = val;
	vbc_reg.mask = mask;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_REG, -1,
		SND_VBC_DSP_IO_KCTL_SET,
		&vbc_reg, sizeof(struct sprd_vbc_kcontrol),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
	sp_asoc_pr_dbg("%s reg=%x, value = %x, mask=%x\n", __func__,
		reg, val, mask);

	return 0;
}

u32 dsp_vbc_reg_read(u32 reg)
{
	int ret;
	struct sprd_vbc_kcontrol vbc_reg = { };

	vbc_reg.reg = reg;
	vbc_reg.mask = 0xFFFFFFFF;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_REG,
		-1, SND_VBC_DSP_IO_KCTL_GET,
		&vbc_reg, sizeof(struct sprd_vbc_kcontrol),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed, ret: %d\n", __func__, ret);

	return vbc_reg.value;
}

/* SND_KCTL_TYPE_MDG */
int dsp_vbc_mdg_set(int id, int enable, int mdg_step)
{
	int ret;
	struct vbc_mute_dg_para mdg_para = { };

	mdg_para.mdg_id = id;
	mdg_para.mdg_mute = enable;
	mdg_para.mdg_step = mdg_step;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MDG,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mdg_para, sizeof(struct vbc_mute_dg_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_SRC */
int dsp_vbc_src_set(int id, int32_t fs)
{
	int ret;
	struct vbc_src_para src_para = { };

	src_para.src_id = id;
	src_para.fs = fs;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_SRC,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&src_para, sizeof(struct vbc_src_para),
		AUDIO_SIPC_WAIT_FOREVER);

	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_VBC_DSP_IO_KCTL_SET */
int dsp_vbc_dg_set(int id, int dg_l, int dg_r)
{
	int ret;
	struct vbc_dg_para dg_para = { };

	dg_para.dg_id = id;
	dg_para.dg_left = dg_l;
	dg_para.dg_right = dg_r;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_DG,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&dg_para, sizeof(struct vbc_dg_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_SMTHDG */
int dsp_vbc_smthdg_set(int id, int smthdg_l, int smthdg_r)
{
	int ret;
	struct vbc_smthdg_para smthdg_para = { };

	smthdg_para.smthdg_id = id;
	smthdg_para.smthdg_left = smthdg_l;
	smthdg_para.smthdg_right = smthdg_r;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_SMTHDG,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&smthdg_para, sizeof(struct vbc_smthdg_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_SMTHDG_STEP */
int dsp_vbc_smthdg_step_set(int id, int smthdg_step)
{
	int ret;
	struct vbc_smthdg_step_para smthdg_step_para = { };

	smthdg_step_para.smthdg_id = id;
	smthdg_step_para.step = smthdg_step;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_SMTHDG_STEP,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&smthdg_step_para, sizeof(struct vbc_smthdg_step_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MIXERDG_MAIN */
int dsp_vbc_mixerdg_mainpath_set(int id,
	int mixerdg_main_l, int mixerdg_main_r)
{
	int ret;
	struct vbc_mixerdg_mainpath_para mixerdg_mainpath_para = { };

	mixerdg_mainpath_para.mixerdg_id = id;
	mixerdg_mainpath_para.mixerdg_main_left = mixerdg_main_l;
	mixerdg_mainpath_para.mixerdg_main_right = mixerdg_main_r;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MIXERDG_MAIN,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mixerdg_mainpath_para,
		sizeof(struct vbc_mixerdg_mainpath_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MIXERDG_MIX */
int dsp_vbc_mixerdg_mixpath_set(int id,
	int mixerdg_mix_l, int mixerdg_mix_r)
{
	int ret;
	struct vbc_mixerdg_mixpath_para mixerdg_mixpath_para = { };

	mixerdg_mixpath_para.mixerdg_id = id;
	mixerdg_mixpath_para.mixerdg_mix_left = mixerdg_mix_l;
	mixerdg_mixpath_para.mixerdg_mix_right = mixerdg_mix_r;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MIXERDG_MIX,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mixerdg_mixpath_para, sizeof(struct vbc_mixerdg_mixpath_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MIXERDG_STEP */
int dsp_vbc_mixerdg_step_set(int mixerdg_step)
{
	int ret;
	int16_t mixerdg_step_para;

	mixerdg_step_para = mixerdg_step;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MIXERDG_STEP,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mixerdg_step_para, sizeof(int16_t),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MIXER */
int dsp_vbc_mixer_set(int id, int ops_type)
{
	int ret;
	struct vbc_mixer_para mixer = { };

	mixer.mixer_id = id;
	mixer.type = ops_type;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MIXER, -1,
		SND_VBC_DSP_IO_KCTL_SET, &mixer, sizeof(struct vbc_mixer_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_ADC_SOURCE */
int dsp_vbc_mux_adc_source_set(int id, int val)
{
	int ret;
	struct vbc_mux_adc_source mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_ADC_SOURCE,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_adc_source),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_DAC_OUT */
int dsp_vbc_mux_dac_out_set(int id, int val)
{
	int ret;
	struct vbc_mux_dac_out mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_DAC_OUT,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_dac_out),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_ADC */
int dsp_vbc_mux_adc_set(int id, int val)
{
	int ret;
	struct vbc_mux_adc_in mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_ADC,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_adc_in),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_FM */
int dsp_vbc_mux_fm_set(int id, int val)
{
	int ret;
	struct vbc_mux_fm mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_FM,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_fm),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_ST */
int dsp_vbc_mux_st_set(int id, int val)
{
	int ret;
	struct vbc_mux_st mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_ST,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_st),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_LOOP_DA0 */
int dsp_vbc_mux_loop_da0_set(int id, int val)
{
	int ret;
	struct vbc_mux_loop_dac0 mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_LOOP_DA0,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_loop_dac0),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_LOOP_DA1 */
int dsp_vbc_mux_loop_da1_set(int id, int val)
{
	int ret;
	struct vbc_mux_loop_dac1 mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_LOOP_DA1,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_loop_dac1),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_LOOP_DA0_DA1 */
int dsp_vbc_mux_loop_da0_da1_set(int id, int val)
{
	int ret;
	struct vbc_mux_loop_dac0_dac1 mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_LOOP_DA0_DA1,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_loop_dac0_dac1),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_AUDRCD */
int dsp_vbc_mux_audrcd_set(int id, int val)
{
	int ret;
	struct vbc_mux_audrcd_in mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_AUDRCD,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_audrcd_in),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_TDM_AUDRCD23 */
int dsp_vbc_mux_tdm_audrcd23_set(int id, int val)
{
	int ret;
	struct vbc_mux_tdm_audrcd23 mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_TDM_AUDRCD23,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_tdm_audrcd23),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_AP01_DSP */
int dsp_vbc_mux_ap01_dsp_set(int id, int val)
{
	int ret;
	struct vbc_mux_ap01_dsp mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_AP01_DSP,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_ap01_dsp),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_IIS_TX */
int dsp_vbc_mux_iis_tx_set(int id, int val)
{
	int ret;
	struct vbc_mux_iis_tx mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_IIS_TX,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_iis_tx),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_MUX_IIS_RX */
int dsp_vbc_mux_iis_rx_set(int id, int val)
{
	int ret;
	struct vbc_mux_iis_rx mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MUX_IIS_RX,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_iis_rx),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_IIS_PORT_DO */
int dsp_vbc_mux_iis_port_do_set(int id, int val)
{
	int ret;
	struct vbc_mux_adc_source mux_para = { };

	mux_para.id = id;
	mux_para.val = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_IIS_PORT_DO,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&mux_para, sizeof(struct vbc_mux_iis_port_do),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_VOLUME */
int dsp_vbc_set_volume(int volume)
{
	int value;
	int ret;

	value = volume;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_VOLUME,
		value, SND_VBC_DSP_IO_KCTL_SET,
		&value, sizeof(value), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_ADDER */
int dsp_vbc_adder_set(int id, int adder_mode_l, int adder_mode_r)
{
	int ret;
	struct vbc_adder_para adder_para = { };

	adder_para.adder_id = id;
	adder_para.adder_mode_l = adder_mode_l;
	adder_para.adder_mode_r = adder_mode_r;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_ADDER,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&adder_para, sizeof(struct vbc_adder_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_LOOPBACK_TYPE */
int dsp_vbc_loopback_set(struct vbc_loopback_para *loopback)
{
	int ret;

	sp_asoc_pr_dbg("%s loopback_type = %d,loopback\n",
		__func__, loopback->loopback_type);
	sp_asoc_pr_dbg("voice_fmt =%d, loopback.amr_rate =%d\n",
		loopback->voice_fmt, loopback->amr_rate);
	/* send audio cmd */
	sp_asoc_pr_dbg("cmd=%d, parameter0=%d\n", SND_VBC_DSP_IO_KCTL_SET,
		SND_KCTL_TYPE_LOOPBACK_TYPE);
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_LOOPBACK_TYPE,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		loopback, sizeof(struct vbc_loopback_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_DATAPATH */
int dsp_vbc_dp_en_set(int id, u16 enable)
{
	int ret;
	struct vbc_dp_en_para dp_en = { };

	dp_en.id = id;
	dp_en.enable = enable;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_DATAPATH,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&dp_en, sizeof(struct vbc_dp_en_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_CALL_MUTE */
int dsp_call_mute_set(int id, u16 mute)
{
	int ret;
	struct call_mute_para mute_p = { };

	mute_p.id = id;
	mute_p.mute = mute;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_CALL_MUTE,
		-1, SND_VBC_DSP_IO_KCTL_SET, &mute_p,
		sizeof(struct call_mute_para), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_IIS_TX_WIDTH_SEL */
int dsp_vbc_iis_tx_width_set(int id, u32 width)
{
	int ret;
	struct vbc_iis_tx_wd_para iis_tx_width = { };

	iis_tx_width.id = id;
	iis_tx_width.value = width;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_IIS_TX_WIDTH_SEL,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&iis_tx_width, sizeof(struct vbc_iis_tx_wd_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_IIS_TX_LRMOD_SEL */
int dsp_vbc_iis_tx_lr_mod_set(int id, u32 lr_mod)
{
	int ret;
	struct vbc_iis_tx_lr_mod_para iis_tx_lr_mod = { };

	iis_tx_lr_mod.id = id;
	iis_tx_lr_mod.value = lr_mod;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_IIS_TX_LRMOD_SEL,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&iis_tx_lr_mod, sizeof(struct vbc_iis_tx_lr_mod_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_IIS_RX_WIDTH_SEL */
int dsp_vbc_iis_rx_width_set(int id, u32 width)
{
	int ret;
	struct vbc_iis_rx_wd_para iis_rx_width = { };

	iis_rx_width.id = id;
	iis_rx_width.value = width;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_IIS_RX_WIDTH_SEL,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&iis_rx_width, sizeof(struct vbc_iis_rx_wd_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_IIS_RX_LRMOD_SEL */
int dsp_vbc_iis_rx_lr_mod_set(int id, u32 lr_mod)
{
	int ret;
	struct vbc_iis_rx_lr_mod_para iis_rx_lr_mod = { };

	iis_rx_lr_mod.id = id;
	iis_rx_lr_mod.value = lr_mod;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_IIS_RX_LRMOD_SEL,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&iis_rx_lr_mod, sizeof(struct vbc_iis_rx_lr_mod_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_EXT_INNER_IIS_MST_SEL */
int dsp_vbc_mst_sel_type_set(int id, u32 type)
{
	int ret;
	struct vbc_iis_mst_sel_para iis_mst_sel_type = { };

	iis_mst_sel_type.id = id;
	iis_mst_sel_type.mst_type = type;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_EXT_INNER_IIS_MST_SEL,
			   -1, SND_VBC_DSP_IO_KCTL_SET, &iis_mst_sel_type,
			   sizeof(iis_mst_sel_type), AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("mst_sel_type_set failed %d\n", ret);

	return ret;
}

/* SND_KCTL_TYPE_VBC_IIS_MASTER_START */
int dsp_vbc_iis_master_start(u32 enable)
{
	int ret;
	struct vbc_iis_master_para iis_master = { };

	iis_master.enable = enable;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_VBC_IIS_MASTER_START,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&iis_master, sizeof(struct vbc_iis_master_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

void dsp_vbc_iis_master_width_set(u32 iis_width)
{
	int ret;
	u32 iis_mst_width = iis_width;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL,
			   SND_KCTL_TYPE_VBC_IIS_MASTER_WIDTH_SET,
			   -1, SND_VBC_DSP_IO_KCTL_SET,
			   &iis_mst_width, sizeof(iis_mst_width),
			   AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_warn("Failed to set iis_mst_width, ret %d\n", ret);
}

/* SND_KCTL_TYPE_MAIN_MIC_PATH_FROM */
int dsp_vbc_mainmic_path_set(int type, int val)
{
	int ret;
	struct mainmic_from_para para = { };

	para.type = type;
	para.main_mic_from = val;
	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_MAIN_MIC_PATH_FROM,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&para, sizeof(struct mainmic_from_para),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/* SND_KCTL_TYPE_IVSENCE_FUNC */
int dsp_ivsence_func(int enable, int iv_adc_id)
{
	int ret;
	struct ivsense_smartpa_t ivs_smtpa = { };

	ivs_smtpa.enable = enable;
	ivs_smtpa.iv_adc_id = iv_adc_id;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL, SND_KCTL_TYPE_IVSENCE_FUNC,
		-1, SND_VBC_DSP_IO_KCTL_SET,
		&ivs_smtpa, sizeof(struct ivsense_smartpa_t),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);

	return 0;
}

/*********************************************************
 * cmd for SND_VBC_DSP_FUNC_STARTUP
 *********************************************************/
int vbc_dsp_func_startup(int scene_id, int stream,
	struct sprd_vbc_stream_startup_shutdown *startup_info)
{
	int ret;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL, scene_id, stream,
		SND_VBC_DSP_FUNC_STARTUP,
		startup_info, sizeof(struct sprd_vbc_stream_startup_shutdown),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		return -EIO;

	return 0;
}

/*********************************************************
 * cmd for SND_VBC_DSP_FUNC_SHUTDOWN
 *********************************************************/
int vbc_dsp_func_shutdown(int scene_id, int stream,
	struct sprd_vbc_stream_startup_shutdown *shutdown_info)
{
	int ret;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL, scene_id, stream,
		SND_VBC_DSP_FUNC_SHUTDOWN, shutdown_info,
		sizeof(struct sprd_vbc_stream_startup_shutdown),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		return -EIO;

	return 0;
}

/*********************************************************
 * cmd for SND_VBC_DSP_FUNC_HW_PARAMS
 *********************************************************/
int vbc_dsp_func_hwparam(int scene_id, int stream,
	struct sprd_vbc_stream_hw_paras *hw_data)
{
	int ret;

	ret = aud_send_cmd(AMSG_CH_VBC_CTL, scene_id, stream,
		SND_VBC_DSP_FUNC_HW_PARAMS, hw_data,
		sizeof(struct sprd_vbc_stream_hw_paras),
		AUDIO_SIPC_WAIT_FOREVER);
	if (ret < 0)
		return -EIO;

	return 0;
}

/*********************************************************
 * cmd for SND_VBC_DSP_FUNC_HW_TRIGGER
 *********************************************************/
int vbc_dsp_func_trigger(int id, int stream, int up_down)
{
	int ret;
	/* send audio cmd */
	ret = aud_send_cmd_no_wait(AMSG_CH_VBC_CTL,
		SND_VBC_DSP_FUNC_HW_TRIGGER, id, stream,
		up_down, 0);
	if (ret < 0)
		return -EIO;

	return 0;
}

/*********************************************************
 * others
 *********************************************************/

int dsp_fm_mute_by_set_dg(void)
{
	int dg_id = VBC_DG_FM;
	int dg_l = 127;
	int dg_r = 127;
	int ret;

	/* send audio cmd no receive*/
	ret = aud_send_use_noreplychan(
			SND_VBC_DSP_IO_KCTL_SET,
			SND_KCTL_TYPE_DG, dg_id, dg_l, dg_r);
	if (ret < 0) {
		pr_err("%s, Failed to set, ret: %d\n", __func__, ret);
		return -1;
	}
	pr_debug("%s\n", __func__);

	return 0;
}

/***********************************************************
 * audio digital control
 ***********************************************************/
int aud_dig_iis_master(struct snd_soc_card *card,
	int setting)
{
	unsigned long virt_out;
	struct snd_ctl_elem_id id = {
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	};
	struct snd_kcontrol *kctrl;
	struct snd_ctl_elem_value ucontrol = {
		.value.integer.value[0] = !!(setting / 2),
	};

	sp_asoc_pr_dbg("%s setting: %d\n", __func__, setting);
	virt_out = sprd_get_codec_dp_base();
	if (virt_out == 0) {
		pr_err("%s audio digital iis master failed set\n", __func__);
		return -1;
	}

	if (setting == 0 || setting == 2) /* IIS0 */
		strcpy(id.name,
		       "Virt VBC Master Clock Mixer IIS0 Switch");
	else if (setting == 1 || setting == 3) /* Loop */
		strcpy(id.name,
		       "Virt VBC Master Clock Mixer Loop Switch");
	kctrl = snd_ctl_find_id(card->snd_card, &id);
	if (!kctrl) {
		pr_err("%s can't find kctrl '%s'\n", __func__, id.name);
		return -1;
	}

	return sprd_codec_virt_mclk_mixer_put(kctrl, &ucontrol);
}

int pm_shutdown(void)
{
	struct sprd_vbc_stream_startup_shutdown shutdown_info;

	memset(&shutdown_info, 0,
		sizeof(struct sprd_vbc_stream_startup_shutdown));
	shutdown_info.stream_info.id = VBC_DAI_ID_NORMAL_AP01;
	shutdown_info.stream_info.stream = SNDRV_PCM_STREAM_PLAYBACK;
	shutdown_info.startup_para.dac_id = VBC_DA0;

	return aud_send_cmd(AMSG_CH_VBC_CTL, VBC_DAI_ID_NORMAL_AP01,
		SNDRV_PCM_STREAM_PLAYBACK, SND_VBC_DSP_FUNC_SHUTDOWN,
		&shutdown_info,
		sizeof(struct sprd_vbc_stream_startup_shutdown),
		AUDIO_SIPC_WAIT_FOREVER);
}

/* vbc driver function end */
