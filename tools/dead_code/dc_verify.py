"""Add a word count to each candidate of dc_classify.py.

Usage: python tools/dead_code/dc_verify.py <repo root> <candidates.md> <verified.md>

The count is the number of times the last name of the symbol appears as a word in the tracked text files. A count of 1 or 2 is
the definition and its declaration. A higher count means a text mention that the index did not see, so the row needs a look.
"""

import collections
import re
import subprocess
import sys

TEXT = (".cpp", ".h", ".hpp", ".inl", ".txt", ".cmake", ".py", ".lua", ".json", ".md", ".in")


def tracked_text(root):
    names = subprocess.run(["git", "-C", root, "ls-files"], capture_output=True, text=True).stdout.splitlines()
    return [f for f in names if f.endswith(TEXT) and not f.startswith(("vcpkg", "externals", "assets"))]


def main():
    root, cand, out_path = sys.argv[1:4]
    count = collections.Counter()
    word = re.compile(r"[A-Za-z_]\w*")
    for f in tracked_text(root):
        try:
            with open(root + "/" + f, encoding="utf-8", errors="ignore") as fh:
                count.update(word.findall(fh.read()))
        except OSError:
            continue
    cls = None
    with open(cand, encoding="utf-8") as src, open(out_path, "w", encoding="utf-8") as out:
        out.write("| class | symbol | file:line | kind | evidence | word count |\n|---|---|---|---|---|---|\n")
        for line in src:
            if line.startswith("## "):
                cls = line.split()[1]
            elif line.startswith("| ") and not line.startswith("| symbol"):
                sym, loc, kind, ev = [x.strip() for x in line.strip().strip("|").split("|")]
                out.write("| %s | %s | %s | %s | %s | %d |\n" % (cls, sym, loc, kind, ev, count.get(sym.split("::")[-1], 0)))


if __name__ == "__main__":
    main()
