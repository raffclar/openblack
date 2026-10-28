"""Compares the checksum of each turn in two logs of the game or the server, for example a server log of the Mac and a server
log of the PC of the same land, seed and AI seed. Two builds for other processors must play the same turns.

    python tools/sim/checksum_compare.py mac/server.log pc/server.log

It prints one JSON line: the turns that both logs have, the first turn whose checksum is not the same, and "same". Exit code
0 when every common turn is the same and the logs have at least one common turn, 1 otherwise.
"""

import json
import re
import sys

CHECKSUM_LINE = re.compile(r"turn (\d+) checksum ([0-9a-f]{16})")


def checksums(lines):
    """The checksum of each turn in the lines of a log (the first line of a turn counts)"""
    turns = {}
    for line in lines:
        found = CHECKSUM_LINE.search(line)
        if found:
            turns.setdefault(int(found.group(1)), found.group(2))
    return turns


def compare(first, second):
    """The common turns, the first different turn (or None) and the count of different turns"""
    common = sorted(set(first) & set(second))
    different = [turn for turn in common if first[turn] != second[turn]]
    return {"common": len(common), "last_common": common[-1] if common else None,
            "first_different": different[0] if different else None, "different": len(different),
            "same": bool(common) and not different}


def read(path):
    with open(path, encoding="utf-8", errors="replace") as file:
        return checksums(file)


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    result = compare(read(argv[0]), read(argv[1]))
    print(json.dumps(result))
    return 0 if result["same"] else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
