/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 */


#ifndef _AUD_TOPA_RF_REG_H
#define _AUD_TOPA_RF_REG_H

#define CTL_BASE_AUD_TOPA_RF (CTL_BASE_AUD_CFGA_RF - 0x1000 + 0x100)

#define AUD_CFGA_CLK_EN                   (CTL_BASE_AUD_TOPA_RF + 0x0000)
#define AUD_CFGA_SOFT_RST                 (CTL_BASE_AUD_TOPA_RF + 0x0004)
#define AUD_CFGA_LP_MODULE_CTRL           (CTL_BASE_AUD_TOPA_RF + 0x0008)
#define AUD_CFGA_ANA_ET1                  (CTL_BASE_AUD_TOPA_RF + 0x000C)
#define AUD_CFGA_ANA_ET2                  (CTL_BASE_AUD_TOPA_RF + 0x0010)
#define AUD_CFGA_ANA_ET3                  (CTL_BASE_AUD_TOPA_RF + 0x0014)
#define AUD_CFGA_ANA_ET4                  (CTL_BASE_AUD_TOPA_RF + 0x0018)
#define AUD_CFGA_AUDIF_CTL0               (CTL_BASE_AUD_TOPA_RF + 0x0040)
#define AUD_CFGA_DAC_FIFO_STS             (CTL_BASE_AUD_TOPA_RF + 0x0044)
#define AUD_CFGA_ADC_FIFO_STS             (CTL_BASE_AUD_TOPA_RF + 0x0048)
#define AUD_CFGA_AUDIF_STS                (CTL_BASE_AUD_TOPA_RF + 0x004C)
#define AUD_CFGA_RAW_STS                  (CTL_BASE_AUD_TOPA_RF + 0x0050)
#define AUD_CFGA_RAW_STS_CLR              (CTL_BASE_AUD_TOPA_RF + 0x0054)

/*
 * Register Name  :REG_AUD_TOPA_RF_CLK_EN
 * Register Offset:0x0000
 * Description    :
 */

#define BIT_CLK_AUD_LOOP_INV_EN                     BIT(2)
#define BIT_CLK_AUD_TOPA_6P5M_EN                         BIT(1)
#define BIT_CLK_AUD_LOOP_EN                         BIT(0)

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_SOFT_RST
 * Register Offset:0x0004
 * Description    :
 */

#define BIT_AUD_DAC_POST_SOFT_RST                   BIT(2)
#define BIT_AUD_DIG_6P5M_SOFT_RST                   BIT(1)
#define BIT_AUD_DIG_LOOP_SOFT_RST                   BIT(0)

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_AUD_CFGA_LP_MODULE_CTRL
 * Register Offset:0x0008
 * Description    :
 */

#define BIT_ADC_EN_R                                BIT(5)
#define BIT_DAC_EN_R                                BIT(4)
#define BIT_ADC_EN_L                                BIT(3)
#define BIT_DAC_EN_L                                BIT(2)
#define BIT_AUDIO_LOOP_MAP_SEL                      BIT(1)
#define BIT_AUDIO_ADIE_LOOP_EN                      BIT(0)

#define BIT_ADC_EN_R_S                              5
#define BIT_DAC_EN_R_S                              4
#define BIT_ADC_EN_L_S                              3
#define BIT_DAC_EN_L_S                              2

/*
 * Register Name  :REG_AUD_TOPA_RF_ANA_ET1
 * Register Offset:0x000C
 * Description    :
 */

#define BIT_RG_AUD_DA0_DC(x)                        (((x) & 0x7F) << 7)
#define BIT_RG_AUD_DA1_DC(x)                        (((x) & 0x7F))

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_ANA_ET2
 * Register Offset:0x0010
 * Description    :
 */

#define BIT_DEM_BYPASS                              BIT(9)
#define BIT_RG_AUD_ET_EN                            BIT(8)
#define BIT_RG_AUD_ET_OUT_INV                       BIT(7)
#define BIT_RG_AUD_DALR_MIX_SEL(x)                  (((x) & 0x3) << 5)
#define BIT_RG_AUD_DAS_MIX_SEL(x)                   (((x) & 0x3) << 3)
#define BIT_RG_AUD_CP_V_AUTO                        BIT(2)
#define BIT_RG_AUD_CP_V_SEL                         BIT(1)
#define BIT_RG_AUD_CP_VIN_S                         BIT(0)

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_ANA_ET3
 * Register Offset:0x0014
 * Description    :
 */

