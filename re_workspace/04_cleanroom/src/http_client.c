/*
 * cleanroom/http_client.c
 * -----------------------
 * Clean-room HTTP/1.1 client with optional TLS (TLS1.2 / TLS1.3) — P2 integration
 * layer.  Minimal request execution: connect, send headers + optional body, read
 * status line + headers + body (Content-Length delimited).
 */

#include "http_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <stddef.h>
#include <stdint.h>

http_ret_t http_client_init(http_client_t *c, int tls_version)
{
    if (c == NULL) { return HTTP_RET_ERR; }
    c->fd = -1;
    c->tls_version = tls_version;
    return HTTP_RET_OK;
}

static SSL *http_tls_connect(int fd, int tls_version)
{
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;

    if (tls_version == HTTP_TLS_TLS13) { ctx = SSL_CTX_new(TLSv1_3_method()); }
    else { ctx = SSL_CTX_new(TLSv1_2_method()); }
    if (ctx == NULL) { return NULL; }
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    SSL_set_connect_state(ssl);
    if (SSL_connect(ssl) != 1) { SSL_free(ssl); SSL_CTX_free(ctx); return NULL; }
    SSL_CTX_free(ctx);
    return ssl;
}

static int http_send_all(int fd, SSL *ssl, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = 0;
        if (ssl != NULL) { n = SSL_write(ssl, data + sent, (int)(len - sent)); }
        else { n = send(fd, data + sent, (int)(len - sent), MSG_NOSIGNAL); }
        if (n <= 0) { return -1; }
        sent += (size_t)n;
    }
    return 0;
}

static int http_recv(int fd, SSL *ssl, char *buf, size_t cap, size_t *len)
{
    size_t total = 0;

    while (total < (cap - 1)) {
        int n = 0;
        if (ssl != NULL) { n = SSL_read(ssl, buf + total, (int)(cap - 1 - total)); }
        else { n = recv(fd, buf + total, (int)(cap - 1 - total), 0); }
        if (n <= 0) { return -1; }
        total += (size_t)n;
        buf[total] = '\0';
        if ((total >= 4) && (buf[total - 4] == '\r') && (buf[total - 3] == '\n') && (buf[total - 2] == '\r') && (buf[total - 1] == '\n')) { break; }
    }
    *len = total;
    buf[total] = '\0';
    return 0;
}

http_ret_t http_request(const char *host, uint16_t port, http_method_t method, const char *path,
                       const char *body, size_t body_len, http_resp_t *out)
{
    int fd = -1;
    SSL *ssl = NULL;
    char req[2048];
    char resp[8192];
    size_t resp_len = 0;
    long content_len = -1;
    int status = 0;
    size_t body_off = 0;
    size_t req_len = 0;
    int n;
    /* http_request() carries no explicit TLS argument (http_client_t does), so
     * the transport is selected from the conventional HTTPS port. */
    int tls_version = (port == 443u) ? HTTP_TLS_TLS12 : HTTP_TLS_CLEAR;

    if ((out == NULL) || (host == NULL) || (path == NULL)) { return HTTP_RET_ERR; }
    out->status = 0;
    out->body = NULL;
    out->body_len = 0;
    out->tls_version = tls_version;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { return HTTP_RET_ERR_CONN; }
    {
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        if (inet_pton(AF_INET, host, &sa.sin_addr) != 1) { close(fd); return HTTP_RET_ERR_CONN; }
        if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(fd); return HTTP_RET_ERR_CONN; }
    }

    n = snprintf(req, sizeof(req),
                 "%s %s HTTP/1.1\r\nHost: %s:%u\r\nUser-Agent: CleanRoomHTTP/1.0\r\n"
                 "Accept: */*\r\nConnection: close\r\n",
                 (method == HTTP_METH_GET) ? "GET" : ((method == HTTP_METH_POST) ? "POST" : "PUT"),
                 path, host, (unsigned)port);
    if ((n < 0) || ((size_t)n >= sizeof(req))) { close(fd); return HTTP_RET_ERR_BUILD; }
    req_len = (size_t)n;
    if ((method == HTTP_METH_POST) || (method == HTTP_METH_PUT)) {
        n = snprintf(req + req_len, sizeof(req) - req_len,
                     "Content-Type: application/json\r\nContent-Length: %u\r\n\r\n%s",
                     (unsigned)body_len, (body != NULL) ? body : "");
        if ((n < 0) || ((size_t)n >= (sizeof(req) - req_len))) { close(fd); return HTTP_RET_ERR_BUILD; }
        req_len = strlen(req);
        req_len += body_len;
    }

    if ((tls_version == HTTP_TLS_TLS12) || (tls_version == HTTP_TLS_TLS13)) {
        ssl = http_tls_connect(fd, tls_version);
        if (ssl == NULL) { close(fd); return HTTP_RET_ERR_TLS; }
        if (http_send_all(fd, ssl, (const uint8_t *)req, req_len) != 0) {
            if (ssl != NULL) { SSL_free(ssl); }
            close(fd);
            return HTTP_RET_ERR_CONN;
        }
    } else {
        if (http_send_all(fd, NULL, (const uint8_t *)req, req_len) != 0) { close(fd); return HTTP_RET_ERR_CONN; }
    }

    if (http_recv(fd, ssl, resp, sizeof(resp), &resp_len) != 0) {
        if (ssl != NULL) { SSL_free(ssl); }
        close(fd);
        return HTTP_RET_ERR_CONN;
    }
    if (ssl != NULL) { SSL_free(ssl); }
    close(fd);

    {
        char *sep = strstr(resp, "\r\n\r\n");
        body_off = (sep != NULL) ? (size_t)(sep - resp) + 4 : 0;
    }
    if (sscanf(resp, "HTTP/1.1 %d", &status) != 1) { status = 0; }
    {
        char *cl = strstr(&resp[body_off], "Content-Length: ");
        if ((cl != NULL) && (cl[15] >= '0') && (cl[15] <= '9')) {
            content_len = (long)strtol(&cl[15], NULL, 10);
        }
    }

    if (content_len > 0) {
        size_t avail = sizeof(resp) - body_off;
        size_t take = ((size_t)content_len < avail) ? (size_t)content_len : avail;
        uint8_t *b = (uint8_t *)malloc(take > 0 ? take : 1);
        if (b == NULL) { return HTTP_RET_ERR; }
        memcpy(b, &resp[body_off], take);
        out->body = b;
        out->body_len = take;
    }
    out->status = status;
    return HTTP_RET_OK;
}

void http_resp_free(http_resp_t *r)
{
    if (r == NULL) { return; }
    free(r->body);
    r->body = NULL;
    r->body_len = 0;
    r->status = 0;
}
