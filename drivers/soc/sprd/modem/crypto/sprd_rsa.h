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
 * The header file for software implementation of rsa algorithm.
 *
 */

#ifndef SPRD_RSA_H
#define SPRD_RSA_H

#define RSA_3	0x3L
#define RSA_F4	0x10001L

/*
 * Encrypt using private key with pkcs1 padding, Means signature
 * Parameter:
 * [IN]d:  private exponent ,length should be bitLen_N>>3 bytes
 * [IN]n:  modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: clear text to be signed,
 *           MAX length is bitLen_N>>3-11 due to will be padded using pkcs1
 * [IN]flen: data length of "from" buffer, MAX is  bitLen_N>>3-11
 * [OUT]to: signature, always be bitLen_N>>3 bytes length
 * Return value:
 *   true: sign success
 *   false: sign failed
 */
int rsa_enc_private_key(unsigned char *d, unsigned char *n, int n_bit_len,
		unsigned char *from, int flen, unsigned char *to);

/*
 * Encrypt using private key with no padding, Means signature
 * Parameter:
 * [IN]e:  private exponent ,length should be bitLen_N>>3 bytes
 * [IN]n:  modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: clear text to be signed, MAX length is bitLen_N>>3
 * [IN]flen: data length of "from" buffer, MAX is  bitLen_N>>3
 * [OUT]to: signature, always be bitLen_N>>3 bytes length
 * Return value:
 *   true: sign success
 *   false: sign failed
 */
int rsa_enc_private_key_without_padding(unsigned char *d, unsigned char *n,
		int n_bit_len, unsigned char *from,
		int flen, unsigned char *to);

/*
 * Decrypt using public key with pkcs1 padding,  Means verification
 * Parameter:
 * [IN]e: public expoenent, should be RSA_3 or RSA_F4,
 *        to be compatible with rsa->e from openssl,
 *        the pubExp should be BIG-ENDIAN like [0~3]={0x00,0x01,0x00,0x01}
 * [IN]n: modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: signature to be verified, always be bitLen_N>>3 bytes length
 * [OUT]to: clear text after public decryption
 * Return value:
 *   Bytes length of clear data in "to" buffer, MAX is  bitLen_N>>3-11
 */
int rsa_dec_public_key(unsigned char *e, unsigned char *n, int n_bit_len,
		unsigned char *from, unsigned char *to);

/*
 * Decrypt using public key with no padding, Means verification
 * Parameter:
 * [IN]e: public expoenent, should be RSA_3 or RSA_F4,
 *        to be compatible with rsa->e from openssl,
 *        the pubExp should be BIG-ENDIAN like [0~3]={0x00,0x01,0x00,0x01}
 * [IN]n: modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: signature to be verified, always be bitLen_N>>3 bytes length
 * [OUT]to: clear text after public decryption
 * Return value:
 *   Bytes length of clear data in "to" buffer, MAX is  bitLen_N>>3
 */
int rsa_dec_public_key_without_padding(unsigned char *e, unsigned char *n,
		int n_bit_len, unsigned char *from, unsigned char *to);

/*
 * Encrypt using public key with pkcs1 padding.
 * Parameter:
 * [IN]e: public expoenent,should be RSA_3 or RSA_F4,
 *        to be compatible with rsa->e from openssl,
 *        the pubExp should be BIG-ENDIAN like [0~3]={0x00,0x01,0x00,0x01}
 * [IN]n: modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: clear text to be signed,
 *           MAX length is bitLen_N>>3-11 due to will be padded using pkcs1
 * [IN]int flen: data length of "from" buffer, MAX is  bitLen_N>>3-11
 * [OUT]to: cipher data, always be bitLen_N>>3 bytes length
 * Return value:
 *   true: encrypt success
 *   false: encrypt failed
 */
int rsa_enc(unsigned char *e, unsigned char *n, int n_bit_len,
		unsigned char *from, int flen, unsigned char *to);

/*
 * Encrypt using public key with no padding.
 * Parameter:
 * [IN]e: public expoenent,should be RSA_3 or RSA_F4,
 *        to be compatible with rsa->e from openssl,
 *        the pubExp should be BIG-ENDIAN like [0~3]={0x00,0x01,0x00,0x01}
 * [IN]n: modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: clear text to be signed, MAX length is bitLen_N>>3
 * [IN]int flen: data length of "from" buffer, MAX is  bitLen_N>>3
 * [OUT]to: cipher data, always be bitLen_N>>3 bytes length
 * Return value:
 *   true: encrypt success
 *   false: encrypt failed
 */
int rsa_enc_without_padding(unsigned char *e, unsigned char *n, int n_bit_len,
		unsigned char *from, int flen, unsigned char *to);

/*
 * Decrypt using private key with pkcs1 padding
 * Parameter:
 * [IN]d: private exponent ,length should be bitLen_N>>3 bytes
 * [IN]n: modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: cipher data to be decrypted, always be bitLen_N>>3 bytes length
 * [OUT]to: clear text after public decryption
 * Return value:
 *   Bytes length of clear data in "to" buffer, MAX is  bitLen_N>>3-11
 */
int rsa_dec(unsigned char *d, unsigned char *n, int n_bit_len,
		unsigned char *from, unsigned char *to);

/*
 * Decrypt using private key with no padding
 * Parameter:
 * [IN]d: private exponent ,length should be bitLen_N>>3 bytes
 * [IN]n: modulus ,length should be bitLen_N>>3 bytes
 * [IN]n_bit_len: bit length of mod_N
 * [IN]from: cipher data to be decrypted, always be bitLen_N>>3 bytes length
 * [OUT]to: clear text after public decryption
 * Return value:
 *   Bytes length of clear data in "to" buffer, MAX is  bitLen_N>>3-11
 */
int rsa_dec_without_padding(unsigned char *d, unsigned char *n, int n_bit_len,
		unsigned char *from, unsigned char *to);

#endif
