#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the rules of the AI soak runner and of the AI summary of the logs, with fake runs (no game starts)"""

import argparse
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ai_soak  # noqa: E402
import sim_report  # noqa: E402

try:
    import net_check  # noqa: E402
except ModuleNotFoundError:
    # net_check needs the cryptography package for the test tickets; the tests of its helpers wait for it
    net_check = None

SAME = {"same": True, "server_turns": 2000, "clients": [{"client": 1, "different": [], "max_turn_ms": 3.5},
                                                          {"client": 2, "different": [], "max_turn_ms": 4.0}],
        "ai": {"gods": [0, 1], "acted": False, "per_god": {"0": {"needs": {}, "hand": {}, "casts": {}},
                                                            "1": {"needs": {"ExpandTown": 2}, "hand": {"takes": 3},
                                                                  "casts": {"Heal": 1}}}}}

PART = {"same": False, "server_turns": 300,
        "clients": [{"client": 1, "different": [], "parts_against_server": None},
                    {"client": 2, "different": [212, 213, -1], "parts_against_server": ["ComputerPlayers"],
                     "rows_against_server": {"entities": [{"component": "ComputerPlayer"}]},
                     "draws_against_server": {"first": 5}}]}


def options(**kwargs):
    values = {"lands": "TwoGods", "seeds": "1", "ai_seeds": "1", "modes": "all-computer", "observers": 2,
              "web_client": False, "turns": 2000, "short": False, "night": False, "bin": None, "config": "RelWithDebInfo",
              "until_end": False, "shuffle_seed": None, "web_one_game": False}
    values.update(kwargs)
    return argparse.Namespace(**values)


class Matrix(unittest.TestCase):
    def test_the_matrix_is_lands_seeds_ai_seeds_modes_in_order(self):
        games = ai_soak.games_of(options(lands="TwoGods,ring32", seeds="1,2", ai_seeds="0,3", modes="all-computer,all-but-one"))
        self.assertEqual(len(games), 2 * 2 * 2 * 2)
        self.assertEqual((games[0].land, games[0].seed, games[0].ai_seed, games[0].mode), ("TwoGods", 1, 0, "all-computer"))
        self.assertEqual((games[1].land, games[1].mode), ("TwoGods", "all-but-one"))
        self.assertEqual(games[-1].land, "ring32")

    def test_every_land_plays_the_turns_and_the_observers_that_the_command_line_gives(self):
        games = ai_soak.games_of(options(lands="TwoGods,ring32", turns=3000, observers=3, until_end=True))
        self.assertEqual({(game.land, game.turns, game.observers, game.until_end) for game in games},
                         {("TwoGods", 3000, 3, True), ("ring32", 3000, 3, True)})

    def test_a_shuffle_seed_gives_the_same_other_order_each_time(self):
        order = [game.name for game in ai_soak.games_of(options(night=True, seeds="1,2,3", shuffle_seed=5))]
        again = [game.name for game in ai_soak.games_of(options(night=True, seeds="1,2,3", shuffle_seed=5))]
        plain = [game.name for game in ai_soak.games_of(options(night=True, seeds="1,2,3"))]
        self.assertEqual(order, again)
        self.assertNotEqual(order, plain)
        self.assertEqual(sorted(order), sorted(plain))

    def test_one_game_of_a_night_has_the_web_client(self):
        games = ai_soak.games_of(options(night=True, seeds="1,2", web_one_game=True))
        self.assertEqual(sum(game.web_client for game in games), 1)
        self.assertTrue(games[0].web_client)

    def test_a_part_adds_a_line_for_the_lead_to_the_queue_handover(self):
        row = {"game": "g", "started": "2026-10-26T01:00:00", "first_part_turn": 9, "parts": ["Villager"], "repro": "python x"}
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "queue.md")
            ai_soak.queue_line(path, row)
            ai_soak.queue_line(path, row)
            with open(path, encoding="utf-8") as file:
                lines = file.read().splitlines()
        self.assertEqual(len(lines), 2)
        self.assertIn("AI soak part", lines[0])
        self.assertIn("python x", lines[0])

    def test_a_wrong_command_line_is_refused_with_every_reason(self):
        with self.assertRaises(ValueError) as caught:
            ai_soak.games_of(options(lands="Nowhere", modes="never", observers=9, turns=0))
        text = str(caught.exception)
        for word in ("Nowhere", "never", "--observers", "--turns"):
            self.assertIn(word, text)

    def test_all_computer_needs_an_observer(self):
        with self.assertRaises(ValueError):
            ai_soak.games_of(options(observers=0))
        ai_soak.games_of(options(observers=0, web_client=True))
        ai_soak.games_of(options(observers=0, modes="all-but-one"))

    def test_the_short_soak_is_the_two_games_of_a_car(self):
        games = ai_soak.short_plan()
        self.assertEqual([(g.land, g.turns, g.observers, g.mode, g.seed, g.ai_seed) for g in games],
                         [("TwoGods", 2000, 2, "all-computer", 1, 1), ("ring32", 1200, 1, "all-computer", 1, 1)])
        self.assertEqual(ai_soak.games_of(options(short=True)), games)

    def test_the_night_has_every_land_of_the_list(self):
        games = ai_soak.games_of(options(night=True, turns=100))
        self.assertEqual([game.land for game in games], ai_soak.NIGHT_LANDS)


