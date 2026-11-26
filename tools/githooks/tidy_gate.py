#!/usr/bin/env python3
"""clang-tidy 17 gate on the CHANGED LINES of C++ files.

A finding on a line that the change did not add or edit never blocks. New code must not add findings, so a new finding
on a changed line does. A NOLINT must name the checks it hides and must carry a reason comment (on the same line, or on
the line before). A NOLINT without a check name or without a reason is a finding too.

usage:
  tidy_gate.py staged                  the staged changes (the pre-commit hook)
  tidy_gate.py range <base> [--strict] the changes of <base>...HEAD (the merge-train car)

options:
  --build-dir <dir>   the folder with compile_commands.json (see find_build_dir for the other places)
  --web-build-dir <dir>  the folder of the web build (Emscripten) with its compile_commands.json, for src/Web (see below)
  --jobs <n>          clang-tidy processes at the same time (default 2)
  --timeout <s>       time limit of one file (default 120)
  --strict            a missing tool, compile database or a time-out is a failure (exit 2), not a pass

Exit 0: no finding on a changed line (or a pass with one clear line when the check cannot run and --strict is off).
Exit 1: a finding on a changed line.
Exit 2: the check cannot run and --strict is on.

Find clang-tidy 17 in this order (the first one that is version 17 wins):
  the environment variable OB_CLANG_TIDY_DIR, the git setting openblack.clangTidyDir, the value tools.clangTidy17Dir in
  .claude/openblack.local.json, clang-tidy-17 on PATH, clang-tidy on PATH.
Find the build folder (compile_commands.json) in this order:
  --build-dir, the environment variable OB_TIDY_BUILD_DIR, the git setting openblack.tidyBuildDir, the value
  tools.tidyBuildDir in .claude/openblack.local.json, then <paths.builds>/<name of the work tree>.
A file of src/Web is compiled only by the web build, so the desktop database has no command for it. The gate checks such
a file with the compile command of the web build (ob-web configure writes the compile_commands.json of that build) and
the target flags that the Emscripten compiler gives. Find the web build folder in this order: --web-build-dir, the
environment variable OB_TIDY_WEB_BUILD_DIR, the git setting openblack.tidyWebBuildDir, the value tools.tidyWebBuildDir in
.claude/openblack.local.json, then <paths.builds>/<name of the work tree>-web. Without that database, a change to a file
of src/Web stops the gate (exit 2 with --strict), as for the desktop database.
On Windows, clang-tidy needs the MSVC and Windows SDK headers. If INCLUDE is not set, the gate sets it from
OB_TIDY_MSVC_DIR, the git setting openblack.tidyMsvcDir or the value tools.tidyMsvcDir (the MSVC folder with include
inside), else from the newest installed MSVC 14.40 or 14.41 (the newer ones need a newer clang).
"""

import argparse
import glob
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

SOURCE_FOLDERS = ("src/", "apps/", "components/", "test/")
WEB_FOLDER = "src/Web/"
CPP_EXTENSIONS = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl")
TU_EXTENSIONS = (".c", ".cc", ".cpp", ".cxx")

HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")
FINDING = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): (?P<kind>warning|error): (?P<text>.*?)(?: \[(?P<check>[^\]]+)\])?$")
NOLINT = re.compile(r"NOLINT(?P<kind>NEXTLINE|BEGIN|END)?(?:\((?P<checks>[^)]*)\))?")


def parse_diff_lines(diff_text):
    """Read a unified diff made with -U0. Return {path: [(first, last), ...]} of the added or changed lines."""
    changed = {}
    path = None
    for raw in diff_text.splitlines():
        line = raw.rstrip("\r")
        if line.startswith("+++ "):
            name = line[4:].split("\t")[0]
            path = None if name == "/dev/null" else (name[2:] if name.startswith("b/") else name)
            continue
        if line.startswith("--- ") or path is None:
            continue
        match = HUNK.match(line)
        if not match:
            continue
        first = int(match.group(1))
        count = 1 if match.group(2) is None else int(match.group(2))
        if count == 0:
            continue
        changed.setdefault(path, []).append((first, first + count - 1))
    return changed


def is_checked_file(path):
    """A file of the folders that the format check covers, with a C++ extension."""
    return path.startswith(SOURCE_FOLDERS) and path.lower().endswith(CPP_EXTENSIONS)


def line_filter(changed):
    """The JSON text for the -line-filter option of clang-tidy. A name has the separators of this system."""
    entries = [{"name": os.path.normpath(path), "lines": [[a, b] for a, b in ranges]} for path, ranges in sorted(changed.items())]
    return json.dumps(entries, separators=(",", ":"))


