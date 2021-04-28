/*
 * Copyright (c) 2019, Spreadtrum Communications.
 *
 * The above copyright notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The software implementation of rsa algorithm
 *
 */

#include <linux/printk.h>
#include <linux/string.h>
#include "sprd_pkcs_padding.h"
#include "sprd_rsa.h"

#define		BIGINT_MAXLEN 130
#define		DEC 10
#define		HEX 16

static void bignum_reverse(unsigned char *src, int len)
{
	int i = 0, j = len - 1;
	unsigned char tmp;

	while (i < j) {
		tmp = src[i];
		src[i] = src[j];
		src[j] = tmp;
		i += 1;
		j -= 1;
	}
}

typedef struct {
	int m_iLength32;
	unsigned int m_ulValue[BIGINT_MAXLEN];
} SBigInt;

SBigInt rsa_c, rsa_m, rsa_n, rsa_d, rsa_e;

static void rsa_init(void)
{
	int i;

	for (i = 0; i < BIGINT_MAXLEN; i++) {
		rsa_c.m_ulValue[i] = 0;
		rsa_d.m_ulValue[i] = 0;
		rsa_e.m_ulValue[i] = 0;
		rsa_m.m_ulValue[i] = 0;
		rsa_n.m_ulValue[i] = 0;
	}
	rsa_c.m_iLength32 = 1;
	rsa_d.m_iLength32 = 1;
	rsa_e.m_iLength32 = 1;
	rsa_m.m_iLength32 = 1;
	rsa_n.m_iLength32 = 1;
}

static int bignum_cmp(SBigInt *s_in1, SBigInt *s_in2)
{
	int i;

	if (s_in1->m_iLength32 > s_in2->m_iLength32)
		return 1;

	if (s_in1->m_iLength32 < s_in2->m_iLength32)
		return -1;

	for (i = s_in1->m_iLength32 - 1; i >= 0; i--) {
		if (s_in1->m_ulValue[i] > s_in2->m_ulValue[i])
			return 1;
		if (s_in1->m_ulValue[i] < s_in2->m_ulValue[i])
			return -1;
	}

	return 0;
}

