#!/usr/bin/env python3
"""
tooltrace.py — CI tool-chain tracer for the clean-room firmware build.

Every build/test/package step is run *through* this tracer so the resulting
artifact answers three questions:

  1. WHICH tools ran, in which order, and which version of each one?   (TOOLCHAIN.md)
  2. WHAT did each tool consume and produce?                          (ARTIFACT_CHAIN.md)
  3. WHY did a step fail — with the exact stderr?                     (ERRORS.md + steps/*.err)

The trace is plain files under a trace directory, so it survives across jobs
(each job uploads it, the report job merges them) and it is readable even when
the run that produced it is gone.

Sub-commands
------------
    init    --dir DIR --job NAME        start a trace, record runner + tool inventory
    run     --dir DIR --name NAME -- CMD [ARGS...]
                                        run one step, capture stdout/stderr, timing,
                                        exit code, tool version and file hashes
    note    --dir DIR --name NAME --status STATUS --message MSG
                                        record a step that is not a command
                                        (e.g. "MIPS libcrypto unavailable")
    report  --dir DIR [--out OUTDIR]    render TOOLCHAIN.md / ARTIFACT_CHAIN.md /
                                        ERRORS.md / summary.md from the trace
    merge   --dir DIR --from A B C      merge sibling traces into one report

Only the Python standard library is used.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone

TRACE_VERSION = 1
DEFAULT_DIR = os.environ.get("TOOLTRACE_DIR", "ci-out/trace")
MAX_INLINE_LOG_LINES = 400
IN_GHA = os.environ.get("GITHUB_ACTIONS") == "true"


# --------------------------------------------------------------------------- #
# small helpers
# --------------------------------------------------------------------------- #
def now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


def gh(*args: str) -> None:
    """Emit a GitHub Actions workflow command (no-op outside Actions)."""
    if IN_GHA:
        print("::" + " ".join(args), flush=True)


def gha_escape(text: str) -> str:
    return text.replace("%", "%25").replace("\n", "%0A").replace("\r", "%0D")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
    except OSError:
        return ""
    return h.hexdigest()


def file_info(path: str) -> dict:
    """`path` as the caller wrote it, plus the absolute path used as the chain key
    (verification runs in a different cwd than the build in some jobs)."""
    abspath = os.path.abspath(path)
    try:
        st = os.stat(abspath)
    except OSError:
        return {"path": path, "abs": abspath, "exists": False}
    return {
        "path": path,
        "abs": abspath,
        "exists": True,
        "bytes": st.st_size,
        "sha256": sha256_file(abspath),
    }


def split_list(value: str | None) -> list[str]:
    if not value:
        return []
    return [p.strip() for p in value.replace(";", ",").split(",") if p.strip()]


def trace_path(tdir: str) -> str:
    return os.path.join(tdir, "toolchain.jsonl")


def read_records(tdir: str) -> list[dict]:
    path = trace_path(tdir)
    if not os.path.exists(path):
        return []
    out = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except json.JSONDecodeError:
                out.append({"kind": "corrupt", "raw": line[:400]})
    return out


def append_record(tdir: str, rec: dict) -> None:
    os.makedirs(tdir, exist_ok=True)
    with open(trace_path(tdir), "a", encoding="utf-8") as f:
        f.write(json.dumps(rec, sort_keys=False) + "\n")


def next_seq(tdir: str) -> int:
    steps = [r for r in read_records(tdir) if r.get("kind") in ("command", "note")]
    return len(steps) + 1


def tool_version(tool: str) -> str:
    """Best-effort one-line version string for a tool."""
    resolved = shutil.which(tool)
    if not resolved:
        return ""
    for flag in ("--version", "-version", "-V", "version"):
        try:
            p = subprocess.run(
                [resolved, flag],
                capture_output=True, text=True, timeout=20, check=False,
            )
        except (OSError, subprocess.SubprocessError):
            continue
        out = (p.stdout or p.stderr or "").strip()
        if out:
            return out.splitlines()[0].strip()[:200]
    return ""


def runner_inventory() -> dict:
    tools = [
        "bash", "sh", "gcc", "cc", "clang", "make", "cmake", "ld", "objcopy",
        "readelf", "nm", "size", "strip", "python3", "pip3", "sha256sum",
        "openssl", "apt-get", "dpkg", "tar", "gzip", "unzip", "git",
        "mips-linux-gnu-gcc", "mipsel-linux-gnu-gcc", "mips-linux-gnu-objcopy",
        "mipsel-linux-gnu-objcopy", "mipsel-linux-gnu-readelf",
    ]
    inv = {}
    for t in tools:
        p = shutil.which(t)
        if p:
            inv[t] = {"path": p, "version": tool_version(t)}
    return {
        "os": platform.platform(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "cwd": os.getcwd(),
        "user": os.environ.get("USER") or os.environ.get("USERNAME") or "",
        "runner": os.environ.get("RUNNER_OS", ""),
        "tools": inv,
    }


def repo_inventory() -> dict:
    inv = {
        "sha": os.environ.get("GITHUB_SHA", ""),
        "ref": os.environ.get("GITHUB_REF_NAME", ""),
        "run_id": os.environ.get("GITHUB_RUN_ID", ""),
        "run_attempt": os.environ.get("GITHUB_RUN_NUMBER", ""),
        "repository": os.environ.get("GITHUB_REPOSITORY", ""),
        "workflow": os.environ.get("GITHUB_WORKFLOW", ""),
        "job": os.environ.get("GITHUB_JOB", ""),
        "actor": os.environ.get("GITHUB_ACTOR", ""),
        "server_url": os.environ.get("GITHUB_SERVER_URL", ""),
    }
    if not inv["sha"]:
        try:
            inv["sha"] = subprocess.run(
                ["git", "rev-parse", "HEAD"], capture_output=True, text=True,
                timeout=20, check=False,
            ).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            pass
    return inv


# --------------------------------------------------------------------------- #
# sub-commands
# --------------------------------------------------------------------------- #
def cmd_init(args: argparse.Namespace) -> int:
    tdir = args.dir
    os.makedirs(os.path.join(tdir, "steps"), exist_ok=True)
    # A fresh trace per job: keep an existing one only when merging is wanted.
    if os.path.exists(trace_path(tdir)) and not args.append:
        os.remove(trace_path(tdir))
    append_record(tdir, {
        "seq": 0,
        "kind": "meta",
        "trace_version": TRACE_VERSION,
        "job": args.job,
        "recorded_at": now_iso(),
        "repo": repo_inventory(),
        "runner": runner_inventory(),
    })
    print(f"[tooltrace] initialised trace for job '{args.job}' at {tdir}")
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    tdir = args.dir
    argv = args.command
    if not argv:
        print("[tooltrace] run: no command given", file=sys.stderr)
        return 2

    seq = next_seq(tdir)
    safe = "".join(c if (c.isalnum() or c in "-_.") else "_" for c in args.name)[:80]
    stem = os.path.join(tdir, "steps", f"{seq:02d}-{safe}")
    out_log, err_log = stem + ".log", stem + ".err"
    # `--tool` names the tool that matters (e.g. mipsel-linux-gnu-ar) even when the
    # command is wrapped in `bash -c`, so resolve path/version from it first.
    tool = args.tool or os.path.basename(argv[0])
    tool_path = shutil.which(tool) or shutil.which(argv[0]) or ""
    if not tool_path:
        tool = os.path.basename(argv[0])

    consumes = [file_info(p) for p in split_list(args.consumes)]
    pre_produced = {p for p in split_list(args.produces) if os.path.exists(p)}

    print(f"\n[tooltrace] step {seq:02d} '{args.name}' :: {' '.join(argv)}", flush=True)
    gh("group::", f"[{args.job}] {seq:02d} {args.name} — {' '.join(argv)}")

    started = time.monotonic()
    started_at = now_iso()
    with open(out_log, "wb") as fo, open(err_log, "wb") as fe:
        try:
            proc = subprocess.run(
                argv, stdout=fo, stderr=fe, cwd=args.cwd or None,
                env=None, check=False, timeout=args.timeout or None,
                input=None,
            )
            code, timed_out = proc.returncode, False
        except subprocess.TimeoutExpired:
            code, timed_out = 124, True
            fe.write(b"\n[tooltrace] step timed out\n")
        except OSError as exc:
            code, timed_out = 127, False
            fe.write(f"\n[tooltrace] could not execute: {exc}\n".encode())
    duration_ms = int((time.monotonic() - started) * 1000)
    ended_at = now_iso()

    # Echo the captured output so it is also visible in the live Actions log.
    for path, stream in ((out_log, "stdout"), (err_log, "stderr")):
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        if text.strip():
            print(f"--- {stream} ({os.path.basename(path)}) ---")
            sys.stdout.write(text if text.endswith("\n") else text + "\n")

    out_bytes = os.path.getsize(out_log) if os.path.exists(out_log) else 0
    err_bytes = os.path.getsize(err_log) if os.path.exists(err_log) else 0
    produces = [file_info(p) for p in split_list(args.produces)]
    unexpected = [p for p in produces if p["exists"] and p["path"] not in pre_produced]

    if code == 0:
        status = "ok"
    elif args.allow_fail:
        status = "degraded"
    else:
        status = "fail"

    rec = {
        "seq": seq,
        "kind": "command",
        "job": args.job,
        "name": args.name,
        "tool": tool,
        "tool_path": tool_path,
        "tool_version": tool_version(tool) if tool_path else "",
        "invoked_as": argv[0],
        "argv": argv,
        "cwd": args.cwd or os.getcwd(),
        "started_at": started_at,
        "ended_at": ended_at,
        "duration_ms": duration_ms,
        "exit_code": code,
        "status": status,
        "timed_out": timed_out,
        "allow_fail": bool(args.allow_fail),
        "stdout_log": os.path.relpath(out_log, tdir),
        "stderr_log": os.path.relpath(err_log, tdir),
        "stdout_bytes": out_bytes,
        "stderr_bytes": err_bytes,
        "consumes": consumes,
        "produces": produces,
        "created_by_this_step": [p["path"] for p in unexpected],
        "message": args.message or "",
    }
    append_record(tdir, rec)
    gh("endgroup::")

    # lead with the diagnostics; keep the tail as a fallback for non-compiler tools
    digest = _error_digest(err_log) or _error_digest(out_log)
    tail = " | ".join(digest) if digest else (_tail(err_log, 12) or _tail(out_log, 12))
    if status == "fail":
        title = f"{args.job}/{args.name} failed (exit {code})"
        gh(f"error title={gha_escape(title)}::"
           f"{gha_escape(tool)} {gha_escape(' '.join(argv[1:])[:200])}"
           + (f" | {gha_escape(tail)}" if tail else ""))
        print(f"[tooltrace] FAILED  {args.name} (exit {code}, {duration_ms} ms)")
    elif status == "degraded":
        title = f"{args.job}/{args.name} degraded (exit {code}, tolerated)"
        gh(f"warning title={gha_escape(title)}::{gha_escape(tail or args.message or '')}")
        print(f"[tooltrace] DEGRADED {args.name} (exit {code}, {duration_ms} ms) — tolerated")
    else:
        print(f"[tooltrace] ok       {args.name} ({duration_ms} ms)"
              + (f" → {len([p for p in produces if p['exists']])} output(s)" if produces else ""))

    return 0 if status != "fail" else code


def _read_lines(path: str) -> list:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return [l.rstrip() for l in f if l.strip()]
    except OSError:
        return []


# What a human actually needs from a failed build: the diagnostics, not the last
# few lines of whatever happened to be printed. Compiler and linker messages are
# recognised so the annotation leads with the cause instead of trailing context.
_ERROR_RE = re.compile(
    r"(?:error:|fatal error:|undefined reference|multiple definition|"
    r"cannot find|No such file|not declared|implicit declaration|"
    r"\*\*\* \[|Error \d+|ld returned|collect2:)", re.IGNORECASE)


def _error_digest(path: str, budget: int = 4000) -> list:
    """The lines that explain a failure, as a list (callers choose the joiner)."""
    lines = _read_lines(path)
    if not lines:
        return []
    hits, seen = [], set()
    for i, l in enumerate(lines):
        if _ERROR_RE.search(l):
            for cand in lines[max(0, i - 1):i + 2]:   # one line of context either side
                if cand not in seen:
                    seen.add(cand)
                    hits.append(cand)
    picked = hits or lines[-12:]
    out, used = [], 0
    for l in picked:
        if used + len(l) + 1 > budget:
            out.append(f"... [{len(picked) - len(out)} more line(s) in {os.path.basename(path)}] ...")
            break
        out.append(l)
        used += len(l) + 1
    return out


def _tail(path: str, n: int, budget: int = 4000) -> str:
    lines = _read_lines(path)
    if not lines:
        return ""
    out, used = [], 0
    for l in lines[-n:]:
        if used + len(l) + 3 > budget:
            break
        out.append(l)
        used += len(l) + 3
    return " | ".join(out)


def cmd_note(args: argparse.Namespace) -> int:
    tdir = args.dir
    rec = {
        "seq": next_seq(tdir),
        "kind": "note",
        "job": args.job,
        "name": args.name,
        "status": args.status,
        "message": args.message,
        "recorded_at": now_iso(),
        "produces": [file_info(p) for p in split_list(args.produces)],
        "consumes": [file_info(p) for p in split_list(args.consumes)],
    }
    append_record(tdir, rec)
    if args.status == "fail":
        gh(f"error title={gha_escape(args.job + '/' + args.name)}::{gha_escape(args.message)}")
    elif args.status in ("degraded", "warn"):
        gh(f"warning title={gha_escape(args.job + '/' + args.name)}::{gha_escape(args.message)}")
    print(f"[tooltrace] note ({args.status}) {args.name}: {args.message}")
    return 0


# --------------------------------------------------------------------------- #
# rendering
# --------------------------------------------------------------------------- #
def _status_icon(status: str) -> str:
    return {"ok": "PASS", "fail": "FAIL", "degraded": "WARN",
            "warn": "WARN", "skip": "SKIP"}.get(status, status.upper())


def _fmt_bytes(n) -> str:
    if not isinstance(n, int):
        return "-"
    v = float(n)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if v < 1024 or unit == "GiB":
            return f"{v:.0f} {unit}" if unit == "B" else f"{v:.1f} {unit}"
        v /= 1024
    return f"{n} B"


def render_toolchain(records: list[dict], jobs: list[str]) -> str:
    metas = [r for r in records if r.get("kind") == "meta"]
    meta = metas[0] if metas else {}
    steps = [r for r in records if r.get("kind") in ("command", "note")]
    lines = [
        "# Tool chain",
        "",
        "Every tool the pipeline invoked, in the order it was invoked. Versions are",
        "read from the runner at run time (`tool --version`), so this file is the",
        "authority on *what actually built these artifacts*.",
        "",
    ]
    if meta:
        repo = meta.get("repo", {})
        lines.append(f"- Commit: `{repo.get('sha') or 'unknown'}`  "
                     f"ref: `{repo.get('ref') or 'unknown'}`")
        if repo.get("run_id"):
            lines.append(f"- Run: {repo.get('server_url', '')}/{repo.get('repository', '')}"
                         f"/actions/runs/{repo['run_id']}")
        lines.append(f"- Job(s) in this trace: {len(metas)}")
        for m in metas:
            runner = m.get("runner", {})
            lines.append(f"  - `{m.get('job', '?')}` recorded {m.get('recorded_at', '?')} on "
                         f"`{runner.get('os', '?')}` ({runner.get('machine', '?')})")
        lines.append("")

    lines += ["## Ordered invocations", "",
              "| # | Job | Step | Tool | Version | Exit | Status | Time |",
              "|---|-----|------|------|---------|------|--------|------|"]
    for r in steps:
        ver = (r.get("tool_version") or "").replace("|", "/")[:60]
        lines.append(
            f"| {r.get('seq')} | {r.get('job', '')} | `{r.get('name', '')}` | "
            f"`{r.get('tool', '-')}` | {ver or '-'} | {r.get('exit_code', '-')} | "
            f"{_status_icon(r.get('status', ''))} | {r.get('duration_ms', '-')} ms |"
        )

    # Distinct tools, with resolved paths — the "chain" in one glance.
    seen: dict[str, dict] = {}
    for r in steps:
        t = r.get("tool")
        if t and t not in seen:
            seen[t] = r
    lines += ["", "## Distinct tools resolved on the runner", "",
              "| Tool | Path | Version |", "|------|------|---------|"]
    for t, r in sorted(seen.items()):
        lines.append(f"| `{t}` | `{r.get('tool_path') or '-'}` | "
                     f"{(r.get('tool_version') or '-').replace('|', '/')} |")

    inv = {}
    for m in metas:
        for name, info in ((m.get("runner") or {}).get("tools") or {}).items():
            inv.setdefault(name, info)
    if inv:
        lines += ["", "## Full tool inventory probed at job start", "",
                  "| Tool | Path | Version |", "|------|------|---------|"]
        for t, info in sorted(inv.items()):
            lines.append(f"| `{t}` | `{info.get('path')}` | "
                         f"{(info.get('version') or '-').replace('|', '/')} |")

    lines += ["", f"_jobs merged into this report: {', '.join(j.split('/')[-1] for j in jobs) or '-'}_", ""]
    return "\n".join(lines)


def render_chain(records: list[dict]) -> tuple[str, dict]:
    """Build the artifact provenance DAG from consumes/produces edges."""
    steps = [r for r in records if r.get("kind") == "command"]
    nodes: dict[str, dict] = {}
    edges = []

    def node(path: str, info: dict) -> str:
        n = nodes.setdefault(path, {"path": info.get("path", path),
                                    "abs": path, "producers": [], "consumers": []})
        if info.get("exists"):
            n.update({"bytes": info.get("bytes"), "sha256": info.get("sha256"), "exists": True})
        else:
            n.setdefault("exists", False)
        return path

    for r in steps:
        key = lambda i: i.get("abs") or i.get("path")  # noqa: E731
        prod = [node(key(p), p) for p in r.get("produces", []) if p.get("exists")]
        cons = [node(key(c), c) for c in r.get("consumes", []) if c.get("exists")]
        for p in prod:
            nodes[p]["producers"].append(r.get("name"))
        for c in cons:
            nodes[c]["consumers"].append(r.get("name"))
        for c in cons:
            for p in prod:
                edges.append({"from": c, "to": p, "step": r.get("name"),
                              "tool": r.get("tool"), "job": r.get("job")})

    lines = ["# Artifact chain", "",
             "How each artifact was derived: which file went into which tool and",
             "what came out, with SHA-256 at every hop. `verify_artifacts.py`",
             "re-checks these hashes on the finished artifact.", ""]
    if not edges:
        lines += ["_No file-level edges were recorded (steps declared no `--consumes`/`--produces`)._", ""]
    else:
        lines += ["## Derivations", "", "| Step (job) | Tool | Input | → | Output | Output SHA-256 |",
                  "|------------|------|-------|---|--------|----------------|"]
        for e in edges:
            o = nodes.get(e["to"], {})
            sha = (o.get("sha256") or "")[:16]
            lines.append(f"| `{e['step']}` ({e['job']}) | `{e['tool']}` | `{os.path.basename(e['from'])}` "
                         f"| → | `{os.path.basename(e['to'])}` | `{sha}…` |")

    lines += ["", "## Nodes", "", "| File | Size | SHA-256 | Produced by | Consumed by |",
              "|------|------|---------|-------------|-------------|"]
    for path, n in sorted(nodes.items(), key=lambda kv: kv[1].get("path", kv[0])):
        lines.append(
            f"| `{n.get('path', path)}` | {_fmt_bytes(n.get('bytes'))} | `{(n.get('sha256') or '-')[:16]}…` | "
            f"{', '.join(sorted(set(n['producers']))) or '-'} | "
            f"{', '.join(sorted(set(n['consumers']))) or '-'} |")
    lines.append("")
    return "\n".join(lines), {"nodes": nodes, "edges": edges}


def render_errors(records: list[dict], tdir: str) -> str:
    bad = [r for r in records if r.get("status") in ("fail", "degraded", "warn")]
    lines = ["# Error log", "",
             "Every step that did not finish clean, with the stderr that explains it.",
             "This file is uploaded even when the job fails, so the reason is never",
             "locked inside a run that has already been deleted.", ""]
    if not bad:
        lines += ["**No failures or degradations recorded.** Every traced step exited 0.", ""]
        return "\n".join(lines)

    lines += [f"**{len(bad)} step(s) need attention:**", ""]
    for r in bad:
        icon = _status_icon(r.get("status", ""))
        lines.append(f"- `{icon}` **{r.get('job')}/{r.get('name')}** — exit "
                     f"{r.get('exit_code', '-')}: {r.get('message') or r.get('tool', '')}")
    lines.append("")

    for r in bad:
        lines += ["---", "",
                  f"## {r.get('job')}/{r.get('name')} — {_status_icon(r.get('status', ''))}", ""]
        if r.get("kind") == "note":
            lines += [r.get("message", ""), ""]
            continue
        lines += [
            f"- exit code: `{r.get('exit_code')}`",
            f"- command: `{r.get('tool')} {' '.join(r.get('argv', [])[1:])}`".rstrip(),
            f"- tool version: `{r.get('tool_version') or 'unknown'}`",
            f"- tool path: `{r.get('tool_path') or 'unknown'}`",
            f"- duration: {r.get('duration_ms')} ms",
            f"- timed out: {r.get('timed_out')}",
            f"- tolerated (`--allow-fail`): {r.get('allow_fail')}",
            f"- stderr: `{r.get('stderr_log')}` ({r.get('stderr_bytes')} bytes)",
            f"- stdout: `{r.get('stdout_log')}` ({r.get('stdout_bytes')} bytes)",
            "",
        ]
        if r.get("message"):
            lines += [f"> {r['message']}", ""]
        err_path = os.path.join(tdir, r.get("stderr_log", ""))
        digest = _error_digest(err_path, budget=8000)
        if digest:
            lines += ["### What actually failed", "", "```", *digest, "```", ""]
        err_text = _read_log(err_path, MAX_INLINE_LOG_LINES)
        if err_text:
            lines += ["### stderr", "", "```", err_text, "```", ""]
        else:
            lines += ["### stderr", "", "_empty_", ""]
            out_text = _read_log(os.path.join(tdir, r.get("stdout_log", "")), 60)
            if out_text:
                lines += ["### stdout (tail)", "", "```", out_text, "```", ""]
    return "\n".join(lines)


def _read_log(path: str, max_lines: int) -> str:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        return ""
    if len(lines) > max_lines:
        head = lines[: max_lines // 2]
        tail = lines[-(max_lines // 2):]
        lines = head + [f"... [{len(lines) - max_lines} lines truncated] ..."] + tail
    return "\n".join(lines).rstrip()


def render_summary(records: list[dict]) -> str:
    steps = [r for r in records if r.get("kind") in ("command", "note")]
    ok = len([r for r in steps if r.get("status") == "ok"])
    fail = len([r for r in steps if r.get("status") == "fail"])
    warn = len([r for r in steps if r.get("status") in ("degraded", "warn")])
    total_ms = sum(r.get("duration_ms", 0) or 0 for r in steps)
    lines = ["## Clean-room CI trace", "",
             f"- steps: **{len(steps)}** — pass **{ok}**, warn **{warn}**, fail **{fail}**",
             f"- traced wall time: **{total_ms / 1000:.1f} s**", "",
             "| Step | Job | Tool | Exit | Status |", "|------|-----|------|------|--------|"]
    for r in steps:
        lines.append(f"| `{r.get('name')}` | {r.get('job', '')} | `{r.get('tool', '-')}` | "
                     f"{r.get('exit_code', '-')} | {_status_icon(r.get('status', ''))} |")
    lines += ["", "Artifacts: `TOOLCHAIN.md` (tools + versions), `ARTIFACT_CHAIN.md` "
                 "(file provenance + hashes), `ERRORS.md` (stderr of every failure), "
                 "`steps/` (full per-step logs).", ""]
    return "\n".join(lines)


def render_ci_report(records: list[dict], tdir: str, artifacts_root: str | None) -> str:
    """Top-level overview: what ran, what it produced, and where to look next."""
    steps = [r for r in records if r.get("kind") in ("command", "note")]
    jobs: dict[str, dict] = {}
    for r in steps:
        j = jobs.setdefault(r.get("job", "?"), {"ok": 0, "fail": 0, "warn": 0, "ms": 0})
        st = r.get("status")
        if st == "ok":
            j["ok"] += 1
        elif st == "fail":
            j["fail"] += 1
        else:
            j["warn"] += 1
        j["ms"] += r.get("duration_ms", 0) or 0

    meta = next((r for r in records if r.get("kind") == "meta"), {})
    repo = (meta.get("repo") or {})
    lines = ["# Clean-room CI report", ""]
    if repo.get("run_id"):
        lines.append(f"- run: {repo.get('server_url', '')}/{repo.get('repository', '')}"
                     f"/actions/runs/{repo['run_id']} (attempt {repo.get('run_attempt', '?')})")
    lines += [
        f"- commit: `{repo.get('sha') or 'unknown'}` on `{repo.get('ref') or 'unknown'}`",
        f"- generated: {now_iso()}",
        "",
        "## Jobs", "",
        "| Job | Steps | Pass | Warn | Fail | Traced time |",
        "|-----|-------|------|------|------|-------------|",
    ]
    for name, j in sorted(jobs.items()):
        lines.append(f"| `{name}` | {j['ok'] + j['warn'] + j['fail']} | {j['ok']} | "
                     f"{j['warn']} | {j['fail']} | {j['ms'] / 1000:.1f} s |")

    produced = []
    for r in steps:
        for p in r.get("produces", []):
            if p.get("exists"):
                produced.append((r.get("job"), r.get("name"), p))
    lines += ["", "## Files produced", "",
              "| Job | Step | File | Size | SHA-256 |", "|-----|------|------|------|---------|"]
    for job, step, p in produced:
        lines.append(f"| {job} | `{step}` | `{p['path']}` | {_fmt_bytes(p.get('bytes'))} | "
                     f"`{(p.get('sha256') or '')[:16]}…` |")
    if not produced:
        lines.append("| - | - | _none recorded_ | - | - |")

    bad = [r for r in steps if r.get("status") != "ok"]
    lines += ["", "## Outcome", ""]
    if bad:
        lines.append(f"**{len(bad)} step(s) did not finish clean** — see `ERRORS.md` for the "
                     "stderr of each one:")
        lines.append("")
        for r in bad:
            lines.append(f"- `{_status_icon(r.get('status', ''))}` {r.get('job')}/"
                         f"`{r.get('name')}` (exit {r.get('exit_code', '-')}) — "
                         f"{(r.get('message') or _tail(os.path.join(tdir, r.get('stderr_log', '')), 3) or '')[:200]}")
    else:
        lines.append("Every traced step exited 0.")

    lines += ["", "## How to read this bundle", "",
              "| File | What it tells you |", "|------|-------------------|",
              "| `CI_REPORT.md` | this overview |",
              "| `TOOLCHAIN.md` | every tool invoked, in order, with the version that ran |",
              "| `ARTIFACT_CHAIN.md` | which file fed which tool, with a SHA-256 at every hop |",
              "| `ERRORS.md` | stderr of every failed/degraded step, inline |",
              "| `VERIFICATION.md` | independent re-check of the artifacts (CRC32, ELF, hashes) |",
              "| `toolchain.jsonl` | the machine-readable trace (one JSON record per step) |",
              "| `trace-index.json` | step counts + the names of failed/degraded steps |",
              "| `steps/<job>/NN-*.log`, `.err` | full stdout / stderr of each step |", ""]

    if artifacts_root and os.path.isdir(artifacts_root):
        lines += ["## Artifact inventory", "", "| Artifact | File | Size |", "|----------|------|------|"]
        for root, _dirs, files in os.walk(artifacts_root):
            for name in sorted(files):
                p = os.path.join(root, name)
                rel = os.path.relpath(p, artifacts_root)
                try:
                    size = os.path.getsize(p)
                except OSError:
                    continue
                top = rel.split(os.sep)[0]
                lines.append(f"| `{top}` | `{rel}` | {_fmt_bytes(size)} |")
        lines.append("")
    return "\n".join(lines)


def cmd_report(args: argparse.Namespace) -> int:
    tdir = args.dir
    outdir = args.out or tdir
    os.makedirs(outdir, exist_ok=True)
    records = read_records(tdir)
    if not records:
        print(f"[tooltrace] no trace at {tdir}", file=sys.stderr)
        return 1

    jobs = sorted({r.get("job", "?") for r in records if r.get("job")})
    toolchain_md = render_toolchain(records, jobs)
    chain_md, chain_json = render_chain(records)
    errors_md = render_errors(records, tdir)
    summary_md = render_summary(records)

    def write(name: str, text: str) -> str:
        p = os.path.join(outdir, name)
        with open(p, "w", encoding="utf-8") as f:
            f.write(text if text.endswith("\n") else text + "\n")
        return p

    write("CI_REPORT.md", render_ci_report(records, tdir, getattr(args, "artifacts_root", None)))
    write("TOOLCHAIN.md", toolchain_md)
    write("ARTIFACT_CHAIN.md", chain_md)
    write("ERRORS.md", errors_md)
    write("summary.md", summary_md)

    # Ship the raw trace and the per-step logs next to the rendered report, so the
    # bundle is self-contained: ERRORS.md quotes `steps/NN-*.err` and CI_REPORT.md
    # points at `toolchain.jsonl`, and both must actually be in the artifact.
    src_trace = trace_path(tdir)
    dst_trace = os.path.join(outdir, "toolchain.jsonl")
    if os.path.abspath(src_trace) != os.path.abspath(dst_trace) and os.path.exists(src_trace):
        shutil.copy2(src_trace, dst_trace)
    src_steps, dst_steps = os.path.join(tdir, "steps"), os.path.join(outdir, "steps")
    if os.path.isdir(src_steps) and os.path.abspath(src_steps) != os.path.abspath(dst_steps):
        shutil.copytree(src_steps, dst_steps, dirs_exist_ok=True)

    with open(os.path.join(outdir, "artifact-chain.json"), "w", encoding="utf-8") as f:
        json.dump({"generated_at": now_iso(), "jobs": jobs, **chain_json}, f, indent=2)

    # machine-readable index of the whole trace
    steps = [r for r in records if r.get("kind") in ("command", "note")]
    with open(os.path.join(outdir, "trace-index.json"), "w", encoding="utf-8") as f:
        json.dump({
            "trace_version": TRACE_VERSION,
            "generated_at": now_iso(),
            "jobs": jobs,
            "steps": len(steps),
            "failed": [r.get("name") for r in steps if r.get("status") == "fail"],
            "degraded": [r.get("name") for r in steps if r.get("status") in ("degraded", "warn")],
        }, f, indent=2)

    if args.step_summary and os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as f:
            f.write(summary_md + "\n")

    print(f"[tooltrace] report written to {outdir} "
          f"({len(steps)} steps, {len([r for r in steps if r.get('status') == 'fail'])} failed)")
    return 0


def cmd_merge(args: argparse.Namespace) -> int:
    """Copy sibling traces into one directory and render a single report."""
    merged = os.path.join(args.dir, "merged")
    os.makedirs(os.path.join(merged, "steps"), exist_ok=True)
    combined: list[dict] = []
    for src in args.sources:
        src = src.strip()
        if not src or not os.path.isdir(src):
            print(f"[tooltrace] merge: skipping missing {src}", file=sys.stderr)
            continue
        steps_dir = os.path.join(src, "steps")
        job = os.path.basename(src.rstrip("/"))
        if os.path.isdir(steps_dir):
            dst = os.path.join(merged, "steps", job)
            os.makedirs(dst, exist_ok=True)
            for name in sorted(os.listdir(steps_dir)):
                shutil.copy2(os.path.join(steps_dir, name), os.path.join(dst, name))
        for rec in read_records(src):
            if rec.get("kind") == "meta" and rec.get("job"):
                rec["job"] = f"{rec['job']} ({job})"
            for key in ("stdout_log", "stderr_log"):
                if rec.get(key):
                    rec[key] = os.path.join("steps", job, os.path.basename(rec[key]))
            rec["source_trace"] = job
            combined.append(rec)

    if not combined:
        print("[tooltrace] merge: nothing to merge", file=sys.stderr)
        return 1
    combined.sort(key=lambda r: (r.get("job", ""), r.get("seq", 0)))
    os.makedirs(merged, exist_ok=True)
    with open(trace_path(merged), "w", encoding="utf-8") as f:
        for rec in combined:
            f.write(json.dumps(rec) + "\n")
    print(f"[tooltrace] merged {len(combined)} records from {len(args.sources)} trace(s)")
    args.dir, args.out = merged, args.out or merged
    return cmd_report(args)


# --------------------------------------------------------------------------- #
def main() -> int:
    ap = argparse.ArgumentParser(description="CI tool-chain tracer")
    ap.add_argument("--dir", default=DEFAULT_DIR, help="trace directory")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("init", help="start a trace")
    p.add_argument("--job", required=True)
    p.add_argument("--append", action="store_true", help="keep an existing trace file")
    p.set_defaults(func=cmd_init)

    p = sub.add_parser("run", help="run one traced step")
    p.add_argument("--name", required=True)
    p.add_argument("--job", default=os.environ.get("TOOLTRACE_JOB", "job"))
    p.add_argument("--tool", default=None, help="tool name to record (default: argv[0])")
    p.add_argument("--consumes", default=None, help="comma-separated input files")
    p.add_argument("--produces", default=None, help="comma-separated output files")
    p.add_argument("--cwd", default=None)
    p.add_argument("--allow-fail", action="store_true",
                   help="record a non-zero exit as 'degraded' instead of failing the step")
    p.add_argument("--message", default=None)
    p.add_argument("--timeout", type=int, default=0, help="seconds (0 = no limit)")
    p.add_argument("command", nargs=argparse.REMAINDER)
    p.set_defaults(func=cmd_run)

    p = sub.add_parser("note", help="record a non-command event")
    p.add_argument("--name", required=True)
    p.add_argument("--job", default=os.environ.get("TOOLTRACE_JOB", "job"))
    p.add_argument("--status", default="ok", choices=["ok", "warn", "degraded", "fail", "skip"])
    p.add_argument("--message", default="")
    p.add_argument("--consumes", default=None)
    p.add_argument("--produces", default=None)
    p.set_defaults(func=cmd_note)

    p = sub.add_parser("report", help="render TOOLCHAIN/ARTIFACT_CHAIN/ERRORS from a trace")
    p.add_argument("--out", default=None)
    p.add_argument("--artifacts-root", default=None,
                   help="directory of downloaded artifacts to inventory in CI_REPORT.md")
    p.add_argument("--step-summary", action="store_true",
                   help="append the summary to $GITHUB_STEP_SUMMARY")
    p.set_defaults(func=cmd_report)

    p = sub.add_parser("merge", help="merge sibling traces and render one report")
    p.add_argument("--out", default=None)
    p.add_argument("--artifacts-root", default=None)
    p.add_argument("--step-summary", action="store_true")
    p.add_argument("sources", nargs="*")
    p.set_defaults(func=cmd_merge)

    args = ap.parse_args()
    if getattr(args, "cmd", None) == "run" and args.command and args.command[0] == "--":
        args.command = args.command[1:]
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
