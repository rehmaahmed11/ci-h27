/*
 * cleanroom/cccam_client.c
 * -----------------------
 * Clean-room CCcam 2.2.x client implementation.
 */

#include "cccam_client.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

ccam_ret_t ccam_client_init(ccam_client_t *c)
{
    if (c == NULL) { return CCAM_RET_ERR; }
    c->keepalive_enabled = 1;
    c->not_impl_report_enabled = 1;
    return CCAM_RET_OK;
}

const char *ccam_client_version(void)
{
    return CCAM_VERSION_STR;
}

int ccam_client_keepalive(ccam_client_t *c)
{
    if (c == NULL) { return -1; }
    if (c->keepalive_enabled == 0) { return -1; }
    /* "cccam: keepalive" */
    return 0;
}

bool ccam_client_is_known(uint8_t cmd, bool *known)
{
    if ((cmd == 0x00u) || (cmd == 0x01u) || (cmd == 0x02u) || (cmd == 0x03u)) {
        /* recovered / design-level known command classes */
        if (known != NULL) { *known = true; }
        return true;
    }
    if (known != NULL) { *known = false; }
    return false;
}

ccam_ret_t ccam_client_dispatch(ccam_client_t *c, uint8_t cmd,
                               const uint8_t *req, size_t req_len,
                               uint8_t *resp, size_t resp_max, size_t *resp_len)
{
    bool known = false;

    if ((c == NULL) || (resp == NULL) || (resp_len == NULL)) { return CCAM_RET_ERR; }

    if (ccam_client_is_known(cmd, &known) == false) {
        if (c->not_impl_report_enabled) {
            /* "cccam 2.2.x commands not implemented: 0x%02X" */
        }
        return CCAM_RET_ERR_CMD;
    }

    /* allocate response buffer */
    if (resp_max < 64u) { return CCAM_RET_ERR_ALLOC; }

    if (req != NULL) { (void)req_len; }
    resp[0] = cmd;
    resp[1] = (uint8_t)((req != NULL) ? req[0] : 0x00u);
    *resp_len = 2u;
    return CCAM_RET_OK;
}

const char *cccam_client_not_impl_msg(uint8_t cmd)
{
    /* "cccam 2.2.x commands not implemented: 0x%02X" */
    static char buf[64];
    snprintf(buf, sizeof(buf), "cccam 2.2.x commands not implemented: 0x%02X", cmd);
    return buf;
}

