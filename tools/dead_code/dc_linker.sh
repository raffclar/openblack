#!/usr/bin/env bash
# The linker cross-check: links openblack with /OPT:REF /VERBOSE:REF and compares the functions the linker drops with
# the candidate list of dc_classify.py.
#
#   tools/dead_code/dc_linker.sh <worktree> <out folder> [candidates.md]
#
# Run it while you hold a build lock. The worktree must have a RelWithDebInfo build (ob-build build ... openblack).
# Set OB_BUILD to the ob-build script when it is not on the PATH, and DC_VCVARS to vcvars64.bat of Visual Studio.
#
# The link runs here and not through ninja. The link launcher of ob-build does not pass the output of the linker on, so the
# /VERBOSE:REF text would be lost. The script makes ninja write the response file of the link (keeprsp), and then runs
# link.exe itself in the Visual Studio environment. The result goes to a probe folder in the out folder, so the game of the
# build folder stays as it is. The log of a full link is about 400 MB.
set -uo pipefail
[ $# -ge 2 ] || { echo "usage: $0 <worktree> <out folder> [candidates.md]" >&2; exit 2; }
WT=$1
OUT=$2
CAND=${3:-$OUT/candidates.md}
OB=${OB_BUILD:-ob-build}
VCVARS=${DC_VCVARS:-C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat}
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
mkdir -p "$OUT/probe"
export OB_LINK_JOBS=1 OB_CONFIG=RelWithDebInfo
BUILD=$("$OB" dir --worktree "$WT" --config RelWithDebInfo)
# The old game is deleted so that ninja links again and writes the response file.
rm -f "$BUILD/bin/RelWithDebInfo/openblack.exe"
"$OB" build --worktree "$WT" --config RelWithDebInfo openblack -- -d keeprsp >"$OUT/build.log" 2>&1 || exit 1
RSP="$BUILD/CMakeFiles/openblack.RelWithDebInfo.rsp"
[ -f "$RSP" ] || { echo "no response file $RSP" >&2; exit 1; }
WIN() { cygpath -w "$1"; }
BAT="$OUT/link_probe.bat"
{
	echo '@echo off'
	echo "call \"$(WIN "$VCVARS")\" >nul 2>&1"
	echo "cd /d $(WIN "$BUILD")"
	echo "link.exe /nologo @CMakeFiles\openblack.RelWithDebInfo.rsp /out:$(WIN "$OUT/probe/openblack.exe")" \
		"/implib:$(WIN "$OUT/probe/openblack.lib") /pdb:$(WIN "$OUT/probe/openblack.pdb") /version:0.0 /machine:x64" \
		"/OPT:REF /OPT:NOICF /VERBOSE:REF /debug /INCREMENTAL:NO /subsystem:console > $(WIN "$OUT/linker_verbose.log") 2>&1"
	echo 'exit /b %errorlevel%'
} >"$BAT"
cmd //c "$(WIN "$BAT")" || { echo "the link failed" >&2; exit 1; }
rm -rf "$OUT/probe"
grep -c "Discarded" "$OUT/linker_verbose.log"
python "$HERE/dc_linker_compare.py" "$OUT/linker_verbose.log" "$CAND" "$OUT/linker_compare.md"
