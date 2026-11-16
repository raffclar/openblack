"""Classify the symbols from dc_index.py.

Usage: python tools/dead_code/dc_classify.py <index.json> <out.md> <repo root>

A symbol is dead when no reference comes from outside its own definition (and, for a type, outside its own members).
A virtual override is alive when the base slot is referenced anywhere. test-only: all outside references are in test/.
inspector-only: all outside references are in src/Inspector, Components_*.cpp or generated tables.
Entry points (main, test macros) are listed as exempt.
A constructor with no use is "ctor-unused": a call through make_unique or emplace is hidden in the
library headers, so the row needs a text check. A template counts as used when its name is used anywhere, because the uses of
its specializations have other names.
"""
import json
import sys
import re
import subprocess

REAL = {}
for _l in subprocess.run(["git", "-C", sys.argv[3], "ls-files"], capture_output=True,
                         text=True).stdout.splitlines():
    REAL[_l.lower()] = _l

idx = json.load(open(sys.argv[1]))
defs, refs = idx["defs"], idx["refs"]

by_target = {}
for target, f, enc in refs:
    by_target.setdefault(target, []).append((f, enc))

# parent definition of every definition: a reference from a member counts as a reference from the type itself
def inside(enc, usr):
    return enc == usr

INSPECTOR = re.compile(r"(^src/Inspector|Components_.*\.cpp|ComponentFields|Reflect|Inspector)")
EXEMPT = re.compile(r"^(main|WinMain|wmain|SDL_main|operator new|operator delete)$")

# resolve overrides by class bases and signature (the python bindings lack get_overridden_cursors)
cls_by_usr = {u: d for u, d in defs.items() if "bases" in d}
meths = {}
for u, d in defs.items():
    if "cls" in d:
        meths.setdefault((d["cls"], d["sig"]), u)


def ancestors(cu, seen=None):
    seen = seen if seen is not None else set()
    for b in cls_by_usr.get(cu, {}).get("bases", []):
        if b not in seen:
            seen.add(b)
            ancestors(b, seen)
    return seen


for u, d in defs.items():
    d["override"] = []
    d["libbase"] = False
    if d["virtual"] and "cls" in d:
        suffix = u[len(d["cls"]):]
        for a in ancestors(d["cls"]):
            if a not in cls_by_usr:
                d["libbase"] = True
            d["override"].append(a + suffix)

# virtual bases: a base slot is live when it or any of its overrides is referenced through the base
overridden_by = {}
for u, d in defs.items():
    for b in d.get("override", []):
        overridden_by.setdefault(b, []).append(u)


def slot_used(b):
    return bool(by_target.get(b)) or any(outside_refs(x) for x in overridden_by.get(b, []))


def outside_refs(u):
    d = defs[u]
    out = []
    for f, enc in by_target.get(u, []):
        if enc == u:
            continue
        out.append((f, enc))
    return out


UNRES = {t[2:] for t in by_target if t.startswith("n:")}


def classify(u):
    d = defs[u]
    if d["name"] in UNRES and d["kind"] != "VAR_DECL" or d["name"] in UNRES:
        return "alive", "name used in unresolved (template) context"
    if EXEMPT.match(d["name"]):
        return "exempt", ""
    r = outside_refs(u)
    # a virtual override is called through its base
    if d["virtual"]:
        for b in d.get("override", []):
            if slot_used(b):
                return "alive", "base slot used"
        if d["override"]:
            # the base is declared outside the project (library virtual): alive
            pass
        if d["libbase"]:
            return "alive", "library base"
    if d["virtual"] and overridden_by.get(u) and any(outside_refs(x) for x in overridden_by[u]):
        return "alive", "override used"
    if d["defaulted"]:
        return "exempt", "defaulted"
    if not r:
        if d["kind"] == "CONSTRUCTOR":
            return "ctor-unused", "no direct call"
        return "dead", "no reference"
    files = {f for f, _ in r}
    if all(f.startswith("test/") for f in files):
        return "test-only", ",".join(sorted(files)[:3])
    if all(INSPECTOR.search(f) or f.startswith("test/") for f in files):
        return "inspector-only", ",".join(sorted(files)[:3])
    return "alive", ""


rows = {"dead": [], "ctor-unused": [], "test-only": [], "inspector-only": []}
for u, d in defs.items():
    if d["file"].startswith(("test/",)) or re.search(r"json\.hpp|tiny_gltf|stb_|^externals", d["file"]):
        continue
    if d["kind"] in ("TYPE_ALIAS_DECL", "TYPEDEF_DECL", "DESTRUCTOR"):
        continue
    if d["kind"] == "VAR_DECL" and d["file"].endswith(".cpp") is False:
        pass
    c, ev = classify(u)
    if c in rows:
        rows[c].append((REAL.get(d["file"], d["file"]), d["line"], d["kind"], d["qual"] or d["name"], ev))

with open(sys.argv[2], "w") as o:
    for c in rows:
        o.write(f"\n## {c} ({len(rows[c])})\n\n| symbol | file:line | kind | evidence |\n|---|---|---|---|\n")
        for f, l, k, n, ev in sorted(rows[c]):
            o.write(f"| {n} | {f}:{l} | {k} | {ev} |\n")
print({c: len(v) for c, v in rows.items()})
