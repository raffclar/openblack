if(VCPKG_TARGET_IS_WINDOWS)
  vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_download_distfile(
  ARCHIVE_FILE
  URLS https://github.com/bkaradzic/bgfx.cmake/releases/download/v${VERSION}/bgfx.cmake.v${VERSION}.tar.gz
  FILENAME bgfx.cmake.v${VERSION}.tar.gz
  SHA512 520c542b65e76e92eae818e32eeb62bb2347ac89a1e10fc07cd5059a3b8a9a543cadca87d451a3bc157c415f6183b1f0e5031248e38fae704b8efd54679d482b
)

vcpkg_extract_source_archive(
  SOURCE_PATH
  ARCHIVE "${ARCHIVE_FILE}"
  PATCHES
    fix-dependencies.patch
)
file(REMOVE_RECURSE
  "${SOURCE_PATH}/bgfx/3rdparty/dear-imgui"
  "${SOURCE_PATH}/bgfx/3rdparty/glslang"
  "${SOURCE_PATH}/bgfx/3rdparty/meshoptimizer"
  "${SOURCE_PATH}/bgfx/3rdparty/spirv-cross"
  "${SOURCE_PATH}/bgfx/3rdparty/spirv-headers"
  "${SOURCE_PATH}/bgfx/3rdparty/spirv-opt"
  "${SOURCE_PATH}/bgfx/3rdparty/stb"
  "${SOURCE_PATH}/bimg/3rdparty/libsquish"
  "${SOURCE_PATH}/bimg/3rdparty/lodepng"
  "${SOURCE_PATH}/bimg/3rdparty/stb"
  "${SOURCE_PATH}/bimg/3rdparty/tinyexr"
)

# WebGL 2 has 2D texture arrays and the vertex index in shaders, but bgfx turns
# both off for every Emscripten build. openblack needs the texture arrays (the
# land textures), so the web build keeps both.
if(VCPKG_TARGET_IS_EMSCRIPTEN)
  file(READ "${SOURCE_PATH}/bgfx/src/renderer_gl.cpp" _renderer_gl)
  string(REPLACE "|| (m_gles3 && !BX_ENABLED(BX_PLATFORM_EMSCRIPTEN) )" "|| m_gles3" _renderer_gl "${_renderer_gl}")
  # The GLSL ES 3.00 shaders from shaderc call texture2DArrayLodEXT, which the
  # OpenGL ES 3 path of bgfx does not define. There it is textureLod.
  string(REPLACE "\"#define texture2DLod    textureLod\\n\""
                 "\"#define texture2DLod    textureLod\\n\"\n\t\t\t\t\t\t  \"#define texture2DArrayLodEXT textureLod\\n\""
                 _renderer_gl "${_renderer_gl}")
  # WebGL 2 has no 16-bit formats with the channel order of BGRA4 and BGR5A1, and a texture array needs a sized
  # format. Thus bgfx takes no 16-bit format as valid, and changes these textures to RGBA8 as it loads them.
  string(REGEX REPLACE
    "(case TextureFormat::R8: // Luminance\r?\n)([\t ]*case TextureFormat::(B5G6R5|R5G6B5|BGRA4|RGBA4|BGR5A1|RGB5A1):\r?\n)+"
    "\\1" _renderer_gl "${_renderer_gl}")
  if(NOT _renderer_gl MATCHES "case TextureFormat::R8: // Luminance\r?\n[\t ]*// GLES2 formats without sRGB")
    message(FATAL_ERROR "The bgfx source changed: the web build cannot turn off its 16-bit formats")
  endif()
  file(WRITE "${SOURCE_PATH}/bgfx/src/renderer_gl.cpp" "${_renderer_gl}")
endif()

vcpkg_check_features(
  OUT_FEATURE_OPTIONS FEATURE_OPTIONS
  FEATURES
    tools         BGFX_BUILD_TOOLS
    multithreaded BGFX_CONFIG_MULTITHREADED
)

# The web build loads resources on worker threads, as the desktop build does. bgfx keeps its locks only in its
# multithreaded configuration. The game draws on its own thread in the web build (see Renderer.cpp), so bgfx starts
# no render thread there.
if(VCPKG_TARGET_IS_EMSCRIPTEN)
  list(APPEND FEATURE_OPTIONS -DBGFX_CONFIG_MULTITHREADED=ON)
endif()

if(VCPKG_LIBRARY_LINKAGE STREQUAL dynamic)
  set(BGFX_LIBRARY_TYPE "SHARED")
else ()
  set(BGFX_LIBRARY_TYPE "STATIC")
endif ()

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DBGFX_LIBRARY_TYPE=${BGFX_LIBRARY_TYPE}
    -DBGFX_AMALGAMATED=ON
    -DBGFX_BUILD_EXAMPLES=OFF
    -DBGFX_OPENGLES_VERSION=30
    "-DBGFX_ADDITIONAL_TOOL_PATHS=${CURRENT_INSTALLED_DIR}/../${HOST_TRIPLET}/tools/bgfx"
    ${FEATURE_OPTIONS}
  OPTIONS_DEBUG
    -DBGFX_BUILD_TOOLS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")
vcpkg_copy_pdbs()

if ("tools" IN_LIST FEATURES)
  vcpkg_copy_tools(TOOL_NAMES bin2c shaderc geometryc texturec AUTO_CLEAN)
endif ()

vcpkg_install_copyright(
  FILE_LIST "${CURRENT_PACKAGES_DIR}/share/licences/${PORT}/LICENSE"
  COMMENT [[
bgfx includes third-party components which are subject to specific license
terms. Check the sources for details.
]])

file(REMOVE_RECURSE
  "${CURRENT_PACKAGES_DIR}/share/licences"
  "${CURRENT_PACKAGES_DIR}/debug/include"
  "${CURRENT_PACKAGES_DIR}/debug/share"
)
