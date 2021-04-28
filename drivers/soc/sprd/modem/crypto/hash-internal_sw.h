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

#ifndef SYSTEM_CORE_INCLUDE_MINCRYPT_HASH_INTERNAL_H_
#define SYSTEM_CORE_INCLUDE_MINCRYPT_HASH_INTERNAL_H_

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

	struct HASH_CTX;  // forward decl

	typedef struct HASH_VTAB {
		void (*const init)(struct HASH_CTX *);
		void (*const update)(struct HASH_CTX *, const void *, int);
		const uint8_t* (*const final)(struct HASH_CTX *);
		void (*const hash)(const unsigned char *, unsigned int, unsigned char *, unsigned int);
		int size;
	} HASH_VTAB;

	typedef struct HASH_CTX {
		const HASH_VTAB *f;
		uint64_t count;
		uint8_t buf[64];
		uint32_t state[8];  // upto SHA2
	} HASH_CTX;

#define HASH_init(ctx) ((ctx)->f->init(ctx))
#define HASH_update(ctx, data, len) ((ctx)->f->update(ctx, data, len))
#define HASH_final(ctx) ((ctx)->f->final(ctx))
#define HASH_hash(data, len, digest) ((ctx)->f->hash(data, len, digest))
#define HASH_size(ctx) ((ctx)->f->size)

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // SYSTEM_CORE_INCLUDE_MINCRYPT_HASH_INTERNAL_H_
