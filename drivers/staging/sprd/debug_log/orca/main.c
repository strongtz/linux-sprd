/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <video/sprd_mmsys_pw_domain.h>
#include "core.h"

static u32 s_freq_array[] = {
	500000,
	1500000,
	2000000,
	2500000,
};

static u32 s_dsi_ctrl_offset_array[] = {
	0x20,	/* serdes0 */
	0x60,	/* serdes1 */
	0xa0,	/* serdes2 */
};

static char serdes_name[50];

static void test_io(struct dbg_log_device *dbg, u32 addr, u32 data)
{
	u32 dsi_ctrl_offset = s_dsi_ctrl_offset_array[dbg->serdes_id];

	regmap_write_bits(dbg->phy->dsi_apb,
			  dsi_ctrl_offset + 0x8,
			  0xffffffff,
			  ((addr) << 11) | (0x1 << 2));
	regmap_write_bits(dbg->phy->dsi_apb,
			  dsi_ctrl_offset + 0x8,
			  0xffffffff,
			  ((addr) << 11) | (0x1 << 2) | (0x1 << 1));
	regmap_write_bits(dbg->phy->dsi_apb,
			  dsi_ctrl_offset + 0x8,
			  0xffffffff,
			  ((addr) << 11) | (0x1 << 2));
	regmap_write_bits(dbg->phy->dsi_apb,
			  dsi_ctrl_offset + 0x8,
			  0xffffffff,
			  ((data) << 11));
	regmap_write_bits(dbg->phy->dsi_apb,
			  dsi_ctrl_offset + 0x8,
			  0xffffffff,
			  ((data) << 11) | (0x1 << 1));
	regmap_write_bits(dbg->phy->dsi_apb,
			  dsi_ctrl_offset + 0x8,
			  0xffffffff,
			  ((data) << 11));
}

static void config_clk_1500M(struct dbg_log_device *dbg)
{
	test_io(dbg, 0x03, 0x7);
	test_io(dbg, 0x04, 0xc8);
	test_io(dbg, 0x06, 0x39);
	test_io(dbg, 0x07, 0x4);
	test_io(dbg, 0x08, 0xde);
	test_io(dbg, 0x09, 0xb1);
	test_io(dbg, 0x0a, 0x3b);
	test_io(dbg, 0x0b, 0x11);
	test_io(dbg, 0x0c, 0x2);
	test_io(dbg, 0x0d, 0x8a);
	test_io(dbg, 0x0e, 0x0);
	test_io(dbg, 0x0f, 0x81);
}

static void config_clk_2000M(struct dbg_log_device *dbg)
{
	test_io(dbg, 0x03, 0x3);
	test_io(dbg, 0x04, 0xc8);
	test_io(dbg, 0x06, 0x4c);
	test_io(dbg, 0x07, 0x4);
	test_io(dbg, 0x08, 0xdf);
	test_io(dbg, 0x09, 0xec);
	test_io(dbg, 0x0a, 0x4e);
	test_io(dbg, 0x0b, 0xc1);
	test_io(dbg, 0x0c, 0x2);
	test_io(dbg, 0x0d, 0x8a);
	test_io(dbg, 0x0e, 0x0);
	test_io(dbg, 0x0f, 0x81);
}

static void config_clk_2500M(struct dbg_log_device *dbg)
{
	test_io(dbg, 0x03, 0x3);
	test_io(dbg, 0x04, 0xc8);
	test_io(dbg, 0x06, 0x60);
	test_io(dbg, 0x07, 0x4);
	test_io(dbg, 0x08, 0xdf);
	test_io(dbg, 0x09, 0x27);
	test_io(dbg, 0x0a, 0x62);
	test_io(dbg, 0x0b, 0x71);
	test_io(dbg, 0x0c, 0x2);
	test_io(dbg, 0x0d, 0x8a);
	test_io(dbg, 0x0e, 0x0);
	test_io(dbg, 0x0f, 0x81);
}

static void config_clk_500M(struct dbg_log_device *dbg)
{
	test_io(dbg, 0x3, 0x3);
	test_io(dbg, 0x4, 0xc8);
	test_io(dbg, 0x6, 0x4c);
	test_io(dbg, 0x7, 0x13);
	test_io(dbg, 0x8, 0x6f);
	test_io(dbg, 0x9, 0xec);
	test_io(dbg, 0xa, 0x4e);
	test_io(dbg, 0xb, 0xc4);
	test_io(dbg, 0xc, 0x2);
	test_io(dbg, 0xd, 0x8a);
	test_io(dbg, 0xe, 0x0);
	test_io(dbg, 0xf, 0x81);
}

