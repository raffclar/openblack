"""Compare the functions that the linker drops with the candidates of dc_classify.py.

Usage: python tools/dead_code/dc_linker_compare.py <linker_verbose.log> <candidates.md> <out.md>

The log is the /VERBOSE:REF output of link.exe. A line has the form
    Discarded "<undecorated name>" (<decorated name>) from <object file>
A candidate "ns::Class::Name" matches the decorated names that start with "?Name@Class@ns@@". A constructor matches
"??0Class@ns@@".

What a drop proves: a function that is defined in a .cpp file has one copy in one object file. When the linker drops it, no
code in the finished game uses it. A function that a header defines (an inline function) has a copy in each object file that
includes the header. The linker keeps one copy when the game uses the function and drops all the copies when it does not.
So the script counts the objects that held the function: "dropped" needs a drop in the object of the definition of a .cpp
function, "dropped (inline)" needs a drop in every object that has a copy, which the log cannot show, so this class is
only "discarded in N objects". A candidate of any other kind (a variable, a type) is "not a function".
Classes of the result: dropped, discarded-inline, absent, not a function.
"absent" has three causes. The linker kept the function. The compiler left out an unused inline function, so the linker never
saw it. The function is virtual: a virtual function stays in the game because the table of the class names it, so for a
virtual function "absent" is no proof of use. Check an "absent" row that is not inline and not virtual by hand.
"""

import re
import sys

LINE = re.compile(r'Discarded ".*" \((\?[^)]+)\) from (\S+)')


def read_log(path):
    names = {}
    with open(path, encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            m = LINE.search(line)
            if not m:
                continue
            decorated, obj = m.groups()
            key = decorated[1:].split("@@", 1)[0]
            names.setdefault(key, set()).add(obj)
    return names


def key_of(qual, kind):
    parts = qual.split("::")
    if kind == "CONSTRUCTOR":
        return "?0" + "@".join(reversed(parts[:-1]))
    return "@".join(reversed(parts))


def main():
    log_path, cand_path, out_path = sys.argv[1:4]
    names = read_log(log_path)
    cls = None
    stats = {}
    rows = []
    with open(cand_path, encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("## "):
                cls = line.split()[1]
            elif line.startswith("| ") and not line.startswith("| symbol"):
                sym, loc, kind, _ = [x.strip() for x in line.strip().strip("|").split("|")]
                if kind not in ("FUNCTION_DECL", "CXX_METHOD", "CONSTRUCTOR"):
                    state, objs = "not a function", set()
                else:
                    objs = names.get(key_of(sym, kind), set())
                    if not objs:
                        state = "absent"
                    elif loc.split(":")[0].endswith(".cpp"):
                        state = "dropped"
                    else:
                        state = "discarded-inline"
                stats[(cls, state)] = stats.get((cls, state), 0) + 1
                rows.append((cls, sym, loc, kind, state, len(objs)))
    with open(out_path, "w", encoding="utf-8") as o:
        o.write("# Linker cross-check\n\nNames with a Discarded line: %d\n\n| class | state | count |\n|---|---|---|\n" % len(names))
        for (c, s), n in sorted(stats.items()):
            o.write("| %s | %s | %d |\n" % (c, s, n))
        o.write("\n## Candidates that the linker did not drop\n\n| class | symbol | file:line | kind | state | objects |\n"
                "|---|---|---|---|---|---|\n")
        for r in rows:
            if r[4] in ("absent", "discarded-inline") and r[0] in ("dead", "ctor-unused", "test-only"):
                o.write("| %s | %s | %s | %s | %s | %d |\n" % r)
    print({"%s/%s" % k: v for k, v in stats.items()})


if __name__ == "__main__":
    main()
