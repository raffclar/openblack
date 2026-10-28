"""Resolve one diff3 conflict block by hand. usage: hunk.py <file> <index from 1> <mode> [drop-regex]
mode: union (master lines, then branch lines), ours (master), theirs (branch), theirs-drop / union-drop (as theirs or
union, without branch lines that match drop-regex), show (print the block)."""
import re
import sys

path, index, mode = sys.argv[1], int(sys.argv[2]), sys.argv[3]
drop = re.compile(sys.argv[4]) if len(sys.argv) > 4 else None
lines = open(path, encoding="utf-8", newline="").read().splitlines(True)
out, n, i = [], 0, 0
while i < len(lines):
	if lines[i].startswith("<<<<<<< "):
		n += 1
		j = i + 1
		head, base, theirs, part = [], [], [], "head"
		while not lines[j].startswith(">>>>>>> "):
			if lines[j].startswith("||||||| "):
				part = "base"
			elif lines[j].startswith("======="):
				part = "theirs"
			else:
				{"head": head, "base": base, "theirs": theirs}[part].append(lines[j])
			j += 1
		if n == index:
			if mode == "show":
				print("".join(lines[i : j + 1]))
				sys.exit(0)
			t = [x for x in theirs if not (drop and drop.search(x))]
			out += {"union": head + t, "ours": head, "theirs": t, "theirs-drop": t, "union-drop": head + t}[mode]
		else:
			out += lines[i : j + 1]
		i = j + 1
		continue
	out.append(lines[i])
	i += 1
open(path, "w", encoding="utf-8", newline="").write("".join(out))
print("remaining", sum(1 for x in out if x.startswith("<<<<<<< ")))
