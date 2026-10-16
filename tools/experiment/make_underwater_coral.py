# EXPERIMENT (not for merge): makes the coral atlas for the underwater experiment.
#
# The method follows "Procedurally Generated Coral" by John Freeman, Jose Rivas, Julia Goldman and Matheus de Carvalho
# Souza (Williams College, 2016): an L-system grows each coral's centre lines, and the lines are drawn into an image
# with a thickness that falls with the depth of the recursion, shaded across their width as their bump map is. The
# images go on crossed quads in the game, so each coral has few vertices.
#
# Writes coral.raw (the colours, 3 bytes a texel), corala.raw (the alpha, 1 byte a texel) and coral.png (a preview) to
# the folder given. The atlas is 3 by 3 cells of 256 texels: the paper's six kinds (branch, thin, finger, flat, tall,
# crazy), then three more of branch, finger and crazy grown with other random numbers.
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

CELL = 256
SUPER = 2

# The kinds: the rule for F, the axiom, the recursion depth, the branch angle range in degrees, the first thickness in
# texels, how much the thickness falls at each depth, and how much a segment shortens at each depth. The paper's
# ranges are 12 to 20, 0 to 12 and 0 to 20 degrees and thicknesses of 6 and 12; the corals here are wider and thicker,
# so that they read at the game's distances.
KINDS = {
    "branch": ("F[+F]F[-F]F", "F", 4, (20, 35), 18, 0.72, 0.62),
    "thin": ("F[+F][-F]", "F", 6, (10, 25), 9, 0.68, 0.74),
    "finger": ("F[+F][-F]", "F", 4, (5, 15), 22, 0.85, 0.75),
    "flat": ("F[++F][--F]F", "F", 4, (25, 40), 14, 0.72, 0.55),
    "tall": ("F[+F]F", "F", 5, (5, 15), 12, 0.85, 0.72),
    "crazy": ("F[+F][-F][++F]F", "F", 4, (15, 35), 16, 0.7, 0.6),
}
ORDER = ["branch", "thin", "finger", "flat", "tall", "crazy", "branch", "finger", "crazy"]


def expand(axiom, rule, depth):
    """The L-system's string after `depth` rewrites, each symbol tagged with the depth it was made at."""
    symbols = [(c, 0) for c in axiom]
    for level in range(1, depth + 1):
        out = []
        for c, d in symbols:
            if c == "F":
                out.extend((r, level) for r in rule)
            else:
                out.append((c, d))
        symbols = out
    return symbols


def edges(kind, rng):
    """The centre lines of a coral: (start, end, depth), grown upward from the origin."""
    rule, axiom, depth, angles, _, _, shorten = KINDS[kind]
    stack = []
    x, y, heading, level = 0.0, 0.0, 90.0, 0
    out = []
    for c, d in expand(axiom, rule, depth):
        if c == "F":
            length = 10.0 * (shorten ** d) * rng.uniform(0.8, 1.2)
            heading += rng.uniform(-4.0, 4.0)
            nx = x + length * math.cos(math.radians(heading))
            ny = y + length * math.sin(math.radians(heading))
            out.append(((x, y), (nx, ny), d))
            x, y = nx, ny
        elif c == "+":
            heading += rng.uniform(*angles)
        elif c == "-":
            heading -= rng.uniform(*angles)
        elif c == "[":
            stack.append((x, y, heading, level))
        elif c == "]":
            x, y, heading, level = stack.pop()
    return out


def draw(kind, rng):
    """One cell: the coral's lines drawn thick, darker at their edges and lighter at their middle, with its alpha."""
    lines = edges(kind, rng)
    _, _, _, _, thick, thin, _ = KINDS[kind]
    xs = [p[0] for e in lines for p in e[:2]]
    ys = [p[1] for e in lines for p in e[:2]]
    span = max(max(xs) - min(xs), max(ys) - min(ys), 1e-3)
    size = CELL * SUPER
    margin = 0.06 * size
    scale = (size - 2 * margin) / span
    cx = (max(xs) + min(xs)) * 0.5

    def at(p):
        return (size * 0.5 + (p[0] - cx) * scale, size - margin * 0.5 - (p[1] - min(ys)) * scale)

    colour = Image.new("L", (size, size), 0)
    alpha = Image.new("L", (size, size), 0)
    paint = ImageDraw.Draw(colour)
    cover = ImageDraw.Draw(alpha)
    # Each line three times, narrower and lighter each time, as the paper's bump map shades it
    for share, shade in ((1.0, 110), (0.6, 170), (0.25, 230)):
        for start, end, d in lines:
            width = max(thick * (thin ** d) * SUPER * share, 1.0)
            a, b = at(start), at(end)
            paint.line([a, b], fill=shade, width=int(round(width)))
            r = width * 0.5
            paint.ellipse([b[0] - r, b[1] - r, b[0] + r, b[1] + r], fill=shade)
            if share == 1.0:
                cover.line([a, b], fill=255, width=int(round(width)))
                cover.ellipse([b[0] - r, b[1] - r, b[0] + r, b[1] + r], fill=255)
    colour = colour.filter(ImageFilter.GaussianBlur(SUPER * 0.6)).resize((CELL, CELL), Image.LANCZOS)
    alpha = alpha.resize((CELL, CELL), Image.LANCZOS)
    return colour, alpha


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    side = CELL * 3
    colours = Image.new("RGB", (side, side), (0, 0, 0))
    alphas = Image.new("L", (side, side), 0)
    for i, kind in enumerate(ORDER):
        rng = random.Random(1998 + i * 77)
        grey, alpha = draw(kind, rng)
        # The colour is pale, a little warm; each coral is tinted in the game
        tinted = Image.merge("RGB", [grey.point(lambda v: min(255, int(v * 1.0))), grey.point(lambda v: int(v * 0.96)),
                                     grey.point(lambda v: int(v * 0.9))])
        x, y = (i % 3) * CELL, (i // 3) * CELL
        colours.paste(tinted, (x, y))
        alphas.paste(alpha, (x, y))
    (out / "coral.raw").write_bytes(colours.tobytes())
    (out / "corala.raw").write_bytes(alphas.tobytes())
    preview = colours.copy()
    preview.putalpha(alphas)
    preview.save(out / "coral.png")
    print("wrote", out / "coral.raw", out / "corala.raw", out / "coral.png")


if __name__ == "__main__":
    main()
