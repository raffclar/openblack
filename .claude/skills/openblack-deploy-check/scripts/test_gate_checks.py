# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Unit tests of gate_checks.py with fake console text, fake logs and pictures made in memory.

    python -I test_gate_checks.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate_checks  # noqa: E402

# The console of the black screen build (web 551d052b4 on WebGPU), shortened
BLACK_CONSOLE = """14:43:50.001 [log warning] The powerPreference option is currently ignored when calling requestAdapter() on Windows.
14:43:52.400 [log warning] [Invalid CommandBuffer] is invalid due to a previous error.
 - While calling [Queue].Submit([[Invalid CommandBuffer]])
14:43:52.411 [log warning] WebGPU: too many warnings, no more warnings will be reported to the console for this GPUDevice.
"""
GOOD_CONSOLE = """14:44:55.933 [log error] Failed to load resource: the server responded with a status of 404 (File not found) <http://127.0.0.1:8771/developer-tools.txt>
14:44:57.829 [log warning] The powerPreference option is currently ignored when calling requestAdapter() on Windows.
15:08:20.182 [log warning] WebGL: INVALID_ENUM: getInternalformatParameter: invalid internalformat
14:47:36.739 [shot] question.png
"""


class ConsoleTest(unittest.TestCase):
    def test_black_screen_console_fails(self):
        names = {name for name, _ in gate_checks.console_faults(BLACK_CONSOLE)}
        self.assertEqual(names, {"WebGPU validation"})

    def test_good_console_passes(self):
        self.assertEqual(gate_checks.console_faults(GOOD_CONSOLE), [])

    def test_other_faults(self):
        text = ("1 [exception worker] Uncaught RuntimeError: unreachable\n"
                "2 [error] the game stopped\n"
                "3 [log error] Failed to load resource: 404 <http://x/openblack.wasm>\n"
                "4 [log warning] WebGL: INVALID_OPERATION: drawElements: no buffer\n"
                "5 [log warning] Failed to construct 'Worker': the script cannot load\n")
        names = [name for name, _ in gate_checks.console_faults(text)]
        self.assertEqual(names, ["uncaught exception", "console error", "console error", "WebGL error",
                                 "worker load error"])

    def test_failed_requests(self):
        log = ('serve: "GET /index.html HTTP/1.1" 200 -\n'
               'serve: "GET /developer-tools.txt HTTP/1.1" 404 -\n'
               'serve: "GET /openblack.wasm HTTP/1.1" 404 -\n')
        self.assertEqual(gate_checks.failed_requests(log), ["/openblack.wasm 404"])


class PictureTest(unittest.TestCase):
    def picture(self, colour):
        from PIL import Image
        picture = Image.new("RGB", (1280, 1024), colour)
        picture.paste((20, 20, 20), (0, 0, 1280, 22))
        return picture

    def test_black_picture_fails(self):
        ok, detail = gate_checks.land_check(gate_checks.picture_measures(self.picture((0, 0, 0))))
        self.assertFalse(ok)
        self.assertIn("lit 0%", detail)

    def test_land_picture_passes(self):
        from PIL import Image
        picture = self.picture((90, 150, 210))
        picture.paste((110, 140, 60), (0, 500, 1280, 1024))
        measures = gate_checks.picture_measures(picture)
        self.assertTrue(gate_checks.land_check(measures)[0])
        self.assertTrue(gate_checks.land_check(measures, measures)[0])
        other = gate_checks.picture_measures(self.picture((200, 0, 200)))
        self.assertFalse(gate_checks.land_check(measures, other)[0])
        self.assertIsInstance(picture, Image.Image)

    def test_sea_only_is_not_land(self):
        self.assertFalse(gate_checks.land_check(gate_checks.picture_measures(self.picture((40, 120, 170))))[0])


class ServerTest(unittest.TestCase):
    SERVER = ("[game] [info] [DedicatedServer.cpp:1577] Network game: slot 0 Player1: commands [6x10], "
              "300 checksums compared, 300 the same, first different none, 0 resumes\n")
    CLIENT = "[game] [info] net: Network game: 300 turns compared with the server, 300 the same\n"

    def test_lockstep_passes(self):
        self.assertTrue(gate_checks.lockstep_check(self.SERVER, self.CLIENT, 300)[0])

    def test_lockstep_fails_on_a_part(self):
        server = self.SERVER.replace("300 the same, first different none, 0 resumes",
                                     "0 the same, first different 1, 3 resumes")
        self.assertFalse(gate_checks.lockstep_check(server, self.CLIENT, 300)[0])
        self.assertFalse(gate_checks.lockstep_check("", self.CLIENT, 300)[0])
        self.assertFalse(gate_checks.lockstep_check(self.SERVER, "", 300)[0])

    def test_build_id(self):
        self.assertEqual(gate_checks.build_id("openblack-server 0.1.0+9f73a40c4184 (build id 0.1.0+9f73a40c4184/codec36)"),
                         "0.1.0+9f73a40c4184/codec36")
        self.assertIsNone(gate_checks.build_id("no version"))


if __name__ == "__main__":
    try:
        import PIL  # noqa: F401
    except ImportError:
        print("no PIL: skip")
        sys.exit(77)
    unittest.main()
