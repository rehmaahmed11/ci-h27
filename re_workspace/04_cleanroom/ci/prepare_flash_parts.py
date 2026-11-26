#!/usr/bin/env python3
"""
prepare_flash_parts.py — stage the three flash partitions for packing, reproducibly.

`tools/pack_flash_image.py` needs three inputs: the *original* bootloader, the
*original* kernel and the freshly built clean-room maincode.  The two original
parts live in `re_workspace/01_extracted/`, which is deliberately **not** part of
this repository (no vendor binary artifacts are committed), and the maincode only
exists when the MIPS link succeeded.

This script resolves each partition from wherever it is available and, when it is
not, produces a clearly-labelled 0xFF (erased-flash) placeholder so the packer and
its verifier can still be exercised end to end.  The provenance of every part is
written to `flash_parts.json`, and the resulting image is marked
`flashable: false` whenever any part is synthetic — a placeholder image must never
be mistaken for something that can go on a receiver.

Usage:
    python3 ci/prepare_flash_parts.py --out-dir build/flash_parts \
        --maincode build-mips/cleanroom_mips.bin
    python3 ci/prepare_flash_parts.py --out-dir build/flash_parts \
        --search ../../01_extracted --search /opt/firmware/parts
    python3 ci/prepare_flash_parts.py --out-dir build/flash_parts --require-extracted
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
from datetime import datetime, timezone

# Partition slots recovered in Phase 1 (see FLASH_ARRANGEMENTS.md and
# tools/pack_flash_image.py).  Usable payload = slot - 0x80 header - 4 CRC.
PARTITIONS = {
    "bootloader": {
        "slot": 0x003800,
        "candidates": ["part_bootloader_raw.bin", "bootloader.bin", "part_bootloader.bin"],
        "placeholder": 0x001000,
    },
    "kernel": {
        "slot": 0x01C600,
        "candidates": ["part_kernel_raw.bin", "kernel.bin", "part_kernel.bin"],
        "placeholder": 0x010000,
    },
}

MAINCODE = {"slot": 0x2A3000, "placeholder": 0x001000}

NCRC_PAYLOAD_OVERHEAD = 0x80 + 4


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_part(name: str, search_dirs: list[str]) -> str | None:
    spec = PARTITIONS[name]
    for d in search_dirs:
        if not d or not os.path.isdir(d):
            continue
        for cand in spec["candidates"]:
            p = os.path.join(d, cand)
            if os.path.isfile(p) and os.path.getsize(p) > 0:
                return p
    return None


def _placeholder(dst: str, size: int) -> None:
    with open(dst, "wb") as f:
        f.write(b"\xFF" * size)


def _stage(report: dict, name: str, src: str | None, dst: str, slot: int,
           placeholder_size: int, provenance: str, note_when_synthetic: str,
           require: bool, searched: list[str]) -> None:
    """Copy `src` to `dst`, or synthesise an erased-flash placeholder."""
    max_payload = slot - NCRC_PAYLOAD_OVERHEAD

    if src is None:
        if require:
            print(f"ERROR: {name} partition not found (searched: {searched})",
                  file=sys.stderr)
            sys.exit(2)
        size = min(placeholder_size, max_payload)
        _placeholder(dst, size)
        report["parts"][name] = {
            "path": dst, "provenance": "synthetic", "bytes": size,
            "sha256": sha256_file(dst), "slot": slot, "max_payload": max_payload,
            "note": note_when_synthetic.format(size=size, allowed=max_payload),
        }
        report["synthetic_parts"].append(name)
        report["flashable"] = False
        print(f"WARNING: {name}: synthetic 0xFF placeholder "
              f"({size} of {max_payload} allowed bytes)", file=sys.stderr)
        return

    size = os.path.getsize(src)
    if size > max_payload:
        print(f"ERROR: {name}: {src} is {size} bytes but the slot only holds "
              f"{max_payload} bytes of payload", file=sys.stderr)
        sys.exit(2)
    if size == 0:
        print(f"ERROR: {name}: {src} is empty", file=sys.stderr)
        sys.exit(2)
    shutil.copyfile(src, dst)
    report["parts"][name] = {
        "path": dst, "provenance": provenance, "source": os.path.abspath(src),
        "bytes": size, "sha256": sha256_file(dst), "slot": slot,
        "max_payload": max_payload,
    }
    print(f"ok: {name}: {src} ({size} bytes, {provenance})")


def resolve(args: argparse.Namespace) -> dict:
    search = [os.path.abspath(d) for d in args.search if d]
    outdir = os.path.abspath(args.out_dir)
    os.makedirs(outdir, exist_ok=True)

    report: dict = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "searched": search,
        "out_dir": outdir,
        "parts": {},
        "synthetic_parts": [],
        "flashable": True,
        "notes": [],
    }

    for name, spec in PARTITIONS.items():
        _stage(report, name, find_part(name, search),
               os.path.join(outdir, f"part_{name}_raw.bin"), spec["slot"],
               spec["placeholder"], "extracted",
               f"0xFF erased-flash placeholder ({{size}} of {{allowed}} bytes); the "
               f"extracted {name} partition is not committed to this repository",
               args.require_extracted, search)

    maincode = args.maincode if (args.maincode and os.path.isfile(args.maincode)) else None
    _stage(report, "maincode", maincode,
           os.path.join(outdir, "part_maincode_raw.bin"), MAINCODE["slot"],
           MAINCODE["placeholder"], "build",
           "0xFF erased-flash placeholder ({size} of {allowed} bytes); no device "
           "binary was available from the build (the MIPS link did not produce one)",
           False, search)
    if args.maincode and maincode is None:
        report["notes"].append(f"--maincode {args.maincode} does not exist; "
                               f"a placeholder was staged instead")

    if not report["flashable"]:
        report["notes"].append(
            "Image built with synthetic partition(s): "
            + ", ".join(report["synthetic_parts"])
            + ". It exercises the packer and verifier only and MUST NOT be flashed "
              "to a receiver."
        )
    return report


def render_readme(report: dict) -> str:
    lines = [
        "FLASH IMAGE — READ BEFORE FLASHING",
        "==================================",
        "",
        f"flashable     : {str(report['flashable']).lower()}",
        f"staged at     : {report['generated_at']}",
        "",
        "partition provenance",
        "--------------------",
    ]
    for name, p in report["parts"].items():
        lines.append(f"  {name:<11} {p['provenance']:<10} {p['bytes']:>9} bytes  "
                     f"slot 0x{p['slot']:X}  sha256={p['sha256'][:16]}…")
        if p.get("note"):
            lines.append(f"              ^ {p['note']}")
    if report["notes"]:
        lines += ["", "notes", "-----"]
        lines += [f"  * {n}" for n in report["notes"]]
    if not report["flashable"]:
        lines += ["", "*** DO NOT FLASH THIS IMAGE ***",
                  "It was assembled with synthetic (0xFF erased-flash) placeholder",
                  "partition(s) so that the packer and verifier can be exercised in CI.",
                  "A flashable image needs the extracted bootloader + kernel from",
                  "re_workspace/01_extracted/ and a real maincode from a completed",
                  "MIPS static link (see BOOT_READINESS_GAP_REPORT.md)."]
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    cleanroom = os.path.dirname(here)
    default_search = [
        os.path.join(cleanroom, "..", "01_extracted"),
        os.path.join(cleanroom, "build", "extracted_parts"),
        os.environ.get("CLEANROOM_FLASH_PARTS_DIR", ""),
    ]

    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out-dir", default=os.path.join(cleanroom, "build", "flash_parts"))
    ap.add_argument("--maincode", default=None,
                    help="device binary to stage as the maincode partition "
                         "(e.g. build-mips/cleanroom_mips.bin); a placeholder is "
                         "staged when it is absent")
    ap.add_argument("--search", action="append", default=None,
                    help="directory to look in (repeatable); defaults include "
                         "re_workspace/01_extracted and $CLEANROOM_FLASH_PARTS_DIR")
    ap.add_argument("--require-extracted", action="store_true",
                    help="fail instead of synthesising a placeholder")
    ap.add_argument("--report", default=None,
                    help="where to write flash_parts.json (default: <out-dir>/flash_parts.json)")
    ap.add_argument("--readme", default=None,
                    help="also write a human-readable FLASH_IMAGE_README.txt here")
    args = ap.parse_args()
    if args.search is None:
        args.search = default_search

    report = resolve(args)
    out = args.report or os.path.join(os.path.abspath(args.out_dir), "flash_parts.json")
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
    print(f"wrote {out} (flashable={report['flashable']})")

    if args.readme:
        os.makedirs(os.path.dirname(os.path.abspath(args.readme)) or ".", exist_ok=True)
        with open(args.readme, "w", encoding="utf-8") as f:
            f.write(render_readme(report))
        print(f"wrote {args.readme}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
