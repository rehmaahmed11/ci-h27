# PHASE 4 — CLEAN-ROOM REDESIGN BLUEPRINT

**Source under analysis:** `D:\WORKSHOP\dump202611220912.abs` (ALi-style MIPS32 ISP firmware,
TurnRound IKS CAM manager / CCcam 2.2.x client / SoftCam / YouTube proxy / IPTV backend)
**Prepared from:** Phase 3 static decompilation (`re_workspace/03_analysis/PHASE3_ANALYSIS_REPORT.md`)
**Purpose:** specification for a **new, fully independent, clean-room implementation**
(firmware handler / server / client) that replicates — and where justified improves — the
recovered original capabilities.

---

## 1. Design Principles (Clean-Room)

1. **No vendor IP is reproduced.** All crypto, key material, protocol framing and business logic
   must be re-implemented from the *behavioural specification* derived here, not by transliteration
   of vendor assembly.
2. **Public / open primitives only.** AES (FIPS-197), AES-CTR, HMAC, SHA-256, ARC4 (where needed),
   TLS (TLS-1.2/1.3), standard base64/hex, standard JSON.
3. **Minimal dependencies.** Replace `libcurl` with a small async HTTP/FTP/HTTPS client; replace
   `libnetrtsp` with a compact RTSP state machine; replace custom LZMA1 wrapper with standard
   LZMA/lzma SDK (same wire parameters).
4. **Modular, testable units.** Every subsystem below is a standalone module with an interface.

---

## 2. Target Feature Modules (mapped to original subsystems)

### 2.1 Flash / Update Manager (`fw_update`)

```
module: FlashUpdateManager
inputs : image blob, partition table (NCRC header + CRC32), rollback state
outputs: burn schedule, verify, rollback
behaviour:
  - frame: 'H' 'E' 'A' 'D' | hdr[11] = type,flag,idx_BE32,subtype,paylen_BE32,payCRC_BE32
  - body = payload || CRC32(payload) (LE)
  - commands: address/burn/comtest/move/reboot/transfer/transferraw/version/dump/set
  - progress: 'B%04dK' ; error 'E%07d' ; 'FINISH'
  - CRC32: reflected, poly 0xEDB88320 (reflected 0x04C11DB7), init 0xFFFFFFFF, xorout 0xFFFFFFFF
```

### 2.2 NVM / Key Store (`fw_nvm`)

```
module: NvmStore
api:
  TurnRoundNVM_Read / TurnRoundNVM_Write
  fill-flag test (all 0xFF) -> erase path
stores: HDCPKey (0x70 B), TurnRoundKey, DefTurnRoundKey, defaultdb, SysData, userdb, …
```

### 2.3 SoftCam / CAM Key Management (`cam_keys`)

```
module: SoftCamKeyStore
sources:
  - flash default partition (DefTurnRoundKey / blcommondata / HDCPKey)
  - USB (auto-add: cryptoworks, viaccess, biss-Q, powervu T/P/Z/Y lines)
  - /mnt/uda1/SoftCam.key  (SoftCam.key file, "W <ident> <idx> <16B key> ;provider" format)
algorithms: AES-128-CBC, AES-192-CBC, AES-256-CBC, AES-128-CTR … (FIPS-197)
hdcp: first byte 0x01, KSV validation
```

### 2.4 CAM / IKS Server (`cam_server_bxcam`, `cam_server_foxcam`)

```
module: CamIksServer
connection state machine:
  IDLE -> STOP
  -> CONNECT_GATE_SERVER -> LOGIN_GATE_SERVER -> CONNECT_CW_SERVER -> LOGIN_CW_SERVER
     -> REQUEST_CHANNEL_LIST -> SET_CHANNEL -> RETRIEVE_CW | KEEP_ALIVE | RECHARGE
events: EN_TOKENS_REPORTING, PI_TOKEN_DEBIT_INFO, auth type, Token <n>, b8Insufficient_Tokens
sub-protocols: IKS (Login/SetCW/IKSPing), Gate, CW (SONCARD), AAA, BXCAM/FOXCAM server tasks
```

### 2.5 CCcam Client (`cccam_client`)

```
module: CCcamClient
version: 2.2.x
commands: <implemented subset> ; "cccam 2.2.x commands not implemented: 0x%02X"
keepalive: 'cccam: keepalive'
manager: server_cccam_manager_unkown_act_proc
```

### 2.6 HTTP / FTP / RTSP / HTTPS Client (`net_transport`)

