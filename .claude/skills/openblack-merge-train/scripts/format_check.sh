# usage: format_check.sh <worktree>
# The format step of a car: every C++ file that the car changes against the main branch must match clang-format 17
# (src, apps, components and test, minus .clang-format-ignore). Exit 0 clean, 21 not clean, 22 no clang-format 17.
source "$(dirname "$0")/common.sh"
cd "$1" || exit 22
git diff --name-only --diff-filter=ACMR "$R/$M...HEAD" | sh tools/githooks/clang-format-gate.sh
s=$?
[ $s = 0 ] && exit 0
[ $s = 2 ] && exit 22
exit 21
