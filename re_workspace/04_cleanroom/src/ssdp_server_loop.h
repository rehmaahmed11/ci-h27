#ifndef CLEANROOM_SSDP_SERVER_LOOP_H
#define CLEANROOM_SSDP_SERVER_LOOP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t port;
    char st[64];
} ssdp_server_loop_t;

int ssdp_server_loop_init(ssdp_server_loop_t *l, uint16_t port, const char *st);
int ssdp_server_loop_open(ssdp_server_loop_t *l, int *out_fd);
int ssdp_server_loop_run(int fd, const char *st, char *resp, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_SSDP_SERVER_LOOP_H */
