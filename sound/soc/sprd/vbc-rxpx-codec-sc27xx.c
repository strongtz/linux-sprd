/*
 * sound/soc/sprd/vbc-rxpx-codec-sc27xx.c
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
#define pr_fmt(fmt) pr_sprd_fmt("BOARD")""fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <uapi/sound/asound.h>

#include "dfm.h"
#include "sprd-asoc-card-utils.h"
#include "sprd-asoc-common.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
void dfm_priv_set(struct sprd_dfm_priv *in_dfm)
__attribute__ ((weak, alias("__dfm_priv_set")));

static void __dfm_priv_set(struct sprd_dfm_priv *in_dfm)
{
	pr_debug("%s is empty.\n", __func__);
}
#pragma GCC diagnostic pop

static int dfm_rate(struct snd_pcm_substream *substream,
		    struct snd_pcm_hw_params *params)
{
	int ret;
	struct sprd_dfm_priv dfm;
	struct sprd_card_data *mdata = NULL;
	struct snd_soc_pcm_runtime *srtd = substream->private_data;

	mdata = snd_soc_card_get_drvdata(srtd->card);

	if (mdata == NULL) {
		pr_err("%s mdata is NULL\n", __func__);
		return 0;
	}
	if (mdata->fm_hw_rate)
		dfm.hw_rate = mdata->fm_hw_rate;
	else
		dfm.hw_rate = params_rate(params);

	if (dfm.hw_rate == 8000)
		dfm.sample_rate = 8000;
	else if (mdata->is_fm_open_src)
		dfm.sample_rate = 44100;
	else
		dfm.sample_rate = params_rate(params);

	pr_info("%s: hw_rate:%d,  sample_rate:%d\n",
		__func__, dfm.hw_rate, dfm.sample_rate);

	if (dfm.hw_rate != dfm.sample_rate) {
		struct snd_interval *rate =
		    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
		struct snd_interval dfm_rates;

		dfm_rates.min = dfm.sample_rate;
		dfm_rates.max = dfm.sample_rate;
		dfm_rates.openmin = dfm_rates.openmax = 0;
		dfm_rates.integer = 0;

		ret = snd_interval_refine(rate, &dfm_rates);
		if (ret < 0) {
			pr_err("ERR: dfm sample rate refine failed!\n");
			return ret;
		}
	}

	dfm_priv_set(&dfm);

	return 0;
}

static int dfm_params(struct snd_pcm_substream *substream,
		      struct snd_pcm_hw_params *params)
{

	sp_asoc_pr_dbg("%s\n", __func__);
	dfm_rate(substream, params);

	return 0;
}

static struct snd_soc_ops sprd_dai_link_ops[] = {
	{.hw_params = dfm_params,},	/* dfm_ops */
};

static int vbc_rxpx_codec_sc27xx_late_probe(struct snd_soc_card *card)
{
	sprd_asoc_board_comm_late_probe(card);

	snd_soc_dapm_ignore_suspend(&card->dapm, "inter HP PA");
	snd_soc_dapm_ignore_suspend(&card->dapm, "inter Spk PA");
	snd_soc_dapm_ignore_suspend(&card->dapm, "inter Ear PA");

	return 0;
}

static const struct sprd_asoc_data sprd_asoc_data_r3p0_2731 = {
	.vbc_type = BOARD_T_VBC_R3P0,
	.codec_type = BOARD_T_CODEC_2731,
};
static const struct sprd_asoc_data sprd_asoc_data_r1p0v3_2721 = {
	.vbc_type = BOARD_T_VBC_R1P0V3,
	.codec_type = BOARD_T_CODEC_2721,
};
static const struct sprd_asoc_data sprd_asoc_data_r1p0v3_2731 = {
	.vbc_type = BOARD_T_VBC_R1P0V3,
	.codec_type = BOARD_T_CODEC_2731,
};
static const struct sprd_asoc_data sprd_asoc_data_r2p0_2723 = {
	.vbc_type = BOARD_T_VBC_R2P0,
	.codec_type = BOARD_T_CODEC_2723,
};
static const struct sprd_asoc_data sprd_asoc_data_v4_2730 = {
	.vbc_type = BOARD_T_VBC_V4,
	.codec_type = BOARD_T_CODEC_2730,
};

static const struct of_device_id vbc_rxpx_codec_sc27xx_of_match[] = {
	{.compatible = "sprd,vbc-r3p0-codec-sc2731",
	 .data = &sprd_asoc_data_r3p0_2731},
	{.compatible = "sprd,vbc-r1p0v3-codec-sc2721",
	 .data = &sprd_asoc_data_r1p0v3_2721},
	{.compatible = "sprd,vbc-r1p0v3-codec-sc2731",
	 .data = &sprd_asoc_data_r1p0v3_2731},
	{.compatible = "sprd,vbc-r2p0-codec-sc2723",
	 .data = &sprd_asoc_data_r2p0_2723},
	 {.compatible = "sprd,vbc-v4-codec-sc2730",
	 .data = &sprd_asoc_data_v4_2730},
	{},
};

MODULE_DEVICE_TABLE(of, vbc_rxpx_codec_sc27xx_of_match);

static int vbc_rxpx_codec_sc27xx_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card;
	struct asoc_sprd_ptr_num pn_ops = ASOC_SPRD_PRT_NUM(sprd_dai_link_ops);
	const struct of_device_id *of_id;
	struct sprd_card_data *mdata;

	asoc_sprd_card_set_ops(&pn_ops);

	ret = asoc_sprd_card_probe(pdev, &card);
	if (ret) {
		pr_err("ERR: %s, asoc_sprd_card_probe failed!\n", __func__);
		return ret;
	}

	sprd_asoc_board_comm_probe();
	/* Get the board type. */
	of_id = of_match_node(vbc_rxpx_codec_sc27xx_of_match,
			      pdev->dev.of_node);
	if (!of_id) {
		pr_err("Get the asoc board of device id failed!\n");
		return -ENODEV;
	}
	mdata = snd_soc_card_get_drvdata(card);
	if (mdata)
		mdata->board_type = (struct sprd_asoc_data *)of_id->data;

	/* Add your special configurations here */
	/* Add card special kcontrols */
	card->controls = sprd_asoc_card_controls.ptr;
	card->num_controls = sprd_asoc_card_controls.size;

	card->dapm_widgets = sprd_asoc_card_widgets.ptr;
	card->num_dapm_widgets = sprd_asoc_card_widgets.size;

	card->late_probe = vbc_rxpx_codec_sc27xx_late_probe;

	return asoc_sprd_register_card(&pdev->dev, card);
}

static struct platform_driver vbc_rxpx_codec_sc27xx_driver = {
	.driver = {
		.name = "vbc-rxpx-codec-sc27xx",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table =
			vbc_rxpx_codec_sc27xx_of_match,
	},
	.probe = vbc_rxpx_codec_sc27xx_probe,
	.remove = asoc_sprd_card_remove,
	.shutdown = sprd_asoc_shutdown,
};

static int __init vbc_rxpx_codec_sc27xx_init(void)
{
	return platform_driver_register(&vbc_rxpx_codec_sc27xx_driver);
}

late_initcall_sync(vbc_rxpx_codec_sc27xx_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC SPRD Sound Card VBC rxpx & Codec Sc27xx");
MODULE_AUTHOR("Peng Lee <peng.lee@spreadtrum.com>");
