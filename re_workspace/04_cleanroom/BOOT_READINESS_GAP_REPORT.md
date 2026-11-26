# REAL-BOOT READINESS GAP REPORT — CLEAN-ROOM vs. EXTRACTED FIRMWARE

**Target:** MIPS32 LE dish receiver (ALi-class SoC, NCRC flash layout)
**Clean-room deliverable:** `re_workspace/04_cleanroom/` (P0 → P7, FINAL, source-only)
**Source dump:** `D:\WORKSHOP\dump202611220912.abs`

## 1. Executive answer

**The clean-room deliverable CANNOT boot on a real MIPS dish receiver as it stands today.**

It is a **portable capability reimplementation** (CAM server, CCcam, RTSP/SAT>IP/DLNA,
crypto, HTTP/TLS, YouTube/IPTV clients, orchestrator, token ledger, branded menu model).
It is **not** a firmware image, has no bootloader, no kernel, no SoC drivers, no entry
point, and no hardware bring-up. The original `maincode` partition (~10 MB of MIPS)
contains the actual drivers (tuner, demod, HDMI/OSD, flash controller, UART, etc.)
which the clean-room does **not** reimplement.

## 2. What we extracted (from the start) — inventory

- **Bootloader** (`part_bootloader_raw.bin`, ~0x3800): NCRC-flashed, v1.0.0, 2016-07-22.
- **Kernel** (`part_kernel_raw.bin`, ~0x1C600): custom MIPS, base `0x81000000`,
  388 functions, flash/OTA protocol, NVM, partition CRC, task framework, command
  dispatcher (`address, burn, comtest, move, reboot, transfer, transferraw, version, dump, set`).
- **Maincode** (`part_maincode_raw.bin`, 0x952118): base `0x80000000`, entry `0x80000e00`,
  real flow `0x80003b2c`, `gp=0x81023ddc`. ~1321 decompiled function blocks.
- **MMIO / SoC bus:** `0xb8000000` / `0xb801xxxx` / `0xb803xxxx` (ALi SoC).
- **Partition table:** NCRC, 14 partitions (bootloader, kernel, HDCPKey, blcommondata,
  commondata, maincode, osd_string, defaultdb, Sys Data, DefTurnRoundKey, MenuBackground,
  TurnRoundKey, Radioback, userdb).
- **Compression:** custom LZMA1 wrapper (`6C 00 00 80`, props 0x6C, dict 0x800000, LE64 usize @+5).
- **Flash OTA frame:** sync `HEAD`, hdr `[type][flag][idx BE32][subtype][paylen BE32][payCRC BE32]`,
  payload + CRC32 trailer (reflected poly 0xEDB88320).
- **Response codes:** `r101 r301 r302 r401 r402 r403 r501 r502 r503 r601 r801 r901 r902 r999 err1 err2`;
  progress `B%04dK`, error `E%07d`, `FINISH`.
- **Capabilities:** CAM (BXCAM/FOXCAM), CCcam 2.2.x, SoftCam keys, AES/TLS, HTTP/FTP/RTSP,
  YouTube proxy, IPTV panels, flash manager, OSD strings (6 languages), menu background.

## 3. Audit of the clean-room source (no hardware references)

Automated scan of `04_cleanroom/src/*.c`:

| Check | Result |
|---|---|
| Files referencing MMIO (`0xb8…`) | **0** |
| Files referencing coprocessor config (`setCopReg`/`Config`) | **0** |
| Files with `main()` (device entry) | **1** (`main.c` — self-test harness only) |
| Files with hardware drivers (tuner/demod/HDMI/flash/UART) | **0** |
| Modules explicitly labeled "facade" | net_socket, rtsp_server_full, satip_server_full, dlna_server, orchestrator, runtime |
| Modules with unimplemented command paths | cccam_client (`"cccam 2.2.x commands not implemented: 0x%02X"`) |

**Conclusion: zero hardware bring-up exists in the clean-room. It cannot initialize the SoC,
boot, or drive any peripheral.**

## 4. Gap list — what is missing for real boot

### A. Boot path (critical)
1. **Bootloader** — extracted (`part_bootloader_raw.bin`) but not reimplemented. The original
   bootloader handles NCRC boot, partition CRC, and jumps to kernel. *Reusable as-is.*
