/*
 * cleanroom/iptv_backend.c
 * ------------------------
 * Clean-room IPTV backend integration implementation.
 */

#include "iptv_backend.h"
#include <string.h>
#include <stddef.h>

ipt_ret_t iptv_backend_init(iptv_backend_t *b)
{
    if (b == NULL) { return IPT_RET_ERR; }
    memset(b, 0, sizeof(*b));
    b->panel_base = "http://panel.ip-hd.net";
    b->golden_base = "http://golden.mmsscode.com";
    b->sonic_base = "http://sonres.com";
    b->sstv_base = "http://sstv3.ddnb.tn";
    b->api_base = "https://api.ipfox.co";
    b->register_base = "http://gogoday.info";
    b->demo_base = "http://172.104.99.93";
    return IPT_RET_OK;
}

ipt_ret_t iptv_backend_activate(const char *backend, const char *ident,
                               char *json_out, size_t json_cap, size_t *json_len)
{
    if ((backend == NULL) || (json_out == NULL) || (json_len == NULL)) { return IPT_RET_ERR; }
    (void)ident;
    /* Recovered JSON shapes:
         {'customer':'%s','model':'%s','chipsn':'%s','mac':'%02x…','version':'%s','time':'%d'}
         {'username':'%s','card':'%s','time':'%d'}
    */
    *json_len = 0;
    if (json_cap > 0) { json_out[0] = '\0'; }
    /* real implementation would POST/GET the backend activate/run endpoint */
    return IPT_RET_ERR;
}

