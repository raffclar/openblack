#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Checks that every docs/*.md file and every tools/**/*.json file decodes as UTF-8 and holds no mojibake.

Mojibake is text that was decoded with the wrong code page. It shows as the marker strings "Ã", "â€" and "Â" or as
the replacement character U+FFFD. A real word that holds a marker (for example a name with "Ã") goes in ALLOWED.
Exit code 0 when all files are clean, 1 with a list of the faults.

Usage: check_text_encoding.py [repository folder]
       check_text_encoding.py --test
"""
import pathlib
import sys
import tempfile

MARKERS = ("Ã", "â€", "Â", "�")
# Real words that hold a marker. Each entry is the exact text that is allowed to appear.
ALLOWED = ()
PATTERNS = ("docs/*.md", "tools/**/*.json")


def check_file(path):
    """Returns a list of fault texts for one file."""
    data = path.read_bytes()
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as error:
        return [f"{path}: byte 0x{data[error.start]:02x} at offset {error.start} is not valid UTF-8"]
    faults = []
    for number, line in enumerate(text.splitlines(), 1):
        clean = line
        for word in ALLOWED:
            clean = clean.replace(word, "")
        for marker in MARKERS:
            if marker in clean:
                faults.append(f"{path}:{number}: mojibake marker U+{ord(marker[0]):04X} in: {line.strip()[:80]}")
                break
    return faults


def check_tree(root):
    faults = []
    for pattern in PATTERNS:
        for path in sorted(root.glob(pattern)):
            if path.is_file():
                faults += check_file(path)
    return faults


def self_test():
    """Proves that the check finds a bad byte and a marker and accepts clean text."""
    with tempfile.TemporaryDirectory() as folder:
        root = pathlib.Path(folder)
        (root / "docs").mkdir()
        (root / "tools" / "a").mkdir(parents=True)
        (root / "docs" / "ok.md").write_bytes("crèche — fine\n".encode("utf-8"))
        (root / "tools" / "a" / "ok.json").write_bytes(b'{"a": 1}\n')
        assert check_tree(root) == [], "clean tree must pass"
        (root / "docs" / "bad_byte.md").write_bytes(b"dash \x97 here\n")
        (root / "docs" / "bad_marker.md").write_bytes("dash â€” here\n".encode("utf-8"))
        (root / "tools" / "a" / "bad.json").write_bytes("{\"a\": \"�\"}\n".encode("utf-8"))
        faults = check_tree(root)
        assert len(faults) == 3, faults
        assert any("0x97" in fault for fault in faults), faults
    print("self-test passed")
    return 0


def main(argv):
    if argv[1:] == ["--test"]:
        return self_test()
    root = pathlib.Path(argv[1]) if len(argv) > 1 else pathlib.Path(__file__).resolve().parents[2]
    faults = check_tree(root)
    for fault in faults:
        print(fault)
    if faults:
        print(f"{len(faults)} text encoding fault(s)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
