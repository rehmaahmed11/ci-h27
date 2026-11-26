/*
 * cleanroom/cam_frame.c
 * --------------------
 * Clean-room CAM / TurnRound IKS binary frame codec (P2 integration layer).
 *
 * Frame layout (clean-room specification derived from the recovered protocol):
 *   [0]      cmd
 *   [1]      flags
 *   [2..3]   sid        : BE16
 *   [4..5]   sub_id    : BE16
 *   [6..9]   token     : BE32
 *   [10..13] auth_type : BE32
 *   [14..17] dcw       : BE32
 *   [18]     key_index
 *   [19]     channel_id
 *   [20]     status
 */

#include "cam_frame.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

cam_frame_ret_t cam_frame_build(cam_frame_t *f, uint8_t cmd, uint8_t flags, uint16_t sid,
                                uint16_t sub_id, uint32_t token, uint32_t auth_type,
                                const uint8_t *dcw, uint8_t key_index, uint8_t channel_id,
                                uint8_t status)
{
    if ((f == NULL) || ((dcw == NULL) && (channel_id != 0u))) { return CAM_FRAME_RET_ERR; }
    f->cmd = cmd;
    f->flags = flags;
    f->sid = sid;
    f->sub_id = sub_id;
    f->token = token;
    f->auth_type = auth_type;
    memset(f->dcw, 0, sizeof(f->dcw));
    if (dcw != NULL) { memcpy(f->dcw, dcw, sizeof(f->dcw)); }
    f->key_index = key_index;
    f->channel_id = channel_id;
    f->status = status;
    return CAM_FRAME_RET_OK;
}

static uint16_t cam_frame_be16_get(const uint8_t in[2])
{
    return (uint16_t)(((uint16_t)in[0] << 8) | (uint16_t)in[1]);
}

static uint32_t cam_frame_be32_get(const uint8_t in[4])
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

cam_frame_ret_t cam_frame_encode(const cam_frame_t *f, uint8_t *buf, size_t cap, size_t *len)
{
    if ((f == NULL) || (buf == NULL) || (len == NULL)) { return CAM_FRAME_RET_ERR; }
    if (cap < (size_t)CAM_FRAME_HDR_SIZE) { return CAM_FRAME_RET_ERR_BUILD; }

    buf[0] = f->cmd;
    buf[1] = f->flags;
    buf[2] = (uint8_t)((f->sid >> 8) & 0xFFu);
    buf[3] = (uint8_t)(f->sid & 0xFFu);
    buf[4] = (uint8_t)((f->sub_id >> 8) & 0xFFu);
    buf[5] = (uint8_t)(f->sub_id & 0xFFu);
    buf[6] = (uint8_t)((f->token >> 24) & 0xFFu);
    buf[7] = (uint8_t)((f->token >> 16) & 0xFFu);
    buf[8] = (uint8_t)((f->token >> 8) & 0xFFu);
    buf[9] = (uint8_t)(f->token & 0xFFu);
    buf[10] = (uint8_t)((f->auth_type >> 24) & 0xFFu);
    buf[11] = (uint8_t)((f->auth_type >> 16) & 0xFFu);
    buf[12] = (uint8_t)((f->auth_type >> 8) & 0xFFu);
    buf[13] = (uint8_t)(f->auth_type & 0xFFu);
    buf[14] = f->dcw[0];
    buf[15] = f->dcw[1];
    buf[16] = f->dcw[2];
    buf[17] = f->dcw[3];
    buf[18] = f->key_index;
    buf[19] = f->channel_id;
    buf[20] = f->status;

    *len = (size_t)CAM_FRAME_HDR_SIZE;
    return CAM_FRAME_RET_OK;
}

cam_frame_ret_t cam_frame_decode(const uint8_t *buf, size_t len, cam_frame_t *f)
{
    if ((buf == NULL) || (f == NULL)) { return CAM_FRAME_RET_ERR; }
    if (len < (size_t)CAM_FRAME_HDR_SIZE) { return CAM_FRAME_RET_ERR; }

    f->cmd = buf[0];
    f->flags = buf[1];
    f->sid = cam_frame_be16_get(&buf[2]);
    f->sub_id = cam_frame_be16_get(&buf[4]);
    f->token = cam_frame_be32_get(&buf[6]);
    f->auth_type = cam_frame_be32_get(&buf[10]);
    f->dcw[0] = buf[14];
    f->dcw[1] = buf[15];
    f->dcw[2] = buf[16];
    f->dcw[3] = buf[17];
    f->key_index = buf[18];
    f->channel_id = buf[19];
    f->status = buf[20];

    return CAM_FRAME_RET_OK;
}
