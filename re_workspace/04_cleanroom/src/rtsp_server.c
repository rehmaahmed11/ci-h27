/*
 * cleanroom/rtsp_server.c
 * ----------------------
 * Clean-room RTSP server session handling (RFC 2326 subset).
 *
 * Session state machine:
 *   IDLE -> SETUP? (via DESCRIBE) -> PLAY -> PAUSE -> PLAY | TEARDOWN -> IDLE
 *
 * Supported methods: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN,
 * GET_PARAMETER, SET_PARAMETER.
 * Transport: RTP over UDP (proto 0) or RTP over TCP interleaved (proto 1).
 */

#include "rtsp_server.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

__attribute__((unused)) static const char *rtsp_state_str(rtsp_conn_state_t s)
{
    switch (s) {
    case RTSP_CONN_IDLE:     return "IDLE";
    case RTSP_CONN_DESCRIBE: return "DESCRIBE";
    case RTSP_CONN_SETUP:    return "SETUP";
    case RTSP_CONN_PLAY:     return "PLAY";
    case RTSP_CONN_PAUSE:    return "PAUSE";
    case RTSP_CONN_OPTIONS:  return "OPTIONS";
    default:                 return "UNKNOWN";
    }
}

rtsp_ret_t rtsp_server_init(rtsp_server_t *s)
{
    if (s == NULL) { return RTSP_RET_ERR; }
    s->enabled = true;
    s->port = (uint16_t)RTSP_SERVER_PORT;
    return RTSP_RET_OK;
}

void rtsp_conn_reset(rtsp_conn_t *c)
{
    if (c == NULL) { return; }
    c->state = RTSP_CONN_IDLE;
    c->method[0] = '\0';
    c->uri[0] = '\0';
    c->sdp[0] = '\0';
    c->client_port = 0u;
    c->server_port = 0u;
    c->transport_proto = 0;
    c->session_id[0] = '\0';
    c->cseq = 0u;
}

void rtsp_conn_set_transport(rtsp_conn_t *c, int proto)
{
    if (c == NULL) { return; }
    c->transport_proto = proto;
}

const char *rtsp_transport_str(int proto)
{
    if (proto == 1) { return "RTP;interleaved=UDP"; }
    return "RTP;unicast;client_port=5000;server_port=5002;mode=PLAY";
}

static rtsp_ret_t rtsp_simple(rtsp_conn_t *c, int code, const char *text,
                              const char *extra_headers,
                              char *resp, size_t cap, size_t *resp_len)
{
    int n;
    size_t used;

    if ((resp == NULL) || (cap == 0u)) { return RTSP_RET_ERR_BUILD; }
    n = snprintf(resp, cap, "RTSP/1.0 %d %s\r\nCSeq: %u\r\n", code, text, c->cseq);
    if ((n < 0) || ((size_t)n >= cap)) { return RTSP_RET_ERR_BUILD; }
    if ((extra_headers != NULL) && (extra_headers[0] != '\0')) {
        used = (size_t)n + 1u;
        if (used < cap) {
            int m = snprintf(resp + used, (cap - used), "%s\r\n", extra_headers);
            if (m < 0) { return RTSP_RET_ERR_BUILD; }
            n = (int)((size_t)n + (size_t)m);
            if ((size_t)n >= cap) { return RTSP_RET_ERR_BUILD; }
        }
    }
    if (resp_len != NULL) { *resp_len = (size_t)n; }
    return RTSP_RET_OK;
}

static rtsp_ret_t rtsp_sdp_resp(rtsp_conn_t *c, const char *sdp, size_t sdp_len,
                               char *resp, size_t cap, size_t *resp_len)
{
    int n;

    if ((resp == NULL) || (cap == 0u)) { return RTSP_RET_ERR_BUILD; }
    n = snprintf(resp, cap,
                 "RTSP/1.0 200 OK\r\nCSeq: %u\r\nSession: %s\r\nContent-Type: application/sdp\r\nContent-Length: %u\r\n\r\n",
                 c->cseq, c->session_id, (unsigned)sdp_len);
    if ((n < 0) || ((size_t)n >= cap)) { return RTSP_RET_ERR_BUILD; }
    memcpy(resp + n, sdp, sdp_len);
    if (resp_len != NULL) { *resp_len = (size_t)n + sdp_len; }
    return RTSP_RET_OK;
}

