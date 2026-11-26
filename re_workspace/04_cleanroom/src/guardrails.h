#ifndef CLEANROOM_GUARDRAILS_H
#define CLEANROOM_GUARDRAILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Resource & security guardrails (clean-room P1.1):
 *  - configurable maximum concurrent client streams (default 1-2 single
 *    transponder streams, matching recovered tuner limits)
 *  - LAN-only binding / ACL checks for SSDP / RTSP / HTTP / DLNA servers
 */
typedef struct {
    size_t max_concurrent_streams;
    bool enabled;
    size_t in_use;
} cleanroom_guardrails_t;

bool cleanroom_guardrails_init(cleanroom_guardrails_t *g, size_t max_streams, bool enabled);
bool cleanroom_guardrails_acquire(cleanroom_guardrails_t *g, size_t needed);
void  cleanroom_guardrails_release(cleanroom_guardrails_t *g);
bool  cleanroom_guardrails_lan_only_enabled(const cleanroom_guardrails_t *g);
bool  cleanroom_guardrails_lan_bind_ok(const cleanroom_guardrails_t *g, uint32_t addr_be);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_GUARDRAILS_H */
