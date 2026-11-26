#ifndef CLEANROOM_NET_SOCKET_H
#define CLEANROOM_NET_SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal BSD-socket transport facade for clean-room streaming servers (P4). */
int net_socket_init(void);
int net_socket_tcp_listen(uint16_t port, int *out_fd);
int net_socket_tcp_accept(int fd, int *out_fd);
int net_socket_tcp_read(int fd, char *buf, size_t cap, size_t *len);
int net_socket_tcp_write(int fd, const char *buf, size_t len);
int net_socket_udp_bind(uint16_t port, int *out_fd);
int net_socket_udp_recv(int fd, char *buf, size_t cap, size_t *len, void *addr, void *addrlen);
int net_socket_udp_send(int fd, const char *buf, size_t len, const void *addr, void *addrlen);
void net_socket_close(int fd);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_NET_SOCKET_H */
