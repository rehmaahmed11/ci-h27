#ifndef CLEANROOM_DLNA_SERVICE_H
#define CLEANROOM_DLNA_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DLNA_SSDP_PORT 1900
#define DLNA_SSDP_MULTICAST "239.255.255.1"

#define DLNA_SERVICE_CONTENT_DIRECTORY "urn:schemas-upnp-org:service:ContentDirectory:1"
#define DLNA_SERVICE_AVT             "urn:schemas-upnp-org:service:AVTransport:1"
#define DLNA_SERVICE_RENDERING_CONTROL "urn:schemas-upnp-org:service:RenderingControl:1"

typedef struct {
    uint16_t ssdp_port;
    uint8_t  local_addr[4];
    char     uuid[40];
    bool     enabled;
} dlna_service_t;

typedef enum {
    DLNA_RET_OK = 0,
    DLNA_RET_ERR = -1,
    DLNA_RET_ERR_BUILD = -2
} dlna_ret_t;

dlna_ret_t dlna_service_init(dlna_service_t *d);
dlna_ret_t dlna_service_handle_discovery(const uint8_t *req, uint8_t *resp, size_t cap, size_t *len);
dlna_ret_t dlna_service_avt_action(const char *method, const char *args, char *resp, size_t cap, size_t *len);
dlna_ret_t dlna_service_rc_action(const char *method, const char *args, char *resp, size_t cap, size_t *len);
dlna_ret_t dlna_service_cdd_action(const char *method, const char *object_id, char *resp, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_DLNA_SERVICE_H */
