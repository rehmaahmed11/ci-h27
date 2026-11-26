#!/usr/bin/env python3
"""
ensure_tree.py — make sure the clean-room source tree is present, then fingerprint it.

The repository ships the clean-room deliverable both as an extracted tree
(`re_workspace/04_cleanroom/`) and as the original upload (`h27.zip`, whose entries
use Windows-style backslash separators).  This script:

  1. uses the extracted tree when it is there;
  2. otherwise extracts `h27.zip` (normalising `\\` to `/`) so CI still works on a
     checkout that only carries the archive;
  3. writes a manifest of every source file (path, size, SHA-256) plus one combined
     `manifest_sha256`.

Step 3 is what makes the artifact chain start at the *inputs*: the build steps
record the manifest as their `--consumes` edge, so `ARTIFACT_CHAIN.md` shows
source inventory -> objects -> binary -> flash image with a hash at every hop.

Usage:
    python3 ci/ensure_tree.py [--cr re_workspace/04_cleanroom] [--zip h27.zip]
                              [--manifest ci-out/tree-manifest.json]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import sys
import zipfile
from datetime import datetime, timezone


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract_zip(zip_path: str, dest: pathlib.Path) -> int:
    count = 0
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            # Windows-created archives use "\" as the separator.
            name = info.filename.replace("\\", "/")
            if name.endswith("/") or not name:
                continue
            target = (dest / name).resolve()
            # refuse anything that would escape the destination (zip-slip)
            if not str(target).startswith(str(dest.resolve())):
                print(f"ERROR: refusing unsafe zip entry {info.filename}", file=sys.stderr)
                return -1
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(zf.read(info))
            count += 1
    return count


def main() -> int:
    here = pathlib.Path(__file__).resolve().parent
    cleanroom_default = here.parent
    repo_root = cleanroom_default.parent.parent

    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cr", default=str(cleanroom_default))
    ap.add_argument("--zip", default=str(repo_root / "h27.zip"))
    ap.add_argument("--manifest", default=str(repo_root / "ci-out" / "tree-manifest.json"))
    ap.add_argument("--strict", action="store_true",
                    help="fail if the tree had to be extracted from the zip")
    args = ap.parse_args()

    cr = pathlib.Path(args.cr).resolve()
    marker = cr / "src" / "main.c"

    source = "repo"
    extracted = 0
    if not marker.is_file():
        zip_path = pathlib.Path(args.zip)
        if not zip_path.is_file():
            print(f"ERROR: no source tree at {cr} and no archive at {zip_path}",
                  file=sys.stderr)
            return 1
        print(f"source tree missing — extracting {zip_path}")
        extracted = extract_zip(str(zip_path), repo_root)
        if extracted < 0:
            return 2
        if not marker.is_file():
            print(f"ERROR: {zip_path} did not contain src/main.c", file=sys.stderr)
            return 1
        source = "zip"

    files = []
    for path in sorted(cr.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(cr).as_posix()
        if rel.startswith("build/") or rel.startswith("build-mips/"):
            continue
        data = path.read_bytes()
        files.append({"path": rel, "bytes": len(data), "sha256": sha256_bytes(data)})

    combined = sha256_bytes("\n".join(f"{f['path']}:{f['sha256']}" for f in files).encode())
    manifest = {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "cleanroom_root": str(cr),
        "tree_source": source,
        "extracted_files": extracted,
        "file_count": len(files),
        "total_bytes": sum(f["bytes"] for f in files),
        "manifest_sha256": combined,
        "files": files,
    }
    out = pathlib.Path(args.manifest)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(manifest, indent=2) + "\n")

    print(f"tree source : {source}" + (f" (extracted {extracted} files)" if extracted else ""))
    print(f"source files: {len(files)} ({manifest['total_bytes']} bytes)")
    print(f"manifest    : {out} sha256={combined[:16]}…")

    if args.strict and source != "repo":
        print("ERROR: --strict requires the tree to be committed, not extracted",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
