 /*
 * Copyright (C) 2015-2018 Spreadtrum Communications Inc.
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

#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_

struct syscon_ufs {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct ufs_sprd_host {
	struct ufs_hba *hba;
	void __iomem *ufsutp_reg;
	void __iomem *unipro_reg;
	void __iomem *ufs_ao_reg;
	void __iomem *mphy_reg;
	void __iomem *rus;
	struct syscon_ufs ap_apb_ufs_en;
	struct syscon_ufs ap_apb_ufs_rst;
	struct syscon_ufs anlg_mphy_ufs_rst;
	struct syscon_ufs aon_apb_ufs_rst;
};

/* UFS host controller vendor specific registers */
#define REG_SW_RST	0xb0
#define HCI_RST		(1 << 12)
#define HCI_CLOD_RST	(1 << 28)

/* UFS unipro registers */
#define REG_PA_7	0x1c
#define REDESKEW_MASK	(1 << 21)

#define REG_PA_15		0x3c
#define RMMI_TX_L0_RST		(1 << 24)
#define RMMI_TX_L1_RST		(1 << 25)
#define RMMI_RX_L0_RST		(1 << 26)
#define RMMI_RX_L1_RST		(1 << 27)
#define RMMI_CB_RST		(1 << 28)
#define RMMI_RST		(1 << 29)

#define REG_PA_27		0x148
#define RMMI_TX_DIRDY_SEL	(1 << 0)

#define REG_DL_0	0x40
#define DL_RST		(1 << 0)

#define REG_N_1		0x84
#define N_RST		(1 << 1)

#define REG_T_9		0xc0
#define T_RST		(1 << 4)

#define REG_DME_0	0xd0
#define DME_RST		(1 << 2)

/* UFS utp registers */
#define REG_UTP_MISC	0x100
#define TX_RSTZ		(1 << 0)
#define RX_RSTZ		(1 << 1)

/* UFS ao registers */
#define REG_AO_SW_RST	0x1c
#define XTAL_RST	(1 << 1)

/* UFS mphy registers */
#define REG_DIG_CFG7		0x1c
#define CDR_MONITOR_BYPASS	(1 << 24)

#define REG_DIG_CFG35	0x8c
#define TX_FIFOMODE	(1 << 15)

#endif/* _UFS_SPRD_H_ */
