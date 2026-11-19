#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Adds the function names and the source lines to the stack of a crash report of the web game.

The web game writes each frame of its stack as the byte offset in openblack.wasm, for example
"#3  0x00000000001A2B3C openblack.wasm!wasm-function[1234]". The debug file of the same build
(web-debug/openblack.debug.wasm of a build with OPENBLACK_WEB_DEBUG_INFO) has the names and the
lines. This script gives all the offsets to llvm-symbolizer in one run, and writes the report
again with the function and the source line under each frame.

    python tools/crash/web_crash_symbolize.py <report.txt> <openblack.debug.wasm> [--symbolizer PATH]

The debug file must come from the same build as the game that crashed. Compare the build id of
the debug file with the build id of the shipped WebAssembly if you are not sure.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# A frame line of the crash report: its number, its address and the rest of the line
FRAME_RE = re.compile(r"^(\s*#\d+\s+)0x([0-9A-Fa-f]+)(\s.*)?$")


def frame_addresses(report: str) -> list[int]:
    """Gives the address of each frame line of the report, in the order of the report."""
    addresses = []
    for line in report.splitlines():
        match = FRAME_RE.match(line)
        if match:
            addresses.append(int(match.group(2), 16))
    return addresses


def parse_symbolizer_json(output: str) -> dict[int, list[dict]]:
    """Reads the JSON output of llvm-symbolizer into the list of the frames of each address.

    Each address can give more than one frame when the compiler put a function into its caller.
    The first frame is the innermost function.
    """
    entries = []
    for line in output.splitlines():
        line = line.strip()
        if not line:
            continue
        # The tool writes one list for the addresses of its command line, and one object for each line of its input
        value = json.loads(line)
        entries += value if isinstance(value, list) else [value]
    result: dict[int, list[dict]] = {}
    for entry in entries:
        address_text = entry.get("Address", "")
        if not address_text:
            continue
        frames = [
            frame for frame in entry.get("Symbol", []) if frame.get("FunctionName") or frame.get("FileName")
        ]
        result[int(address_text, 16)] = frames
    return result


def describe(frame: dict) -> str:
    """The text of one frame of llvm-symbolizer: the function and the source line, when it knows them."""
    name = frame.get("FunctionName") or "??"
    file_name = frame.get("FileName") or ""
    if not file_name:
        return name
    line = frame.get("Line", 0)
    column = frame.get("Column", 0)
    location = f"{file_name}:{line}" if not column else f"{file_name}:{line}:{column}"
    return f"{name} ({location})"


def symbolize_report(report: str, symbols: dict[int, list[dict]]) -> str:
    """Writes the report again, with the function and the source line of each frame under it."""
    lines = []
    for line in report.splitlines():
        lines.append(line)
        match = FRAME_RE.match(line)
        if not match:
            continue
        frames = symbols.get(int(match.group(2), 16), [])
        indent = " " * len(match.group(1))
        for index, frame in enumerate(frames):
            mark = "->" if index == 0 else "   in"
            lines.append(f"{indent}{mark} {describe(frame)}")
    return "\n".join(lines) + "\n"


def find_symbolizer(given: str | None) -> str:
    """Finds llvm-symbolizer: the given path, then the one of the Emscripten SDK, then the one on the PATH."""
    if given:
        return given
    emsdk = os.environ.get("EMSDK")
    if emsdk:
        for name in ("llvm-symbolizer.exe", "llvm-symbolizer"):
            candidate = Path(emsdk) / "upstream" / "bin" / name
            if candidate.exists():
                return str(candidate)
    found = shutil.which("llvm-symbolizer")
    if found:
        return found
    raise SystemExit("llvm-symbolizer not found: give --symbolizer or set EMSDK")


def run_symbolizer(symbolizer: str, debug_file: Path, addresses: list[int]) -> str:
    """Runs llvm-symbolizer once for all the addresses and gives its JSON lines."""
    command = [symbolizer, "--obj", str(debug_file), "--output-style=JSON", "--inlines", "--demangle"]
    command += [f"0x{address:x}" for address in sorted(set(addresses))]
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("report", type=Path, help="the crash report of the web game")
    parser.add_argument("debug_file", type=Path, help="openblack.debug.wasm of the same build")
    parser.add_argument("--symbolizer", help="the path of llvm-symbolizer")
    args = parser.parse_args()

    report = args.report.read_text(encoding="utf-8", errors="replace")
    addresses = frame_addresses(report)
    if not addresses:
        print("The report has no frames", file=sys.stderr)
        sys.stdout.write(report)
        return 1
    output = run_symbolizer(find_symbolizer(args.symbolizer), args.debug_file, addresses)
    sys.stdout.write(symbolize_report(report, parse_symbolizer_json(output)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
