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

#ifndef SYSTEM_CORE_INCLUDE_MINCRYPT_SHA256_H_
#define SYSTEM_CORE_INCLUDE_MINCRYPT_SHA256_H_

#include <linux/types.h>
#include "hash-internal_sw.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

	typedef HASH_CTX SHA256_CTX;

	void SHA256_init(SHA256_CTX *ctx);
	void SHA256_update(SHA256_CTX *ctx, const void *data, int len);
	const uint8_t *SHA256_final(SHA256_CTX *ctx);

	// Convenience method. Returns digest address.

void SHA256_hash(const unsigned char *input, unsigned int ilen, unsigned char *output, unsigned int chunk_size);
#define SHA256_DIGEST_SIZE 32

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  // SYSTEM_CORE_INCLUDE_MINCRYPT_SHA256_H_
