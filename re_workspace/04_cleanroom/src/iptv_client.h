#ifndef CLEANROOM_IPTV_CLIENT_H
#define CLEANROOM_IPTV_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IPT_CLIENT_RET_OK = 0,
    IPT_CLIENT_RET_ERR = -1,
    IPT_CLIENT_RET_ERR_HTTP = -2
} ipt_client_ret_t;

/*
 * Generic IPTV backend activation/run call.  `base` examples:
 *   "http://golden.mmsscode.com/Protocol", action "Activate"
 *   "http://panel.ip-hd.net/app/v2",        action "login"
 * Builds path "<base>/<action>.php" and posts the JSON activation payload.
 */
ipt_client_ret_t iptv_backend_call(const char *base, const char *action, const char *json_body,
                                  char *resp, size_t cap, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_IPTV_CLIENT_H */
