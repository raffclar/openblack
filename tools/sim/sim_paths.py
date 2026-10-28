"""The default places of the sim tools on each computer: the game data, the build folders and the names of the programs.

On Windows the builds are in E:/openblack/builds/<worktree>/bin/<config> (ob-build) and the game data is in
C:/projects/black. On the Mac build box the lanes root is ~/projects/openblack: the trees are in trees/<name>, the builds in
builds/<name>/<config>/bin (ob-mac.sh) and the game data is in ~/projects/black. OB_BUILDS and OB_GAME_PATH change the
defaults on each computer.
"""

import os
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
