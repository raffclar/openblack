# Toolchain for the web build of openblack and of its vcpkg ports.
# It uses the Emscripten toolchain and adds the flags that all code needs.
include("$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

# Newer Emscripten releases do not define the old EMSCRIPTEN macro. The texture
# encoder in bimg needs it to find the platform.
set(OPENBLACK_WEB_FLAGS "-DEMSCRIPTEN=1")
# All code shares one memory between threads, so all code must compile with
# thread support. Exceptions use the WebAssembly exception instructions.
string(APPEND OPENBLACK_WEB_FLAGS " -pthread -fwasm-exceptions")
# WebGL 2 has no texture copy and no texture read. bgfx then does them with
# frame buffers.
string(APPEND OPENBLACK_WEB_FLAGS
       " -DBGFX_GL_CONFIG_BLIT_EMULATION=1 -DBGFX_GL_CONFIG_TEXTURE_READ_BACK_EMULATION=1")

string(APPEND CMAKE_C_FLAGS_INIT " ${OPENBLACK_WEB_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS_INIT " ${OPENBLACK_WEB_FLAGS}")
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -pthread -fwasm-exceptions")
