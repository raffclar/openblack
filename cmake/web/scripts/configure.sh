#!/bin/bash
# Configures the web experiment build. Usage: configure.sh [extra cmake args]
source "$(dirname "$0")/web-env.sh"
df -h /e | tail -1
avail=$(df --output=avail -BG /e | tail -1 | tr -dc 0-9)
if [ "$avail" -lt 20 ]; then echo "less than 20 GB free, stop"; exit 2; fi
mkdir -p "$B"
"$CMAKE" -S "$WT" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_MAKE_PROGRAM=C:/Users/Aluda/AppData/Local/Programs/CLion/bin/ninja/win/x64/ninja.exe \
	-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
	-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$WT/cmake/web/emscripten-openblack.cmake \
	-DVCPKG_OVERLAY_TRIPLETS=$WT/cmake/web/triplets -DVCPKG_OVERLAY_PORTS=$WT/cmake/web/ports \
	-DVCPKG_TARGET_TRIPLET=wasm32-emscripten-openblack -DVCPKG_HOST_TRIPLET=x64-windows -DOPENBLACK_USE_VCPKG=ON \
	"-DVCPKG_INSTALL_OPTIONS=--x-buildtrees-root=$B/vcpkg-buildtrees;--x-packages-root=$B/vcpkg-packages" \
	-DOPENBLACK_WARNINGS_AS_ERRORS=OFF "$@"
echo "configure exit $?"
