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
#include "sprd_rsassa.h"

/** PKCS #1 v1.5 decode.
 *
 *  msg              The encoded data to decode
 *  msglen           The length of the encoded data (octets)
 *  block_type       Block type to use in padding
 *  modulus_bitlen   The bit length of the RSA modulus
 *  out              [out] Destination of decoding
 *  outlen           [in/out] The max size and resulting size of the decoding
 *  is_valid         [out] Boolean whether the padding was valid
 *
 *  SPRD_CRYPTO_SUCCESS if successful (even if invalid)
 */
unsigned int sprd_pkcs_v1_5_decode(const unsigned char *msg,
		unsigned int msglen,
		int block_type,
		unsigned int  modulus_len,
		unsigned char *out,
		unsigned int  *outlen,
		int *is_valid)
{
	unsigned int ps_len, i;
	sprd_crypto_err_t err = SPRD_CRYPTO_SUCCESS;

	if (msg == NULL || msglen == 0) {
		pr_err("msg is %p or msglen is %d\n", msg, msglen);
		return SPRD_CRYPTO_INVALID_ARG;
	}

	if (is_valid == NULL) {
		pr_err("is_valid is NULL\n");
		return SPRD_CRYPTO_INVALID_ARG;
	}

	/* default to invalid packet */
	*is_valid = 0;

	/* test message size */
	if ((msglen > modulus_len) || (modulus_len < 11)) {
		pr_err("msglen(%d) is too big or too small\n", msglen);
		return SPRD_CRYPTO_INVALID_PACKET;
	}

	/* separate encoded message */
	if ((msg[0] != 0x00) || (msg[1] != (unsigned char)block_type)) {
		pr_err("bad msg[0](0x%x) or msg[1](0x%x)\n", msg[0], msg[1]);
		return SPRD_CRYPTO_INVALID_PACKET;
	}

	switch (block_type) {
	case SPRD_RSASSA_EME:
		for (i = 2; i < modulus_len; i++) {
		/* separator */
			if (msg[i] == 0x00)
				break;
		}
		break;

	case SPRD_RSASSA_EMSA:
		for (i = 2; i < modulus_len; i++) {
			if (msg[i] != 0xFF)
				break;
		}

		/* separator check */
		if (msg[i] != 0x00) {
			pr_err("There was no 0x00 in msg(msg[%d] = 0x%x)\n",
					i, msg[i]);
			return SPRD_CRYPTO_INVALID_PACKET;
		}
		break;

	default:
		pr_err("invalid block type in padding(0x%x)\n", block_type);
		return SPRD_CRYPTO_INVALID_PADDING;
	}

	ps_len = i++ - 2;
	if (i >= modulus_len) {
		pr_err("There was no 0x00 in msg(msg[%d] = 0x%x)\n",
				i, msg[i]);
		return SPRD_CRYPTO_INVALID_PACKET;
	}

	if (ps_len < 8) {
		pr_err("the length of ps(%d) is less than 8\n", ps_len);
		return SPRD_CRYPTO_INVALID_PACKET;
	}

	if (out == NULL || outlen == NULL
			|| *outlen < (msglen - (2 + ps_len + 1))) {
		pr_err("out(%p) or outlen(%p) is less than required\n",
				out, outlen);
		return SPRD_CRYPTO_INVALID_ARG;
	}

	*outlen = (msglen - (2 + ps_len + 1));
	memcpy(out, &msg[2 + ps_len + 1], *outlen);
	*is_valid = 1;

	return err;
}

