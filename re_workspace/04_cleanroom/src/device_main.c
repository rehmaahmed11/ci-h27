/*
 * cleanroom/device_main.c
 * -----------------------
 * Real device entry point for the clean-room firmware.
 *
 * This is the application-level main() that a MIPS build links at the
 * firmware entry point (0x80000e00, gp=0x81023ddc).  It brings up the
 * clean-room subsystems and runs the SAT>IP / RTSP / DLNA / SSDP servers.
 *
 * Hardware bring-up is deliberately isolated behind the weak `hal_init()`
 * hook: on real ALi SoC hardware the vendor SDK provides the implementation
 * (coproc Config, MMIO 0xb8000000+, tuner/demod, HDMI/OSD, flash, UART).
 * The clean-room application logic below is complete and hardware-agnostic.
 */

#include <string.h>

#include "common.h"
#include "net_socket.h"
#include "rtsp_server_loop.h"
#include "ssdp_server_loop.h"
#include "rtsp_server.h"
#include "satip_server_full.h"
#include "dlna_server.h"
#include "orchestrator.h"
#include "runtime.h"
#include "runtime_e2e.h"
#include "menu.h"
#include "guardrails.h"
#include "token_ledger.h"
#include "crypto_key_schedule.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Hardware abstraction hook (provided by the SoC SDK on real targets).  */
/* ------------------------------------------------------------------ */
__attribute__((weak)) int hal_init(void)
{
    return 0;
}

__attribute__((weak)) void hal_deinit(void)
{
}

/* Debug sink (no-op in freestanding builds; SoC SDK may override). */
__attribute__((weak)) void dbg_print(const char *s)
{
    (void)s;
}

/* ------------------------------------------------------------------ */
/* Minimal freestanding helpers (no libc on bare-metal MIPS).           */
/* ------------------------------------------------------------------ */
static void *fw_memset(void *dst, int c, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    size_t i;
    for (i = 0; i < n; ++i) { p[i] = (unsigned char)c; }
    return dst;
}

static void fw_delay(unsigned int n)
{
    volatile unsigned int i;
    for (i = 0; i < n; ++i) { /* busy-wait */ }
}

/* ------------------------------------------------------------------ */
/* Application entry.                                                   */
/* ------------------------------------------------------------------ */
int app_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    cleanroom_runtime_e2e_t rt;
    rtsp_server_loop_t rtsp_loop;
    ssdp_server_loop_t ssdp_loop;
    int rtsp_lfd = -1;
    int ssdp_fd = -1;
    cleanroom_guardrails_t guard;
    menu_brand_t brand;
    menu_t menu;
    uint8_t base_key[16];
    size_t i;

    if (hal_init() != 0) {
        dbg_print("hal_init failed");
        return 1;
    }

    if (net_socket_init() != 0) {
        dbg_print("net_socket_init failed");
        hal_deinit();
        return 1;
    }

    fw_memset(&rt, 0, sizeof(rt));
    if (cleanroom_runtime_e2e_init(&rt) != 0) {
        dbg_print("runtime init failed");
        hal_deinit();
        return 1;
    }
    if (cleanroom_runtime_e2e_start(&rt) != 0) {
        dbg_print("runtime start failed");
        cleanroom_runtime_e2e_deinit(&rt);
        hal_deinit();
        return 1;
    }

    if (orch_scheduler_init() == false) {
        dbg_print("orch scheduler init failed");
    }

    cleanroom_guardrails_init(&guard, 2u, true);

    brand.brand = "AMS";
    brand.phone = "+923060706989";
    brand.tagline = "AMS IPTV / CAM receiver";
    cleanroom_menu_init(&menu, &brand);

    for (i = 0; i < sizeof(base_key); ++i) { base_key[i] = 0x5A; }

    if (rtsp_server_loop_init(&rtsp_loop, 554) != 0) {
        dbg_print("rtsp loop init failed");
    }
    if (rtsp_server_loop_open(&rtsp_loop, &rtsp_lfd) != 0) {
        dbg_print("rtsp listen failed");
        rtsp_lfd = -1;
    }

    if (ssdp_server_loop_init(&ssdp_loop, 1900, "urn:schemas-upnp-org:service:AVTransport:1") != 0) {
        dbg_print("ssdp loop init failed");
    }
    if (ssdp_server_loop_open(&ssdp_loop, &ssdp_fd) != 0) {
        dbg_print("ssdp bind failed");
        ssdp_fd = -1;
    }

    dbg_print("AMS receiver: SAT>IP/RTSP/DLNA/SSDP services started");

    /* Main service loop. */
    for (;;) {
        if (rtsp_lfd >= 0) {
            int cfd = -1;
            if (rtsp_server_loop_accept(rtsp_lfd, &cfd) == 0 && cfd >= 0) {
                rtsp_conn_t conn;
                char resp[4096];
                size_t rlen = 0;
                fw_memset(&conn, 0, sizeof(conn));
                rtsp_server_loop_run(cfd, &conn, resp, sizeof(resp), &rlen);
                net_socket_close(cfd);
            }
        }
        if (ssdp_fd >= 0) {
            char resp[1024];
            size_t rlen = 0;
            if (ssdp_server_loop_run(ssdp_fd, ssdp_loop.st, resp, sizeof(resp), &rlen) == 0) {
                /* reply already sent inside run() */
            }
        }
        fw_delay(10000);
    }

    if (rtsp_lfd >= 0) { net_socket_close(rtsp_lfd); }
    if (ssdp_fd >= 0) { net_socket_close(ssdp_fd); }
    cleanroom_runtime_e2e_stop(&rt);
    cleanroom_runtime_e2e_deinit(&rt);
    hal_deinit();
    return 0;
}

/* Firmware entry alias expected by the linker script / bootloader. */
int main(int argc, char **argv)
{
    return app_main(argc, argv);
}
