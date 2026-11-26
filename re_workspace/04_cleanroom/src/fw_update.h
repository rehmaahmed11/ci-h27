/*
 * cleanroom/fw_update.h
 * ---------------------
 * Clean-room implementation of the recovered firmware **update / flashing**
 * frame codec (P0 module).
 *
 * The original vendor implementation used a framed binary protocol over a
 * flash/OTA bus.  The wire layout recovered during reverse engineering was
 * re-implemented here from the *specification* into generic, public C.
 *
 *   frame := "HEAD" | type | flag | index:BE32 | subtype | paylen:BE32 | payCRC:BE32
 *           | payload[paylen] | CRC32(payload):BE32
 *
 *   header CRC32 covers header bytes [0..10] (i.e. type..paylen) and is stored
 *   big-endian at header offsets [11..14].
 *   The trailing CRC32 is computed over the payload only and stored BE.
 */

#ifndef CLEANROOM_FW_UPDATE_H
#define CLEANROOM_FW_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Frame sync characters ("HEAD") */
#define FW_UPDATE_SYNC0  (uint8_t)'H'
#define FW_UPDATE_SYNC1  (uint8_t)'E'
#define FW_UPDATE_SYNC2  (uint8_t)'A'
#define FW_UPDATE_SYNC3  (uint8_t)'D'
#define FW_UPDATE_SYNC_STR "HEAD"

/* Frame geometry */
#define FW_UPDATE_HDR_SIZE     19u   /* sync(4)+type(1)+flag(1)+index(4)+subtype(1)+paylen(4)+hdrcrc(4) */
#define FW_UPDATE_TRAILER_SIZE 4    /* trailing CRC32 over payload */
#define FW_UPDATE_FRAME_OVERHEAD (FW_UPDATE_HDR_SIZE + FW_UPDATE_TRAILER_SIZE)

/* Command identifiers (recovered command-name table order:
   address, burn, comtest, move, reboot, transfer, transferraw, version, dump, set)
 */
typedef enum {
    FW_UPDATE_CMD_ADDRESS      = 0x01,
    FW_UPDATE_CMD_BURN         = 0x02,
    FW_UPDATE_CMD_COMTEST      = 0x03,
    FW_UPDATE_CMD_MOVE         = 0x04,
    FW_UPDATE_CMD_REBOOT       = 0x05,
    FW_UPDATE_CMD_TRANSFER    = 0x06,
    FW_UPDATE_CMD_TRANSFERRAW = 0x07,
    FW_UPDATE_CMD_VERSION     = 0x08,
    FW_UPDATE_CMD_DUMP         = 0x09,
    FW_UPDATE_CMD_SET          = 0x0Au
} fw_update_cmd_t;

/* Response status codes (recovered identifiers; values are symbolic groupings) */
#define FW_UPDATE_RSP_R101  0x00000101
#define FW_UPDATE_RSP_R301  0x00000301
#define FW_UPDATE_RSP_R302  0x00000302
#define FW_UPDATE_RSP_R401  0x00000401
#define FW_UPDATE_RSP_R402  0x00000402
#define FW_UPDATE_RSP_R403  0x00000403
#define FW_UPDATE_RSP_R501  0x00000501
#define FW_UPDATE_RSP_R502  0x00000502
#define FW_UPDATE_RSP_R503  0x00000503
#define FW_UPDATE_RSP_R601  0x00000601
#define FW_UPDATE_RSP_R801  0x00000801
#define FW_UPDATE_RSP_R901  0x00000901
#define FW_UPDATE_RSP_R902  0x00000902
#define FW_UPDATE_RSP_R999  0x00000999
#define FW_UPDATE_RSP_ERR1  0x00000E01
#define FW_UPDATE_RSP_ERR2  0x00000E02

typedef enum {
    FW_UPDATE_RET_OK = 0,
    FW_UPDATE_RET_ERR_SYNC = -1,
    FW_UPDATE_RET_ERR_HDR  = -2,
    FW_UPDATE_RET_ERR_CRC  = -3,
    FW_UPDATE_RET_ERR_LEN  = -4,
    FW_UPDATE_RET_ERR_BODY = -5
} fw_update_ret_t;

/*
 * Encode an update frame into `buf` (size >= FW_UPDATE_FRAME_OVERHEAD + payload_len).
 * Returns 0 on success, negative error code on failure.
 */
fw_update_ret_t fw_update_frame_encode(uint8_t *buf, size_t buf_size,
                                      size_t *out_len,
                                      uint8_t type, uint8_t flag,
                                      uint32_t index, uint8_t subtype,
                                      const uint8_t *payload, size_t payload_len);

/*
 * Decode an update frame from `buf` (size >= frame size).
 * On success returns FW_UPDATE_RET_OK, fills metadata and copies payload
 * (if requested) into `payload`/`payload_actual`.
 */
fw_update_ret_t fw_update_frame_decode(const uint8_t *buf, size_t buf_len,
                                      size_t *payload_len,
                                      uint8_t *type, uint8_t *flag,
                                      uint32_t *index, uint8_t *subtype,
                                      uint8_t *payload, size_t payload_max,
                                      size_t *payload_actual);

/* Progress / status text helpers. */
const char *fw_update_progress_str(uint32_t chunk_k);
const char *fw_update_error_str(uint32_t code);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_FW_UPDATE_H */

