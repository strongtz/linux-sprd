/*
 * Copyright (c) 2017, Spreadtrum Communications.
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
#include "sprd_rsassa.h"
#include "sprd_pss_mgf1.h"

#define PSS_HASH_MIN SPRD_CRYPTO_HASH_SHA1
#define PSS_HASH_MAX SPRD_CRYPTO_HASH_SHA256

static unsigned char db[SPRD_CRYPTO_MAX_RSA_SIZE] __aligned(8);
static unsigned char mask[SPRD_CRYPTO_MAX_RSA_SIZE*2] __aligned(8);
static unsigned char hash[SPRD_HASH_MAX_HASH_SIZE] __aligned(8);

/*
 * PKCS #1 v2.00 PSS decode
 * @param  msghash         The hash to verify
 * @param  msghashlen      The length of the hash (octets)
 * @param  sig             The signature data (encoded data)
 * @param  siglen          The length of the signature data (octets)
 * @param  saltlen         The length of the salt used (octets)
 * @param  hash_type       The index of the hash desired
 * @param  mgf1_hash_type  The index of the mgf1 hash desired
 * @param  modulus_bitlen  The bit length of the RSA modulus
 * @param  res: [out] The result of the comparison, 1==valid, 0==invalid
 * @return CRYPT_OK if successful (even if the comparison failed)
 */
unsigned int sprd_pss_decode(const unsigned char *msghash,
		unsigned int msghashlen, const unsigned char *sig,
		unsigned int siglen, unsigned int saltlen,
		sprd_crypto_algo_t hash_type, sprd_crypto_algo_t mgf1_hash_type,
		unsigned int modulus_bitlen, int *res)
{
	unsigned int x, y, hLen, modulus_len;
	unsigned int err;

	if (msghash == NULL || res == NULL) {
		pr_err("msghash or res invalid\n");
		return SPRD_CRYPTO_INVALID_ARG;
	}

	/* default to invalid */
	*res = SPRD_VERIFY_FAILED;

	/* ensure hash is valid */
	if (hash_type < PSS_HASH_MIN || hash_type > PSS_HASH_MAX) {
		pr_err("hash_type invalid\n");
		return SPRD_CRYPTO_INVALID_ARG;
	}

	hLen = SPRD_HASH_SIZE(hash_type);
	modulus_bitlen--;
	modulus_len = (modulus_bitlen>>3) + (modulus_bitlen & 7 ? 1 : 0);

	/* check sizes */
	if ((saltlen > modulus_len) ||
			(modulus_len < hLen + saltlen + 2)) {
		return SPRD_CRYPTO_INVALID_ARG;
	}

	memset(db, 0, SPRD_CRYPTO_MAX_RSA_SIZE);
	memset(mask, 0, SPRD_CRYPTO_MAX_RSA_SIZE*2);
	memset(hash, 0, SPRD_HASH_MAX_HASH_SIZE);

	/* ensure the 0xBC byte */
	if (sig[siglen-1] != 0xBC) {
		err = SPRD_CRYPTO_INVALID_PACKET;
		goto LBL_ERR;
	}

	/* copy out the db */
	x = 0;
	memcpy(db, sig + x, modulus_len - hLen - 1);
	x += modulus_len - hLen - 1;

	/* copy out the hash */
	memcpy(hash, sig + x, hLen);
	/* x += hLen; */

	/* check the MSB */
	if ((sig[0] & ~(0xFF >> ((modulus_len<<3) - (modulus_bitlen)))) != 0) {
		err = SPRD_CRYPTO_INVALID_PACKET;
		goto LBL_ERR;
	}

	/* generate mask of length modulus_len - hLen - 1 from hash */
	err = sprd_pss_mgf1(mgf1_hash_type, hash,
			hLen, mask, modulus_len-hLen-1);
	if (err != SPRD_CRYPTO_SUCCESS)
		goto LBL_ERR;

	/* xor against db */
	for (y = 0; y < (modulus_len - hLen - 1); y++)
		db[y] ^= mask[y];

	/* now clear the first byte [make sure smaller than modulus] */
	db[0] &= 0xFF >> ((modulus_len<<3) - (modulus_bitlen));

	/* db = PS || 0x01 || salt, PS == modulus_len - saltlen - hLen - 2 zero bytes */

	/* check for zeroes and 0x01 */
	for (x = 0; x < modulus_len - saltlen - hLen - 2; x++) {
		if (db[x] != 0x00) {
			err = SPRD_CRYPTO_INVALID_PACKET;
			goto LBL_ERR;
		}
	}

	/* check for the 0x01 */
	if (db[x++] != 0x01) {
		err = SPRD_CRYPTO_INVALID_PACKET;
		goto LBL_ERR;
	}

	/* M = (eight) 0x00 || msghash || salt, mask = H(M) */
	memset(mask, 0, 8);
	/* construct 8zeros|msghash|salt in one buffer
	 * because the uboot version config data end to 1 by default*/
	memcpy(mask+8, msghash, msghashlen);
	memcpy(mask+8+msghashlen, db+x, saltlen);

	sha256_csum_wd(mask, 8+msghashlen+saltlen, mask, 0);

	/* mask == hash means valid signature */
	if (memcmp(mask, hash, hLen) == 0)
		*res = SPRD_VERIFY_SUCCESS;

	err = SPRD_CRYPTO_SUCCESS;

LBL_ERR:
	return err;
}
