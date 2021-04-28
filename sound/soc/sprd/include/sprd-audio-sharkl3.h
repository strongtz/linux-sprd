/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifndef __SPRD_AUDIO_SHARKL3_H
#define __SPRD_AUDIO_SHARKL3_H

#ifndef __SPRD_AUDIO_H
#error  "Don't include this file directly, include sprd-audio.h"
#endif

#include <linux/io.h>


#define CODEC_DP_BASE		0x1000
#define VBC_BASE		0x1000
#define CODEC_AP_BASE		0x2000
#define CODEC_AP_OFFSET		0

#define CODEC_DP_BASE_DEFAULT		0x40000000
#define CODEC_DP_SIZE_DEFAULT		SZ_8K

#define VBC_BASE_DEFAULT			0x40020000
#define VBC_BASE_SIZE_DEFAULT		(SZ_4K + SZ_8K)

#define VBC_PHY_BASE		0

enum ag_iis {
	AG_IIS0,
	AG_IIS1,
	AG_IIS2,
	AG_IIS_MAX
};

enum {
	VBC_NO_CHANGE,
	VBC_TO_AP_CTRL,
	VBC_TO_WTLCP_CTRL,
	VBC_TO_PUBCP_CTRL,
};

enum {
	VBC_INT_NO_CHANGE,
	VBC_INT_TO_AP_CTRL,
	VBC_INT_TO_WTLCP_CTRL,
	VBC_INT_TO_PUBCP_CTRL,
};

enum {
	VBC_DMA_NO_CHANGE,
	VBC_DMA_TO_AP_AON_CTRL,
	VBC_DMA_TO_WTLCP_TGDSP_CTRL,
	VBC_DMA_TO_WTLCP_LDSP_CTRL,
};

enum {
	ADU_DIGITAL_INT_NO_CHANGE,
	ADU_DIGITAL_INT_TO_AP_CTRL,
	ADU_DIGITAL_INT_TO_PUBCP_CTRL,
	ADU_DIGITAL_INT_TO_WTLCP_CTRL,
};
/* note: I2S_MAGIC_ID: in i2s driver
 * VBC_DAI_NORMAL: play & fm capture
 * VBC_DAI_AD23: normal capture
 * VBC_DAI_DEEP_BUF: play, capture not support
 */
enum {
	VBC_DAI_NORMAL = 0,
	VBC_DAI_AD23 = 1,
	VBC_DAI_DEEP_BUF = 2,
	VBC_DAI_MAX
};

/* for CP2 */
#define VBC_CP2_PHY_BASE	(0x02020000)
#define CP2_PHYS_VBDA0		(VBC_CP2_PHY_BASE + 0x0000)
#define CP2_PHYS_VBDA1		(VBC_CP2_PHY_BASE + 0x0004)

#define CLASS_G_LDO_ID			"vddclsg"

#define VBC_USE_AON_DMA_CASE_DAC01 BIT(0)
#define VBC_USE_AON_DMA_CASE_DAC23 BIT(1)
#define VBC_USE_AON_DMA_CASE_ADC01 BIT(2)
#define VBC_USE_AON_DMA_CASE_ADC23 BIT(3)

/* dma device id (uid) */
/* AON vbc dma device id , spec init index form 0 */
#define DMA_REQ_ARM7_VBC_AD0            (2 + 1)
#define DMA_REQ_ARM7_VBC_AD1            (3 + 1)
#define DMA_REQ_ARM7_VBC_AD2            (4 + 1)
#define DMA_REQ_ARM7_VBC_AD3            (5 + 1)

#define DMA_REQ_ARM7_VBC_DA0            (6 + 1)
#define DMA_REQ_ARM7_VBC_DA1            (7 + 1)
#define DMA_REQ_ARM7_VBC_DA2            (8 + 1)
#define DMA_REQ_ARM7_VBC_DA3            (9 + 1)

