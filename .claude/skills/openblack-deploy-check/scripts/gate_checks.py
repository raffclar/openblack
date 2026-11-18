# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The pure checks of the deploy gate: the console of the page, the pictures of the land, and the lines of the game
server. They read only the text and the files that the caller gives, so the unit tests give them fakes.
"""
import re

# The lines of the console of the page that make a check fail. Each is (a name, a pattern).
CONSOLE_FAULTS = (
    ("uncaught exception", re.compile(r"\[exception")),
    ("console error", re.compile(r"\[error\]")),
    ("WebGPU validation", re.compile(
        r"is invalid due to a previous error|\[Invalid (BindGroup|CommandBuffer|RenderPipeline|Texture|Buffer)|"
        r"GPUValidationError|None of the supported sample types|too many warnings, no more warnings|"
        r"device (was )?lost|GPUDevice.*lost", re.IGNORECASE)),
    ("WebGL error", re.compile(r"WebGL: (INVALID_\w+|CONTEXT_LOST)|GL_INVALID_\w+|WebGL context was lost")),
    ("worker load error", re.compile(r"(Failed to (construct|load|fetch).*[Ww]orker|[Ww]orker.*(failed|error))")),
)
# Console lines that the good builds also give, and that are not faults:
# - bgfx asks WebGL 2 for the formats of textures at its start; Chrome answers some asks with INVALID_ENUM.
KNOWN_HARMLESS = (
    re.compile(r"WebGL: INVALID_ENUM: getInternalformatParameter: invalid internalformat"),
)
LOG_ERROR = re.compile(r"\[log error\] (?P<text>.*?)(?: <(?P<url>[^>]*)>)?$")
# A file that the page asks for and that a build with no developer tools does not have (web/page/launcher.js)
OPTIONAL_FILES = ("developer-tools.txt",)


def console_faults(text):
    """The lines of the console text that show a fault, as a list of (the name of the fault, the line)"""
    faults = []
    for line in text.splitlines():
        if not line.strip() or line.startswith((" ", "\t")):
            continue
        if any(pattern.search(line) for pattern in KNOWN_HARMLESS):
            continue
        error = LOG_ERROR.search(line)
        if error:
            url = error.group("url") or ""
            if "Failed to load resource" in error.group("text") and url.endswith(OPTIONAL_FILES):
                continue
            faults.append(("console error", line))
            continue
        for name, pattern in CONSOLE_FAULTS:
            if pattern.search(line):
                faults.append((name, line))
                break
    return faults


def failed_requests(serve_log):
    """The paths that the local page server answered with a status of 400 or more, other than the optional files"""
    failed = []
    for match in re.finditer(r'"(?:GET|HEAD) (\S+) HTTP/[\d.]+" (\d{3})', serve_log):
        path, status = match.group(1), int(match.group(2))
        if status >= 400 and not path.split("?", 1)[0].endswith(OPTIONAL_FILES):
            failed.append(f"{path} {status}")
    return failed


# The pictures: the gate looks at the game below the strip of the page that names the build (--badge-bar in
# web/page/page.css), at a small size.
BADGE_BAR = 22
SAMPLE_SIZE = (320, 250)
# A pixel is lit when one of its channels is more than this value
LIT_LEVEL = 32
# The least share of lit pixels, and of the colours of land (grass, earth, sand, rock), in a picture of the land
MIN_LIT = 0.5
MIN_LAND = 0.05
# The least likeness of the colours of a picture to the colours of the same picture of the reference build
MIN_LIKENESS = 0.35
HISTOGRAM_BINS = 8


def is_land_colour(red, green, blue):
    """True for a colour of grass, earth, sand or rock: green over blue, or red over green over blue"""
    grass = green > 50 and green + 8 >= red * 0.85 and green > blue + 10
    earth = red > 70 and red >= green >= blue and red - blue > 25
    return grass or earth


def picture_measures(picture):
    """The share of lit pixels, the share of land colours and the colour histogram of a picture (PIL Image)"""
    rgb = picture.convert("RGB")
    top = min(BADGE_BAR * rgb.width // 1280, rgb.height - 1)
    pixels = list(rgb.crop((0, top, rgb.width, rgb.height)).resize(SAMPLE_SIZE).getdata())
    lit = sum(1 for red, green, blue in pixels if max(red, green, blue) > LIT_LEVEL)
    land = sum(1 for red, green, blue in pixels if is_land_colour(red, green, blue))
    step = 256 // HISTOGRAM_BINS
    histogram = [0] * HISTOGRAM_BINS ** 3
    for red, green, blue in pixels:
        histogram[(red // step) * HISTOGRAM_BINS ** 2 + (green // step) * HISTOGRAM_BINS + blue // step] += 1
    count = len(pixels)
    return {"lit": lit / count, "land": land / count, "histogram": [value / count for value in histogram]}


def likeness(histogram, reference):
    """The likeness of two colour histograms, from 0 (no colour the same) to 1 (the same colours)"""
    return sum(min(a, b) for a, b in zip(histogram, reference))


def land_check(measures, reference=None):
    """(pass, detail) for the measures of one picture of the land, and the measures of the reference picture if any"""
    parts = [f"lit {measures['lit']:.0%}", f"land colours {measures['land']:.0%}"]
    ok = measures["lit"] >= MIN_LIT and measures["land"] >= MIN_LAND
    if reference is not None:
        alike = likeness(measures["histogram"], reference["histogram"])
        parts.append(f"likeness to the reference {alike:.2f}")
        ok = ok and alike >= MIN_LIKENESS
    return ok, ", ".join(parts)


# The lines of the game server and of the game
# An older server puts the player name after the slot ("slot 0 Player1: commands"). A newer server does not
# ("slot 0: commands").
SERVER_SUMMARY = re.compile(
    r"Network game: slot (\d+)[ :].*?(\d+) checksums compared, (\d+) the same, first different (\w+), (\d+) resumes")
CLIENT_COMPARED = re.compile(r"Network game: (\d+) turns compared with the server, (\d+) the same")
BUILD_ID = re.compile(r"\(build id ([^)]+)\)")


def lockstep_check(server_log, client_log, turns):
    """(pass, detail): the server compared at least the turns with the client, all the same, with no resume"""
    summary = SERVER_SUMMARY.findall(server_log)
    if not summary:
        return False, "the server wrote no summary of the network game"
    _, compared, same, first, resumes = summary[-1]
    client = CLIENT_COMPARED.findall(client_log)
    client_same = client and int(client[-1][0]) >= turns and client[-1][0] == client[-1][1]
    ok = int(compared) >= turns and compared == same and first == "none" and resumes == "0" and bool(client_same)
    detail = (f"server: {compared} checksums compared, {same} the same, first different {first}, {resumes} resumes; "
              f"client: {client[-1][0] + ' compared, ' + client[-1][1] + ' the same' if client else 'no compare line'}")
    return ok, detail


def build_id(version_output):
    """The build id in the output of --version, such as 0.1.0+9f73a40c4184/codec36, or None"""
    match = BUILD_ID.search(version_output)
    return match.group(1) if match else None

