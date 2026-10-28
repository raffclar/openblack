"""Resolve a test/CMakeLists.txt merge conflict: master's file plus the branch's new statements.

usage: resolve_cmake.py <worktree> <base-rev> <branch-rev> <master-rev>
Statements are whole CMake commands (multi-line, balanced parens). The result is master's text with every statement
the branch added (not in the base, not already in master) appended in the branch's order.
"""
import subprocess
import sys

wt, base, branch, master = sys.argv[1:5]
PATH = "test/CMakeLists.txt"


def show(rev):
    return subprocess.run(["git", "-C", wt, "show", f"{rev}:{PATH}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout


def statements(text):
    out, cur, depth = [], [], 0
    for line in text.splitlines():
        if not cur and (not line.strip() or line.lstrip().startswith("#")):
            continue
        cur.append(line)
        depth += line.count("(") - line.count(")")
        if depth <= 0:
            out.append("\n".join(cur))
            cur, depth = [], 0
    return out


def key(stmt):
    # whitespace-free, so a statement cmake-format rewrapped over several lines is still the same statement
    return "".join(stmt.split())


base_s = {key(s) for s in statements(show(base))}
master_text = show(master)
master_s = {key(s) for s in statements(master_text)}
added = []
for s in statements(show(branch)):
    if key(s) not in base_s and key(s) not in master_s and key(s) not in {key(a) for a in added}:
        added.append(s)
result = master_text.rstrip("\n") + "\n" + "".join(s + "\n" for s in added)
with open(f"{wt}/{PATH}", "w", encoding="utf-8", newline="\n") as f:
    f.write(result)
print(f"resolved {PATH}: appended {len(added)} statement(s)")
