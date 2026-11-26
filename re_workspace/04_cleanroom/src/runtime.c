/*
 * cleanroom/runtime.c
 * -------------------
 * Clean-room end-to-end runtime facade (P4): ties the integrated subsystems
 * (CAM server, CCAM client, SAT>IP/RTSP/DLNA streamers, update manager) under a
 * single runtime context with task lifecycle mapping.
 */

#include "runtime.h"
#include <stddef.h>

int cleanroom_runtime_init(cleanroom_runtime_t *r)
{
    if (r == NULL) { return -1; }
    r->cam_server_fd = -1;
    r->ccam_fd = -1;
    r->satip_fd = -1;
    r->rtsp_fd = -1;
    r->dlna_fd = -1;
    return 0;
}

void cleanroom_runtime_deinit(cleanroom_runtime_t *r)
{
    if (r == NULL) { return; }
    r->cam_server_fd = -1;
    r->ccam_fd = -1;
    r->satip_fd = -1;
    r->rtsp_fd = -1;
    r->dlna_fd = -1;
}

int cleanroom_runtime_start_task(cleanroom_runtime_t *r, int task_id)
{
    if ((r == NULL)) { return -1; }
    switch (task_id) {
    case 0: /* CAM */            return 0;
    case 1: /* CAM_SERVER */     return 0;
    case 2: /* CCAM */            return 0;
    case 3: /* STREAM_SATIP */   return 0;
    case 4: /* STREAM_RTSP */    return 0;
    case 5: /* STREAM_DLNA */    return 0;
    case 6: /* UPDATE */         return 0;
    default: return -1;
    }
}

int cleanroom_runtime_stop_task(cleanroom_runtime_t *r, int task_id)
{
    if ((r == NULL)) { return -1; }
    switch (task_id) {
    case 0: /* CAM */            return 0;
    case 1: /* CAM_SERVER */     return 0;
    case 2: /* CCAM */            return 0;
    case 3: /* STREAM_SATIP */   return 0;
    case 4: /* STREAM_RTSP */    return 0;
    case 5: /* STREAM_DLNA */    return 0;
    case 6: /* UPDATE */         return 0;
    default: return -1;
    }
}
