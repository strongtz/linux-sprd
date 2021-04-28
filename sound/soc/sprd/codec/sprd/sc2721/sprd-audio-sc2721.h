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

#ifndef __SPRD_AUDIO_SC2721_H
#define __SPRD_AUDIO_SC2721_H

//#include <linux/mfd/sprd/pmic_glb_reg.h>
//#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#define CHIP_ID_2720 0x2720
#define CHIP_ID_2721 0x2721
#define MASK_ANA_VER	(0xFFF)

#define CTL_BASE_ANA_GLB 0x0C00

/*
 * Register Name          :   ANA_REG_GLB_MODULE_EN0
 * Register Offset        :   0x0008
 * Register Description   :
 */

#define BIT_AUD_EN                              BIT(4)
#define BIT_ANA_AUD_EN                   BIT_AUD_EN

/*
 * Register Name          :   ANA_REG_GLB_ARM_CLK_EN0
 * Register Offset        :   0x000C
 * Register Description   :
 */

#define BIT_CLK_AUD_IF_6P5M_EN                  BIT(1)
#define BIT_CLK_AUD_IF_EN                       BIT(0)
#define BIT_AUDRX_SOFT_RST                      BIT(13)
#define BIT_AUDTX_SOFT_RST                      BIT(12)
#define BIT_AUD_IF_SOFT_RST                     BIT(8)


#define BIT_XTL_EN                              BIT(8)

/*
 * Register Name          :   ANA_REG_GLB_AUDIO_CTRL0
 * Register Offset        :   0x0208
 * Register Description   :
 */

#define BIT_CLK_AUD_IF_6P5M_TX_INV_EN           BIT(1)
#define BIT_CLK_AUD_IF_6P5M_TX_INV_EN           BIT(1)
#define BIT_CLK_AUD_IF_6P5M_RX_INV_EN           BIT(0)

#define ANA_REG_GLB_CHIP_ID_LOW     (CTL_BASE_ANA_GLB + 0x0000)
#define ANA_REG_GLB_CHIP_ID_HIGH    (CTL_BASE_ANA_GLB + 0x0004)
#define ANA_REG_GLB_MODULE_EN0      (CTL_BASE_ANA_GLB + 0x0008)
#define ANA_REG_GLB_ARM_CLK_EN0     (CTL_BASE_ANA_GLB + 0x000C)
#define ANA_REG_GLB_SOFT_RST0       (CTL_BASE_ANA_GLB + 0x0014)

/*
 * Use CONFIG_SND_SOC_SPRD_PIKE2 to clasify 2720
 * because 2720 only used on PIKE2 and no 2720 related
 * macro currently
 */

#ifdef CONFIG_SND_SOC_SPRD_PIKE2
#define ANA_REG_GLB_XTL_WAIT_CTRL0  (CTL_BASE_ANA_GLB + 0x01E8)
#define ANA_REG_GLB_AUDIO_CTRL0     (CTL_BASE_ANA_GLB + 0x0208)
#else
#define ANA_REG_GLB_XTL_WAIT_CTRL0  (CTL_BASE_ANA_GLB + 0x029C)
#define ANA_REG_GLB_AUDIO_CTRL0     (CTL_BASE_ANA_GLB + 0x02BC)
#endif

#define BIT_CLK_AUDIF_EN			BIT_CLK_AUD_IF_EN
#define BIT_ANA_AUDRX_SOFT_RST		BIT_AUDRX_SOFT_RST
#define BIT_ANA_AUDTX_SOFT_RST		BIT_AUDTX_SOFT_RST
#define BIT_ANA_AUD_SOFT_RST		BIT_AUD_IF_SOFT_RST
#define BIT_CLK_AUD_6P5M_TX_INV_EN	BIT_CLK_AUD_IF_6P5M_TX_INV_EN
#define BIT_CLK_AUD_6P5M_RX_INV_EN	BIT_CLK_AUD_IF_6P5M_RX_INV_EN
#define BIT_CLK_AUD_6P5M_EN		    BIT_CLK_AUD_IF_6P5M_EN
#define ANA_REG_GLB_ARM_MODULE_EN	ANA_REG_GLB_MODULE_EN0
#define ANA_REG_GLB_XTL_WAIT_CTRL	ANA_REG_GLB_XTL_WAIT_CTRL0
#define ANA_REG_GLB_ARM_CLK_EN		ANA_REG_GLB_ARM_CLK_EN0


/* pmic registers operating interfaces
 * codec_regmap and codec_ana_reg_offset will be set by
 * sprd-codec.c in its probe func.
 */
static struct regmap *codec_regmap;
static unsigned long codec_reg_offset;

#define CODEC_REG(reg) \
	((reg) + CODEC_AP_OFFSET + codec_reg_offset - CODEC_AP_BASE)

#define REGMAP_OFFSET_INIT_CHECK() do { \
	if (codec_regmap == NULL || codec_reg_offset == -1) { \
		pr_err("ERR: %s codec_regmap or reg offset isn't initialized!\n", \
		       __func__); \
		return -1; \
	} \
} while (0)

#define REGMAP_INIT_CHECK() do { \
	if (codec_regmap == NULL) { \
		pr_err("ERR: %s codec_regmap isn't initialized!\n", __func__); \
		return -1; \
	} \
} while (0)

#define sci_adi_write(reg, val, msk) \
	regmap_update_bits(codec_regmap, (reg), (msk), (val))