2. **Kernel** — extracted (`part_kernel_raw.bin`) but not reimplemented. Handles flash/OTA,
   NVM, task framework, command dispatcher. *Reusable as-is.*
3. **Entry point** — must be at `0x80000e00` with `gp=0x81023ddc` and coproc `Config`
   init (`Config & 0xf1ff7ff8 | 0x6230003`, cop0_reg22 |= 1) then call `0x80003b2c`.
   No such code exists.
4. **Hardware init** — MMIO at `0xb8000000+` (ALi SoC). No drivers exist.

### B. Capability gaps (stubs / facades)
5. **rtsp_server_loop** — reads a request but never writes the response to the socket.
6. **ssdp_server_loop** — binds UDP but the reply path is not wired to a real send.
7. **crypto_ctr / crypto_tls** — depend on OpenSSL (`-lcrypto`), which is not available on
   the target MIPS SoC without porting; need a portable crypto implementation or SoC crypto.
8. **menu** — model + theme + navigation only; **no renderer** (no OSD/framebuffer).
9. **cccam_client** — command dispatch incomplete ("not implemented" for unknown cmds).
10. **net_socket** — BSD sockets; on a bare-metal MIPS SoC there is no socket layer
    without a TCP/IP stack (lwIP or vendor stack).

### C. Build / image gaps
11. **No MIPS cross-toolchain build** — only host `gcc` build exists.
12. **No flash image generator** — NCRC container + partition table + CRC not produced.
13. **No device `main()`** — only a self-test harness.

### D. Scope reality
14. The original `maincode` is ~10 MB of MIPS with full hardware drivers. Reimplementing
    all of it as clean-room C is a multi-month effort requiring the **ALi SoC SDK**.
    The clean-room covers the *application-level capabilities* (CAM, IPTV, YouTube,
    crypto, protocols, menu model) but not the *hardware abstraction layer*.

## 5. Recommended path to a real boot

### Short-term (reuse extracted binaries — fastest to a booting device)
1. Keep the **original bootloader + kernel** (extracted, proven) in the flash image.
2. Keep the **original maincode** for hardware drivers; inject only the clean-room
   capability modules where they can be called from the original (not feasible without
   the original's build system).
3. Re-flash the original dump as-is to confirm the hardware boot path works (baseline).

### Medium-term (clean-room on real hardware)
4. Obtain the **ALi SoC SDK** (compiler, drivers, BSP) for the target chip (M36xx/M37xx family).
5. Implement hardware init: coproc `Config`, MMIO `0xb8000000+`, tuner/demod, HDMI/OSD,
   flash controller, UART — using the SDK.
6. Implement the boot entry at `0x80000e00` → `0x80003b2c` with `gp=0x81023ddc`.
7. Port crypto to a MIPS-friendly library (mbedTLS or SoC crypto) instead of OpenSSL.
8. Replace BSD sockets with the vendor TCP/IP stack or lwIP.
9. Implement the OSD renderer for the menu (framebuffer blit using extracted
   `MenuBackground` / `osd_string` resources).
10. Cross-compile with `mips-linux-gnu-gcc` (or vendor toolchain) targeting
    `0x80000000`, entry `0x80000e00`.
11. Generate the NCRC flash image (bootloader + kernel + maincode + resources + CRC).
12. Flash and validate.

### Long-term
13. Full replacement of `maincode` with clean-room C, dropping the original binary
    entirely (requires complete driver reimplementation).

## 6. What the clean-room deliverable IS good for
- Studying / reusing the **application-level logic** (CAM protocol, IPTV panels,
  YouTube proxy, crypto suites, OTA frame format, NVM layout, menu model).
- Building a **portable reference implementation** that can be adapted to a new
  platform or integrated into a vendor SDK build.
- **Not** a drop-in replacement firmware for the original hardware.

## 7. Honest status
- **Clean-room source (P0 → P7):** COMPLETE, source-only, self-tests designed.
- **Real-boot readiness:** **NOT READY.** Requires SoC SDK, drivers, hardware init,
  cross-compile, and flash image generation (see §4–§5).
- **No placeholders were added** in this report; the gaps are stated as-is.

---
*Generated as part of the clean-room reverse-engineering deliverable.*
