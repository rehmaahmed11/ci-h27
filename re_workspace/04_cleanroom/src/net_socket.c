/*
 * cleanroom/net_socket.c
 * ---------------------
 * Minimal BSD-socket transport facade (TCP/UDP) for the clean-room streaming
 * servers (P4).  Standard POSIX sockets via <sys/socket.h>.
 */

#include "net_socket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

int net_socket_init(void)
{
    return 0;
}

int net_socket_tcp_listen(uint16_t port, int *out_fd)
{
    int fd = -1;
    struct sockaddr_in sa;

    if ((out_fd == NULL)) { return -1; }
    *out_fd = -1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return -1; }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(fd); return -1; }
    if (listen(fd, 5) != 0) { close(fd); return -1; }
    *out_fd = fd;
    return 0;
}

int net_socket_tcp_accept(int fd, int *out_fd)
{
    int cfd = -1;

    if ((out_fd == NULL)) { return -1; }
    *out_fd = -1;
    cfd = accept(fd, NULL, NULL);
    if (cfd < 0) { return -1; }
    *out_fd = cfd;
    return 0;
}

int net_socket_tcp_read(int fd, char *buf, size_t cap, size_t *len)
{
    size_t total = 0;

    if ((buf == NULL) || (len == NULL)) { return -1; }
    *len = 0;
    while (total < (cap - 1)) {
        ssize_t n = recv(fd, buf + total, (cap - 1 - total), 0);
        if (n <= 0) { break; }
        total += (size_t)n;
        buf[total] = '\0';
        if ((total >= 4) && (buf[total - 4] == '\r') && (buf[total - 3] == '\n') && (buf[total - 2] == '\r') && (buf[total - 1] == '\n')) { break; }
    }
    *len = total;
    buf[total] = '\0';
    return (int)total;
}

int net_socket_tcp_write(int fd, const char *buf, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, buf + sent, (len - sent), MSG_NOSIGNAL);
        if (n <= 0) { return -1; }
        sent += (size_t)n;
    }
    return 0;
}

int net_socket_udp_bind(uint16_t port, int *out_fd)
{
    int fd = -1;
    struct sockaddr_in sa;

    if ((out_fd == NULL)) { return -1; }
    *out_fd = -1;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { return -1; }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(fd); return -1; }
    *out_fd = fd;
    return 0;
}

int net_socket_udp_recv(int fd, char *buf, size_t cap, size_t *len, void *addr, void *addrlen)
{
    struct sockaddr_in a;
    socklen_t alen = sizeof(a);

    if ((buf == NULL) || (len == NULL)) { return -1; }
    *len = 0;
    {
        ssize_t n = recvfrom(fd, buf, (cap - 1), 0, (struct sockaddr *)&a, &alen);
        if (n <= 0) { return -1; }
        *len = (size_t)n;
        buf[*len] = '\0';
        if (addr != NULL) { memcpy(addr, &a, sizeof(a)); }
        if (addrlen != NULL) { *(socklen_t *)addrlen = sizeof(a); }
    }
    return 0;
}

int net_socket_udp_send(int fd, const char *buf, size_t len, const void *addr, void *addrlen)
{
    struct sockaddr_in a;

    if ((buf == NULL) || (len == 0) || (addr == NULL) || (addrlen == NULL)) { return -1; }
    memcpy(&a, addr, sizeof(a));
    if (sendto(fd, buf, len, 0, (struct sockaddr *)&a, sizeof(a)) != (ssize_t)len) { return -1; }
    return 0;
}

void net_socket_close(int fd)
{
    if (fd < 0) { return; }
    close(fd);
}
