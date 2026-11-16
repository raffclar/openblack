#!/usr/bin/env bash
# Runs the dead code index of a configured build folder: index, classify, verify, report.
#
#   tools/dead_code/dc_run.sh <worktree> <build folder> <out folder> [jobs]
#
# The build folder must hold compile_commands.json (a RelWithDebInfo configure writes it). The index only uses CPU and
# reads the source, so it needs no build lock. Set DC_LIBCLANG and DC_PYLIB as dc_index.py says.
set -euo pipefail
[ $# -ge 3 ] || { echo "usage: $0 <worktree> <build folder> <out folder> [jobs]" >&2; exit 2; }
WT=$1
BUILD=$2
OUT=$3
JOBS=${4:-2}
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
mkdir -p "$OUT"
[ -f "$BUILD/compile_commands.json" ] || { echo "no compile database in $BUILD" >&2; exit 1; }
python "$HERE/dc_index.py" "$BUILD/compile_commands.json" "$WT" "$OUT/index.json" "$JOBS"
python "$HERE/dc_classify.py" "$OUT/index.json" "$OUT/candidates.md" "$WT"
python "$HERE/dc_verify.py" "$WT" "$OUT/candidates.md" "$OUT/verified.md"
python "$HERE/dc_report.py" "$WT" "$OUT/verified.md" "$OUT/report.md"
