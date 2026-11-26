/*
 * cleanroom/rtsp_server_loop.c
 * ----------------------------
 * Clean-room RTSP server accept/read/process/write loop (P5).
 */

#include "rtsp_server_loop.h"
#include "net_socket.h"
#include "rtsp_server_full.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

int rtsp_server_loop_init(rtsp_server_loop_t *l, uint16_t port)
{
    if ((l == NULL)) { return -1; }
    l->port = port;
    return 0;
}

int rtsp_server_loop_open(rtsp_server_loop_t *l, int *out_fd)
{
    if ((l == NULL) || (out_fd == NULL)) { return -1; }
    *out_fd = -1;
    return net_socket_tcp_listen(l->port, out_fd);
}

int rtsp_server_loop_accept(int lfd, int *out_cfd)
{
    if ((lfd < 0) || (out_cfd == NULL)) { return -1; }
    *out_cfd = -1;
    return net_socket_tcp_accept(lfd, out_cfd);
}

int rtsp_server_loop_run(int cfd, rtsp_conn_t *c, char *resp, size_t cap, size_t *len)
{
    char req[2048];
    size_t req_len = 0;
    const char *sp1 = NULL;
    char method[16];
    size_t method_len = 0;
    int rc;

    if ((cfd < 0) || (c == NULL) || (resp == NULL) || (len == NULL)) { return -1; }
    if (net_socket_tcp_read(cfd, req, sizeof(req), &req_len) != 0) { return -1; }
    if (req_len < 8) { return -1; }

    sp1 = strchr(req, ' ');
    if (sp1 == NULL) { return -1; }
    if (strchr(sp1 + 1, ' ') == NULL) { return -1; }

    method_len = (size_t)(sp1 - req);
    if (method_len >= sizeof(method)) { method_len = sizeof(method) - 1; }
    memcpy(method, req, method_len);
    method[method_len] = '\0';

    *len = 0;
    rc = rtsp_server_full_process(c, method, (sp1 + 1), NULL, 0, resp, cap, len);
    if (rc != 0) { return rc; }
    if (*len > 0) {
        if (net_socket_tcp_write(cfd, resp, *len) != 0) { return -1; }
    }
    return 0;
}
