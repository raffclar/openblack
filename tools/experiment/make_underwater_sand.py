# EXPERIMENT (not for merge): makes the tileable sea-bed sand texture for the underwater experiment.
# Writes sand.raw (512x512, 3 bytes a texel, as the game's raw textures) and sand.png (a preview) to the folder given.
import sys
from pathlib import Path

import numpy as np
from PIL import Image

SIDE = 512


def tile_noise(rng, cells):
    """Smooth value noise that tiles across the square, with `cells` cells along a side."""
    grid = rng.random((cells, cells))
    coords = np.arange(SIDE) * cells / SIDE
    i0 = np.floor(coords).astype(int)
    f = coords - i0
    f = f * f * (3 - 2 * f)
    i1 = (i0 + 1) % cells
    a = grid[np.ix_(i0, i0)]
    b = grid[np.ix_(i0, i1)]
    c = grid[np.ix_(i1, i0)]
    d = grid[np.ix_(i1, i1)]
    fx = f[None, :]
    fy = f[:, None]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(1998)
    # Patches of lighter and darker sand, then fine grains
    patches = 0.55 * tile_noise(rng, 4) + 0.3 * tile_noise(rng, 8) + 0.15 * tile_noise(rng, 16)
    grains = 0.5 * tile_noise(rng, 128) + 0.5 * tile_noise(rng, 256)
    speck = rng.random((SIDE, SIDE))
    base = np.array([0.80, 0.72, 0.55])
    light = np.array([0.90, 0.84, 0.68])
    dark = np.array([0.62, 0.54, 0.40])
    t = patches[..., None]
    colour = base * (1 - t) + light * t
    colour = colour * (0.86 + 0.24 * grains[..., None])
    # Dark grains of other stone and light grains of shell
    colour = np.where((speck > 0.985)[..., None], dark * (0.8 + 0.2 * grains[..., None]), colour)
    colour = np.where((speck < 0.008)[..., None], np.array([0.96, 0.93, 0.86]), colour)
    rgb = np.clip(colour * 255.0 + 0.5, 0, 255).astype(np.uint8)
    (out / "sand.raw").write_bytes(rgb.tobytes())
    Image.fromarray(rgb, "RGB").save(out / "sand.png")
    print("wrote", out / "sand.raw", out / "sand.png")


if __name__ == "__main__":
    main()
