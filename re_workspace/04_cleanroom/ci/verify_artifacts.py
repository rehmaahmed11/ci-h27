#!/usr/bin/env python3
"""
verify_artifacts.py — prove the artifacts are what the build claims they are.

Checks run against the finished files (not against the build log), so a broken
artifact cannot slip through just because the step that produced it exited 0:

  * flash image : 4 MiB, NCRC magic per partition, per-partition payload CRC32,
                  whole-image CRC32, maincode payload == the built MIPS binary
  * ELF         : class, endianness, machine (MIPS / x86-64), entry point
  * raw binary  : size fits the recovered maincode slot
  * self-test   : log contains the expected pass line
  * chain       : every SHA-256 recorded by tooltrace.py still matches on disk

Writes VERIFICATION.md (human) + verification.json (machine) and exits non-zero
when a hard check fails.

Usage:
    python3 ci/verify_artifacts.py \
        --flash-image build/flash_image.bin \
        --maincode    build-mips/cleanroom_mips.bin \
        --elf         build-mips/cleanroom_mips.elf --expect-machine mips --expect-entry 0x80000e00 \
        --selftest-log build/selftest.log --expect "ALL TESTS PASSED" \
        --chain       ci-out/trace/artifact-chain.json \
        --out-json    ci-out/report/verification.json \
        --out-md      ci-out/report/VERIFICATION.md
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys
import zlib
from datetime import datetime, timezone

FLASH_SIZE = 4 * 1024 * 1024
NCRC_MAGIC = b"NCRC"
MAINCODE_OFFSET = 0x022000
MAINCODE_SLOT = 0x2A3000
PARTITIONS = [("bootloader", 0x000000, 0x003800),
              ("kernel", 0x003800, 0x01C600),
              ("maincode", 0x022000, MAINCODE_SLOT)]
MACHINES = {"mips": 8, "x86-64": 62, "x86_64": 62, "aarch64": 183, "arm": 40}

RESULTS: list[dict] = []


def _jsonable(value):
    """Make a check value safe for JSON + Markdown."""
    if isinstance(value, (bytes, bytearray)):
        printable = all(32 <= b < 127 for b in value)
        return value.decode("ascii") if printable else "0x" + bytes(value).hex()
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)


def check(cid: str, description: str, expected, actual, ok: bool, severity: str = "hard") -> bool:
    expected, actual = _jsonable(expected), _jsonable(actual)
    RESULTS.append({
        "id": cid, "description": description, "expected": expected, "actual": actual,
        "status": "pass" if ok else ("fail" if severity == "hard" else "warn"),
        "severity": severity,
    })
    mark = "PASS" if ok else ("FAIL" if severity == "hard" else "WARN")
    print(f"[verify] {mark} {cid}: {description} (expected={expected!r} actual={actual!r})")
    return ok


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def file_exists(cid: str, path: str | None, what: str) -> bytes | None:
    if not path:
        return None
    if not os.path.isfile(path):
        check(cid, f"{what} present", "a file", f"missing: {path}", False)
        return None
    with open(path, "rb") as f:
        return f.read()


# --------------------------------------------------------------------------- #
def verify_flash_image(path: str, maincode_path: str | None) -> None:
    img = file_exists("flash.present", path, "flash image")
    if img is None:
        return

    check("flash.size", "flash image is exactly 4 MiB", FLASH_SIZE, len(img),
          len(img) == FLASH_SIZE)
    if len(img) != FLASH_SIZE:
        return

    check("flash.image_crc32", "whole-image CRC32 trailer matches",
          "crc32(image[:-4])", hex(struct.unpack_from("<I", img, FLASH_SIZE - 4)[0]),
          struct.unpack_from("<I", img, FLASH_SIZE - 4)[0] == crc32(img[:FLASH_SIZE - 4]))

    payloads: dict[str, bytes] = {}
    for name, off, slot in PARTITIONS:
        hdr = img[off:off + 0x80]
        magic_ok = hdr[:4] == NCRC_MAGIC
        check(f"flash.{name}.magic", f"{name} partition starts with NCRC magic",
              "NCRC", hdr[:4], magic_ok)
        if not magic_ok:
            continue
        pname = hdr[4:20].split(b"\x00")[0].decode("ascii", "replace")
        datalen, part_slot = struct.unpack_from("<II", hdr, 20)
        check(f"flash.{name}.name", f"{name} header names the partition", name, pname,
              pname == name)
        check(f"flash.{name}.slot", f"{name} header slot matches the recovered map",
              hex(slot), hex(part_slot), part_slot == slot)
        if datalen > slot - 0x84:
            check(f"flash.{name}.datalen", f"{name} payload fits its slot",
                  f"<= {slot - 0x84}", datalen, False)
            continue
        payload = img[off + 0x80: off + 0x80 + datalen]
        payloads[name] = payload
        stored = struct.unpack_from("<I", img, off + 0x80 + datalen)[0]
        check(f"flash.{name}.crc32", f"{name} payload CRC32 matches its header",
              hex(crc32(payload)), hex(stored), stored == crc32(payload))
        empty = payload.count(0xFF) == len(payload)
        check(f"flash.{name}.nonempty", f"{name} payload is not all 0xFF (erased)",
              "some data", f"{datalen} bytes, all-0xFF={empty}", not empty,
              severity="soft")

    if maincode_path and os.path.isfile(maincode_path) and "maincode" in payloads:
        want = sha256_file(maincode_path)
        got = hashlib.sha256(payloads["maincode"]).hexdigest()
        check("chain.maincode", "maincode payload inside the image IS the built binary",
              want[:16] + "…", got[:16] + "…", want == got)


def verify_elf(path: str, machine: str | None, entry: int | None, endian: str | None) -> None:
    data = file_exists("elf.present", path, "ELF")
    if data is None or len(data) < 64:
        return
    check("elf.magic", "file starts with the ELF magic", b"\x7fELF", data[:4],
          data[:4] == b"\x7fELF")
    if data[:4] != b"\x7fELF":
        return
    ei_class = {1: "ELF32", 2: "ELF64"}.get(data[4], f"?{data[4]}")
    ei_data = {1: "little", 2: "big"}.get(data[5], f"?{data[5]}")
    fmt = "<" if data[5] == 1 else ">"
    e_type, e_machine = struct.unpack_from(fmt + "HH", data, 16)
    e_entry = struct.unpack_from(fmt + ("I" if data[4] == 1 else "Q"), data,
                                 24 if data[4] == 1 else 24)[0]
    check("elf.class", "ELF class recorded", "ELF32 or ELF64", ei_class, data[4] in (1, 2),
          severity="soft")
    if endian:
        check("elf.endian", f"ELF is {endian}-endian", endian, ei_data, ei_data == endian)
    if machine:
        want = MACHINES.get(machine.lower())
        check("elf.machine", f"ELF machine is {machine}", want, e_machine,
              want == e_machine)
    if entry is not None:
        check("elf.entry", "ELF entry point matches the linker script",
              hex(entry), hex(e_entry), e_entry == entry)
    types = {1: "REL", 2: "EXEC", 3: "DYN", 4: "CORE"}
    check("elf.type", "ELF type recorded", "EXEC/DYN", types.get(e_type, e_type),
          e_type in (1, 2, 3), severity="soft")


def verify_raw_binary(path: str, max_bytes: int) -> None:
    data = file_exists("bin.present", path, "raw binary")
    if data is None:
        return
    check("bin.nonempty", "raw binary is not empty", "> 0 bytes", len(data), len(data) > 0)
    check("bin.fits_slot", "raw binary fits the recovered maincode slot",
          f"<= {max_bytes} ({hex(max_bytes)})", len(data), len(data) <= max_bytes)


def verify_selftest_log(path: str, expect: str) -> None:
    if not path:
        return
    if not os.path.isfile(path):
        check("selftest.log", "self-test log present", "a file", f"missing: {path}", False)
        return
    text = open(path, "r", encoding="utf-8", errors="replace").read()
    check("selftest.pass", f"self-test log contains '{expect}'", expect,
          "found" if expect in text else "not found", expect in text)
    fails = [l for l in text.splitlines() if l.startswith("FAIL")]
    check("selftest.no_failures", "self-test log has no FAIL lines", 0, len(fails),
          not fails)


def verify_chain(chain_path: str) -> None:
    if not chain_path or not os.path.isfile(chain_path):
        return
    with open(chain_path, "r", encoding="utf-8") as f:
        chain = json.load(f)
    nodes = chain.get("nodes", {})
    checked = mismatched = missing = 0
    for path, node in nodes.items():
        if not node.get("exists"):
            continue
        # Older traces only carry the caller-relative path; prefer the absolute one.
        target = node.get("abs") or path
        if not os.path.isfile(target) and os.path.isfile(path):
            target = path
        if not os.path.isfile(target):
            missing += 1
            continue
        checked += 1
        if sha256_file(target) != node.get("sha256"):
            mismatched += 1
    check("chain.hashes", "every hashed file still matches its recorded SHA-256",
          f"{checked} file(s) unchanged", f"{mismatched} mismatched, {missing} missing",
          mismatched == 0 and missing == 0)
    check("chain.edges", "artifact chain recorded derivations", "> 0 edge",
          len(chain.get("edges", [])), len(chain.get("edges", [])) > 0, severity="soft")


def render_md(args: argparse.Namespace) -> str:
    hard_fail = [r for r in RESULTS if r["status"] == "fail"]
    soft_fail = [r for r in RESULTS if r["status"] == "warn"]
    passed = [r for r in RESULTS if r["status"] == "pass"]
    lines = ["# Artifact verification", "",
             f"- generated: {datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}",
             f"- checks: **{len(RESULTS)}** — pass **{len(passed)}**, "
             f"warn **{len(soft_fail)}**, fail **{len(hard_fail)}**", ""]
    if hard_fail:
        lines += ["## FAILED CHECKS", ""] + [
            f"- `{r['id']}` {r['description']} — expected `{r['expected']}`, got `{r['actual']}`"
            for r in hard_fail] + [""]
    if soft_fail:
        lines += ["## Warnings", ""] + [
            f"- `{r['id']}` {r['description']} — expected `{r['expected']}`, got `{r['actual']}`"
            for r in soft_fail] + [""]
    lines += ["## All checks", "", "| Status | Check | Description | Expected | Actual |",
              "|--------|-------|-------------|----------|--------|"]
    for r in RESULTS:
        lines.append(f"| {r['status'].upper()} | `{r['id']}` | {r['description']} | "
                     f"`{r['expected']}` | `{r['actual']}` |")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--flash-image")
    ap.add_argument("--maincode")
    ap.add_argument("--elf")
    ap.add_argument("--expect-machine", choices=sorted(MACHINES))
    ap.add_argument("--expect-entry", default=None, help="e.g. 0x80000e00")
    ap.add_argument("--expect-endian", choices=["little", "big"])
    ap.add_argument("--binary")
    ap.add_argument("--max-bytes", type=lambda v: int(v, 0), default=MAINCODE_SLOT)
    ap.add_argument("--selftest-log")
    ap.add_argument("--expect", default="ALL TESTS PASSED")
    ap.add_argument("--chain")
    ap.add_argument("--out-json")
    ap.add_argument("--out-md")
    args = ap.parse_args()

    entry = int(args.expect_entry, 0) if args.expect_entry else None

    if args.binary:
        verify_raw_binary(args.binary, args.max_bytes)
    if args.elf:
        verify_elf(args.elf, args.expect_machine, entry, args.expect_endian)
    if args.flash_image:
        verify_flash_image(args.flash_image, args.maincode)
    if args.selftest_log:
        verify_selftest_log(args.selftest_log, args.expect)
    if args.chain:
        verify_chain(args.chain)

    hard_fail = [r for r in RESULTS if r["status"] == "fail"]
    doc = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "total": len(RESULTS),
        "passed": len([r for r in RESULTS if r["status"] == "pass"]),
        "warnings": len([r for r in RESULTS if r["status"] == "warn"]),
        "failed": [r["id"] for r in hard_fail],
        "checks": RESULTS,
    }
    for path, content in ((args.out_json, json.dumps(doc, indent=2)),
                          (args.out_md, render_md(args))):
        if path:
            os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                f.write(content + "\n")
            print(f"[verify] wrote {path}")

    if hard_fail:
        print(f"[verify] {len(hard_fail)} check(s) FAILED: "
              + ", ".join(r["id"] for r in hard_fail), file=sys.stderr)
        return 1
    print(f"[verify] all {len(RESULTS)} checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
