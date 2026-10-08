"""Checks relative links and #anchors between the wiki pages, with GitHub's heading slugs.

    python docs/check_links.py docs/bw1-notes
"""
import glob
import os
import re
import sys


def slug(heading):
    """GitHub's anchor for a heading: lower case, markup and punctuation dropped, spaces to hyphens."""
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", heading.strip().lower())
    text = re.sub(r"[^\w\- ]", "", text.replace("`", ""), flags=re.U)
    return text.replace(" ", "-")


def anchors_of(path):
    found, seen, fenced = set(), {}, False
    for line in open(path, encoding="utf-8"):
        if line.startswith("```"):
            fenced = not fenced
        match = None if fenced else re.match(r"#{1,6} (.*)", line)
        if match:
            base = slug(match.group(1))
            count = seen.get(base, 0)
            seen[base] = count + 1
            found.add(base if count == 0 else "%s-%d" % (base, count))
    for name in re.findall(r'<a (?:name|id)="([^"]+)"', open(path, encoding="utf-8").read()):
        found.add(name)
    return found


def main(folder):
    os.chdir(folder)
    pages = {name: anchors_of(name) for name in glob.glob("*.md")}
    bad = 0
    for name in sorted(pages):
        text = open(name, encoding="utf-8").read()
        for link in re.findall(r"\]\(([^)\s]+)\)", text):
            if link.startswith(("http", "mailto", "../refactor/", "img/")):
                continue
            target, _, anchor = link.partition("#")
            target = target or name
            if not os.path.exists(target):
                print("missing page", name, link)
                bad += 1
            elif anchor and target in pages and anchor not in pages[target]:
                print("missing anchor", name, link)
                bad += 1
    print("%d broken links" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
