#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Compares the draw traces of two computers and shows the first synced draw where they are not the same.

The game writes a draw trace with --sim-trace-draws FILE, and the game server with --trace-draws FILE. Each line is
one draw on the synced seed: the turn, the phase ("turn" in a game turn, "frame" between the turns), the count of synced
draws after it, the place of the call (file:line), the function and the context (the script task that runs, if any). A "restart" line starts a new network game, so the
compare starts after the last "restart" of each file. A "set" line, from a world load, puts back the count of draws: the
draws go by their counts, so the compare continues after a load. The compare stops at the last turn that both traces
have.

    python tools/sim/draw_trace.py server/draws.tsv client2/draws.tsv
"""

import argparse
import collections
import json
import sys

FIELDS = ("turn", "phase", "index", "where", "function")


def read(path):
    """The draws of a trace after its last restart, as dicts, and the counts that world loads put back"""
    draws = []
    loads = []
    with open(path, encoding="utf-8", errors="replace") as file:
        for line in file:
            parts = line.rstrip("\r\n").split("\t")
            if parts[0] == "restart":
                draws = []
                loads = []
            elif parts[0] == "set" and len(parts) > 1:
                loads.append({"after": len(draws), "draws": int(parts[1])})
            elif parts[0] == "draw" and len(parts) >= 6:
                draws.append({"turn": int(parts[1]), "phase": parts[2], "index": int(parts[3]), "where": parts[4],
                              "function": parts[5], "context": parts[6] if len(parts) > 6 else ""})
    return draws, loads


def common_turns(first, second):
    """The draws of both traces up to the last game turn that both traces have"""
    last = min(max((draw["turn"] for draw in first if draw["phase"] == "turn"), default=0),
               max((draw["turn"] for draw in second if draw["phase"] == "turn"), default=0))

    def cut(draws):
        return [draw for draw in draws if draw["turn"] < last or (draw["turn"] == last and draw["phase"] == "turn")]

    return cut(first), cut(second), last


def same(first, second):
    return all(first[key] == second[key] for key in ("turn", "phase", "where"))


def first_difference(reference, other):
    """The first draw of the other trace that is not the same as the reference's draw with the same count, as (reference
    draw or None, other draw or None), or None. The draws go by their counts, so a world load that puts back the count of
    draws continues the compare at that count. A reference draw that the other trace does not have also differs."""
    by_index = {draw["index"]: draw for draw in reference}
    for draw in other:
        ours = by_index.get(draw["index"])
        if ours is None or not same(ours, draw):
            return ours, draw
    last = max((draw["index"] for draw in other), default=0)
    missing = [draw for draw in reference if draw["index"] > last]
    if missing:
        return missing[0], None
    return None


def sites_of_turn(draws, turn):
    """The count of the draws of each place in one turn, turn and frame phases apart"""
    counts = collections.Counter(f"{draw['phase']} {draw['where']}" for draw in draws if draw["turn"] == turn)
    return dict(sorted(counts.items()))


def compare(reference_path, other_path, context=3):
    """The first difference of two traces as a dict for JSON, with no "first" when the traces are the same"""
    reference, reference_loads = read(reference_path)
    other, other_loads = read(other_path)
    reference, other, last = common_turns(reference, other)
    result = {"compared_turns": last, "reference_draws": len(reference), "other_draws": len(other),
              "reference_loads": reference_loads, "other_loads": other_loads}
    found = first_difference(reference, other)
    if found is None:
        return result
    ours, theirs = found
    turn = min(draw["turn"] for draw in (ours, theirs) if draw is not None)
    index = (ours or theirs)["index"]
    reference_sites = sites_of_turn(reference, turn)
    other_sites = sites_of_turn(other, turn)
    names = set(reference_sites) | set(other_sites)
    result["first"] = {
        "index": index,
        "reference": ours,
        "other": theirs,
        "before": [draw for draw in reference if index - context <= draw["index"] < index],
        "reference_sites_of_turn": reference_sites,
        "other_sites_of_turn": other_sites,
        # The places whose counts are not the same in that turn: the likely extra draw
        "count_differences": {name: [reference_sites.get(name, 0), other_sites.get(name, 0)] for name in sorted(names)
                              if reference_sites.get(name, 0) != other_sites.get(name, 0)},
    }
    return result


def describe(draw):
    if draw is None:
        return "no draw (the trace ends)"
    context = f" ({draw['context']})" if draw.get("context") else ""
    return f"turn {draw['turn']} {draw['phase']} draw {draw['index']} at {draw['where']} in {draw['function']}{context}"


def summary_lines(result, reference_name="reference", other_name="other"):
    """A short text of a compare"""
    lines = [f"{result['compared_turns']} turns compared: {reference_name} {result['reference_draws']} draws, "
             f"{other_name} {result['other_draws']} draws"]
    first = result.get("first")
    if first is None:
        lines.append("all draws the same")
        return lines
    lines.append(f"first different draw, synced draw {first['index']}:")
    lines.append(f"  {reference_name}: {describe(first['reference'])}")
    lines.append(f"  {other_name}: {describe(first['other'])}")
    for name, (ours, theirs) in first["count_differences"].items():
        lines.append(f"  in that turn {name}: {reference_name} {ours}, {other_name} {theirs}")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("reference", help="the trace to compare with, such as the server's")
    parser.add_argument("other", help="the trace of the other computer")
    parser.add_argument("--context", type=int, default=3, help="the same draws to show before the first difference")
    parser.add_argument("--json", action="store_true", help="write the whole compare as JSON")
    options = parser.parse_args()
    result = compare(options.reference, options.other, options.context)
    if options.json:
        print(json.dumps(result, indent=2))
    else:
        print("\n".join(summary_lines(result)))
    return 0 if "first" not in result else 1


if __name__ == "__main__":
    sys.exit(main())
