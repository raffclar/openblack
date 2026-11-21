#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tables, slopes and graphs of the samples of mem_soak.py runs.

    python tools/soak/mem_soak_report.py <run folder> [<run folder> ...] [--png <file>] [--json <file>]

For a skirmish run: the values at the end of each game, and the slope per game (least squares) over the games after
--skip games. For a land or server run: the slope per hour of game time over the samples after --skip-hours. The rule
(--limit-per-game, --limit-per-hour) says whether the run is stable: the slope of the private bytes is under the limit.
"""

import argparse
import json
import sys
from pathlib import Path

KEYS = ["private_mb", "working_set_mb", "handles", "threads", "gpu_mb", "textures_mb", "component_rows", "drawn_rows"]


def read(folder):
    path = Path(folder) / "samples.jsonl"
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        try:
            rows.append(json.loads(line))
        except ValueError:
            continue
    return rows


def slope(xs, ys):
    """The least-squares slope of ys over xs, or None with fewer than 2 points"""
    pairs = [(x, y) for x, y in zip(xs, ys) if x is not None and y is not None]
    if len(pairs) < 2:
        return None
    n = len(pairs)
    mx = sum(x for x, _ in pairs) / n
    my = sum(y for _, y in pairs) / n
    den = sum((x - mx) ** 2 for x, _ in pairs)
    if den == 0:
        return None
    return sum((x - mx) * (y - my) for x, y in pairs) / den


def resources_total(row):
    resources = row.get("resources")
    return sum(v for v in resources.values() if isinstance(v, (int, float))) if isinstance(resources, dict) else None


def skirmish_summary(rows, skip, limit):
    ends = [row for row in rows if row.get("kind") == "game_end"]
    table = []
    for row in ends:
        entry = {"game": row["game"], "end": row.get("end"), "turns": row.get("game_turns")}
        entry.update({key: row.get(key) for key in KEYS})
        entry["resources"] = resources_total(row)
        table.append(entry)
    later = [entry for entry in table if entry["game"] > skip]
    slopes = {key: slope([e["game"] for e in later], [e.get(key) for e in later]) for key in KEYS + ["resources"]}
    stable = slopes["private_mb"] is not None and slopes["private_mb"] < limit
    return {"kind": "skirmish", "games": table, "slope_per_game": slopes, "rule": f"private bytes slope after game {skip} "
            f"under {limit} MB a game", "stable": stable}


def long_summary(rows, skip_hours, limit):
    samples = [row for row in rows if row.get("turn") is not None]
    if not samples:
        samples = rows
    first_turn = min((row.get("turn") or 0) for row in samples)
    hours = [((row.get("turn") or 0) - first_turn) / 36000.0 for row in samples]
    later = [(h, row) for h, row in zip(hours, samples) if h >= skip_hours]
    slopes = {key: slope([h for h, _ in later], [row.get(key) for _, row in later]) for key in KEYS}
    slopes["resources"] = slope([h for h, _ in later], [resources_total(row) for _, row in later])
    first, last = (samples[0], samples[-1]) if samples else ({}, {})
    stable = slopes["private_mb"] is not None and slopes["private_mb"] < limit
    return {"kind": "long", "game_hours": hours[-1] if hours else 0, "first": {k: first.get(k) for k in KEYS},
            "last": {k: last.get(k) for k in KEYS}, "slope_per_hour": slopes,
            "rule": f"private bytes slope after {skip_hours} h under {limit} MB an hour", "stable": stable}


def plot(runs, png):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    figure, axes = plt.subplots(2, 2, figsize=(12, 8))
    for name, rows in runs:
        skirmish = any(row.get("kind") == "game_end" for row in rows)
        if skirmish:
            ends = [row for row in rows if row.get("kind") == "game_end"]
            xs = [row["game"] for row in ends]
            label_x = "game"
        else:
            ends = [row for row in rows if row.get("turn") is not None] or rows
            first = min((row.get("turn") or 0) for row in ends)
            xs = [((row.get("turn") or 0) - first) / 36000.0 for row in ends]
            label_x = "game hours"
        for axis, key in zip(axes.flat, ["private_mb", "working_set_mb", "gpu_mb", "handles"]):
            axis.plot(xs, [row.get(key) for row in ends], marker="." if skirmish else None, label=name)
            axis.set_title(key)
            axis.set_xlabel(label_x)
    for axis in axes.flat:
        axis.legend(fontsize=7)
        axis.grid(alpha=0.3)
    figure.tight_layout()
    figure.savefig(png, dpi=90)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("runs", nargs="+")
    parser.add_argument("--skip", type=int, default=3, help="Games left out of the slope at the start")
    parser.add_argument("--skip-hours", type=float, default=0.5, help="Game hours left out of the slope at the start")
    parser.add_argument("--limit-per-game", type=float, default=1.0)
    parser.add_argument("--limit-per-hour", type=float, default=5.0)
    parser.add_argument("--png")
    parser.add_argument("--json")
    args = parser.parse_args()
    runs = [(Path(folder).name, read(folder)) for folder in args.runs]
    summaries = {}
    for name, rows in runs:
        if any(row.get("kind") == "game_end" for row in rows):
            summaries[name] = skirmish_summary(rows, args.skip, args.limit_per_game)
        else:
            summaries[name] = long_summary(rows, args.skip_hours, args.limit_per_hour)
    print(json.dumps(summaries, indent=1, default=str))
    if args.json:
        Path(args.json).write_text(json.dumps(summaries, indent=1, default=str), encoding="utf-8")
    if args.png:
        plot(runs, args.png)
    return 0 if all(summary["stable"] for summary in summaries.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