class Commands(unittest.TestCase):
    def test_an_all_computer_game_has_no_human_client(self):
        game = ai_soak.Game("TwoGods", 3, 5, "all-computer", 2, 2000)
        arguments = ai_soak.net_check_arguments(game, "out", None, "Debug")
        for word in ("--all-computer-gods", "--hash-parts"):
            self.assertIn(word, arguments)
        self.assertEqual(arguments[arguments.index("--ai-seed") + 1], "5")
        self.assertEqual(arguments[arguments.index("--observers") + 1], "2")
        self.assertNotIn("--clients", arguments)
        self.assertEqual(arguments[arguments.index("--land") + 1], "Playgrounds/TwoGods.txt")

    def test_a_game_with_no_ai_seed_keeps_the_built_in_personalities(self):
        game = ai_soak.Game("ring32", 1, 0, "all-but-one", 1, 1200)
        arguments = ai_soak.net_check_arguments(game, "out")
        self.assertNotIn("--ai-seed", arguments)
        self.assertIn("--gods-ring-map", arguments)
        self.assertEqual(arguments[arguments.index("--clients") + 1], "1")
        self.assertNotIn("--all-computer-gods", arguments)

    def test_the_replay_adds_the_tools_that_find_the_cause(self):
        extra = ai_soak.replay_extra(212)
        self.assertEqual(extra[extra.index("--row-dump-turn") + 1], "212")
        for word in ("--row-dump", "--trace-draws", "--hash-fields", "--frame-guard"):
            self.assertIn(word, extra)

    def test_the_repro_line_plays_the_same_game(self):
        game = ai_soak.Game("IslandWars", 2, 4, "all-computer", 1, 500)
        line = ai_soak.repro_line(game, "E:/builds/x", "Debug")
        parsed = ai_soak.build_parser().parse_args(line.replace('"', "").split()[2:])
        self.assertEqual(ai_soak.games_of(parsed), [game])
        self.assertEqual(parsed.bin, "E:/builds/x")


class Reports(unittest.TestCase):
    def test_a_run_that_stays_in_step_is_same(self):
        self.assertEqual(ai_soak.classify(0, SAME), "same")

    def test_a_different_turn_of_a_client_is_a_part_with_its_first_turn(self):
        self.assertEqual(ai_soak.classify(1, PART), "part")
        self.assertEqual(ai_soak.first_part(PART), (212, 2))
        self.assertEqual(ai_soak.part_names(PART), ["ComputerPlayers"])

    def test_a_missing_result_or_a_failed_run_with_no_part_is_an_error(self):
        self.assertEqual(ai_soak.classify(1, None), "error")
        self.assertEqual(ai_soak.classify(1, {"same": False, "clients": [{"different": []}]}), "error")
        self.assertEqual(ai_soak.classify(0, {"same": False, "clients": [{"different": [-1]}]}), "error")

    def test_the_counts_of_the_ai_add_up_over_the_gods(self):
        self.assertEqual(ai_soak.ai_counts(SAME), {"gods": 2, "needs": 2, "hand": 3, "casts": 1})
        self.assertEqual(ai_soak.ai_counts({}), {"gods": 0, "needs": 0, "hand": 0, "casts": 0})

    def test_the_row_of_a_part_has_the_turn_the_parts_and_the_repro_line(self):
        game = ai_soak.Game("TwoGods", 1, 1, "all-computer", 2, 300)
        row = ai_soak.summary_row(game, "part", PART, "2026-10-26T10:00:00", 100.0, 160.5, "python x", {"parts": ["a"]})
        self.assertEqual((row["first_part_turn"], row["parts"], row["repro"], row["seconds"]),
                         (212, ["ComputerPlayers"], "python x", 60.5))
        self.assertEqual(row["ai_seed"], 1)
        same = ai_soak.summary_row(game, "same", SAME, "t", 0.0, 1.0)
        self.assertIsNone(same["first_part_turn"])
        self.assertEqual(same["max_turn_ms"], [3.5, 4.0])
        self.assertNotIn("repro", same)