#define BIT_RG_AUD_ET_HOLD_MS(x)                    (((x) & 0xFFFF))

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_ANA_ET4
 * Register Offset:0x0018
 * Description    :
 */

#define BIT_RG_AUD_ET_MAX_SEL(x)                    (((x) & 0x3) << 14)
#define BIT_RG_AUD_ET_MAX_SET(x)                    (((x) & 0x7F) << 7)
#define BIT_RG_AUD_ET_VTRIG(x)                      (((x) & 0x7F))

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_AUDIF_CTL0
 * Register Offset:0x0040
 * Description    :
 */

#define BIT_AUDIF_5P_MODE                           BIT(4)
#define BIT_DA_SDM_OUT_SEL                          BIT(3)
#define BIT_DAC_FIFO_AF_LVL_R(x)                    (((x) & 0x7))

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_DAC_FIFO_STS
 * Register Offset:0x0044
 * Description    :
 */

#define BIT_DAC_FIFO_EMPTY_R                        BIT(7)
#define BIT_DAC_FIFO_FULL_W                         BIT(6)
#define BIT_DAC_FIFO_ADDR_R(x)                      (((x) & 0x7) << 3)
#define BIT_DAC_FIFO_ADDR_W(x)                      (((x) & 0x7))

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_ADC_FIFO_STS
 * Register Offset:0x0048
 * Description    :
 */

#define BIT_ADC_FIFO_AF                             BIT(8)
#define BIT_ADC_FIFO_EMPTY_R                        BIT(7)
#define BIT_ADC_FIFO_FULL_W                         BIT(6)
#define BIT_ADC_FIFO_ADDR_R(x)                      (((x) & 0x7) << 3)
#define BIT_ADC_FIFO_ADDR_W(x)                      (((x) & 0x7))

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_AUDIF_STS
 * Register Offset:0x004C
 * Description    :
 */

#define BIT_CUR_ST(x)                               (((x) & 0x3) << 3)
#define BIT_TS_CNT(x)                               (((x) & 0x3) << 1)
#define BIT_ADC_RX_DATA_RDY                         BIT(0)

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_RAW_STS
 * Register Offset:0x0050
 * Description    :
 */

#define BIT_ADC_FIFO_UNDERFL_STS                    BIT(1)
#define BIT_DAC_FIFO_OVFL_STS                       BIT(0)

/*---------------------------------------------------------------------------
 * Register Name  :REG_AUD_TOPA_RF_RAW_STS_CLR
 * Register Offset:0x0054
 * Description    :
 */

#define BIT_DAC_FIFO_OVFL_CLR                       BIT(1)
#define BIT_ADC_FIFO_UNDERFL_CLR                    BIT(0)



static inline void sprd_codec_audif_et_en(struct snd_soc_codec *codec, int on)
{
	int mask = 0;
	int val = 0;

	mask = BIT_RG_AUD_ET_EN;
	val = on ? mask:0;
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_ANA_ET2), mask, val);
}

static inline void sprd_codec_audif_dc_os_set(struct snd_soc_codec *codec,
	int offset)
{
	int mask, val;

	mask = BIT_RG_AUD_DA0_DC(0x7f) | BIT_RG_AUD_DA1_DC(0x7f);
	val =  (BIT_RG_AUD_DA0_DC(offset) | BIT_RG_AUD_DA1_DC(offset));
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_ANA_ET1), mask, val);
}

static void sprd_codec_audif_clk_enable(struct snd_soc_codec *codec, int en)
{
	int msk, val;

	msk = BIT_CLK_AUD_TOPA_6P5M_EN;
	val = en ? msk:0;
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_CLK_EN), msk, val);
	msk = BIT_AUD_DAC_POST_SOFT_RST | BIT_AUD_DIG_6P5M_SOFT_RST;
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_SOFT_RST), msk, msk);
	udelay(10);
	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_SOFT_RST), msk, 0);

	snd_soc_update_bits(codec, SOC_REG(AUD_CFGA_AUDIF_CTL0),
		BIT_AUDIF_5P_MODE, 0);
	/* yintang: for temp test */
	sprd_codec_audif_et_en(codec, en);
}


#endif /* _AUD_TOPA_RF_REG_H */
