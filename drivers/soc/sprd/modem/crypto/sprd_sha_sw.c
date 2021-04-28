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
 */


// Optimized for minimal code size.

#include "sprd_sha_sw.h"

#include <linux/zlib.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/printk.h>

#define rol(bits, value) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void SHA1_Transform(SHA_CTX *ctx)
{
	uint32_t W[80];
	uint32_t A, B, C, D, E;
	uint8_t *p = ctx->buf;
	int t;

	for (t = 0; t < 16; ++t) {
		uint32_t tmp =  *p++ << 24;

		tmp |= *p++ << 16;
		tmp |= *p++ << 8;
		tmp |= *p++;
		W[t] = tmp;
	}

	for (; t < 80; t++)
		W[t] = rol(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

	A = ctx->state[0];
	B = ctx->state[1];
	C = ctx->state[2];
	D = ctx->state[3];
	E = ctx->state[4];

	for (t = 0; t < 80; t++) {
		uint32_t tmp = rol(5, A) + E + W[t];

		if (t < 20)
			tmp += (D^(B&(C^D))) + 0x5A827999;
		else if (t < 40)
			tmp += (B^C^D) + 0x6ED9EBA1;
		else if (t < 60)
			tmp += ((B&C)|(D&(B|C))) + 0x8F1BBCDC;
		else
			tmp += (B^C^D) + 0xCA62C1D6;

		E = D;
		D = C;
		C = rol(30, B);
		B = A;
		A = tmp;
	}

	ctx->state[0] += A;
	ctx->state[1] += B;
	ctx->state[2] += C;
	ctx->state[3] += D;
	ctx->state[4] += E;
}

static const HASH_VTAB SHA_VTAB = {
	SHA_init,
	SHA_update,
	SHA_final,
	SHA_hash,
	SHA_DIGEST_SIZE
};

void SHA_init(SHA_CTX *ctx)
{
	ctx->f = &SHA_VTAB;
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
	ctx->count = 0;
}

void SHA_update(SHA_CTX *ctx, const void *data, int len)
{
	int i = (int) (ctx->count & 63);
	const uint8_t *p = (const uint8_t *)data;

	ctx->count += len;

	while (len--) {
		ctx->buf[i++] = *p++;
		if (i == 64) {
			SHA1_Transform(ctx);
			i = 0;
		}
	}
}


const uint8_t *SHA_final(SHA_CTX *ctx)
{
	uint8_t *p = ctx->buf;
	uint64_t cnt = ctx->count * 8;
	int i;

	SHA_update(ctx, (uint8_t *)"\x80", 1);
	while ((ctx->count & 63) != 56)
		SHA_update(ctx, (uint8_t *)"\0", 1);
	for (i = 0; i < 8; ++i) {
		uint8_t tmp = (uint8_t) (cnt >> ((7 - i) * 8));

		SHA_update(ctx, &tmp, 1);
	}

	for (i = 0; i < 5; i++) {
		uint32_t tmp = ctx->state[i];

		*p++ = tmp >> 24;
		*p++ = tmp >> 16;
		*p++ = tmp >> 8;
		*p++ = tmp >> 0;
	}

	return ctx->buf;
}

/* Convenience function */
void SHA_hash(const unsigned char *input, unsigned int ilen, unsigned char *output, unsigned int chunk_size)
{
	SHA_CTX ctx;

	if (input == NULL || ilen == 0 || output == NULL) {
		pr_err("%s:%d: Parameter error\n", __func__, __LINE__);
		return;
	}

	SHA_init(&ctx);
	SHA_update(&ctx, input, ilen);
	memcpy(output, SHA_final(&ctx), SHA_DIGEST_SIZE);
}
