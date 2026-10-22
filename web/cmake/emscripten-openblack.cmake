# Toolchain for the web build of openblack and of its vcpkg ports. It loads the Emscripten toolchain and adds the flags
# that all code of the web build needs. The desktop builds do not use this file.
if (NOT DEFINED ENV{EMSDK})
  message(FATAL_ERROR "Set EMSDK to the folder of the Emscripten SDK")
endif ()
include(
  "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
)

# New Emscripten releases do not define the old EMSCRIPTEN macro. The texture code of bimg needs it to find the
# platform.
set(OPENBLACK_WEB_FLAGS "-DEMSCRIPTEN=1")
# All threads share one memory, so all code must compile with thread support. Exceptions use the WebAssembly exception
# instructions.
string(APPEND OPENBLACK_WEB_FLAGS " -pthread -fwasm-exceptions")
# The web build has threads, which bx turns off for every Emscripten build. bgfx needs them for its locks.
string(APPEND OPENBLACK_WEB_FLAGS " -DBX_CONFIG_SUPPORTS_THREADING=1")

string(APPEND CMAKE_C_FLAGS_INIT " ${OPENBLACK_WEB_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS_INIT " ${OPENBLACK_WEB_FLAGS}")
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -pthread -fwasm-exceptions")
