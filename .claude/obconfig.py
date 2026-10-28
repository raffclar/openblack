"""Read the local settings of the openblack Claude Code skills.

Each developer keeps their own paths, remotes and limits in .claude/openblack.local.json (git ignores it). A worktree
does not have the ignored file, so the reader also looks in the main checkout of the same repository.

The search order:
  1. the file that OPENBLACK_CONFIG names;
  2. .claude/openblack.local.json in the worktree that holds this file;
  3. .claude/openblack.local.json in the main checkout (found from the git common directory).
Values that are not in the file come from .claude/openblack.local.example.json. A relative path in "paths" is
relative to the main checkout.

usage:
  python .claude/obconfig.py get <dotted.key>    print one value (a list prints one item a line)
  python .claude/obconfig.py shell               print "export OB_...=..." lines for the shell scripts
  python .claude/obconfig.py where               print the file that the reader uses
"""

import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
EXAMPLE = HERE / "openblack.local.example.json"
LOCAL_NAME = "openblack.local.json"

# The shell names of the values that the merge-train scripts use
SHELL_KEYS = {
    "OB_REMOTE": "git.remote",
    "OB_MAIN_BRANCH": "git.mainBranch",
    "OB_GITHUB_REPO": "git.githubRepo",
    "OB_MAIN_CHECKOUT": "paths.mainCheckout",
    "OB_WORKTREES": "paths.worktrees",
    "OB_BUILDS": "paths.builds",
    "OB_HANDOVERS": "paths.handovers",
    "OB_TRAIN": "paths.mergeTrain",
    "OB_LEAD_LOG": "paths.leadLog",
    "OB_GAME_DATA": "paths.gameData",
    "OB_CLANG_FORMAT": "tools.clangFormat",
    "OB_BUILD_TOOL": "tools.obBuild",
    "OB_PYTHON": "tools.python",
    "OB_TRAILER": "commit.trailer",
    "OB_TRAIN_TRAILER": "commit.trainTrailer",
    "OB_EXTRA_CHECK": "train.extraCheck",
}


def git(*args):
    # Ask the checkout that holds this file, so the reader works from any current folder
    try:
        out = subprocess.run(["git", "-C", str(HERE), *args], capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return None
    return out.stdout.strip()


def find_local():
    named = os.environ.get("OPENBLACK_CONFIG")
    if named:
        return Path(named)
    top = git("rev-parse", "--show-toplevel")
    if top:
        here = Path(top) / ".claude" / LOCAL_NAME
        if here.is_file():
            return here
    common = git("rev-parse", "--path-format=absolute", "--git-common-dir")
    if common:
        main = Path(common).parent / ".claude" / LOCAL_NAME
        if main.is_file():
            return main
    return None


def merge(base, over):
    out = dict(base)
    for key, value in over.items():
        out[key] = merge(out[key], value) if isinstance(value, dict) and isinstance(out.get(key), dict) else value
    return out


def load():
    config = json.loads(EXAMPLE.read_text(encoding="utf-8"))
    local = find_local()
    if local is not None:
        config = merge(config, json.loads(local.read_text(encoding="utf-8")))
    top = git("rev-parse", "--show-toplevel")
    paths = config.setdefault("paths", {})
    if not paths.get("mainCheckout"):
        common = git("rev-parse", "--path-format=absolute", "--git-common-dir")
        paths["mainCheckout"] = str(Path(common).parent) if common else (top or "")
    # A relative path is relative to the main checkout, so every worktree reads the same folders
    base = Path(paths["mainCheckout"]) if paths["mainCheckout"] else Path.cwd()

    def absolute(value):
        return str((base / value).resolve()) if value and not Path(value).is_absolute() else value

    for key, value in paths.items():
        if key != "mainCheckout":
            paths[key] = [absolute(v) for v in value] if isinstance(value, list) else absolute(value)
    tools = config.setdefault("tools", {})
    if not tools.get("obBuild") and paths["mainCheckout"]:
        tools["obBuild"] = str(Path(paths["mainCheckout"]) / "tools" / "build" / "ob-build")
    return config, local


def lookup(config, dotted):
    value = config
    for part in dotted.split("."):
        if not isinstance(value, dict) or part not in value:
            return None
        value = value[part]
    return value


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    config, local = load()
    command = sys.argv[1]
    if command == "where":
        print(local if local else f"no {LOCAL_NAME}: the example values apply ({EXAMPLE})")
        return 0
    if command == "get" and len(sys.argv) == 3:
        value = lookup(config, sys.argv[2])
        if value is None:
            print(f"no value for {sys.argv[2]}", file=sys.stderr)
            return 1
        print("\n".join(map(str, value)) if isinstance(value, list) else value)
        return 0
    if command == "shell":
        for name, dotted in SHELL_KEYS.items():
            value = lookup(config, dotted)
            print(f"export {name}={shlex.quote('' if value is None else str(value))}")
        return 0
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main())
