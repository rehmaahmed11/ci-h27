#ifndef CLEANROOM_RTSP_SERVER_H
#define CLEANROOM_RTSP_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTSP_SERVER_PORT 554u
#define RTSP_SDP_MAX 1024

typedef enum {
    RTSP_CONN_IDLE = 0,
    RTSP_CONN_DESCRIBE,
    RTSP_CONN_SETUP,
    RTSP_CONN_PLAY,
    RTSP_CONN_PAUSE,
    RTSP_CONN_OPTIONS
} rtsp_conn_state_t;

typedef struct {
    rtsp_conn_state_t state;
    char method[16];
    char uri[128];
    char sdp[RTSP_SDP_MAX];
    uint16_t client_port;
    uint16_t server_port;
    int transport_proto; /* 0 = RTP/UDP, 1 = RTP/TCP interleaved */
    char session_id[40];
    uint32_t cseq;
} rtsp_conn_t;

typedef struct {
    bool enabled;
    uint16_t port;
} rtsp_server_t;

typedef enum {
    RTSP_RET_OK = 0,
    RTSP_RET_ERR = -1,
    RTSP_RET_ERR_STATE = -2,
    RTSP_RET_ERR_TRANS = -3,
    RTSP_RET_ERR_BUILD = -4
} rtsp_ret_t;

rtsp_ret_t rtsp_server_init(rtsp_server_t *s);
rtsp_ret_t rtsp_conn_handle(rtsp_conn_t *c, const char *method, const char *request_uri,
                            const char *sdp_body, size_t sdp_len,
                            char *resp, size_t resp_cap, size_t *resp_len);
void rtsp_conn_reset(rtsp_conn_t *c);
const char *rtsp_transport_str(int proto);
void rtsp_conn_set_transport(rtsp_conn_t *c, int proto);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_RTSP_SERVER_H */
