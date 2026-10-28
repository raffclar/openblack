"""Vulkan pixel check for the merge train (fixed 2026-10-26).

The game data folder comes from OB_GAME_DATA (paths.gameData in .claude/openblack.local.json).

Usage: python uz_shots.py <ref openblack.exe> <branch openblack.exe> <out dir> [scene ...]

Compares the RGB bytes of each screenshot pair exactly. The old script used
ImageChops.difference(RGBA).getbbox(); in Pillow 11 that looks only at the alpha band, so it
reported IDENTICAL for any colour change. Do not use getbbox() on an RGBA image.
Prints IDENTICAL, or DIFFERENT with the bounding box, the count of differing pixels and the
maximum channel difference, and saves <scene>-diff.png (amplified) in the out dir.
"""
import os, subprocess, sys
from PIL import Image, ImageChops

SCENES = {
    "land-menu": (["--skip-logos"], 300),
    "land-story": (["--skip-logos"], 500),
    "testbed-shield": (["--scenario", "miracles.creature_shield_idles", "--scenario-hide-window"], 300),
    "storm-shadow": (["--scenario", "liquid.storm_shadow", "--scenario-hide-window"], 300),
    # Land 1 in play. The two scenes above show the front end only.
    "land1-300": (["-s", "Land1.txt", "--skip-opening", "cow"], 300),
    "land1-500": (["-s", "Land1.txt", "--skip-opening", "cow"], 500),
}
DEFAULT = ["land-menu", "land-story", "land1-300", "land1-500", "testbed-shield"]
COMMON = ["--background", "--seed", "1", "--sim-frame-ms", "20", "-W", "1280", "-H", "1024",
          "-g", os.environ["OB_GAME_DATA"], "-b", "Vulkan"]


def compare(a, b):
    """Return (bbox, differing pixel count, max channel difference) for two images, or 'size'."""
    if a.size != b.size:
        return "size", 0, 0
    d = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
    bbox = d.getbbox()
    if bbox is None:
        return None, 0, 0
    r, g, bl = d.split()
    mask = ImageChops.lighter(ImageChops.lighter(r, g), bl)
    count = sum(1 for v in mask.getdata() if v)
    return bbox, count, mask.getextrema()[1]


def self_test():
    a = Image.new("RGBA", (8, 8), (10, 20, 30, 255))
    assert compare(a, a.copy())[0] is None
    b = a.copy(); b.putpixel((3, 4), (11, 20, 30, 255))
    bbox, count, mx = compare(a, b)
    assert bbox == (3, 4, 4, 5) and count == 1 and mx == 1, (bbox, count, mx)
    c = a.copy(); c.putpixel((0, 0), (10, 20, 30, 0))  # alpha-only change is not a colour change
    assert compare(a, c)[0] is None
    assert compare(a, Image.new("RGBA", (9, 8)))[0] == "size"


def run(ref, branch, out, names):
    os.makedirs(out, exist_ok=True)
    result = {}
    for name in names:
        extra, frame = SCENES[name]
        shots = []
        for tag, exe in (("ref", ref), ("branch", branch)):
            work = os.path.join(out, f"work-{tag}"); os.makedirs(work, exist_ok=True)
            shot = os.path.abspath(os.path.join(out, f"{name}-{tag}.png"))
            if os.path.exists(shot): os.remove(shot)
            args = [exe, *COMMON, *extra, "-n", str(frame + 5), "--screenshot-frame", str(frame), "--screenshot-path", shot]
            p = subprocess.Popen(args, cwd=work, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
            try: p.wait(timeout=600)
            except subprocess.TimeoutExpired:
                subprocess.run(["taskkill", "/PID", str(p.pid), "/F"], check=False)
            shots.append(shot)
        if not all(os.path.exists(s) for s in shots):
            print(name, "missing", flush=True); result[name] = "missing"; continue
        a, b = (Image.open(s) for s in shots)
        bbox, count, mx = compare(a, b)
        if bbox is None:
            print(name, "IDENTICAL", flush=True)
        elif bbox == "size":
            print(name, "DIFFERENT size", a.size, b.size, flush=True)
        else:
            d = ImageChops.difference(a.convert("RGB"), b.convert("RGB")).point(lambda v: min(255, v * 16))
            d.save(os.path.join(out, f"{name}-diff.png"))
            print(name, "DIFFERENT bbox", bbox, "pixels", count, "max", mx, flush=True)
        result[name] = (bbox, count, mx)
    return result


if __name__ == "__main__":
    self_test()
    if len(sys.argv) >= 2 and sys.argv[1] == "--self-test":
        print("self-test ok"); sys.exit(0)
    ref, branch, out = sys.argv[1:4]
    run(ref, branch, out, sys.argv[4:] or DEFAULT)
