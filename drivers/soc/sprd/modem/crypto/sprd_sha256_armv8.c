/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgment:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>

#define SHA256_CBLOCK 64
#define SHA256_DIGEST_LENGTH 32

#define HOST_l2c(l, c)                        \
	do {                                        \
		*((c)++) = (unsigned char)(((l) >> 24) & 0xff); \
		*((c)++) = (unsigned char)(((l) >> 16) & 0xff); \
		*((c)++) = (unsigned char)(((l) >> 8) & 0xff);  \
		*((c)++) = (unsigned char)(((l)) & 0xff);       \
	} while (0)

#define HASH_MAKE_STRING(c, s)                               \
	do {                                                     \
		unsigned int ll;                                     \
		unsigned int nn;                                     \
		switch ((c)->md_len) {                               \
		case SHA256_DIGEST_LENGTH:                           \
			for (nn = 0; nn < SHA256_DIGEST_LENGTH/4; nn++) {\
				ll = (c)->h[nn];                             \
				HOST_l2c(ll, (s));                           \
			}                                                \
			break;                                           \
		default:                                             \
			break;                                           \
		}                                                    \
	} while (0)

struct sha256_state_st {
	unsigned int h[8];
	unsigned int Nl, Nh;
	unsigned char data[SHA256_CBLOCK];
	unsigned int num, md_len;
};

typedef struct sha256_state_st SHA256_CTX;

extern void sprd_sha256_block_data_order(unsigned int *state,
		const unsigned char *in, unsigned int num);

int SHA256_Init(SHA256_CTX *sha)
{
	memset(sha, 0, sizeof(SHA256_CTX));
	sha->h[0] = 0x6a09e667UL;
	sha->h[1] = 0xbb67ae85UL;
	sha->h[2] = 0x3c6ef372UL;
	sha->h[3] = 0xa54ff53aUL;
	sha->h[4] = 0x510e527fUL;
	sha->h[5] = 0x9b05688cUL;
	sha->h[6] = 0x1f83d9abUL;
	sha->h[7] = 0x5be0cd19UL;
	sha->md_len = SHA256_DIGEST_LENGTH;
	return 1;
}

int SHA256_Update(SHA256_CTX *c, const void *data_, unsigned int len)
{
	const unsigned char *data = data_;
	unsigned int l, n;

	if (len == 0)
		return 1;

	l = c->Nl + (((unsigned int)len) << 3);
	if (l < c->Nl)
		/* Handle carries. */
		c->Nh++;
	c->Nh += (unsigned int)(len >> 29);
	c->Nl = l;

	n = c->num;
	if (n != 0) {
		if (len >= SHA256_CBLOCK || len + n >= SHA256_CBLOCK) {
			memcpy(c->data + n, data, SHA256_CBLOCK - n);
			sprd_sha256_block_data_order(c->h, c->data, 1);
			n = SHA256_CBLOCK - n;
			data += n;
			len -= n;
			c->num = 0;
			/* Keep |c->data| zeroed when unused. */
			memset(c->data, 0, SHA256_CBLOCK);
		} else {
			memcpy(c->data + n, data, len);
			c->num += (unsigned int)len;
			return 1;
		}
	}

	n = len / SHA256_CBLOCK;
	if (n > 0) {
		sprd_sha256_block_data_order(c->h, data, n);
		n *= SHA256_CBLOCK;
		data += n;
		len -= n;
	}

	if (len != 0) {
		c->num = (unsigned int)len;
		memcpy(c->data, data, len);
	}

	return 1;
}

int SHA256_Final(unsigned char *md, SHA256_CTX *c)
{
	/* |c->data| always has room for at least one byte. A full block would have
	 * been consumed. */
	unsigned int n = c->num;
	unsigned char *p;

	//assert(n < SHA256_CBLOCK);
	c->data[n] = 0x80;
	n++;

	/* Fill the block with zeros if there isn't room for a 64-bit length. */
	if (n > (SHA256_CBLOCK - 8)) {
		memset(c->data + n, 0, SHA256_CBLOCK - n);
		n = 0;
		sprd_sha256_block_data_order(c->h, c->data, 1);
	}
	memset(c->data + n, 0, SHA256_CBLOCK - 8 - n);

	/* Append a 64-bit length to the block and process it. */
	p = c->data + SHA256_CBLOCK - 8;
	HOST_l2c(c->Nh, p);
	HOST_l2c(c->Nl, p);
	//assert(p == c->data + SHA256_CBLOCK);
	sprd_sha256_block_data_order(c->h, c->data, 1);
	c->num = 0;
	memset(c->data, 0, SHA256_CBLOCK);

	HASH_MAKE_STRING(c, md);

	return 1;
}

void sha256_csum_wd(const unsigned char *input, unsigned int ilen,
		unsigned char *output, unsigned int chunk_sz)
{
	SHA256_CTX ctx;

	if (input == NULL || ilen == 0 || output == NULL) {
		pr_err("%s:%d: Parameter error\n", __func__, __LINE__);
		return;
	}

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, input, ilen);
	SHA256_Final(output, &ctx);

	memset(&ctx, 0, sizeof(ctx));
}