#define sci_adi_set(reg, bits) \
	regmap_update_bits(codec_regmap, (reg), (bits), (bits))
#define sci_adi_clr(reg, bits) \
	regmap_update_bits(codec_regmap, (reg), (bits), 0)
#define sci_adi_read(reg, r_val) \
	regmap_read(codec_regmap, (reg), (r_val))

static inline void arch_audio_codec_set_regmap(struct regmap *rgmp)
{
	codec_regmap = rgmp;
}

static inline struct regmap *arch_audio_codec_get_regmap(void)
{
	return codec_regmap;
}

static inline void arch_audio_codec_set_reg_offset(unsigned long offset)
{
	codec_reg_offset = offset;
}

/* codec parts in pmic setting */
static inline int arch_audio_codec_write_mask(int reg, int val, int mask)
{
	int ret = 0;

	REGMAP_OFFSET_INIT_CHECK();

	ret = sci_adi_write(CODEC_REG(reg), val, mask);

	return ret;
}

static inline int arch_audio_codec_write(int reg, int val)
{
	int ret = 0;

	REGMAP_OFFSET_INIT_CHECK();

	ret = sci_adi_write(CODEC_REG(reg), val, 0xFFFF);

	return ret;
}

static inline int arch_audio_codec_read(int reg)
{
	int ret = 0;
	unsigned int val;

	REGMAP_OFFSET_INIT_CHECK();

	ret = sci_adi_read(CODEC_REG(reg), &val);
	if (ret) {
		pr_err("%s: sci_adi_read failed!\n", __func__);
		return ret;
	}

	return val;
}

static inline int arch_audio_codec_analog_reg_enable(void)
{
	int ret = 0;

	REGMAP_INIT_CHECK();
	ret = sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN);

	return ret;
}

static inline int arch_audio_codec_analog_reg_disable(void)
{
	int ret = 0;

	REGMAP_INIT_CHECK();
	ret = sci_adi_write(ANA_REG_GLB_ARM_MODULE_EN, 0, BIT_ANA_AUD_EN);

	return ret;
}

static inline int arch_audio_codec_analog_enable(void)
{
	int ret = 0;

	/* AUDIF , 6.5M */
	int mask = BIT_CLK_AUD_IF_6P5M_EN | BIT_CLK_AUD_IF_EN;

	sci_adi_write(ANA_REG_GLB_ARM_CLK_EN, mask, mask);
	/* peng debug how to set it in sc2721? */
	sci_adi_write(ANA_REG_GLB_AUDIO_CTRL0, BIT_CLK_AUD_IF_6P5M_TX_INV_EN,
			  BIT_CLK_AUD_IF_6P5M_TX_INV_EN);

	/* 26M */
	sci_adi_write(ANA_REG_GLB_XTL_WAIT_CTRL, BIT_XTL_EN, BIT_XTL_EN);

	return ret;
}

static inline int arch_audio_codec_analog_disable(void)
{
	int ret = 0;
	/* AUDIF , 6.5M */
	int mask = BIT_CLK_AUD_IF_6P5M_EN | BIT_CLK_AUD_IF_EN;

	REGMAP_INIT_CHECK();

	sci_adi_clr(ANA_REG_GLB_ARM_CLK_EN, mask);
	sci_adi_clr(ANA_REG_GLB_AUDIO_CTRL0,
		BIT_CLK_AUD_IF_6P5M_TX_INV_EN);

	return ret;
}

static inline int arch_audio_codec_analog_reset(void)
{
	int ret = 0;
	int mask = BIT_AUD_IF_SOFT_RST
		| BIT_AUDTX_SOFT_RST
		| BIT_AUDRX_SOFT_RST;

	REGMAP_INIT_CHECK();
	ret = sci_adi_write(ANA_REG_GLB_SOFT_RST0, mask, mask);
	udelay(10);
	if (ret >= 0)
		ret = sci_adi_write(ANA_REG_GLB_SOFT_RST0, 0, mask);

	return ret;
}

static inline u32 sci_get_ana_chip_id(void)
{
	int ret;
	unsigned int val1, val2;

	ret = sci_adi_read(ANA_REG_GLB_CHIP_ID_HIGH, &val1);
	if (ret) {
		pr_err("%s: sci_adi_read reg#%d failed!\n",
			__func__, ANA_REG_GLB_CHIP_ID_HIGH);
		return 0;
	}
	val1 = val1 << 16;
	ret = sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW, &val2);
	if (ret) {
		pr_err("%s: sci_adi_read reg#%d failed!\n",
			__func__, ANA_REG_GLB_CHIP_ID_LOW);
		return 0;
	}
	val1 |= (val2 & ~MASK_ANA_VER);

	return (u32)val1;
}

static inline int sci_get_ana_chip_ver(void)
{
	int ret;
	unsigned int val;

	ret = sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW, &val);
	if (ret) {
		pr_err("%s: sci_adi_read failed!\n", __func__);
		return ret;
	}

	return val & MASK_ANA_VER;
}

/* --------------------------------- */
/* power setting */
static inline int arch_audio_power_write_mask(int reg, int val, int mask)
{
	int ret = 0;

	ret = arch_audio_codec_write_mask(reg, val, mask);

	return ret;
}

static inline int arch_audio_power_write(int reg, int val)
{
	int ret = 0;

	ret = arch_audio_codec_write(reg, val);

	return ret;
}

static inline int arch_audio_power_read(int reg)
{
	int ret = 0;

	ret = arch_audio_codec_read(reg);

	return ret;
}

#endif
