#ifndef CLEANROOM_RUNTIME_H
#define CLEANROOM_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * End-to-end clean-room firmware runtime facade (P4).  Owns the file-descriptor
 * handles of the integrated subsystems and maps scheduler task ids to subsystem
 * lifecycle operations.
 */
typedef struct {
    int cam_server_fd;
    int ccam_fd;
    int satip_fd;
    int rtsp_fd;
    int dlna_fd;
} cleanroom_runtime_t;

int cleanroom_runtime_init(cleanroom_runtime_t *r);
void cleanroom_runtime_deinit(cleanroom_runtime_t *r);
int cleanroom_runtime_start_task(cleanroom_runtime_t *r, int task_id);
int cleanroom_runtime_stop_task(cleanroom_runtime_t *r, int task_id);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_RUNTIME_H */
