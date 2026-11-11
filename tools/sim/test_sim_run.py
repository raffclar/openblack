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

import argparse
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


class FakeGame:
    """A game that gives one sample, and strokes and slaps that always count"""

    def __init__(self, creature):
        self.creature = creature
        self.calls = []

    def call(self, query, params=None):
        self.calls.append(query)
        if query == "sim.sample":
            return {"creature": self.creature}
        if query == "sim.feedback":
            kind = (params or {}).get("kind")
            return {"did": {"stroke": "stroked", "slap": "slapped"}.get(kind)}
        return {}

    def step_frames(self, frames):
        return True


class TrainingStyleTests(unittest.TestCase):
    def test_the_last_finished_thing_is_judged_once(self):
        finished = dict(context("Compassion", 1.0), object=12)
        game = FakeGame({"id": 5, "context": context("Rest", 0.0, running=True), "last_finished": finished})
        state = {}
        self.assertEqual(sim_run.act(game, "training", state), "stroke")
        # The same thing, later, with more seconds since it ended: no second stroke
        game.creature["last_finished"] = dict(finished, seconds_since=3.0)
        self.assertIsNone(sim_run.act(game, "training", state))

    def test_an_evil_finished_thing_is_slapped(self):
        game = FakeGame({"id": 5, "last_finished": dict(context("Anger", -0.2), object=3)})
        self.assertEqual(sim_run.act(game, "training", {}), "slap")

    def test_no_finished_thing_gets_nothing(self):
        game = FakeGame({"id": 5, "context": context("Compassion", 1.0, running=True), "last_finished": None})
        self.assertIsNone(sim_run.act(game, "training", {}))
        self.assertNotIn("sim.feedback", game.calls)

    def test_the_name_of_a_thing_leaves_out_its_seconds(self):
        first = dict(context("Play"), object=1)
        self.assertEqual(sim_run.training_key(first), sim_run.training_key(dict(first, seconds_since=9.0)))
        self.assertIsNone(sim_run.training_key(None))


class BusyGame(FakeGame):
    """A game whose creature refuses the first touches, as a body that is not far enough through its action does"""

    def __init__(self, creature, refusals):
        super().__init__(creature)
        self.refusals = refusals
        self.frames = 0
        self.feedback = []

    def call(self, query, params=None):
        if query == "sim.feedback" and (params or {}).get("kind") != "release":
            self.feedback.append(params)
            if self.refusals > 0:
                self.refusals -= 1
                return {"did": "busy"}
        return super().call(query, params)

    def step_frames(self, frames):
        self.frames += frames
        return True


class TouchTests(unittest.TestCase):
    def test_each_of_the_three_strokes_plays_when_the_body_is_busy_at_first(self):
        game = BusyGame({"id": 5}, refusals=4)
        self.assertEqual(sim_run.stroke(game, 5), 3)
        self.assertEqual([p.get("part") for p in game.feedback], [3, 3, 3, 3, 3, 0, 4])

    def test_a_stroke_that_the_body_always_refuses_stops_after_the_last_try(self):
        game = BusyGame({"id": 5}, refusals=10 ** 6)
        self.assertEqual(sim_run.stroke(game, 5), 0)
        tries = sim_run.TOUCH_GIVE_UP_FRAMES // sim_run.TOUCH_RETRY_FRAMES + 1
        self.assertEqual(len(game.feedback), 3 * tries)

    def test_one_slap_input_gives_one_slap(self):
        game = BusyGame({"id": 5}, refusals=2)
        self.assertTrue(sim_run.slap(game, 5))
        self.assertEqual(len(game.feedback), 3)

    def test_a_slap_on_a_ready_body_is_tried_once(self):
        game = BusyGame({"id": 5}, refusals=0)
        self.assertTrue(sim_run.slap(game, 5))
        self.assertEqual(len(game.feedback), 1)


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

    def test_a_frame_rate_repeat_plays_the_commands_of_the_first_run(self):
        self.assertEqual(sim_run.repeat_extra(1, ["--x"], "c.jsonl", True, None), ["--x", "--sim-commands", "c.jsonl"])
        self.assertEqual(sim_run.repeat_extra(3, [], "c.jsonl", True, None), ["--sim-play-commands", "c.jsonl"])

    def test_a_load_check_plays_the_commands_of_the_first_run(self):
        self.assertEqual(sim_run.repeat_extra(2, [], "c.jsonl", False, 700), ["--sim-play-commands", "c.jsonl"])

    def test_a_plain_repeat_keeps_the_input_of_each_run(self):
        self.assertEqual(sim_run.repeat_extra(2, ["--x"], "c.jsonl", False, None), ["--x"])


class WorldSeedTests(unittest.TestCase):
    @staticmethod
    def options(**changes):
        values = {"game_path": "C:/black", "seed": 1, "every": 200, "frame_ms": 20, "draw_every": 0, "limits": None,
                  "hash_parts": False, "scenario": None, "testbed": False, "land": "Land1.txt", "opening": "cow",
                  "extra": []}
        values.update(changes)
        return argparse.Namespace(**values)

    def test_a_world_seed_goes_to_the_game(self):
        args = sim_run.game_arguments(self.options(world_seed=7), "r.jsonl", "r.log", 100, True, True)
        self.assertEqual(args[args.index("--sim-world-seed") + 1], "7")

    def test_with_no_world_seed_the_game_keeps_its_zero(self):
        args = sim_run.game_arguments(self.options(), "r.jsonl", "r.log", 100, True, True)
        self.assertNotIn("--sim-world-seed", args)


class LoadPartingTests(unittest.TestCase):
    def test_a_help_text_that_is_not_read_parts_the_runs(self):
        dialogue = {"text": 662, "started": True, "text_read": False, "owner": 70, "start_turn": 169, "end_turn": 217}
        self.assertEqual(sim_run.load_partings(dialogue, []),
                         [{"rule": "help_text", "text": 662, "owner": 70, "start_turn": 169, "end_turn": 217}])

    def test_a_read_text_or_an_empty_box_does_not_part_the_runs(self):
        self.assertEqual(sim_run.load_partings({"text": 662, "started": True, "text_read": True}, []), [])
        self.assertEqual(sim_run.load_partings({"text": 0, "started": False, "text_read": False}, []), [])
        self.assertEqual(sim_run.load_partings(None, []), [])

    def test_a_computer_god_that_plays_parts_the_runs(self):
        self.assertEqual(sim_run.load_partings({}, [6, 5]), [{"rule": "computer_gods", "gods": [5, 6]}])


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
