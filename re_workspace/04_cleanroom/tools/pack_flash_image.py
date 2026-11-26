#!/usr/bin/env python3
"""
pack_flash_image.py — assemble a clean-room flash image for the dish receiver.

Combines the extracted (original) bootloader + kernel with a newly built
clean-room maincode binary into a 4 MiB NCRC flash image suitable for
flashing to the ALi-class MIPS dish receiver.

Usage:
    python3 tools/pack_flash_image.py \
        --bootloader  re_workspace/01_extracted/part_bootloader_raw.bin \
        --kernel      re_workspace/01_extracted/part_kernel_raw.bin \
        --maincode    re_workspace/04_cleanroom/build-mips/cleanroom_mips.bin \
        --out         re_workspace/04_cleanroom/build/flash_image.bin

The bootloader and kernel are reused verbatim from the original firmware
(extracted, proven). Only the maincode partition is replaced by the
clean-room build. Partition table + CRC32 are regenerated per the NCRC
container format recovered in Phase 1.
"""
import argparse
import os
import struct
import zlib

FLASH_SIZE = 4 * 1024 * 1024  # 4 MiB
NCRC_MAGIC = b"NCRC"

# Partition layout recovered in Phase 1 (offset, name, slot size).
PARTITIONS = [
    # (name, offset, slot_size, source_key)
    ("bootloader", 0x000000, 0x003800, "bootloader"),
    ("kernel",     0x003800, 0x01C600, "kernel"),
    ("maincode",   0x022000, 0x2A3000, "maincode"),
]


def crc32_reflected(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def build_ncrc_header(name: bytes, datalen: int, slot: int, version: bytes = b"1.0.0") -> bytes:
    # NCRC container header: magic(4) + name(16) + datalen(4 LE) + slot(4 LE)
    # + version(8) + pad to 0x80. Simplified; matches recovered layout.
    hdr = NCRC_MAGIC + name.ljust(16, b"\x00") + struct.pack("<II", datalen, slot) + version.ljust(8, b"\x00")
    hdr = hdr.ljust(0x80, b"\x00")
    return hdr


def pack(bootloader: bytes, kernel: bytes, maincode: bytes, out_path: str) -> None:
    img = bytearray([0xFF]) * FLASH_SIZE

    sources = {"bootloader": bootloader, "kernel": kernel, "maincode": maincode}

    for name, off, slot, key in PARTITIONS:
        data = sources[key]
        if len(data) > slot:
            raise SystemExit(f"{name}: data {len(data)} > slot {slot}")
        hdr = build_ncrc_header(name.encode(), len(data), slot)
        img[off:off + len(hdr)] = hdr
        img[off + 0x80:off + 0x80 + len(data)] = data
        # trailing CRC over the payload
        c = crc32_reflected(data)
        struct.pack_into("<I", img, off + 0x80 + len(data), c)

    # top-level image CRC (stored at end of image, last 4 bytes)
    struct.pack_into("<I", img, FLASH_SIZE - 4, crc32_reflected(bytes(img[:FLASH_SIZE - 4])))

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(img)
    print(f"Wrote {out_path} ({len(img)} bytes)")
    print(f"  bootloader: {len(bootloader)} B @ 0x000000")
    print(f"  kernel:     {len(kernel)} B @ 0x003800")
    print(f"  maincode:   {len(maincode)} B @ 0x022000")
    print(f"  image CRC32: 0x{crc32_reflected(bytes(img[:FLASH_SIZE-4])):08X}")


def main() -> None:
    ap = argparse.ArgumentParser(description="Pack clean-room flash image")
    ap.add_argument("--bootloader", required=True)
    ap.add_argument("--kernel", required=True)
    ap.add_argument("--maincode", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    with open(args.bootloader, "rb") as f:
        bootloader = f.read()
    with open(args.kernel, "rb") as f:
        kernel = f.read()
    with open(args.maincode, "rb") as f:
        maincode = f.read()

    pack(bootloader, kernel, maincode, args.out)


if __name__ == "__main__":
    main()
