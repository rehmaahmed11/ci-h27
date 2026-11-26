/*
 * cleanroom/main.c
 * -----------------
 * Minimal self-test entry point for the clean-room subsystem set:
 * common, fw_update, fw_nvm, cam_keys, cam_server, crypto_tls, cccam_client,
 * net_transport, yt_proxy, iptv_backend, dvb_ts_mux, satip_server, rtsp_server,
 * dlna_service, guardrails.
 *
 * Build: see Makefile / README.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "fw_update.h"
#include "fw_nvm.h"
#include "cam_keys.h"
#include "cam_server.h"
#include "crypto_tls.h"
#include "cccam_client.h"
#include "net_transport.h"
#include "yt_proxy.h"
#include "iptv_backend.h"
#include "dvb_ts_mux.h"
#include "satip_server.h"
#include "rtsp_server.h"
#include "dlna_service.h"
#include "guardrails.h"
#include "cam_frame.h"
#include "http_client.h"
#include "yt_api.h"
#include "iptv_client.h"
#include "orchestrator.h"
#include "token_ledger.h"
#include "net_socket.h"
#include "rtsp_server_full.h"
#include "satip_server_full.h"
#include "dlna_server.h"
#include "crypto_ctr.h"
#include "runtime.h"
#include "rtsp_server_loop.h"
#include "ssdp_server_loop.h"
#include "crypto_key_schedule.h"
#include "runtime_e2e.h"
#include "menu.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_failures++; \
    } \
} while (0)

/* CRC32 test vector: CRC32("123456789") == 0xCBF43926 */
static void test_crc32(void)
{
    const uint8_t tv[] = "123456789";
    uint32_t crc = cleanroom_crc32(tv, strlen((const char *)tv));
    CHECK(crc == 0xCBF43926u);
}

/* Flash frame round-trip: encode then decode must be lossless. */
static void test_fw_update_frame(void)
{
    uint8_t buf[512];
    size_t out_len = 0;
    size_t payload_actual = 0;
    uint8_t type = FW_UPDATE_CMD_BURN;
    uint8_t flag = 0x01;
    uint32_t index = 0x11223344u;
    uint8_t subtype = 0x07;
    const uint8_t payload[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04 };
    size_t payload_len = sizeof(payload);
    uint8_t d_type = 0, d_flag = 0;
    uint32_t d_index = 0;
    uint8_t d_subtype = 0;
    uint8_t d_payload[64];
    size_t d_payload_actual = 0;

    fw_update_ret_t r = fw_update_frame_encode(buf, sizeof(buf), &out_len,
                                               type, flag, index, subtype, payload, payload_len);
    CHECK(r == FW_UPDATE_RET_OK);
    CHECK(out_len == (FW_UPDATE_HDR_SIZE + payload_len + FW_UPDATE_TRAILER_SIZE));

    r = fw_update_frame_decode(buf, out_len, &payload_actual,
                                &d_type, &d_flag, &d_index, &d_subtype,
                                d_payload, sizeof(d_payload), &d_payload_actual);
    CHECK(r == FW_UPDATE_RET_OK);
    CHECK(d_type == type);
    CHECK(d_flag == flag);
    CHECK(d_index == index);
    CHECK(d_subtype == subtype);
    CHECK(payload_actual == payload_len);
    CHECK(d_payload_actual == payload_len);
    CHECK(memcmp(d_payload, payload, payload_len) == 0);
}

/* NVM store: write / read round trip and erase-flag test. */
static void test_fw_nvm(void)
{
    fw_nvm_partition_t p;
    uint8_t tmp[16];
    fw_nvm_ret_t r;

    r = fw_nvm_alloc(&p, sizeof(tmp));
    CHECK(r == FW_NVM_RET_OK);

    memset(tmp, 0x00, sizeof(tmp));
    CHECK(fw_nvm_write(&p, 0, tmp, sizeof(tmp)) == FW_NVM_RET_OK);
    CHECK(fw_nvm_read(&p, 0, tmp, sizeof(tmp)) == FW_NVM_RET_OK);
    {
        const uint8_t expect[16];
        memset((void *)expect, 0x00, sizeof(expect));
        CHECK(memcmp(tmp, expect, sizeof(expect)) == 0);
    }

    CHECK(fw_nvm_is_erased(p.data, 16) == false);
    fw_nvm_erase(&p, 0, sizeof(tmp));
    CHECK(fw_nvm_is_erased(p.data, sizeof(tmp)) == true);

    free(p.data);
}

