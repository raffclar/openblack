#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The picture repeat check: the same game run more than once, with a seed and a fixed frame time, must draw the same
picture at the same frame.

Each run is one game that saves the picture of one frame and ends a few frames later, by its frame count. The runs play
one after the other, in the background. The check compares each picture with the picture of the first run.

    python tools/sim/picture_repeat.py --exe <openblack.exe> --out <folder> --frames 300,600 \
        -- --scenario multiplayer.gods_ring_map --scenario-hide-window

Exit code 0: all pictures are the same; 1: a picture is not the same or is missing.
"""

import argparse
import os
import subprocess
import sys

# The frames that a game plays after the frame of its picture, so that the renderer writes the file before the end
FRAMES_AFTER_PICTURE = 30


def parse_frames(text):
    """The frames of the pictures, from '300,600', each once and in order"""
    frames = sorted({int(part) for part in text.split(",") if part.strip()})
    if not frames or frames[0] < 1:
        raise ValueError(f"the frames must be 1 or more: {text}")
    return frames


def game_arguments(options, frame, picture, log, profile):
    """The command line of one run that saves the picture of this frame"""
    return [options.exe, "-g", options.game_path, "--seed", str(options.seed), "--sim-frame-ms", str(options.frame_ms),
            "--background", "--skip-logos", "--screenshot-frame", str(frame), "--screenshot-path", picture,
            "-n", str(frame + FRAMES_AFTER_PICTURE), "--profile-folder", profile, "-l", log] + list(options.extra)


def compare(first, other):
    """'same', 'missing' or 'differs' for two picture files. The renderer writes the same file for the same pixels."""
    if not os.path.isfile(first) or not os.path.isfile(other):
        return "missing"
    with open(first, "rb") as a, open(other, "rb") as b:
        return "same" if a.read() == b.read() else "differs"


def different_pixels(first, other):
    """The count of the pixels that are not the same, when Pillow is there to read the pictures"""
    try:
        from PIL import Image, ImageChops
    except ImportError:
        return None
    a = Image.open(first).convert("RGB")
    b = Image.open(other).convert("RGB")
    if a.size != b.size:
        return None
    return sum(1 for pixel in ImageChops.difference(a, b).getdata() if pixel != (0, 0, 0))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--game-path", default="C:/projects/black")
    parser.add_argument("--out", required=True)
    parser.add_argument("--frames", default="300", help="the frames of the pictures, as '300,600'")
    parser.add_argument("--runs", type=int, default=2)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--frame-ms", type=float, default=20.0)
    parser.add_argument("extra", nargs="*", help="more arguments for the game, after --")
    options = parser.parse_args(argv)
    if options.runs < 2:
        parser.error("--runs must be 2 or more")
    frames = parse_frames(options.frames)
    os.makedirs(options.out, exist_ok=True)

    all_same = True
    for frame in frames:
        pictures = []
        for run in range(1, options.runs + 1):
            name = os.path.join(options.out, f"frame{frame}-run{run}")
            os.makedirs(name + ".profile", exist_ok=True)
            picture = name + ".png"
            arguments = game_arguments(options, frame, picture, name + ".log", name + ".profile")
            code = subprocess.call(arguments, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
            print(f"frame {frame} run {run}: exit {code}", flush=True)
            pictures.append(picture)
        for run, picture in enumerate(pictures[1:], start=2):
            verdict = compare(pictures[0], picture)
            detail = ""
            if verdict == "differs":
                pixels = different_pixels(pictures[0], picture)
                detail = f" ({pixels} pixels)" if pixels is not None else ""
            print(f"frame {frame} run {run} against run 1: {verdict}{detail}", flush=True)
            all_same = all_same and verdict == "same"
    print("SAME" if all_same else "NOT THE SAME")
    return 0 if all_same else 1


if __name__ == "__main__":
    sys.exit(main())
