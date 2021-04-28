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

#ifndef SPRD_CRYPTO_H
#define SPRD_CRYPTO_H

#include <linux/types.h>

typedef int sprd_crypto_err_t;

enum sprd_crypto_status {
	/* Success */
	SPRD_CRYPTO_SUCCESS = 0,
	/* Generic Error */
	SPRD_CRYPTO_ERROR   = 0xffff0000,
	/* Scheme not support */
	SPRD_CRYPTO_NOSUPPORT,
	/* Invalid Key in asymmetric scheme: RSA/DSA/ECCP/DH etc */
	SPRD_CRYPTO_INVALID_KEY,
	/* Invalid aes_type/des_type/authenc_type/hash_type/cbcmac_type/cmac_type */
	SPRD_CRYPTO_INVALID_TYPE,
	/* Invalid context in multi-thread cipher/authenc/mac/hash etc */
	SPRD_CRYPTO_INVALID_CONTEXT,
	/* Invalid sym_padding/rsassa_padding/rsaes_padding */
	SPRD_CRYPTO_INVALID_PADDING,
	/* Invalid authentication in (AES-CCM/AES-GCM)/(RSA/DSA/ECCP DSA) */
	SPRD_CRYPTO_INVALID_AUTHENTICATION,
	/* Invalid arguments */
	SPRD_CRYPTO_INVALID_ARG,
	/* Invalid packet in asymmetric enc/dec(RSA) */
	SPRD_CRYPTO_INVALID_PACKET,
	/* Invalid Length in arguments */
	SPRD_CRYPTO_LENGTH_ERR,
	/* Memory alloc NULL */
	SPRD_CRYPTO_OUTOFMEM,
	/* Output buffer is too short to store result */
	SPRD_CRYPTO_SHORT_BUFFER,
	/* NULL pointer in arguments */
	SPRD_CRYPTO_NULL,
	/* Bad state in mulit-thread cipher/authenc/mac/hash etc */
	SPRD_CRYPTO_ERR_STATE,
	/* Bad result of test crypto */
	SPRD_CRYPTO_ERR_RESULT,
};

typedef unsigned int sprd_crypto_algo_t;

enum sprd_verify_res {
	SPRD_VERIFY_SUCCESS = 0,
	SPRD_VERIFY_FAILED,
};

#define SPRD_CRYPTO_HASH                0x02000000
#define SPRD_CRYPTO_HASH_MD5            (SPRD_CRYPTO_HASH + 0x0001)
#define SPRD_CRYPTO_HASH_SHA1           (SPRD_CRYPTO_HASH + 0x0002)
#define SPRD_CRYPTO_HASH_SHA224         (SPRD_CRYPTO_HASH + 0x0003)
#define SPRD_CRYPTO_HASH_SHA256         (SPRD_CRYPTO_HASH + 0x0004)
#define SPRD_CRYPTO_HASH_SHA384         (SPRD_CRYPTO_HASH + 0x0005)
#define SPRD_CRYPTO_HASH_SHA512         (SPRD_CRYPTO_HASH + 0x0006)
#define SPRD_CRYPTO_HASH_SHA512_224     (SPRD_CRYPTO_HASH + 0x0007)
#define SPRD_CRYPTO_HASH_SHA512_256     (SPRD_CRYPTO_HASH + 0x0008)
#define SPRD_CRYPTO_HASH_SM3            (SPRD_CRYPTO_HASH + 0x0009)

#define SPRD_CRYPTO_MAX_RSA_SIZE        256

enum {
	SPRD_HASH_MD5_SIZE                  = 16,
	SPRD_HASH_SHA1_SIZE                 = 20,
	SPRD_HASH_SHA224_SIZE               = 28,
	SPRD_HASH_SHA256_SIZE               = 32,
	SPRD_HASH_SHA384_SIZE               = 48,
	SPRD_HASH_SHA512_SIZE               = 64,
	SPRD_HASH_SHA512_224_SIZE           = 28,
	SPRD_HASH_SHA512_256_SIZE           = 32,
	SPRD_HASH_MAX_HASH_SIZE             = 64,
};

