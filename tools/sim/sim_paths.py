"""The default places of the sim tools on each computer: the game data, the build folders and the names of the programs.

On Windows the builds are in E:/openblack/builds/<worktree>/bin/<config> (ob-build) and the game data is in
C:/projects/black. On the Mac build box the lanes root is ~/projects/openblack: the trees are in trees/<name>, the builds in
builds/<name>/<config>/bin (ob-mac.sh) and the game data is in ~/projects/black. OB_BUILDS and OB_GAME_PATH change the
defaults on each computer.
"""

import os
import shutil
import sys

IS_MAC = sys.platform == "darwin"
EXE = ".exe" if os.name == "nt" else ""
LANES_ROOT = os.path.expanduser("~/projects/openblack")

if IS_MAC:
    DEFAULT_BUILDS = os.environ.get("OB_BUILDS", os.path.join(LANES_ROOT, "builds"))
    DEFAULT_GAME_PATH = os.environ.get("OB_GAME_PATH", os.path.expanduser("~/projects/black"))
    DEFAULT_RUNS = os.path.join(LANES_ROOT, "sim-runs")
else:
    DEFAULT_BUILDS = os.environ.get("OB_BUILDS", "E:/openblack/builds")
    DEFAULT_GAME_PATH = os.environ.get("OB_GAME_PATH", "C:/projects/black")
    DEFAULT_RUNS = "E:/openblack/sim-runs"


def worktree_name():
    """The name of the tree of these tools (the folder above tools/sim)"""
    return os.path.basename(os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))


def default_bin(config):
    """The folder of the programs of this tree's build for a configuration"""
    if IS_MAC:
        return os.path.join(DEFAULT_BUILDS, worktree_name(), config, "bin")
    return os.path.join(DEFAULT_BUILDS, worktree_name(), "bin", config)


def program(folder, name):
    """The path of a program (openblack or openblack-server) in a folder, with the name that this computer uses"""
    return os.path.join(folder, name + EXE)


# The switches of the original game. The game looks for them in its whole command line, the program path included: a
# "-" or a "/" followed by one of these names, in any case, starts that switch. A "\" does not.
LAUNCH_SWITCHES = ("NEWGAME", "MULTIPLAYER", "LAND", "SKIRMISH", "PREINTROVIDEO", "FORCEINETCONN", "NOINETCONN",
                   "EDITOR", "CONVERT", "VERSION", "NOLOADMUSIC", "SETTINGS:")


def launch_switch_in(text):
    """The first switch of the original game that the game finds in a text, as its scan of the command line finds it,
    or None. The character directly after a "-" or a "/" is not a switch start, so "--land" holds no switch."""
    upper = text.upper()
    index = 0
    while index < len(upper):
        if upper[index] not in "-/":
            index += 1
            continue
        rest = upper[index + 1:]
        for name in LAUNCH_SWITCHES:
            if rest.startswith(name):
                return name
        index += 2
    return None


def _short_path(path):
    """The short (8.3) form of a Windows path that exists, or None"""
    if os.name != "nt" or not os.path.exists(path):
        return None
    import ctypes
    size = ctypes.windll.kernel32.GetShortPathNameW(path, None, 0)
    if size == 0:
        return None
    buffer = ctypes.create_unicode_buffer(size)
    if ctypes.windll.kernel32.GetShortPathNameW(path, buffer, size) == 0:
        return None
    return buffer.value


def _looks_like_path(text):
    return os.path.isabs(text) or "/" in text or os.sep in text


def _switch_safe_path(path, cwd):
    """A form of a path that holds no switch of the original game: the path with "\\" on Windows, the path from the
    working folder, or the short form. None if no form is safe."""
    # A relative path is from the working folder of the game
    full = os.path.abspath(os.path.join(cwd, path))
    forms = []
    if os.name == "nt":
        forms.append(path.replace("/", "\\"))
    try:
        forms.append(os.path.relpath(full, os.path.abspath(cwd)))
    except ValueError:
        pass  # Another drive than the working folder
    short = _short_path(full)
    if short:
        forms.append(short)
    return next((form for form in forms if launch_switch_in(form) is None), None)


def switch_safe_command(command, cwd):
    """The program and the arguments to start a game with, so that no path starts a switch of the original game.

    The game scans its whole command line for the switches, the program path and the paths of the options too, as the
    original game does. So a build folder or a run folder such as "cg-skirmish-fights-1" opens the skirmish box. This
    gives the program path apart, as the executable, and puts only the name of the program in the command line. Each
    path argument that holds a switch changes to a form with no switch. Arguments that are not paths, such as a switch
    that the caller gives on purpose, stay as they are.

    A program name with no folder, such as "docker", changes to its full path on PATH: Windows does not look on PATH
    for the executable that Popen gives apart.

    Returns (executable, arguments) for subprocess.Popen(arguments, executable=executable, cwd=cwd). Raises ValueError
    when a path holds a switch in every form."""
    executable = command[0]
    if not os.path.dirname(executable):
        executable = shutil.which(executable) or executable
    arguments = [os.path.basename(executable)]
    for argument in command[1:]:
        prefix, value = "", argument
        if argument.startswith("--") and "=" in argument:
            prefix, value = argument.split("=", 1)
            prefix += "="
        name = launch_switch_in(value)
        if name is None or not _looks_like_path(value):
            arguments.append(argument)
            continue
        safe = _switch_safe_path(value, cwd)
        if safe is None:
            raise ValueError(f"the path {value} holds the switch -{name} of the original game, and the game would start "
                             "that switch: use a folder name with no switch name after a '-' or a '/'")
        arguments.append(prefix + safe)
    if launch_switch_in(arguments[0]) is not None:
        raise ValueError(f"the program name {arguments[0]} holds a switch of the original game")
    return executable, arguments
