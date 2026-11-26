# CLEAN-ROOM FIRMWARE — FLASH ARRANGEMENTS FOR DISH RECEIVER TESTING

**Source deliverable:** `re_workspace/04_cleanroom/` (clean-room C implementation, P0 → P6, FINAL)
**Target hardware context:** MIPS32 little-endian dish receiver (ALi-class / TurnRound IKS platform)
**Source dump analyzed:** `D:\WORKSHOP\dump202611220912.abs`

---

## 1. Flash Arrangement Overview

The clean-room firmware will eventually be flashed to the dish receiver for testing. The
arrangement follows the recovered flash geometry while remaining source-only:

```
Offset      Size        Region
0x0000000   0x400000    0x400000   Flash image area (4 MiB class, matches analyzed dump)
0x0000000   ...         Bootloader partition (recovered: 0x0 – 0x3800)
0x0003800   ...          Kernel partition     (recovered: 0x3800 – 0x1FE00)
0x0001FE0   ...          HDCPKey partition     (recovered: 0x1FE00 – 0x21000)
0x0002100   ...          commondata partition  (recovered: 0x21000 – 0x22000)
0x0002200   ...          maincode partition   (recovered: 0x22000 – 0x2C5000)
... (full NCRC map recovered in Phase 1; see 03_analysis artifacts)
```

For a clean-room build, the arrangement can either:
- **(A)** Reuse the recovered NCRC partition table + region map (flash via recovered OTA protocol), or
- **(B)** Use a simplified clean-room layout (bootloader / kernel / app container) as agreed.

## 2. Clean-Room Image Build Arrangements

1. Cross-compile clean-room subsystems into a single static MIPS32 LE image:
   - `mips-linux-gnu-gcc -static -o cleanroom_app.elf ...` (standard open toolchain).
2. Entry / base address per subsystem design (clean-room: app base `0x80000000`, kernel base `0x81000000`).
3. No vendor blobs — only clean-room source compiled in.

## 3. Flashing Procedure Arrangements

1. Build clean-room image per §2.
2. Validate CRC32 (reflected, poly `0xEDB88320`, init/xorout `0xFFFFFFFF`) of image + partition table.
3. Flash via:
   - Recovered flash OTA frame protocol (`HEAD` sync, header `[type][flag][idx BE32][subtype][paylen BE32][payCRC BE32]`, payload + CRC32 trailer), or
   - Physical debug/serial interface (as supported by hardware).
4. Post-flash boot validation: boot partition table, verify kernel CRC, launch app.

## 4. Test Arrangement Checklist

- [ ] Clean-room builds without errors (gcc `-std=c11 -Wall -Wextra`).
- [ ] Self-tests pass (`build/cleanroom_p0` → ALL TESTS PASSED).
- [ ] Flash image CRC32 validation passes.
- [ ] Flash to dish receiver.
- [ ] Boot + subsystem smoke tests (CAM, streamer, crypto, transport).

## 5. Current Status

- Clean-room source deliverable: **COMPLETE (P0 → P6, FINAL)**.
- Flash arrangements for dish receiver: **DOCUMENTED HERE (this file)** — binary image generation pending final build tooling.
