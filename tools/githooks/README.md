# Git hooks

Install the hooks with `sh tools/githooks/install.sh`. The script puts them in the common git folder, so they apply to
every worktree. Do not skip a hook (no `--no-verify`).

| File | Use |
|---|---|
| `pre-commit` | Checks the staged lines of C++ files. First clang-format 17, then clang-tidy 17. |
| `pre-merge-commit` | Checks that every C++ file of a merge matches clang-format 17. |
| `clang-format-gate.sh` | The whole-file format check. The merge-train car and `pre-merge-commit` use it. |
| `tidy_gate.py` | The clang-tidy 17 check of the changed lines. The hook and the merge-train car use it. |
| `test_tidy_gate.py` | Tests of the diff and line-filter logic of `tidy_gate.py`. Run: `python tools/githooks/test_tidy_gate.py`. |

## The clang-tidy check

New code must not add clang-tidy 17 findings. The check looks only at the lines that your change adds or edits. A
finding on an old line never stops a commit.

1. The hook reads `git diff --cached -U0` and makes a line filter from it.
2. It runs clang-tidy 17 on each staged C++ file, with the compile command of the build folder.
3. A finding on a changed line stops the commit. The hook prints the file, the line, the text and the check name.
4. A header borrows the compile command of a source file in the same folder.

### NOLINT rule

A `NOLINT` on a changed line is a finding when one of these is true:

- It has no check name. Write `NOLINT(check-name)`. `NOLINT` and `NOLINT(*)` are not allowed.
- It has no reason. Write the reason in a comment on the same line, or in a comment on the line before.

`NOLINTBEGIN(check-name)` follows the same rule. `NOLINTEND` needs neither.

```cpp
// The API of the library needs a raw pointer here.
Open(handle); // NOLINT(cppcoreguidelines-owning-memory)
```

### Settings

The hook needs clang-tidy 17 and a `compile_commands.json`. It looks for each in this order. The first match wins.

| What | Order |
|---|---|
| clang-tidy 17 folder | Environment variable `OB_CLANG_TIDY_DIR`. Git setting `openblack.clangTidyDir`. Value `tools.clangTidy17Dir` in `.claude/openblack.local.json`. `clang-tidy-17` or `clang-tidy` on `PATH`. |
| Build folder | Environment variable `OB_TIDY_BUILD_DIR`. Git setting `openblack.tidyBuildDir`. Value `tools.tidyBuildDir` in `.claude/openblack.local.json`. The folder `<paths.builds>/<name of the work tree>`. |
| Web build folder (for `src/Web`) | Environment variable `OB_TIDY_WEB_BUILD_DIR`. Git setting `openblack.tidyWebBuildDir`. Value `tools.tidyWebBuildDir` in `.claude/openblack.local.json`. The folder `<paths.builds>/<name of the work tree>-web` (the default of `ob-web`). |
| MSVC folder (Windows) | Environment variable `OB_TIDY_MSVC_DIR`. Git setting `openblack.tidyMsvcDir`. Value `tools.tidyMsvcDir`. Else the newest MSVC 14.40 or 14.41 on the machine. |

Only version 17 of clang-tidy is used. To get it, run `python -m pip install --target <folder> clang-tidy==17.0.1`.
The folder with `clang-tidy` inside it (on Windows, `<folder>/clang_tidy/data/bin`) is the value to set.

The build folder must come from a CMake configure of the same work tree. Configure again after you add a source file.

The files of `src/Web` build only in the web build (Emscripten), so the desktop database has no command for them. The check
runs them with the `compile_commands.json` of the web build: `ob-web configure` writes it. The check takes the target
flags from the Emscripten compiler (`em++ --cflags`). If that web database is not found, a change to a file of `src/Web`
is treated like a missing compile database (the hook passes with one line, the car fails).

If clang-tidy 17 or the compile database is not found, the hook prints one line and passes. If one file takes more than
120 seconds, the hook prints the file name and passes. The merge-train car runs the same check with `--strict`: there,
a missing tool, a missing compile database or a time-out fails the car.

### Run it by hand

- Staged changes: `python tools/githooks/tidy_gate.py staged`
- A branch against the main branch: `python tools/githooks/tidy_gate.py range me/master --strict`
- Options: `--build-dir <folder>`, `--jobs <n>`, `--timeout <seconds>`.

Exit codes: 0 clean, 1 a finding on a changed line, 2 the check cannot run (only with `--strict`).

### How to fix a finding

1. Change the code so that the check passes. This is the best fix.
2. If the code must stay, add `NOLINT(check-name)` with a reason (see the rule above).
3. Do not turn a check off in `.clang-tidy` to make one commit pass.
