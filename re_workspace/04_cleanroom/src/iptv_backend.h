/*
 * cleanroom/iptv_backend.h
 * ------------------------
 * Clean-room IPTV backend integration module — P1 subsystem.
 *
 * Recovered endpoints:
 *   http://panel.ip-hd.net/app/v2/{login,playlist,member}.php
 *   http://golden.mmsscode.com/Protocol/{Activate,Run}.php
 *   http://sonres.com/sonic/resevers/{Run,Activate}.php
 *   http://sstv3.ddnb.tn/{act,pack,channels}.php
 *   https://api.ipfox.co
 *   http://gogoday.info/demo/ci_demo/index.php/hiporegister/create_register
 *   http://172.104.99.93/demo/ci_demo/index.php/hipopatch/
 */

#ifndef CLEANROOM_IPTV_BACKEND_H
#define CLEANROOM_IPTV_BACKEND_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IPT_RET_OK = 0,
    IPT_RET_ERR = -1,
    IPT_RET_ERR_NOTFOUND = -2,
    IPT_RET_ERR_HTTP = -3
} ipt_ret_t;

typedef struct {
    const char *panel_base;
    const char *golden_base;
    const char *sonic_base;
    const char *sstv_base;
    const char *api_base;
    const char *register_base;
    const char *demo_base;
} iptv_backend_t;

ipt_ret_t iptv_backend_init(iptv_backend_t *b);
ipt_ret_t iptv_backend_activate(const char *backend, const char *ident,
                              char *json_out, size_t json_cap, size_t *json_len);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_IPTV_BACKEND_H */