/* Key store: add / get / HDCP validity. */
static void test_cam_keys(void)
{
    uint8_t key[16];
    size_t i;

    CHECK(cam_key_store_init() == CAM_KEY_RET_OK);
    for (i = 0; i < 16; ++i) { key[i] = (uint8_t)(0x10 + i); }
    CHECK(cam_key_store_add("bxcam", CAM_KEY_AES_128_CBC, key, sizeof(key)) == CAM_KEY_RET_OK);
    CHECK(cam_key_store_get("bxcam", CAM_KEY_AES_128_CBC, key, sizeof(key)) == true);
    CHECK(cam_key_store_get("bxcam", CAM_KEY_AES_256_CBC, key, sizeof(key)) == false);

    CHECK(cam_key_hdcp_valid((const uint8_t *)"abc", 3) == false);
    CHECK(cam_key_hdcp_valid((const uint8_t *)"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10", 16) == true);
}

/* CAM server: IDLE -> ... transitions. */
static void test_cam_server(void)
{
    cam_server_t s;
    cam_server_event_out_t out;
    cam_server_ret_t r;

    cam_server_init(&s);
    CHECK(s.state == CAM_CONN_IDLE);

    r = cam_server_handle(&s, CAM_EV_CONNECTED, &out);
    CHECK(r == CAM_SERVER_RET_OK);
    CHECK(out.ok == true);
    CHECK(s.state == CAM_CONN_CONNECT_GATE_SERVER);

    r = cam_server_handle(&s, CAM_EV_DISCONNECTED, &out);
    CHECK(r == CAM_SERVER_RET_OK);
    CHECK(s.state == CAM_CONN_IDLE);
}

/* Crypto / TLS: AES key lengths + ARC4. */
static void test_crypto_tls(void)
{
    uint8_t key[16];
    uint8_t in[8];
    uint8_t out[8];
    size_t i;

    CHECK(tls_crypto_key_len(TLS_CRYPTO_AES_128_CBC) == 16);
    CHECK(tls_crypto_key_len(TLS_CRYPTO_AES_192_CBC) == 24);
    CHECK(tls_crypto_key_len(TLS_CRYPTO_AES_256_CBC) == 32);
    CHECK(tls_crypto_key_len(TLS_CRYPTO_AES_128_CTR) == 16);
    for (i = 0; i < 16; ++i) { key[i] = (uint8_t)(0x20 + i); }
    memset(in, 0x00, sizeof(in));
    CHECK(tls_crypto_aes_cbc_decrypt(TLS_CRYPTO_AES_128_CBC, key, key, in, sizeof(in), out, sizeof(out)) != 0);
    /* AES-CTR is a stream mode: an 8-byte buffer is a valid operation.  (It used
     * to "fail" only because EVP_DecryptUpdate() was called with a NULL out-param.) */
    CHECK(tls_crypto_aes_ctr(TLS_CRYPTO_AES_128_CTR, key, key, in, sizeof(in), out, sizeof(out)) == 0);
    CHECK(tls_crypto_arc4(key, sizeof(key), in, sizeof(in), out, sizeof(out)) == 0);
}

/* CCcam client: version / keepalive / dispatch. */
static void test_ccam_client(void)
{
    ccam_client_t c;
    uint8_t resp[64];
    size_t resp_len = 0;

    CHECK(ccam_client_init(&c) == CCAM_RET_OK);
    CHECK(strcmp(ccam_client_version(), "cccam 2.2.x") == 0);
    CHECK(ccam_client_keepalive(&c) == 0);

    {
        bool known = false;
        CHECK(ccam_client_is_known(0x00u, &known) == true);
        CHECK(known == true);
        CHECK(ccam_client_dispatch(&c, 0x99u, NULL, 0, resp, sizeof(resp), &resp_len) == CCAM_RET_ERR_CMD);
    }
}

/* Network transport: open / unsupported-scheme handling. */
static void test_net_transport(void)
{
    net_transport_t t;
    uint8_t *buf = NULL;
    size_t len = 0;

    CHECK(net_transport_open(&t) == NET_RET_OK);
    CHECK(t.proto == NET_PROTO_HTTP);
    CHECK(net_http_download("https://panel.ip-hd.net/app/v2/login.php", &buf, &len) == NET_RET_ERR_NOTSUPP);
}

