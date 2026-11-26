#ifndef CLEANROOM_SATIP_SERVER_H
#define CLEANROOM_SATIP_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EN 50585 SAT>IP discovery / description surface (clean-room re-implementation). */
#define SATIP_SSDP_PORT 1900u
#define SATIP_SSDP_MULTICAST "239.255.255.1"
#define SATIP_DESC_PATH "/desc.xml"
#define SATIP_MAX_PID 8191

typedef struct {
    char base_url[64];
    char model[64];
    uint16_t vendor_id;
    uint16_t product_id;
    uint32_t ip_be; /* device IPv4 address, big-endian */
    uint16_t freq; /* MHz */
    uint16_t pol;  /* 0 = auto / 1 = H */
    uint16_t msys; /* 0 = auto */
    uint32_t sr;   /* symbol rate */
    uint16_t pids[8];
    int pid_count;
} satip_device_t;

typedef enum {
    SATIP_RET_OK = 0,
    SATIP_RET_ERR = -1,
    SATIP_RET_ERR_BUILD = -2
} satip_ret_t;

satip_ret_t satip_server_init(void);
satip_ret_t satip_server_build_desc(const satip_device_t *d, char *out, size_t cap);
satip_ret_t satip_server_build_sdp(const satip_device_t *d, uint16_t server_rtp_port,
                                  uint32_t mcast_addr_be, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_SATIP_SERVER_H */