/* AP vbc and iis dma device id*/
#define DMA_REQ_IIS0_RX                 (2 + 1)
#define DMA_REQ_IIS0_TX                 (3 + 1)
#define DMA_REQ_IIS1_RX                 (4 + 1)
#define DMA_REQ_IIS1_TX                 (5 + 1)
#define DMA_REQ_IIS2_RX                 (6 + 1)
#define DMA_REQ_IIS2_TX                 (7 + 1)
#define DMA_REQ_IIS3_RX                 (8 + 1)
#define DMA_REQ_IIS3_TX                 (9 + 1)
#define DMA_REQ_AP_VBDA0                   (28 + 1)
#define DMA_REQ_AP_VBDA1                   (29 + 1)
#define DMA_REQ_AP_VBAD0                   (30 + 1)
#define DMA_REQ_AP_VBAD1                   (31 + 1)
#define DMA_REQ_AP_VBAD2                   (32 + 1)
#define DMA_REQ_AP_VBAD3                   (33 + 1)
#define DMA_REQ_AP_VBDA2                   (44 + 1)
#define DMA_REQ_AP_VBDA3                   (45 + 1)

/*aon registers offset */
#define REG_AON_APB_APB_EB0                     (0x0000)
#define REG_AON_APB_APB_RST0                    (0x0008)
#define REG_AON_APB_SINDRV_CTRL                 (0x0018)

/* REG_AON_APB_VBC_CTRL */
#define REG_AON_APB_VBC_CTRL                    (0x0020)


/*AP_APB registers offset */
#define REG_AP_APB_APB_EB                   (0x0000)
#define REG_AP_APB_APB_RST                  (0x0004)



/*PMU APB register offset*/
#define REG_PMU_APB_XTLBUF1_REL_CFG                           (0x0090)
#define REG_PMU_APB_SLEEP_XTLON_CTRL                          (0x0168)
#define REG_PMU_APB_LIGHT_SLEEP_ENABLE                        (0x0230)



/* REG_AON_APB_APB_EB0 */
#define BIT_AON_APB_VBC_EB                            BIT(19)
#define BIT_AON_APB_AUD_EB                            BIT(18)
#define BIT_AON_APB_AUDIF_EB                          BIT(17)



/* REG_PMU_APB_LIGHT_SLEEP_ENABLE */
#define BIT_PMU_APB_DMA_CHNALL_LSLP_ENA                         BIT(16)


/* REG_AON_APB_APB_RST0 */
#define BIT_AON_APB_VBC_SOFT_RST                      BIT(20)

/* REG_AON_APB_SINDRV_CTRL */
#define BIT_AON_APB_SINDRV_ENA                        BIT(0)


/* REG_AON_APB_APB_RST0 */
#define BIT_AON_APB_AUD_SOFT_RST                      BIT(19)
#define BIT_AON_APB_AUDIF_SOFT_RST                    BIT(18)


/* REG_AP_APB_APB_EB */
#define BIT_AP_APB_IIS0_EB                      BIT(1)

/* REG_AP_APB_APB_RST */
#define BIT_AP_APB_IIS0_SOFT_RST                BIT(1)

/* REG_PMU_APB_XTLBUF1_REL_CFG */
#define BIT_PMU_APB_XTLBUF1_WTLCP_SEL                           BIT(1)
#define BIT_PMU_APB_XTLBUF1_AP_SEL                              BIT(0)


/* REG_PMU_APB_SLEEP_XTLON_CTRL */
#define BIT_PMU_APB_AP_SLEEP_XTL_ON                             BIT(0)



/* REG_AON_APB_VBC_CTRL */
#define BIT_AON_APB_AUDIF_CKG_AUTO_EN                 BIT(20)
#define BIT_AON_APB_AUD_INT_SYS_SEL(x)                (((x) & 0x3) << 18)
#define BIT_AON_APB_VBC_DA23_INT_SYS_SEL(x)           (((x) & 0x3) << 16)
#define BIT_AON_APB_VBC_AD01_INT_SYS_SEL(x)           (((x) & 0x3) << 12)
#define BIT_AON_APB_VBC_DA01_INT_SYS_SEL(x)           (((x) & 0x3) << 10)
#define BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(x)           (((x) & 0x3) << 6)
#define BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(x)           (((x) & 0x3) << 4)
#define BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(x)           (((x) & 0x3) << 2)
#define BIT_AON_APB_VBC_DMA_WTLCP_ARM_SEL             BIT(1)








static inline int _arch_audio_vbc_reset(void)
{
	aon_apb_gpr_null_check();
	aon_apb_reg_set(REG_AON_APB_APB_RST0, BIT_AON_APB_VBC_SOFT_RST);
	udelay(10);
	aon_apb_reg_clr(REG_AON_APB_APB_RST0, BIT_AON_APB_VBC_SOFT_RST);
	return 0;
}

static u32 protect_cnt;
static void *g_clk_status_addr;

