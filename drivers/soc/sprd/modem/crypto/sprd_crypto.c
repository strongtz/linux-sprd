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

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <asm/page.h>
#include "sprd_pkcs_padding.h"
#include "sprd_rsassa.h"
#include "sprd_rsa.h"
#include "sprd_sha256.h"
#include "sprd_crypto.h"
#include "sprd_sha256_sw.h"

struct sprd_crypto_alg *sprd_crypto_alg;

sprd_crypto_err_t sprd_rsa_verify(const sprd_rsa_pubkey_t *pub_key,
		const unsigned char *dig, unsigned int dig_size,
		const unsigned char *sig, unsigned int sig_size,
		sprd_rsa_padding_t padding, int *result)
{
	sprd_crypto_err_t err = SPRD_CRYPTO_SUCCESS;
	unsigned char temp1[256] __aligned(8);
	unsigned char temp2[256] __aligned(8);
	unsigned int temp1_len, temp2_len;
	int is_valid;

	if (pub_key == NULL) {
		pr_err("Bad pub key(%p)!\n", pub_key);
		return SPRD_CRYPTO_INVALID_KEY;
	}

	if (dig == NULL) {
		pr_err("Bad dig(%p)!\n", dig);
		return SPRD_CRYPTO_INVALID_ARG;
	}

	if (sig == NULL || sig_size <= 0) {
		pr_err("Bad sig(%p) or sig_size(%d)!\n", sig, sig_size);
		return SPRD_CRYPTO_INVALID_ARG;
	}

	if (result == NULL) {
		pr_err("Bad result(%p)!\n", result);
		return SPRD_CRYPTO_INVALID_ARG;
	}

	*result = SPRD_VERIFY_FAILED;

	temp1_len = rsa_dec_public_key_without_padding(pub_key->e, pub_key->n,
			pub_key->n_len << 3, (unsigned char *)sig, temp1);

	switch (padding.type) {
	case SPRD_RSA_NOPAD:
		if (dig_size == 0) {
			memcpy((void *)dig, temp1, temp1_len);
			*result = temp1_len;
		} else {
			if (memcmp(temp1, dig, dig_size) != 0)
				*result = SPRD_VERIFY_FAILED;
			else
				*result = SPRD_VERIFY_SUCCESS;
		}
		err = SPRD_CRYPTO_SUCCESS;
		break;

	case SPRD_RSASSA_PKCS_V1_5:
		temp2_len = 256;
		err = sprd_pkcs_v1_5_decode(temp1, temp1_len,
				SPRD_RSASSA_EMSA, pub_key->n_len,
				temp2, &temp2_len, &is_valid);
		if (err != SPRD_CRYPTO_SUCCESS || is_valid == 0) {
			pr_err("rsa padding decode failed(0x%x), is_valid(%d)!\n",
					err, is_valid);
			*result = SPRD_VERIFY_FAILED;
			return err;
		}

		if (dig_size == 0) {
			memcpy((unsigned char *)dig, temp2, temp2_len);
			*result = temp2_len;
		} else {
			if (temp2_len != dig_size
				|| memcmp(temp2, dig, dig_size) != 0) {
				pr_err("temp_len(%d) is not equal dig_size(%d)",
						temp2_len, dig_size);
				*result = SPRD_VERIFY_FAILED;
			} else {
				*result = SPRD_VERIFY_SUCCESS;
			}
		}
		break;

	case SPRD_RSASSA_PSS:
		err = sprd_pss_decode(dig, dig_size,
				temp1, temp1_len,
				padding.pad.rsassa_pss.salt_len,
				padding.pad.rsassa_pss.type,
				padding.pad.rsassa_pss.mgf1_hash_type,
				(pub_key->n_len)*8 /*needs bitlen*/, result);
		break;

	default:
		pr_err("Bad padding type(%d)!\n", padding.type);
		err = SPRD_CRYPTO_INVALID_PADDING;
	}

	return err;
}

sprd_crypto_err_t sprd_crypto_verify(const sprd_rsa_pubkey_t *pub_key,
		const unsigned char *digest, unsigned int digest_size,
		const unsigned char *signature, unsigned int signature_size,
		sprd_rsa_padding_t padding, int *result)
{
	if (sprd_crypto_alg) {
		if (sprd_crypto_alg->verify &&
			((sprd_crypto_alg->supported_alg & SPRD_CRYPTO_VERIFY) ==
			SPRD_CRYPTO_VERIFY))
			return sprd_crypto_alg->verify(pub_key, digest,
					digest_size, signature, signature_size,
					padding, result);
	}

	return SPRD_CRYPTO_NOSUPPORT;
}
EXPORT_SYMBOL_GPL(sprd_crypto_verify);

