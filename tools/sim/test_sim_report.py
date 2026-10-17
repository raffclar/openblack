#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the summary and the comparison of simulation runs, on records made here"""

import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_report  # noqa: E402


def record(turn, stage=0, flags=None, hash_value="a", state="s", turn_ms=1.0, plan="Eat"):
    return {
        "type": "record", "turn": turn, "turns": 100, "turn_ms": turn_ms, "turn_ms_max": turn_ms * 2,
        "frame_ms": 0.5, "frame_ms_max": 1.0, "memory_mb": 100.0 + turn / 1000, "entities": 10, "hash": hash_value,
        "flags": flags or [], "wall_s": turn / 100,
        "creature": {"stage": stage, "age": turn // 1000, "size": 1.0, "hunger": 0.5, "tiredness": 0.1,
                     "health": 1.0, "alignment": 0.0, "plan": {"action": plan}, "activity": "Acting on a desire",
                     "beliefs": {"count": 3}, "state_hash": state},
    }


def lines(items):
    return [json.dumps(item) for item in items]


class ReadTests(unittest.TestCase):
    def test_a_cut_last_line_is_left_out(self):
        text = lines([{"type": "start", "seed": 4}, record(100)]) + ['{"type": "rec']
        start, records, end = sim_report.read_records(text)
        self.assertEqual(start["seed"], 4)
        self.assertEqual(len(records), 1)
        self.assertIsNone(end)


class SummaryTests(unittest.TestCase):
    def test_a_clean_run_is_stable(self):
        start = {"type": "start", "seed": 1, "fast": True, "frame_ms": 20, "every": 100}
        records = [record(100), record(200, stage=1), record(300, stage=1)]
        end = {"type": "end", "turn": 300, "wall_s": 2.0, "longest": [{"key": "plan:Eat", "turns": 200, "limit": None}]}
        result = sim_report.summary(start, records, end, ["[info] all well"], 0)
        self.assertEqual(result["verdict"], "stable")
        self.assertEqual(result["stages"], {"0": 100, "1": 200})
        self.assertAlmostEqual(result["turns_per_second"], 100.0)
        self.assertAlmostEqual(result["times_real_time"], 10.0)
        self.assertEqual(result["plans"], {"Eat": 3})
        self.assertIn("Verdict: **stable**", sim_report.markdown(result))

    def test_flags_errors_and_crashes_are_problems(self):
        flag = {"kind": "growth", "what": "list:beliefs", "turn": 200, "detail": "grew"}
        records = [record(100), record(200, flags=[flag])]
        end = {"type": "end", "turn": 200, "wall_s": 1.0}
        self.assertEqual(sim_report.summary(None, records, end, [], 0)["verdict"], "problems")
        clean = [record(100), record(200)]
        log = ["[2026] [game] [error] the file is missing"]
        result = sim_report.summary(None, clean, end, log, 0)
        self.assertEqual(result["log"]["counts"], {"error": 1})
        self.assertEqual(result["verdict"], "problems")
        self.assertEqual(sim_report.summary(None, clean, end, [], 3)["verdict"], "problems")
        self.assertEqual(sim_report.summary(None, clean, None, [], 0)["verdict"], "problems")

    def test_asserts_count_at_any_level(self):
        log = ["[warning] Assertion failed: x", "[warning] the creature is hungry"]
        self.assertEqual(sim_report.scan_log(log)["counts"], {"assert": 1})


class CompareTests(unittest.TestCase):
    def test_the_same_runs_agree(self):
        first = [record(100), record(200)]
        self.assertEqual(sim_report.compare(first, [record(100), record(200)]),
                         {"same": True, "compared": 2, "turn": None})

    def test_the_first_turn_that_differs_is_found(self):
        first = [record(100), record(200), record(300)]
        second = [record(100), record(200, hash_value="b"), record(300, state="t")]
        second[1]["hash_parts"] = {"Villager": "1"}
        first[1]["hash_parts"] = {"Villager": "2"}
        result = sim_report.compare(first, second)
        self.assertFalse(result["same"])
        self.assertEqual(result["turn"], 200)
        self.assertTrue(result["creature_same"])
        self.assertEqual(result["parts"], ["Villager"])

    def test_runs_with_no_turn_in_common_do_not_agree(self):
        self.assertFalse(sim_report.compare([record(100)], [record(200)])["same"])


if __name__ == "__main__":
    unittest.main()
