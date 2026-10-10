#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the inspector adapter's choice of game, against fake games on loopback ports and fake discovery files,
of this build's protocol and of older games' (which don't name themselves in answers nor say whether they are ready)"""

import json
import os
import shutil
import socket
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import openblack_inspector_mcp as mcp  # noqa: E402


class FakeGame:
    """A loopback server answering as a game's inspector does: ping with its identity, anything else with its name.
    It counts the clients that sent something other than a ping, as the game's input lock does.

    old: answers as a game built before answers named their game and before the ready state (no "game", no "ready").
    loading: how many requests (other than pings) are answered "loading" first; pings say not ready meanwhile.
    delay: seconds before each answer other than a ping, as an older game busy loading a land.
    claims: the pid its answers name, when it isn't its own (a crossed answer)."""

    def __init__(self, pid, name, old=False, loading=0, delay=0.0, claims=None, worktree=None):
        self.pid = pid
        self.name = name
        self.old = old
        self.loading = loading
        self.delay = delay
        self.claims = claims
        self.worktree = worktree
        self.requests = []
        # Answers of particular queries: query -> function(request) -> result, or an error string
        self.handlers = {}
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(8)
        self.port = self.listener.getsockname()[1]
        self.controlling = set()
        self.lock = threading.Lock()
        self.running = True
        threading.Thread(target=self._accept, daemon=True).start()

    def close(self):
        self.running = False
        self.listener.close()

    def controlling_clients(self):
        with self.lock:
            return len(self.controlling)

    def asked(self):
        with self.lock:
            return list(self.requests)

    def _accept(self):
        while self.running:
            try:
                client, _ = self.listener.accept()
            except OSError:
                return
            threading.Thread(target=self._serve, args=(client,), daemon=True).start()

    def _answer(self, client, request):
        if request["query"] == "ping":
            result = {"pong": True, "pid": self.pid, "port": self.port}
            if self.worktree is not None:
                result["worktree"] = self.worktree
            if not self.old:
                result["ready"] = self.loading == 0
                if self.loading:
                    result["loading"] = "Land1.txt"
            answer = {"id": request.get("id"), "ok": True, "result": result}
        else:
            with self.lock:
                self.controlling.add(id(client))
                self.requests.append(request)
            time.sleep(self.delay)
            if self.loading > 0:
                self.loading -= 1
                answer = {"id": request.get("id"), "ok": False, "error": "the game is loading",
                          "loading": "Land1.txt"}
            elif request["query"] in self.handlers:
                result = self.handlers[request["query"]](request)
                answer = {"id": request.get("id"), "ok": not isinstance(result, str)}
                answer["error" if isinstance(result, str) else "result"] = result
            else:
                answer = {"id": request.get("id"), "ok": True, "result": {"who": self.name,
                                                                           "query": request["query"]}}
        if not self.old:
            answer["game"] = {"pid": self.claims or self.pid, "port": self.port}
        client.sendall((json.dumps(answer) + "\n").encode())

    def _serve(self, client):
        buffer = b""
        with client:
            while True:
                try:
                    chunk = client.recv(65536)
                except OSError:
                    break
                if not chunk:
                    break
                buffer += chunk
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    try:
                        self._answer(client, json.loads(line))
                    except OSError:
                        break
        with self.lock:
            self.controlling.discard(id(client))


def write_record(folder, pid, port, worktree, land="Land1"):
    with open(os.path.join(folder, f"{pid}.json"), "w", encoding="utf-8") as file:
        json.dump({"pid": pid, "port": port, "worktree": worktree, "executable": worktree + "/openblack.exe",
                   "build_type": "Debug", "land": land, "started": 1760000000}, file)


def wait_for(condition, seconds=20.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            raise AssertionError("timed out")
        time.sleep(0.01)


class ReadGamesTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp(prefix="openblack-inspector-mcp-test-")

    def tearDown(self):
        shutil.rmtree(self.folder, ignore_errors=True)

    def test_stale_files_are_removed_and_others_left(self):
        write_record(self.folder, 100, 47801, "C:/projects/ob-wt-a")
        write_record(self.folder, 200, 47802, "C:/projects/ob-wt-b")
        with open(os.path.join(self.folder, "300.json"), "w", encoding="utf-8") as file:
            file.write("not json")
        write_record(self.folder, 400, 47804, "C:/projects/ob-wt-c")
        # A record naming another process than its file
        with open(os.path.join(self.folder, "500.json"), "w", encoding="utf-8") as file:
            json.dump({"pid": 501, "port": 47805}, file)
        with open(os.path.join(self.folder, "frame_12.png"), "w", encoding="utf-8") as file:
            file.write("picture")

        running = {100, 300, 400, 500}
        games = mcp.read_games(self.folder, lambda pid: pid in running)

        self.assertEqual([game["pid"] for game in games], [100, 400])
        self.assertFalse(os.path.exists(os.path.join(self.folder, "200.json")))
        for kept in ("100.json", "300.json", "400.json", "500.json", "frame_12.png"):
            self.assertTrue(os.path.exists(os.path.join(self.folder, kept)), kept)

    def test_shots_of_games_that_have_gone_are_removed(self):
        write_record(self.folder, 100, 47801, "C:/projects/ob-wt-a")
        shots = os.path.join(self.folder, "shots")
        for pid in ("100", "200", "300"):
            os.makedirs(os.path.join(shots, pid))
            with open(os.path.join(shots, pid, "frame_12.png"), "w", encoding="utf-8") as file:
                file.write("picture")
        os.makedirs(os.path.join(shots, "notes"))
        with open(os.path.join(shots, "400"), "w", encoding="utf-8") as file:
            file.write("a file, not a game's folder")

        games = mcp.read_games(self.folder, lambda pid: pid == 100)

        self.assertEqual([game["pid"] for game in games], [100])
        self.assertTrue(os.path.exists(os.path.join(shots, "100", "frame_12.png")))
        self.assertFalse(os.path.exists(os.path.join(shots, "200")))
        self.assertFalse(os.path.exists(os.path.join(shots, "300")))
        self.assertTrue(os.path.isdir(os.path.join(shots, "notes")))
        self.assertTrue(os.path.isfile(os.path.join(shots, "400")))

    def test_a_missing_folder_has_no_games(self):
        self.assertEqual(mcp.read_games(os.path.join(self.folder, "absent"), lambda pid: True), [])

    def test_this_process_is_alive(self):
        self.assertTrue(mcp.pid_alive(os.getpid()))
        self.assertFalse(mcp.pid_alive(0))
        self.assertFalse(mcp.pid_alive(None))


class SelectGameTest(unittest.TestCase):
    games = [
        {"pid": 10, "port": 47801, "worktree": "C:/projects/ob-wt-inspect"},
        {"pid": 11, "port": 47802, "worktree": "C:/projects/ob-wt-world"},
        {"pid": 12, "port": 47803, "worktree": "C:/projects/ob-wt-world"},
    ]

    def test_a_pid_or_port_matches_however_it_comes(self):
        # An MCP client may send 77120 as an integer, a float or text; a file may hold it either way too
        for pid in (11, 11.0, "11", " 11 "):
            game, error = mcp.select_game(self.games, pid=pid)
            self.assertIsNotNone(game, error)
            self.assertEqual(game["pid"], 11)
        for port in (47803, "47803", 47803.0):
            self.assertEqual(mcp.select_game(self.games, port=port)[0]["pid"], 12)
        texts = [{**game, "pid": str(game["pid"]), "port": str(game["port"])} for game in self.games]
        self.assertEqual(mcp.select_game(texts, pid=10)[0]["port"], "47801")
        self.assertIsNone(mcp.select_game(self.games, pid="eleven")[0])

    def test_a_build_apart_from_its_worktree_is_found_by_it(self):
        # Built from E:/openblack/worktrees/ob-wt-gate into E:/openblack/builds/ob-wt-gate: the game names the worktree
        games = [{"pid": 1, "port": 1, "worktree": "E:/openblack/worktrees/ob-wt-gate",
                  "executable": "E:/openblack/builds/ob-wt-gate/bin/Debug/openblack.exe"},
                 {"pid": 2, "port": 2, "worktree": "C:/projects/ob-wt-fish",
                  "executable": "C:/projects/ob-wt-fish/cmake-build-debug/bin/Debug/openblack.exe"}]
        for wanted in ("ob-wt-gate", "E:/openblack/worktrees/ob-wt-gate", "E:\\openblack\\worktrees\\ob-wt-gate\\src"):
            self.assertEqual(mcp.select_game(games, worktree=wanted)[0]["pid"], 1, wanted)
        self.assertEqual(mcp.select_game(games, worktree="ob-wt-fish")[0]["pid"], 2)
        # An older build that couldn't tell its worktree is still found by the build folder named after it
        games[0]["worktree"] = ""
        self.assertEqual(mcp.select_game(games, worktree="ob-wt-gate")[0]["pid"], 1)
        self.assertIsNone(mcp.select_game(games, worktree="ob-wt-none")[0])

    def test_by_port_and_pid(self):
        self.assertEqual(mcp.select_game(self.games, port=47802)[0]["pid"], 11)
        self.assertEqual(mcp.select_game(self.games, pid=12)[0]["port"], 47803)

    def test_by_worktree_path_name_or_a_path_inside(self):
        for wanted in ("C:/projects/ob-wt-inspect", "C:\\projects\\ob-wt-inspect\\", "ob-wt-inspect",
                       "C:/projects/ob-wt-inspect/src/Game.cpp"):
            game, error = mcp.select_game(self.games, worktree=wanted)
            self.assertIsNotNone(game, error)
            self.assertEqual(game["pid"], 10)

    def test_a_name_is_the_whole_folder_name(self):
        game, error = mcp.select_game(self.games, worktree="inspect")
        self.assertIsNone(game)
        self.assertIn("no running game", error)

    def test_several_matches_ask_for_a_pid(self):
        game, error = mcp.select_game(self.games, worktree="ob-wt-world")
        self.assertIsNone(game)
        self.assertIn("choose one by pid", error)
        self.assertIn("pid 11", error)
        self.assertIn("pid 12", error)
        self.assertEqual(mcp.select_game(self.games, worktree="ob-wt-world", pid=12)[0]["port"], 47803)

    def test_nothing_running(self):
        game, error = mcp.select_game([], pid=5)
        self.assertIsNone(game)
        self.assertIn("running: none", error)


class SessionBase(unittest.TestCase):
    """Two games, from two worktrees; old=True makes them answer as games built before this adapter"""
    old = False

    def setUp(self):
        self.folder = tempfile.mkdtemp(prefix="openblack-inspector-mcp-test-")
        self.first = FakeGame(1001, "first", old=self.old)
        self.second = FakeGame(1002, "second", old=self.old)
        self.games = [self.first, self.second]
        write_record(self.folder, 1001, self.first.port, "C:/projects/ob-wt-first")
        write_record(self.folder, 1002, self.second.port, "C:/projects/ob-wt-second", land="Land2")
        self.running = {1001, 1002}
        self.session = self.make_session()

    # How long an answer and a ping may take: generous, so that a machine busy with builds and other tests doesn't
    # fail the tests; the tests of what happens past a limit set theirs short, or wait past these
    ANSWER_TIMEOUT = 5.0
    PING_TIMEOUT = 5.0

    def make_session(self, **options):
        options.setdefault("ready_timeout", 15.0)
        options.setdefault("load_timeout", 15.0)
        options.setdefault("ping_timeout", self.PING_TIMEOUT)
        return mcp.Session(self.first.port, self.ANSWER_TIMEOUT, folder=self.folder,
                           alive=lambda pid: pid in self.running, **options)

    def tearDown(self):
        self.session.close()
        for game in self.games:
            game.close()
        shutil.rmtree(self.folder, ignore_errors=True)

    def add_game(self, game, worktree):
        self.games.append(game)
        # Running before its file is there, as a real game is: a file seen for a process not running is removed
        self.running.add(game.pid)
        write_record(self.folder, game.pid, game.port, worktree)
        return game

    def stop(self, game):
        self.running.discard(game.pid)
        game.close()

    def moon(self, **arguments):
        answer = self.session.call("game_moon", arguments)
        self.assertTrue(answer["ok"], answer)
        return answer["result"]["who"]


class SessionTest(SessionBase):
    """The rules every call follows, so that agents sharing the adapter never get each other's game"""

    def test_a_game_named_by_a_pid_as_text_is_found(self):
        self.assertEqual(self.moon(pid="1002"), "second")
        self.assertEqual(self.moon(port=str(self.first.port)), "first")

    def test_a_file_written_with_text_numbers_is_still_a_game(self):
        with open(os.path.join(self.folder, "1002.json"), "w", encoding="utf-8") as file:
            json.dump({"pid": "1002", "port": str(self.second.port), "worktree": "C:/projects/ob-wt-second"}, file)
        self.assertEqual([game["pid"] for game in mcp.read_games(self.folder, lambda pid: pid in self.running)],
                         [1001, 1002])

    def test_a_file_being_rewritten_is_read_again_not_left_out(self):
        path = os.path.join(self.folder, "1002.json")
        with open(path, encoding="utf-8") as file:
            whole = file.read()
        with open(path, "w", encoding="utf-8") as file:
            file.write(whole[:10])

        def finish():
            time.sleep(0.05)
            with open(path, "w", encoding="utf-8") as file:
                file.write(whole)
        threading.Thread(target=finish, daemon=True).start()
        self.assertEqual([game["pid"] for game in mcp.read_games(self.folder, lambda pid: pid in self.running)],
                         [1001, 1002])

    def test_lists_games_without_taking_control(self):
        answer = self.session.call("inspector_games", {})
        games = answer["result"]["games"]
        self.assertEqual([game["pid"] for game in games], [1001, 1002])
        self.assertTrue(all(game["responding"] for game in games))
        self.assertTrue(all(game["ready"] for game in games))
        self.assertEqual([game["connected"] for game in games], [False, False])
        self.assertEqual(games[1]["land"], "Land2")
        self.assertEqual(self.first.controlling_clients(), 0)
        self.assertEqual(self.second.controlling_clients(), 0)

    def test_a_game_that_has_gone_is_not_listed(self):
        self.running.discard(1002)
        games = self.session.call("inspector_games", {})["result"]["games"]
        self.assertEqual([game["pid"] for game in games], [1001])
        self.assertFalse(os.path.exists(os.path.join(self.folder, "1002.json")))

    def test_several_running_and_none_named_is_refused_with_the_list(self):
        answer = self.session.call("game_moon", {})
        self.assertFalse(answer["ok"])
        self.assertIn("2 games are running", answer["error"])
        self.assertIn("pid 1001", answer["error"])
        self.assertIn("pid 1002", answer["error"])
        # Nothing was sent to either
        self.assertEqual(self.first.asked(), [])
        self.assertEqual(self.second.asked(), [])

    def test_connecting_doesnt_choose_for_calls_while_several_run(self):
        connected = self.session.call("inspector_connect", {"worktree": "ob-wt-second"})
        self.assertTrue(connected["ok"], connected)
        self.assertEqual(connected["result"]["connected"]["pid"], 1002)
        self.assertIn("every call must still name its game", connected["result"]["note"])
        self.assertFalse(self.session.call("game_moon", {})["ok"])

    def test_each_call_goes_to_the_game_it_names(self):
        self.assertEqual(self.moon(worktree="ob-wt-first"), "first")
        self.assertEqual(self.moon(pid=1002), "second")
        self.assertEqual(self.moon(port=self.first.port), "first")
        self.assertEqual(self.moon(worktree="C:/projects/ob-wt-second/src"), "second")
        self.assertEqual(len(self.first.asked()), 2)
        self.assertEqual(len(self.second.asked()), 2)

    def test_two_agents_interleaved_never_cross(self):
        # Two agents share the session, each naming its own game, their calls interleaved
        for _ in range(5):
            self.assertEqual(self.moon(pid=1001), "first")
            self.session.call("inspector_connect", {"pid": 1002})
            self.assertEqual(self.moon(pid=1002), "second")
            self.session.call("inspector_connect", {"pid": 1001})
        self.assertEqual(len(self.first.asked()), 5)
        self.assertEqual(len(self.second.asked()), 5)

    def test_the_only_game_running_needs_no_name(self):
        self.stop(self.second)
        self.assertEqual(self.moon(), "first")

    def test_every_answer_names_its_game(self):
        answer = self.session.call("game_moon", {"pid": 1002})
        self.assertEqual(answer["game"]["pid"], 1002)
        self.assertEqual(answer["game"]["port"], self.second.port)
        content = json.loads(mcp.tool_result(answer)["content"][0]["text"])
        self.assertEqual(content["game"]["pid"], 1002)
        self.assertEqual(content["result"]["who"], "second")
        failed = mcp.tool_result({"ok": False, "error": "no", "game": {"pid": 1002}})
        self.assertTrue(failed["isError"])
        self.assertIn('"pid":1002', failed["content"][0]["text"])

    def test_a_connection_stays_open_and_keeps_its_game_locked(self):
        self.moon(pid=1001)
        self.moon(pid=1001)
        self.assertEqual(self.first.controlling_clients(), 1)
        self.assertEqual(self.second.controlling_clients(), 0)

    def test_a_named_game_not_running_is_refused(self):
        answer = self.session.call("game_moon", {"worktree": "ob-wt-none"})
        self.assertFalse(answer["ok"])
        self.assertIn("no running game", answer["error"])

    def test_a_game_restarted_on_another_port_is_found_by_worktree(self):
        self.assertEqual(self.moon(worktree="ob-wt-second"), "second")
        self.stop(self.second)
        os.remove(os.path.join(self.folder, "1002.json"))
        self.add_game(FakeGame(1003, "second again", old=self.old), "C:/projects/ob-wt-second")
        self.assertEqual(self.moon(worktree="ob-wt-second"), "second again")

    def test_a_port_now_answered_by_another_game_is_refused(self):
        # The file says pid 1002 is on this port, but another process answers it
        impostor = FakeGame(1999, "impostor", old=self.old)
        self.games.append(impostor)
        write_record(self.folder, 1002, impostor.port, "C:/projects/ob-wt-second")
        answer_or_error = None
        try:
            answer_or_error = self.session.call("game_moon", {"pid": 1002})
        except ConnectionError as error:
            answer_or_error = str(error)
        self.assertIn("pid 1999", str(answer_or_error))
        self.assertEqual(impostor.asked(), [])

    def test_tools_take_a_game_and_the_game_tools_come_first(self):
        names = [tool["name"] for tool in mcp.GAME_TOOLS]
        self.assertEqual(names, ["inspector_games", "inspector_connect"])
        for tool in mcp.TOOLS:
            for key in ("port", "pid", "worktree"):
                self.assertIn(key, tool["inputSchema"]["properties"], tool["name"])
        # The shaping options shared between tools were not given the selector
        for key in ("port", "pid", "worktree"):
            self.assertNotIn(key, mcp.SHAPING)
            self.assertNotIn(key, mcp.SHAPING_KEYS)

    def test_a_game_pinned_on_the_command_line_is_always_followed(self):
        self.session.pinned = {"worktree": "ob-wt-second"}
        self.assertEqual(self.moon(), "second")
        self.assertEqual(self.moon(pid=1001), "first")


class OldGameSessionTest(SessionTest):
    """The same rules hold with games built before answers named their game, as the adapter names them itself"""
    old = True

    def test_an_old_game_is_named_by_the_adapter(self):
        answer = self.session.call("game_moon", {"pid": 1001})
        self.assertEqual(answer["game"], {"pid": 1001, "port": self.first.port, "worktree": "C:/projects/ob-wt-first"})


class WaitingTest(SessionBase):
    """Loading, starting and crossed games"""

    def test_a_loading_game_is_asked_again_until_it_has_loaded(self):
        self.stop(self.second)
        self.first.loading = 3
        self.assertEqual(self.moon(), "first")
        self.assertEqual(len(self.first.asked()), 4)

    def test_wait_ms_bounds_the_wait_and_zero_answers_at_once(self):
        self.stop(self.second)
        self.first.loading = 1000
        start = time.monotonic()
        answer = self.session.call("game_moon", {"wait_ms": 0})
        self.assertLess(time.monotonic() - start, 2.0)
        self.assertFalse(answer["ok"])
        self.assertIn("is loading Land1.txt", answer["error"])
        answer = self.session.call("game_moon", {"wait_ms": 600})
        self.assertFalse(answer["ok"])
        self.assertIn("wait_ms", answer["error"])
        # A wait long enough sees it through; wait_ms is the adapter's, never sent to the game
        self.first.loading = 2
        self.assertTrue(self.session.call("inspector_query", {"query": "sky.moon", "wait_ms": 20000})["ok"])
        self.assertNotIn("wait_ms", self.first.asked()[-1].get("params", {}))

    def test_the_default_wait_outlasts_a_debug_load(self):
        self.assertGreaterEqual(mcp.Session(1, 10.0).load_timeout, 120.0)

    def test_a_game_loading_too_long_says_so(self):
        self.stop(self.second)
        self.first.loading = 1000
        session = self.make_session(load_timeout=2.0)
        try:
            answer = session.call("game_moon", {})
        finally:
            session.close()
        self.assertFalse(answer["ok"])
        self.assertIn("still loading Land1.txt", answer["error"])

    def test_an_old_game_busy_loading_is_waited_for_while_it_runs(self):
        self.stop(self.second)
        self.first.old = True
        self.first.delay = self.ANSWER_TIMEOUT + 1.5
        # Longer than the answer timeout, but the process runs: the answer is waited for
        self.assertEqual(self.moon(), "first")

    def test_an_old_game_that_has_gone_isnt_waited_for(self):
        self.stop(self.second)
        self.first.old = True
        self.first.delay = 60.0
        self.running.discard(1001)
        write_record(self.folder, 1001, self.first.port, "C:/projects/ob-wt-first")
        # Its file is gone with its process; the default port is the first game's
        start = time.monotonic()
        with self.assertRaises(ConnectionError):
            self.session.call("game_moon", {"port": self.first.port})
        # Given up once an answer is overdue, not waited for until the game would answer
        self.assertLess(time.monotonic() - start, 45.0)

    def test_an_answer_from_another_game_is_refused(self):
        self.second.claims = 1001
        with self.assertRaises(ConnectionError) as raised:
            self.session.call("game_moon", {"pid": 1002})
        self.assertIn("another game", str(raised.exception))

    def test_connect_waits_for_a_loading_game_to_be_ready(self):
        self.second.loading = 2

        def loaded():
            time.sleep(0.6)
            self.second.loading = 0

        threading.Thread(target=loaded, daemon=True).start()
        connected = self.session.call("inspector_connect", {"pid": 1002})
        self.assertTrue(connected["ok"], connected)
        self.assertEqual(connected["game"]["pid"], 1002)

    def test_connect_refuses_a_game_never_ready(self):
        self.second.loading = 1000
        session = self.make_session(ready_timeout=2.0)
        try:
            answer = session.call("inspector_connect", {"pid": 1002})
        finally:
            session.close()
        self.assertFalse(answer["ok"])
        self.assertIn("is loading Land1.txt", answer["error"])

    def test_connect_waits_for_a_game_just_started(self):
        late = FakeGame(1004, "late")
        self.games.append(late)

        def start():
            time.sleep(0.5)
            self.add_game(late, "C:/projects/ob-wt-late")

        threading.Thread(target=start, daemon=True).start()
        connected = self.session.call("inspector_connect", {"worktree": "ob-wt-late"})
        self.assertTrue(connected["ok"], connected)

    def test_a_step_reports_once_run(self):
        self.stop(self.second)
        answer = self.session.call("game_step", {"frames": 2})
        self.assertTrue(answer["ok"])


PNG = b"\x89PNG\r\n\x1a\n" + b"\0" * 64 + b"\0\0\0\0IEND\xaeB`\x82"


class ScreenshotTest(SessionBase):
    """A picture is answered once its file is whole; a game too old for an option is told so"""

    def setUp(self):
        super().setUp()
        self.stop(self.second)
        self.path = os.path.join(self.folder, "shot.png")

    def describe_screenshot(self, parameters):
        def describe(request):
            if request.get("params", {}).get("query") == "screenshot.take":
                return {"query": "screenshot.take", "parameters": [{"name": name} for name in parameters]}
            return "no query"
        self.first.handlers["describe"] = describe

    def write_slowly(self, request):
        # Written in pieces a little after the answer, as the game draws the frame
        def write():
            time.sleep(0.2)
            with open(self.path, "wb") as file:
                file.write(PNG[:20])
                file.flush()
                time.sleep(0.3)
                file.write(PNG[20:])
        threading.Thread(target=write, daemon=True).start()
        return {"path": self.path, "frame": 5}

    def test_answered_once_the_file_is_whole(self):
        self.first.handlers["screenshot.take"] = self.write_slowly
        answer = self.session.call("screenshot", {})
        self.assertTrue(answer["ok"], answer)
        self.assertTrue(answer["result"]["written"])
        with open(self.path, "rb") as file:
            self.assertEqual(file.read(), PNG)

    def test_an_old_file_at_the_path_is_not_taken_for_the_new_one(self):
        with open(self.path, "wb") as file:
            file.write(PNG)
        old = time.time() - 60
        os.utime(self.path, (old, old))
        self.first.handlers["screenshot.take"] = lambda request: {"path": self.path, "frame": 5}
        session = self.make_session()
        session.timeout = 0.2
        try:
            answer = session.call("screenshot", {})
        finally:
            session.close()
        self.assertFalse(answer["result"]["written"])
        self.assertIn("isn't written yet", answer["result"]["note"])

    def test_a_picture_the_game_gives_up_is_answered_with_why(self):
        self.first.handlers["screenshot.take"] = lambda request: {"path": self.path, "frame": 5}
        self.first.handlers["screenshot.pending"] = lambda request: {
            "pending": [], "failed": [f"{self.path}: the camera didn't stay where it was put"]}
        started = time.monotonic()
        answer = self.session.call("screenshot", {})
        self.assertTrue(answer["ok"], answer)
        self.assertFalse(answer["result"]["written"])
        self.assertEqual(answer["result"]["failed"], "the camera didn't stay where it was put")
        self.assertIn("gave the picture up", answer["result"]["note"])
        # Said as soon as the game gives it up, not after the whole wait
        self.assertLess(time.monotonic() - started, self.session.timeout * 3)

    def test_options_an_older_game_lacks_are_refused_clearly(self):
        self.describe_screenshot(["path", "in_frames", "at_frame", "camera"])
        self.first.handlers["screenshot.take"] = self.write_slowly
        answer = self.session.call("screenshot", {"frame": 12, "hide_gui": True})
        self.assertFalse(answer["ok"])
        self.assertIn("older build", answer["error"])
        self.assertIn("frame, hide_gui", answer["error"])
        self.assertNotIn("screenshot.take", [request["query"] for request in self.first.asked()])
        # Without them it is taken
        self.assertTrue(self.session.call("screenshot", {})["ok"])

    def test_options_a_newer_game_has_are_sent(self):
        self.describe_screenshot(["path", "frame", "hide_gui"])
        self.first.handlers["screenshot.take"] = self.write_slowly
        answer = self.session.call("screenshot", {"frame": {"id": 12, "distance": 30}, "hide_gui": True})
        self.assertTrue(answer["ok"], answer)
        sent = [request for request in self.first.asked() if request["query"] == "screenshot.take"][0]
        self.assertEqual(sent["params"], {"frame": {"id": 12, "distance": 30}, "hide_gui": True})

    def test_a_kept_picture_goes_to_the_adapters_screenshot_folder(self):
        self.first.handlers["screenshot.take"] = lambda request: {"path": self.path, "frame": 5,
                                                                  "params": request.get("params", {})}
        self.describe_screenshot(["path", "frame", "hide_gui", "feature", "what", "note", "root"])
        self.session.screenshot_root = "E:/openblack/screenshots"
        answer = self.session.call("screenshot", {"feature": "sky/moon", "what": "full-moon", "wait": False})
        self.assertTrue(answer["ok"], answer)
        sent = [request for request in self.first.asked() if request["query"] == "screenshot.take"][-1]
        self.assertEqual(sent["params"], {"feature": "sky/moon", "what": "full-moon",
                                          "root": "E:/openblack/screenshots"})
        # Not for a temporary one, nor over a folder the call names
        self.session.call("screenshot", {"wait": False})
        self.assertNotIn("root", [request for request in self.first.asked()
                                  if request["query"] == "screenshot.take"][-1].get("params", {}))
        self.session.call("screenshot", {"feature": "sky/moon", "what": "x", "root": "D:/s", "wait": False})
        self.assertEqual(self.first.asked()[-1]["params"]["root"], "D:/s")

    def test_a_query_an_older_game_lacks_says_why(self):
        self.first.handlers["camera.frame"] = lambda request: "no query camera.frame; ask describe"
        answer = self.session.call("camera_frame", {"id": 3})
        self.assertFalse(answer["ok"])
        self.assertIn("older build", answer["error"])

    def test_a_query_nobody_knows_is_answered_with_the_closest_names(self):
        # A tool's name sent as a query: the game doesn't know it, nor does the adapter, so it is no older build
        self.first.handlers["screenshot"] = lambda request: "no query screenshot; ask describe"
        answer = self.session.send({"query": "screenshot"}, target={"pid": self.first.pid})
        self.assertFalse(answer["ok"])
        self.assertNotIn("older build", answer["error"])
        self.assertIn("screenshot.take", answer["error"])

    def test_call_takes_tool_names(self):
        self.assertTrue(mcp.call_tool_name("screenshot"))
        self.assertTrue(mcp.call_tool_name("game_entities"))
        self.assertTrue(mcp.call_tool_name("inspector_games"))
        self.assertFalse(mcp.call_tool_name("screenshot.take"))
        self.assertFalse(mcp.call_tool_name("describe"))
        self.assertFalse(mcp.call_tool_name("no_such_tool"))
        self.assertIn("ecs.entities", mcp.suggestions("game_entitys"))


class ArgumentsTest(SessionBase):
    """What a call's arguments become: the query's parameters go in params, and nothing given is dropped"""
    CATALOGUE = {"ecs.entities": {"query": "ecs.entities", "parameters": [{"name": "component"}, {"name": "kind"}]},
                 "edit.add": {"query": "edit.add", "parameters": [{"name": "id"}, {"name": "fields"}]}}

    def test_call_json_is_the_parameters(self):
        request = mcp.call_request("ecs.entities", '{"component": "Temple"}', self.CATALOGUE)
        self.assertEqual(request, {"query": "ecs.entities", "params": {"component": "Temple"}})

    def test_call_shaping_options_stay_beside_the_parameters(self):
        request = mcp.call_request("ecs.entities", '{"component": "Tree", "near": [0, 0], "radius": 50, "limit": 5}',
                                   self.CATALOGUE)
        self.assertEqual(request, {"query": "ecs.entities", "params": {"component": "Tree"}, "near": [0, 0],
                                   "radius": 50, "limit": 5})

    def test_call_a_parameter_named_as_an_option_is_the_parameter(self):
        request = mcp.call_request("edit.add", '{"id": 7, "fields": {"life": 1}}', self.CATALOGUE)
        self.assertEqual(request, {"query": "edit.add", "params": {"id": 7, "fields": {"life": 1}}})

    def test_call_reported_forms_with_the_games_own_catalogue(self):
        # game.frame_time's ms is its parameter, not a request member the game refuses
        self.assertEqual(mcp.call_request("game.frame_time", '{"ms": 100}'),
                         {"query": "game.frame_time", "params": {"ms": 100}})
        # level.testbed takes no parameters: an id goes in params, where the game refuses it by name, rather than
        # loading the empty testbed as if it had been given (a scenario is game.scenario's id)
        self.assertEqual(mcp.call_request("level.testbed", '{"id": "movement.course"}'),
                         {"query": "level.testbed", "params": {"id": "movement.course"}})
        self.assertEqual(mcp.call_request("game.scenario", '{"id": "movement.course"}'),
                         {"query": "game.scenario", "params": {"id": "movement.course"}})

    def test_the_game_refuses_by_name_what_the_call_sends(self):
        def refuse_unknown(request):
            known = {entry["name"] for entry in mcp.CATALOGUE[request["query"]]["parameters"]}
            unknown = sorted(set(request.get("params", {})) - known)
            if unknown:
                return f"{request['query']} doesn't take {', '.join(unknown)}"
            return {"params": request.get("params", {})}
        for query in ("level.testbed", "game.frame_time"):
            self.first.handlers[query] = refuse_unknown
        refused = self.session.send(mcp.call_request("level.testbed", '{"id": "movement.course"}'), target={"pid": 1001})
        self.assertFalse(refused["ok"])
        self.assertIn("doesn't take id", refused["error"])
        taken = self.session.send(mcp.call_request("game.frame_time", '{"ms": 100}'), target={"pid": 1001})
        self.assertTrue(taken["ok"], taken)
        self.assertEqual(taken["result"]["params"], {"ms": 100})

    def test_call_screenshot_path_is_its_parameter(self):
        # As the MCP tool sends it: the shell's path is screenshot.take's, never dropped for the temporary default
        catalogue = {"screenshot.take": {"query": "screenshot.take",
                                         "parameters": [{"name": "path"}, {"name": "hide_gui"}]}}
        self.assertEqual(mcp.call_request("screenshot.take", '{"path": "E:/shots/x.png", "hide_gui": true}', catalogue),
                         {"query": "screenshot.take", "params": {"path": "E:/shots/x.png", "hide_gui": True}})
        self.assertEqual(mcp.call_request("screenshot.take", '{"path": "E:/shots/x.png"}'),
                         {"query": "screenshot.take", "params": {"path": "E:/shots/x.png"}})

    def test_call_describe_takes_its_query(self):
        request = mcp.call_request("describe", '{"query": "sky.moon"}', self.CATALOGUE)
        self.assertEqual(request, {"query": "describe", "params": {"query": "sky.moon"}})

    def test_call_explicit_params_form_still_works(self):
        request = mcp.call_request("ecs.entities", '{"params": {"component": "Tree"}, "limit": 3}', self.CATALOGUE)
        self.assertEqual(request, {"query": "ecs.entities", "params": {"component": "Tree"}, "limit": 3})

    def test_call_without_json_has_no_parameters(self):
        self.assertEqual(mcp.call_request("sky.moon", "{}", self.CATALOGUE), {"query": "sky.moon"})
        self.assertEqual(mcp.call_request("sky.moon", "", self.CATALOGUE), {"query": "sky.moon"})

    def test_call_refuses_what_isnt_an_object(self):
        with self.assertRaises(ValueError):
            mcp.call_request("sky.moon", "[1]", self.CATALOGUE)
        with self.assertRaises(ValueError):
            mcp.call_request("sky.moon", "{nope", self.CATALOGUE)

    def test_the_game_is_sent_the_parameters(self):
        self.session.send(mcp.call_request("ecs.entities", '{"component": "Temple"}', self.CATALOGUE),
                          target={"pid": 1001})
        self.assertEqual(self.first.asked()[-1]["params"], {"component": "Temple"})
        self.assertNotIn("component", self.first.asked()[-1])

    def test_inspector_query_takes_parameters_beside_params(self):
        answer = self.session.call("inspector_query", {"query": "ecs.entities", "component": "Temple", "limit": 2,
                                                       "pid": 1001})
        self.assertTrue(answer["ok"], answer)
        self.assertEqual(self.first.asked()[-1]["params"], {"component": "Temple"})
        self.assertEqual(self.first.asked()[-1]["limit"], 2)
        clash = self.session.call("inspector_query", {"query": "ecs.entities", "params": {"component": "A"},
                                                      "component": "B", "pid": 1001})
        self.assertFalse(clash["ok"])
        self.assertIn("component", clash["error"])

    def test_a_value_goes_as_it_is_given(self):
        for value in (6.0, 3, True, "text", [1, 2, 3], {"a": 1}, None):
            self.session.call("edit_set", {"id": 157, "component": "Creature", "field": "size", "value": value,
                                           "pid": 1001})
            sent = self.first.asked()[-1]["params"]["value"]
            self.assertEqual(sent, value)
            self.assertEqual(type(sent), type(value))

    def test_a_tool_refuses_arguments_it_doesnt_take(self):
        asked = len(self.first.asked())
        answer = self.session.call("game_moon", {"pid": 1001, "colour": "red"})
        self.assertFalse(answer["ok"])
        self.assertIn("colour", answer["error"])
        self.assertEqual(len(self.first.asked()), asked)


class SchemaTest(unittest.TestCase):
    """Tools' parameters come from the game's own descriptions of its queries"""
    BUILT_IN = {"describe", "writes"}

    def test_every_tools_query_is_one_the_game_describes(self):
        self.assertGreater(len(mcp.CATALOGUE), 100)
        for tool in mcp.TOOLS:
            if tool.get("query") and tool["query"] not in self.BUILT_IN:
                self.assertIn(tool["query"], mcp.CATALOGUE, tool["name"])

    def test_required_parameters_are_the_games(self):
        for tool in mcp.TOOLS:
            entry = mcp.CATALOGUE.get(tool.get("query"))
            if entry is None:
                continue
            wanted = [parameter["name"] for parameter in entry["parameters"] if parameter.get("required")]
            if entry.get("needs_near"):
                wanted += ["near", "radius"]
            self.assertEqual(sorted(tool["inputSchema"]["required"]), sorted(set(wanted)), tool["name"])
            for parameter in entry["parameters"]:
                self.assertIn(parameter["name"], tool["inputSchema"]["properties"], tool["name"])
                self.assertIn(parameter["name"], tool["params"], tool["name"])

    def test_the_sounds_near_a_point_need_the_point_and_radius(self):
        self.assertEqual(sorted(mcp.TOOLS_BY_NAME["audio_sounds"]["inputSchema"]["required"]), ["near", "radius"])

    def test_a_schema_from_a_description(self):
        tool = {"name": "thing", "query": "x.thing", "params": ["legacy"],
                "inputSchema": mcp.schema({"id": {"type": "integer", "description": "Mine"},
                                           "wait": {"type": "boolean"}})}
        entry = {"query": "x.thing", "needs_near": True, "parameters": [
            {"name": "id", "type": "integer", "required": True, "description": "The game's"},
            {"name": "at", "type": "point", "description": "Where"},
            {"name": "frame", "type": "integer|object"}]}
        mcp.schema_from_catalogue(tool, entry)
        properties = tool["inputSchema"]["properties"]
        self.assertEqual(properties["id"]["description"], "Mine")
        self.assertEqual(properties["at"], {"type": "array", "items": {"type": "number"}, "description": "Where"})
        self.assertEqual(properties["frame"]["type"], ["integer", "object"])
        self.assertIn("wait", properties)
        self.assertEqual(sorted(tool["inputSchema"]["required"]), ["id", "near", "radius"])
        self.assertEqual(tool["params"], ["id", "at", "frame", "legacy"])

    def test_every_parameter_has_a_type(self):
        # A parameter without one may be sent as text by a client (a number as "6.0")
        for tool in mcp.TOOLS:
            for name, prop in tool["inputSchema"]["properties"].items():
                self.assertIn("type", prop, f"{tool['name']}.{name}")
        value = mcp.TOOLS_BY_NAME["edit_set"]["inputSchema"]["properties"]["value"]["type"]
        self.assertTrue({"number", "boolean", "string", "array", "object"} <= set(value))
        self.assertIn("number", mcp.TOOLS_BY_NAME["script_set_global"]["inputSchema"]["properties"]["value"]["type"])
        self.assertEqual(mcp.parameter_schema({"type": 'array or "all"'})["type"], ["array", "string"])
        self.assertEqual(mcp.parameter_schema({"type": "string or integer"})["type"], ["string", "integer"])
        self.assertIn("object", mcp.parameter_schema({"type": "any"})["type"])

    def test_a_missing_catalogue_is_empty(self):
        self.assertEqual(mcp.read_catalogue(os.path.join(tempfile.gettempdir(), "no-such-catalogue.json")), {})


if __name__ == "__main__":
    unittest.main()