```
module: HttpFtpRtspClient
protocols: HTTP/1.0, HTTP/1.1, HTTPS, FTP, RTSP/1.x, ICY
features: cookie store (# Netscape HTTP Cookie File, #HttpOnly_), auth (Basic, Digest),
  byte-range resume, chunked upload NOT supported (HTTP 1.0), CONNECT proxy, TLS
lib replacements: libcurl, libnetrtsp, OpenSSL/SSLeay, mbedTLS
```

### 2.7 YouTube Proxy / Player (`yt_proxy`)

```
module: YouTubeProxy
api_base: https://www.googleapis.com/youtube/v3
watch_url: https://www.youtube.com/watch?v=
token_url: https://u2b.sysio.info:18000/u2b/parse_token
session: https://u2b.sysio.info:18000/u2b/parse
player config keys: url_encoded_fmt_stream_map, assets, js, videoCategories?part=snippet,
                   videos?part=snippet.<ver>
player js: ytplayer.config / ytplayer.load = function(){args}
charset: 0123456789.+-eE, 0123456789abcdef ; escapes: \b \n \r \t \" \\ /
```

### 2.8 IPTV Backend Integration (`iptv_backend`)

```
module: IptvBackend
panel:      http://panel.ip-hd.net/app/v2/{login,playlist,member}.php
golden:      http://golden.mmsscode.com/Protocol/{Activate,Run}.php
sonic:       http://sonres.com/sonic/resevers/{Activate,Run}.php
sstv:        http://sstv3.ddnb.tn/{act,pack,channels}.php
api:         https://api.ipfox.co
register:    http://gogoday.info/demo/ci_demo/index.php/hiporegister/create_register
demo:        http://172.104.99.93/demo/ci_demo/index.php/hipopatch/
api key:     https://www.googleapis.com/youtube/v3
```

Register / activation JSON shapes (recovered templates):

```json
{'customer':'%s','model':'%s','chipsn':'%s','mac':'%02x%02x%02x%02x%02x%02x%02x%02x',
 'version':'%s','time':'%d'}
{'username':'%s','card':'%s','time':'%d'}
{'username':'%s','card':'%s','time':'%d'}
```

---

## 3. Clean-Room Implementation Specification

### 3.1 Firmware Handler (new custom handler)

```
Entry: fw_entry (MIPS32 LE32, base 0x8xxxxxxx in new image)
  init:   set Cop0 SVP0/HPEN? -> reset behaviour per original (Config & 0xf1ff7ff8 | 0x6230003)
  tasks: TaskScheduler (8 slots x 0x2d8)
  loop:   task poll -> dispatch
```

### 3.2 Wire Protocol Summary (clean-room, public framing)

**Flash frame**

```
'H','E','A','D' | type | flag | idx:BE32 | subtype | paylen:BE32 | payCRC:BE32
  | payload[len..len+paylen-1] | CRC32(payload):BE32
```

**IKS / CAM frames**: binary with identifiers, sub-IDs, `dcw` (descrambling control word),
`csaUsed/fixedKey/oddKey/bid/keyIndex/channelId`, `pmt_index/ecm_index/host`,
`Token`, `auth type`, `INSUFFICIENT_TOKENS`.

### 3.3 Module Interfaces (C header sketch)

```c
typedef struct { uint32_t state; uint32_t sub; uint32_t token; uint32_t auth_type; } iks_ctx_t;

cam_ret cam_server_task(iks_ctx_t *c, cam_event_t ev);
ccam_ret ccam_task(uint8_t cmd_not_impl_mask, uint8_t *out, uint32_t *len);
http_ret http_download(const char *url, uint8_t **buf, uint32_t *len, http_err_t *err);
rtsp_ret rtsp_session_open(rtsp_session_t *s, const char *url);
yt_ret   yt_player_config(const char *fmt_stream_map, char **js, char **cat_url);
ipt_ret  ipt_backend_activate(const char *backend, const char *ident, char *json_out, uint32_t *json_len);
```

### 3.4 Security Requirements (clean-room additions recommended)

- Replace hardcoded/weak credential stores with secure key derivation (PBKDF2/SHA-256).
- Move tokens to server-side authoritative store; keep client token debit ledger local.
- TLS 1.2+ with certificate pinning (original used TLS_PSK / NTRU / custom mixes — flagged as
  high-risk custom crypto, NOT recommended for clean-room).
- Remove demo/registration endpoints and hardcoded activation URLs from production builds.

### 3.5 Testing / Validation Plan

1. Unit: CRC32, LZMA round-trip, AES-CBC/CTR vectors, HMAC-SHA256.
2. Protocol: frame encode/decode, CRC check, response code mapping (`r101 … r999`, `err1`, `err2`).
3. State machine: CAM connection transitions incl. timeouts, retries, token debit.
4. Backend integration: playlist/activate/run request/response JSON conformance.
5. YouTube: fmt stream map parsing, JS injection, category URL shape.

