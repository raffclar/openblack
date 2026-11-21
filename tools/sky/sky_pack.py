#!/usr/bin/env python3
"""Make the sky pack that the debug menu shows: the nine pictures that the game reads from <profile>/sky_pictures.

Each picture of the game has two halves: rows 0 to 127 and rows 128 to 255 are different parts of the sky. Each half
must be enlarged by itself, so that no half takes colours from the other half. A half is <name>_a.png (the top) or
<name>_b.png (the bottom).

The game's pictures have 5 bits for each channel. Where the sky is flat, the steps of one or two levels between
neighbour texels show as bands. The deband command makes a smooth gradient there, in 16-bit precision, and keeps the
larger jumps of the clouds.

Commands:
    standin <exported pictures> <output> [--scale 4]
        A stand-in pack for tests only: a plain bicubic enlargement of each half of the exported 256 x 256 pictures.
        Do not commit its pictures.
    deband <halves> <output> [--radius 6] [--flat 2] [--cell 0.5] [--passes 4]
        Removes the bands from the 18 halves of the game's pictures (256 x 128, from the export) and writes them as
        16-bit PNG halves of the same size.
    upscale <halves> <output> [--scale 4] [--detail <halves> --game <halves>] [--flat 2]
        Enlarges the halves with a bicubic filter, in 16-bit precision. With --detail, the result keeps the smooth
        gradients of the halves and takes the clouds from the detail halves (the game's own halves, or the halves of an
        upscaler, at any whole scale). The mask of the clouds comes from the game's halves (--game).
    join <halves> <output>
        Puts each pair of enlarged halves back into one picture. The two halves must have the same width, and each must
        be half as high. A pack of 16-bit halves gives 16-bit pictures.
"""

import argparse
import pathlib
import struct
import sys
import zlib

import numpy as np
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
# The highest level of a channel in the game's pictures
LEVELS = 31
# The signature at the start of each PNG file
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def halves() -> list[str]:
    """The names of the 18 halves, without the extension."""
    return [f"{name[: -len('.png')]}_{part}" for name in PICTURES for part in ("a", "b")]


# PNG files ------------------------------------------------------------------------------------------------------------


def png_depth(path: pathlib.Path) -> int:
    """The bits of each channel of a PNG file."""
    with open(path, "rb") as file:
        head = file.read(33)
    if head[:8] != PNG_SIGNATURE or head[12:16] != b"IHDR":
        raise ValueError(f"{path} is not a PNG file")
    return head[24]


def _unfilter(rows: bytes, height: int, stride: int, step: int) -> np.ndarray:
    """Removes the PNG row filters. The result has one row of bytes for each row of the picture."""
    out = np.zeros((height, stride), dtype=np.uint8)
    previous = np.zeros(stride, dtype=np.int32)
    for y in range(height):
        start = y * (stride + 1)
        kind = rows[start]
        line = np.frombuffer(rows, dtype=np.uint8, count=stride, offset=start + 1).astype(np.int32)
        if kind == 0:
            row = line
        elif kind == 1:
            # Each byte adds the byte of the texel on its left: a sum for each byte of a texel
            row = (np.cumsum(line.reshape(-1, step), axis=0) & 0xFF).reshape(-1)
        elif kind == 2:
            row = (line + previous) & 0xFF
        elif kind in (3, 4):
            raw, up = line.tolist(), previous.tolist()
            values = [0] * stride
            for i in range(stride):
                left = values[i - step] if i >= step else 0
                if kind == 3:
                    values[i] = (raw[i] + ((left + up[i]) >> 1)) & 0xFF
                else:
                    corner = up[i - step] if i >= step else 0
                    guess = left + up[i] - corner
                    near = min((abs(guess - left), 0, left), (abs(guess - up[i]), 1, up[i]),
                               (abs(guess - corner), 2, corner))
                    values[i] = (raw[i] + near[2]) & 0xFF
            row = np.array(values, dtype=np.int32)
        else:
            raise ValueError(f"unknown PNG filter {kind}")
        out[y] = row
        previous = row
    return out


