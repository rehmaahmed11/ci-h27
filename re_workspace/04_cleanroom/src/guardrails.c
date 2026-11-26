/*
 * cleanroom/guardrails.c
 * ----------------------
 * Clean-room resource & security guardrails: max concurrent stream enforcement
 * and LAN-only bind check for the clean-room front-end servers.
 */

#include "guardrails.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool cleanroom_guardrails_init(cleanroom_guardrails_t *g, size_t max_streams, bool enabled)
{
    if (g == NULL) { return false; }
    g->max_concurrent_streams = max_streams;
    g->enabled = enabled;
    g->in_use = 0u;
    return true;
}

bool cleanroom_guardrails_acquire(cleanroom_guardrails_t *g, size_t needed)
{
    if ((g == NULL) || (g->enabled == false)) { return true; }
    if ((needed == 0u) || (g->in_use + needed <= g->max_concurrent_streams)) {
        g->in_use += needed;
        return true;
    }
    return false;
}

void cleanroom_guardrails_release(cleanroom_guardrails_t *g)
{
    if (g == NULL) { return; }
    if (g->in_use > 0u) { g->in_use -= 1u; }
}

bool cleanroom_guardrails_lan_only_enabled(const cleanroom_guardrails_t *g)
{
    if (g == NULL) { return false; }
    return g->enabled;
}

bool cleanroom_guardrails_lan_bind_ok(const cleanroom_guardrails_t *g, uint32_t addr_be)
{
    unsigned a = (unsigned)((addr_be >> 24) & 0xFFu);
    unsigned b = (unsigned)((addr_be >> 16) & 0xFFu);
    unsigned c = (unsigned)((addr_be >> 8) & 0xFFu);
    unsigned d = (unsigned)(addr_be & 0xFFu);

    if (g == NULL) { return false; }
    /* RFC1918 LAN ranges: 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16. */
    (void)c; (void)d;   /* kept for future /24 ACL checks */
    if (a == 10u) { return true; }
    if ((a == 192u) && (b == 168u)) { return true; }
    if ((a == 172u) && (b >= 16u) && (b <= 31u)) { return true; }
    return false;
}
