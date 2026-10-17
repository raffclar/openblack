#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the feedback that the training style of the simulation driver gives"""

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


if __name__ == "__main__":
    unittest.main()