/* YouTube proxy: init / config / tokens. */
static void test_yt_proxy(void)
{
    yt_proxy_t y;
    char *js = NULL;
    char *cat = NULL;

    CHECK(yt_proxy_init(&y, "AIzaSyA-test-key") == YT_RET_OK);
    CHECK(y.api_key[0] == 'A');
    CHECK(yt_player_config("CFR", &js, &cat) == YT_RET_ERR);
    CHECK(yt_tokens_parse("{}", &js) == YT_RET_ERR);
}

/* IPTV backend: init / activate. */
static void test_iptv_backend(void)
{
    iptv_backend_t b;
    char json[256];
    size_t json_len = 0;

    CHECK(iptv_backend_init(&b) == IPT_RET_OK);
    CHECK(b.panel_base != NULL);
    CHECK(b.golden_base != NULL);
    CHECK(b.sonic_base != NULL);
    CHECK(b.sstv_base != NULL);
    CHECK(b.api_base != NULL);
    CHECK(b.register_base != NULL);
    CHECK(b.demo_base != NULL);
    CHECK(iptv_backend_activate("panel", "00:11:22:33:44:55", json, sizeof(json), &json_len) == IPT_RET_ERR);
}

/* DVB TS mux ring buffer: push / pop / count. */
static void test_dvb_ts_mux(void)
{
    dvb_ts_mux_t m;
    uint8_t pkt[188];
    uint8_t out[188];
    size_t out_len = 0;

    CHECK(dvb_ts_mux_init(&m, NULL, NULL) == 0);
    memset(pkt, 0x47, sizeof(pkt));
    CHECK(dvb_ts_mux_push(&m, pkt, sizeof(pkt)) == 0);
    CHECK(dvb_ts_mux_push(&m, pkt, sizeof(pkt)) == 0);
    CHECK(dvb_ts_mux_count(&m) == 2u);
    CHECK(dvb_ts_mux_pop(&m, out, sizeof(out), &out_len) == 0);
    CHECK(out_len == 188u);
    CHECK(dvb_ts_mux_pop(&m, out, sizeof(out), &out_len) == 0);
    CHECK(dvb_ts_mux_pop(&m, out, sizeof(out), &out_len) == -3);
}

/* SAT>IP server: desc.xml + SDP builder. */
static void test_satip_server(void)
{
    satip_device_t d;
    char sdp[512];
    char desc[1024];

    CHECK(satip_server_init() == SATIP_RET_OK);
    memset(&d, 0, sizeof(d));
    snprintf(d.base_url, sizeof(d.base_url), "http://192.168.100.50:8080");
    snprintf(d.model, sizeof(d.model), "tdt-700");
    d.ip_be = 0xC0A86432u; /* 192.168.100.50 */
    d.freq = 12000u;
    d.pid_count = 1;
    d.pids[0] = 0x1FFFu;
    CHECK(satip_server_build_desc(&d, desc, sizeof(desc)) == SATIP_RET_OK);
    CHECK(strstr(desc, "urn:ses-com:satip:1.0") != NULL);
    CHECK(satip_server_build_sdp(&d, 5000, 0xE1123456u, sdp, sizeof(sdp)) == SATIP_RET_OK);
    CHECK(strstr(sdp, "m=application/satip") != NULL);
}

/* RTSP server: OPTIONS -> DESCRIBE -> SETUP -> PLAY state machine. */
static void test_rtsp_server(void)
{
    rtsp_server_t s;
    rtsp_conn_t c;
    char resp[1024];
    size_t resp_len = 0;

    CHECK(rtsp_server_init(&s) == RTSP_RET_OK);
    memset(&c, 0, sizeof(c));
    CHECK(rtsp_conn_handle(&c, "OPTIONS", "rtsp://192.168.100.50:554/ch/1", NULL, 0, resp, sizeof(resp), &resp_len) == RTSP_RET_OK);
    CHECK(strstr(resp, "200 OK") != NULL);
    {
        const char *sdp_body = "v=0\r\no=- 0 0 IN IP4 1.2.3.4 5000\r\n";
        size_t sdp_len = strlen(sdp_body);
        CHECK(rtsp_conn_handle(&c, "DESCRIBE", "rtsp://192.168.100.50:554/ch/1", sdp_body, sdp_len, resp, sizeof(resp), &resp_len) == RTSP_RET_OK);
    }
    CHECK(strstr(resp, "200 OK") != NULL);
    CHECK(c.state == RTSP_CONN_SETUP);
    CHECK(rtsp_conn_handle(&c, "SETUP", "rtsp://192.168.100.50:554/ch/1", NULL, 0, resp, sizeof(resp), &resp_len) == RTSP_RET_OK);
    CHECK(c.state == RTSP_CONN_PLAY);
}

