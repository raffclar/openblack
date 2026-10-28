# Shared settings of the merge-train scripts. Source this file; do not run it.
# The values come from .claude/openblack.local.json through .claude/obconfig.py (OB_REMOTE, OB_TRAIN and the others).
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CLAUDE_DIR=$(cd "$HERE/../../.." && pwd)
eval "$(python "$CLAUDE_DIR/obconfig.py" shell)" || { echo "STOP: cannot read the skill settings (.claude/obconfig.py)"; exit 20; }
P=$OB_TRAIN
OB=$OB_BUILD_TOOL
R=$OB_REMOTE
M=$OB_MAIN_BRANCH
[ -n "$P" ] && [ -n "$OB_MAIN_CHECKOUT" ] || { echo "STOP: set paths.mergeTrain and paths.mainCheckout in .claude/openblack.local.json"; exit 20; }
mkdir -p "$P/carlogs" "$P/prs"

# Counts the compiler warnings in a build log (MSVC, GCC and Clang forms)
warnings() { grep -cE 'warning C[0-9]|: warning:' "$1"; }

# The test program of a build folder, with or without the Windows suffix
test_program() { if [ -f "$1/bin/Debug/openblack_tests.exe" ]; then echo "$1/bin/Debug/openblack_tests.exe"; else echo "$1/bin/Debug/openblack_tests"; fi; }

# The optional check of another machine (train.extraCheck): called with "start" before the build and "report" after it
extra_check() { [ -z "$OB_EXTRA_CHECK" ] || bash "$OB_EXTRA_CHECK" "$@" </dev/null; }
