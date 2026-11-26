/*
 * cleanroom/satip_server_full.c
 * ----------------------------
 * Clean-room SAT>IP server facade (P4): device-scoped description/SDP builders
 * plus SSDP M-SEARCH responder (EN 50585).
 */

#include "satip_server_full.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

int satip_server_full_init(satip_server_full_t *s, const satip_device_t *dev)
{
    if ((s == NULL) || (dev == NULL)) { return -1; }
    s->dev = *dev;
    return 0;
}

int satip_server_full_build_desc(const satip_server_full_t *s, char *out, size_t cap)
{
    if ((s == NULL) || (out == NULL)) { return -1; }
    return satip_server_build_desc(&s->dev, out, cap);
}

int satip_server_full_build_sdp(const satip_server_full_t *s, uint16_t server_rtp_port,
                               uint32_t mcast_addr_be, char *out, size_t cap)
{
    if ((s == NULL) || (out == NULL)) { return -1; }
    return satip_server_build_sdp(&s->dev, server_rtp_port, mcast_addr_be, out, cap);
}

int satip_server_full_handle_msearch(const satip_server_full_t *s, const char *st,
                                    char *resp, size_t cap, size_t *len)
{
    int n;

    if ((s == NULL) || (resp == NULL) || (len == NULL) || (st == NULL)) { return -1; }
    n = snprintf(resp, cap,
                 "HTTP/1.0 200 OK\r\n"
                 "CACHE-CONTROL: max-age=1800\r\n"
                 "DATE: Thu, 01 Jan 1970 00:00:00 GMT\r\n"
                 "SERVER: Linux/2.6 UPnP/1.0 SATIP/1.0\r\n"
                 "LOCATION: http://%u.%u.%u.%u:%u/desc.xml\r\n"
                 "ST: %s\r\n"
                 "USN: uuid:12345678-1234-1234-1234-123456789012:1\r\n"
                 "\r\n",
                 192, 168, 100, 50, 8080, st);
    if (n < 0) { return -1; }
    *len = (size_t)n;
    return 0;
}
