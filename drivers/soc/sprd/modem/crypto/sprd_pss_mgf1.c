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

#include <linux/printk.h>
#include <linux/string.h>
#include "sprd_crypto.h"
#include "sprd_sha256.h"
#include "sprd_pss_mgf1.h"

#define MGF1_HASH_MIN SPRD_CRYPTO_HASH_SHA1
#define MGF1_HASH_MAX SPRD_CRYPTO_HASH_SHA256

#define MGF1_STORE32H(x, y)                      \
	do {                                        \
		(y)[0] = (unsigned char)(((x)>>24)&255); \
		(y)[1] = (unsigned char)(((x)>>16)&255); \
		(y)[2] = (unsigned char)(((x)>>8)&255);  \
		(y)[3] = (unsigned char)((x)&255);       \
	} while (0)

static unsigned char buf[SPRD_HASH_MAX_HASH_SIZE] __aligned(8);
static unsigned char tmp[SPRD_HASH_MAX_HASH_SIZE+4] __aligned(8);

/*
 * Perform PKCS #1 MGF1 (internal)
 * param hash_idx    The index of the hash desired
 * param seed        The seed for MGF1
 * param seedlen     The length of the seed
 * param mask        [out] The destination
 * param masklen     The length of the mask desired
 * return CRYPT_OK if successful
 */
unsigned int sprd_pss_mgf1(sprd_crypto_algo_t hash_type,
		const unsigned char *seed, unsigned long seedlen,
		unsigned char *mask, unsigned long masklen)
{
	unsigned long hLen, x;
	unsigned int counter;

	if (seed == NULL || mask == NULL) {
		pr_err("seed or mask invalid\n");
		return SPRD_CRYPTO_INVALID_ARG;
	}

	/* ensure valid hash */
	if (hash_type < MGF1_HASH_MIN || hash_type > MGF1_HASH_MAX) {
		pr_err("hash_type invalid\n");
		return SPRD_CRYPTO_INVALID_ARG;
	}

	/* get hash output size */
	hLen = SPRD_HASH_SIZE(hash_type);

	memset(buf, 0, SPRD_HASH_MAX_HASH_SIZE);
	memset(tmp, 0, SPRD_HASH_MAX_HASH_SIZE+4);

	/*we asume seed is hash result*/
	if (seedlen > (SPRD_HASH_MAX_HASH_SIZE)) {
		pr_err("seedlen larger than max modulus size\n");
		return SPRD_CRYPTO_INVALID_ARG;
	}
	memcpy(tmp, seed, seedlen);

	/* start counter */
	counter = 0;

	while (masklen > 0) {
		/* handle counter */
		MGF1_STORE32H(counter, tmp+seedlen);
		++counter;

		sha256_csum_wd(tmp, seedlen+4, buf, 0);

		/* store it */
		for (x = 0; x < hLen && masklen > 0; x++, masklen--)
			*mask++ = buf[x];
	}

	return SPRD_CRYPTO_SUCCESS;
}
