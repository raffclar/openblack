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

    def test_runs_with_other_draws_do_not_agree(self):
        first = [record(100), record(200)]
        second = [record(100), record(200)]
        for item in first + second:
            item["seeds"], item["draws"] = [7, 9], item["turn"] * 3
        self.assertTrue(sim_report.compare(first, second)["same"])
        second[1]["seeds"] = [8, 9]
        result = sim_report.compare(first, second)
        self.assertFalse(result["same"])
        self.assertEqual(result["turn"], 200)
        self.assertFalse(result["draws_same"])
        second[1]["seeds"], second[1]["draws"] = [7, 9], 601
        self.assertEqual(sim_report.compare(first, second)["draws"], [600, 601])

    def test_the_first_record_that_differs_names_its_fields(self):
        first = [record(100), record(200)]
        second = [record(100), record(200)]
        for item in first + second:
            item["seeds"], item["draws"], item["counts"] = [7, 9], 600, {"Villager": 3}
        second[1]["seeds"] = [7, 10]
        second[1]["counts"] = {"Villager": 4}
        result = sim_report.compare(first, second)
        self.assertEqual(result["differs"], ["counts", "seeds.local"])
        self.assertEqual(result["seeds"], [[7, 9], [7, 10]])
        self.assertEqual(result["counts"], {"Villager": [3, 4]})

    def test_runs_with_no_turn_in_common_do_not_agree(self):
        self.assertFalse(sim_report.compare([record(100)], [record(200)])["same"])


def synced_record(turn, hash_value="s", local=9, drawn="d", parts=None):
    synced = {"hash": hash_value, "seed": 7, "draws": turn * 3}
    if parts is not None:
        synced["parts"] = parts
    return {"type": "record", "turn": turn, "hash": drawn, "seeds": [7, local], "synced": synced}


class CompareSyncedTests(unittest.TestCase):
    def test_other_local_draws_and_drawn_places_agree(self):
        first = [synced_record(1), synced_record(2)]
        other = [synced_record(1, local=4, drawn="x"), synced_record(2, local=5, drawn="y")]
        self.assertEqual(sim_report.compare_synced(first, other), {"same": True, "compared": 2, "turn": None})

    def test_the_first_synced_world_that_differs_names_its_parts(self):
        first = [synced_record(1, parts={"Villager": "1", "Tree": "2"}), synced_record(2, parts={"Villager": "1"})]
        other = [synced_record(1, parts={"Villager": "1", "Tree": "2"}),
                 synced_record(2, hash_value="t", parts={"Villager": "3"})]
        result = sim_report.compare_synced(first, other)
        self.assertFalse(result["same"])
        self.assertEqual(result["turn"], 2)
        self.assertEqual(result["parts"], ["Villager"])

    def test_records_without_the_synced_world_do_not_agree(self):
        plain = {"type": "record", "turn": 1, "hash": "a"}
        self.assertFalse(sim_report.compare_synced([plain], [plain])["same"])


class CompareSavesTests(unittest.TestCase):
    def test_the_same_saves_agree(self):
        data = b"headECSW1234RCTX5678"
        self.assertEqual(sim_report.compare_saves(data, data), {})

    def test_each_block_counts_its_bytes_that_differ(self):
        first = b"headECSW1234RCTX5678"
        second = b"headECSW1934RCTX5600"
        self.assertEqual(sim_report.compare_saves(first, second), {"ECSW": 1, "RCTX": 2})

    def test_saves_of_other_sizes_say_so(self):
        self.assertEqual(sim_report.compare_saves(b"ECSW12", b"ECSW123"), {"size": [6, 7]})


class UnregisteredTests(unittest.TestCase):
    def test_a_log_line_gives_its_names(self):
        line = "[2026-10-25 12:00:00] [game] [error] unregistered component storage: Alpha, Beta"
        self.assertEqual(sim_report.unregistered_in_text([line]), {"Alpha", "Beta"})

    def test_a_record_gives_the_names_of_its_synced_part(self):
        line = json.dumps({"type": "record", "turn": 5, "synced": {"hash": "a", "unregistered": ["Alpha"]}})
        self.assertEqual(sim_report.unregistered_in_text([line]), {"Alpha"})

    def test_a_checksum_line_gives_its_names(self):
        line = '{"turn":1,"checksum":"0000000000000002","unregistered":["Alpha","Beta"]}'
        self.assertEqual(sim_report.unregistered_in_text([line]), {"Alpha", "Beta"})

    def test_a_flag_that_quotes_the_message_is_not_read_as_names(self):
        flag = {"kind": "Log", "what": "unregistered component storage: Alpha", "detail": "x"}
        line = json.dumps({"type": "record", "turn": 5, "flags": [flag]})
        self.assertEqual(sim_report.unregistered_in_text([line]), set())

    def test_a_clean_run_has_none(self):
        self.assertEqual(sim_report.unregistered_in_text(["[info] nothing", json.dumps(record(1))]), set())

    def test_a_missing_file_has_none(self):
        self.assertEqual(sim_report.unregistered_in_files("no/such/file.log", None), set())

    def test_the_summary_is_a_problem_when_a_log_has_a_name(self):
        result = sim_report.summary({}, [record(10)], {"turn": 10}, ["[error] unregistered component storage: Alpha"], 0)
        self.assertEqual(result["unregistered"], ["Alpha"])
        self.assertEqual(result["verdict"], "problems")
        self.assertIn("unregistered component storage: Alpha", sim_report.markdown(result))

    def test_the_line_has_the_sorted_names(self):
        self.assertEqual(sim_report.unregistered_line({"Beta", "Alpha"}), "unregistered component storage: Alpha, Beta")


if __name__ == "__main__":
    unittest.main()
