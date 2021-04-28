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

#ifndef SPRD_RSASSA_H
#define SPRD_RSASSA_H

#include "sprd_crypto.h"

enum sprd_rsassa_padding_type {
	SPRD_RSASSA_EMSA   = 1,        /* RSASSA signature padding */
	SPRD_RSASSA_EME    = 2         /* RSASSA encryption padding */
};

unsigned int sprd_pkcs_v1_5_decode(const unsigned char *msg,
		unsigned int msglen,
		int block_type,
		unsigned int modulus_len,
		unsigned char *out,
		unsigned int *outlen,
		int *is_valid);

unsigned int sprd_pss_decode(const unsigned char *msghash,
		unsigned int msghashlen,
		const unsigned char *sig, unsigned int siglen,
		unsigned int saltlen, sprd_crypto_algo_t hash_type,
		sprd_crypto_algo_t mgf1_hash_type,
		unsigned int modulus_bitlen, int *res);

#endif
