/*
 * cleanroom/satip_server.c
 * ------------------------
 * Clean-room SAT>IP (EN 50585) server helpers: SSDP + /desc.xml + SDP builder
 * for the single-transponder RTP stream handed to SAT>IP clients.
 *
 * Pure string/XML building; no vendor code involved.  Open libraries only.
 */

#include "satip_server.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

satip_ret_t satip_server_init(void)
{
    return SATIP_RET_OK;
}

static int satip_append(char *out, size_t cap, size_t *len, const char *fmt, ...)
{
    va_list ap;
    int n;

    if ((out == NULL) || (cap == 0u) || (len == NULL)) { return -1; }
    if (*len >= cap) { return -2; }
    va_start(ap, fmt);
    n = vsnprintf(out + *len, (cap - *len), fmt, ap);
    va_end(ap);
    if ((n < 0) || ((size_t)n >= (cap - *len))) { return -2; }
    *len += (size_t)n;
    out[*len] = '\0';
    return 0;
}

static void ip_be_to_str(uint32_t ip_be, char *out, size_t cap)
{
    unsigned a = (unsigned)((ip_be >> 24) & 0xFFu);
    unsigned b = (unsigned)((ip_be >> 16) & 0xFFu);
    unsigned c = (unsigned)((ip_be >> 8) & 0xFFu);
    unsigned d = (unsigned)(ip_be & 0xFFu);

    snprintf(out, cap, "%u.%u.%u.%u", a, b, c, d);
}

satip_ret_t satip_server_build_desc(const satip_device_t *d, char *out, size_t cap)
{
    size_t len = 0u;
    int i;

    if ((d == NULL) || (out == NULL)) { return SATIP_RET_ERR; }
    out[0] = '\0';
    len = 0u;

    if (satip_append(out, cap, &len, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<root xmlns=\"urn:ses-com:satip:1.0\">") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<device>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<deviceType>urn:ses-com:satip:1.0:device:model:%s</deviceType>", d->model) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<friendlyName>%s</friendlyName>", d->model) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<manufacturer>TurnRound</manufacturer>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<modelName>%s</modelName>", d->model) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<vendorId>%u</vendorId>", (unsigned)d->vendor_id) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<productId>%u</productId>", (unsigned)d->product_id) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<purchaseDate>2026-01-01T00:00:00Z</purchaseDate>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<firstUseDate>2026-01-01T00:00:00Z</firstUseDate>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<lastUseDate>2026-01-01T00:00:00Z</lastUseDate>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<maxSdrStreams>1</maxSdrStreams>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<maxLnbCascade>2</maxLnbCascade>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<tuning>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<freq>%u</freq>", (unsigned)d->freq) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<pol>%u</pol>", (unsigned)d->pol) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<msys>%u</msys>", (unsigned)d->msys) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<sr>%u</sr>", (unsigned)d->sr) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "<pids>") != 0) { return SATIP_RET_ERR_BUILD; }
    for (i = 0; i < d->pid_count; ++i) {
        if (satip_append(out, cap, &len, "<pid>%u</pid>", (unsigned)d->pids[i]) != 0) { return SATIP_RET_ERR_BUILD; }
    }
    if (satip_append(out, cap, &len, "</pids></tuning>") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "</device></root>") != 0) { return SATIP_RET_ERR_BUILD; }

    return SATIP_RET_OK;
}

satip_ret_t satip_server_build_sdp(const satip_device_t *d, uint16_t server_rtp_port,
                                  uint32_t mcast_addr_be, char *out, size_t cap)
{
    char ip[16];
    char mcast[16];
    size_t len = 0u;

    if ((d == NULL) || (out == NULL)) { return SATIP_RET_ERR; }
    out[0] = '\0';
    len = 0u;

    ip_be_to_str(d->ip_be, ip, sizeof(ip));
    ip_be_to_str(mcast_addr_be, mcast, sizeof(mcast));

    if (satip_append(out, cap, &len, "v=0\r\n") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "o=- 0 0 IN IP4 %s %u\r\n", ip, (unsigned)server_rtp_port) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "s=%s\r\n", d->model) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "c=IN IP4 %s %u\r\n", mcast, (unsigned)server_rtp_port) != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "t=0 0\r\n") != 0) { return SATIP_RET_ERR_BUILD; }
    if (satip_append(out, cap, &len, "m=application/satip\r\n") != 0) { return SATIP_RET_ERR_BUILD; }

    return SATIP_RET_OK;
}
