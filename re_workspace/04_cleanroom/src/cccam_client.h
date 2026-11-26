/*
 * cleanroom/cccam_client.h
 * ------------------------
 * Clean-room CCcam client (2.2.x) module — P1 subsystem.
 *
 * Recovered behaviour:
 *   - "cccam: keepalive"
 *   - "cccam 2.2.x commands not implemented: 0x%02X"
 *   - "cccam:%s cannot allocate memory!!"
 *   - server_cccam_manager_unkown_act_proc
 */

#ifndef CLEANROOM_CCAM_CLIENT_H
#define CLEANROOM_CCAM_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CCAM_VERSION_STR "cccam 2.2.x"
#define CCAM_KEEPALIVE_STR "cccam: keepalive"

typedef enum {
    CCAM_RET_OK = 0,
    CCAM_RET_ERR = -1,
    CCAM_RET_ERR_ALLOC = -2,
    CCAM_RET_ERR_CMD = -3
} ccam_ret_t;

typedef struct {
    int keepalive_enabled;
    int not_impl_report_enabled;
} ccam_client_t;

ccam_ret_t ccam_client_init(ccam_client_t *c);
const char *ccam_client_version(void);
int ccam_client_keepalive(ccam_client_t *c);
bool ccam_client_is_known(uint8_t cmd, bool *known);
ccam_ret_t ccam_client_dispatch(ccam_client_t *c, uint8_t cmd,
                               const uint8_t *req, size_t req_len,
                               uint8_t *resp, size_t resp_max, size_t *resp_len);
const char *ccam_client_not_impl_msg(uint8_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_CCAM_CLIENT_H */
