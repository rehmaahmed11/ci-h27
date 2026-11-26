#ifndef CLEANROOM_RTSP_SERVER_FULL_H
#define CLEANROOM_RTSP_SERVER_FULL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "rtsp_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    rtsp_server_t srv;
    int transport_proto;
} rtsp_server_full_t;

int rtsp_server_full_init(rtsp_server_full_t *r, uint16_t port, int transport_proto);
int rtsp_server_full_open(rtsp_server_full_t *r, int *out_cfd);
int rtsp_server_full_process(rtsp_conn_t *c, const char *method, const char *uri,
                             const char *sdp_body, size_t sdp_len,
                             char *resp, size_t cap, size_t *resp_len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_RTSP_SERVER_FULL_H */