#define WAIT_XTLBUF_STABLE_MAX_COUNT 20
/*
 * according asic's suggestion XTLBUF clock stable
 * need at least 200 us
 */
#define WAIT_XTLBUF_DELAY_TIME 200

static inline void _xtlbuf1_eb_set(void)
{
	int ret;
	u32 val;
	/* use while 1, because if update reg failed kernel will crash,
	 * need not add timeout
	 */
	while (1) {
		ret =  pmu_apb_reg_update(REG_PMU_APB_XTLBUF1_REL_CFG,
			BIT_PMU_APB_XTLBUF1_WTLCP_SEL,
			BIT_PMU_APB_XTLBUF1_WTLCP_SEL);
		if (ret == 0) {
			ret = pmu_apb_reg_read(
				REG_PMU_APB_XTLBUF1_REL_CFG, &val);
			if (ret == 0 && val & BIT_PMU_APB_XTLBUF1_WTLCP_SEL) {
				pr_debug("xtlbuf1 eb wtlcp success\n");
				break;
			}
			pr_warn("%s line[%d] ret=%d, val=%#x reg read failed\n",
				__func__, __LINE__, ret, val);
		} else
			pr_err("%s pmu_reg_set to wtlcp failed\n",
				__func__);
	}
	val = 0;
	while (1) {
		ret =  pmu_apb_reg_update(REG_PMU_APB_XTLBUF1_REL_CFG,
			BIT_PMU_APB_XTLBUF1_AP_SEL,
			BIT_PMU_APB_XTLBUF1_AP_SEL);
		if (ret == 0) {
			ret = pmu_apb_reg_read(
				REG_PMU_APB_XTLBUF1_REL_CFG, &val);
			if (ret == 0 &&
				val & BIT_PMU_APB_XTLBUF1_AP_SEL) {
				pr_debug("xtlbuf1 eb ap success\n");
				break;
			}
			pr_warn("%s line[%d] ret=%d, val=%#x reg read failed\n",
				__func__, __LINE__, ret, val);
		} else
			pr_err("%s line[%d] pmu_reg_set to wtlcp failed\n",
				__func__, __LINE__);
	}
	/* wait for xbuf stable */
	val = 0;
	while (1) {
		protect_cnt++;
		udelay(WAIT_XTLBUF_DELAY_TIME);
		val = readl_relaxed(g_clk_status_addr);
		pr_debug("%s %d protect_cnt=%d, val=%#x\n",
			__func__, __LINE__, protect_cnt, val);
		if (val & BIT(14)) {
			pr_debug("xbuf stable protect_cnt=%d success\n",
				 protect_cnt);
			protect_cnt = 0;
			break;
		}
		if (protect_cnt > WAIT_XTLBUF_STABLE_MAX_COUNT) {
			pr_err("protect_cnt %d too many time break\n",
			      protect_cnt);
			protect_cnt = 0;
			break;
		}
	}
	pr_debug("%s %d setted\n", __func__, __LINE__);
}

static inline void _vbc_eb_set(void)
{
	int ret;
	u32 val;

	while (1) {
		ret = aud_aon_bit_raw_set(REG_AON_APB_APB_EB0,
			BIT_AON_APB_VBC_EB);
		if (ret == 0) {
			ret = aon_apb_reg_read(REG_AON_APB_APB_EB0,
					       &val);
			if (ret == 0 && val & BIT_AON_APB_VBC_EB) {
				pr_debug("vbc_eb success\n");
				break;
			}
			pr_warn("%s line[%d] ret=%d, val=%#x reg read failed\n",
					__func__, __LINE__, ret, val);
		} else
			pr_warn("%s line[%d] ret=%d bit raw set failed\n",
				__func__, __LINE__, ret);
	}
}

static inline void _xtlbuf1_eb_clr(void)
{
	int ret;

	ret = pmu_apb_reg_update(REG_PMU_APB_XTLBUF1_REL_CFG,
		BIT_PMU_APB_XTLBUF1_AP_SEL, 0);
	if (ret)
		pr_err("%s pmu_reg_clr to ap failed\n",
			__func__);
	ret = pmu_apb_reg_update(REG_PMU_APB_XTLBUF1_REL_CFG,
		BIT_PMU_APB_XTLBUF1_WTLCP_SEL, 0);
	if (ret)
		pr_err("%s pmu_reg_clr to wtlcp failed\n",
			__func__);
}