void sprd_crypto_sha256(const unsigned char *message, unsigned int message_size,
		unsigned char *digest, unsigned int chunk_size)
{
	if (sprd_crypto_alg) {
		if (sprd_crypto_alg->sha256 &&
				((sprd_crypto_alg->supported_alg & SPRD_CRYPTO_SHA256) ==
				SPRD_CRYPTO_SHA256))
			return sprd_crypto_alg->sha256(message, message_size,
					digest, 0);
	}
}
EXPORT_SYMBOL_GPL(sprd_crypto_sha256);

static int sprd_crypto_probe(struct platform_device *pdev)
{
	struct sprd_crypto_device *sprd_crypto;
	struct sprd_crypto_alg *alg;
	const char *dev_name = "sprd_crypto";
	int err;

	sprd_crypto = kzalloc(sizeof(*sprd_crypto), GFP_KERNEL);
	if (!sprd_crypto) {
		err = -ENOMEM;
		pr_err("%s: alloc sprd_crypto failed: %d\n", __func__, err);
		return err;
	}

	sprd_crypto->p_dev = &pdev->dev;
	strcpy((char *)sprd_crypto->crypto_name, dev_name);
	alg = kzalloc(sizeof(*alg), GFP_KERNEL);
	if (!alg) {
		err = -ENOMEM;
		pr_err("%s: alloc sprd_crypto_alg failed: %d\n",
				__func__, err);
		goto err_alloc_alg;
	}

#ifdef CONFIG_SPRD_CRYPTO_TEST
	err = sprd_crypto_test();
	if (err) {
		pr_err("%s: SPRD CRYPTO test failure(%d)\n",
				__func__, err);
		goto err_alloc_alg;
	}
	pr_info("%s: SPRD CRYPTO test success\n", __func__);
#endif

#ifdef CONFIG_SPRD_CRYPTO_PERFORMANCE_TEST
	err = sprd_crypto_speed_test();
	if (err) {
		pr_err("%s: SPRD CRYPTO PERFORMANCE test failure(%d)\n",
				__func__, err);
		goto err_alloc_alg;
	}
	pr_info("%s: SPRD CRYPTO PERFORMANCE test success\n", __func__);
#endif

	sprd_crypto->alg = alg;
	sprd_crypto->alg->verify = sprd_crypto_verify;
#ifdef CONFIG_SPRD_CRYPTO_SW
	sprd_crypto->alg->sha256 = *SHA256_hash;
#else
	sprd_crypto->alg->sha256 = sha256_csum_wd;
#endif
	sprd_crypto->alg->supported_alg = SPRD_CRYPTO_VERIFY |
		SPRD_CRYPTO_SHA256;

	sprd_crypto_alg = sprd_crypto->alg;

	sprd_crypto->state = SPRD_CRYPTO_READY;

	platform_set_drvdata(pdev, sprd_crypto);

	return 0;

err_alloc_alg:
	kfree(sprd_crypto);
	return err;
}

static int sprd_crypto_remove(struct platform_device *pdev)
{
	struct sprd_crypto_device *sprd_crypto = platform_get_drvdata(pdev);

	if (sprd_crypto) {
		platform_set_drvdata(pdev, NULL);

		if (sprd_crypto->alg) {
			sprd_crypto->alg->verify = NULL;
			sprd_crypto->alg->sha256 = NULL;
			kfree(sprd_crypto->alg);
		}

		sprd_crypto_alg = NULL;
		kfree(sprd_crypto);
	}

	return 0;
}

static const struct of_device_id sprd_crypto_of_match[] = {
	{ .compatible = "sprd,crypto_modem_verify", },
	{},
};

static struct platform_driver sprd_crypto_driver = {
	.probe = sprd_crypto_probe,
	.remove = sprd_crypto_remove,
	.driver = {
		.name = "sprd-crypto",
		.owner = THIS_MODULE,
		.of_match_table = sprd_crypto_of_match,
	},
};

static int __init sprd_crypto_init(void)
{
	return platform_driver_register(&sprd_crypto_driver);
}

static void __exit sprd_crypto_exit(void)
{
	platform_driver_unregister(&sprd_crypto_driver);
}

module_init(sprd_crypto_init);
module_exit(sprd_crypto_exit);

MODULE_AUTHOR("Shijiu Ren");
MODULE_DESCRIPTION("Crypto driver for Verifying modem image");
MODULE_LICENSE("GPL v2");
