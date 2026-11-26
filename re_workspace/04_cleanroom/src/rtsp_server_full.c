/*
 * cleanroom/rtsp_server_full.c
 * ---------------------------
 * Clean-room RTSP server facade on real sockets (P4): TCP control listener +
 * request processing delegated to the clean-room RFC 2326 session state machine.
 */

#include "rtsp_server_full.h"
#include "net_socket.h"
#include "rtsp_server.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

int rtsp_server_full_init(rtsp_server_full_t *r, uint16_t port, int transport_proto)
{
    if ((r == NULL)) { return -1; }
    r->srv.enabled = true;
    r->srv.port = port;
    r->transport_proto = transport_proto;
    return 0;
}

int rtsp_server_full_open(rtsp_server_full_t *r, int *out_cfd)
{
    if ((r == NULL) || (out_cfd == NULL)) { return -1; }
    *out_cfd = -1;
    if (net_socket_tcp_listen(r->srv.port, out_cfd) != 0) { return -1; }
    return 0;
}

int rtsp_server_full_process(rtsp_conn_t *c, const char *method, const char *uri,
                             const char *sdp_body, size_t sdp_len,
                             char *resp, size_t cap, size_t *resp_len)
{
    if ((c == NULL) || (resp == NULL) || (resp_len == NULL) || (method == NULL) || (uri == NULL)) { return -1; }
    if (c->method[0] == '\0') { snprintf(c->method, sizeof(c->method), "%s", method); }
    return rtsp_conn_handle(c, method, uri, sdp_body, sdp_len, resp, cap, resp_len);
}
