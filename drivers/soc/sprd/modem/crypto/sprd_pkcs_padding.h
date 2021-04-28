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
#ifndef SPRD_PKCS_PADDING_H
#define SPRD_PKCS_PADDING_H

int padding_add_pkcs_type_1(unsigned char *to, int tlen,
		const unsigned char *from, int flen);

int padding_check_pkcs_type_1(unsigned char *to, int tlen,
		const unsigned char *from, int flen, int num);

int padding_add_pkcs_type_2(unsigned char *to, int tlen,
		const unsigned char *from, int flen);

int padding_check_pkcs_type_2(unsigned char *to, int tlen,
		const unsigned char *from, int flen, int num);

#endif
