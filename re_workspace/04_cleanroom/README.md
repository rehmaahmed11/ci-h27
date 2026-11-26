# Clean-Room Firmware Subsystems — P0 / P1 / P1.1 Source

This directory contains the **clean-room** C implementation of the recovered firmware
subsystems.  It is written from the behavioural specification produced by the reverse-engineering
phases; no vendor firmware code or binary artefacts are used or reproduced.

## Layout

```
04_cleanroom/
  Makefile
  README.md            (this file)
  CLEANROOM_BLUEPRINT.md
  src/
    common.h / common.c
    fw_update.h / fw_update.c
    fw_nvm.h    / fw_nvm.c
    cam_keys.h  / cam_keys.c
    cam_server.h / cam_server.c
    crypto_tls.h / crypto_tls.c
    cccam_client.h / cccam_client.c
    net_transport.h / net_transport.c
    yt_proxy.h / yt_proxy.c
    iptv_backend.h / iptv_backend.c
    dvb_ts_mux.h / dvb_ts_mux.c          <-- P1.1: TS tap ring buffer
    satip_server.h / satip_server.c      <-- P1.1: SAT>IP server helpers
    rtsp_server.h / rtsp_server.c        <-- P1.1: RTSP server session
    dlna_service.h / dlna_service.c      <-- P1.1: DLNA service helpers
    guardrails.h / guardrails.c          <-- P1.1: resource & security guardrails
    cam_frame.h / cam_frame.c            <-- P2: CAM/IKS frame codec
    http_client.h / http_client.c        <-- P2: HTTP/1.1 + TLS client
    yt_api.h / yt_api.c                  <-- P2: YouTube player-config API client
    iptv_client.h / iptv_client.c        <-- P2: IPTV backend activate/run client
    main.c
```

## Implemented modules

| Module | Files | Purpose | Phase |
|---|---|---|---|
| common | `src/common.[ch]` | CRC32 (reflected 0xEDB88320), BE32 helpers, ret codes | P0 |
| fw_update | `src/fw_update.[ch]` | Flash OTA frame encode/decode, progress/error text, response codes | P0 |
| fw_nvm | `src/fw_nvm.[ch]` | NVM partition R/W, erase-flag (all-0xFF) test, erase | P0 |
| cam_keys | `src/cam_keys.[ch]` | SoftCam key store (AES-128/192/256 CBC & CTR via OpenSSL EVP), HDCP KSV check | P0 |
| cam_server | `src/cam_server.[ch]` | TurnRound IKS CAM connection state machine | P0 |
| crypto_tls | `src/crypto_tls.[ch]` | AES-128/192/256 CBC & CTR, ARC4, TLS capability layer | P1 |
| cccam_client | `src/cccam_client.[ch]` | CCcam 2.2.x client: version, keepalive, cmd dispatch, not-impl | P1 |
| net_transport | `src/net_transport.[ch]` | HTTP/HTTPS/FTP/RTSP/ICY/MMS transport surface, ret codes | P1 |
| yt_proxy | `src/yt_proxy.[ch]` | YouTube API base, watch/token/parse URLs, player config, tokens | P1 |
| iptv_backend | `src/iptv_backend.[ch]` | IPTV panel/golden/sonic/sstv/api/register/demo endpoints, activate JSON | P1 |
| dvb_ts_mux | `src/dvb_ts_mux.[ch]` | DVB TS packet ring buffer (188 B) between CAM/descrambler output and LAN streamers | P1.1 |
| satip_server | `src/satip_server.[ch]` | EN 50585 SAT>IP: SSDP + `/desc.xml` + SDP builder for single-transponder RTP | P1.1 |
| rtsp_server | `src/rtsp_server.[ch]` | RFC 2326 RTSP session: OPTIONS/DESCRIBE/SETUP/PLAY/PAUSE/TEARDOWN | P1.1 |
| dlna_service | `src/dlna_service.[ch]` | UPnP SSDP + SOAP AVTransport/RenderingControl/ContentDirectory | P1.1 |
| guardrails | `src/guardrails.[ch]` | Max concurrent streams + LAN-only bind/ACL guardrails | P1.1 |
| cam_frame | `src/cam_frame.[ch]` | CAM/IKS binary frame codec (cmd/flags/sid/sub/token/auth/dcw/status) | P2 |
| http_client | `src/http_client.[ch]` | HTTP/1.1 + TLS1.2/1.3 client (OpenSSL), status/body | P2 |
| yt_api | `src/yt_api.[ch]` | YouTube player-config API client (fmt stream map) | P2 |
| iptv_client | `src/iptv_client.[ch]` | IPTV backend activate/run HTTP client | P2 |
| orchestrator | `src/orchestrator.[ch]` | Task scheduler facade wiring CAM/CCAM/streamer/update tasks | P3 |
| token_ledger | `src/token_ledger.[ch]` | CAM token debit/credit ledger + INSUFFICIENT handling | P3 |
| net_socket | `src/net_socket.[ch]` | BSD-socket TCP/UDP transport facade (P4) | P4 |
| rtsp_server_full | `src/rtsp_server_full.[ch]` | RTSP server facade on real TCP socket + state machine | P4 |
| satip_server_full | `src/satip_server_full.[ch]` | SAT>IP desc/SDP + SSDP M-SEARCH responder (P4) | P4 |
| dlna_server | `src/dlna_server.[ch]` | DLNA SSDP M-SEARCH responder (P4) | P4 |
| crypto_ctr | `src/crypto_ctr.[ch]` | Real AES-128/192/256 CTR encrypt/decrypt via OpenSSL EVP (P4) | P4 |
| runtime | `src/runtime.[ch]` | End-to-end runtime facade (P4) | P4 |
| rtsp_server_loop | `src/rtsp_server_loop.[ch]` | RTSP accept/read/process/write loop (P5) | P5 |
| ssdp_server_loop | `src/ssdp_server_loop.[ch]` | SSDP responder loop (P5) | P5 |
| crypto_key_schedule | `src/crypto_key_schedule.[ch]` | AES-CTR key/nonce scheduling (P5) | P5 |
| token_ledger | `src/token_ledger.[ch]` | Token debit ledger + memory persistence (P5) | P5 |
| runtime_e2e | `src/runtime_e2e.[ch]` | End-to-end composed runtime (P6) | P6 |
| menu | `src/menu.[ch]` | Branded menu subsystem: AMS identity (+923060706989), modern sleek theme, view-stack navigation (P7) | P7 |
| main | `src/main.c` | Self-test for the above subsystems | — |