---

## 4. Clean-Room Deliverable Summary

| # | Subsystem | Original location (analysis) | Clean-room module | Priority |
|---|---|---|---|---|
| 1 | Flash update manager | `ClearFlowAndRepairCmd`, table `0x8101b470` | `fw_update` | P0 |
| 2 | NVM store | `FUN_8100dfc8`, NVM RW | `fw_nvm` | P0 |
| 3 | SoftCam / CAM keys | `_turnround_softcam_*`, USB adders, `/mnt/uda1/SoftCam.key` | `cam_keys` | P0 |
| 4 | CAM IKS server (bxcam/foxcam) | `_turnround_server_*` task family | `cam_server` | P0 |
| 5 | CCcam client 2.2.x | `cccam:*` strings, manager | `cccam_client` | P1 |
| 6 | HTTP/FTP/RTSP | `http_download`, `libnetrtsp_*`, libcurl strings | `net_transport` | P1 |
| 7 | YouTube proxy/player | `ytplayer.*`, googleapis/u2b URLs | `yt_proxy` | P1 |
| 8 | IPTV backend | panel/golden/sonic/sstv/api/register URLs + JSON | `iptv_backend` | P1 |
| 9 | Crypto/TLS | AES-*, TLS_*, SSL_*, ARC4, custom mixes | `crypto_tls` | P2 (audit) |
| 10 | TS tap / LAN streamer mux | `dvb_ts_mux` (P1.1) | `dvb_ts_mux` | P1.1 |
| 11 | SAT>IP server | `satip_server` (P1.1) | `satip_server` | P1.1 |
| 12 | RTSP server | `rtsp_server` (P1.1) | `rtsp_server` | P1.1 |
| 13 | DLNA service | `dlna_service` (P1.1) | `dlna_service` | P1.1 |
| 14 | Guardrails | `guardrails` (P1.1) | `guardrails` | P1.1 |
| 15 | CAM frame codec | `cam_frame` (P2) | `cam_frame` | P2 |
| 16 | HTTP/TLS client | `http_client` (P2) | `http_client` | P2 |
| 17 | YouTube API client | `yt_api` (P2) | `yt_api` | P2 |
| 18 | IPTV backend client | `iptv_client` (P2) | `iptv_client` | P2 |
| 19 | Orchestrator / task scheduler | `orchestrator` (P3) | `orchestrator` | P3 |
| 20 | Token debit ledger | `token_ledger` (P3) | `token_ledger` | P3 |
| 21 | Socket transport (TCP/UDP) | `net_socket` (P4) | `net_socket` | P4 |
| 22 | RTSP server full | `rtsp_server_full` (P4) | `rtsp_server_full` | P4 |
| 23 | SAT>IP server full | `satip_server_full` (P4) | `satip_server_full` | P4 |
| 24 | DLNA server | `dlna_server` (P4) | `dlna_server` | P4 |
| 25 | AES-CTR crypto | `crypto_ctr` (P4) | `crypto_ctr` | P4 |
| 26 | Runtime facade | `runtime` (P4) | `runtime` | P4 |
| 27 | RTSP server loop | `rtsp_server_loop` (P5) | `rtsp_server_loop` | P5 |
| 28 | SSDP server loop | `ssdp_server_loop` (P5) | `ssdp_server_loop` | P5 |
| 29 | AES-CTR key schedule | `crypto_key_schedule` (P5) | `crypto_key_schedule` | P5 |
| 30 | Token ledger persistence | `token_ledger` (P5) | `token_ledger` | P5 |
| 31 | End-to-end runtime | `runtime_e2e` (P6) | `runtime_e2e` | P6 |
| 32 | Branded menu subsystem | `menu` (P7) | `menu` | P7 |

---

## 5. Phase 4 Conclusion

The recovered firmware is best characterised as a **CAM-centric OTT/IPTV receiver**: a MIPS32
firmware that (a) manages flash/NVM, (b) runs a CAM server speaking an IKS protocol, (c) consumes
CCcam-style clients, (d) manages SoftCam/AES keys, (e) fetches ECM/EMM/playlist data over
HTTP(S)/FTP/RTSP, (f) proxies YouTube via Google APIs, and (g) integrates with subscription
panels.

The clean-room redesign should re-implement these capabilities using only public cryptographic
primitives and standard protocol implementations, organised into the eight modules listed above,
with the flash frame / CRC / command set specified in §3.2 as the integration contract.