static void serdes_i_init(struct dbg_log_device *dbg)
{
	u32 dsi_ctrl_offset = s_dsi_ctrl_offset_array[dbg->serdes_id];

	/* enable phy ref clk */
	clk_prepare_enable(dbg->clk_dsi_ref_eb);

	/* enable serdes DPHY_CFG_EB & DPHY_REF_EB */
	clk_prepare_enable(dbg->clk_dphy_cfg_eb);
	clk_prepare_enable(dbg->clk_dphy_ref_eb);

	/* enable serdes */
	clk_prepare_enable(dbg->clk_serdes_eb);

	/* enable ana eb */
	clk_prepare_enable(dbg->clk_ana_eb);

	/* enable dbg sel */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset + 0x18,
			   0x7f,
			   0x7f);

	/* dbg_sel TEST_CLR */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset + 0x1c,
			   0x40,
			   0x40);

	/* set 2P2L DSI TEST_CLR */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset + 0x8,
			   0x1,
			   0x1);

	/* dbg sel for pd */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset + 0x18,
			   0x180,
			   0x180);

	/* pd clr */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset,
			   0x200000,
			   ~0x200000);

	/* #12 us */
	usleep_range(1000, 1100); /* Wait for 1mS */

	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset,
			   0x100000,
			   ~0x100000);

	/* #12 us */
	usleep_range(1000, 1100); /* Wait for 1mS */

	/* dbg sel --test en, test clk, iso sel */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset + 0x1c,
			   0x390,
			   0x390);

	/* iso clr */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset + 0x14,
			   0x10,
			   ~0x10);

	/* test clr . clr */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset + 0x8,
			   0x1,
			   ~0x1);

	/* shutdownz */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset,
			   (1 << 19), (1 << 19));

	/* dsi rstz */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset,
			   (1 << 18), (1 << 18));

	/* dsi enable clk */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset,
			   (1 << 13), (1 << 13));

	/* dsi enable0123 */
	regmap_update_bits(dbg->phy->dsi_apb,
			   dsi_ctrl_offset,
			   (1 << 14) | (1 << 15) | (1 << 16) | (1 << 17),
			   (1 << 14) | (1 << 15) | (1 << 16) | (1 << 17));

	switch (dbg->phy->freq) {
	case 1500000:
		config_clk_1500M(dbg);
		break;
	case 2000000:
		config_clk_2000M(dbg);
		break;
	case 2500000:
		config_clk_2500M(dbg);
		break;
	case 500000:
	default:
		config_clk_500M(dbg);
		break;
	}

	/* 130us */
	usleep_range(1000, 1100); /* Wait for 1mS */
}

static void inter_dbg_log_init(struct dbg_log_device *dbg)
{
	serdes_i_init(dbg);
}

static void inter_dbg_log_exit(struct dbg_log_device *dbg)
{
	/* disable ana eb */
	clk_disable_unprepare(dbg->clk_ana_eb);

	/* disable serdes */
	clk_disable_unprepare(dbg->clk_serdes_eb);

	/* disable serdes DPHY_CFG_EB & DPHY_REF_EB */
	clk_disable_unprepare(dbg->clk_dphy_cfg_eb);
	clk_disable_unprepare(dbg->clk_dphy_ref_eb);

	/* disable phy ref clk */
	clk_disable_unprepare(dbg->clk_dsi_ref_eb);
}

static void inter_dbg_log_chn_sel(struct dbg_log_device *dbg)
{
	if (dbg->channel) {
		dbg->serdes.channel = dbg->serdes.ch_map[dbg->channel - 1];
		serdes_enable(&dbg->serdes, 1);
	} else {
		serdes_enable(&dbg->serdes, 0);
	}
}

static bool inter_dbg_log_is_freq_valid(struct dbg_log_device *dbg,
					unsigned int freq)
{
	int i;

	DEBUG_LOG_PRINT("input freq %d\n", freq);
	for (i = 0; i < ARRAY_SIZE(s_freq_array); i++) {
		if (s_freq_array[i] == freq) {
			dbg->phy->clk_sel = i;
			return true;
		}
	}
	DEBUG_LOG_PRINT("input freq %d not match\n", freq);
	return false;
}

static int inter_dbg_log_get_valid_channel(struct dbg_log_device *dbg,
					   const char *buf)
{
	int i, cmp_len = strlen(buf);

	DEBUG_LOG_PRINT("input channel %s", buf);
	if (!strncasecmp(STR_CH_DISABLE, buf, cmp_len))
		return 0;
	for (i = 0; i < dbg->serdes.ch_num; i++) {
		if (!strncasecmp(dbg->serdes.ch_str[i], buf, cmp_len))
			return i + 1;
	}
	DEBUG_LOG_PRINT("not match input channel %s", buf);
	return -EINVAL;
}

static bool inter_dbg_log_fill_freq_array(struct dbg_log_device *dbg,
					  char *sbuf)
{
	int i, ret;
	char temp_buf[16];

	strcat(sbuf, "[");
	for (i = 0; i < ARRAY_SIZE(s_freq_array); i++) {
		ret = snprintf(temp_buf,
			       16, " %u",
			       (unsigned int)s_freq_array[i]);
		if (ret >= 16) {
			DEBUG_LOG_PRINT("len(%d) of s_freq_array[%d] >= 16",
					ret,
					i);
			return false;
		}
		strcat(sbuf, temp_buf);
	}
	strcat(sbuf, "]");

	return true;
}

