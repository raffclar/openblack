"""Resolve conflict blocks where both sides only ADDED lines next to each other: keep the other side's lines (master),
then ours, dropping exact duplicates. Refuses (exit 1) if the merge base had lines inside a block (an edit, not an
append), so a real clash is never papered over. usage: union_conflicts.py <file>   (run with a diff3 conflict style)."""
import sys
import re


def edit_merge(ours, base, theirs):
    """Two edits both sides of a testbed merge make to the same lines: the command names' array size (the sum of both
    sides' additions), and a list of "kind == Kind::X ||" terms (the union, ours first). Anything else: None."""
    size = re.compile(r"^(\s*constexpr std::array<std::string_view, )(\d+)(> k_Names \{)\s*$")
    if len(ours) == len(base) == len(theirs) == 1 and all(size.match(x) for x in (ours[0], base[0], theirs[0])):
        o, b, th = (int(size.match(x).group(2)) for x in (ours[0], base[0], theirs[0]))
        m = size.match(ours[0])
        return [f"{m.group(1)}{o + th - b}{m.group(3)}"]
    term = re.compile(r"kind == Kind::(\w+)")
    joined = [" ".join(x) for x in (ours, base, theirs)]
    if all(term.search(j) for j in joined) and all(re.fullmatch(r"[\s\w:=|();]*", j) for j in joined):
        o, b, th = (term.findall(j) for j in joined)
        if not set(b) <= set(o) or not set(b) <= set(th):
            return None
        extra = [k for k in th if k not in o]
        if "IsPointerCommand(kind)" not in joined[0]:
            return None
        indent = ours[0][: len(ours[0]) - len(ours[0].lstrip())]
        line = " ".join(x.strip() for x in ours)
        line = line.replace("IsPointerCommand(kind)", "".join(f"kind == Kind::{k} || " for k in extra) + "IsPointerCommand(kind)")
        return [indent + line]
    return None


p = sys.argv[1]
lines = open(p, encoding="utf-8", newline="").read().split("\n")
out, i, blocks = [], 0, 0
while i < len(lines):
    line = lines[i]
    if not line.startswith("<<<<<<< "):
        out.append(line)
        i += 1
        continue
    ours, base, theirs, part = [], [], [], "ours"
    i += 1
    while not lines[i].startswith(">>>>>>> "):
        l = lines[i]
        if l.startswith("||||||| "):
            part = "base"
        elif l.startswith("======="):
            part = "theirs"
        else:
            {"ours": ours, "base": base, "theirs": theirs}[part].append(l)
        i += 1
    i += 1
    if any(b.strip() for b in base):
        merged = edit_merge(ours, base, theirs)
        if merged is None:
            print(f"{p}: a block edits existing lines; not a pure append", file=sys.stderr)
            sys.exit(1)
        out.extend(merged)
        blocks += 1
        continue
    # Keep both sides whole (code repeats lines like "{", "}" and "return;"); only an identical block is kept once
    out.extend(theirs if theirs == ours else theirs + ours)
    blocks += 1
open(p, "w", encoding="utf-8", newline="").write("\n".join(out))
print(f"resolved {p}: {blocks} block(s)")