/* DLNA service: SSDP discovery + SOAP AVT/RC/CDD responses. */
static void test_dlna_service(void)
{
    dlna_service_t d;
    uint8_t respbuf[2048];
    size_t resp_len = 0;

    CHECK(dlna_service_init(&d) == DLNA_RET_OK);
    CHECK(dlna_service_handle_discovery(NULL, respbuf, sizeof(respbuf), &resp_len) == DLNA_RET_OK);
    CHECK(strstr((char *)respbuf, "200 OK") != NULL);
    CHECK(dlna_service_avt_action("GetCurrentTransportState", NULL, (char *)respbuf, sizeof(respbuf), &resp_len) == DLNA_RET_OK);
    CHECK(strstr((char *)respbuf, "TRANSIENT") != NULL);
    CHECK(dlna_service_rc_action("GetState", NULL, (char *)respbuf, sizeof(respbuf), &resp_len) == DLNA_RET_OK);
    CHECK(dlna_service_cdd_action("GetSearchResults", "0", (char *)respbuf, sizeof(respbuf), &resp_len) == DLNA_RET_OK);
}

/* Resource & security guardrails: max concurrent streams + LAN-only check. */
static void test_guardrails(void)
{
    cleanroom_guardrails_t g;

    CHECK(cleanroom_guardrails_init(&g, 2, true) == true);
    CHECK(cleanroom_guardrails_lan_only_enabled(&g) == true);
    CHECK(cleanroom_guardrails_acquire(&g, 2) == true);
    CHECK(cleanroom_guardrails_acquire(&g, 1) == false);
    cleanroom_guardrails_release(&g);
    CHECK(cleanroom_guardrails_acquire(&g, 1) == true);
}

/* CAM frame: build -> encode -> decode round trip. */
static void test_cam_frame(void)
{
    cam_frame_t f;
    cam_frame_t f2;
    uint8_t buf[64];
    size_t len = 0;

    CHECK(cam_frame_build(&f, 0x01, 0x00, 0x1234, 0x0002, 0x11223344u, 0x55667788u,
                           (const uint8_t *)"\x01\x02\x03\x04", 0x03, 0x07, 0x00) == CAM_FRAME_RET_OK);
    CHECK(cam_frame_encode(&f, buf, sizeof(buf), &len) == CAM_FRAME_RET_OK);
    CHECK(len == CAM_FRAME_HDR_SIZE);
    CHECK(cam_frame_decode(buf, len, &f2) == CAM_FRAME_RET_OK);
    CHECK(f2.cmd == f.cmd);
    CHECK(f2.flags == f.flags);
    CHECK(f2.sid == f.sid);
    CHECK(f2.sub_id == f.sub_id);
    CHECK(f2.token == f.token);
    CHECK(f2.auth_type == f.auth_type);
    CHECK(f2.key_index == f.key_index);
    CHECK(f2.channel_id == f.channel_id);
    CHECK(f2.status == f.status);
}

/* HTTP client: init / response-free init paths. */
static void test_http_client(void)
{
    http_client_t c;
    http_resp_t r;

    CHECK(http_client_init(&c, HTTP_TLS_TLS13) == HTTP_RET_OK);
    CHECK(c.tls_version == HTTP_TLS_TLS13);
    CHECK(http_client_init(NULL, HTTP_TLS_CLEAR) == HTTP_RET_ERR);
    /* Hermetic: an unparseable host fails in inet_pton(), before any connect(),
     * so the self-test needs no network and no listener. */
    CHECK(http_request("256.256.256.256", 80, HTTP_METH_GET, "/", NULL, 0, &r) == HTTP_RET_ERR_CONN);
    CHECK(r.status == 0);
}

