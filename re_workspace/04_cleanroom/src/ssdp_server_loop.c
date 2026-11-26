/*
 * cleanroom/ssdp_server_loop.c
 * ----------------------------
 * Clean-room SSDP responder loop (P5): UDP bind + M-SEARCH receive + 200 OK reply.
 */

#include "ssdp_server_loop.h"
#include "net_socket.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

int ssdp_server_loop_init(ssdp_server_loop_t *l, uint16_t port, const char *st)
{
    if ((l == NULL) || (st == NULL)) { return -1; }
    l->port = port;
    memset(l->st, 0, sizeof(l->st));
    snprintf(l->st, sizeof(l->st), "%s", st);
    return 0;
}

int ssdp_server_loop_open(ssdp_server_loop_t *l, int *out_fd)
{
    if ((l == NULL) || (out_fd == NULL)) { return -1; }
    *out_fd = -1;
    return net_socket_udp_bind(l->port, out_fd);
}

int ssdp_server_loop_run(int fd, const char *st, char *resp, size_t cap, size_t *len)
{
    char buf[1024];
    size_t buf_len = 0;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int n;

    if ((fd < 0) || (st == NULL) || (resp == NULL) || (len == NULL)) { return -1; }
    memset(&addr, 0, sizeof(addr));
    if (net_socket_udp_recv(fd, buf, sizeof(buf), &buf_len, &addr, &addrlen) != 0) { return -1; }
    n = snprintf(resp, cap,
                 "HTTP/1.0 200 OK\r\n"
                 "CACHE-CONTROL: max-age=1800\r\n"
                 "DATE: Thu, 01 Jan 1970 00:00:00 GMT\r\n"
                 "SERVER: Linux/2.6 UPnP/1.0 CLEANROOM_SSDP/1.0\r\n"
                 "LOCATION: http://192.168.100.50:8080/root.xml\r\n"
                 "ST: %s\r\n"
                 "USN: uuid:12345678-1234-1234-1234-123456789012:1\r\n"
                 "\r\n",
                 st);
    if (n < 0) { return -1; }
    *len = (size_t)n;
    if (net_socket_udp_send(fd, resp, *len, &addr, &addrlen) != 0) { return -1; }
    return 0;
}
