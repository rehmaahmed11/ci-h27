/*
 * cleanroom/dvb_ts_mux.c
 * ---------------------
 * Clean-room DVB transport-stream mux ring buffer (P1.1 addition).
 *
 * This module is the integration point added between the recovered P0 CAM /
 * descrambler output stage and the outbound LAN streamers (SAT>IP / RTSP / DLNA
 * front ends).  The recovered P0 wire protocols (HEAD+CRC32 flash frames, CAM
 * IKS frames, AES-CTR descrambling) are NOT modified here: the mux only consumes
 * fixed 188-byte TS packets produced by the descrambler stage and buffers them
 * for downstream RTP/TS streamers.
 */

#include "dvb_ts_mux.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

int dvb_ts_mux_init(dvb_ts_mux_t *m, dvb_ts_mux_sink_t sink, void *ctx)
{
    if (m == NULL) { return -1; }
    m->head = 0u;
    m->tail = 0u;
    m->count = 0u;
    m->sink = sink;
    m->ctx = ctx;
    return 0;
}

int dvb_ts_mux_set_sink(dvb_ts_mux_t *m, dvb_ts_mux_sink_t sink, void *ctx)
{
    if (m == NULL) { return -1; }
    m->sink = sink;
    m->ctx = ctx;
    return 0;
}

int dvb_ts_mux_push(dvb_ts_mux_t *m, const uint8_t *ts, size_t len)
{
    if ((m == NULL) || ((ts == NULL) && (len != 0u))) { return -1; }
    if (len != (size_t)DVB_TS_MUX_TS_PACKET_SIZE) { return -2; }
    if (m->count >= (size_t)DVB_TS_MUX_RING_SIZE) { return -3; }
    memcpy(m->ring[m->tail].data, ts, (size_t)DVB_TS_MUX_TS_PACKET_SIZE);
    m->tail = (m->tail + 1u) % (size_t)DVB_TS_MUX_RING_SIZE;
    m->count += 1u;
    return 0;
}

int dvb_ts_mux_pop(dvb_ts_mux_t *m, uint8_t *out, size_t out_cap, size_t *out_len)
{
    if ((m == NULL) || (out == NULL)) { return -1; }
    if (out_cap < (size_t)DVB_TS_MUX_TS_PACKET_SIZE) { return -2; }
    if (m->count == 0u) { return -3; }
    memcpy(out, m->ring[m->head].data, (size_t)DVB_TS_MUX_TS_PACKET_SIZE);
    m->head = (m->head + 1u) % (size_t)DVB_TS_MUX_RING_SIZE;
    m->count -= 1u;
    if (out_len != NULL) { *out_len = (size_t)DVB_TS_MUX_TS_PACKET_SIZE; }
    return 0;
}

size_t dvb_ts_mux_count(const dvb_ts_mux_t *m)
{
    if (m == NULL) { return 0u; }
    return m->count;
}

void dvb_ts_mux_flush(dvb_ts_mux_t *m)
{
    if (m == NULL) { return; }
    m->head = 0u;
    m->tail = 0u;
    m->count = 0u;
}