def in_ranges(line, ranges):
    return any(a <= line <= b for a, b in ranges)


def _comment_text(line):
    """The text of the comment on a line (the part after // or /*), or an empty string."""
    for marker in ("//", "/*"):
        at = line.find(marker)
        if at >= 0:
            return line[at + 2 :]
    return ""


def _has_reason(text):
    """A reason is at least one word of three letters after the NOLINT marks are removed."""
    cleaned = NOLINT.sub(" ", text).replace("*/", " ")
    return re.search(r"[A-Za-z]{3,}", cleaned) is not None


def nolint_findings(lines, ranges):
    """Return [(line number, message)] for each NOLINT on a changed line without a check name or without a reason.

    lines are the lines of the file after the change. ranges are the changed line ranges. NOLINTEND needs neither.
    """
    found = []
    for index, text in enumerate(lines):
        number = index + 1
        if not in_ranges(number, ranges):
            continue
        comment = _comment_text(text)
        for match in NOLINT.finditer(comment):
            if match.group("kind") == "END":
                continue
            checks = (match.group("checks") or "").strip()
            if not checks or checks == "*":
                found.append((number, "NOLINT must name the checks it hides, for example NOLINT(check-name)"))
                continue
            same_line = _has_reason(comment)
            before = lines[index - 1].strip() if index > 0 else ""
            line_before = before.startswith("//") and _has_reason(before[2:])
            if not same_line and not line_before:
                found.append((number, "NOLINT needs a reason comment on the same line or on the line before"))
    return found


def parse_tidy_output(text, changed, root):
    """Return the findings of a clang-tidy run that are on changed lines: [(path, line, check, text)]."""
    normal = {os.path.normcase(os.path.normpath(os.path.join(root, p))): (p, r) for p, r in changed.items()}
    found = []
    seen = set()
    for raw in text.splitlines():
        match = FINDING.match(raw.rstrip("\r"))
        if not match or match.group("kind") == "note":
            continue
        key = os.path.normcase(os.path.normpath(os.path.join(root, match.group("file"))))
        if key not in normal:
            continue
        path, ranges = normal[key]
        line = int(match.group("line"))
        if not in_ranges(line, ranges):
            continue
        item = (path, line, match.group("check") or "", match.group("text"))
        if item not in seen:
            seen.add(item)
            found.append(item)
    return found


def pick_template_entry(database, path, root):
    """For a header: the compile command of a source file of the same folder, else of the nearest folder above."""
    folder = os.path.dirname(os.path.normpath(os.path.join(root, path)))
    entries = [(os.path.normcase(os.path.dirname(os.path.normpath(e["file"]))), e) for e in database]
    root_case = os.path.normcase(os.path.normpath(root))
    while True:
        here = os.path.normcase(folder)
        for entry_folder, entry in entries:
            if entry_folder == here:
                return entry
        for entry_folder, entry in entries:
            if entry_folder.startswith(here + os.sep) and entry["file"].lower().endswith(TU_EXTENSIONS):
                return entry
        parent = os.path.dirname(folder)
        if parent == folder or not os.path.normcase(parent).startswith(root_case):
            break
        folder = parent
    return entries[0][1] if entries else None


def _git(*args, cwd=None):
    result = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, errors="replace", stdin=subprocess.DEVNULL)
    return result.returncode, result.stdout


def _config_value(root, key):
    script = Path(root) / ".claude" / "obconfig.py"
    if not script.is_file():
        return ""
    for python in (sys.executable, "python", "python3"):
        try:
            result = subprocess.run([python, str(script), "get", key], cwd=root, capture_output=True, text=True,
                                    stdin=subprocess.DEVNULL, timeout=20)
        except (OSError, subprocess.SubprocessError):
            continue
        if result.returncode == 0:
            return result.stdout.strip()
        return ""
    return ""


def _setting(root, env_name, git_key, config_key):
    value = os.environ.get(env_name, "").strip()
    if value:
        return value
    value = _git("config", "--get", git_key, cwd=root)[1].strip()
    if value:
        return value
    return _config_value(root, config_key)


def _is_v17(program):
    try:
        out = subprocess.run([program, "--version"], capture_output=True, text=True, timeout=30,
                             stdin=subprocess.DEVNULL).stdout
    except (OSError, subprocess.SubprocessError):
        return False
    return re.search(r"version 17\.", out) is not None


