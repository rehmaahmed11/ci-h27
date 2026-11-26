#ifndef CLEANROOM_HTTP_CLIENT_H
#define CLEANROOM_HTTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TLS versions supported by the clean-room HTTP client. */
#define HTTP_TLS_CLEAR 0
#define HTTP_TLS_TLS12 1
#define HTTP_TLS_TLS13 2

typedef struct {
    int      status; /* HTTP status code, 0 = none yet */
    uint8_t *body;
    size_t   body_len;
    int      tls_version;
} http_resp_t;

typedef struct {
    int      fd;
    int      tls_version;
} http_client_t;

typedef enum {
    HTTP_METH_GET = 0,
    HTTP_METH_POST = 1,
    HTTP_METH_PUT = 2,
    HTTP_METH_HEAD = 3
} http_method_t;

typedef enum {
    HTTP_RET_OK = 0,
    HTTP_RET_ERR = -1,
    HTTP_RET_ERR_TLS = -2,
    HTTP_RET_ERR_CONN = -3,
    HTTP_RET_ERR_BUILD = -4
} http_ret_t;

http_ret_t http_client_init(http_client_t *c, int tls_version);
http_ret_t http_request(const char *host, uint16_t port, http_method_t method, const char *path,
                       const char *body, size_t body_len, http_resp_t *out);
void      http_resp_free(http_resp_t *r);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_HTTP_CLIENT_H */
