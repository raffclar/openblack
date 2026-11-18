#!/usr/bin/env bash
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
# Tests the build id check of "ob-build store" and "ob-build store-check" with a fake worktree, a fake build folder
# and a fake store in a temporary folder. No compiler runs.
#
#   bash tools/build/test_ob_build_store.sh
#
# A merge-train car and its merge commit have the same tree but not the same build id. The store must refuse the
# build of the car under the merge commit, and keep it under the car's own commit.
set -u
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OB_BUILD="$HERE/ob-build"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
export OB_ROOT="$TMP/root" OB_MAIN_REPO="$TMP/wt" OB_JOB_METER=0
failures=0

check() { # check <name> <expected exit code> <command...>
	local name=$1 want=$2
	shift 2
	"$@" >"$TMP/out.txt" 2>&1
	local got=$?
	cp "$TMP/out.txt" "$TMP/last.txt"
	if [ "$got" = "$want" ]; then
		echo "ok: $name"
	else
		echo "FAILED: $name (exit $got, wanted $want)"
		sed 's/^/  /' "$TMP/out.txt"
		failures=$((failures + 1))
	fi
}

# A worktree with a car commit and a merge commit of the same tree
mkdir -p "$TMP/wt" "$OB_ROOT/builds"
git -C "$TMP/wt" init -q
touch "$TMP/wt/CMakeLists.txt" "$TMP/wt/vcpkg.json"
git -C "$TMP/wt" add -A
git -C "$TMP/wt" -c user.name=test -c user.email=test@example.com commit -q -m car
car=$(git -C "$TMP/wt" rev-parse HEAD)
git -C "$TMP/wt" -c user.name=test -c user.email=test@example.com commit -q --allow-empty -m merge
merge=$(git -C "$TMP/wt" rev-parse HEAD)
tree=$(git -C "$TMP/wt" rev-parse 'HEAD^{tree}')

# A build folder of the car: its programs hold the car's build id, and the server a sample id of its help text too
build="$OB_ROOT/builds/car"
mkdir -p "$build/bin/RelWithDebInfo"
printf 'x\0000.1.0+%s\0x' "${car:0:12}" >"$build/bin/RelWithDebInfo/openblack.exe"
printf 'x\0000.1.0+0123456789ab\0000.1.0+%s\0x' "${car:0:12}" >"$build/bin/RelWithDebInfo/openblack-server.exe"
touch "$build/bin/RelWithDebInfo/bgfx.dll"
printf '%s' "$tree" >"$build/.ob-build-tree-RelWithDebInfo"

store() { bash "$OB_BUILD" store --worktree "$TMP/wt" --build-dir "$build" --config RelWithDebInfo "$1"; }
check "the store refuses the car's build under the merge commit" 1 store "$merge"
check "the refusal names the build id of the car" 0 grep -q "0.1.0+${car:0:12} and not the id of" "$TMP/last.txt"
check "nothing is stored under the merge commit" 1 test -e "$OB_ROOT/builds/master-bin/${merge:0:9}/RelWithDebInfo"
check "the store keeps the car's build under the car's commit" 0 store "$car"
check "store-check passes the car's commit" 0 bash "$OB_BUILD" store-check --config RelWithDebInfo "$car"

# An older store put the car's build under the merge commit
cp -r "$OB_ROOT/builds/master-bin/${car:0:9}" "$OB_ROOT/builds/master-bin/${merge:0:9}"
check "store-check refuses the car's build under the merge commit" 1 \
	bash "$OB_BUILD" store-check --config RelWithDebInfo "$merge"

# The build again at the merge commit has the merge commit's id: the store replaces the old folder
printf 'x\0000.1.0+%s\0x' "${merge:0:12}" >"$build/bin/RelWithDebInfo/openblack.exe"
printf 'x\0000.1.0+0123456789ab\0000.1.0+%s\0x' "${merge:0:12}" >"$build/bin/RelWithDebInfo/openblack-server.exe"
check "the store replaces a stored build with another commit's id" 0 store "$merge"
check "store-check passes the merge commit after the new build" 0 \
	bash "$OB_BUILD" store-check --config RelWithDebInfo "$merge"

[ $failures = 0 ] && echo "ob-build store tests: PASS" || echo "ob-build store tests: FAIL ($failures)"
[ $failures = 0 ]
