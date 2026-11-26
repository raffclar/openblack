# usage: tidy_check.sh <worktree> <build dir> [web build dir]
# The tidy step of a car: clang-tidy 17 on the lines that the car changes against the main branch (src, apps, components
# and test). A finding on a changed line, or a NOLINT without a check name or a reason, fails. The build dir is the
# folder of the car's own configure (it has compile_commands.json). A finding on a line the car did not change never fails.
# A changed file of src/Web is compiled only by the web build, so it is checked with the compile_commands.json of the web
# build (the third argument, from ob-web configure). Without that database, such a change stops the check (exit 24).
# Exit 0 clean, 23 a finding, 24 the check cannot run (no clang-tidy 17, no compile_commands.json, or a time-out).
source "$(dirname "$0")/common.sh"
cd "$1" || exit 24
python tools/githooks/tidy_gate.py range "$R/$M" --strict --build-dir "$2" ${3:+--web-build-dir "$3"} --jobs 4 --timeout 300 </dev/null
s=$?
[ $s = 0 ] && exit 0
[ $s = 1 ] && exit 23
exit 24
