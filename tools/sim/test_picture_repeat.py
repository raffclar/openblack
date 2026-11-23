#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the frames, the command line and the comparison of the picture repeat check"""

import argparse
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import picture_repeat  # noqa: E402


class FrameTests(unittest.TestCase):
    def test_the_frames_come_once_each_in_order(self):
        self.assertEqual(picture_repeat.parse_frames("600, 300,600"), [300, 600])

    def test_a_frame_before_the_first_is_refused(self):
        with self.assertRaises(ValueError):
            picture_repeat.parse_frames("0")


class ArgumentTests(unittest.TestCase):
    def test_a_run_has_the_seed_the_fixed_frame_time_and_an_end(self):
        options = argparse.Namespace(exe="game.exe", game_path="black", seed=1, frame_ms=20.0,
                                     extra=["--scenario", "multiplayer.gods_ring_map"])
        args = picture_repeat.game_arguments(options, 300, "p.png", "p.log", "p.profile")
        self.assertEqual(args[0], "game.exe")
        self.assertEqual(args[args.index("--seed") + 1], "1")
        self.assertEqual(args[args.index("--sim-frame-ms") + 1], "20.0")
        self.assertEqual(args[args.index("--screenshot-frame") + 1], "300")
        self.assertEqual(args[args.index("-n") + 1], str(300 + picture_repeat.FRAMES_AFTER_PICTURE))
        self.assertIn("--background", args)
        self.assertEqual(args[-2:], ["--scenario", "multiplayer.gods_ring_map"])


class CompareTests(unittest.TestCase):
    def test_the_same_bytes_are_the_same_picture(self):
        with tempfile.TemporaryDirectory() as folder:
            paths = [os.path.join(folder, name) for name in ("a.png", "b.png", "c.png")]
            for path, data in zip(paths, (b"one", b"one", b"two")):
                with open(path, "wb") as file:
                    file.write(data)
            self.assertEqual(picture_repeat.compare(paths[0], paths[1]), "same")
            self.assertEqual(picture_repeat.compare(paths[0], paths[2]), "differs")
            self.assertEqual(picture_repeat.compare(paths[0], os.path.join(folder, "none.png")), "missing")


if __name__ == "__main__":
    unittest.main()