class Guarding(unittest.TestCase):
    def guard(self, cpu, memory, disk, later=None):
        values = {"cpu": cpu, "memory_gb": memory, "disk_gb": disk}
        probes = {name: (lambda name=name: values[name]) for name in values}
        clock = [0.0]
        waits = []

        def sleep(seconds):
            clock[0] += seconds
            waits.append(seconds)
            if later:
                values.update(later.pop(0))

        guard = ai_soak.Guard("out", probes, sleep=sleep, clock=lambda: clock[0], wait_limit=100, poll=10)
        return guard, waits

    def test_a_free_computer_allows_a_game(self):
        guard, waits = self.guard(40.0, 32.0, 500.0)
        self.assertEqual(guard.blockers(), [])
        guard.wait()
        self.assertEqual(waits, [])

    def test_each_limit_blocks_a_game_and_names_itself(self):
        for values, word in (((80.0, 32.0, 500.0), "processor"), ((10.0, 8.0, 500.0), "memory"),
                             ((10.0, 32.0, 100.0), "disk")):
            guard, _ = self.guard(*values)
            blockers = guard.blockers()
            self.assertEqual(len(blockers), 1)
            self.assertIn(word, blockers[0])

    def test_the_limits_are_the_rules_of_the_lead(self):
        self.assertEqual((ai_soak.CPU_LIMIT_PERCENT, ai_soak.FREE_MEMORY_GB, ai_soak.FREE_DISK_GB), (75.0, 16.0, 150.0))
        guard, _ = self.guard(74.9, 16.0, 150.0)
        self.assertEqual(guard.blockers(), [])

    def test_a_busy_computer_makes_the_runner_wait_until_it_is_free(self):
        guard, waits = self.guard(90.0, 32.0, 500.0, later=[{"cpu": 90.0}, {"cpu": 20.0}])
        guard.wait()
        self.assertEqual(waits, [10, 10])

    def test_a_computer_that_stays_busy_ends_the_wait_with_an_error(self):
        guard, _ = self.guard(99.0, 32.0, 500.0)
        with self.assertRaises(TimeoutError):
            guard.wait()


class Playing(unittest.TestCase):
    def runner_of(self, summaries):
        calls = []

        def runner(arguments, log, limit):
            calls.append(arguments)
            folder = arguments[arguments.index("--out") + 1]
            summary = summaries[min(len(calls) - 1, len(summaries) - 1)]
            if summary is not None:
                os.makedirs(folder, exist_ok=True)
                with open(os.path.join(folder, "net_check.json"), "w", encoding="utf-8") as file:
                    json.dump(summary, file)
            return 0 if summary and summary.get("same") else 1

        return runner, calls

    def test_a_game_that_stays_in_step_runs_once(self):
        game = ai_soak.Game("TwoGods", 1, 1, "all-computer", 2, 300)
        with tempfile.TemporaryDirectory() as day:
            runner, calls = self.runner_of([SAME])
            row = ai_soak.play(game, day, options(), runner=runner)
        self.assertEqual(row["status"], "same")
        self.assertEqual(len(calls), 1)

    def test_a_part_plays_again_with_the_tools_and_writes_the_repro_line(self):
        game = ai_soak.Game("TwoGods", 1, 1, "all-computer", 2, 300)
        with tempfile.TemporaryDirectory() as day:
            runner, calls = self.runner_of([PART, PART])
            row = ai_soak.play(game, day, options(), runner=runner)
            self.assertEqual(row["status"], "part")
            self.assertEqual(len(calls), 2)
            self.assertIn("--row-dump", calls[1])
            self.assertEqual(calls[1][calls[1].index("--row-dump-turn") + 1], "212")
            self.assertNotIn("--row-dump", calls[0])
            with open(os.path.join(day, game.name, "repro.txt"), encoding="utf-8") as file:
                text = file.read()
            self.assertTrue(text.startswith("python tools/sim/ai_soak.py"))
        self.assertEqual(row["first_part_turn"], 212)
        self.assertEqual(row["replay"]["first_part_turn"], 212)
        self.assertEqual(row["replay"]["rows"], {"entities": [{"component": "ComputerPlayer"}]})

    def test_a_run_with_no_result_is_an_error_and_is_not_played_again(self):
        game = ai_soak.Game("TwoGods", 1, 1, "all-computer", 2, 300)
        with tempfile.TemporaryDirectory() as day:
            runner, calls = self.runner_of([None])
            row = ai_soak.play(game, day, options(), runner=runner)
        self.assertEqual(row["status"], "error")
        self.assertEqual(len(calls), 1)

    def test_the_injected_part_adds_a_second_god_that_stays_apart(self):
        game = ai_soak.Game("TwoGods", 1, 0, "all-but-one", 0, 160)
        with tempfile.TemporaryDirectory() as day:
            runner, calls = self.runner_of([PART, PART])
            ai_soak.play(game, day, options(), runner=runner, inject=40)
        first = calls[0]
        self.assertEqual(first[first.index("--part-second-at") + 1], "40")
        self.assertIn("--part-second-again", first)
        # The later --clients wins over the first: two gods
        self.assertEqual(first[len(first) - 1 - first[::-1].index("--clients") + 1], "2")

    def test_the_web_page_goes_to_net_check_only_for_a_game_with_the_web_client(self):
        with tempfile.TemporaryDirectory() as day:
            runner, calls = self.runner_of([SAME])
            ai_soak.play(ai_soak.Game("TwoGods", 1, 1, "all-computer", 1, 100, web_client=True), day,
                         options(web_page="E:/web/page"), runner=runner)
            ai_soak.play(ai_soak.Game("TwoGods", 1, 1, "all-computer", 1, 100), day, options(web_page="E:/web/page"),
                         runner=runner)
        self.assertEqual(calls[0][calls[0].index("--web-page") + 1], "E:/web/page")
        self.assertIn("--web-client", calls[0])
        self.assertNotIn("--web-page", calls[1])

    def test_the_rows_go_to_one_file_a_line_each(self):
        with tempfile.TemporaryDirectory() as day:
            ai_soak.append_row(day, {"a": 1})
            ai_soak.append_row(day, {"a": 2})
            with open(os.path.join(day, "summary.jsonl"), encoding="utf-8") as file:
                self.assertEqual([json.loads(line)["a"] for line in file], [1, 2])


