"""Sort the rows of the longest numbered table in a Markdown file by their number.

usage: sort_rows.py <file>   (for example docs/vanilla-bugs.md after rows were renumbered)
Rows are lines that start with "| <number> |". Only the longest run of such lines in a row is sorted.
"""
import re
import sys

ROW = re.compile(r"^\| (\d+) \|")
path = sys.argv[1]
with open(path, encoding="utf-8", newline="") as f:
    lines = f.read().split("\n")
runs, current = [], []
for index, line in enumerate(lines):
    if ROW.match(line):
        if current and index != current[-1] + 1:
            runs.append(current)
            current = []
        current.append(index)
if current:
    runs.append(current)
if runs:
    run = max(runs, key=len)
    rows = sorted((lines[i] for i in run), key=lambda line: int(ROW.match(line).group(1)))
    for index, row in zip(run, rows):
        lines[index] = row
with open(path, "w", encoding="utf-8", newline="") as f:
    f.write("\n".join(lines))
