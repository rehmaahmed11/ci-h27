/*
 * cleanroom/fw_nvm.c
 * ------------------
 * Clean-room NVM partition store: bounds-checked read/write plus the
 * recovered erase-flag test (all-0xFF region => erased).
 */

#include "fw_nvm.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

fw_nvm_ret_t fw_nvm_read(fw_nvm_partition_t *p, size_t offset, uint8_t *dst, size_t n)
{
    size_t i;

    if ((p == NULL) || (p->data == NULL) || (dst == NULL)) { return FW_NVM_RET_ERR_NULL; }
    if ((offset + n) > p->size) { return FW_NVM_RET_ERR_RANGE; }

    for (i = 0; i < n; ++i) { dst[i] = p->data[offset + i]; }
    return FW_NVM_RET_OK;
}

fw_nvm_ret_t fw_nvm_write(fw_nvm_partition_t *p, size_t offset, const uint8_t *src, size_t n)
{
    size_t i;

    if ((p == NULL) || (p->data == NULL) || (src == NULL)) { return FW_NVM_RET_ERR_NULL; }
    if ((offset + n) > p->size) { return FW_NVM_RET_ERR_RANGE; }

    for (i = 0; i < n; ++i) { p->data[offset + i] = src[i]; }
    return FW_NVM_RET_OK;
}

bool fw_nvm_is_erased(const uint8_t *p, size_t n)
{
    size_t i;
    bool all_ff = true;

    if ((p == NULL) && (n > 0u)) { return false; }
    for (i = 0; i < n; ++i) {
        if (p[i] != 0xFFu) { all_ff = false; break; }
    }
    return all_ff;
}

void fw_nvm_erase(fw_nvm_partition_t *p, size_t offset, size_t n)
{
    size_t i;
    if ((p == NULL) || (p->data == NULL)) { return; }
    if ((offset + n) > p->size) { return; }
    for (i = 0; i < n; ++i) { p->data[offset + i] = 0xFFu; }
}

fw_nvm_ret_t fw_nvm_alloc(fw_nvm_partition_t *p, size_t size)
{
    if ((p == NULL)) { return FW_NVM_RET_ERR_NULL; }
    p->data = (uint8_t *)calloc(1, (size_t)(size > 0 ? size : 1));
    p->size = (size_t)(size > 0 ? size : 0);
    if (p->data == NULL) { p->size = 0; return FW_NVM_RET_ERR_SIZE; }
    return FW_NVM_RET_OK;
}
