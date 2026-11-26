#!/bin/sh
# Clean-room firmware build script (host build + self-test)
# Usage: sh ci/build.sh
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

mkdir -p build

gcc -std=c11 -Wall -Wextra -O2 -Isrc \
    src/main.c src/common.c src/fw_update.c src/fw_nvm.c src/cam_keys.c src/cam_server.c \
    src/crypto_tls.c src/cccam_client.c src/net_transport.c src/yt_proxy.c src/iptv_backend.c \
    src/dvb_ts_mux.c src/satip_server.c src/rtsp_server.c src/dlna_service.c src/guardrails.c \
    src/cam_frame.c src/http_client.c src/yt_api.c src/iptv_client.c \
    src/orchestrator.c src/token_ledger.c src/net_socket.c src/rtsp_server_full.c \
    src/satip_server_full.c src/dlna_server.c src/crypto_ctr.c src/runtime.c \
    src/rtsp_server_loop.c src/ssdp_server_loop.c src/crypto_key_schedule.c src/runtime_e2e.c \
    src/menu.c \
    -o build/cleanroom_p0 -lssl -lcrypto

echo "Build OK: build/cleanroom_p0"
./build/cleanroom_p0
