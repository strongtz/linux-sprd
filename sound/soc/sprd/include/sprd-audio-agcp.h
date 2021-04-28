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

#ifndef __SPRD_AUDIO_AGCP_H_
#define __SPRD_AUDIO_AGCP_H_

#ifndef __SPRD_AUDIO_H
#error  "Don't include this file directly, include sprd-audio.h"
#endif

#include "agdsp_access.h"

#define CODEC_DP_BASE		0x1000
#define CODEC_AP_BASE		0x2000
#define CODEC_AP_OFFSET		0

/* AGCP AHB registers doesn't defined by global header file. So
 * define them here.
 */
#define REG_AGCP_AHB_MODULE_EB0_STS	0x00
#define REG_AGCP_AHB_MODULE_RST0_STS	0x08
#define REG_AGCP_AHB_EXT_ACC_AG_SEL	0x3c

/*--------------------------------------------------
 * Register Name   : REG_AGCP_AHB_MODULE_EB0_STS
 * Register Offset : 0x0000
 * Description     :
 * ---------------------------------------------------
 */
#define BIT_AUDIF_CKG_AUTO_EN	BIT(20)
#define BIT_AUD_EB				BIT(19)

/*--------------------------------------------------
 * Register Name   : REG_AGCP_AHB_MODULE_RST0_STS
 * Register Offset : 0x0008
 * Description     :
 * ---------------------------------------------------
 */
#define BIT_AUD_SOFT_RST		BIT(25)

/*--------------------------------------------------
 * Register Name   : REG_AGCP_AHB_EXT_ACC_AG_SEL
 * Register Offset : 0x003c
 * Description     :
 * ---------------------------------------------------
 */
#define BIT_AG_IIS2_EXT_SEL                      BIT(2)
#define BIT_AG_IIS1_EXT_SEL                      BIT(1)
#define BIT_AG_IIS0_EXT_SEL                      BIT(0)

/* ----------------------------------------------- */
enum ag_iis {
	AG_IIS0,
	AG_IIS1,
	AG_IIS2,
	AG_IIS_MAX
};

/* AGCP IIS multiplexer setting.
 * @iis: the iis channel to be set.
 * @en:
 *   0: AG_IIS0_EXT_SEL to whale2 top
 *   1: AG_IIS0_EXT_SEL to audio top
 */
static inline int arch_audio_iis_to_audio_top_enable(
	enum ag_iis iis, int en)
{
	u32 val;
	int ret;

	agcp_ahb_gpr_null_check();

	switch (iis) {
	case AG_IIS0:
		val = BIT_AG_IIS0_EXT_SEL;
		break;
	case AG_IIS1:
		val = BIT_AG_IIS1_EXT_SEL;
		break;
	case AG_IIS2:
		val = BIT_AG_IIS2_EXT_SEL;
		break;
	default:
		pr_err("%s, agcp iis mux setting error!\n", __func__);
		return -1;
	};

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}

	if (en)
		agcp_ahb_reg_set(REG_AGCP_AHB_EXT_ACC_AG_SEL, val);
	else
		agcp_ahb_reg_clr(REG_AGCP_AHB_EXT_ACC_AG_SEL, val);
	agdsp_access_disable();

	return 0;
}

/* Codec digital part in soc setting */
static inline int arch_audio_codec_digital_reg_enable(void)
{
	int ret = 0;
	int test_v;

	agcp_ahb_gpr_null_check();
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}
	agcp_ahb_reg_read(REG_AGCP_AHB_MODULE_EB0_STS, &test_v);

	pr_debug("enter %s REG_AGCP_AHB_MODULE_EB0_STS, test_v=%#x\n",
		__func__, test_v);
	ret = agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_EB0_STS, BIT_AUD_EB);
	if (ret >= 0)
		ret = agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_EB0_STS,
			BIT_AUDIF_CKG_AUTO_EN);

	agcp_ahb_reg_read(REG_AGCP_AHB_MODULE_EB0_STS, &test_v);
	pr_debug("%s set aud en %#x\n", __func__, test_v);
	agdsp_access_disable();

	return ret;
}

static inline int arch_audio_codec_digital_reg_disable(void)
{
	int ret = 0;

	agcp_ahb_gpr_null_check();
	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}
	ret = agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_EB0_STS,
		BIT_AUDIF_CKG_AUTO_EN);
	if (ret >= 0)
		ret = agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_EB0_STS, BIT_AUD_EB);

	agdsp_access_disable();

	return ret;
}

static inline int arch_audio_codec_digital_enable(void)
{
	return 0;
}

static inline int arch_audio_codec_digital_disable(void)
{
	return 0;
}

static inline int arch_audio_codec_switch2ap(void)
{
	return 0;
}

static inline int arch_audio_codec_digital_reset(void)
{
	int ret = 0;

	ret = agdsp_access_enable();
	if (ret) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}
	agcp_ahb_gpr_null_check();
	agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_RST0_STS, BIT_AUD_SOFT_RST);
	udelay(10);
	agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_RST0_STS, BIT_AUD_SOFT_RST);

	agdsp_access_disable();

	return ret;
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


/*AP_APB registers offset */
#define REG_AP_APB_APB_EB		0x0000
#define REG_AP_APB_APB_RST		0x0004

/* REG_AP_APB_APB_EB */
#define BIT_AP_APB_IIS0_EB		BIT(1)

/* REG_AP_APB_APB_RST */
#define BIT_AP_APB_IIS0_SOFT_RST	BIT(1)

#define DMA_REQ_IIS0_RX			(2 + 1)
#define DMA_REQ_IIS0_TX			(3 + 1)


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
		ret = -EINVAL;
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
		ret = -EINVAL;
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
		ret = -EINVAL;
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
		ret = -EINVAL;
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
	case 2:
	case 3:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#endif/* __SPRD_AUDIO_AGCP_H_ */
