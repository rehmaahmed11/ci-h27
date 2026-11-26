/*
 * cleanroom/cam_keys.h
 * -------------------
 * Clean-room SoftCam / CAM key store (P0 module).
 *
 * Implements the recovered key-source model:
 *   - flash default partition keys (DefTurnRoundKey / blcommondata / HDCPKey)
 *   - USB auto-add paths (cryptoworks, viaccess, biss-Q, powervu T/P/Z/Y lines)
 *   - /mnt/uda1/SoftCam.key style key file entries
 *   - AES-128/192/256 CBC & CTR key usage (via OpenSSL EVP).
 *
 * Only standard open cryptographic primitives are used.
 */

#ifndef CLEANROOM_CAM_KEYS_H
#define CLEANROOM_CAM_KEYS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AES algorithms supported by the recovered key store. */
typedef enum {
    CAM_KEY_AES_128_CBC = 0,
    CAM_KEY_AES_192_CBC = 1,
    CAM_KEY_AES_256_CBC = 2,
    CAM_KEY_AES_128_CTR = 3,
    CAM_KEY_AES_192_CTR = 4,
    CAM_KEY_AES_256_CTR = 5
} cam_key_algo_t;

#define CAM_KEY_ID_MAX_LEN 64

typedef struct {
    char     id[CAM_KEY_ID_MAX_LEN];
    int      algo;
    uint8_t key[32];
    bool     in_use;
} cam_softcam_key_t;

typedef enum {
    CAM_KEY_RET_OK = 0,
    CAM_KEY_RET_ERR_ALLOC = -1,
    CAM_KEY_RET_ERR_RANGE = -2,
    CAM_KEY_RET_ERR_ALGO = -3
} cam_key_ret_t;

/* Initialise the (empty) key store. */
cam_key_ret_t cam_key_store_init(void);

/* Add / overwrite a SoftCam key.  key_len must be 16 (AES-128), 24 (AES-192) or 32 (AES-256). */
cam_key_ret_t cam_key_store_add(const char *id, int algo, const uint8_t *key, size_t key_len);

/* Retrieve a key by id + algorithm; returns false when absent. */
bool cam_key_store_get(const char *id, int algo, uint8_t *out, size_t out_len);

/* HDCP key sanity: first byte must be 0x01 (KSV check) and key length 16 bytes. */
bool cam_key_hdcp_valid(const uint8_t *kvp, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_CAM_KEYS_H */
