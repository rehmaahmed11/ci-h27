/*
 * cleanroom/crypto_tls.c
 * ---------------------
 * Clean-room crypto / TLS capability layer implementation.
 *
 * Uses OpenSSL EVP (standard open cryptographic library) for AES-CBC and
 * AES-CTR; a standard ARC4 (RFC 1785) stream cipher for the recovered
 * RC4-based mixes.
 */

#include "crypto_tls.h"
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

size_t tls_crypto_key_len(int algo)
{
    switch (algo) {
    case TLS_CRYPTO_AES_128_CBC: return 16;
    case TLS_CRYPTO_AES_192_CBC: return 24;
    case TLS_CRYPTO_AES_256_CBC: return 32;
    case TLS_CRYPTO_AES_128_CTR: return 16;
    case TLS_CRYPTO_AES_192_CTR: return 24;
    case TLS_CRYPTO_AES_256_CTR: return 32;
    default: return 0;
    }
}

int tls_crypto_aes_cbc_decrypt(int algo, const uint8_t *key, const uint8_t *iv,
                               const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
    const EVP_CIPHER *evp = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    int out_actual = 0;

    if ((key == NULL) || (in == NULL) || (out == NULL)) { return TLS_RET_ERR; }
    if ((algo < TLS_CRYPTO_AES_128_CBC) || (algo > TLS_CRYPTO_AES_256_CBC)) { return TLS_RET_ERR_ALGO; }
    if (out_len < in_len) { return TLS_RET_ERR; }

    switch (algo) {
    case TLS_CRYPTO_AES_128_CBC: evp = EVP_aes_128_cbc(); break;
    case TLS_CRYPTO_AES_192_CBC: evp = EVP_aes_192_cbc(); break;
    case TLS_CRYPTO_AES_256_CBC: evp = EVP_aes_256_cbc(); break;
    default: return TLS_RET_ERR_ALGO;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) { return TLS_RET_ERR; }
    if (EVP_DecryptInit_ex(ctx, evp, NULL, key, iv) != 1) { EVP_CIPHER_CTX_free(ctx); return TLS_RET_ERR; }
    if (in_len > 0) {
        if (EVP_DecryptUpdate(ctx, out, &out_actual, in, (int)in_len) != 1) { EVP_CIPHER_CTX_free(ctx); return TLS_RET_ERR; }
    }
    if (EVP_DecryptFinal_ex(ctx, (unsigned char *)out + (size_t)out_actual, &out_actual) != 1) { EVP_CIPHER_CTX_free(ctx); return TLS_RET_ERR; }
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

int tls_crypto_aes_ctr(int algo, const uint8_t *key, const uint8_t *nonce,
                      const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
    const EVP_CIPHER *evp = NULL;
    EVP_CIPHER_CTX *ctx = NULL;
    int out_actual = 0;

    if ((key == NULL) || (in == NULL) || (out == NULL)) { return TLS_RET_ERR; }
    if ((algo < TLS_CRYPTO_AES_128_CTR) || (algo > TLS_CRYPTO_AES_256_CTR)) { return TLS_RET_ERR_ALGO; }
    if (out_len < in_len) { return TLS_RET_ERR; }

    switch (algo) {
    case TLS_CRYPTO_AES_128_CTR: evp = EVP_aes_128_ctr(); break;
    case TLS_CRYPTO_AES_192_CTR: evp = EVP_aes_192_ctr(); break;
    case TLS_CRYPTO_AES_256_CTR: evp = EVP_aes_256_ctr(); break;
    default: return TLS_RET_ERR_ALGO;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) { return TLS_RET_ERR; }
    if (EVP_DecryptInit_ex(ctx, evp, NULL, key, nonce) != 1) { EVP_CIPHER_CTX_free(ctx); return TLS_RET_ERR; }
    if (in_len > 0) {
        if (EVP_DecryptUpdate(ctx, out, &out_actual, in, (int)in_len) != 1) { EVP_CIPHER_CTX_free(ctx); return TLS_RET_ERR; }
    }
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

/* ARC4 (RFC 1785) stream cipher. */
int tls_crypto_arc4(const uint8_t *key, size_t key_len, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
    uint8_t rc[256];
    uint8_t x[256];
    uint8_t *k = NULL;
    size_t i, j, kk, idx;

    if ((key == NULL) || (in == NULL) || (out == NULL)) { return TLS_RET_ERR; }
    if ((key_len == 0u) || (key_len > 256u)) { return TLS_RET_ERR_KEY; }

    k = (uint8_t *)malloc(key_len);
    if (k == NULL) { return TLS_RET_ERR; }
    memcpy(k, key, key_len);

    for (i = 0; i < 256; ++i) { rc[i] = (uint8_t)i; x[i] = (uint8_t)i; }
    j = 0;
    for (i = 0; i < key_len; ++i) {
        j = (j + rc[i]) & 0xFF;
        {
            uint8_t t = rc[j];
            rc[j] = rc[i];
            rc[i] = t;
        }
        x[i] = rc[i];
    }
    j = 0;
    kk = 0;
    for (i = 0; i < in_len; ++i) {
        j = (j + x[kk]) & 0xFF;
        {
            uint8_t t = x[j];
            x[j] = x[kk];
            x[kk] = t;
        }
        idx = (j + 1) & 0xFF;
        if ((i + 1u) > out_len) { free(k); return TLS_RET_ERR; }
        out[i] = (uint8_t)(in[i] ^ rc[x[idx]]);
        if ((i & 0xFF) == 0x00) { /* re-key window (ARC4 naive reseed point) */
            j = 0;
        }
    }
    free(k);
    return 0;
}