def read_png16(path: pathlib.Path) -> np.ndarray:
    """Reads a PNG file of 16 bits for each channel (grey, RGB or RGBA, not interlaced). Gives RGB as uint16."""
    data = path.read_bytes()
    if data[:8] != PNG_SIGNATURE:
        raise ValueError(f"{path} is not a PNG file")
    offset, idat, header = 8, bytearray(), None
    while offset < len(data):
        length, kind = struct.unpack(">I4s", data[offset: offset + 8])
        body = data[offset + 8: offset + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        offset += 12 + length
    width, height, depth, colour, _, _, interlace = header
    channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(colour)
    if depth != 16 or channels is None or interlace != 0:
        raise ValueError(f"{path}: only 16-bit grey, RGB or RGBA PNG files that are not interlaced can be read")
    step = channels * 2
    rows = _unfilter(zlib.decompress(bytes(idat)), height, width * step, step)
    texels = rows.reshape(height, width * channels, 2)
    values = (texels[..., 0].astype(np.uint16) << 8 | texels[..., 1]).reshape(height, width, channels)
    if channels <= 2:
        return np.repeat(values[..., :1], 3, axis=2)
    return values[..., :3]


def write_png16(path: pathlib.Path, texels: np.ndarray) -> None:
    """Writes RGB texels of 16 bits for each channel as a PNG file."""
    height, width, _ = texels.shape
    rows = np.ascontiguousarray(texels.astype(">u2")).view(np.uint8).reshape(height, width * 6)
    raw = np.concatenate([np.zeros((height, 1), dtype=np.uint8), rows], axis=1).tobytes()

    def chunk(kind: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 16, 2, 0, 0, 0)
    path.write_bytes(PNG_SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) +
                     chunk(b"IEND", b""))


def read_picture(path: pathlib.Path) -> np.ndarray:
    """Reads a PNG file of 8 or 16 bits for each channel. Gives RGB as floats from 0 to 1."""
    if png_depth(path) == 16:
        return read_png16(path).astype(np.float64) / 65535.0
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def write_picture(path: pathlib.Path, picture: np.ndarray) -> None:
    """Writes RGB floats from 0 to 1 as a PNG file of 16 bits for each channel."""
    write_png16(path, np.clip(np.rint(picture * 65535.0), 0, 65535).astype(np.uint16))


# Deband ---------------------------------------------------------------------------------------------------------------


def to_levels(picture: np.ndarray) -> np.ndarray:
    """The 5-bit levels of an exported picture of the game, whose 8-bit values copy the top bits to the bottom."""
    return np.floor(np.rint(picture * 255.0) / 8.0)


def deband(levels: np.ndarray, radius: int = 6, flat: float = 2.0, cell: float = 0.5, passes: int = 4) -> np.ndarray:
    """Makes smooth gradients from the steps of a picture of 5-bit levels, and keeps its larger jumps.

    Each pass averages each texel with the texels near it whose levels differ from its own level by `flat` or less, in
    all channels. Thus a cloud edge, a jump of more levels, does not mix. After each pass, each texel goes back into
    `cell` levels of its own level. Thus no texel moves far from the game's picture, and each step becomes a slope.
    The result is in levels, as floats.
    """
    height, width, _ = levels.shape
    guide = np.pad(levels, ((radius, radius), (radius, radius), (0, 0)), mode="reflect")
    sigma = max(radius / 2.0, 0.5)
    offsets = [(dy, dx) for dy in range(-radius, radius + 1) for dx in range(-radius, radius + 1)
               if dy * dy + dx * dx <= radius * radius]
    weights = []
    for dy, dx in offsets:
        near = guide[radius + dy: radius + dy + height, radius + dx: radius + dx + width]
        same = np.max(np.abs(near - levels), axis=2) <= flat
        weights.append(same * np.exp(-(dy * dy + dx * dx) / (2.0 * sigma * sigma)))
    total = np.sum(weights, axis=0)[..., None]
    result = levels.copy()
    for _ in range(passes):
        padded = np.pad(result, ((radius, radius), (radius, radius), (0, 0)), mode="reflect")
        mixed = np.zeros_like(result)
        for (dy, dx), weight in zip(offsets, weights):
            mixed += weight[..., None] * padded[radius + dy: radius + dy + height, radius + dx: radius + dx + width]
        result = np.clip(mixed / total, levels - cell, levels + cell)
    return result


