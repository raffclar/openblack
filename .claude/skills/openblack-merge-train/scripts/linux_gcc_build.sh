#!/bin/bash
# Runs inside WSL (linux_gcc_server.sh copies it to <dir>/build.sh). Arguments: <dir> <vcpkg commit> [jobs].
# <dir>/stage holds the car's files. Exit 0: openblack-server builds; 1: it does not.
set -u
DIR=$1 VSHA=$2 JOBS=${3:-}
SRC=$DIR/src
PRESET=ninja-multi-vcpkg-shipping
note() { echo "gcc: $*"; }
mkdir -p "$SRC" "$DIR/vcpkg-binary"
avail=$(df -BG --output=avail "$DIR" | tail -1 | tr -dc 0-9)
[ "$avail" -ge 30 ] || { note "STOP: only $avail GB free in WSL"; exit 1; }
mem=$(awk '/MemAvailable/ {print int($2/1048576)}' /proc/meminfo)
[ "$mem" -ge 8 ] || { note "STOP: only $mem GB memory free in WSL"; exit 1; }
# Only the files that differ change, so ninja keeps the objects of the others
rsync -a --checksum --delete --exclude=/vcpkg --exclude=/cmake-build-presets "$DIR/stage/" "$SRC/" || { note "STOP: rsync"; exit 1; }
if [ ! -d "$SRC/vcpkg/.git" ]; then git clone -q --no-checkout https://github.com/microsoft/vcpkg.git "$SRC/vcpkg" || { note "STOP: vcpkg clone"; exit 1; }; fi
if ! git -C "$SRC/vcpkg" cat-file -e "$VSHA^{commit}" 2> /dev/null; then git -C "$SRC/vcpkg" fetch -q origin || { note "STOP: vcpkg fetch"; exit 1; }; fi
if [ "$(git -C "$SRC/vcpkg" rev-parse HEAD 2> /dev/null)" != "$VSHA" ]; then
  git -C "$SRC/vcpkg" checkout -q --force "$VSHA" || { note "STOP: vcpkg checkout $VSHA"; exit 1; }
  rm -f "$SRC/vcpkg/vcpkg"
fi
[ -x "$SRC/vcpkg/vcpkg" ] || "$SRC/vcpkg/bootstrap-vcpkg.sh" -disableMetrics > "$DIR/bootstrap.log" 2>&1 || { note "STOP: vcpkg bootstrap"; tail -5 "$DIR/bootstrap.log"; exit 1; }
export VCPKG_ROOT=$SRC/vcpkg VCPKG_DEFAULT_BINARY_CACHE=$DIR/vcpkg-binary
export VCPKG_BINARY_SOURCES="clear;files,$DIR/vcpkg-binary,readwrite" VCPKG_DISABLE_METRICS=1
cd "$SRC" || exit 1
cmake --preset "$PRESET" -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" > "$DIR/configure.log" 2>&1 || { note "STOP: configure"; grep -E "error|Error" "$DIR/configure.log" | head -20; tail -5 "$DIR/configure.log"; exit 1; }
cmake --build --preset "$PRESET-release" --target openblack-server ${JOBS:+--parallel "$JOBS"} > "$DIR/build.log" 2>&1
rc=$?
w=$(grep -cE ': warning:' "$DIR/build.log")
if [ $rc != 0 ]; then
  note "STOP: openblack-server does not build with GCC 13 (log $DIR/build.log in WSL); the errors:"
  grep -E "error:|FAILED:" "$DIR/build.log" | head -20
  exit 1
fi
[ -x "$SRC/cmake-build-presets/$PRESET/bin/Release/openblack-server" ] || { note "STOP: no openblack-server program"; exit 1; }
note "openblack-server builds with GCC 13, $w warnings; build tree $(du -sh "$SRC/cmake-build-presets" | cut -f1), vcpkg cache $(du -sh "$DIR/vcpkg-binary" | cut -f1)"
exit 0
