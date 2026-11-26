/*
 * cleanroom/yt_api.c
 * ------------------
 * Clean-room YouTube Data/API player-config client (P2 integration layer).
 * Builds the player/config request URL from the recovered API surface and
 * executes it through the clean-room HTTP client (TLS via http_client).
 */

#include "yt_api.h"
#include "http_client.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

yt_ret_t yt_api_init(yt_api_t *y, const char *api_key)
{
    if ((y == NULL) || (api_key == NULL)) { return YT_RET_ERR; }
    memset(y, 0, sizeof(*y));
    snprintf(y->api_key, sizeof(y->api_key), "%s", api_key);
    return YT_RET_OK;
}

yt_ret_t yt_api_get_player_config(const yt_api_t *y, const char *video_id, char *out, size_t cap, size_t *len)
{
    char url[512];
    http_client_t c;
    http_resp_t r;
    yt_ret_t ret = YT_RET_ERR;

    if ((y == NULL) || (out == NULL) || (len == NULL)) { return YT_RET_ERR; }
    if (y->api_key[0] == '\0') { return YT_RET_ERR_KEY; }
    if (video_id == NULL) { return YT_RET_ERR; }

    snprintf(url, sizeof(url), "%s/player?part=stream&key=%s&videoId=%s",
             YT_API_BASE, y->api_key, video_id);

    if (http_client_init(&c, HTTP_TLS_TLS13) != HTTP_RET_OK) { return YT_RET_ERR; }
    if (http_request("www.googleapis.com", 443, HTTP_METH_GET, url, NULL, 0, &r) != HTTP_RET_OK) { return YT_RET_ERR; }
    if (r.status < 200) { http_resp_free(&r); return YT_RET_ERR; }
    if ((size_t)r.body_len >= cap) { http_resp_free(&r); return YT_RET_ERR; }
    memcpy(out, r.body, r.body_len);
    out[r.body_len] = '\0';
    *len = r.body_len;
    http_resp_free(&r);
    ret = YT_RET_OK;
    return ret;
}
