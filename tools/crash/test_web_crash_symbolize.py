#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of web_crash_symbolize.py with a made-up report and made-up output of llvm-symbolizer."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import web_crash_symbolize as symbolize  # noqa: E402

REPORT = """==================== openblack crash report ====================
Crash:    Uncaught exception
Type:     std::runtime_error
Stack trace:
  #0  0x0000000000000100 openblack.wasm!wasm-function[9]
  #1  0x0000000000000200 openblack.wasm!wasm-function[8]
  #2  0x0000000000000300 openblack.wasm!wasm-function[7]
================================================================
"""

SYMBOLIZER_OUTPUT = """{"Address":"0x100","ModuleName":"openblack.debug.wasm","Symbol":[{"Column":7,"FileName":"src/Common/CrashHandler.cpp","FunctionName":"HandleCrash","Line":420},{"Column":0,"FileName":"src/Common/CrashHandler.cpp","FunctionName":"ReportFatal","Line":780}]}
{"Address":"0x200","ModuleName":"openblack.debug.wasm","Symbol":[{"Column":0,"FileName":"src/main.cpp","FunctionName":"main","Line":1090}]}
{"Address":"0x300","ModuleName":"openblack.debug.wasm","Symbol":[{"Column":0,"FileName":"","FunctionName":"","Line":0}]}
"""


class WebCrashSymbolizeTest(unittest.TestCase):
    def test_reads_the_addresses_of_the_frames(self):
        self.assertEqual(symbolize.frame_addresses(REPORT), [0x100, 0x200, 0x300])

    def test_reads_the_frames_of_each_address(self):
        symbols = symbolize.parse_symbolizer_json(SYMBOLIZER_OUTPUT)
        self.assertEqual(len(symbols[0x100]), 2)
        self.assertEqual(symbols[0x200][0]["FunctionName"], "main")
        self.assertEqual(symbols[0x300], [])

    def test_reads_the_list_of_the_addresses_of_the_command_line(self):
        entries = [line for line in SYMBOLIZER_OUTPUT.splitlines() if line]
        symbols = symbolize.parse_symbolizer_json("[" + ",".join(entries) + "]")
        self.assertEqual(sorted(symbols), [0x100, 0x200, 0x300])
        self.assertEqual(symbols[0x100][1]["FunctionName"], "ReportFatal")

    def test_writes_the_function_and_the_line_under_each_frame(self):
        result = symbolize.symbolize_report(REPORT, symbolize.parse_symbolizer_json(SYMBOLIZER_OUTPUT))
        self.assertIn(
            "  #0  0x0000000000000100 openblack.wasm!wasm-function[9]\n"
            "      -> HandleCrash (src/Common/CrashHandler.cpp:420:7)\n"
            "         in ReportFatal (src/Common/CrashHandler.cpp:780)\n",
            result,
        )
        self.assertIn("      -> main (src/main.cpp:1090)\n", result)
        self.assertIn("  #2  0x0000000000000300 openblack.wasm!wasm-function[7]\n====", result)
        self.assertIn("Type:     std::runtime_error\n", result)


if __name__ == "__main__":
    unittest.main()
