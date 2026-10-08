import os
import re
import sys

# Rebuilds progress/INDEX.md from the feature files' own rows.
root = sys.argv[1]
statuses = ("done", "partial", "todo", "n/a")


def count(path):
    counts = dict.fromkeys(statuses, 0)
    title = None
    for line in open(path, encoding="utf-8").read().splitlines():
        if title is None and line.startswith("# "):
            title = line[2:].strip()
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 3 and cells[1] in counts:
            counts[cells[1]] += 1
    return title, counts


def score(c):
    total = c["done"] + c["partial"] + c["todo"]
    return round(100 * (c["done"] + 0.5 * c["partial"]) / total) if total else 0


def pct(c):
    return f"{score(c)}%" if c["done"] + c["partial"] + c["todo"] else ""


domains = {}
for d in sorted(os.listdir(root)):
    p = os.path.join(root, d)
    if not os.path.isdir(p) or d.startswith("."):
        continue
    files = []
    for f in sorted(os.listdir(p)):
        if f.endswith(".md"):
            title, c = count(os.path.join(p, f))
            files.append((f, title, c))
    # one level of sub-folders (e.g. story/silver_scrolls), listed after the folder's own files
    for sub in sorted(os.listdir(p)):
        sp = os.path.join(p, sub)
        if os.path.isdir(sp) and not sub.startswith("."):
            for f in sorted(os.listdir(sp)):
                if f.endswith(".md"):
                    title, c = count(os.path.join(sp, f))
                    files.append((f"{sub}/{f}", title, c))
    if files:
        domains[d] = files

overall = dict.fromkeys(statuses, 0)
summary = []
for d, files in domains.items():
    c = dict.fromkeys(statuses, 0)
    for _, _, fc in files:
        for k in statuses:
            c[k] += fc[k]
            overall[k] += fc[k]
    summary.append((d, len(files), sum(c.values()), score(c)))

nfiles = sum(len(f) for f in domains.values())
out = ["# Progress index", "",
       "Every domain and feature file, with its score (done = 1, partial = ½, todo = 0, n/a left out). Generated from "
       "the files' own rows; regenerate after editing.", "",
       f"**Overall: {overall['done']} done, {overall['partial']} partial, {overall['todo']} todo, {overall['n/a']} n/a "
       f"across {sum(overall.values())} rows in {nfiles} files — {score(overall)}%**", "",
       "| Domain | Files | Rows | Score |", "|---|---|---|---|"]
for d, n, rows, s in sorted(summary, key=lambda x: -x[3]):
    out.append(f"| [{d}]({d}/) | {n} | {rows} | {s}% |")
for d, files in domains.items():
    c = dict.fromkeys(statuses, 0)
    for _, _, fc in files:
        for k in statuses:
            c[k] += fc[k]
    out += ["", f"## [{d}]({d}/) — {score(c)}%", "", "| File | Rows | Done | Partial | Todo | n/a | Score |",
            "|---|---|---|---|---|---|---|"]
    for f, title, fc in files:
        out.append(f"| [{title}]({d}/{f}) | {sum(fc.values())} | {fc['done']} | {fc['partial']} | {fc['todo']} | "
                   f"{fc['n/a']} | {pct(fc)} |")
open(os.path.join(root, "INDEX.md"), "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
print("files", nfiles, overall, score(overall))
