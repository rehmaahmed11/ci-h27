#ifndef CLEANROOM_DVB_TS_MUX_H
#define CLEANROOM_DVB_TS_MUX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DVB transport stream packet size (188 bytes + 4-byte header handled by caller). */
#define DVB_TS_MUX_TS_PACKET_SIZE 188u

/* Ring-buffer capacity in TS packets (power-of-two friendly slot count). */
#define DVB_TS_MUX_RING_SIZE 64u

typedef struct {
    uint8_t data[DVB_TS_MUX_TS_PACKET_SIZE];
} dvb_ts_mux_packet_t;

typedef void (*dvb_ts_mux_sink_t)(const uint8_t *ts, size_t len, void *ctx);

typedef struct {
    dvb_ts_mux_packet_t ring[DVB_TS_MUX_RING_SIZE];
    size_t head;
    size_t tail;
    size_t count;
    dvb_ts_mux_sink_t sink;
    void *ctx;
} dvb_ts_mux_t;

int    dvb_ts_mux_init(dvb_ts_mux_t *m, dvb_ts_mux_sink_t sink, void *ctx);
int    dvb_ts_mux_set_sink(dvb_ts_mux_t *m, dvb_ts_mux_sink_t sink, void *ctx);
int    dvb_ts_mux_push(dvb_ts_mux_t *m, const uint8_t *ts, size_t len);
int    dvb_ts_mux_pop(dvb_ts_mux_t *m, uint8_t *out, size_t out_cap, size_t *out_len);
size_t dvb_ts_mux_count(const dvb_ts_mux_t *m);
void   dvb_ts_mux_flush(dvb_ts_mux_t *m);

#ifdef __cplusplus
}
#endif

#endif /* CLEANROOM_DVB_TS_MUX_H */