/* YouTube API: init / key validation. */
static void test_yt_api(void)
{
    yt_api_t y;

    CHECK(yt_api_init(&y, NULL) == YT_RET_ERR);
    CHECK(yt_api_init(&y, "AIzaSyA-test-key") == YT_RET_OK);
    CHECK(y.api_key[0] == 'A');
    CHECK(yt_api_get_player_config(&y, "dQw4w9WgXcQ", NULL, 0, NULL) == YT_RET_ERR);
}

/* IPTV backend client: argument validation. */
static void test_iptv_client(void)
{
    char resp[256];
    size_t len = 0;

    CHECK(iptv_backend_call(NULL, "Activate", "{}", resp, sizeof(resp), &len) == IPT_CLIENT_RET_ERR);
    CHECK(iptv_backend_call("http://golden.mmsscode.com/Protocol", "Activate", "{}", resp, sizeof(resp), &len) == IPT_CLIENT_RET_ERR);
}

/* Orchestrator: task scheduler state machine. */
static void test_orchestrator(void)
{
    orch_task_t t;

    t.id = ORCH_TASK_CAM;
    t.state = (int)ORCH_TASK_STATE_IDLE;
    CHECK(orch_scheduler_init() == true);
    CHECK(orch_task_start(&t, ORCH_TASK_CAM) == true);
    CHECK(orch_task_state_get(&t) == ORCH_TASK_STATE_RUNNING);
    CHECK(orch_task_stop(&t, ORCH_TASK_CAM) == true);
    CHECK(orch_task_state_get(&t) == ORCH_TASK_STATE_DONE);
    CHECK(orch_task_start(&t, ORCH_TASK_CCAM) == false);
}

/* Token ledger: debit / credit / insufficient checks. */
static void test_token_ledger(void)
{
    token_ledger_t l;

    CHECK(token_ledger_init(&l) == TOKEN_LEDGER_OK);
    CHECK(token_ledger_insufficient(&l, 1) == true);
    CHECK(token_ledger_credit(&l, 10) == TOKEN_LEDGER_OK);
    CHECK(token_ledger_insufficient(&l, 10) == false);
    CHECK(token_ledger_debit(&l, 4) == TOKEN_LEDGER_OK);
    CHECK(token_ledger_insufficient(&l, 7) == true);
    CHECK(token_ledger_debit(&l, 7) == TOKEN_LEDGER_ERR_INSUFFICIENT);
    CHECK(token_ledger_debit(&l, 6) == TOKEN_LEDGER_OK);
    CHECK(token_ledger_insufficient(&l, 1) == true);
}

/* Net socket: init + bind/listen smoke test on ephemeral port. */
static void test_net_socket(void)
{
    int fd = -1;

    CHECK(net_socket_init() == 0);
    CHECK(net_socket_tcp_listen(0, &fd) == 0);
    CHECK(fd > 0);
    net_socket_close(fd);
}

/* RTSP server full: init + OPTIONS processing through state machine. */
static void test_rtsp_server_full(void)
{
    rtsp_server_full_t r;
    rtsp_conn_t c;
    char resp[1024];
    size_t resp_len = 0;

    CHECK(rtsp_server_full_init(&r, 554, 0) == 0);
    memset(&c, 0, sizeof(c));
    CHECK(rtsp_server_full_process(&c, "OPTIONS", "rtsp://192.168.100.5:554/ch/1", NULL, 0, resp, sizeof(resp), &resp_len) == 0);
    CHECK(strstr(resp, "200 OK") != NULL);
}

/* SAT>IP server full: desc/SDP + M-SEARCH response. */
static void test_satip_server_full(void)
{
    satip_server_full_t s;
    char desc[1024];
    char sdp[512];
    char resp[1024];
    size_t resp_len = 0;

    CHECK(satip_server_full_init(&s, NULL) == -1);
    {
        satip_device_t dev;
        memset(&dev, 0, sizeof(dev));
        CHECK(satip_server_full_init(&s, &dev) == 0);
        CHECK(satip_server_full_build_desc(&s, desc, sizeof(desc)) == 0);
        CHECK(strstr(desc, "urn:ses-com:satip:1.0") != NULL);
        CHECK(satip_server_full_build_sdp(&s, 5000, 0xE1123456u, sdp, sizeof(sdp)) == 0);
        CHECK(strstr(sdp, "m=application/satip") != NULL);
        CHECK(satip_server_full_handle_msearch(&s, "urn:ses-com:satip:1.0:service:satip:1.0", resp, sizeof(resp), &resp_len) == 0);
        CHECK(strstr(resp, "200 OK") != NULL);
    }
}

