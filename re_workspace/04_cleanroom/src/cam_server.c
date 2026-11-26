/*
 * cleanroom/cam_server.c
 * ---------------------
 * Clean-room TurnRound IKS CAM connection state machine.
 *
 * Transition table (recovered state machine, re-expressed as a clean-room
 * guarded transition function):
 *
 *   IDLE                --CONNECT_GATE--> CONNECT_GATE_SERVER
 *   CONNECT_GATE_SERVER--LOGIN_OK-----> LOGIN_GATE_SERVER
 *   LOGIN_GATE_SERVER  --LOGIN_OK-----> CONNECT_CW_SERVER
 *   CONNECT_CW_SERVER  --LOGIN_OK-----> LOGIN_CW_SERVER
 *   LOGIN_CW_SERVER    --LOGIN_OK-----> REQUEST_CHANNEL_LIST
 *   REQUEST_CHANNEL_LIST--CW_OK-----> SET_CHANNEL
 *   SET_CHANNEL        --ACK_OK------> RETRIEVE_CW
 *   RETRIEVE_CW        --CW_OK-------> KEEP_ALIVE
 *   KEEP_ALIVE         --PING_OK-----> KEEP_ALIVE
 *   KEEP_ALIVE         --DEBIT_LOW -> TOKEN_DEBIT
 *   KEEP_ALIVE         --RECHARGE_REQ-> RECHARGE
 *   RECHARGE           --OK---------> RETRIEVE_CW
 *   any state          --DISCONNECT--> IDLE
 */

#include "cam_server.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void cam_server_init(cam_server_t *s)
{
    if (s == NULL) { return; }
    s->state = CAM_CONN_IDLE;
    s->token = 0;
    s->auth_type = 0;
    s->sub_id = 0;
    s->com_id = 0;
    s->last_ret = 0;
    memset(s->dcw, 0, sizeof(s->dcw));
}

cam_server_ret_t cam_server_handle(cam_server_t *s, cam_event_t ev, cam_server_event_out_t *out)
{
    if ((s == NULL) || (out == NULL)) { return CAM_SERVER_RET_ERR_EVENT; }

    out->next_state = (uint32_t)s->state;
    out->ok = false;

    switch (s->state) {
    case CAM_CONN_IDLE:
        if (ev == CAM_EV_CONNECTED) {
            out->next_state = (uint32_t)CAM_CONN_CONNECT_GATE_SERVER;
            out->ok = true;
        }
        break;

    case CAM_CONN_CONNECT_GATE_SERVER:
        if (ev == CAM_EV_CONNECTED) {
            out->next_state = (uint32_t)CAM_CONN_LOGIN_GATE_SERVER;
            out->ok = true;
        }
        break;

    case CAM_CONN_LOGIN_GATE_SERVER:
        if (ev == CAM_EV_LOGIN_OK) {
            out->next_state = (uint32_t)CAM_CONN_CONNECT_CW_SERVER;
            out->ok = true;
        }
        break;

    case CAM_CONN_CONNECT_CW_SERVER:
        if (ev == CAM_EV_CONNECTED) {
            out->next_state = (uint32_t)CAM_CONN_LOGIN_CW_SERVER;
            out->ok = true;
        }
        break;

    case CAM_CONN_LOGIN_CW_SERVER:
        if (ev == CAM_EV_LOGIN_OK) {
            out->next_state = (uint32_t)CAM_CONN_REQUEST_CHANNEL_LIST;
            out->ok = true;
        }
        break;

    case CAM_CONN_REQUEST_CHANNEL_LIST:
        if (ev == CAM_EV_CW_RECEIVED) {
            out->next_state = (uint32_t)CAM_CONN_SET_CHANNEL;
            out->ok = true;
        }
        break;

    case CAM_CONN_SET_CHANNEL:
        if (ev == CAM_EV_CW_RECEIVED) {
            out->next_state = (uint32_t)CAM_CONN_RETRIEVE_CW;
            out->ok = true;
        }
        break;

    case CAM_CONN_RETRIEVE_CW:
        if (ev == CAM_EV_TOKEN_DEBIT) {
            out->next_state = (uint32_t)CAM_CONN_KEEP_ALIVE;
            out->ok = true;
        }
        break;

    case CAM_CONN_KEEP_ALIVE:
        if (ev == CAM_EV_RECHARGE_OK) {
            out->next_state = (uint32_t)CAM_CONN_RECHARGE;
            out->ok = true;
        }
        break;

    case CAM_CONN_RECHARGE:
        if (ev == CAM_EV_RECHARGE_OK) {
            out->next_state = (uint32_t)CAM_CONN_RETRIEVE_CW;
            out->ok = true;
        }
        break;

    /* CAM_CONN_IDLE is handled at the top of the switch; CAM_CONN_STOP and any
     * unknown state are terminal no-ops. */
    case CAM_CONN_STOP:
    default:
        break;
    }

    if (ev == CAM_EV_DISCONNECTED) {
        out->next_state = (uint32_t)CAM_CONN_IDLE;
        out->ok = true;
    }

    s->state = (cam_conn_state_t)out->next_state;
    s->last_ret = out->ok ? 0 : -1;
    return CAM_SERVER_RET_OK;
}
