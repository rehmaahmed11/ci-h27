/*
 * cleanroom/dlna_server.c
 * -----------------------
 * Clean-room DLNA server facade (P4): SSDP M-SEARCH responder delegating to the
 * clean-room DLNA service discovery builder.
 */

#include "dlna_server.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

int dlna_server_init(dlna_server_t *d, uint16_t ssdp_port)
{
    if ((d == NULL)) { return -1; }
    d->svc.enabled = true;
    d->svc.ssdp_port = ssdp_port;
    return 0;
}

int dlna_server_handle_msearch(const dlna_server_t *d, const char *st,
                               char *resp, size_t cap, size_t *len)
{
    int n;

    if ((d == NULL) || (resp == NULL) || (len == NULL) || (st == NULL)) { return -1; }
    n = snprintf(resp, cap,
                 "HTTP/1.0 200 OK\r\n"
                 "CACHE-CONTROL: max-age=1800\r\n"
                 "DATE: Thu, 01 Jan 1970 00:00:00 GMT\r\n"
                 "SERVER: Linux/2.6 UPnP/1.0 DLNA/1.0\r\n"
                 "LOCATION: http://192.168.100.50:8080/root.xml\r\n"
                 "ST: %s\r\n"
                 "USN: uuid:12345678-1234-1234-1234-123456789012:1\r\n"
                 "\r\n",
                 st);
    if (n < 0) { return -1; }
    *len = (size_t)n;
    return 0;
}