## Build instructions

Requirements: a C11 compiler and OpenSSL (`libcrypto`) for `cam_keys` / `crypto_tls`.

```sh
cd D:\Users\AHMED\Documents\Default Project\re_workspace\04_cleanroom

# Linux / macOS
make

# or directly with gcc
gcc -std=c11 -Wall -Wextra -O2 -Isrc \
    src/main.c src/common.c src/fw_update.c src/fw_nvm.c src/cam_keys.c src/cam_server.c \
    src/crypto_tls.c src/cccam_client.c src/net_transport.c src/yt_proxy.c src/iptv_backend.c \
    src/dvb_ts_mux.c src/satip_server.c src/rtsp_server.c src/dlna_service.c src/guardrails.c \
    src/cam_frame.c src/http_client.c src/yt_api.c src/iptv_client.c \
    src/orchestrator.c src/token_ledger.c \
    src/net_socket.c src/rtsp_server_full.c src/satip_server_full.c src/dlna_server.c \
    src/crypto_ctr.c src/runtime.c \
    src/rtsp_server_loop.c src/ssdp_server_loop.c src/crypto_key_schedule.c \
    src/runtime_e2e.c \
    -o build/cleanroom_p0 -lcrypto

# run
./build/cleanroom_p0
```

Clean build (no warnings expected):

```sh
make clean && make
```

## Notes

- No binary images, packed/compressed firmware blobs, hex dumps or other artefacts are part of
  this source tree; only textual C source, the Makefile and this README.
- `cam_keys`, `crypto_tls` depend on OpenSSL (`libcrypto`); all cryptographic primitives are the
  standard, open implementations.
- P0/P1 modules are interface/capability-level implementations per the clean-room blueprint; wire
  level cryptographic and transport operation are isolated behind these interfaces for the next
  phase.
- **P1.1 additions** add the LAN streaming front-end stack on top of the P0/P1 core:
  - `dvb_ts_mux` — 188-byte TS ring buffer between descrambler output and streamers (no P0 wire
    protocol changes).
  - `satip_server` — SAT>IP (EN 50585) description + SDP generation.
  - `rtsp_server` — RFC 2326 session state machine with RTP/UDP and RTP/TCP interleaved modes.
  - `dlna_service` — UPnP SSDP + SOAP AVTransport/RenderingControl/ContentDirectory.
  - `guardrails` — configurable max concurrent streams (1-2 single-transponder streams) and
    LAN-only bind/ACL enforcement.
- **P2 (current) additions** — integration layer:
  - `cam_frame` — CAM/IKS binary frame codec (Login/SetCW/IKS-Ping/Token envelopes).
  - `http_client` — HTTP/1.1 client with TLS1.2/1.3 (OpenSSL), status/body.
  - `yt_api` — YouTube player-config API client (fmt stream map via `player?part=stream`).
  - `iptv_client` — IPTV backend activate/run HTTP client (JSON POST).
- **P3 (current) additions** — orchestration layer:
  - `orchestrator` — task scheduler facade (`orch_task_*`, `orch_scheduler_*`) wiring CAM / CAM-server /
    CCAM / streamer (SAT-IP, RTSP, DLNA) / update tasks.
  - `token_ledger` — token debit/credit ledger with INSUFFICIENT detection (`token_ledger_*`).
- Next step: **P4** — complete RTSP / DLNA / SAT-IP server implementations on real sockets; full
  crypto (AES-CTR key schedule, token debit ledger persistence); end-to-end runtime integration.

## Device build (MIPS cross-compile) and flash image
- Device entry point: src/device_main.c (app_main -> main) wiring SAT>IP/RTSP/DLNA/SSDP + menu.
- Cross-compile: make -f Makefile.mips -> uild-mips/cleanroom_mips.bin (requires mips-linux-gnu-gcc + SoC SDK for hal_init).
- Flash image packer: python3 tools/pack_flash_image.py --bootloader ... --kernel ... --maincode build-mips/cleanroom_mips.bin --out build/flash_image.bin
  (reuses original extracted bootloader + kernel; replaces only maincode partition).
- See BOOT_READINESS_GAP_REPORT.md for hardware bring-up requirements.
