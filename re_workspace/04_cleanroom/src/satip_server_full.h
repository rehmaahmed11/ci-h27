#ifndef CLEANROOM_SATIP_SERVER_FULL_H
#define CLEANROOM_SATIP_SERVER_FULL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "satip_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    satip_device_t dev;
} satip_server_full_t;

int satip_server_full_init(satip_server_full_t *s, const satip_device_t *dev);
int satip_server_full_build_desc(const satip_server_full_t *s, char *out, size_t cap);
int satip_server_full_build_sdp(const satip_server_full_t *s, uint16_t server_rtp_port,
                               uint32_t mcast_addr_be, char *out, size_t cap);
int satip_server_full_handle_msearch(const satip_server_full_t *s, const char *st,
                                    char *resp, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_SATIP_SERVER_FULL_H */
