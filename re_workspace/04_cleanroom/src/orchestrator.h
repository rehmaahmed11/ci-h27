#ifndef CLEANROOM_ORCHESTRATOR_H
#define CLEANROOM_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ORCH_TASK_CAM = 0,
    ORCH_TASK_CAM_SERVER = 1,
    ORCH_TASK_CCAM = 2,
    ORCH_TASK_STREAM_SATIP = 3,
    ORCH_TASK_STREAM_RTSP = 4,
    ORCH_TASK_STREAM_DLNA = 5,
    ORCH_TASK_UPDATE = 6
} orch_task_id_t;

typedef enum {
    ORCH_TASK_STATE_IDLE = 0,
    ORCH_TASK_STATE_RUNNING = 1,
    ORCH_TASK_STATE_PAUSED = 2,
    ORCH_TASK_STATE_DONE = 3
} orch_task_state_t;

typedef struct {
    orch_task_id_t id;
    int state;
} orch_task_t;

orch_task_state_t orch_task_state_get(orch_task_t *t);
bool orch_task_start(orch_task_t *t, orch_task_id_t id);
bool orch_task_stop(orch_task_t *t, orch_task_id_t id);
bool orch_scheduler_init(void);
bool orch_scheduler_submit(orch_task_t *t, orch_task_id_t id);
bool orch_scheduler_run(orch_task_t *t);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_ORCHESTRATOR_H */