def detail_mask(levels: np.ndarray, flat: float = 2.0) -> np.ndarray:
    """1 where a texel or a neighbour has a jump of more than `flat` levels to a neighbour, else 0."""
    padded = np.pad(levels, ((1, 1), (1, 1), (0, 0)), mode="edge")
    height, width, _ = levels.shape
    jump = np.zeros((height, width), dtype=bool)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        near = padded[1 + dy: 1 + dy + height, 1 + dx: 1 + dx + width]
        jump |= np.max(np.abs(near - levels), axis=2) > flat
    padded = np.pad(jump, 1, mode="edge")
    grown = np.zeros_like(jump)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            grown |= padded[1 + dy: 1 + dy + height, 1 + dx: 1 + dx + width]
    return grown.astype(np.float64)


# Enlarge --------------------------------------------------------------------------------------------------------------


def resize(picture: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    """Enlarges RGB floats to (width, height) with a bicubic filter, in float precision."""
    channels = [np.asarray(Image.fromarray(picture[..., c].astype(np.float32), "F").resize(size, Image.Resampling.BICUBIC))
                for c in range(picture.shape[2])]
    return np.clip(np.stack(channels, axis=2).astype(np.float64), 0.0, 1.0)


def keep_detail(smooth: np.ndarray, detail: np.ndarray, game_levels: np.ndarray, flat: float) -> np.ndarray:
    """Takes the clouds from `detail` and the flat sky from `smooth`. Both have the same size. The mask comes from the
    game's half and has a soft edge."""
    height, width, _ = smooth.shape
    mask = resize(np.repeat(detail_mask(game_levels, flat)[..., None], 3, axis=2), (width, height))
    return smooth + mask * (detail - smooth)


# Commands -------------------------------------------------------------------------------------------------------------


def need(path: pathlib.Path) -> bool:
    if not path.is_file():
        print(f"missing file: {path}", file=sys.stderr)
        return False
    return True


def command_standin(source: pathlib.Path, output: pathlib.Path, scale: int) -> int:
    output.mkdir(parents=True, exist_ok=True)
    half = SIDE // 2
    for name in PICTURES:
        path = source / name
        if not need(path):
            return 1
        picture = Image.open(path).convert("RGB")
        if picture.size != (SIDE, SIDE):
            print(f"{path} is {picture.size[0]} x {picture.size[1]}, not {SIDE} x {SIDE}", file=sys.stderr)
            return 1
        out = Image.new("RGB", (SIDE * scale, SIDE * scale))
        for top in (0, half):
            part = picture.crop((0, top, SIDE, top + half))
            out.paste(part.resize((SIDE * scale, half * scale), Image.Resampling.BICUBIC), (0, top * scale))
        out.save(output / name)
        print(f"{name}: {SIDE * scale} x {SIDE * scale}")
    return 0


def command_deband(source: pathlib.Path, output: pathlib.Path, args: argparse.Namespace) -> int:
    output.mkdir(parents=True, exist_ok=True)
    for stem in halves():
        path = source / f"{stem}.png"
        if not need(path):
            return 1
        levels = to_levels(read_picture(path))
        smooth = deband(levels, args.radius, args.flat, args.cell, args.passes)
        write_picture(output / f"{stem}.png", smooth / LEVELS)
        print(f"{stem}: {levels.shape[1]} x {levels.shape[0]}")
    return 0


def command_upscale(source: pathlib.Path, output: pathlib.Path, args: argparse.Namespace) -> int:
    output.mkdir(parents=True, exist_ok=True)
    for stem in halves():
        path = source / f"{stem}.png"
        if not need(path):
            return 1
        picture = read_picture(path)
        size = (picture.shape[1] * args.scale, picture.shape[0] * args.scale)
        result = resize(picture, size)
        if args.detail is not None:
            detail_path = args.detail / f"{stem}.png"
            game_path = args.game / f"{stem}.png"
            if not need(detail_path) or not need(game_path):
                return 1
            detail = read_picture(detail_path)
            if detail.shape[:2] != result.shape[:2]:
                detail = resize(detail, size)
            result = keep_detail(result, detail, to_levels(read_picture(game_path)), args.flat)
        write_picture(output / f"{stem}.png", result)
        print(f"{stem}: {size[0]} x {size[1]}")
    return 0


def command_join(source: pathlib.Path, output: pathlib.Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    for name in PICTURES:
        stem = name[: -len(".png")]
        top_path, bottom_path = source / f"{stem}_a.png", source / f"{stem}_b.png"
        if not need(top_path) or not need(bottom_path):
            return 1
        deep = png_depth(top_path) == 16 or png_depth(bottom_path) == 16
        top, bottom = read_picture(top_path), read_picture(bottom_path)
        side = top.shape[1]
        if top.shape != bottom.shape or top.shape[0] * 2 != side or side % SIDE != 0:
            print(f"{stem}: the halves are {top.shape[1]} x {top.shape[0]} and {bottom.shape[1]} x {bottom.shape[0]};"
                  f" each must be N x N/2, N a multiple of {SIDE}", file=sys.stderr)
            return 1
        joined = np.concatenate([top, bottom], axis=0)
        if deep:
            write_picture(output / name, joined)
        else:
            Image.fromarray(np.clip(np.rint(joined * 255.0), 0, 255).astype(np.uint8), "RGB").save(output / name)
        print(f"{name}: {side} x {side}, {16 if deep else 8} bits")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    standin = commands.add_parser("standin", help="a bicubic stand-in pack for tests")
    standin.add_argument("source", type=pathlib.Path, help="the folder of the exported 256 x 256 PNG pictures")
    standin.add_argument("output", type=pathlib.Path, help="the folder for the stand-in pack")
    standin.add_argument("--scale", type=int, default=4, help="the whole scale of the enlargement (default 4)")
    debanded = commands.add_parser("deband", help="smooth 16-bit halves from the game's halves")
    debanded.add_argument("source", type=pathlib.Path, help="the folder of the game's 18 halves")
    debanded.add_argument("output", type=pathlib.Path, help="the folder for the 16-bit halves")
    debanded.add_argument("--radius", type=int, default=6, help="the radius of the average, in texels (default 6)")
    debanded.add_argument("--flat", type=float, default=2.0, help="the largest step of a band, in levels (default 2)")
    debanded.add_argument("--cell", type=float, default=0.5, help="how far a texel can move, in levels (default 0.5)")
    debanded.add_argument("--passes", type=int, default=4, help="the passes of the average (default 4)")
    upscaled = commands.add_parser("upscale", help="a bicubic enlargement of halves, in 16-bit precision")
    upscaled.add_argument("source", type=pathlib.Path, help="the folder of the halves, for example the debanded halves")
    upscaled.add_argument("output", type=pathlib.Path, help="the folder for the enlarged 16-bit halves")
    upscaled.add_argument("--scale", type=int, default=4, help="the whole scale of the enlargement (default 4)")
    upscaled.add_argument("--detail", type=pathlib.Path, help="the folder of the halves that give the clouds")
    upscaled.add_argument("--game", type=pathlib.Path, help="the folder of the game's 18 halves (with --detail)")
    upscaled.add_argument("--flat", type=float, default=2.0, help="the largest step of a band, in levels (default 2)")
    joined = commands.add_parser("join", help="the pack from the enlarged halves")
    joined.add_argument("source", type=pathlib.Path, help="the folder of the enlarged halves, <name>_a.png and _b.png")
    joined.add_argument("output", type=pathlib.Path, help="the folder for the pack")
    args = parser.parse_args()
    if getattr(args, "scale", 1) < 1:
        print("the scale must be 1 or more", file=sys.stderr)
        return 2
    if args.command == "standin":
        return command_standin(args.source, args.output, args.scale)
    if args.command == "deband":
        return command_deband(args.source, args.output, args)
    if args.command == "upscale":
        if args.detail is not None and args.game is None:
            print("--detail needs --game", file=sys.stderr)
            return 2
        return command_upscale(args.source, args.output, args)
    return command_join(args.source, args.output)


if __name__ == "__main__":
    sys.exit(main())
