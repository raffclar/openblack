"""After a merge: the testbed command names array must be as long as the Command::Kind enum and in the same order.
Sets the array's size to its count of names; exits 1 if the enum and the names differ in count, 0 otherwise (prints
'fixed' when it changed the size). usage: fix_kind_names.py <worktree>"""
import re
import sys

root = sys.argv[1]
header = open(f"{root}/src/Debug/TestbedScenarioRegistry.h", encoding="utf-8").read()
# The command kinds are the enum class Kind that has the Stop kind (other enums of the same name may come after it)
starts = [m.start() for m in re.finditer(r"enum class Kind", header)]
start = [s for s in starts if re.search(r"\bStop\b", header[s : header.index("};", s)])][-1]
body = re.sub(r"//[^\n]*", "", header[header.index("{", start) + 1 : header.index("};", start)])
kinds = [k.strip().split("=")[0].strip() for k in body.split(",") if k.strip()]

path = f"{root}/src/Debug/TestbedScenarioRegistry.cpp"
source = open(path, encoding="utf-8", newline="").read()
at = source.index("Name(Command::Kind kind)")
match = re.compile(r"(std::array<std::string_view, )(\d+)(> k_Names \{)(.*?)\};", re.S).search(source, at)
names = re.findall(r'"([^"]*)"', match.group(4))
if len(names) != len(kinds):
    print(f"enum has {len(kinds)} kinds, names array {len(names)}", file=sys.stderr)
    sys.exit(1)
if int(match.group(2)) != len(names):
    source = source[: match.start(2)] + str(len(names)) + source[match.end(2) :]
    open(path, "w", encoding="utf-8", newline="").write(source)
    print("fixed")
