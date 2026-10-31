#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the feedback that the training style of the simulation driver gives, and of its repeat comparison"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_run  # noqa: E402


def context(desire="Play", change=0.0, kind=None, learnable=True, running=False):
    return {"action": "Act", "desire": desire, "alignment_change": change, "object_kind": kind,
            "learnable": learnable, "running": running, "done": True, "seconds_since": 0.0}


class TrainingFeedbackTests(unittest.TestCase):
    def test_good_desire_is_stroked(self):
        self.assertEqual(sim_run.training_feedback(context("Compassion", 1.0)), "stroke")

    def test_evil_desire_is_slapped(self):
        self.assertEqual(sim_run.training_feedback(context("Anger", -0.2)), "slap")

    def test_desire_with_no_change_gets_nothing(self):
        self.assertIsNone(sim_run.training_feedback(context("Play", 0.0)))

    def test_eaten_villager_is_slapped(self):
        self.assertEqual(sim_run.training_feedback(context("Hunger", 0.0, sim_run.VILLAGER_BELIEF)), "slap")

    def test_eaten_animal_gets_nothing(self):
        self.assertIsNone(sim_run.training_feedback(context("Hunger", 0.0, 7)))

    def test_running_or_unlearnable_gets_nothing(self):
        self.assertIsNone(sim_run.training_feedback(context("Compassion", 1.0, running=True)))
        self.assertIsNone(sim_run.training_feedback(context("Compassion", 1.0, learnable=False)))
        self.assertIsNone(sim_run.training_feedback(None))


def repeat_record(turn, local=9, draws=None):
    return {"type": "record", "turn": turn, "hash": "a", "seeds": [7, local], "draws": draws or turn * 3}


class RepeatTests(unittest.TestCase):
    def test_a_repeat_that_agrees_gives_its_last_draws(self):
        result = sim_run.repeat_result([repeat_record(1), repeat_record(2)], [repeat_record(1), repeat_record(2)])
        self.assertTrue(result["same"])
        self.assertEqual(result["records"], [2, 2])
        self.assertEqual(result["draws_last"], [6, 6])

    def test_a_repeat_that_parts_gives_its_first_record_that_differs(self):
        other = [repeat_record(1), repeat_record(2, local=10), repeat_record(3, draws=10)]
        result = sim_run.repeat_result([repeat_record(1), repeat_record(2), repeat_record(3)], other)
        self.assertFalse(result["same"])
        self.assertEqual(result["turn"], 2)
        self.assertEqual(result["differs"], ["seeds.local"])
        self.assertEqual(result["draws_last"], [9, 10])


class LoadReplayTests(unittest.TestCase):
    def test_the_first_run_writes_the_commands_and_the_others_play_them(self):
        self.assertEqual(sim_run.replay_arguments(1, "c.jsonl"), ["--sim-commands", "c.jsonl"])
        self.assertEqual(sim_run.replay_arguments(2, "c.jsonl"), ["--sim-play-commands", "c.jsonl"])
        self.assertEqual(sim_run.replay_arguments(5, "c.jsonl"), ["--sim-play-commands", "c.jsonl"])


class FramesTests(unittest.TestCase):
    def test_each_item_gives_the_frame_time_and_the_draw_every(self):
        self.assertEqual(sim_run.parse_frames("10, 17,20:1"), [(10, 0), (17, 0), (20, 1)])

    def test_no_list_is_no_frame_rate_repeat(self):
        self.assertEqual(sim_run.parse_frames(None), [])
        self.assertEqual(sim_run.parse_frames(""), [])

    def test_a_frame_rate_repeat_compares_only_the_synced_worlds(self):
        first = [{"type": "record", "turn": 1, "hash": "a", "seeds": [7, 1], "draws": 3,
                  "synced": {"hash": "s", "seed": 7, "draws": 3}}]
        other = [{"type": "record", "turn": 1, "hash": "b", "seeds": [7, 2], "draws": 3,
                  "synced": {"hash": "s", "seed": 7, "draws": 3}}]
        self.assertTrue(sim_run.repeat_result(first, other, synced=True)["same"])
        self.assertFalse(sim_run.repeat_result(first, other)["same"])


if __name__ == "__main__":
    unittest.main()
