#ifndef CLEANROOM_CRYPTO_CTR_H
#define CLEANROOM_CRYPTO_CTR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <openssl/evp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTR_CRYPTO_AES_128 = 0,
    CTR_CRYPTO_AES_192 = 1,
    CTR_CRYPTO_AES_256 = 2
} ctr_crypto_algo_t;

typedef struct {
    int algo;
    EVP_CIPHER_CTX *ctx;
} ctr_crypto_t;

typedef enum {
    CTR_RET_OK = 0,
    CTR_RET_ERR = -1,
    CTR_RET_ERR_ALGO = -2
} ctr_ret_t;

ctr_ret_t ctr_crypto_init(ctr_crypto_t *c, int algo);
ctr_ret_t ctr_crypto_encrypt(const ctr_crypto_t *c, const uint8_t *key, const uint8_t *nonce,
                             const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len);
ctr_ret_t ctr_crypto_decrypt(const ctr_crypto_t *c, const uint8_t *key, const uint8_t *nonce,
                             const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len);
void ctr_crypto_free(ctr_crypto_t *c);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_CRYPTO_CTR_H */
