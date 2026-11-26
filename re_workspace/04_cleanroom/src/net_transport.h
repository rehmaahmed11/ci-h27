/*
 * cleanroom/net_transport.h
 * -------------------------
 * Clean-room network transport layer (HTTP / FTP / RTSP / HTTPS / ICY) — P1 subsystem.
 *
 * Recovered protocol surface:
 *   HTTP/1.0, HTTP/1.1, HTTPS, FTP, RTSP/1.x, ICY
 *   - cookies: "# Netscape HTTP Cookie File", "#HttpOnly_", "httponly"
 *   - auth: Basic, Digest, "# Http://curl.haxx.se/docs/http-cookies.html"
 *   - byte-range resume; chunked upload NOT supported (HTTP 1.0)
 *   - "CONNECT %s HTTP/%s" proxy tunneling; "ICY"
 */

#ifndef CLEANROOM_NET_TRANSPORT_H
#define CLEANROOM_NET_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_RET_OK = 0,
    NET_RET_ERR = -1,
    NET_RET_ERR_TLS = -2,
    NET_RET_ERR_AUTH = -3,
    NET_RET_ERR_RANGE = -4,
    NET_RET_ERR_NOTSUPP = -5
} net_ret_t;

typedef enum {
    NET_PROTO_HTTP = 0,
    NET_PROTO_HTTPS = 1,
    NET_PROTO_RTSP = 2,
    NET_PROTO_FTP = 3,
    NET_PROTO_ICY = 4,
    NET_PROTO_MMS = 5
} net_proto_t;

typedef struct {
    net_proto_t proto;
    int version_major;
    int version_minor;
} net_transport_t;

net_ret_t net_transport_open(net_transport_t *t);
net_ret_t net_http_download(const char *url, uint8_t **out, size_t *out_len);
net_ret_t net_rtsp_open(const char *url, void *sess);
net_ret_t net_ftp_upload(const char *host, const char *user, const char *pass, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_NET_TRANSPORT_H */
