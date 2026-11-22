#!/usr/bin/env python3
"""Make the sky pack that the debug menu shows: the nine pictures that the game reads from <profile>/sky_pictures.

Each picture of the game has two halves: rows 0 to 127 and rows 128 to 255 are different parts of the sky. Each half
must be enlarged by itself, so that no half takes colours from the other half.

Two commands:
    standin <exported pictures> <output> [--scale 4]
        A stand-in pack for tests only: a plain bicubic enlargement of each half of the exported 256 x 256 pictures.
        Do not commit its pictures. The real pack replaces it.
    join <enlarged halves> <output>
        The real pack from an upscaler: puts each pair of enlarged halves, <name>_a.png (the top) and <name>_b.png (the
        bottom), back into one picture. The two halves must have the same width, and each must be half as high.
"""

import argparse
import pathlib
import sys

from PIL import Image

# The names of the nine pictures, in the order of the game
PICTURES = [
    "sky_good_night.png",
    "sky_good_dusk.png",
    "sky_good_day.png",
    "Sky_Ntrl_Night.png",
    "Sky_Ntrl_Dusk.png",
    "Sky_Ntrl_Day.png",
    "sky_evil_night.png",
    "sky_evil_dusk.png",
    "sky_evil_day.png",
]
# The side of the game's own pictures
SIDE = 256


def enlarge(picture: Image.Image, scale: int) -> Image.Image:
    """Enlarge each half of a picture by itself, with a bicubic filter."""
    half = SIDE // 2
    out = Image.new("RGB", (SIDE * scale, SIDE * scale))
    for top in (0, half):
        part = picture.crop((0, top, SIDE, top + half))
        out.paste(part.resize((SIDE * scale, half * scale), Image.Resampling.BICUBIC), (0, top * scale))
    return out


def join(source: pathlib.Path, output: pathlib.Path) -> int:
    """Put each pair of enlarged halves back into one picture."""
    output.mkdir(parents=True, exist_ok=True)
    for name in PICTURES:
        stem = name[: -len(".png")]
        top_path, bottom_path = source / f"{stem}_a.png", source / f"{stem}_b.png"
        for path in (top_path, bottom_path):
            if not path.is_file():
                print(f"missing half: {path}", file=sys.stderr)
                return 1
        top = Image.open(top_path).convert("RGB")
        bottom = Image.open(bottom_path).convert("RGB")
        side = top.size[0]
        if top.size != bottom.size or top.size[1] * 2 != side or side % SIDE != 0:
            print(f"{stem}: the halves are {top.size} and {bottom.size}; each must be N x N/2, N a multiple of {SIDE}",
                  file=sys.stderr)
            return 1
        out = Image.new("RGB", (side, side))
        out.paste(top, (0, 0))
        out.paste(bottom, (0, side // 2))
        out.save(output / name)
        print(f"{name}: {side} x {side}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    standin = commands.add_parser("standin", help="a bicubic stand-in pack for tests")
    standin.add_argument("source", type=pathlib.Path, help="the folder of the exported 256 x 256 PNG pictures")
    standin.add_argument("output", type=pathlib.Path, help="the folder for the stand-in pack")
    standin.add_argument("--scale", type=int, default=4, help="the whole scale of the enlargement (default 4)")
    joined = commands.add_parser("join", help="the pack from the enlarged halves of an upscaler")
    joined.add_argument("source", type=pathlib.Path, help="the folder of the enlarged halves, <name>_a.png and _b.png")
    joined.add_argument("output", type=pathlib.Path, help="the folder for the pack")
    args = parser.parse_args()
    if args.command == "join":
        return join(args.source, args.output)
    if args.scale < 1:
        print("the scale must be 1 or more", file=sys.stderr)
        return 2
    args.output.mkdir(parents=True, exist_ok=True)
    for name in PICTURES:
        path = args.source / name
        if not path.is_file():
            print(f"missing picture: {path}", file=sys.stderr)
            return 1
        picture = Image.open(path).convert("RGB")
        if picture.size != (SIDE, SIDE):
            print(f"{path} is {picture.size[0]} x {picture.size[1]}, not {SIDE} x {SIDE}", file=sys.stderr)
            return 1
        enlarge(picture, args.scale).save(args.output / name)
        print(f"{name}: {SIDE * args.scale} x {SIDE * args.scale}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
