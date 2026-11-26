# ci-h27 — clean-room firmware CI

This repository carries the **clean-room firmware re-implementation** for the
MIPS32 little-endian dish receiver (`re_workspace/04_cleanroom/`) and a GitHub
Actions pipeline that builds it, tests it, packs a flash image from it, and
publishes everything as downloadable artifacts — **including the tool chain and
the error logs**, so a failure can be diagnosed from the artifact alone.

```
.
├── .github/workflows/cleanroom-build.yml   the pipeline
├── h27.zip                                 original upload (kept for provenance)
└── re_workspace/04_cleanroom/
    ├── src/            34 translation units (P0 → P7 subsystems)
    ├── tools/          pack_flash_image.py — NCRC 4 MiB flash image packer
    ├── ci/             the CI toolkit (tracer, verifier, part stager, bootstrap)
    ├── Makefile        host build (gcc + libcrypto)
    ├── Makefile.mips   device build (mipsel-linux-gnu, static)
    └── CI_BUILD_FIXES.md   every defect fixed to make the tree build
```

## The pipeline

```
prepare ──┬──> host    ─────────────────────────┐
          └──> device  ──> package ──────────────┴──> report   (if: always())
```

| Job | What it does | Traced tool chain |
|---|---|---|
| `prepare` | uses the committed tree, or extracts `h27.zip`; fingerprints every source file into `tree-manifest.json` (the root of the chain); probes the runner's tools | `ensure_tree.py`, `sha256`, tool inventory |
| `host` | installs gcc/make/libssl-dev, builds `cleanroom_p0`, runs the self-test, records `file`/`size`/`ldd`/SHA-256 facts, verifies the result | `make` → `gcc` → `cleanroom_p0` → `verify_artifacts.py` |
| `device` | installs a MIPS cross toolchain, cross-compiles every translation unit, archives the objects, attempts the static link + `objcopy`, writes a per-unit compile matrix and a build-status report | `make -f Makefile.mips` → `mipsel-linux-gnu-gcc` → `ar` → `objcopy` → `readelf` |
| `package` | takes delivery of the device artifact, stages bootloader/kernel/maincode with recorded provenance, packs the 4 MiB NCRC image, verifies every CRC32 | `prepare_flash_parts.py` → `pack_flash_image.py` → `verify_artifacts.py` |
| `report` | runs even when everything else failed: merges all four traces into one bundle and uploads it | `tooltrace.py merge` |

Jobs hand work to each other **through artifacts** (the source manifest feeds both
builds; the device binary feeds the packer), and every hop is recorded with a
SHA-256, so the chain is auditable end to end.

## Artifacts

| Artifact | Contents |
|---|---|
| `ci-report` | **start here.** `CI_REPORT.md`, `TOOLCHAIN.md`, `ARTIFACT_CHAIN.md`, `ERRORS.md`, all verifications, the merged `toolchain.jsonl` and every per-step log |
| `cleanroom-host` | `cleanroom_p0`, `selftest.log`, `host-binary-info.txt`, host verification, per-job trace reports, `SHA256SUMS.txt` |
| `cleanroom-device` | `libcleanroom_mips.a`, `cleanroom_mips.elf`/`.bin` when the link completes, `compile-matrix.md`, `DEVICE_BUILD_STATUS.md`, `cross-toolchain-install.log`, device verification |
| `flash-image` | `flash_image.bin` (4 MiB), `flash_parts.json`, `FLASH_IMAGE_README.txt`, flash verification |
| `source-manifest` | `tree-manifest.json` — per-file SHA-256 of the whole source tree |
| `trace-prepare` / `trace-host` / `trace-device` / `trace-package` | each job's own trace, uploaded with `if: always()` so a single failing job is still inspectable |

### Reading a failure

Every step runs through `ci/tooltrace.py`, which captures stdout and stderr
separately and records the tool that ran:

1. **`ERRORS.md`** — every step that did not exit 0, with its stderr inline, the
   exact command line, the tool version, the exit code and the duration. Failed
   steps are `FAIL`; tolerated ones (`--allow-fail`) are `WARN` and say why they
   were tolerated.
2. **`TOOLCHAIN.md`** — the ordered list of tool invocations with the version each
   one reported, plus the full tool inventory probed when the job started.
3. **`ARTIFACT_CHAIN.md`** — which file fed which tool and what came out, with a
   SHA-256 at every hop (`tree-manifest.json` → objects → ELF → BIN → flash image).
4. **`VERIFICATION.md`** — checks that do not trust the build log: image size,
   NCRC magic, per-partition CRC32, whole-image CRC32, ELF machine/endian/entry
   point, slot fit, and that the maincode inside the image *is* the built binary.
5. **`steps/<job>/NN-*.log` / `.err`** — the complete raw output of each step.

