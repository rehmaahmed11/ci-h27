/*
 * cleanroom/orchestrator.c
 * ------------------------
 * Clean-room task scheduler / orchestration facade (P3 integration layer).
 * Wires the recovered subsystems (CAM server, CCcam client, streamer front-ends,
 * update manager) behind a uniform task interface.
 */

#include "orchestrator.h"
#include <stddef.h>
#include <stdbool.h>

orch_task_state_t orch_task_state_get(orch_task_t *t)
{
    if (t == NULL) { return ORCH_TASK_STATE_IDLE; }
    return (orch_task_state_t)t->state;
}

bool orch_task_start(orch_task_t *t, orch_task_id_t id)
{
    if ((t == NULL) || (t->id != id)) { return false; }
    t->state = (int)ORCH_TASK_STATE_RUNNING;
    return true;
}

bool orch_task_stop(orch_task_t *t, orch_task_id_t id)
{
    if ((t == NULL) || (t->id != id)) { return false; }
    t->state = (int)ORCH_TASK_STATE_DONE;
    return true;
}

bool orch_scheduler_init(void)
{
    return true;
}

bool orch_scheduler_submit(orch_task_t *t, orch_task_id_t id)
{
    if ((t == NULL) || (t->id != id)) { return false; }
    t->state = (int)ORCH_TASK_STATE_IDLE;
    return true;
}

bool orch_scheduler_run(orch_task_t *t)
{
    orch_task_state_t s;

    if ((t == NULL) || (t->id == (int)ORCH_TASK_CAM)) { return true; }
    s = orch_task_state_get(t);
    if (s == ORCH_TASK_STATE_RUNNING) { t->state = (int)ORCH_TASK_STATE_DONE; }
    return true;
}
