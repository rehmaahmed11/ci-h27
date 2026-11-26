/*
 * cleanroom/dlna_service.c
 * -----------------------
 * Clean-room DLNA (UPnP AV) service helpers: SSDP discovery response, SOAP
 * AVTransport / RenderingControl / ContentDirectory action envelopes.
 */

#include "dlna_service.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

dlna_ret_t dlna_service_init(dlna_service_t *d)
{
    if (d == NULL) { return DLNA_RET_ERR; }
    d->ssdp_port = (uint16_t)DLNA_SSDP_PORT;
    d->local_addr[0] = 192u;
    d->local_addr[1] = 168u;
    d->local_addr[2] = 100u;
    d->local_addr[3] = 50u;
    d->uuid[0] = '\0';
    d->enabled = true;
    return DLNA_RET_OK;
}

dlna_ret_t dlna_service_handle_discovery(const uint8_t *req, uint8_t *resp, size_t cap, size_t *len)
{
    int n;

    (void)req;
    if ((resp == NULL) || (len == NULL)) { return DLNA_RET_ERR; }
    if (cap < 256u) { return DLNA_RET_ERR; }

    n = snprintf((char *)resp, cap,
                 "HTTP/1.0 200 OK\r\n"
                 "CACHE-CONTROL: max-age=1800\r\n"
                 "DATE: Thu, 01 Jan 1970 00:00:00 GMT\r\n"
                 "SERVER: Linux/2.6 UPnP/1.0 CleanRoomDLNA/1.0\r\n"
                 "LOCATION: http://192.168.100.50:8080/root.xml\r\n"
                 "ST: %s\r\n"
                 "USN: uuid:12345678-1234-1234-1234-123456789012\r\n"
                 "\r\n",
                 DLNA_SERVICE_AVT);
    if (n < 0) { return DLNA_RET_ERR; }
    *len = (size_t)n;
    return DLNA_RET_OK;
}

dlna_ret_t dlna_service_avt_action(const char *method, const char *args, char *resp, size_t cap, size_t *len)
{
    char body[512];
    int n;

    (void)args;
    if ((resp == NULL) || (len == NULL)) { return DLNA_RET_ERR; }
    if (cap < 512u) { return DLNA_RET_ERR; }

    if ((method != NULL) && (strcmp(method, "GetCurrentTransportState") == 0)) {
        snprintf(body, sizeof(body), "<NewTransportState>TRANSIENT</NewTransportState><NewPlaybackState>PLAYING</NewPlaybackState>");
    } else {
        body[0] = '\0';
    }

    n = snprintf(resp, cap,
                 "<?xml version=\"1.0\"?>\r\n"
                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n"
                 "<SOAP-ENV:Body>\r\n"
                 "<u:%sResponse xmlns:u=\"%s\">\r\n%s</u:%sResponse>\r\n"
                 "</SOAP-ENV:Body>\r\n"
                 "</SOAP-ENV:Envelope>\r\n",
                 method != NULL ? method : "Unknown", DLNA_SERVICE_AVT, body, "Response");
    if (n < 0) { return DLNA_RET_ERR; }
    *len = (size_t)n;
    return DLNA_RET_OK;
}

dlna_ret_t dlna_service_rc_action(const char *method, const char *args, char *resp, size_t cap, size_t *len)
{
    char body[256];
    int n;

    (void)args;
    if ((resp == NULL) || (len == NULL)) { return DLNA_RET_ERR; }
    if (cap < 512u) { return DLNA_RET_ERR; }

    if ((method != NULL) && (strcmp(method, "GetState") == 0)) {
        snprintf(body, sizeof(body), "<InstanceID>0</InstanceID><ChannelID>1</ChannelID><Level>0</Level>");
    } else {
        body[0] = '\0';
    }

    n = snprintf(resp, cap,
                 "<?xml version=\"1.0\"?>\r\n"
                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n"
                 "<SOAP-ENV:Body>\r\n"
                 "<u:%sResponse xmlns:u=\"%s\">\r\n%s</u:%sResponse>\r\n"
                 "</SOAP-ENV:Body>\r\n"
                 "</SOAP-ENV:Envelope>\r\n",
                 method != NULL ? method : "Unknown", DLNA_SERVICE_RENDERING_CONTROL, body, "Response");
    if (n < 0) { return DLNA_RET_ERR; }
    *len = (size_t)n;
    return DLNA_RET_OK;
}

dlna_ret_t dlna_service_cdd_action(const char *method, const char *object_id, char *resp, size_t cap, size_t *len)
{
    char body[1024];
    int n;

    if ((resp == NULL) || (len == NULL)) { return DLNA_RET_ERR; }
    (void)object_id;
    if (cap < 2048u) { return DLNA_RET_ERR; }

    if ((method != NULL) && (strcmp(method, "GetSearchResults") == 0)) {
        snprintf(body, sizeof(body),
                 "<Query>Media</Query><ResultSet>"
                 "<Result>"
                 "<dc:identifier xmlns:dc=\"http://purl.org/dc/elements/1.1/\">%s</dc:identifier>"
                 "<dc:title>Satellite Channel</dc:title>"
                 "<dc:date>2026-01-01T00:00:00Z</dc:date>"
                 "</Result>"
                 "</ResultSet>",
                 (object_id != NULL) ? object_id : "0");
    } else {
        body[0] = '\0';
    }

    n = snprintf(resp, cap,
                 "<?xml version=\"1.0\"?>\r\n"
                 "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n"
                 "<SOAP-ENV:Body>\r\n"
                 "<u:%sResponse xmlns:u=\"%s\">\r\n%s</u:%sResponse>\r\n"
                 "</SOAP-ENV:Body>\r\n"
                 "</SOAP-ENV:Envelope>\r\n",
                 method != NULL ? method : "Unknown", DLNA_SERVICE_CONTENT_DIRECTORY, body, "Response");
    if (n < 0) { return DLNA_RET_ERR; }
    *len = (size_t)n;
    return DLNA_RET_OK;
}
