"""Build the "Looks waiting on you" review page for openblack visual changes.

Each visual lane writes one look file, <looks>/<branch>.json. The looks folder is paths.looks in
.claude/openblack.local.json (or OB_LOOKS):

{
  "branch": "creature-dispenser-vials",
  "title": "Creature dispenser vials",
  "meta": "d6d64aa3c · vanilla-bugs row 26 (Kept) · no deviations",
  "summary": "What vanilla does and what changed, in plain English.",
  "bullets": ["optional", "extra points"],
  "check": "What the user should look for.",
  "deviations": ["optional: row 47 PENDING - text the user must sign off"],
  "media": [
    {"path": "<screenshots>/.../x.gif", "caption": "Before | after | vanilla"}
  ],
  "grid": false,
  "status": "waiting"
}

Media may be .gif/.png/.jpg (img) or .mp4/.webm (looping video). status is "waiting" until the user answers, then
"approved" or "rejected" (with "answer": "the user's words").

Usage:
  python looks.py                      # every waiting look -> page, opened in the browser
  python looks.py BRANCH [BRANCH...]   # just these lanes
  python looks.py --all                # include answered ones
  python looks.py --out PATH --no-open
"""

import argparse
import subprocess
import html
import json
import os
import sys
import webbrowser
from pathlib import Path

