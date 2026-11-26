# CI BUILD FIXES — what had to change before the pipeline could produce artifacts

The clean-room tree that arrived in `h27.zip` **did not compile**: 30 of its 34
translation units failed with `-std=c11 -Wall -Wextra`. A workflow cannot produce
build artifacts from a tree that does not build, so the defects were fixed first.
Every change below is listed with the reason, so nothing here is a silent edit.

Verification after the fixes (host build):

* 34/34 translation units compile with **0 errors, 0 warnings**
* `build/cleanroom_p0` links against libcrypto
* the self-test prints `cleanroom: ALL TESTS PASSED`

---

## A. Missing standard includes (51 files)

Headers used `size_t` / `uint32_t` / `bool` / `NULL` without including
`<stddef.h>`, `<stdint.h>` or `<stdbool.h>`, and several `.c` files relied on
`NULL` transitively. Each file now includes what it uses.

Affected headers: `cam_frame.h`, `cam_keys.h`, `crypto_ctr.h`, `dlna_server.h`,
`dlna_service.h`, `dvb_ts_mux.h`, `guardrails.h`, `http_client.h`,
`iptv_client.h`, `net_socket.h`, `rtsp_server.h`, `rtsp_server_full.h`,
`rtsp_server_loop.h`, `satip_server.h`, `satip_server_full.h`,
`ssdp_server_loop.h`, `yt_api.h` (plus the matching `.c` files).

## B. Type and API mismatches

| File | Defect | Fix |
|---|---|---|
| `menu.h` | prototypes used `cleanroom_menu_t`, but the struct is typedef'd `menu_t` | added `typedef menu_t cleanroom_menu_t;` |
| `menu.h` | `menu_entry_t.target` was `menu_item_id_t` although it stores a **view** (compared against `menu_view_t`, passed to `cleanroom_menu_navigate`) | retyped to `menu_view_t` (identical value range, so behaviour is unchanged) |
| `iptv_backend.h` | declared `ipt_backend_init` / `ipt_backend_activate`; the implementation and `main.c` both call `iptv_backend_*` | header renamed to match |
| `iptv_backend.c` | `iptv_ret_t` (does not exist) as the return type of `iptv_backend_init` | `ipt_ret_t` |
| `yt_api.h` + `yt_proxy.h` | both defined `yt_ret_t` / `YT_RET_*`, so any file including both (e.g. `main.c`) failed | one guarded definition (`CLEANROOM_YT_RET_DEFINED`) |
| `token_ledger.c` | static helper `token_ledger_maybe_save` called before its definition → implicit non-static declaration, then a conflicting static definition | forward declaration |

## C. Logic bugs

| File | Defect | Fix |
|---|---|---|
| `common.c` | `uint32_t crc = cleanroom_crc32_init;` assigned a **function pointer** to the accumulator, so `cleanroom_crc32()` never started from `0xFFFFFFFF`. The self-test vector `CRC32("123456789") == 0xCBF43926` could not pass, and every frame/partition CRC built on it was wrong | `cleanroom_crc32_init(&crc)` |
| `fw_update.c` / `fw_update.h` | the encoder wrote `paylen` at offset **7**, overwriting `index` (6–9) and `subtype` (10), and `FW_UPDATE_HDR_SIZE` was 15 — the 4-byte `HEAD` sync was not counted, so payload and trailer CRC were read from the wrong place | layout is now `sync(4) type(1) flag(1) index(4) subtype(1) paylen(4) hdrcrc(4)` = 19 bytes; encoder and decoder use the same offsets and the header CRC covers bytes `[0,15)` |
| `cam_server.c` | `case CAM_CONN_IDLE` appeared twice in the transition switch → *duplicate case value* | second occurrence removed (idle is handled at the top) |
| `crypto_tls.c` | `EVP_DecryptUpdate(ctx, out, NULL, in, len)` — OpenSSL dereferences `*outl` unconditionally for custom ciphers such as AES-CTR (`crypto/evp/evp_enc.c`, `evp_EncryptDecryptUpdate`), so this was a crash/UB risk whose outcome depended on the OpenSSL build | real out-param, plus an `out_len >= in_len` bound check |
| `crypto_tls.c`, `crypto_ctr.c` | EVP out-params declared `unsigned int` but the API takes `int *` (`-Wpointer-sign`) | `int` |
| `guardrails.c` | `if (a == 192u) return true;` accepted **all** of 192.0.0.0/8 as "LAN" | RFC1918 `192.168.0.0/16` only |
| `iptv_backend.c` | `json_len = 0;` overwrote the caller's `size_t *` with NULL instead of writing through it | `*json_len = 0;` |
| `rtsp_server_full.c` | `c->method != NULL` on a **char array** is always true, so the recorded method was silently dropped when non-empty, and `snprintf` was used without `<stdio.h>` | condition corrected, header added |
| `runtime_e2e.c` | `_start()` opened the RTSP/SSDP sockets; `_stop()` never closed them | `_stop()` closes both and resets them to `-1` |
| `main.c` | `ctr_crypto_t c;` was passed to `ctr_crypto_init()` uninitialised, and init rejects a non-NULL `ctx` → nondeterministic failure depending on stack contents | zeroed before init |
| `main.c` | `fw_update_frame_decode()` received the **same** pointer for `payload_len` and `payload_actual`, and the declared `payload_actual` was unused | each out-param gets its own variable, and both are asserted |
| `cccam_client.c` | 32-byte buffer for a 40-character message (`-Wformat-truncation`), `snprintf` without `<stdio.h>` | buffer widened to 64, header added |
| `cam_server.c`, `net_socket.c`, `crypto_ctr.c`, `crypto_tls.c`, `guardrails.c`, `main.c` | unused variables/parameters | removed or `(void)`-cast |
| `cam_keys.c`, `rtsp_server.c` | unused static helpers (`cam_key_algo_evp`, `rtsp_state_str`) | kept and marked `__attribute__((unused))` — they are documented capability/debug hooks |

