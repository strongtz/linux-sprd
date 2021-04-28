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

#ifndef SPRD_PSS_MGF1_H
#define SPRD_PSS_MGF1_H

#include "sprd_crypto.h"

unsigned int sprd_pss_mgf1(sprd_crypto_algo_t hash_type,
		const unsigned char *seed, unsigned long seedlen,
		unsigned char *mask, unsigned long masklen);

#endif
