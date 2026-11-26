#ifndef CLEANROOM_RTSP_SERVER_LOOP_H
#define CLEANROOM_RTSP_SERVER_LOOP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "rtsp_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t port;
} rtsp_server_loop_t;

int rtsp_server_loop_init(rtsp_server_loop_t *l, uint16_t port);
int rtsp_server_loop_open(rtsp_server_loop_t *l, int *out_fd);
int rtsp_server_loop_accept(int lfd, int *out_cfd);
int rtsp_server_loop_run(int cfd, rtsp_conn_t *c, char *resp, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_RTSP_SERVER_LOOP_H */