/* DLNA server: M-SEARCH response. */
static void test_dlna_server(void)
{
    dlna_server_t d;
    char resp[1024];
    size_t resp_len = 0;

    CHECK(dlna_server_init(&d, 1900) == 0);
    CHECK(dlna_server_handle_msearch(&d, "urn:schemas-upnp-org:service:AVTransport:1", resp, sizeof(resp), &resp_len) == 0);
    CHECK(strstr(resp, "200 OK") != NULL);
}

/* AES-CTR: encrypt/decrypt round trip. */
static void test_crypto_ctr(void)
{
    ctr_crypto_t c;
    uint8_t key[16];
    uint8_t nonce[16];
    uint8_t in[32];
    uint8_t out[32];
    size_t i;

    for (i = 0; i < 16; ++i) { key[i] = (uint8_t)(0x30 + i); nonce[i] = (uint8_t)(0x40 + i); }
    for (i = 0; i < 32; ++i) { in[i] = (uint8_t)(0x50 + i); }
    memset(&c, 0, sizeof(c));   /* init() rejects a stale ctx pointer */
    CHECK(ctr_crypto_init(&c, CTR_CRYPTO_AES_128) == CTR_RET_OK);
    CHECK(ctr_crypto_encrypt(&c, key, nonce, in, 32, out, 32) == CTR_RET_OK);
    CHECK(ctr_crypto_decrypt(&c, key, nonce, out, 32, in, 32) == CTR_RET_OK);
    ctr_crypto_free(&c);
}

/* Runtime: task start/stop mapping. */
static void test_runtime(void)
{
    cleanroom_runtime_t r;

    CHECK(cleanroom_runtime_init(&r) == 0);
    CHECK(cleanroom_runtime_start_task(&r, 1) == 0);
    CHECK(cleanroom_runtime_stop_task(&r, 1) == 0);
}

/* RTSP server loop: init + open listener. */
static void test_rtsp_server_loop(void)
{
    rtsp_server_loop_t l;
    int fd = -1;

    CHECK(rtsp_server_loop_init(&l, CLEANROOM_RTSP_PORT) == 0);
    CHECK(rtsp_server_loop_open(&l, &fd) == 0);
    CHECK(fd > 0);
    net_socket_close(fd);
}

/* SSDP server loop: init + open UDP responder. */
static void test_ssdp_server_loop(void)
{
    ssdp_server_loop_t l;
    int fd = -1;

    CHECK(ssdp_server_loop_init(&l, 1900, "urn:schemas-upnp-org:service:AVTransport:1") == 0);
    CHECK(ssdp_server_loop_open(&l, &fd) == 0);
    CHECK(fd > 0);
    net_socket_close(fd);
}

/* AES-CTR key schedule: base key -> session key/nonce. */
static void test_crypto_key_schedule(void)
{
    ctr_key_schedule_t ks;
    uint8_t base_key[16];
    size_t i;

    for (i = 0; i < 16; ++i) { base_key[i] = (uint8_t)(0x60 + i); }
    CHECK(ctr_key_schedule_init(&ks, 0, base_key) == CTR_KEY_SCHED_OK);
    CHECK(ks.algo == CTR_CRYPTO_AES_128);
    CHECK(ks.nonce[0] == 1);
}

/* Token ledger: memory persistence hook. */
static void test_token_ledger_memory(void)
{
    token_ledger_t l;

    CHECK(token_ledger_init(&l) == TOKEN_LEDGER_OK);
    token_ledger_set_memory_store(&l);
    CHECK(token_ledger_credit(&l, 5) == TOKEN_LEDGER_OK);
}

/* End-to-end runtime: compose subsystems and start/stop. */
static void test_runtime_e2e(void)
{
    cleanroom_runtime_e2e_t e;

    CHECK(cleanroom_runtime_e2e_init(&e) == 0);
    CHECK(cleanroom_runtime_e2e_start(&e) == 0);
    CHECK(cleanroom_runtime_e2e_stop(&e) == 0);
}

