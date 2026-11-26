/*
 * cleanroom/common.h
 * ------------------
 * Shared primitives for the clean-room firmware re-implementation.
 *
 * This is a *clean-room* implementation: nothing here is copied from vendor
 * firmware binary code.  Only generic, public definitions are used.
 *
 * P0 modules: fw_update, fw_nvm, cam_keys, cam_server
 */

#ifndef CLEANROOM_COMMON_H
#define CLEANROOM_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* CRC32 (reflected).                                                 */
/*   poly  = 0xEDB88320 (reflected form of 0x04C11DB7)                 */
/*   init  = 0xFFFFFFFF                                               */
/*   xorout= 0xFFFFFFFF                                               */
/* ------------------------------------------------------------------ */
#define CLEANROOM_CRC32_POLY  0xEDB88320u
#define CLEANROOM_CRC32_INIT  0xFFFFFFFFu
#define CLEANROOM_CRC32_XOROUT 0xFFFFFFFFu

uint32_t cleanroom_crc32(const uint8_t *data, size_t len);
void     cleanroom_crc32_init(uint32_t *crc);
void     cleanroom_crc32_update(uint32_t *crc, const uint8_t *data, size_t len);
uint32_t cleanroom_crc32_final(uint32_t crc);

/* ------------------------------------------------------------------ */
/* Big-endian (BE) helpers used by the flash frame codec.             */
/* ------------------------------------------------------------------ */
void cleanroom_be32_put(uint8_t out[4], uint32_t v);
uint32_t cleanroom_be32_get(const uint8_t in[4]);

/* ------------------------------------------------------------------ */
/* Generic status / error codes.                                      */
/* ------------------------------------------------------------------ */
typedef int cleanroom_ret;

#define CLEANROOM_RET_OK       0
#define CLEANROOM_RET_ERR      -1
#define CLEANROOM_RET_INVALID  -2
#define CLEANROOM_RET_NOMEM    -3

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_COMMON_H */
