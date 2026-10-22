# The vcpkg triplet of the web build: static WebAssembly libraries made with the toolchain of the web build.
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED EMSCRIPTEN_ROOT EMSDK EM_CONFIG
                                    EMSDK_PYTHON PATH
)

if (NOT DEFINED ENV{EMSDK})
  message(FATAL_ERROR "Set EMSDK to the folder of the Emscripten SDK")
endif ()
if (NOT EXISTS
    "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
)
  message(FATAL_ERROR "The Emscripten toolchain file is not in $ENV{EMSDK}")
endif ()

set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../cmake/emscripten-openblack.cmake"
)

# The ports are built in release only. A debug build of the game links them too, and the port builds take half the
# time.
set(VCPKG_BUILD_TYPE release)
