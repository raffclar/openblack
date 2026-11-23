#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of progress_map.py with a made-up progress folder in a temporary folder."""

import sys
import tempfile
import unittest
import xml.etree.ElementTree as ElementTree
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import progress_map  # noqa: E402

TABLE_HEAD = "| Behaviour | Status | Where / notes |\n|-----------|--------|---------------|\n"


def write(folder, relative, rows):
    path = Path(folder) / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    body = "# Feature\n\n**Progress: made up**\n\n" + TABLE_HEAD
    body += "".join(f"| Row {index} | {status} | notes with a | pipe-free text |\n" for index, status in enumerate(rows))
    path.write_text(body, encoding="utf-8")


class ProgressMapTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.folder = Path(self.temp.name) / "progress"
        write(self.folder, "hand/grab.md", ["done", "done", "partial", "n/a"])
        write(self.folder, "hand/throw.md", ["todo", "**done**"])
        write(self.folder, "town/homes.md", ["done", "n/a"])
        write(self.folder, "platforms.md", ["partial"])
        write(self.folder, "empty/nothing.md", ["n/a", "n/a"])
        write(self.folder, "INDEX.md", ["done"] * 50)
        write(self.folder, "README.md", ["done"] * 50)

    def tearDown(self):
        self.temp.cleanup()

    def test_row_status(self):
        self.assertEqual(progress_map.row_status("| A thing | done | `src/A.cpp` |"), "done")
        self.assertEqual(progress_map.row_status("| A thing | N/A | why |"), "n/a")
        self.assertIsNone(progress_map.row_status("| Behaviour | Status | Where / notes |"))
        self.assertIsNone(progress_map.row_status("|-----------|--------|---------------|"))
        self.assertIsNone(progress_map.row_status("Text with done in it"))

    def test_read_progress_counts_each_domain(self):
        domains = {domain.name: domain for domain in progress_map.read_progress(self.folder)}
        self.assertEqual(sorted(domains), ["hand", "platforms", "town"])
        self.assertEqual(domains["hand"].counts, {"done": 3, "partial": 1, "todo": 1, "n/a": 1})
        self.assertEqual(domains["hand"].rows, 5)
        self.assertEqual(domains["town"].percent, 100)
        self.assertEqual(domains["platforms"].percent, 50)

    def test_score_is_never_100_when_work_remains(self):
        counts = {"done": 999, "partial": 0, "todo": 1, "n/a": 0}
        self.assertEqual(progress_map.score_percent(counts), 99)

    def test_squares_hold_each_status(self):
        domain = progress_map.Domain("hand", {"done": 5, "partial": 1, "todo": 2, "n/a": 9})
        self.assertEqual(progress_map.squares_for(domain, 1), ["done"] * 5 + ["partial"] + ["todo"] * 2)
        self.assertEqual(progress_map.squares_for(domain, 4), ["done", "done", "partial", "todo"])

    def test_svg_has_one_square_for_each_row(self):
        out = Path(self.temp.name) / "map.svg"
        self.assertEqual(progress_map.main([str(self.folder), "--out", str(out), "--rows-per-square", "1",
                                            "--date", "2026-01-02"]), 0)
        root = ElementTree.parse(out).getroot()
        namespace = "{http://www.w3.org/2000/svg}"
        squares = [rect for rect in root.iter(namespace + "rect") if rect.get("class", "").startswith("s")]
        # Seven rows that are not n/a, and one square for each of the three statuses in the legend
        self.assertEqual(len(squares), 7 + 3)
        text = " ".join(element.text for element in root.iter(namespace + "text"))
        self.assertIn("2026-01-02", text)
        self.assertIn("hand 70%", text)
        self.assertIn("One square is 1 row", text)

    def test_missing_folder_fails(self):
        with self.assertRaises(FileNotFoundError):
            progress_map.read_progress(Path(self.temp.name) / "missing")


if __name__ == "__main__":
    unittest.main()
