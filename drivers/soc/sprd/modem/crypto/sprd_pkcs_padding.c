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

#include <linux/string.h>
#include "sprd_pkcs_padding.h"

#define RSA_PKCS_PADDING_SIZE	11

static int get_rand_bytes(unsigned char *buf, int num)
{
	int i;

	for (i = 0; i < num; i++)
		*(buf + i) = 0xAA;

	return 1;
}

int padding_add_pkcs_type_1(unsigned char *to, int tlen,
		const unsigned char *from, int flen)
{
	int j;
	unsigned char *p;

	if (flen > (tlen - RSA_PKCS_PADDING_SIZE))
		return 0;

	p = (unsigned char *)to;

	*(p++) = 0;
	*(p++) = 1;		/* Private Key BT (Block Type) */

	/* pad out with 0xff data */
	j = tlen - 3 - flen;
	memset(p, 0xff, j);
	p += j;
	*(p++) = '\0';

	memcpy(p, from, (unsigned int)flen);

	return 1;
}

int padding_check_pkcs_type_1(unsigned char *to, int tlen,
		const unsigned char *from, int flen, int num)
{
	int i, j;
	const unsigned char *p;

	p = from;
	if ((num != (flen + 1)) || (*(p++) != 01))
		return (-1);

	/* scan over padding data */
	j = flen - 1;		/* one for type. */
	for (i = 0; i < j; i++) {
		if (*p != 0xff) {	/* should decrypt to 0xff */
			if (*p == 0) {
				p++;
				break;
			} else {
				return -1;
			}
		}
		p++;
	}

	if (i == j)
		return -1;
	if (i < 8)
		return -1;
	i++;			/* Skip over the '\0' */
	j -= i;
	if (j > tlen)
		return -1;

	memcpy(to, p, (unsigned int)j);

	return j;
}

int padding_add_pkcs_type_2(unsigned char *to, int tlen,
		const unsigned char *from, int flen)
{
	int i, j;
	unsigned char *p;

	if (flen > (tlen - 11))
		return 0;

	p = (unsigned char *)to;

	*(p++) = 0;
	*(p++) = 2;		/* Public Key BT (Block Type) */

	/* pad out with non-zero random data */
	j = tlen - 3 - flen;

	if (get_rand_bytes(p, j) <= 0)
		return 0;
	for (i = 0; i < j; i++) {
		if (*p == '\0')
			do {
				if (get_rand_bytes(p, 1) <= 0)
					return 0;
			} while (*p == '\0');
		p++;
	}

	*(p++) = '\0';
	memcpy(p, from, (unsigned int)flen);

	return 1;
}

int padding_check_pkcs_type_2(unsigned char *to, int tlen,
		const unsigned char *from, int flen, int num)
{
	int i, j;
	const unsigned char *p;

	p = from;
	if ((num != (flen + 1)) || (*(p++) != 02))
		return -1;
#ifdef PKCS_CHECK
	return num - 11;
#endif

	/* scan over padding data */
	j = flen - 1;		/* one for type. */
	for (i = 0; i < j; i++)
		if (*(p++) == 0)
			break;

	if (i == j)
		return -1;

	if (i < 8)
		return -1;
	i++;			/* Skip over the '\0' */
	j -= i;
	if (j > tlen)
		return -1;
	memcpy(to, p, (unsigned int)j);

	return j;
}