rtsp_ret_t rtsp_conn_handle(rtsp_conn_t *c, const char *method, const char *request_uri,
                            const char *sdp_body, size_t sdp_len,
                            char *resp, size_t resp_cap, size_t *resp_len)
{
    int rc;
    char extra[256];

    if ((c == NULL) || (resp == NULL) || (resp_len == NULL) || (method == NULL)) { return RTSP_RET_ERR; }
    (void)request_uri;
    c->cseq += 1u;

    if (strcmp(method, "OPTIONS") == 0) {
        rc = rtsp_simple(c, 200, "OK",
                         "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER, SET_PARAMETER",
                         resp, resp_cap, resp_len);
        return rc;
    }

    if (strcmp(method, "DESCRIBE") == 0) {
        if (c->state != RTSP_CONN_IDLE) {
            return rtsp_simple(c, 405, "Method Not Allowed", NULL, resp, resp_cap, resp_len);
        }
        if ((sdp_body == NULL) || (sdp_len == 0u)) {
            return rtsp_simple(c, 400, "Bad Request", NULL, resp, resp_cap, resp_len);
        }
        snprintf(c->sdp, sizeof(c->sdp), "%s", sdp_body);
        c->state = RTSP_CONN_SETUP;
        return rtsp_sdp_resp(c, c->sdp, sdp_len, resp, resp_cap, resp_len);
    }

    if (strcmp(method, "SETUP") == 0) {
        if (c->state != RTSP_CONN_SETUP) {
            return rtsp_simple(c, 405, "Method Not Allowed", NULL, resp, resp_cap, resp_len);
        }
        if (c->transport_proto == 0) {
            snprintf(c->session_id, sizeof(c->session_id), "S%08x", (unsigned)c->cseq);
            snprintf(extra, sizeof(extra), "Session: %s\r\nTransport: %s",
                     c->session_id, rtsp_transport_str(c->transport_proto));
            rc = rtsp_simple(c, 200, "OK", extra, resp, resp_cap, resp_len);
            c->state = RTSP_CONN_PLAY;
            return rc;
        }
        return rtsp_simple(c, 461, "Unsupported Transport", NULL, resp, resp_cap, resp_len);
    }

    if (strcmp(method, "PLAY") == 0) {
        if (c->state != RTSP_CONN_PLAY) {
            return rtsp_simple(c, 405, "Method Not Allowed", NULL, resp, resp_cap, resp_len);
        }
        return rtsp_simple(c, 200, "OK", NULL, resp, resp_cap, resp_len);
    }

    if (strcmp(method, "PAUSE") == 0) {
        if (c->state == RTSP_CONN_PLAY) { c->state = RTSP_CONN_PAUSE; }
        return rtsp_simple(c, 200, "OK", NULL, resp, resp_cap, resp_len);
    }

    if (strcmp(method, "TEARDOWN") == 0) {
        c->state = RTSP_CONN_IDLE;
        return rtsp_simple(c, 200, "OK", NULL, resp, resp_cap, resp_len);
    }

    if ((strcmp(method, "GET_PARAMETER") == 0) || (strcmp(method, "SET_PARAMETER") == 0)) {
        if (c->state == RTSP_CONN_PLAY) {
            return rtsp_simple(c, 200, "OK", NULL, resp, resp_cap, resp_len);
        }
        return rtsp_simple(c, 405, "Method Not Allowed", NULL, resp, resp_cap, resp_len);
    }

    return rtsp_simple(c, 501, "Not Implemented", NULL, resp, resp_cap, resp_len);
}
