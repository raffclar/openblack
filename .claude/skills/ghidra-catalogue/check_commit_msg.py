#!/usr/bin/env python3
"""commit-msg check for the Ghidra catalogue: the basic ASD-STE100 rules a script can test."""
import re
import sys

# Words that STE does not approve, or that hide what changed. Use the approved word in brackets.
UNAPPROVED = {
    "tweak": "change", "tweaks": "change", "fixup": "correct", "refactor": "change", "cleanup": "remove",
    "misc": "name the change", "various": "name the change", "stuff": "name the thing", "wip": "complete it",
    "utilize": "use", "utilise": "use", "modify": "change", "implement": "add", "implemented": "added",
    "leverage": "use", "ensure": "make sure", "via": "through", "etc": "name the things",
}

lines = [l.rstrip("\n") for l in open(sys.argv[1], encoding="utf-8") if not l.startswith("#")]
while lines and not lines[-1].strip():
    lines.pop()
errors = []
subject = lines[0] if lines else ""
if not subject.strip():
    errors.append("the subject line is empty")
if len(subject) > 72:
    errors.append(f"the subject has {len(subject)} characters (72 is the maximum)")
if subject.endswith("."):
    errors.append("the subject ends with a full stop")
if subject and not subject[0].isupper():
    errors.append("the subject does not start with a capital letter")
first = subject.split(" ", 1)[0] if subject else ""
if first.lower().endswith("ing") or first.lower().endswith("ed"):
    errors.append(f"start the subject with an instruction verb (for example Add, Change, Remove), not '{first}'")
if len(lines) > 1 and lines[1].strip():
    errors.append("put an empty line after the subject")
for word in re.findall(r"[A-Za-z]+", " ".join(lines)):
    if word.lower() in UNAPPROVED:
        errors.append(f"'{word}' is not STE; use: {UNAPPROVED[word.lower()]}")
for i, line in enumerate(lines[2:], 3):
    if not line.startswith("Co-Authored-By:") and len(re.findall(r"\w+", line)) > 25:
        errors.append(f"line {i} has more than 25 words (STE sentence limit)")
if errors:
    sys.stderr.write("Commit message is not in STE:\n  - " + "\n  - ".join(errors) + "\n")
    sys.exit(1)
