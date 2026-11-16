#!/usr/bin/env python3
"""Write the settings of a run where the game reads them.

A run that gets --profile-folder <folder> reads settings.json inside that folder. Without the switch, the game reads
settings.json next to its profile folder. This tool gives a run its settings in the first way, so that a clip or probe script
does not write the file in a wrong place. A custom_detail object only counts when detail_level is 5 (Custom), so the tool
adds detail_level 5 when the settings have custom_detail and no detail_level.

Use from a script:   write_settings.write_settings(profile_folder, {"custom_detail": {"clouds": False}})
Use from the shell:  write_settings.py <profile_folder> '{"custom_detail": {"clouds": false}}'
"""

import json
import os
import sys

FILE_NAME = "settings.json"
CUSTOM_LEVEL = 5


def write_settings(profile_folder, settings=None):
    """Write settings.json in the profile folder, make the folder if it is missing, and return the path of the file"""
    settings = dict(settings or {})
    if "custom_detail" in settings and "detail_level" not in settings:
        settings["detail_level"] = CUSTOM_LEVEL
    os.makedirs(profile_folder, exist_ok=True)
    path = os.path.join(profile_folder, FILE_NAME)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(settings, handle)
    return path


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__, file=sys.stderr)
        return 2
    settings = json.loads(argv[2]) if len(argv) == 3 else {}
    print(write_settings(argv[1], settings))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
