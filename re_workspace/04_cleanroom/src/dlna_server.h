#ifndef CLEANROOM_DLNA_SERVER_H
#define CLEANROOM_DLNA_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dlna_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dlna_service_t svc;
} dlna_server_t;

int dlna_server_init(dlna_server_t *d, uint16_t ssdp_port);
int dlna_server_handle_msearch(const dlna_server_t *d, const char *st,
                               char *resp, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_DLNA_SERVER_H */
