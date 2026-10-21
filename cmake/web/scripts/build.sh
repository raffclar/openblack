#!/bin/bash
# Builds a target of the web experiment. Usage: build.sh [target] [jobs]
source "$(dirname "$0")/web-env.sh"
avail=$(df --output=avail -BG /e | tail -1 | tr -dc 0-9)
echo "free on E: ${avail} GB"
if [ "$avail" -lt 20 ]; then echo "less than 20 GB free, stop"; exit 2; fi
"$CMAKE" --build "$B" --target "${1:-openblack}" -- -k 0 -j "${2:-12}"
echo "build exit $?"