@unittest.skipIf(net_check is None, "net_check needs the cryptography package")
class NetCheckHelpers(unittest.TestCase):
    def test_a_long_game_gets_a_longer_limit_than_a_short_one(self):
        self.assertEqual(net_check.run_limit_seconds(600), net_check.RUN_LIMIT_SECONDS)
        self.assertEqual(net_check.run_limit_seconds(2000), net_check.RUN_LIMIT_SECONDS)
        self.assertGreater(net_check.run_limit_seconds(18000), 3000)
        self.assertEqual(net_check.MOST_TURNS, 18000)

    def test_the_longest_turn_comes_from_the_record(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "record.jsonl")
            self.assertIsNone(net_check.max_turn_ms(path))
            with open(path, "w", encoding="utf-8") as file:
                file.write(json.dumps({"turn": 1, "turn_ms": 2.5}) + "\n")
                file.write("not json\n")
                file.write(json.dumps({"turn": 2, "turn_ms": 1.0, "turn_ms_max": 9.0}) + "\n")
                file.write(json.dumps({"turn": 3, "other": 1}) + "\n")
            self.assertEqual(net_check.max_turn_ms(path), 9.0)

    def test_the_web_client_checksums_are_read_by_turn(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(net_check.web_checksums(folder), {})
            with open(os.path.join(folder, "checksums.jsonl"), "w", encoding="utf-8") as file:
                file.write(json.dumps({"turn": 5, "checksum": "00ab"}) + "\n")
                file.write("noise\n")
                file.write(json.dumps({"turn": 6}) + "\n")
            self.assertEqual(net_check.web_checksums(folder), {5: "00ab"})

    def test_chrome_gets_the_key_of_the_test_certificate(self):
        with tempfile.TemporaryDirectory() as folder:
            certs = net_check.make_certificates(folder)
            first = net_check.spki_hash(certs["server.pem"])
            self.assertEqual(len(first), 44)
            self.assertEqual(first, net_check.spki_hash(certs["server.pem"]))
            self.assertNotEqual(first, net_check.spki_hash(certs["ca.pem"]))

    def test_the_observers_of_a_scenario_join_before_the_gods(self):
        # The state of a scenario is not in the world, so its observers must be in the room before the start
        self.assertEqual(net_check.join_order(2, 3, True), [2, 0, 1])
        self.assertEqual(net_check.join_order(2, 4, True), [2, 3, 0, 1])
        self.assertEqual(net_check.join_order(2, 2, True), [0, 1])
        self.assertEqual(net_check.join_order(2, 3, False), [0, 1, 2])

    def test_the_web_page_defaults_to_the_web_build_of_the_tree(self):
        page = net_check.web_page_of(argparse.Namespace(web_page=None))
        self.assertTrue(page.replace("\\", "/").endswith("-web/bin/Release"))
        self.assertEqual(net_check.web_page_of(argparse.Namespace(web_page="E:/x")), "E:/x")

    def test_the_tickets_of_observers_say_observer(self):
        with tempfile.TemporaryDirectory() as folder:
            key, kid, _ = net_check.make_ticket_key(folder)
        token = net_check.make_ticket(key, kid, 7, "Watcher", "observer", 60)
        self.assertTrue(token.startswith("v4.public."))
        payload = token.split(".")[2]
        padded = payload + "=" * (-len(payload) % 4)
        message = json.loads(net_check.base64.urlsafe_b64decode(padded)[:-64])
        self.assertEqual(message["role"], "observer")


class Tables(unittest.TestCase):
    ROWS = [{"game": "a", "status": "same", "started": "2026-10-25T23:00:00", "turns_played": 2000, "first_part_turn": None,
             "ai": {"gods": 2, "needs": 5, "hand": 1, "casts": 0}, "max_turn_ms": [3.0, 4.25]},
            {"game": "b", "status": "part", "started": "2026-10-26T01:00:00", "turns_played": 300, "first_part_turn": 212,
             "ai": {"gods": 7, "needs": 0, "hand": 0, "casts": 0}, "max_turn_ms": [None]}]

    def test_the_table_has_a_line_for_each_game(self):
        lines = ai_soak.table_lines(self.ROWS)
        self.assertEqual(len(lines), 4)
        self.assertIn("| a | same | 2000 | - | 2 / 5 / 1 / 0 | 4.2 |", lines[2])
        self.assertIn("| b | part | 300 | 212 | 7 / 0 / 0 / 0 | - |", lines[3])

    def test_the_rows_of_a_file_can_be_cut_by_the_date(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "summary.jsonl")
            with open(path, "w", encoding="utf-8") as file:
                for row in self.ROWS:
                    file.write(json.dumps(row) + "\n")
                file.write("not json\n")
            self.assertEqual(len(ai_soak.read_rows(path)), 2)
            self.assertEqual([row["game"] for row in ai_soak.read_rows(path, "2026-10-26")], ["b"])
            self.assertEqual(ai_soak.read_rows(os.path.join(folder, "none.jsonl")), [])


class AiLog(unittest.TestCase):
    LINES = [
        "[game] Made creature 12 of species 3 for the computer god 1 at 10.0, 20.0",
        "[game] The computer god 2 starts with the personality of AI seed 7",
        "[game] Computer god 1 chooses need ExpandTown",
        "[game] Computer god 1 chooses need ExpandTown",
        "[game] Computer god 1 chooses need TeachCreature",
        "[game] Computer god 2 casts Heal",
        "[game] Hand deed: the hand of player 1 takes thing 5",
        "[game] Hand deed: the hand of player 1 throws thing 5",
        "[game] Hand deed: the hand of player 0 takes thing 9",
    ]

    def test_the_log_tells_what_each_computer_god_did(self):
        result = sim_report.ai_actions_in_text(self.LINES)
        self.assertEqual(result["gods"], [1, 2])
        self.assertTrue(result["acted"])
        self.assertEqual(result["per_god"]["1"]["needs"], {"ExpandTown": 2, "TeachCreature": 1})
        self.assertEqual(result["per_god"]["1"]["hand"], {"takes": 1, "throws": 1})
        self.assertEqual(result["per_god"]["2"]["casts"], {"Heal": 1})

    def test_the_hand_of_a_human_god_is_not_the_work_of_the_mind(self):
        result = sim_report.ai_actions_in_text(self.LINES)
        self.assertNotIn("0", result["per_god"])

    def test_gods_that_did_nothing_are_listed_and_the_run_did_not_act(self):
        result = sim_report.ai_actions_in_text(self.LINES[:2])
        self.assertEqual(result["gods"], [1, 2])
        self.assertFalse(result["acted"])
        self.assertEqual(result["per_god"]["1"], {"needs": {}, "hand": {}, "casts": {}})

    def test_a_missing_log_tells_of_no_god(self):
        self.assertEqual(sim_report.ai_actions_in_file("no/such/file.log")["gods"], [])


if __name__ == "__main__":
    unittest.main()
