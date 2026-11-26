/*
 * cleanroom/crypto_tls.h
 * ---------------------
 * Clean-room crypto / TLS capability layer (P1 module).
 *
 * Wraps the standard open cryptographic primitives (OpenSSL EVP) behind the
 * capability names recovered during analysis:
 *   AES-128-CBC, AES-192-CBC, AES-256-CBC, AES-128-CTR, AES-192-CTR, AES-256-CTR
 *   ARC4, TLSv1 / TLSv1.1 / TLSv1.2, TLS_PSK_*, TLS_RSA_*, SSL_* / TLS_DHE_*
 */

#ifndef CLEANROOM_CRYPTO_TLS_H
#define CLEANROOM_CRYPTO_TLS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TLS_CRYPTO_AES_128_CBC = 0,
    TLS_CRYPTO_AES_192_CBC = 1,
    TLS_CRYPTO_AES_256_CBC = 2,
    TLS_CRYPTO_AES_128_CTR = 3,
    TLS_CRYPTO_AES_192_CTR = 4,
    TLS_CRYPTO_AES_256_CTR = 5
} tls_crypto_algo_t;

typedef enum {
    TLS_RET_OK = 0,
    TLS_RET_ERR = -1,
    TLS_RET_ERR_ALGO = -2,
    TLS_RET_ERR_KEY = -3
} tls_ret_t;

size_t tls_crypto_key_len(int algo);

/* AES-CBC decrypt (standard open AES).  returns 0 ok, negative on error. */
int tls_crypto_aes_cbc_decrypt(int algo, const uint8_t *key, const uint8_t *iv,
                               const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len);

/* AES-CTR crypto operation (standard open AES).  returns 0 ok, negative on error. */
int tls_crypto_aes_ctr(int algo, const uint8_t *key, const uint8_t *nonce,
                      const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len);

/* ARC4 (standard open primitive). */
int tls_crypto_arc4(const uint8_t *key, size_t key_len, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_CRYPTO_TLS_H */