#define SPRD_HASH_SIZE(type)                                       \
	(((type) == (SPRD_CRYPTO_HASH_MD5)) ? (SPRD_HASH_MD5_SIZE) :   \
	(((type) == (SPRD_CRYPTO_HASH_SHA1)) ? (SPRD_HASH_SHA1_SIZE) : \
	(((type) == (SPRD_CRYPTO_HASH_SHA224)) ? (SPRD_HASH_SHA224_SIZE) : \
	(((type) == (SPRD_CRYPTO_HASH_SHA256)) ? (SPRD_HASH_SHA256_SIZE) : \
	(((type) == (SPRD_CRYPTO_HASH_SHA384)) ? (SPRD_HASH_SHA384_SIZE) : \
	(((type) == (SPRD_CRYPTO_HASH_SHA512)) ? (SPRD_HASH_SHA512_SIZE) : \
	(((type) == (SPRD_CRYPTO_HASH_SHA512_224)) ? \
	 (SPRD_HASH_SHA512_224_SIZE) : \
	(((type) == (SPRD_CRYPTO_HASH_SHA512_256)) ? \
	 (SPRD_HASH_SHA512_256_SIZE) : \
	(0)))))))))

typedef enum {
	SPRD_RSA_NOPAD              = 0,

	SPRD_RSAES_PKCS_V1_5        = 10,
	SPRD_RSAES_OAEP             = 11,

	SPRD_RSASSA_PKCS_V1_5       = 20,
	SPRD_RSASSA_PSS             = 21,
} sprd_rsa_pad_type_t;

typedef struct {
	sprd_rsa_pad_type_t type;
	union {
		struct {
			sprd_crypto_algo_t type;
			const unsigned char *lparam;
			unsigned int lparamlen;
		} rsaes_oaep;

		struct {
			/* md5/sha1/sha224/sha256/sha384/sha512 */
			sprd_crypto_algo_t type;
		} rsassa_v1_5;

		struct {
			/* sha1/sha224/sha256/sha384/sha512 */
			sprd_crypto_algo_t type;
			/* sha1/sha224/sha256/sha384/sha512 */
			sprd_crypto_algo_t mgf1_hash_type;
			unsigned int salt_len;
		} rsassa_pss;
	} pad;
} sprd_rsa_padding_t;

typedef struct {
	unsigned char *n;       /* Modulus */
	unsigned char *e;       /* Public exponent */
	unsigned int  n_len;
	unsigned int  e_len;
} sprd_rsa_pubkey_t;


struct sprd_crypto_alg {
	sprd_crypto_err_t (*verify)(const sprd_rsa_pubkey_t *pub_key,
			const unsigned char *digest,
			unsigned int digest_size,
			const unsigned char *signature,
			unsigned int signature_size,
			sprd_rsa_padding_t padding,
			int *result);

	void (*sha256)(const unsigned char *message,
			unsigned int message_size,
			unsigned char *digest,
			unsigned int chunk_size);

	unsigned int supported_alg;
};

/* supported algorithm type */
enum  {
	SPRD_CRYPTO_VERIFY = 0,
	SPRD_CRYPTO_SHA256,
	SPRD_CRYPTO_ALG_NR,
};

/* sprd_crypto_device state */
enum {
    SPRD_CRYPTO_NOT_INIT = 0,
    SPRD_CRYPTO_READY,
};

#define MAX_DEV_NAME_LEN     32

struct sprd_crypto_device {
	const char               crypto_name[MAX_DEV_NAME_LEN];
	unsigned int             state;
	struct sprd_crypto_alg   *alg;
	struct device            *p_dev;
};

/* crypto test */
#ifdef CONFIG_SPRD_CRYPTO_TEST
int sprd_crypto_test(void);
#endif
#ifdef CONFIG_SPRD_CRYPTO_PERFORMANCE_TEST
int sprd_crypto_speed_test(void);
#endif
#endif /* SPRD_CRYPTO_H */
