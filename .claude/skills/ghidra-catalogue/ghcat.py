#!/usr/bin/env python3
"""Ghidra function catalogue: look a function up once, keep it for every agent.

One Markdown file per function under the catalogue root, keyed by address. Each file keeps
the raw disassembly, Ghidra's decompile, callers, and the analysis agents add over time.

  ghcat.py get <addr|name> [--refresh] [--sections meta,asm,c,analysis,comments,callers,docs] [--max-lines N]
  ghcat.py find <regex> [--status raw|analysed] [--limit N]
  ghcat.py note <addr> [--summary S] [--tags a,b] [--analysis-file F | --analysis S] [--comment S] [--by LANE]
  ghcat.py xrefs <data addr> [--write] [--name N] [--summary S] [--comment S] [--by LANE]
  ghcat.py stats

`get` prints CACHED or FETCHED on its first line, then one GHCAT line (address, name, status,
summary, path) that memory tools can index. Only `get --refresh` and a first `get` call Ghidra.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

def _openblack_config(dotted: str, default: str = "") -> str:
    """One value of the developer's openblack skill settings (.claude/openblack.local.json), or the default."""
    reader = Path(__file__).resolve().parents[2] / "obconfig.py"
    try:
        out = subprocess.run([sys.executable, str(reader), "get", dotted], capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return default
    return out.stdout.strip() or default


ROOT = Path(os.environ.get("GHCAT_ROOT") or _openblack_config("paths.ghidraCatalogue", "ghidra-catalogue"))
BINARY = os.environ.get("GHCAT_BINARY") or _openblack_config("tools.ghidraBinary", "BW1W120")
BRIDGE = os.environ.get("GHCAT_BRIDGE") or _openblack_config("tools.ghidraBridge", "http://127.0.0.1:8080")
DOC_DIRS = [Path(p) for p in (os.environ.get("GHCAT_DOCS") or ";".join(
    filter(None, [_openblack_config("paths.bw1DecompDocs"), _openblack_config("paths.handovers")]))).split(";") if p]
SECTIONS = ["Analysis", "Comments", "Disassembly", "Decompiled", "Callers", "Log"]


def fn_dir() -> Path:
    d = ROOT / BINARY / "fn"
    d.mkdir(parents=True, exist_ok=True)
    return d


def norm_addr(text: str) -> str | None:
    m = re.fullmatch(r"(?:0x)?([0-9a-fA-F]{1,8})", text.strip())
    return m.group(1).upper().zfill(8) if m else None


def bridge(path: str, **params) -> str:
    url = f"{BRIDGE}/{path}?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read().decode("utf-8", "replace")


# ---- file format -------------------------------------------------------------------------

def parse(path: Path) -> tuple[dict, dict]:
    text = path.read_text(encoding="utf-8")
    meta, body = {}, text
    if text.startswith("---\n"):
        end = text.index("\n---\n", 4)
        for line in text[4:end].splitlines():
            if ": " in line:
                k, v = line.split(": ", 1)
                meta[k.strip()] = json.loads(v) if v.startswith(("[", '"')) else v.strip()
        body = text[end + 5:]
    sections, cur = {}, None
    for line in body.splitlines():
        m = re.fullmatch(r"## (\w+)", line)
        if m and m.group(1) in SECTIONS:
            cur = m.group(1)
            sections[cur] = []
        elif cur:
            sections[cur].append(line)
    return meta, {k: "\n".join(v).strip("\n") for k, v in sections.items()}


DATA_SECTIONS = ["Analysis", "Comments", "Callers", "Log"]


def write(path: Path, meta: dict, sections: dict, order: list | None = None) -> None:
    out = ["---"]
    for k, v in meta.items():
        out.append(f"{k}: {json.dumps(v)}")
    out.append("---")
    for s in (order or SECTIONS):
        out += ["", f"## {s}", sections.get(s, "").rstrip()]
    tmp = path.with_suffix(".tmp")
    tmp.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    tmp.replace(path)


TRAILER = os.environ.get("GHCAT_TRAILER") or _openblack_config(
    "commit.trailer", "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")


def subject_line(verb: str, meta: dict) -> str:
    # STE: one instruction, active voice, no full stop, at most 72 characters.
    tail = f" at 0x{meta['address']}"
    name = meta.get("name", "")
    room = 72 - len(verb) - len(tail) - 1
    if len(name) > room:
        name = name[: max(room - 3, 0)] + "..."
    return f"{verb} {name}{tail}".replace("  ", " ")


def commit(path: Path, subject: str, by: str) -> None:
    """Commit one catalogue file. Every change goes into the history at once."""
    root = ROOT
    if not (root / ".git").exists():
        sys.exit(f"{root} is not a git repository; run `git init` there first")
    rel = str(path.relative_to(root)).replace("\\", "/")
    body = f"Lane: {by}." if by else ""
    msg = subject + ("\n\n" + body if body else "") + "\n\n" + TRAILER + "\n"
    for attempt in range(20):
        add = subprocess.run(["git", "-C", str(root), "add", "--", rel], capture_output=True, text=True,
                             stdin=subprocess.DEVNULL)
        if add.returncode == 0:
            r = subprocess.run(["git", "-C", str(root), "commit", "-q", "-F", "-", "--", rel], input=msg,
                               capture_output=True, text=True)
            if r.returncode == 0:
                return
            if "nothing to commit" in (r.stdout + r.stderr) or "no changes added" in (r.stdout + r.stderr):
                return
            err = r.stderr
        else:
            err = add.stderr
        if "index.lock" not in err:
            sys.exit(f"git commit failed for {rel}:\n{err.strip()}")
        time.sleep(0.5 + attempt * 0.25)  # another agent is committing; wait and try again
    sys.exit(f"git commit failed for {rel}: the index stayed locked")


def ghcat_line(meta: dict, path: Path) -> str:
    return (f"GHCAT {BINARY} 0x{meta['address']} {meta.get('name', '?')} [{meta.get('status', 'raw')}] "
            f"{meta.get('summary', '') or '(no summary yet)'} -> {path}")


# ---- fetch -------------------------------------------------------------------------------

def resolve(query: str) -> str:
    addr = norm_addr(query)
    if addr:
        return addr
    # A name: look in the catalogue first, then ask Ghidra.
    for p in fn_dir().glob("*.md"):
        meta, _ = parse(p)
        if query in (meta.get("name"), meta.get("short_name")):
            return meta["address"]
    hits = [l for l in bridge("searchFunctions", query=query, limit=50).splitlines() if " @ " in l]
    exact = [l for l in hits if l.split(" @ ")[0].strip() == query.split("::")[-1]]
    pick = exact or hits
    if len(pick) != 1:
        sys.exit(f"name '{query}' gives {len(pick)} matches; pass an address:\n" + "\n".join(pick[:20]))
    return pick[0].split(" @ ")[1].strip().upper().zfill(8)


def fetch(addr: str) -> tuple[dict, dict]:
    info = bridge("get_function_by_address", address=f"0x{addr}")
    if "Function:" not in info:
        sys.exit(f"Ghidra has no function at 0x{addr}: {info.strip()[:200]}")
    decomp = bridge("decompile_function", address=f"0x{addr}")
    disasm = bridge("disassemble_function", address=f"0x{addr}")
    callers = bridge("xrefs_to", address=f"0x{addr}", limit=200)
    short = re.search(r"Function: (\S+)", info).group(1)
    sig = (re.search(r"Signature: (.+)", info) or [None, ""])[1].strip()
    body = re.search(r"Body: ([0-9a-fA-F]+) - ([0-9a-fA-F]+)", info)
    full = re.search(r"/\* (?:public|private|protected)?:? ?(?:virtual |static )?[^*]*?(\w[\w:<>~]*)\(", decomp)
    meta = {
        "address": addr,
        "binary": BINARY,
        "name": full.group(1) if full and short in full.group(1) else short,
        "short_name": short,
        "signature": sig,
        "size": hex(int(body.group(2), 16) - int(body.group(1), 16) + 1) if body else "",
        "fetched": dt.date.today().isoformat(),
        "status": "raw",
        "summary": "",
        "tags": [],
    }
    decomp_lines = [l for l in decomp.splitlines() if not l.startswith("/* WARNING: Enum")]
    sections = {
        "Analysis": "",
        "Comments": "",
        "Disassembly": "```asm\n" + disasm.strip() + "\n```",
        "Decompiled": "```c\n" + "\n".join(decomp_lines).strip() + "\n```",
        "Callers": "```\n" + callers.strip() + "\n```",
        "Log": f"- {meta['fetched']} fetched from Ghidra",
    }
    return meta, sections


def doc_mentions(addr: str, limit: int = 15) -> list[str]:
    short = addr.lstrip("0") or "0"
    pat = rf"0x0*{short}\b"
    out = []
    for d in DOC_DIRS:
        if not d.exists():
            continue
        try:
            r = subprocess.run(["ag", "-il", "--md", pat, str(d)], capture_output=True, text=True,
                               stdin=subprocess.DEVNULL, timeout=60)
            out += [l.strip() for l in r.stdout.splitlines() if l.strip()]
        except (OSError, subprocess.TimeoutExpired):
            pass
    return out[:limit]


# ---- commands ----------------------------------------------------------------------------

def cmd_get(a) -> None:
    addr = resolve(a.target)
    path = fn_dir() / f"{addr}.md"
    state = "CACHED"
    if a.refresh or not path.exists():
        meta, sections = fetch(addr)
        if path.exists():  # keep what agents wrote; replace only Ghidra's parts
            old_meta, old = parse(path)
            for k in ("summary", "tags", "status"):
                meta[k] = old_meta.get(k, meta[k])
            for s in ("Analysis", "Comments"):
                sections[s] = old.get(s, "")
            sections["Log"] = (old.get("Log", "") + f"\n- {meta['fetched']} refreshed from Ghidra").strip()
            state = "REFRESHED"
        else:
            state = "FETCHED"
        write(path, meta, sections)
        verb = "Replace the Ghidra parts of" if state == "REFRESHED" else "Add"
        commit(path, subject_line(verb, meta), a.by or "")
    meta, sections = parse(path)
    print(f"{state} {path}")
    print(ghcat_line(meta, path))
    want = a.sections.split(",")
    if "meta" in want:
        print(f"signature: {meta.get('signature', '')}\nsize: {meta.get('size', '')}  fetched: {meta.get('fetched', '')}"
              f"  tags: {', '.join(meta.get('tags', []) or [])}")
    for key, name in (("analysis", "Analysis"), ("comments", "Comments"), ("callers", "Callers"),
                      ("asm", "Disassembly"), ("c", "Decompiled")):
        if key in want and sections.get(name):
            lines = sections[name].splitlines()
            print(f"\n## {name}")
            print("\n".join(lines[:a.max_lines]))
            if len(lines) > a.max_lines:
                print(f"... {len(lines) - a.max_lines} more lines in {path}")
    if "docs" in want:
        docs = doc_mentions(addr)
        if docs:
            print("\n## Mentioned in notes\n" + "\n".join(docs))


def cmd_find(a) -> None:
    rx = re.compile(a.pattern, re.I)
    n = 0
    for p in sorted(fn_dir().glob("*.md")):
        meta, sections = parse(p)
        if a.status and meta.get("status") != a.status:
            continue
        hay = " ".join([meta.get("name", ""), meta.get("summary", ""), " ".join(meta.get("tags", []) or []),
                        sections.get("Analysis", ""), sections.get("Comments", "")])
        if rx.search(hay) or rx.search(meta["address"]):
            print(ghcat_line(meta, p))
            n += 1
            if n >= a.limit:
                break
    if not n:
        print("no catalogue entry matches; use `get` on an address or name to fetch one")


def cmd_note(a) -> None:
    addr = norm_addr(a.address) or sys.exit("note needs an address")
    path = fn_dir() / f"{addr}.md"
    if not path.exists():
        sys.exit(f"no entry for 0x{addr}; run `get` first")
    meta, sections = parse(path)
    today = dt.date.today().isoformat()
    by = a.by or "agent"
    changed = []
    if a.summary:
        meta["summary"] = a.summary
        changed.append("summary")
    if a.tags:
        meta["tags"] = sorted(set((meta.get("tags") or []) + [t.strip() for t in a.tags.split(",") if t.strip()]))
        changed.append("tags")
    text = Path(a.analysis_file).read_text(encoding="utf-8") if a.analysis_file else a.analysis
    if text:
        sections["Analysis"] = (sections.get("Analysis", "") + f"\n\n### {today} ({by})\n" + text.strip()).strip()
        meta["status"] = "analysed"
        changed.append("analysis")
    if a.comment:
        sections["Comments"] = (sections.get("Comments", "") + f"\n- {today} ({by}): {a.comment.strip()}").strip()
        changed.append("comment")
    if not changed:
        sys.exit("nothing to add")
    sections["Log"] = (sections.get("Log", "") + f"\n- {today} {by}: {', '.join(changed)}").strip()
    write(path, meta, sections)
    verb = ("Add analysis of" if "analysis" in changed else "Change the summary of" if "summary" in changed
            else "Add a comment to" if "comment" in changed else "Change the tags of")
    commit(path, subject_line(verb, meta), by)
    print(ghcat_line(meta, path))


def cmd_xrefs(a) -> None:
    """List the functions that read or write a data address; --write keeps the list in data/<ADDR>.md."""
    addr = norm_addr(a.address) or sys.exit("xrefs needs an address")
    raw = bridge("xrefs_to", address=f"0x{addr}", limit=a.limit)
    rows = []
    for line in raw.splitlines():
        m = re.match(r"From ([0-9a-fA-F]+)(?: in (\S+))? \[(\w+)\]", line.strip())
        if m:
            rows.append((m.group(1).upper().zfill(8), m.group(2) or "-", m.group(3)))
    if not rows:
        sys.exit(f"no references to 0x{addr}: {raw.strip()[:200]}")
    rows.sort()
    print(f"XREFS {BINARY} 0x{addr}: {len(rows)} references")
    for at, fn, kind in rows:
        print(f"  {at}  {kind:<5}  {fn}")
    if not a.write:
        return
    d = ROOT / BINARY / "data"
    d.mkdir(parents=True, exist_ok=True)
    path = d / f"{addr}.md"
    today = dt.date.today().isoformat()
    by = a.by or "agent"
    body = "```\n" + "\n".join(f"{at}  {kind:<5}  {fn}" for at, fn, kind in rows) + "\n```"
    if path.exists():
        meta, sections = parse(path)
        sections["Log"] = (sections.get("Log", "") + f"\n- {today} {by}: xrefs refreshed").strip()
    else:
        meta = {"address": addr, "binary": BINARY, "name": a.name or f"data_{addr}", "kind": "data",
                "fetched": today, "status": "raw", "summary": "", "tags": []}
        sections = {"Log": f"- {today} {by}: xrefs fetched"}
    meta["fetched"] = today
    if a.name:
        meta["name"] = a.name
    if a.summary:
        meta["summary"] = a.summary
    sections["Callers"] = body
    if a.comment:
        sections["Comments"] = (sections.get("Comments", "") + f"\n- {today} ({by}): {a.comment.strip()}").strip()
        meta["status"] = "analysed"
    write(path, meta, sections, DATA_SECTIONS)
    commit(path, f"Add the references to data 0x{addr}", by)
    print(ghcat_line(meta, path))


def cmd_stats(_a) -> None:
    files = list(fn_dir().glob("*.md"))
    analysed = sum(1 for p in files if parse(p)[0].get("status") == "analysed")
    print(f"{BINARY}: {len(files)} functions, {analysed} analysed, {len(files) - analysed} raw  ({fn_dir()})")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("get")
    g.add_argument("target")
    g.add_argument("--refresh", action="store_true")
    g.add_argument("--sections", default="meta,analysis,comments,c,docs")
    g.add_argument("--max-lines", type=int, default=300)
    g.add_argument("--by", help="lane name for the commit")
    f = sub.add_parser("find")
    f.add_argument("pattern")
    f.add_argument("--status", choices=["raw", "analysed"])
    f.add_argument("--limit", type=int, default=50)
    n = sub.add_parser("note")
    n.add_argument("address")
    n.add_argument("--summary")
    n.add_argument("--tags")
    n.add_argument("--analysis")
    n.add_argument("--analysis-file")
    n.add_argument("--comment")
    n.add_argument("--by")
    x = sub.add_parser("xrefs")
    x.add_argument("address")
    x.add_argument("--write", action="store_true")
    x.add_argument("--limit", type=int, default=200)
    x.add_argument("--name")
    x.add_argument("--summary")
    x.add_argument("--comment")
    x.add_argument("--by")
    sub.add_parser("stats")
    a = p.parse_args()
    {"get": cmd_get, "find": cmd_find, "note": cmd_note, "stats": cmd_stats, "xrefs": cmd_xrefs}[a.cmd](a)


if __name__ == "__main__":
    main()
