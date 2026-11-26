/*
 * cleanroom/crypto_ctr.c
 * ---------------------
 * Clean-room AES-CTR real encrypt/decrypt operations via OpenSSL EVP (P4).
 */

#include "crypto_ctr.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

static const EVP_CIPHER *ctr_crypto_evp(int algo)
{
    switch (algo) {
    case CTR_CRYPTO_AES_128: return EVP_aes_128_ctr();
    case CTR_CRYPTO_AES_192: return EVP_aes_192_ctr();
    case CTR_CRYPTO_AES_256: return EVP_aes_256_ctr();
    default: return NULL;
    }
}

ctr_ret_t ctr_crypto_init(ctr_crypto_t *c, int algo)
{
    const EVP_CIPHER *evp = NULL;

    if ((c == NULL) || (c->ctx != NULL)) { return CTR_RET_ERR; }
    evp = ctr_crypto_evp(algo);
    if (evp == NULL) { return CTR_RET_ERR_ALGO; }
    c->algo = algo;
    c->ctx = EVP_CIPHER_CTX_new();
    if (c->ctx == NULL) { return CTR_RET_ERR; }
    return CTR_RET_OK;
}

static int ctr_run(const ctr_crypto_t *c, const uint8_t *key, const uint8_t *nonce,
                   const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len, int encrypt)
{
    EVP_CIPHER_CTX *ctx = NULL;

    if ((c == NULL) || (c->ctx == NULL) || (key == NULL) || (in == NULL) || (out == NULL)) { return CTR_RET_ERR; }
    if (out_len < in_len) { return CTR_RET_ERR; }
    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) { return CTR_RET_ERR; }
    if (EVP_CipherInit_ex(ctx, ctr_crypto_evp(c->algo), NULL, key, nonce, encrypt) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return CTR_RET_ERR;
    }
    if (in_len > 0) {
        int out_len_actual = 0;
        int rc = encrypt ? EVP_EncryptUpdate(ctx, out, &out_len_actual, in, (int)in_len)
                         : EVP_DecryptUpdate(ctx, out, &out_len_actual, in, (int)in_len);
        if (rc != 1) { EVP_CIPHER_CTX_free(ctx); return CTR_RET_ERR; }
    }
    {
        int pad = 0;
        if (encrypt) { EVP_EncryptFinal_ex(ctx, out, &pad); }
        else         { EVP_DecryptFinal_ex(ctx, out, &pad); }
    }
    EVP_CIPHER_CTX_free(ctx);
    return CTR_RET_OK;
}

ctr_ret_t ctr_crypto_encrypt(const ctr_crypto_t *c, const uint8_t *key, const uint8_t *nonce,
                             const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
    return ctr_run(c, key, nonce, in, in_len, out, out_len, 1);
}

ctr_ret_t ctr_crypto_decrypt(const ctr_crypto_t *c, const uint8_t *key, const uint8_t *nonce,
                             const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
    return ctr_run(c, key, nonce, in, in_len, out, out_len, 0);
}

void ctr_crypto_free(ctr_crypto_t *c)
{
    if ((c == NULL) || (c->ctx == NULL)) { return; }
    EVP_CIPHER_CTX_free(c->ctx);
    c->ctx = NULL;
}
