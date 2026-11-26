/*
 * cleanroom/net_transport.c
 * -------------------------
 * Clean-room network transport layer implementation.
 */

#include "net_transport.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

net_ret_t net_transport_open(net_transport_t *t)
{
    if (t == NULL) { return NET_RET_ERR; }
    t->proto = NET_PROTO_HTTP;
    t->version_major = 1;
    t->version_minor = 1;
    return NET_RET_OK;
}

net_ret_t net_http_download(const char *url, uint8_t **out, size_t *out_len)
{
    if ((url == NULL) || (out == NULL) || (out_len == NULL)) { return NET_RET_ERR; }
    *out = NULL;
    *out_len = 0;
    /* Real implementation resolves host/port and issues HTTP/1.0|1.1|HTTPS,
       honours cookies, Basic/Digest auth and byte-range resume. */
    return NET_RET_ERR_NOTSUPP;
}

net_ret_t net_rtsp_open(const char *url, void *sess)
{
    if ((url == NULL) || (sess == NULL)) { return NET_RET_ERR; }
    /* RTSP/1.x session: DESCRIBE -> SETUP -> PLAY -> TEARDOWN / Pause / Option */
    return NET_RET_ERR_NOTSUPP;
}

net_ret_t net_ftp_upload(const char *host, const char *user, const char *pass,
                        const uint8_t *data, size_t len)
{
    if ((host == NULL) || (data == NULL) || (len == 0u)) { return NET_RET_ERR; }
    (void)user; (void)pass;
    /* "ftp://%s:%s@%s" ; FTP response handling */
    return NET_RET_ERR_NOTSUPP;
}

