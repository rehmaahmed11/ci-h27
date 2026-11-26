#ifndef CLEANROOM_CRYPTO_KEY_SCHEDULE_H
#define CLEANROOM_CRYPTO_KEY_SCHEDULE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int algo;
    uint8_t key[32];
    uint8_t nonce[16];
} ctr_key_schedule_t;

typedef enum {
    CTR_KEY_SCHED_OK = 0,
    CTR_KEY_SCHED_ERR = -1,
    CTR_KEY_SCHED_ERR_ALGO = -2
} ctr_key_sched_ret_t;

ctr_key_sched_ret_t ctr_key_schedule_init(ctr_key_schedule_t *ks, int algo, const uint8_t *base_key);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_CRYPTO_KEY_SCHEDULE_H */