def find_clang_tidy(root):
    folder = _setting(root, "OB_CLANG_TIDY_DIR", "openblack.clangTidyDir", "tools.clangTidy17Dir")
    candidates = []
    if folder:
        for name in ("clang-tidy.exe", "clang-tidy"):
            candidates.append(os.path.join(folder, name))
    candidates += [shutil.which("clang-tidy-17"), shutil.which("clang-tidy")]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate) and _is_v17(candidate):
            return candidate
    return None


def find_build_dir(root, given):
    candidates = [given] if given else []
    candidates.append(_setting(root, "OB_TIDY_BUILD_DIR", "openblack.tidyBuildDir", "tools.tidyBuildDir"))
    builds = _config_value(root, "paths.builds")
    if builds:
        candidates.append(os.path.join(builds, os.path.basename(root)))
    for candidate in candidates:
        if candidate and os.path.isfile(os.path.join(candidate, "compile_commands.json")):
            return candidate
    return None


def find_web_build_dir(root, given):
    """The folder of the web build with its compile_commands.json, or None."""
    candidates = [given] if given else []
    candidates.append(_setting(root, "OB_TIDY_WEB_BUILD_DIR", "openblack.tidyWebBuildDir", "tools.tidyWebBuildDir"))
    builds = _config_value(root, "paths.builds")
    if builds:
        candidates.append(os.path.join(builds, os.path.basename(root) + "-web"))
    for candidate in candidates:
        if candidate and os.path.isfile(os.path.join(candidate, "compile_commands.json")):
            return candidate
    return None


def is_web_only(path, desktop_files):
    """A file of src/Web that the desktop build does not compile. desktop_files holds the normalised paths it does."""
    return path.startswith(WEB_FOLDER) and os.path.normcase(os.path.normpath(path)) not in desktop_files


