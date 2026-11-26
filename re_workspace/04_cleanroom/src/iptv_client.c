/*
 * cleanroom/iptv_client.c
 * -----------------------
 * Clean-room IPTV backend activation/run HTTP client (P2 integration layer).
 * Posts the recovered JSON activation payload shape and captures the response.
 */

#include "iptv_client.h"
#include "http_client.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

ipt_client_ret_t iptv_backend_call(const char *base, const char *action, const char *json_body,
                                  char *resp, size_t cap, size_t *len)
{
    char path[256];
    http_client_t c;
    http_resp_t r;
    ipt_client_ret_t ret = IPT_CLIENT_RET_ERR;

    if ((base == NULL) || (action == NULL) || (resp == NULL) || (len == NULL)) { return IPT_CLIENT_RET_ERR; }
    if (snprintf(path, sizeof(path), "%s/%s.php", base, action) >= (int)sizeof(path)) { return IPT_CLIENT_RET_ERR; }

    if (http_client_init(&c, HTTP_TLS_CLEAR) != HTTP_RET_OK) { return IPT_CLIENT_RET_ERR; }
    if (http_request("127.0.0.1", 80, HTTP_METH_POST, path, json_body, (json_body != NULL) ? strlen(json_body) : 0, &r) != HTTP_RET_OK) { return IPT_CLIENT_RET_ERR; }
    if (r.status < 200) { http_resp_free(&r); return IPT_CLIENT_RET_ERR_HTTP; }
    if ((size_t)r.body_len >= cap) { http_resp_free(&r); return IPT_CLIENT_RET_ERR; }
    memcpy(resp, r.body, r.body_len);
    resp[r.body_len] = '\0';
    *len = r.body_len;
    http_resp_free(&r);
    ret = IPT_CLIENT_RET_OK;
    return ret;
}
