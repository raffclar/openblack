"""Resolve a docs/deviations.md merge conflict: master's file, with the branch's new or changed rows.

usage: resolve_deviations.py <worktree> <base-rev> <branch-rev> <master-rev>
Rows are table lines starting "| <number> |". A row the branch changed (differs from the base) replaces master's row
of that number unless master changed it too, which is a real conflict (exit 2). A row the branch added whose number
master also added is renumbered to the next free number and reported (exit 0, with a RENUMBERED line), so the lead
checks the row's mentions. Everything outside the table comes from master.
"""
import re
import subprocess
import sys

wt, base, branch, master = sys.argv[1:5]
PATH = sys.argv[5] if len(sys.argv) > 5 else "docs/deviations.md"
# A row number may carry a " (?)" mark (user, 2026-10-12: a deviation recorded with a question mark)
ROW = re.compile(r"^\| *(\d+)( *\(\?\))? *\|")


def show(rev):
    r = subprocess.run(["git", "-C", wt, "show", f"{rev}:{PATH}"], capture_output=True, text=True, encoding="utf-8")
    return r.stdout if r.returncode == 0 else ""


def rows(text):
    return {int(m.group(1)): line for line in text.splitlines() if (m := ROW.match(line))}


base_rows, branch_rows = rows(show(base)), rows(show(branch))
master_text = show(master)
master_rows = rows(master_text)
lines = master_text.splitlines()
out = dict(master_rows)
added = []
for num, line in sorted(branch_rows.items()):
    if base_rows.get(num) == line:
        continue  # unchanged on the branch
    if num in base_rows:  # the branch edited an existing row
        if master_rows.get(num) not in (base_rows[num], line):
            print(f"CONFLICT row {num}: changed on both sides")
            sys.exit(2)
        out[num] = line
    elif num in master_rows and master_rows[num] != line:
        added.append(line)  # same new number on both sides: renumber
    else:
        out[num] = line
for line in added:
    new = max(out) + 1
    old = int(ROW.match(line).group(1))
    mark = ROW.match(line).group(2) or ""
    out[new] = ROW.sub(f"| {new}{mark} |", line, count=1)
    print(f"RENUMBERED row {old} -> {new}")
table_start = next(i for i, l in enumerate(lines) if ROW.match(l))
table_end = max(i for i, l in enumerate(lines) if ROW.match(l))
result = lines[:table_start] + [out[n] for n in sorted(out)] + lines[table_end + 1:]
open(f"{wt}/{PATH}", "w", encoding="utf-8", newline="\n").write("\n".join(result) + "\n")
print(f"resolved {PATH}: {len(out)} rows")
