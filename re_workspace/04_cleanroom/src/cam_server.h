/*
 * cleanroom/cam_server.h
 * ----------------------
 * Clean-room TurnRound IKS CAM connection state machine (P0 module).
 *
 * State machine recovered from debug strings:
 *   IDLE -> STOP
 *   -> CONNECT_GATE_SERVER -> LOGIN_GATE_SERVER -> CONNECT_CW_SERVER / LOGIN_CW_SERVER
 *      -> REQUEST_CHANNEL_LIST -> SET_CHANNEL -> RETRIEVE_CW | KEEP_ALIVE | RECHARGE
 *
 * Events: connect, disconnect, login_ok, login_fail, cw_retrieved, token_debit,
 *         recharge_ok, recharge_fail.
 */

#ifndef CLEANROOM_CAM_SERVER_H
#define CLEANROOM_CAM_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAM_CONN_IDLE = 0,
    CAM_CONN_STOP = 1,
    CAM_CONN_CONNECT_GATE_SERVER = 2,
    CAM_CONN_LOGIN_GATE_SERVER = 3,
    CAM_CONN_CONNECT_CW_SERVER = 4,
    CAM_CONN_LOGIN_CW_SERVER = 5,
    CAM_CONN_REQUEST_CHANNEL_LIST = 6,
    CAM_CONN_SET_CHANNEL = 7,
    CAM_CONN_RETRIEVE_CW = 8,
    CAM_CONN_KEEP_ALIVE = 9,
    CAM_CONN_RECHARGE = 10
} cam_conn_state_t;

typedef enum {
    CAM_EV_CONNECTED = 0,
    CAM_EV_DISCONNECTED = 1,
    CAM_EV_LOGIN_OK = 2,
    CAM_EV_LOGIN_FAIL = 3,
    CAM_EV_CW_RECEIVED = 4,
    CAM_EV_TOKEN_DEBIT = 5,
    CAM_EV_RECHARGE_OK = 6,
    CAM_EV_RECHARGE_FAIL = 7
} cam_event_t;

typedef struct {
    cam_conn_state_t state;
    uint32_t token;
    uint32_t auth_type;
    uint32_t sub_id;
    uint32_t com_id;
    uint32_t dcw[8];
    int32_t  last_ret;
} cam_server_t;

typedef struct {
    uint32_t next_state;
    bool     ok;
} cam_server_event_out_t;

typedef enum {
    CAM_SERVER_RET_OK = 0,
    CAM_SERVER_RET_ERR_STATE = -1,
    CAM_SERVER_RET_ERR_EVENT = -2
} cam_server_ret_t;

void cam_server_init(cam_server_t *s);
cam_server_ret_t cam_server_handle(cam_server_t *s, cam_event_t ev, cam_server_event_out_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_CAM_SERVER_H */
