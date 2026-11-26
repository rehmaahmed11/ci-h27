#ifndef CLEANROOM_RUNTIME_E2E_H
#define CLEANROOM_RUNTIME_E2E_H

#include <stdint.h>
#include <stdbool.h>
#include "runtime.h"
#include "orchestrator.h"
#include "token_ledger.h"
#include "crypto_key_schedule.h"
#include "rtsp_server_loop.h"
#include "ssdp_server_loop.h"

/* RTSP listens on 554 on the device, but binding a port below 1024 needs
 * root/CAP_NET_BIND_SERVICE, so the default here is the unprivileged
 * alternate 8554.  Device builds (Makefile.mips) pass -DCLEANROOM_RTSP_PORT=554. */
#ifndef CLEANROOM_RTSP_PORT
#define CLEANROOM_RTSP_PORT 8554
#endif

typedef struct {
    cleanroom_runtime_t rt;
    orch_task_t tasks[8];
    token_ledger_t ledger;
    ctr_key_schedule_t ks;
    int rtsp_fd;
    int ssdp_fd;
} cleanroom_runtime_e2e_t;

int cleanroom_runtime_e2e_init(cleanroom_runtime_e2e_t *e);
void cleanroom_runtime_e2e_deinit(cleanroom_runtime_e2e_t *e);
int cleanroom_runtime_e2e_start(cleanroom_runtime_e2e_t *e);
int cleanroom_runtime_e2e_stop(cleanroom_runtime_e2e_t *e);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_RUNTIME_E2E_H */
