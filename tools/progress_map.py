#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Draws the progress of openblack as a map of squares, in the style of the GitHub contribution calendar.

The tool reads a progress folder. Each folder in it is one domain of the game. Each Markdown file in a domain has
tables, and the second column of each table row is the status: done, partial, todo or n/a. A Markdown file at the top
of the folder is a domain of its own. The tool does not read INDEX.md and README.md.

The tool writes an SVG file. Each domain is a block of squares with its name and its score. Each square shows up to
N rows of one status. The tool does not show n/a rows.

usage: python tools/progress_map.py <progress folder> [--out docs/progress-map.svg] [--rows-per-square N]
"""

import argparse
import datetime
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

STATUSES = ("done", "partial", "todo", "n/a")
SHOWN_STATUSES = ("done", "partial", "todo")
SKIPPED_FILES = {"INDEX.md", "README.md"}

# The size of the map, in SVG pixels
SQUARE = 8
PITCH = 10
SQUARES_PER_COLUMN = 7
MAP_WIDTH = 1000
MARGIN = 16
TILE_GAP_X = 18
TILE_GAP_Y = 14
LABEL_HEIGHT = 16
CHAR_WIDTH = 6.4
MAX_COLUMNS_PER_TILE = (MAP_WIDTH - 2 * MARGIN) // PITCH

COLOURS = {
    "done": "#216e39",
    "partial": "#d4a72c",
    "todo": "#afb8c1",
}


@dataclass
class Domain:
    name: str
    counts: dict = field(default_factory=lambda: {status: 0 for status in STATUSES})

    @property
    def rows(self):
        """The number of rows that the map shows (all the rows that are not n/a)."""
        return sum(self.counts[status] for status in SHOWN_STATUSES)

    @property
    def percent(self):
        return score_percent(self.counts)


def score_percent(counts):
    """Gives the score as a percent: done is 1, partial is a half, todo is 0. The score is 100 only when all is done."""
    rows = sum(counts[status] for status in SHOWN_STATUSES)
    if rows == 0:
        return 100
    percent = round(100 * (counts["done"] + 0.5 * counts["partial"]) / rows)
    if counts["done"] < rows:
        percent = min(percent, 99)
    return percent


def row_status(line):
    """Gives the status of one Markdown table row, or None when the line is not a row with a status."""
    line = line.strip()
    if not line.startswith("|"):
        return None
    cells = [cell.strip() for cell in line.strip("|").split("|")]
    if len(cells) < 2:
        return None
    status = cells[1].strip("*` ").lower()
    return status if status in STATUSES else None


def read_progress(folder):
    """Reads the progress folder and gives one Domain for each domain, in the order of the names."""
    folder = Path(folder)
    if not folder.is_dir():
        raise FileNotFoundError(f"The progress folder {folder} does not exist")
    domains = {}
    for path in sorted(folder.rglob("*.md")):
        relative = path.relative_to(folder)
        if len(relative.parts) == 1 and path.name in SKIPPED_FILES:
            continue
        name = relative.parts[0] if len(relative.parts) > 1 else path.stem
        domain = domains.setdefault(name, Domain(name))
        for line in path.read_text(encoding="utf-8").splitlines():
            status = row_status(line)
            if status is not None:
                domain.counts[status] += 1
    return [domain for domain in sorted(domains.values(), key=lambda d: d.name) if domain.rows > 0]


def squares_for(domain, rows_per_square):
    """Gives the status of each square of a domain. Each status gets enough squares for all its rows."""
    squares = []
    for status in SHOWN_STATUSES:
        squares += [status] * math.ceil(domain.counts[status] / rows_per_square)
    return squares


def auto_rows_per_square(domains):
    """Gives the smallest number of rows for each square that puts the largest domain in one line of squares."""
    largest = max((domain.rows for domain in domains), default=0)
    return max(1, math.ceil(largest / (MAX_COLUMNS_PER_TILE * SQUARES_PER_COLUMN)))


def label_of(domain):
    return f"{domain.name.replace('_', ' ')} {domain.percent}%"


def layout(domains, rows_per_square):
    """Puts the domain tiles in lines, the largest first, into the first line with space (first fit)."""
    tiles = []
    for domain in domains:
        squares = squares_for(domain, rows_per_square)
        columns = min(MAX_COLUMNS_PER_TILE, math.ceil(len(squares) / SQUARES_PER_COLUMN))
        lines = math.ceil(len(squares) / (columns * SQUARES_PER_COLUMN))
        width = max(columns * PITCH, len(label_of(domain)) * CHAR_WIDTH)
        height = LABEL_HEIGHT + lines * SQUARES_PER_COLUMN * PITCH
        tiles.append({"domain": domain, "squares": squares, "columns": columns, "width": width, "height": height})
    tiles.sort(key=lambda tile: (-tile["width"], tile["domain"].name))
    lines = []
    for tile in tiles:
        for line in lines:
            if line["width"] + TILE_GAP_X + tile["width"] <= MAP_WIDTH - 2 * MARGIN:
                line["width"] += TILE_GAP_X + tile["width"]
                line["tiles"].append(tile)
                break
        else:
            lines.append({"width": tile["width"], "tiles": [tile]})
    return lines


def square(x, y, status):
    """Gives one square of the map. The class gives the colour of the status."""
    return f'<rect x="{x}" y="{y}" width="{SQUARE}" height="{SQUARE}" rx="2" class="s{status[0]}"/>'


def escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def render_svg(domains, rows_per_square, date):
    """Gives the SVG text of the map."""
    totals = {status: sum(domain.counts[status] for domain in domains) for status in STATUSES}
    shown = sum(totals[status] for status in SHOWN_STATUSES)
    lines = layout(domains, rows_per_square)

    out = []
    y = MARGIN
    title = f"openblack progress against Black & White (2001), {date}"
    summary = (f"{shown} rows: {totals['done']} done, {totals['partial']} partial, {totals['todo']} todo"
               f" ({totals['n/a']} n/a rows not shown). Score {score_percent(totals)}%")
    out.append(f'<text x="{MARGIN}" y="{y + 12}" class="h">{escape(title)}</text>')
    out.append(f'<text x="{MARGIN}" y="{y + 30}" class="t">{escape(summary)}</text>')
    y += 46

    for line in lines:
        x = MARGIN
        for tile in line["tiles"]:
            out.append(f'<text x="{x}" y="{y + 11}" class="t">{escape(label_of(tile["domain"]))}</text>')
            top = y + LABEL_HEIGHT
            per_line = tile["columns"] * SQUARES_PER_COLUMN
            for index, status in enumerate(tile["squares"]):
                strip, place = divmod(index, per_line)
                column, row = divmod(place, SQUARES_PER_COLUMN)
                sx = x + column * PITCH
                sy = top + (strip * SQUARES_PER_COLUMN + row) * PITCH
                out.append(square(sx, sy, status))
            x += tile["width"] + TILE_GAP_X
        y += max(tile["height"] for tile in line["tiles"]) + TILE_GAP_Y

    legend_x = MARGIN
    y += 4
    for status in SHOWN_STATUSES:
        out.append(square(legend_x, y, status))
        out.append(f'<text x="{legend_x + 12}" y="{y + 8}" class="t">{status}</text>')
        legend_x += 12 + len(status) * CHAR_WIDTH + 14
    unit = "1 row" if rows_per_square == 1 else f"up to {rows_per_square} rows"
    note = f"One square is {unit} of one status. The score counts done as 1, partial as 1/2 and todo as 0."
    out.append(f'<text x="{legend_x + 6}" y="{y + 8}" class="t">{escape(note)}</text>')
    height = y + SQUARE + MARGIN

    style = (
        "text{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;fill:#24292f}"
        ".h{font-size:14px;font-weight:600}.t{font-size:11px}"
        + "".join(f".s{status[0]}{{fill:{colour}}}" for status, colour in COLOURS.items())
    )
    head = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{MAP_WIDTH}" height="{height}" '
        f'viewBox="0 0 {MAP_WIDTH} {height}" role="img" aria-label="{escape(title)}">',
        f"<style>{style}</style>",
        f'<rect x="0.5" y="0.5" width="{MAP_WIDTH - 1}" height="{height - 1}" rx="6" '
        'fill="#ffffff" stroke="#d0d7de"/>',
    ]
    return "\n".join(head + out + ["</svg>", ""])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("progress", help="the progress folder (one folder for each domain)")
    parser.add_argument("--out", default="docs/progress-map.svg", help="the SVG file to write")
    parser.add_argument("--rows-per-square", type=int, default=0,
                        help="the rows for each square (0: the smallest number that keeps each domain on one line)")
    parser.add_argument("--date", default=datetime.date.today().isoformat(), help="the date of the snapshot")
    args = parser.parse_args(argv)

    domains = read_progress(args.progress)
    rows_per_square = args.rows_per_square or auto_rows_per_square(domains)
    svg = render_svg(domains, rows_per_square, args.date)
    Path(args.out).write_text(svg, encoding="utf-8", newline="\n")
    shown = sum(domain.rows for domain in domains)
    print(f"progress_map: {len(domains)} domains, {shown} rows, {rows_per_square} row(s) for each square, "
          f"{len(svg)} bytes in {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