/* Branded menu: AMS identity, modern sleek theme, view-stack navigation. */
static void test_menu(void)
{
    menu_brand_t b;
    menu_t m;
    menu_section_t s;
    menu_entry_t e;
    size_t i;

    b.brand = "AMS";
    b.phone = "+923060706989";
    b.tagline = "AMS IPTV / CAM receiver";

    CHECK(cleanroom_menu_init(&m, &b) == MENU_RET_OK);
    CHECK(strcmp(cleanroom_menu_brand_name(&m), "AMS") == 0);
    CHECK(strcmp(cleanroom_menu_brand_phone(&m), "+923060706989") == 0);

    menu_theme_default(&m.theme);
    CHECK(m.theme.corner_radius > 0u);
    CHECK(m.theme.item_height > 0u);
    CHECK(m.theme.font_scale > 0u);
    CHECK(m.theme.accent != 0u);

    /* HOME section: entries into the primary views. */
    memset(&s, 0, sizeof(s));
    s.title = "Home";
    s.entry_count = 0u;
    {
        menu_entry_t ents[2];
        ents[0].id = MENU_ITEM_LIVE_TV;  ents[0].label = "Live TV"; ents[0].target = MENU_VIEW_LIVE_TV; ents[0].enabled = true;
        ents[1].id = MENU_ITEM_YOUTUBE;  ents[1].label = "YouTube"; ents[1].target = MENU_VIEW_YOUTUBE; ents[1].enabled = true;
        for (i = 0u; i < 2u; ++i) { s.entries[s.entry_count] = ents[i]; s.entry_count += 1u; }
    }
    CHECK(cleanroom_menu_add_section(&m, &s) == MENU_RET_OK);

    /* LIVE_TV section: one entry back to Home. */
    memset(&s, 0, sizeof(s));
    s.title = "Live TV";
    s.entry_count = 0u;
    e.id = MENU_ITEM_ABOUT; e.label = "About"; e.target = MENU_VIEW_ABOUT; e.enabled = true;
    s.entries[s.entry_count] = e; s.entry_count = 1u;
    CHECK(cleanroom_menu_add_section(&m, &s) == MENU_RET_OK);

    CHECK(cleanroom_menu_navigate(&m, MENU_VIEW_HOME) == MENU_RET_OK);
    CHECK(strcmp(cleanroom_menu_view_title(MENU_VIEW_HOME), "Home") == 0);
    CHECK(strcmp(cleanroom_menu_view_title(MENU_VIEW_YOUTUBE), "YouTube") == 0);

    CHECK(cleanroom_menu_handle_key(&m, MENU_KEY_DOWN) == MENU_RET_OK);
    CHECK(cleanroom_menu_handle_key(&m, MENU_KEY_OK) == MENU_RET_OK);
    CHECK(cleanroom_menu_back(&m) == MENU_RET_OK);
    CHECK(cleanroom_menu_back(&m) == MENU_RET_OK);
    CHECK(cleanroom_menu_select(&m, MENU_ITEM_NONE) == MENU_RET_ERR);
    CHECK(cleanroom_menu_handle_key(&m, 0x99) == MENU_RET_OK);
}

int main(void)
{
    test_crc32();
    test_fw_update_frame();
    test_fw_nvm();
    test_cam_keys();
    test_cam_server();
    test_crypto_tls();
    test_ccam_client();
    test_net_transport();
    test_yt_proxy();
    test_iptv_backend();
    test_dvb_ts_mux();
    test_satip_server();
    test_rtsp_server();
    test_dlna_service();
    test_guardrails();
    test_cam_frame();
    test_http_client();
    test_yt_api();
    test_iptv_client();
    test_orchestrator();
    test_token_ledger();
    test_net_socket();
    test_rtsp_server_full();
    test_satip_server_full();
    test_dlna_server();
    test_crypto_ctr();
    test_runtime();
    test_rtsp_server_loop();
    test_ssdp_server_loop();
    test_crypto_key_schedule();
    test_token_ledger_memory();
    test_runtime_e2e();
    test_menu();

    if (g_failures == 0) {
        printf("cleanroom: ALL TESTS PASSED\n");
        return 0;
    }
    printf("cleanroom: %d TEST(S) FAILED\n", g_failures);
    return 1;
}
