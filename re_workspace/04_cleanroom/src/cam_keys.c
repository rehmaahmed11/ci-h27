/*
 * cleanroom/cam_keys.c
 * -------------------
 * Clean-room SoftCam / CAM key store implementation.
 *
 * Uses OpenSSL EVP (standard open cryptographic library) for AES-128/192/256
 * CBC and CTR operations.  The key material model mirrors the recovered
 * behaviour: per-identifier SoftCam keys with per-algorithm selection.
 */

#include "cam_keys.h"
#include <string.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define CAM_KEY_MAX 32

static cam_softcam_key_t g_cam_keys[CAM_KEY_MAX];

cam_key_ret_t cam_key_store_init(void)
{
    memset(g_cam_keys, 0, sizeof(g_cam_keys));
    return CAM_KEY_RET_OK;
}

__attribute__((unused)) static int cam_key_algo_evp(int algo, const EVP_CIPHER **evp)
{
    switch (algo) {
    case CAM_KEY_AES_128_CBC: *evp = EVP_aes_128_cbc(); return 0;
    case CAM_KEY_AES_192_CBC: *evp = EVP_aes_192_cbc(); return 0;
    case CAM_KEY_AES_256_CBC: *evp = EVP_aes_256_cbc(); return 0;
    case CAM_KEY_AES_128_CTR: *evp = EVP_aes_128_ctr(); return 0;
    case CAM_KEY_AES_192_CTR: *evp = EVP_aes_192_ctr(); return 0;
    case CAM_KEY_AES_256_CTR: *evp = EVP_aes_256_ctr(); return 0;
    default: return CAM_KEY_RET_ERR_ALGO;
    }
}

static size_t cam_key_len_for(int algo)
{
    switch (algo) {
    case CAM_KEY_AES_128_CBC:
    case CAM_KEY_AES_128_CTR: return 16;
    case CAM_KEY_AES_192_CBC:
    case CAM_KEY_AES_192_CTR: return 24;
    case CAM_KEY_AES_256_CBC:
    case CAM_KEY_AES_256_CTR: return 32;
    default: return 0;
    }
}

cam_key_ret_t cam_key_store_add(const char *id, int algo, const uint8_t *key, size_t key_len)
{
    size_t i;

    if ((id == NULL) || (key == NULL)) { return CAM_KEY_RET_ERR_RANGE; }
    if ((algo < CAM_KEY_AES_128_CBC) || (algo > CAM_KEY_AES_256_CTR)) { return CAM_KEY_RET_ERR_ALGO; }

    for (i = 0; i < (size_t)CAM_KEY_MAX; ++i) {
        if ((g_cam_keys[i].in_use == false) || (strcmp(g_cam_keys[i].id, id) == 0)) { break; }
    }
    if (i >= (size_t)CAM_KEY_MAX) { return CAM_KEY_RET_ERR_RANGE; }

    memset(&g_cam_keys[i], 0, sizeof(g_cam_keys[i]));
    strncpy(g_cam_keys[i].id, id, sizeof(g_cam_keys[i].id) - 1);
    g_cam_keys[i].algo = algo;
    memset(g_cam_keys[i].key, 0, sizeof(g_cam_keys[i].key));
    if (key_len > sizeof(g_cam_keys[i].key)) { key_len = sizeof(g_cam_keys[i].key); }
    memcpy(g_cam_keys[i].key, key, key_len);
    g_cam_keys[i].in_use = true;

    return CAM_KEY_RET_OK;
}

bool cam_key_store_get(const char *id, int algo, uint8_t *out, size_t out_len)
{
    size_t i;

    if ((id == NULL) || (out == NULL)) { return false; }
    for (i = 0; i < (size_t)CAM_KEY_MAX; ++i) {
        if (g_cam_keys[i].in_use == false) { continue; }
        if (strcmp(g_cam_keys[i].id, id) != 0) { continue; }
        if (g_cam_keys[i].algo != algo) { continue; }
        if (out_len < (size_t)cam_key_len_for(algo)) { return false; }
        memset(out, 0, out_len);
        memcpy(out, g_cam_keys[i].key, (out_len < sizeof(g_cam_keys[i].key)) ? out_len : sizeof(g_cam_keys[i].key));
        return true;
    }
    return false;
}

bool cam_key_hdcp_valid(const uint8_t *kvp, size_t len)
{
    /* Recovered rule: first byte of HDCP key must be 0x01 (KSV marker). */
    if ((kvp == NULL) || (len < 1u)) { return false; }
    if (kvp[0] != 0x01u) { return false; }
    if (len < 16u) { return false; }
    return true;
}