def web_target_flags(compiler):
    """The flags that tell clang the web target, from the Emscripten compiler of a compile command.

    em++ --cflags names the target, the sysroot and the include folders of the Emscripten headers. The options for the
    code generator (-mllvm) are not for the checks and clang 17 may not know them, so they are left out.
    """
    folder = os.path.dirname(os.path.normpath(compiler))
    emsdk = os.path.dirname(os.path.dirname(folder))
    env = dict(os.environ)
    env.setdefault("EMSDK", emsdk)
    env.setdefault("EM_CONFIG", os.path.join(emsdk, ".emscripten"))
    python = sorted(glob.glob(os.path.join(emsdk, "python", "*", "python.exe")) + glob.glob(os.path.join(emsdk, "python", "*", "bin", "python3")))
    if python:
        env.setdefault("EMSDK_PYTHON", python[-1])
    try:
        done = subprocess.run([compiler, "--cflags"], capture_output=True, text=True, errors="replace", env=env,
                              stdin=subprocess.DEVNULL, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if done.returncode != 0 or "wasm32" not in done.stdout:
        return None
    words = shlex.split(done.stdout.strip(), posix=True)
    flags = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word == "-mllvm":
            skip = True
        else:
            flags.append(word)
    return flags


def web_command(command, flags, clang):
    """The compile command of the web build for clang-tidy: a clang driver, the target flags, no Emscripten settings."""
    words = shlex.split(command, posix=False)
    start = next((i for i, w in enumerate(words) if web_compiler_of(w)), 0)
    rest = [w for w in words[start + 1 :] if not re.match(r"^-s[A-Z_]+(=|$)", w)]
    return " ".join([f'"{clang}"'] + [f'"{f}"' if " " in f else f for f in flags] + rest)


def find_msvc_dir(root):
    """The MSVC folder (with include inside) that clang-tidy 17 reads, or an empty string off Windows."""
    if os.name != "nt":
        return ""
    msvc = _setting(root, "OB_TIDY_MSVC_DIR", "openblack.tidyMsvcDir", "tools.tidyMsvcDir")
    if msvc:
        return msvc
    found = []
    for base in ("C:/Program Files", "C:/Program Files (x86)"):
        found += glob.glob(base + "/Microsoft Visual Studio/*/*/VC/Tools/MSVC/14.4[01].*")
    return os.path.normpath(sorted(found, key=lambda p: os.path.basename(p))[-1]) if found else ""


def windows_include_paths(msvc):
    """The INCLUDE value for clang-tidy on Windows, or an empty string if nothing is needed or found."""
    if os.name != "nt" or os.environ.get("INCLUDE"):
        return ""
    kits = sorted(glob.glob("C:/Program Files (x86)/Windows Kits/10/Include/10.*/ucrt"))
    if not msvc or not kits:
        return ""
    kit = os.path.dirname(kits[-1])
    parts = [os.path.join(msvc, "include"), os.path.join(msvc, "atlmfc", "include")]
    parts += [os.path.join(kit, name) for name in ("ucrt", "um", "shared", "winrt", "cppwinrt")]
    return ";".join(p.replace("/", "\\") for p in parts)


def with_compiler(command, compiler):
    """The compile command with its first word (the compiler, quoted or not) replaced."""
    command = command.lstrip()
    if command.startswith('"'):
        end = command.find('"', 1)
        rest = command[end + 1 :]
    else:
        rest = command[len(command.split(None, 1)[0]) :]
    return f'"{compiler}"{rest}'


def web_compiler_of(command):
    """The path of the Emscripten compiler in a compile command of the web build, or an empty string."""
    for word in shlex.split(command, posix=False):
        name = os.path.basename(word.strip('"').replace("\\", "/")).lower()
        if name.startswith(("em++", "emcc")):
            return word.strip('"')
    return ""


def run_gate(changed, root, build_dir, tool, jobs, timeout, strict, web=False):
    """Run clang-tidy on each changed file. Return (findings, problems).

    web is true for the files of src/Web: build_dir is then the web build, whose commands name the Emscripten compiler.
    """
    with open(os.path.join(build_dir, "compile_commands.json"), encoding="utf-8") as handle:
        database = json.load(handle)
    by_file = {os.path.normcase(os.path.normpath(e["file"])): e for e in database}
    entries = []
    problems = []
    msvc = "" if web else find_msvc_dir(root)
    compiler = os.path.join(msvc, "bin", "Hostx64", "x64", "cl.exe") if msvc else ""
    web_flags = None
    web_clang = ""
    if web:
        compilers = [web_compiler_of(e.get("command") or " ".join(e.get("arguments", []))) for e in database]
        em_compiler = next((c for c in compilers if c), "")
        web_flags = web_target_flags(em_compiler) if em_compiler else None
        if web_flags is None:
            problems.append(f"src/Web: the target flags of the Emscripten compiler are not found (em++ --cflags failed) for {build_dir}")
            return [], problems
        emsdk = os.path.dirname(os.path.dirname(os.path.dirname(os.path.normpath(em_compiler))))
        clangs = glob.glob(os.path.join(emsdk, "upstream", "bin", "clang++*"))
        web_clang = clangs[0] if clangs else "clang++"
    for path in sorted(changed):
        full = os.path.normpath(os.path.join(root, path))
        entry = by_file.get(os.path.normcase(full))
        if entry is None and path.lower().endswith(TU_EXTENSIONS):
            problems.append(f"{path}: no compile command in {build_dir} (configure the build folder again)")
            continue
        if entry is None:
            template = pick_template_entry(database, path, root)
            if template is None:
                problems.append(f"{path}: no compile command to borrow for a header")
                continue
            entry = dict(template)
            command = entry.get("command") or " ".join(entry.get("arguments", []))
            for style in ("/", "\\"):
                command = command.replace(entry["file"].replace("\\", "/").replace("/", style), full.replace("\\", style))
            entry["command"] = command
        command = entry.get("command") or " ".join(entry.get("arguments", []))
        if web:
            command = web_command(command, web_flags, web_clang)
        elif msvc and os.path.isfile(compiler):
            # clang-tidy reads the MSVC headers from the folder of the compiler named in the command, so name 14.41.
            command = with_compiler(command, compiler)
        entries.append({"directory": entry["directory"], "command": command, "file": full})

    findings = []
    if not entries:
        return findings, problems
    with tempfile.TemporaryDirectory(prefix="ob-tidy-") as temp:
        with open(os.path.join(temp, "compile_commands.json"), "w", encoding="utf-8") as handle:
            json.dump(entries, handle)
        env = dict(os.environ)
        include = "" if web else windows_include_paths(msvc)
        if include:
            env["INCLUDE"] = include
        filt = line_filter(changed)

        def one(entry):
            command = [tool, "-p", temp, entry["file"], "--quiet", f"--line-filter={filt}",
                       "--extra-arg=-Wno-unused-command-line-argument", "--extra-arg=-Wno-error",
                       "--extra-arg=-fbracket-depth=1024"]
            try:
                done = subprocess.run(command, capture_output=True, text=True, errors="replace", env=env,
                                      stdin=subprocess.DEVNULL, timeout=timeout)
            except subprocess.TimeoutExpired:
                return entry["file"], None
            crashed = "Stack dump" in done.stderr or "LLVM ERROR" in done.stderr
            return entry["file"], (None if crashed else done.stdout)

        with ThreadPoolExecutor(max(1, jobs)) as pool:
            for file, output in pool.map(one, entries):
                relative = os.path.relpath(file, root).replace("\\", "/")
                if output is None:
                    problems.append(f"{relative}: clang-tidy did not finish (more than {timeout} s, or it crashed)")
                    continue
                findings += parse_tidy_output(output, {k: v for k, v in changed.items() if k == relative}, root)
    return findings, problems


def read_lines(root, path, source):
    code, text = _git("show", f"{source}:{path}", cwd=root)
    return text.splitlines() if code == 0 else []


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", choices=("staged", "range"))
    parser.add_argument("base", nargs="?")
    parser.add_argument("--build-dir")
    parser.add_argument("--web-build-dir")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args(argv)

    root = _git("rev-parse", "--show-toplevel")[1].strip()
    if not root:
        print("tidy gate: this is not a git work tree.", file=sys.stderr)
        return 2 if args.strict else 0
    if args.mode == "staged":
        diff = _git("diff", "--cached", "-U0", "--no-color", "--diff-filter=ACMR", "--", *SOURCE_FOLDERS, cwd=root)[1]
        source = ""
    else:
        if not args.base:
            parser.error("range needs a base")
        diff = _git("diff", "-U0", "--no-color", "--diff-filter=ACMR", f"{args.base}...HEAD", "--", *SOURCE_FOLDERS, cwd=root)[1]
        source = "HEAD"
    changed = {p: r for p, r in parse_diff_lines(diff).items() if is_checked_file(p)}
    ignore = Path(root, ".clang-format-ignore")
    if ignore.is_file():
        skipped = {x.strip() for x in ignore.read_text(encoding="utf-8").splitlines() if x.strip()}
        changed = {p: r for p, r in changed.items() if p not in skipped}
    if not changed:
        return 0

    failed = []
    for path, ranges in sorted(changed.items()):
        for number, message in nolint_findings(read_lines(root, path, source), ranges):
            failed.append((path, number, "openblack-nolint", message))

    def cannot(why):
        print(f"tidy gate: {why}", file=sys.stderr)
        return 2 if args.strict else 0

    status = 0
    tool = find_clang_tidy(root)
    build_dir = find_build_dir(root, args.build_dir)
    web_dir = find_web_build_dir(root, args.web_build_dir)
    desktop_files = set()
    if build_dir is not None:
        with open(os.path.join(build_dir, "compile_commands.json"), encoding="utf-8") as handle:
            desktop_files = {os.path.normcase(os.path.normpath(os.path.relpath(e["file"], root) if os.path.isabs(e["file"]) else e["file"]))
                             for e in json.load(handle)}
    web_changed = {p: r for p, r in changed.items() if is_web_only(p, desktop_files)}
    desktop_changed = {p: r for p, r in changed.items() if p not in web_changed}
    if tool is None:
        status = cannot("clang-tidy 17 is not found: the check of the changed lines is skipped. Set OB_CLANG_TIDY_DIR, "
                        "git config openblack.clangTidyDir, or tools.clangTidy17Dir.")
    else:
        if desktop_changed:
            if build_dir is None:
                status = cannot("no compile_commands.json is found: the check of the changed lines is skipped. Set "
                                "OB_TIDY_BUILD_DIR, git config openblack.tidyBuildDir, or tools.tidyBuildDir.")
            else:
                findings, problems = run_gate(desktop_changed, root, build_dir, tool, args.jobs, args.timeout, args.strict)
                failed += findings
                for problem in problems:
                    status = max(status, cannot(problem))
        if web_changed:
            if web_dir is None:
                status = max(status, cannot("the changed files of src/Web need the compile_commands.json of the web build, and "
                                            "it is not found. Run ob-web configure, then set OB_TIDY_WEB_BUILD_DIR, git config "
                                            "openblack.tidyWebBuildDir, or tools.tidyWebBuildDir."))
            else:
                findings, problems = run_gate(web_changed, root, web_dir, tool, args.jobs, args.timeout, args.strict, web=True)
                failed += findings
                for problem in problems:
                    status = max(status, cannot(problem))

    if failed:
        print("tidy gate: findings on the changed lines (clang-tidy 17):", file=sys.stderr)
        for path, number, check, text in failed:
            print(f"  {path}:{number}: {text} [{check}]", file=sys.stderr)
        print("tidy gate: fix each finding. Hide one only with NOLINT(check-name) and a reason comment.", file=sys.stderr)
        return 1
    return status


if __name__ == "__main__":
    sys.exit(main())