## D. CI- and environment-correctness

These were not compile errors; they were reasons the pipeline would have been red
or flaky on a GitHub-hosted runner.

| Item | Problem | Fix |
|---|---|---|
| RTSP self-test port | `rtsp_server_loop_init(&l, 554)` binds a **privileged** port; runners are not root, so `bind()` returns EACCES | `CLEANROOM_RTSP_PORT` defaults to the unprivileged alternate **8554**; `Makefile.mips` passes `-DCLEANROOM_RTSP_PORT=554` so device firmware keeps the real port |
| HTTP self-test | asserted `http_request("127.0.0.1", 80, …) == HTTP_RET_ERR`, which depends on nothing listening on port 80 *and* can never return that code for valid arguments | hermetic check: an unparseable host fails in `inet_pton()` before any `connect()`, asserting `HTTP_RET_ERR_CONN` |
| AES-CTR self-test | asserted `tls_crypto_aes_ctr(…) != 0`; it only "failed" because of the NULL out-param bug in C above | asserts success (`== 0`), which holds for any OpenSSL version |
| `Makefile.mips` | default `CROSS=mips-linux-gnu-` (big-endian) while `CFLAGS` forces `-EL` (little-endian) — objects would compile but never link against the big-endian sysroot libraries | default is now `mipsel-linux-gnu-`, matching the MIPS32 LE target |
| `Makefile.mips` | no way to build objects without the final `-lcrypto` link | added `objects`, `archive` and `print-%` targets |
| `ci/build.sh` | drifted from the Makefile — `src/menu.c` was missing, so the link failed with undefined `cleanroom_menu_*` | added |

## E. New CI tooling

| File | Purpose |
|---|---|
| `ci/tooltrace.py` | runs every step and records the **tool chain** (tool, resolved path, version, argv, timing, exit code) plus per-step `stdout`/`stderr`, and renders `TOOLCHAIN.md`, `ARTIFACT_CHAIN.md`, `ERRORS.md`, `CI_REPORT.md` |
| `ci/verify_artifacts.py` | re-checks the finished files independently of the build log: 4 MiB image size, NCRC magic per partition, per-partition CRC32, whole-image CRC32, ELF class/endian/machine/entry, slot fit, and that the maincode inside the image is byte-identical to the built binary |
| `ci/prepare_flash_parts.py` | stages bootloader / kernel / maincode for the packer, recording the provenance of each and marking the image `flashable: false` when any part is a synthetic 0xFF placeholder |
| `ci/ensure_tree.py` | uses the committed tree, or extracts `h27.zip` (normalising its Windows `\` separators) when the tree is absent, then fingerprints every source file into `tree-manifest.json` — the root of the artifact chain |
| `.github/workflows/cleanroom-build.yml` | the pipeline itself (replaces the previous draft, which could never run: it was inside the zip, filtered on paths that were not in the repository, and packed a flash image from `../../01_extracted/*.bin` files that are not committed) |
