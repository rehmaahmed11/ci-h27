/*
 * cleanroom/crypto_key_schedule.c
 * -------------------------------
 * Clean-room AES-CTR key/nonce scheduling (P5): derives per-session key/nonce
 * from a base key for the selected AES-128/192/256 CTR algorithm.
 */

#include "crypto_key_schedule.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

ctr_key_sched_ret_t ctr_key_schedule_init(ctr_key_schedule_t *ks, int algo, const uint8_t *base_key)
{
    if ((ks == NULL) || (base_key == NULL)) { return CTR_KEY_SCHED_ERR; }
    if ((algo != 0) && (algo != 1) && (algo != 2)) { return CTR_KEY_SCHED_ERR_ALGO; }

    ks->algo = algo;
    memset(ks->key, 0, sizeof(ks->key));
    memset(ks->nonce, 0, sizeof(ks->nonce));
    {
        size_t key_len = (algo == 0) ? 16u : ((algo == 1) ? 24u : 32u);
        memcpy(ks->key, base_key, key_len);
    }
    ks->nonce[0] = (uint8_t)(algo + 1);
    return CTR_KEY_SCHED_OK;
}
