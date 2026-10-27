#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Checks that the game touches the pointer of the system (the user's mouse cursor) only through
src/Windowing/SystemPointer.cpp, whose functions do nothing in a background run (--background), and that the
Dear ImGui backend calls the pointer of the system only behind its KeepOffSystemPointer flag.

A background run of a tool or an agent must never move, hide, hold or capture the user's pointer. Exit code 0 when
the sources keep this rule, 1 with a list of the calls that break it.

Usage: check_system_pointer.py [repository folder]
"""
import pathlib
import re
import sys

# The SDL calls that move, hide, hold or capture the pointer of the system
CALLS = re.compile(
    r"\bSDL_(WarpMouseInWindow|WarpMouseGlobal|SetRelativeMouseMode|CaptureMouse|ShowCursor|SetCursor|"
    r"SetWindowGrab|SetWindowMouseGrab|SetWindowKeyboardGrab)\s*\(")
# The one module of the game that calls them, each call behind the background gate
MODULE = pathlib.PurePosixPath("src/Windowing/SystemPointer.cpp")
# The Dear ImGui backend: each call behind its flag, on the line before the call or on its own line
BACKEND = pathlib.PurePosixPath("externals/imgui_user/imgui_impl_sdl2.cpp")
SOURCES = ("src", "externals/imgui_user", "apps", "components")
EXTENSIONS = {".cpp", ".h", ".hpp", ".cc", ".inl"}


def code_of(line):
    """The line without a // comment"""
    return line.split("//", 1)[0]


def check(root):
    problems = []
    for folder in SOURCES:
        base = root / folder
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in EXTENSIONS or not path.is_file():
                continue
            relative = pathlib.PurePosixPath(path.relative_to(root).as_posix())
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            gate_seen = False
            for number, line in enumerate(lines, start=1):
                code = code_of(line)
                if relative == MODULE and "MayTouch(" in code:
                    gate_seen = True
                if not CALLS.search(code):
                    continue
                if relative == MODULE:
                    # Each function of the module asks the gate before its call
                    if not gate_seen:
                        problems.append(f"{relative}:{number}: a call before the background gate")
                    gate_seen = False
                    continue
                if relative == BACKEND:
                    window = " ".join(code_of(text) for text in lines[max(0, number - 4):number])
                    function_start = max(i for i in range(number) if lines[i].startswith(("static ", "void ", "bool ")))
                    function_text = " ".join(code_of(text) for text in lines[function_start:number])
                    if "KeepOffSystemPointer" not in window and "KeepOffSystemPointer" not in function_text:
                        problems.append(f"{relative}:{number}: a call that is not behind KeepOffSystemPointer")
                    continue
                problems.append(f"{relative}:{number}: a call of the pointer of the system outside {MODULE}")
    return problems


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parents[2])
    problems = check(root)
    for problem in problems:
        print(problem)
    if problems:
        print(f"{len(problems)} calls of the pointer of the system break the background rule")
        return 1
    print("The pointer of the system: every call is behind the background gate")
    return 0


if __name__ == "__main__":
    sys.exit(main())
