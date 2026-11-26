/*
 * cleanroom/fw_update.c
 * --------------------
 * Clean-room implementation of the firmware update / flashing frame codec.
 *
 * Pure software: uses only the shared CRC32 helper (cleanroom/common) and
 * byte-order helpers.  No vendor code is referenced.
 */

#include "fw_update.h"
#include "common.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

/* ---- progress / status text ---------------------------------------- */

const char *fw_update_progress_str(uint32_t chunk_k)
{
    /* original: "B%04dK" progress marker, "E%07d" error marker, "FINISH" */
    static char b[16];
    snprintf(b, sizeof(b), "B%04dK", chunk_k);
    return b;
}

const char *fw_update_error_str(uint32_t code)
{
    static char e[16];
    snprintf(e, sizeof(e), "E%07d", code);
    return e;
}

/* ---- frame encode -------------------------------------------------- */

fw_update_ret_t fw_update_frame_encode(uint8_t *buf, size_t buf_size,
                                      size_t *out_len,
                                      uint8_t type, uint8_t flag,
                                      uint32_t index, uint8_t subtype,
                                      const uint8_t *payload, size_t payload_len)
{
    uint32_t crc_hdr;
    uint32_t crc_pay;

    if ((buf == NULL) || (out_len == NULL)) { return FW_UPDATE_RET_ERR_LEN; }
    if ((payload == NULL) && (payload_len > 0u)) { return FW_UPDATE_RET_ERR_LEN; }
    if (buf_size < (FW_UPDATE_FRAME_OVERHEAD + payload_len)) { return FW_UPDATE_RET_ERR_LEN; }

    memcpy(buf, "HEAD", 4);
    buf[4] = type;
    buf[5] = flag;
    cleanroom_be32_put(buf + 6, index);
    buf[10] = subtype;
    cleanroom_be32_put(buf + 11, (uint32_t)payload_len);
    crc_hdr = cleanroom_crc32(buf, 15);          /* sync+type+flag+index+subtype+paylen */
    cleanroom_be32_put(buf + 15, crc_hdr);

    if (payload_len > 0u) {
        memcpy(buf + FW_UPDATE_HDR_SIZE, payload, payload_len);
    }
    crc_pay = cleanroom_crc32(buf + FW_UPDATE_HDR_SIZE, payload_len);
    cleanroom_be32_put(buf + FW_UPDATE_HDR_SIZE + payload_len, crc_pay);

    *out_len = FW_UPDATE_HDR_SIZE + payload_len + FW_UPDATE_TRAILER_SIZE;
    return FW_UPDATE_RET_OK;
}

/* ---- frame decode -------------------------------------------------- */

fw_update_ret_t fw_update_frame_decode(const uint8_t *buf, size_t buf_len,
                                      size_t *payload_len,
                                      uint8_t *type, uint8_t *flag,
                                      uint32_t *index, uint8_t *subtype,
                                      uint8_t *payload, size_t payload_max,
                                      size_t *payload_actual)
{
    uint32_t crc_hdr;
    uint32_t crc_pay;
    uint32_t paylen;
    size_t   total;

    if ((buf == NULL) || (payload_len == NULL)) { return FW_UPDATE_RET_ERR_LEN; }
    if (buf_len < FW_UPDATE_FRAME_OVERHEAD) { return FW_UPDATE_RET_ERR_LEN; }

    if ((buf[0] != FW_UPDATE_SYNC0) || (buf[1] != FW_UPDATE_SYNC1) ||
        (buf[2] != FW_UPDATE_SYNC2) || (buf[3] != FW_UPDATE_SYNC3)) {
        return FW_UPDATE_RET_ERR_SYNC;
    }

    paylen = cleanroom_be32_get(buf + 11);
    crc_hdr = cleanroom_be32_get(buf + 15);
    if (crc_hdr != cleanroom_crc32(buf, 15)) { return FW_UPDATE_RET_ERR_CRC; }

    total = (size_t)FW_UPDATE_HDR_SIZE + (size_t)paylen + FW_UPDATE_TRAILER_SIZE;
    if (buf_len < total) { return FW_UPDATE_RET_ERR_LEN; }

    if (type != NULL) { *type = buf[4]; }
    if (flag != NULL) { *flag = buf[5]; }
    if (index != NULL) { *index = cleanroom_be32_get(buf + 6); }
    if (subtype != NULL) { *subtype = buf[10]; }
    if (payload_len != NULL) { *payload_len = (size_t)paylen; }

    if (payload != NULL) {
        if (payload_max < (size_t)paylen) { return FW_UPDATE_RET_ERR_LEN; }
        if (paylen > 0u) { memcpy(payload, buf + FW_UPDATE_HDR_SIZE, (size_t)paylen); }
    }
    if (payload_actual != NULL) { *payload_actual = (size_t)paylen; }

    crc_pay = cleanroom_crc32(buf + FW_UPDATE_HDR_SIZE, (size_t)paylen);
    if (cleanroom_be32_get(buf + FW_UPDATE_HDR_SIZE + (size_t)paylen) != crc_pay) {
        return FW_UPDATE_RET_ERR_CRC;
    }

    return FW_UPDATE_RET_OK;
}
