#ifndef CLEANROOM_CAM_FRAME_H
#define CLEANROOM_CAM_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Clean-room CAM/IKS frame header layout (recovered behavioural fields). */
#define CAM_FRAME_HDR_SIZE 21u

typedef enum {
    CAM_FRAME_RET_OK = 0,
    CAM_FRAME_RET_ERR = -1,
    CAM_FRAME_RET_ERR_BUILD = -2
} cam_frame_ret_t;

typedef struct {
    uint8_t  cmd;      /* CAM command: LOGIN / SETCW / IKSPING / TOKEN */
    uint8_t  flags;
    uint16_t sid;      /* session identifier */
    uint16_t sub_id;   /* sub-command identifier */
    uint32_t token;    /* CAM session token */
    uint32_t auth_type; /* auth type / token debit state */
    uint8_t  dcw[4];   /* descrambling control word (BE) */
    uint8_t  key_index;
    uint8_t  channel_id;
    uint8_t  status;   /* per-command status (OK / INSUFFICIENT_TOKENS / ...) */
} cam_frame_t;

cam_frame_ret_t cam_frame_build(cam_frame_t *f, uint8_t cmd, uint8_t flags, uint16_t sid,
                                uint16_t sub_id, uint32_t token, uint32_t auth_type,
                                const uint8_t *dcw, uint8_t key_index, uint8_t channel_id,
                                uint8_t status);
cam_frame_ret_t cam_frame_encode(const cam_frame_t *f, uint8_t *buf, size_t cap, size_t *len);
cam_frame_ret_t cam_frame_decode(const uint8_t *buf, size_t len, cam_frame_t *f);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_CAM_FRAME_H */