static struct dbg_log_ops ops = {
	.init = inter_dbg_log_init,
	.exit = inter_dbg_log_exit,
	.select = inter_dbg_log_chn_sel,
	.is_freq_valid = inter_dbg_log_is_freq_valid,
	.fill_freq_array = inter_dbg_log_fill_freq_array,
	.get_valid_channel = inter_dbg_log_get_valid_channel,
};

static int dbg_log_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *addr, *serdes_apb;
	struct dbg_log_device *dbg;
	struct regmap *dsi_apb;
	int count, i, rc;
	int serdes_id;

	DEBUG_LOG_PRINT("%s\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	serdes_apb = (void __iomem *)addr;

	dsi_apb = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						  "sprd,syscon-dsi-apb");
	if (IS_ERR(dsi_apb)) {
		pr_err("dsi apb get failed!\n");
		return PTR_ERR(dsi_apb);
	}

	serdes_id = of_alias_get_id(pdev->dev.of_node, "serdes");
	if (serdes_id < 0) {
		pr_err("of_alias_get_id (%d) failed!\n", serdes_id);
		return serdes_id;
	}
	DEBUG_LOG_PRINT("serdes_id = %d\n", serdes_id);

	sprintf(serdes_name, "serdes%d", serdes_id);
	dbg = dbg_log_device_register(&pdev->dev, &ops, NULL, serdes_name);

	if (!dbg)
		return -ENOMEM;

	dbg->serdes_id = serdes_id;
	dbg->phy->freq = 500000;
	dbg->phy->dsi_apb = dsi_apb;
	dbg->serdes.base = serdes_apb;
	dbg->serdes.cut_off = 0x20;

	if (of_property_read_bool(pdev->dev.of_node, "sprd,dcfix")) {
		DEBUG_LOG_PRINT("dcfix enable\n");
		dbg->serdes.dc_blnc_fix = 3;
	}

	count = of_property_count_strings(pdev->dev.of_node, "sprd,ch-name");
	DEBUG_LOG_PRINT("ch_num %d\n", count);

	if (count > 0 && count < CH_MAX) {
		dbg->serdes.ch_num = count;
		rc = of_property_read_u32_array(pdev->dev.of_node,
						"sprd,ch-index",
						dbg->serdes.ch_map,
						count);
		DEBUG_LOG_PRINT("ch-index count %d, rc %d\n", count, rc);
		if (rc) {
			pr_err("get channel map failed\n");
			dbg->serdes.ch_num = 0;
		}
		for (i = 0; i < count; i++)
			DEBUG_LOG_PRINT("sel %d = 0x%x\n",
					i,
					dbg->serdes.ch_map[i]);

		rc = of_property_read_string_array(pdev->dev.of_node,
						   "sprd,ch-name",
						   dbg->serdes.ch_str,
						   count);
		DEBUG_LOG_PRINT("ch-name count %d, rc %d\n", count, rc);
		if (rc != count) {
			pr_err("get channel string failed\n");
			dbg->serdes.ch_num = 0;
		}
		for (i = 0; i < count; i++)
			DEBUG_LOG_PRINT("str %d = %s\n",
					i,
					dbg->serdes.ch_str[i]);
	}

	dbg->clk_serdes_eb = devm_clk_get(&pdev->dev, "serdes_eb");
	if (IS_ERR(dbg->clk_serdes_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: serdes_eb\n");
		dbg->clk_serdes_eb = NULL;
	}
	dbg->clk_ana_eb = devm_clk_get(&pdev->dev, "ana_eb");
	if (IS_ERR(dbg->clk_ana_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: ana_eb\n");
		dbg->clk_ana_eb = NULL;
	}

	dbg->clk_dphy_cfg_eb = devm_clk_get(&pdev->dev, "dphy_cfg_eb");
	if (IS_ERR(dbg->clk_dphy_cfg_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: clk_dphy_cfg_eb\n");
		dbg->clk_dphy_cfg_eb = NULL;
	}
	dbg->clk_dphy_ref_eb = devm_clk_get(&pdev->dev, "dphy_ref_eb");
	if (IS_ERR(dbg->clk_dphy_ref_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: clk_dphy_ref_eb\n");
		dbg->clk_dphy_ref_eb = NULL;
	}
	dbg->clk_dsi_ref_eb = devm_clk_get(&pdev->dev, "dsi_ref_eb");
	if (IS_ERR(dbg->clk_dsi_ref_eb)) {
		dev_warn(&pdev->dev,
			 "can't get the clock dts config: clk_dsi_ref_eb\n");
		dbg->clk_dsi_ref_eb = NULL;
	}

	inter_dbg_log_is_freq_valid(dbg, dbg->phy->freq);

	return 0;
}

static const struct of_device_id dt_ids[] = {
	{.compatible = "sprd,dbg-log-orca",},
	{},
};

static struct platform_driver dbg_log_driver = {
	.probe = dbg_log_probe,
	.driver = {
		.name = "modem-dbg-log",
		.of_match_table = dt_ids,
	},
};

module_platform_driver(dbg_log_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Bin Ji <bin.ji@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SoC Modem Debug Log Driver");