static inline void _vbc_eb_clear(void)
{
	aud_aon_bit_raw_clear(REG_AON_APB_APB_EB0, BIT_AON_APB_VBC_EB);
}

static inline int arch_audio_vbc_int_switch(int master)
{
	int ret = 0;
	int val;
	int mask = BIT_AON_APB_VBC_DA01_INT_SYS_SEL(3) |
	    BIT_AON_APB_VBC_DA23_INT_SYS_SEL(3) |
	    BIT_AON_APB_VBC_AD01_INT_SYS_SEL(3);

	aon_apb_gpr_null_check();

	switch (master) {
	case VBC_INT_TO_AP_CTRL:
		val = BIT_AON_APB_VBC_DA01_INT_SYS_SEL(0) |
		    BIT_AON_APB_VBC_DA23_INT_SYS_SEL(0) |
		    BIT_AON_APB_VBC_AD01_INT_SYS_SEL(0);
		aon_apb_reg_update(REG_AON_APB_VBC_CTRL, mask, val);
		break;
	case VBC_INT_TO_WTLCP_CTRL:
		val = BIT_AON_APB_VBC_DA01_INT_SYS_SEL(1) |
		    BIT_AON_APB_VBC_DA23_INT_SYS_SEL(1) |
		    BIT_AON_APB_VBC_AD01_INT_SYS_SEL(1);
		aon_apb_reg_update(REG_AON_APB_VBC_CTRL, mask, val);
		break;
	case VBC_INT_TO_PUBCP_CTRL:
		val = BIT_AON_APB_VBC_DA01_INT_SYS_SEL(2) |
		    BIT_AON_APB_VBC_DA23_INT_SYS_SEL(2) |
		    BIT_AON_APB_VBC_AD01_INT_SYS_SEL(2);
		aon_apb_reg_update(REG_AON_APB_VBC_CTRL, mask, val);

		break;
	case VBC_INT_NO_CHANGE:
		break;
	default:
		pr_err("ERR: %s, Invalid master(%d)!\n", __func__, master);
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_audio_vbc_dma_switch(int master,
					    unsigned int vbc_use_dma_type[],
					    int vbc_idx_max)
{
	int val = 0, ret = 0;
	int mask = BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(3) |
	    BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(3) |
	    BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(3);
	/* vbc dma req select for wtlcp  0:req tgdsp, 1: req ldsp
	 * vbc dma req do not use pubcp, so ignore pubcp dam
	 */

	aon_apb_gpr_null_check();

	switch (master) {
		/* da01 da23 requset ap dma, ad01, ad23 request aon dma */
	case VBC_DMA_TO_AP_AON_CTRL:
		/* Capture will only use AP DMA for the limitation
		 * of IRAM size.
		 */
		val |=
		    (vbc_use_dma_type[0] ==
		     0) ? BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(2) :
		    BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(0);
		val |=
		    (vbc_use_dma_type[1] ==
		     0) ? BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(2) :
		    BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(0);
		val |=
		    (vbc_use_dma_type[2] ==
		     0) ? BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(2) :
		    BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(0);
		aon_apb_reg_update(REG_AON_APB_VBC_CTRL, mask, val);
		break;
	case VBC_DMA_TO_WTLCP_TGDSP_CTRL:
		val = BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(1) |
		    BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(1) |
		    BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(1);
		aon_apb_reg_update(REG_AON_APB_VBC_CTRL,
				   mask | BIT_AON_APB_VBC_DMA_WTLCP_ARM_SEL,
				   val);
		break;
	case VBC_DMA_TO_WTLCP_LDSP_CTRL:
		val = BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(1) |
		    BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(1) |
		    BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(1) |
		    BIT_AON_APB_VBC_DMA_WTLCP_ARM_SEL;
		aon_apb_reg_update(REG_AON_APB_VBC_CTRL,
				   mask | BIT_AON_APB_VBC_DMA_WTLCP_ARM_SEL,
				   val);
		break;
	case VBC_DMA_NO_CHANGE:
		break;
	default:
		pr_err("ERR: %s, Invalid master(%d)!\n", __func__, master);
		ret = -ENODEV;
		break;
	}
	pr_info("%s master =%d val =%#x\n", __func__, master, val);

	return ret;
}

static inline int _arch_audio_vbc_switch(int master,
					unsigned int vbc_use_dma_type[],
					int vbc_idx_max)
{
	aon_apb_gpr_null_check();

	switch (master) {
	case VBC_TO_AP_CTRL:
		/* da dma request to aon, ad dma request to ap */
		arch_audio_vbc_dma_switch(VBC_DMA_TO_AP_AON_CTRL,
			vbc_use_dma_type, vbc_idx_max);
		break;
	case VBC_TO_WTLCP_CTRL:
		/* vbc interrupt to tgdsp */
		arch_audio_vbc_int_switch(VBC_INT_TO_WTLCP_CTRL);
		break;
	case VBC_TO_PUBCP_CTRL:
		break;
	case VBC_NO_CHANGE:
		return VBC_NO_CHANGE;
	default:
		pr_err("ERR: %s, Invalid master(%d)!\n",
		       __func__, master);
		return -ENODEV;
	}

	return 0;
}

/* ----------------------------------------------- */
/* Codec digital part in soc setting */
static inline int arch_audio_codec_audif_enable(int auto_clk)
{
	aon_apb_gpr_null_check();
	if (auto_clk) {
		aon_apb_reg_clr(REG_AON_APB_APB_EB0, BIT_AON_APB_AUDIF_EB);
		aon_apb_reg_set(REG_AON_APB_VBC_CTRL,
				BIT_AON_APB_AUDIF_CKG_AUTO_EN);
	} else {
		aon_apb_reg_set(REG_AON_APB_APB_EB0, BIT_AON_APB_AUDIF_EB);
		aon_apb_reg_clr(REG_AON_APB_VBC_CTRL,
				BIT_AON_APB_AUDIF_CKG_AUTO_EN);
	}

	return 0;
}

static inline int arch_audio_codec_audif_disable(void)
{
	aon_apb_gpr_null_check();
	aon_apb_reg_clr(REG_AON_APB_APB_EB0, BIT_AON_APB_AUDIF_EB);
	aon_apb_reg_clr(REG_AON_APB_VBC_CTRL, BIT_AON_APB_AUDIF_CKG_AUTO_EN);

	return 0;
}

static inline int arch_audio_codec_digital_reg_enable(void)
{
	int ret;

	aon_apb_gpr_null_check();
	ret = aon_apb_reg_set(REG_AON_APB_APB_EB0, BIT_AON_APB_AUD_EB);
	if (ret >= 0)
		arch_audio_codec_audif_enable(0);

	return ret;
}

static inline int arch_audio_codec_digital_reg_disable(void)
{
	aon_apb_gpr_null_check();
	arch_audio_codec_audif_disable();
	aon_apb_reg_clr(REG_AON_APB_APB_EB0, BIT_AON_APB_AUD_EB);

	return 0;
}

static inline int arch_audio_codec_digital_enable(void)
{
	int ret;

	ret = anlg_phy_g_null_check();
	if (ret < 0) {
		pr_err("%s failed\n", __func__);
		return ret;
	}
	/* internal digital 26M enable */
	ret = anlg_phy_g_reg_set(REG_AON_APB_SINDRV_CTRL,
			BIT_AON_APB_SINDRV_ENA);
	if (ret != 0)
		pr_err("%s set failed", __func__);

	return ret;
}

static inline int arch_audio_codec_digital_disable(void)
{
	int ret;

	ret = anlg_phy_g_null_check();
	if (ret < 0) {
		pr_err("%s failed\n", __func__);
		return ret;
	}
	/* internal digital 26M disable */
	ret = anlg_phy_g_reg_clr(REG_AON_APB_SINDRV_CTRL,
			BIT_AON_APB_SINDRV_ENA);
	if (ret != 0)
		pr_err("%s set failed", __func__);

	return ret;
}

static inline int arch_audio_codec_switch(int master)
{
	int ret;
	unsigned int msk, val;

	aon_apb_gpr_null_check();

	msk = BIT_AON_APB_AUD_INT_SYS_SEL(3);
	switch (master) {
	case ADU_DIGITAL_INT_TO_AP_CTRL:
		ret = aon_apb_reg_update(REG_AON_APB_VBC_CTRL, msk,
					 BIT_AON_APB_AUD_INT_SYS_SEL(0));
		break;
	case ADU_DIGITAL_INT_TO_WTLCP_CTRL:
		ret = aon_apb_reg_update(REG_AON_APB_VBC_CTRL, msk,
					 BIT_AON_APB_AUD_INT_SYS_SEL(1));
		break;
	case ADU_DIGITAL_INT_TO_PUBCP_CTRL:
		ret = aon_apb_reg_update(REG_AON_APB_VBC_CTRL, msk,
					 BIT_AON_APB_AUD_INT_SYS_SEL(2));
		break;
	case ADU_DIGITAL_INT_NO_CHANGE:
		ret = aon_apb_reg_read(REG_AON_APB_VBC_CTRL, &val);
		if (ret < 0)
			return ret;
		val &= msk;
		switch (val) {
		case BIT_AON_APB_AUD_INT_SYS_SEL(0):
			ret = ADU_DIGITAL_INT_TO_AP_CTRL;
			break;
		case BIT_AON_APB_AUD_INT_SYS_SEL(1):
			ret = ADU_DIGITAL_INT_TO_WTLCP_CTRL;
			break;
		case BIT_AON_APB_AUD_INT_SYS_SEL(2):
			ret = ADU_DIGITAL_INT_TO_PUBCP_CTRL;
			break;
		default:
			pr_warn("%s audio digital int unknown val = %x\n",
				__func__, val);
			ret = ADU_DIGITAL_INT_NO_CHANGE;
			break;
		}
		break;
	default:
		pr_err("ERR: %s, Invalid master(%d)!\n", __func__, master);
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_audio_codec_switch2ap(void)
{
	return arch_audio_codec_switch(ADU_DIGITAL_INT_TO_AP_CTRL);
}

static inline void arch_audio_codec_digital_reset(void)
{
	aon_apb_gpr_null_check();
	aon_apb_reg_set(REG_AON_APB_APB_RST0, BIT_AON_APB_AUD_SOFT_RST);
	aon_apb_reg_set(REG_AON_APB_APB_RST0, BIT_AON_APB_AUDIF_SOFT_RST);
	udelay(10);
	aon_apb_reg_clr(REG_AON_APB_APB_RST0, BIT_AON_APB_AUD_SOFT_RST);
	aon_apb_reg_clr(REG_AON_APB_APB_RST0, BIT_AON_APB_AUDIF_SOFT_RST);
}

static inline void arch_audio_sleep_xtl_enable(void)
{
	pmu_apb_gpr_null_check();
	pmu_apb_reg_set(REG_PMU_APB_SLEEP_XTLON_CTRL,
			BIT_PMU_APB_AP_SLEEP_XTL_ON);
}

static inline void arch_audio_sleep_xtl_disable(void)
{
	pmu_apb_gpr_null_check();
	pmu_apb_reg_clr(REG_PMU_APB_SLEEP_XTLON_CTRL,
			BIT_PMU_APB_AP_SLEEP_XTL_ON);
}

/* vbc r1p0v3 and r2p0 have no such control. */
static inline int arch_audio_iis_to_audio_top_enable(int iis, int en)
{
	return 0;
}

/* i2s setting */
static inline const char *arch_audio_i2s_clk_name(int id)
{
	switch (id) {
	case 0:
		return "clk_iis0";
	case 1:
		return "clk_iis1";
	case 2:
		return "clk_iis2";
	case 3:
		return "clk_iis3";
	default:
		break;
	}
	return NULL;
}

static inline int arch_audio_i2s_enable(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ap_apb_reg_set(REG_AP_APB_APB_EB, BIT_AP_APB_IIS0_EB);
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_audio_i2s_disable(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ap_apb_reg_clr(REG_AP_APB_APB_EB, BIT_AP_APB_IIS0_EB);
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_audio_i2s_tx_dma_info(int id)
{
	int ret = 0;


	switch (id) {
	case 0:
		ret = DMA_REQ_IIS0_TX;
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_audio_i2s_rx_dma_info(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ret = DMA_REQ_IIS0_RX;
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_audio_i2s_reset(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
	case 1:
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
	case 2:
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
	case 3:
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_dma_chanall_lslp_ena(bool enable)
{
	pmu_apb_gpr_null_check();
	if (enable == true)
		pmu_apb_reg_clr(REG_PMU_APB_LIGHT_SLEEP_ENABLE,
			BIT_PMU_APB_DMA_CHNALL_LSLP_ENA);
	else
		pmu_apb_reg_set(REG_PMU_APB_LIGHT_SLEEP_ENABLE,
			BIT_PMU_APB_DMA_CHNALL_LSLP_ENA);

	return 0;
}
#endif /* __SPRD_AUDIO_SHARKL3_H */
