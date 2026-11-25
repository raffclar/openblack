#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the tiers of the game files of the web page (web/page/asset-tiers.json) from the traces of the game.

The web page records a trace with the page parameter recordAssetTrace (web/page/launcher-asset-trace.js). A trace
gives the first read of each block of each game file, with the phase of the game: boot, pre-intro, startup, land-load
or land-soon. The page gets the files of the phases before land-soon before the game starts, and the other files
later.

Usage:
  make_tiers.py --out <asset-tiers.json> --start <trace.json> ... [--later <trace.json> ...]
  make_tiers.py --check <asset-tiers.json>

A start trace goes from the start of the game to the first land. Each file of a start trace gets the first phase in
which a start trace reads it. A later trace is the trace of another land or a network map: its files that no start
trace reads get the phase "later", and the tiers file lists them under the name of the scenario of the trace. The page
gets them when the game goes to that land or map. A file that no trace reads has the phase "background": it is not in
the tiers file.

--check makes the tiers again from the traces that the tiers file names, and fails when the file is not the same.
"""

import argparse
import json
import math
import pathlib
import sys

PHASES = ["boot", "pre-intro", "startup", "land-load", "land-soon", "later", "background"]
# The phases that a trace records, and the phases that come before the start of the game
TRACE_PHASES = 5
START_PHASES = PHASES.index("land-soon")
LATER_PHASE = PHASES.index("later")
TRACE_FORMAT = 1
TIERS_FORMAT = 1


class TraceError(Exception):
    """A trace file that is not correct"""


def is_count(value):
    return isinstance(value, int) and not isinstance(value, bool) and value >= 0


def read_trace(path):
    """The trace in the file at the path, after a check of its fields"""
    try:
        trace = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise TraceError(f"{path}: {error}") from error
    if not isinstance(trace, dict) or trace.get("format") != TRACE_FORMAT:
        raise TraceError(f"{path}: the trace does not have the format {TRACE_FORMAT}")
    if trace.get("phases") != PHASES[:TRACE_PHASES]:
        raise TraceError(f"{path}: the trace has other phases: {trace.get('phases')}")
    if not is_count(trace.get("block_size")) or trace["block_size"] == 0:
        raise TraceError(f"{path}: the trace has no correct block size")
    if not isinstance(trace.get("scenario"), str) or not trace["scenario"]:
        raise TraceError(f"{path}: the trace has no scenario")
    reads = trace.get("reads")
    if not isinstance(reads, list):
        raise TraceError(f"{path}: the trace has no reads")
    for read in reads:
        if not (isinstance(read, dict) and isinstance(read.get("path"), str) and read["path"]
                and is_count(read.get("size")) and is_count(read.get("block"))
                and is_count(read.get("phase")) and read["phase"] < TRACE_PHASES
                and isinstance(read.get("t"), (int, float))):
            raise TraceError(f"{path}: the trace has a read that is not correct: {read}")
    return trace


def block_runs(blocks):
    """The blocks of a file as [phase, first block, number of blocks], for each run of blocks with the same phase"""
    runs = []
    for block in sorted(blocks):
        phase = blocks[block]
        if runs and runs[-1][0] == phase and runs[-1][1] + runs[-1][2] == block:
            runs[-1][2] += 1
        else:
            runs.append([phase, block, 1])
    return runs


def make_tiers(start, later, block_size, made_from):
    """The tiers as an object for the JSON file. start and later are lists of traces, in the order of the arguments."""
    files = {}
    for number, trace in enumerate(start):
        for read in trace["reads"]:
            lower = read["path"].lower()
            key = (read["phase"], number, read["t"])
            entry = files.setdefault(lower, {"key": key, "size": read["size"], "blocks": {}})
            entry["key"] = min(entry["key"], key)
            blocks = entry["blocks"]
            blocks[read["block"]] = min(blocks.get(read["block"], read["phase"]), read["phase"])
    lands = {}
    for number, trace in enumerate(later):
        paths = lands.setdefault(trace["scenario"].lower(), [])
        for read in sorted(trace["reads"], key=lambda read: read["t"]):
            lower = read["path"].lower()
            if lower in files and files[lower]["key"][0] != LATER_PHASE:
                continue
            if lower not in paths:
                paths.append(lower)
            key = (LATER_PHASE, len(start) + number, read["t"])
            entry = files.setdefault(lower, {"key": key, "size": read["size"], "blocks": {}})
            entry["key"] = min(entry["key"], key)
    tiers = {}
    orders = [0] * len(PHASES)
    for lower, entry in sorted(files.items(), key=lambda item: (item[1]["key"], item[0])):
        phase = entry["key"][0]
        tier = {"phase": phase, "order": orders[phase]}
        orders[phase] += 1
        blocks = entry["blocks"]
        count = max(1, math.ceil(entry["size"] / block_size))
        # A file that the start traces read in parts, or in more than one phase, keeps the phase of each block
        if blocks and (len(blocks) < count or any(value != phase for value in blocks.values())):
            tier["blocks"] = block_runs(blocks)
        tiers[lower] = tier
    return {
        "format": TIERS_FORMAT,
        "made_from": made_from,
        "phases": PHASES,
        "start_phases": START_PHASES,
        "block_size": block_size,
        "files": dict(sorted(tiers.items())),
        "lands": lands,
    }


def tiers_text(tiers):
    return json.dumps(tiers, indent=1, sort_keys=True) + "\n"


def build(out, start_paths, later_paths):
    """The text of the tiers file at out, from the traces at the paths"""
    folder = pathlib.Path(out).resolve().parent
    start = [read_trace(path) for path in start_paths]
    later = [read_trace(path) for path in later_paths]
    sizes = {trace["block_size"] for trace in start + later}
    if len(sizes) != 1:
        raise TraceError(f"the traces have different block sizes: {sorted(sizes)}")

    def name(path):
        resolved = pathlib.Path(path).resolve()
        try:
            return resolved.relative_to(folder).as_posix()
        except ValueError:
            return resolved.as_posix()

    made_from = {"start": [name(path) for path in start_paths], "later": [name(path) for path in later_paths]}
    return tiers_text(make_tiers(start, later, sizes.pop(), made_from))


def main(arguments):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", help="the tiers file to write")
    parser.add_argument("--start", nargs="*", default=[], help="the traces from the start of the game to the first land")
    parser.add_argument("--later", nargs="*", default=[], help="the traces of the other lands and of the network maps")
    parser.add_argument("--check", help="the tiers file to make again from its traces and compare")
    options = parser.parse_args(arguments)
    try:
        if options.check:
            path = pathlib.Path(options.check)
            committed = path.read_text(encoding="utf-8")
            made_from = json.loads(committed)["made_from"]
            folder = path.resolve().parent
            text = build(path, [folder / name for name in made_from["start"]],
                         [folder / name for name in made_from["later"]])
            if text != committed:
                print(f"make_tiers: {path} is not the output of its traces. Make it again with --out.",
                      file=sys.stderr)
                return 1
            print(f"make_tiers: {path} is the output of its traces")
            return 0
        if not options.out or not options.start:
            parser.error("give --out and at least one --start trace, or --check")
        text = build(options.out, options.start, options.later)
        pathlib.Path(options.out).write_text(text, encoding="utf-8", newline="\n")
        tiers = json.loads(text)
        counts = [0] * len(PHASES)
        for tier in tiers["files"].values():
            counts[tier["phase"]] += 1
        summary = ", ".join(f"{name} {count}" for name, count in zip(PHASES, counts) if count)
        print(f"make_tiers: wrote {options.out}: {summary}")
        return 0
    except (TraceError, OSError, KeyError, ValueError) as error:
        print(f"make_tiers: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
