/*
 * cleanroom/fw_nvm.h
 * -----------------
 * Clean-room abstraction of the recovered firmware **Non-Volatile Memory
 * (NVM) store** used for key / database persistence (P0 module).
 */

#ifndef CLEANROOM_FW_NVM_H
#define CLEANROOM_FW_NVM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;   /* backing store */
    size_t   size;   /* size of backing store in bytes */
} fw_nvm_partition_t;

typedef enum {
    FW_NVM_RET_OK = 0,
    FW_NVM_RET_ERR_NULL = -1,
    FW_NVM_RET_ERR_SIZE = -2,
    FW_NVM_RET_ERR_RANGE = -3
} fw_nvm_ret_t;

/* Read `n` bytes at `offset` into `dst`. */
fw_nvm_ret_t fw_nvm_read(fw_nvm_partition_t *p, size_t offset, uint8_t *dst, size_t n);

/* Write `n` bytes at `offset` from `src` (caller guarantees erase semantics). */
fw_nvm_ret_t fw_nvm_write(fw_nvm_partition_t *p, size_t offset, const uint8_t *src, size_t n);

/* True if the region is entirely 0xFF (the recovered "erased"/fill-flag state). */
bool fw_nvm_is_erased(const uint8_t *p, size_t n);

/* Erase (fill with 0xFF) a region.  In a real flash this maps to the erase command. */
void fw_nvm_erase(fw_nvm_partition_t *p, size_t offset, size_t n);

/* Allocate + zero a partition of `size` bytes. */
fw_nvm_ret_t fw_nvm_alloc(fw_nvm_partition_t *p, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_FW_NVM_H */