def _openblack_config(dotted: str, default: str = "") -> str:
    """One value of the developer's openblack skill settings (.claude/openblack.local.json), or the default."""
    reader = Path(__file__).resolve().parents[2] / "obconfig.py"
    try:
        out = subprocess.run([sys.executable, str(reader), "get", dotted], capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return default
    return out.stdout.strip() or default


LOOKS = Path(os.environ.get("OB_LOOKS") or _openblack_config("paths.looks", "looks"))
DEFAULT_OUT = LOOKS / "looks.html"
VIDEO = {".mp4", ".webm"}

STYLE = """
:root { --bg:#f6f5f2; --card:#fff; --fg:#1d1d1b; --muted:#666; --line:#ddd; --accent:#8a5a00; --warn:#b3261e; }
@media (prefers-color-scheme: dark) { :root { --bg:#151514; --card:#1f1f1d; --fg:#ecebe6; --muted:#a3a29c; --line:#333; --accent:#e0b04a; --warn:#f2867c; } }
body { background:var(--bg); color:var(--fg); font:15px/1.5 system-ui, sans-serif; margin:0; padding:24px 16px 64px; }
main { max-width:1200px; margin:0 auto; }
h1 { font-size:22px; margin:0 0 4px; }
.lede { color:var(--muted); margin:0 0 24px; }
nav a { color:var(--accent); margin-right:14px; }
section { background:var(--card); border:1px solid var(--line); border-radius:8px; padding:18px 20px; margin:20px 0; }
h2 { font-size:18px; margin:0 0 2px; }
.meta { color:var(--muted); font-size:13px; margin-bottom:10px; }
ul { margin:6px 0 10px; padding-left:20px; }
.check { border-left:3px solid var(--accent); padding:4px 10px; margin:10px 0; }
.dev { border-left:3px solid var(--warn); padding:4px 10px; margin:10px 0; }
.answer { color:var(--muted); font-style:italic; }
figure { margin:12px 0; }
figure img, figure video { max-width:100%; border:1px solid var(--line); border-radius:4px; display:block; }
figcaption { color:var(--muted); font-size:13px; margin-top:4px; word-break:break-all; }
.grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(420px, 1fr)); gap:12px; }
@media (max-width:480px) { .grid { grid-template-columns:1fr; } }
"""


def esc(text):
    return html.escape(str(text))


def file_url(path):
    return Path(path).resolve().as_uri()


def size_note(path):
    try:
        mb = os.path.getsize(path) / 1e6
    except OSError:
        return " (MISSING)"
    return f" ({mb:.0f} MB)" if mb >= 15 else ""


def media_html(item):
    path = item["path"]
    caption = esc(item.get("caption", Path(path).name)) + size_note(path)
    url = esc(file_url(path))
    if Path(path).suffix.lower() in VIDEO:
        body = f'<video src="{url}" autoplay loop muted playsinline controls></video>'
    else:
        body = f'<img loading="lazy" src="{url}">'
    return f"<figure>{body}<figcaption>{caption}</figcaption></figure>"


def section_html(index, look):
    anchor = esc(look["branch"])
    parts = [f'<section id="{anchor}">', f"<h2>{index}. {esc(look['title'])}</h2>"]
    parts.append(f'<div class="meta">Branch {esc(look["branch"])}'
                 + (f" · {esc(look['meta'])}" if look.get("meta") else "") + "</div>")
    if look.get("summary"):
        parts.append(f"<p>{esc(look['summary'])}</p>")
    if look.get("bullets"):
        parts.append("<ul>" + "".join(f"<li>{esc(b)}</li>" for b in look["bullets"]) + "</ul>")
    for dev in look.get("deviations", []):
        parts.append(f'<div class="dev">Deviation for your sign-off: {esc(dev)}</div>')
    if look.get("check"):
        parts.append(f'<div class="check">Check: {esc(look["check"])}</div>')
    if look.get("status", "waiting") != "waiting":
        parts.append(f'<p class="answer">{esc(look["status"])}: {esc(look.get("answer", ""))}</p>')
    media = "".join(media_html(m) for m in look.get("media", []))
    parts.append(f'<div class="grid">{media}</div>' if look.get("grid") else media)
    parts.append("</section>")
    return "\n".join(parts)


def build(looks):
    nav = "".join(f'<a href="#{esc(l["branch"])}">{i} {esc(l["title"])}</a>' for i, l in enumerate(looks, 1))
    sections = "\n".join(section_html(i, l) for i, l in enumerate(looks, 1))
    count = len(looks)
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Looks Waiting</title>
<style>{STYLE}</style>
</head>
<body>
<main>
<h1>Looks waiting on you</h1>
<p class="lede">{count} lane{'s' if count != 1 else ''} held until you've seen them move. Reply with approve / reject (and why) per lane. Large GIFs may take a moment to load.</p>
<nav>{nav}</nav>
{sections}
</main>
</body>
</html>
"""


def load_look(path):
    """Reads one look file. A bad file prints a message naming it and gives None, so the page still builds."""
    try:
        look = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        print(f"BAD look file, skipped: {path}: {type(error).__name__}: {error}", file=sys.stderr)
        return None
    if not isinstance(look, dict):
        print(f"BAD look file, skipped: {path}: the top level is not an object", file=sys.stderr)
        return None
    return look


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("branches", nargs="*")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    parser.add_argument("--no-open", action="store_true")
    parser.add_argument("--looks-dir", default=str(LOOKS), help="folder of look files (default: the shared folder)")
    args = parser.parse_args()

    looks_dir = Path(args.looks_dir)
    files = [looks_dir / f"{b}.json" for b in args.branches] if args.branches else sorted(looks_dir.glob("*.json"))
    looks = []
    bad = []
    for f in files:
        look = load_look(f)
        if look is None:
            bad.append(f)
            continue
        if args.branches or args.all or look.get("status", "waiting") == "waiting":
            looks.append(look)
    if not looks:
        print("no looks waiting")
        return 1
    missing = [m["path"] for l in looks for m in l.get("media", []) if not os.path.exists(m["path"])]
    for path in missing:
        print(f"MISSING media: {path}", file=sys.stderr)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(build(looks), encoding="utf-8")
    print(f"{out} ({len(looks)} looks)")
    if not args.no_open:
        webbrowser.open(out.resolve().as_uri())
    return 2 if missing or bad else 0


if __name__ == "__main__":
    sys.exit(main())
