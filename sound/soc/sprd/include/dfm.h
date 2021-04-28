/*
 * sound/soc/sprd/dfm.h
 *
 * SPRD SoC VBC -- SpreadTrum SOC for VBC DAI function.
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SPRD_DFM_H
#define __SPRD_DFM_H

#define DFM_MAGIC_ID  0x7BC

struct sprd_dfm_priv {
	int hw_rate;
	int sample_rate;
};

void dfm_priv_set(struct sprd_dfm_priv *in_dfm);

#endif /* __SPRD_DFM_H */