static void bignum_move(SBigInt *out, SBigInt *in)
{
	int i;

	out->m_iLength32 = in->m_iLength32;

	for (i = 0; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = in->m_ulValue[i];
}

static void ulong_to_bignum(SBigInt *out, unsigned long ul_in)
{
	int i;

	if (ul_in > 0xffffffff) {
		out->m_iLength32 = 2;
		out->m_ulValue[1] = (unsigned long)(ul_in >> 32);
		out->m_ulValue[0] = (unsigned long)ul_in;
	} else {
		out->m_iLength32 = 1;
		out->m_ulValue[0] = (unsigned long)ul_in;
	}

	for (i = out->m_iLength32; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;
}

void bignum_add(SBigInt *out, SBigInt *s_in1, SBigInt *s_in2)
{
	unsigned long carry = 0;
	unsigned long sum = 0;
	int i;

	if (s_in1->m_iLength32 < s_in2->m_iLength32)
		out->m_iLength32 = s_in2->m_iLength32;
	else
		out->m_iLength32 = s_in1->m_iLength32;

	for (i = 0; i < out->m_iLength32; i++) {
		sum = s_in1->m_ulValue[i];
		sum = sum + s_in2->m_ulValue[i] + carry;
		out->m_ulValue[i] = (unsigned long)sum;
		carry = (unsigned long)(sum >> 32);
	}

	out->m_ulValue[out->m_iLength32] = carry;
	out->m_iLength32 += carry;
	for (i = out->m_iLength32; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;
}

void bignum_add_ulong(SBigInt *out, SBigInt *s_in, unsigned long ul_in)
{
	unsigned long sum;
	int i;

	bignum_move(out, s_in);
	sum = s_in->m_ulValue[0];
	sum += ul_in;
	out->m_ulValue[0] = (unsigned long)sum;

	if (sum > 0xffffffff) {
		i = 1;
		while (s_in->m_ulValue[i] == 0xffffffff) {
			out->m_ulValue[i] = 0;
			i++;
		}
		out->m_ulValue[i]++;
		if (out->m_iLength32 == i)
			out->m_iLength32++;
	}
}

void bignum_sub(SBigInt *out, SBigInt *s_in1, SBigInt *s_in2)
{
	unsigned long carry = 0;
	unsigned long num;
	int i;

	if (bignum_cmp(s_in1, s_in2) <= 0) {
		ulong_to_bignum(out, 0);
		return;
	}

	out->m_iLength32 = s_in1->m_iLength32;
	for (i = 0; i < s_in1->m_iLength32; i++) {
		if ((s_in1->m_ulValue[i] > s_in2->m_ulValue[i]) ||
				((s_in1->m_ulValue[i] == s_in2->m_ulValue[i]) &&
				 (carry == 0))) {
			out->m_ulValue[i] =
				s_in1->m_ulValue[i] - carry - s_in2->m_ulValue[i];
			carry = 0;
		} else {
			num = 0x100000000 + s_in1->m_ulValue[i];
			out->m_ulValue[i] =
				(unsigned long)(num - carry - s_in2->m_ulValue[i]);
			carry = 1;
		}
	}

	while (out->m_ulValue[out->m_iLength32 - 1] == 0)
		out->m_iLength32--;

	for (i = out->m_iLength32; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;
}

void bignum_sub_ulong(SBigInt *out, SBigInt *s_in, unsigned long ul_in)
{
	unsigned long num;
	int i;

	out->m_iLength32 = s_in->m_iLength32;
	if (s_in->m_ulValue[0] >= ul_in) {
		out->m_ulValue[0] = s_in->m_ulValue[0] - ul_in;
		for (i = 1; i < BIGINT_MAXLEN; i++)
			out->m_ulValue[i] = s_in->m_ulValue[i];
		return;
	}

	if (s_in->m_iLength32 == 1) {
		ulong_to_bignum(out, 0);
		return;
	}

	num = 0x100000000 + s_in->m_ulValue[0];
	out->m_ulValue[0] = (unsigned long)(num - ul_in);
	i = 1;
	while (s_in->m_ulValue[i] == 0) {
		out->m_ulValue[i] = 0xffffffff;
		i++;
	}
	out->m_ulValue[i] = s_in->m_ulValue[i] - 1;
	if (out->m_ulValue[i] == 0)
		out->m_iLength32--;
	for (i = out->m_iLength32; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;
}

void bignum_mul_ulong(SBigInt *out, SBigInt *s_in, unsigned long ul_in)
{
	unsigned long mul;
	unsigned long carry;
	int i;

	carry = 0;
	out->m_iLength32 = s_in->m_iLength32;
	for (i = 0; i < out->m_iLength32; i++) {
		mul = s_in->m_ulValue[i];
		mul = mul * ul_in + carry;
		out->m_ulValue[i] = (unsigned long)mul;
		carry = (unsigned long)(mul >> 32);
	}

	if (carry) {
		out->m_ulValue[out->m_iLength32] = carry;
		out->m_iLength32++;
	}
	for (i = out->m_iLength32; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;
}

void bignum_mul(SBigInt *out, SBigInt *s_in1, SBigInt *s_in2)
{
	unsigned long sum, mul = 0, carry = 0;
	int i, j, k;

	if (s_in2->m_iLength32 == 1) {
		bignum_mul_ulong(out, s_in1, s_in2->m_ulValue[0]);
		return;
	}
	out->m_iLength32 = s_in1->m_iLength32 + s_in2->m_iLength32 - 1;

	for (i = 0; i < out->m_iLength32; i++) {
		sum = carry;
		carry = 0;
		for (j = 0; j < s_in2->m_iLength32; j++) {
			k = i - j;
			if ((k >= 0) && (k < s_in1->m_iLength32)) {
				mul = s_in1->m_ulValue[k];
				mul = mul * s_in2->m_ulValue[j];
				carry += mul >> 32;
				mul = mul & 0xffffffff;
				sum += mul;
			}
		}
		carry += sum >> 32;
		out->m_ulValue[i] = (unsigned long)sum;
	}

	if (carry) {
		out->m_iLength32++;
		out->m_ulValue[out->m_iLength32 - 1] = (unsigned long)carry;
	}

	for (i = out->m_iLength32; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;
}

void bignum_div_ulong(SBigInt *out, SBigInt *s_in, unsigned long ul_in)
{
	unsigned long div, mul;
	unsigned long carry = 0;
	int i;

	out->m_iLength32 = s_in->m_iLength32;
	if (s_in->m_iLength32 == 1) {
		out->m_ulValue[0] = s_in->m_ulValue[0] / ul_in;
		return;
	}

	for (i = s_in->m_iLength32 - 1; i >= 0; i--) {
		div = carry;
		div = (div << 32) + s_in->m_ulValue[i];
		out->m_ulValue[i] = (unsigned long)(div / ul_in);
		mul = (div / ul_in) * ul_in;
		carry = (unsigned long)(div - mul);
	}

	if (out->m_ulValue[out->m_iLength32 - 1] == 0)
		out->m_iLength32--;

	for (i = out->m_iLength32; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;
}

void bignum_div(SBigInt *out, SBigInt *s_in1, SBigInt *s_in2)
{
	unsigned i, len;
	unsigned long num, div;
	SBigInt s_temp1, s_temp2, s_temp3;

	if (s_in2->m_iLength32 == 1) {
		bignum_div_ulong(out, s_in1, s_in2->m_ulValue[0]);
		return;
	}

	bignum_move(&s_temp1, s_in1);
	ulong_to_bignum(out, 0);
	while (bignum_cmp(&s_temp1, s_in2) >= 0) {
		div = s_temp1.m_ulValue[s_temp1.m_iLength32 - 1];
		num = s_in2->m_ulValue[s_in2->m_iLength32 - 1];
		len = s_temp1.m_iLength32 - s_in2->m_iLength32;

		if ((div == num) && (len == 0)) {
			bignum_add_ulong(out, out, 1);
			break;
		}

		if ((div <= num) && len) {
			len--;
			div = (div << 32) +
				s_temp1.m_ulValue[s_temp1.m_iLength32 - 2];
		}

		div = div / (num + 1);
		ulong_to_bignum(&s_temp2, div);

		if (len) {
			s_temp2.m_iLength32 += len;
			for (i = s_temp2.m_iLength32 - 1; i >= len; i--)
				s_temp2.m_ulValue[i] =
					s_temp2.m_ulValue[i - len];
			for (i = 0; i < len; i++)
				s_temp2.m_ulValue[i] = 0;
		}

		bignum_add(out, out, &s_temp2);
		bignum_mul(&s_temp3, s_in2, &s_temp2);
		bignum_sub(&s_temp1, &s_temp1, &s_temp3);
	}
}

void bignum_mod(SBigInt *out, SBigInt *s_in1, SBigInt *s_in2)
{
	SBigInt s_temp1, s_temp2;
	unsigned long div, num;
	unsigned i, len;

	bignum_move(out, s_in1);
	while (bignum_cmp(out, s_in2) >= 0) {
		div = out->m_ulValue[out->m_iLength32 - 1];
		num = s_in2->m_ulValue[s_in2->m_iLength32 - 1];
		len = out->m_iLength32 - s_in2->m_iLength32;

		if ((div == num) && (len == 0)) {
			bignum_sub(out, out, s_in2);
			break;
		}

		if ((div <= num) && len) {
			len--;
			div = (div << 32) +
				out->m_ulValue[out->m_iLength32 - 2];
		}

		div = div / (num + 1);
		ulong_to_bignum(&s_temp1, div);
		bignum_mul(&s_temp2, s_in2, &s_temp1);

		if (len) {
			s_temp2.m_iLength32 += len;
			for (i = s_temp2.m_iLength32 - 1; i >= len; i--)
				s_temp2.m_ulValue[i] =
					s_temp2.m_ulValue[i - len];
			for (i = 0; i < len; i++)
				s_temp2.m_ulValue[i] = 0;
		}

		bignum_sub(out, out, &s_temp2);
	}
}

unsigned long bignum_mod_ulong(SBigInt *s_in, unsigned long ul_in)
{
	unsigned long div;
	unsigned long carry = 0;
	int i;

	if (s_in->m_iLength32 == 1)
		return s_in->m_ulValue[0] % ul_in;

	for (i = s_in->m_iLength32 - 1; i >= 0; i--) {
		div = s_in->m_ulValue[i] + carry * 0x100000000;
		carry = (unsigned long)(div % ul_in);
	}

	return carry;
}

static void __bignum_read_radix(SBigInt *out,
		unsigned char ul_in[], int length)
{
	int i;

	memcpy(out->m_ulValue, ul_in, length * sizeof(unsigned int));

	for (i = length; i < BIGINT_MAXLEN; i++)
		out->m_ulValue[i] = 0;

	out->m_iLength32 = length;
}

static void bignum_read_radix(SBigInt *out,
		unsigned char *str, int str_length)
{
	bignum_reverse(str, str_length);
	__bignum_read_radix(out, str, str_length >> 2);
	/* recovery the original input */
	bignum_reverse(str, str_length);
}

static void rsa_trans(SBigInt *s_out, SBigInt *s_in,
		SBigInt *e_or_d, SBigInt *mod_n)
{
	int i, j, k;
	int n;
	unsigned long num;
	SBigInt s_temp1, s_temp2;

	k = e_or_d->m_iLength32 * 32 - 32;
	num = e_or_d->m_ulValue[e_or_d->m_iLength32 - 1];
	while (num) {
		num = num >> 1;
		k++;
	}

	bignum_move(s_out, s_in);
	for (i = k - 2; i >= 0; i--) {
		bignum_mul_ulong(&s_temp1, s_out,
				s_out->m_ulValue[s_out->m_iLength32 - 1]);
		bignum_mod(&s_temp1, &s_temp1, mod_n);

		for (n = 1; n < s_out->m_iLength32; n++) {
			for (j = s_temp1.m_iLength32; j > 0; j--)
				s_temp1.m_ulValue[j] = s_temp1.m_ulValue[j - 1];
			s_temp1.m_ulValue[0] = 0;
			s_temp1.m_iLength32++;
			bignum_mul_ulong(&s_temp2, s_out,
				s_out->m_ulValue[s_out->m_iLength32 - n - 1]);
			bignum_add(&s_temp1, &s_temp1, &s_temp2);
			bignum_mod(&s_temp1, &s_temp1, mod_n);
		}

		bignum_move(s_out, &s_temp1);
		if ((e_or_d->m_ulValue[i >> 5] >> (i & 31)) & 1) {
			bignum_mul_ulong(&s_temp1, s_in,
				s_out->m_ulValue[s_out->m_iLength32 - 1]);
			bignum_mod(&s_temp1, &s_temp1, mod_n);

			for (n = 1; n < s_out->m_iLength32; n++) {
				for (j = s_temp1.m_iLength32; j > 0; j--)
					s_temp1.m_ulValue[j] =
						s_temp1.m_ulValue[j - 1];
				s_temp1.m_ulValue[0] = 0;
				s_temp1.m_iLength32++;
				bignum_mul_ulong(&s_temp2, s_in,
					s_out->m_ulValue[s_out->m_iLength32 - n - 1]);
				bignum_add(&s_temp1, &s_temp1, &s_temp2);
				bignum_mod(&s_temp1, &s_temp1, mod_n);
			}

			bignum_move(s_out, &s_temp1);
		}
	}
}

static int __rsa_enc_private_key(void)
{
	if (bignum_cmp(&rsa_m, &rsa_n) >= 0)
		return -1;

	rsa_trans(&rsa_c, &rsa_m, &rsa_d, &rsa_n);

	return 1;
}

static int __rsa_dec_public_key(void)
{
	if (bignum_cmp(&rsa_c, &rsa_n) >= 0)
		return -1;

	rsa_trans(&rsa_m, &rsa_c, &rsa_e, &rsa_n);

	return 1;
}

static int __rsa_enc(void)
{
	if (bignum_cmp(&rsa_m, &rsa_n) >= 0)
		return -1;

	rsa_trans(&rsa_c, &rsa_m, &rsa_e, &rsa_n);

	return 1;
}

static int __rsa_dec(void)
{
	if (bignum_cmp(&rsa_c, &rsa_n) >= 0)
		return -1;

	rsa_trans(&rsa_m, &rsa_c, &rsa_d, &rsa_n);

	return 1;
}


int rsa_enc(unsigned char *e, unsigned char *n, int n_bit_len,
		unsigned char *from, int flen, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;
	unsigned char text_padded[n_byte_len];

	rsa_init();

	if (e != NULL) {
		bignum_read_radix(&rsa_e, e, 4);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	if (padding_add_pkcs_type_2(text_padded, n_byte_len, from, flen) <= 0)
		return false;

	bignum_read_radix(&rsa_m, text_padded, n_byte_len);

	if (__rsa_enc() < 0)
		return false;

	memcpy(to, (unsigned char *)rsa_c.m_ulValue,
			rsa_c.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(to, n_byte_len);

	return true;
}

int rsa_enc_without_padding(unsigned char *e, unsigned char *n,
		int n_bit_len, unsigned char *from, int flen, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;

	rsa_init();

	if (e != NULL) {
		bignum_read_radix(&rsa_e, e, 4);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	bignum_read_radix(&rsa_m, from, flen);

	if (__rsa_enc() < 0)
		return false;

	memcpy(to, (unsigned char *)rsa_c.m_ulValue,
			rsa_c.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(to, n_byte_len);

	return true;
}

int rsa_dec(unsigned char *d, unsigned char *n, int n_bit_len,
		unsigned char *from, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;
	unsigned char text[n_byte_len];

	rsa_init();

	if (d != NULL) {
		bignum_read_radix(&rsa_d, d, n_byte_len);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	bignum_read_radix(&rsa_c, from, n_byte_len);

	if (__rsa_dec() < 0)
		return false;

	memcpy(text, (unsigned char *)rsa_m.m_ulValue,
			rsa_m.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(text, n_byte_len);

	return padding_check_pkcs_type_2(to, n_byte_len,
			text + 1, n_byte_len - 1, n_byte_len);
}

int rsa_dec_without_padding(unsigned char *d, unsigned char *n,
		int n_bit_len, unsigned char *from, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;

	rsa_init();

	if (d != NULL) {
		bignum_read_radix(&rsa_d, d, n_byte_len);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	bignum_read_radix(&rsa_c, from, n_byte_len);

	if (__rsa_dec() < 0)
		return false;

	memcpy(to, (unsigned char *)rsa_m.m_ulValue,
			rsa_m.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(to, n_byte_len);

	return n_byte_len;
}

int rsa_enc_private_key(unsigned char *d, unsigned char *n, int n_bit_len,
		unsigned char *from, int flen, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;
	unsigned char text_padded[n_byte_len];

	rsa_init();

	if (d != NULL) {
		bignum_read_radix(&rsa_d, d, n_byte_len);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	if (padding_add_pkcs_type_1(text_padded, n_byte_len, from, flen) <= 0)
		return false;

	bignum_read_radix(&rsa_m, text_padded, n_byte_len);

	if (__rsa_enc_private_key() < 0)
		return false;

	memcpy(to, (unsigned char *)rsa_c.m_ulValue,
			rsa_c.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(to, n_byte_len);

	return true;
}

int rsa_enc_private_key_without_padding(unsigned char *d, unsigned char *n,
		int n_bit_len, unsigned char *from, int flen, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;

	rsa_init();

	if (d != NULL) {
		bignum_read_radix(&rsa_d, d, n_byte_len);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	bignum_read_radix(&rsa_m, from, flen);

	if (__rsa_enc_private_key() < 0)
		return false;

	memcpy(to, (unsigned char *)rsa_c.m_ulValue,
			rsa_c.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(to, n_byte_len);

	return true;
}

int rsa_dec_public_key(unsigned char *e, unsigned char *n, int n_bit_len,
		unsigned char *from, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;
	unsigned char text[n_byte_len];

	rsa_init();

	if (e != NULL) {
		bignum_read_radix(&rsa_e, e, 4);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	bignum_read_radix(&rsa_c, from, n_byte_len);

	if (__rsa_dec_public_key() < 0)
		return false;

	memcpy(text, (unsigned char *)rsa_m.m_ulValue,
			rsa_m.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(text, n_byte_len);

	return padding_check_pkcs_type_1(to, n_byte_len,
			text + 1, n_byte_len - 1, n_byte_len);
}

int rsa_dec_public_key_without_padding(unsigned char *e, unsigned char *n,
		int n_bit_len, unsigned char *from, unsigned char *to)
{
	int n_byte_len = n_bit_len >> 3;

	rsa_init();

	if (e != NULL) {
		bignum_read_radix(&rsa_e, e, 4);
	} else {
		pr_err("Public key(e) is empty!\n");
		return false;
	}

	if (n != NULL) {
		bignum_read_radix(&rsa_n, n, n_byte_len);
	} else {
		pr_err("Modulus(n) is empty!\n");
		return false;
	}

	bignum_read_radix(&rsa_c, from, n_byte_len);

	if (__rsa_dec_public_key() < 0)
		return false;

	memcpy(to, (unsigned char *)rsa_m.m_ulValue,
			rsa_m.m_iLength32 * sizeof(unsigned int));

	bignum_reverse(to, n_byte_len);

	return n_byte_len;
}