The same information is pushed into the run as GitHub annotations (`::error::`,
`::warning::`) and into the job summary, so it is visible without downloading
anything.

## The device (MIPS) link, honestly

The device link is `-static … -lssl -lcrypto`. **Ubuntu publishes no `libssl-dev`
for mips/mipsel**, so on a stock GitHub-hosted runner the final link cannot resolve
the OpenSSL libraries for the target, and the cross sysroot has no OpenSSL headers
either. That is why the five units including `<openssl/*.h>` (`cam_keys`,
`crypto_tls`, `crypto_ctr`, `http_client`, `runtime_e2e`) do not compile there; the
other **28 do**, and they are archived into a real MIPS `libcleanroom_mips.a`.

Those five are deliberately **not** compiled against the host x86_64 OpenSSL
headers. Debian and Ubuntu install `opensslconf.h` and `configuration.h` only under
`/usr/include/x86_64-linux-gnu/openssl/`, and both are build-specific: `BN_ULONG`
width, word size and endianness come from the machine OpenSSL was built for. MIPS
o32 objects compiled against them would silently disagree with a real MIPS
libcrypto at link time — the worst possible failure mode for firmware. The pipeline
records that reasoning as a trace note instead of guessing.

**Runner image.** `ubuntu-latest` is now ubuntu-26.04, and Ubuntu dropped mipsel
when Debian dropped it for trixie — there, `apt` reports *"Package
'gcc-mipsel-linux-gnu' has no installation candidate"* and
`binutils-mipsel-linux-gnu` / `libc6-dev-mipsel-cross` are gone entirely, so no
amount of enabling `universe` helps. The `device` job is therefore **pinned to
`ubuntu-24.04`**, which still ships the whole cross toolchain; the other four jobs
stay on `ubuntu-latest` because they need nothing architecture-specific.

Rather than fail silently or fake it, the pipeline:

* cross-compiles everything it can (`make -k`) and records the attempt;
* publishes `compile-matrix.md` naming each translation unit that did not compile;
* archives the objects it did produce (`libcleanroom_mips.a`) as a real MIPS artifact;
* records the link failure as `WARN` with the exact linker stderr in `ERRORS.md`;
* treats "no MIPS toolchain could be installed" as a loud **warning** (an
  environment gap this commit cannot fix) but "toolchain present and zero units
  compiled" as a hard **failure** — `device_link=required` upgrades the former too;
* writes `DEVICE_BUILD_STATUS.md` explaining the cause and the fix;
* stages a 0xFF placeholder maincode, packs the image anyway (so the packer and
  verifier stay under test), and marks it **`flashable: false`** in
  `flash_parts.json` and `FLASH_IMAGE_README.txt`.

To get a genuinely flashable image, provide a MIPS sysroot containing `libssl.a`,
`libcrypto.a` and the OpenSSL headers (vendor SDK, or a self-hosted runner), then re-run with
`device_link=required` so a missing binary fails the run instead of warning. See
`re_workspace/04_cleanroom/BOOT_READINESS_GAP_REPORT.md`.

## Running it locally

```sh
cd re_workspace/04_cleanroom

make clean && make          # host build -> build/cleanroom_p0
./build/cleanroom_p0        # -> cleanroom: ALL TESTS PASSED
sh ci/build.sh              # the same thing as one script

# tool chain + error logs, without GitHub:
python3 ci/tooltrace.py --dir /tmp/trace init --job local
python3 ci/tooltrace.py --dir /tmp/trace run --name build --cwd . \
        --consumes /tmp/tree-manifest.json --produces build/cleanroom_p0 \
        -- make clean all
python3 ci/tooltrace.py --dir /tmp/trace report --out /tmp/report
```

The device build needs `gcc-mipsel-linux-gnu`:

```sh
make -f Makefile.mips objects CROSS=mipsel-linux-gnu-   # compile only
make -f Makefile.mips archive CROSS=mipsel-linux-gnu-   # libcleanroom_mips.a
make -f Makefile.mips        CROSS=mipsel-linux-gnu-    # ELF + BIN (needs MIPS libcrypto)
```

## State of the source tree

The tree that arrived in `h27.zip` did not compile — 30 of 34 translation units
failed. It now builds with **0 errors and 0 warnings** under
`-std=c11 -Wall -Wextra` and its self-test passes. Every defect and the reason for
each fix is recorded in
[`re_workspace/04_cleanroom/CI_BUILD_FIXES.md`](re_workspace/04_cleanroom/CI_BUILD_FIXES.md).

`h27.zip` is kept as the original upload; the extracted tree beside it is what CI
builds. `ci/ensure_tree.py` will re-extract the archive automatically if the tree
is ever missing, and normalises the archive's Windows-style `\` separators while
doing so.
