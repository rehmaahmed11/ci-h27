/*
 * cleanroom/runtime_e2e.c
 * -----------------------
 * Clean-room end-to-end runtime (P6): composes the integrated clean-room
 * subsystems — runtime FDs, task scheduler tasks, token ledger (memory store),
 * AES-CTR key schedule, RTSP loop and SSDP loop — into a single runtime object
 * with start/stop lifecycle.
 */

#include "runtime_e2e.h"
#include "net_socket.h"
#include "crypto_ctr.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

int cleanroom_runtime_e2e_init(cleanroom_runtime_e2e_t *e)
{
    rtsp_server_loop_t rtsp_loop;
    ssdp_server_loop_t ssdp_loop;
    uint8_t base_key[16];
    size_t i;

    if (e == NULL) { return -1; }
    if (cleanroom_runtime_init(&e->rt) != 0) { return -1; }
    if (token_ledger_init(&e->ledger) != TOKEN_LEDGER_OK) { return -1; }
    token_ledger_set_memory_store(&e->ledger);
    memset(base_key, 0x5A, sizeof(base_key));
    if (ctr_key_schedule_init(&e->ks, CTR_CRYPTO_AES_128, base_key) != CTR_KEY_SCHED_OK) { return -1; }

    memset(e->tasks, 0, sizeof(e->tasks));
    for (i = 0; i < (sizeof(e->tasks) / sizeof(e->tasks[0])); ++i) {
        e->tasks[i].id = (orch_task_id_t)(i);
        e->tasks[i].state = (int)ORCH_TASK_STATE_IDLE;
    }

    if (rtsp_server_loop_init(&rtsp_loop, CLEANROOM_RTSP_PORT) != 0) { return -1; }
    if (ssdp_server_loop_init(&ssdp_loop, 1900, "urn:schemas-upnp-org:service:AVTransport:1") != 0) { return -1; }
    e->rtsp_fd = -1;
    e->ssdp_fd = -1;
    return 0;
}

void cleanroom_runtime_e2e_deinit(cleanroom_runtime_e2e_t *e)
{
    if (e == NULL) { return; }
    if (e->rtsp_fd >= 0) { net_socket_close(e->rtsp_fd); }
    if (e->ssdp_fd >= 0) { net_socket_close(e->ssdp_fd); }
    e->rtsp_fd = -1;
    e->ssdp_fd = -1;
    cleanroom_runtime_deinit(&e->rt);
}

int cleanroom_runtime_e2e_start(cleanroom_runtime_e2e_t *e)
{
    rtsp_server_loop_t rtsp_loop;
    ssdp_server_loop_t ssdp_loop;

    if (e == NULL) { return -1; }
    if (rtsp_server_loop_init(&rtsp_loop, CLEANROOM_RTSP_PORT) != 0) { return -1; }
    if (ssdp_server_loop_init(&ssdp_loop, 1900, "urn:schemas-upnp-org:service:AVTransport:1") != 0) { return -1; }
    if (rtsp_server_loop_open(&rtsp_loop, &e->rtsp_fd) != 0) { return -1; }
    if (ssdp_server_loop_open(&ssdp_loop, &e->ssdp_fd) != 0) { return -1; }
    if (cleanroom_runtime_start_task(&e->rt, ORCH_TASK_CAM_SERVER) != 0) { return -1; }
    if (cleanroom_runtime_start_task(&e->rt, ORCH_TASK_STREAM_RTSP) != 0) { return -1; }
    if (cleanroom_runtime_start_task(&e->rt, ORCH_TASK_STREAM_DLNA) != 0) { return -1; }
    return 0;
}

int cleanroom_runtime_e2e_stop(cleanroom_runtime_e2e_t *e)
{
    if (e == NULL) { return -1; }
    if (cleanroom_runtime_stop_task(&e->rt, ORCH_TASK_CAM_SERVER) != 0) { return -1; }
    if (cleanroom_runtime_stop_task(&e->rt, ORCH_TASK_STREAM_RTSP) != 0) { return -1; }
    if (cleanroom_runtime_stop_task(&e->rt, ORCH_TASK_STREAM_DLNA) != 0) { return -1; }
    if (e->rtsp_fd >= 0) { net_socket_close(e->rtsp_fd); e->rtsp_fd = -1; }
    if (e->ssdp_fd >= 0) { net_socket_close(e->ssdp_fd); e->ssdp_fd = -1; }
    return 0;
}
