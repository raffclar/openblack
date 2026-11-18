#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the launch switch scan of the sim tools, and of the command lines they start the game with"""

import os
import subprocess
import sys
import tempfile
import unittest
import unittest.mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sim_paths  # noqa: E402


class LaunchSwitchScanTest(unittest.TestCase):
    def test_a_switch_after_a_dash_or_a_slash_counts_in_any_case(self):
        self.assertEqual(sim_paths.launch_switch_in("E:/builds/cg-skirmish-fights-1/bin"), "SKIRMISH")
        self.assertEqual(sim_paths.launch_switch_in("E:/runs/skirmish-3/log.txt"), "SKIRMISH")
        self.assertEqual(sim_paths.launch_switch_in("x -NewGame"), "NEWGAME")

    def test_a_backslash_and_a_double_dash_start_no_switch(self):
        self.assertIsNone(sim_paths.launch_switch_in("E:\builds\skirmish\bin"))
        self.assertIsNone(sim_paths.launch_switch_in("--land"))
        self.assertIsNone(sim_paths.launch_switch_in("E:/openblack/builds/testtools-gaps-1/bin/Debug"))


class SwitchSafeCommandTest(unittest.TestCase):
    def test_the_command_line_holds_only_the_program_name(self):
        exe = os.path.join("E:/openblack/builds/cg-skirmish-fights-1/bin/Debug", "openblack.exe")
        executable, arguments = sim_paths.switch_safe_command([exe, "--background"], os.getcwd())
        self.assertEqual(executable, exe)
        self.assertEqual(arguments, ["openblack.exe", "--background"])

    def test_a_path_with_a_switch_changes_to_a_safe_form(self):
        with tempfile.TemporaryDirectory() as root:
            run = os.path.join(root, "cg-skirmish-fights-1", "run1")
            os.makedirs(run)
            log = run.replace("\\", "/") + "/game.log"
            _, arguments = sim_paths.switch_safe_command(["openblack", "--log-file", log,
                                                         f"--profile-folder={run}/profile"], run)
            self.assertEqual(arguments[2], "game.log")
            self.assertEqual(arguments[3], "--profile-folder=profile")
            self.assertTrue(all(sim_paths.launch_switch_in(argument) is None for argument in arguments))

    def test_a_program_name_with_no_folder_changes_to_its_path(self):
        name = os.path.splitext(os.path.basename(sys.executable))[0]
        with unittest.mock.patch.dict(os.environ, {"PATH": os.path.dirname(sys.executable)}):
            executable, arguments = sim_paths.switch_safe_command([name, "--version"], os.getcwd())
        self.assertTrue(os.path.isabs(executable))
        self.assertTrue(os.path.exists(executable))
        self.assertEqual(arguments, [os.path.basename(executable), "--version"])

    def test_a_program_name_that_is_not_on_the_path_stays(self):
        executable, _ = sim_paths.switch_safe_command(["no-such-program-ob", "--version"], os.getcwd())
        self.assertEqual(executable, "no-such-program-ob")

    def test_arguments_that_are_not_paths_stay(self):
        _, arguments = sim_paths.switch_safe_command(["openblack", "-SKIRMISH", "--scenario", "a.b"], os.getcwd())
        self.assertEqual(arguments[1:], ["-SKIRMISH", "--scenario", "a.b"])

    @unittest.skipUnless(os.name == "nt", "the command line of the game is GetCommandLine on Windows only")
    def test_the_started_program_sees_the_safe_command_line(self):
        with tempfile.TemporaryDirectory() as root:
            run = os.path.join(root, "flow-skirmish-1")
            os.makedirs(run)
            script = "import ctypes;print(ctypes.windll.kernel32.GetCommandLineW())"
            executable, arguments = sim_paths.switch_safe_command([sys.executable, "-c", script], run)
            line = subprocess.run(arguments, executable=executable, cwd=run, capture_output=True, text=True,
                                  check=True).stdout
            self.assertNotIn(os.path.dirname(sys.executable), line)


if __name__ == "__main__":
    unittest.main()
