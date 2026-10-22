# bgfx 1.166 with the WebGPU renderer. vcpkg's own port is older (1.129), thus openblack keeps this port.
# The port builds bgfx with the third-party copies that come with this release (glslang, SPIRV-Tools,
# SPIRV-Cross and Tint for shaderc; lodepng, miniz, tinyexr and stb inside bimg). These are the versions that
# bgfx itself builds and tests. Only shaderc uses the shader libraries, and shaderc is a host tool.

if(VCPKG_TARGET_IS_WINDOWS)
  vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_download_distfile(
  ARCHIVE_FILE
  URLS https://github.com/bkaradzic/bgfx.cmake/releases/download/v${VERSION}/bgfx.cmake.v${VERSION}.tar.gz
  FILENAME bgfx.cmake.v${VERSION}.tar.gz
  SHA512 3fb29796aecba66f13e1c8541897bf28317c7fc10455f796cc6b1a317ed48bafabfd1b36ae207b4af480b7bc535f211b198f1e7abc5c7c5210dd04c9cfac29eb
)

vcpkg_extract_source_archive(
  SOURCE_PATH
  ARCHIVE "${ARCHIVE_FILE}"
)

# Replaces text in a file of the bgfx source. The build stops when the text is not there, so that a new bgfx
# release cannot drop a change without a message.
function(openblack_bgfx_replace file regex replacement)
  file(READ "${SOURCE_PATH}/${file}" _text)
  if(NOT _text MATCHES "${regex}")
    message(FATAL_ERROR "The bgfx source changed: '${regex}' is not in ${file}")
  endif()
  string(REGEX REPLACE "${regex}" "${replacement}" _text "${_text}")
  file(WRITE "${SOURCE_PATH}/${file}" "${_text}")
endfunction()

if(VCPKG_TARGET_IS_EMSCRIPTEN)
  # WebGL 2 has no 16-bit formats with the channel order of BGRA4 and BGR5A1, and a texture array needs a sized
  # format. Thus bgfx takes no 16-bit format as valid, and changes these textures to RGBA8 as it loads them.
  openblack_bgfx_replace(bgfx/src/renderer_gl.cpp
    "(case TextureFormat::R8: // Luminance\r?\n)([\t ]*case TextureFormat::(B5G6R5|R5G6B5|BGRA4|RGBA4|BGR5A1|RGB5A1):\r?\n)+"
    "\\1")
  # The web build loads resources on worker threads, as the desktop build does. bgfx keeps its locks only in its
  # multithreaded configuration, which bgfx.cmake turns off for Emscripten. The game draws on its own thread in
  # the web build (see Renderer.cpp), so bgfx starts no render thread there.
  openblack_bgfx_replace(CMakeLists.txt
    "cmake_dependent_option\\(BGFX_CONFIG_MULTITHREADED ([^\r\n]*) ON \"NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten\" OFF\\)"
    "option(BGFX_CONFIG_MULTITHREADED \\1 ON)")
endif()

vcpkg_check_features(
  OUT_FEATURE_OPTIONS FEATURE_OPTIONS
  FEATURES
    tools         BGFX_BUILD_TOOLS
    multithreaded BGFX_CONFIG_MULTITHREADED
    webgpu        BGFX_CONFIG_RENDERER_WEBGPU
)

if(VCPKG_TARGET_IS_EMSCRIPTEN)
  list(APPEND FEATURE_OPTIONS -DBGFX_CONFIG_MULTITHREADED=ON)
endif()

# shaderc loads the DirectX shader compiler at run time to make the Direct3D 12 shaders. The bgfx release has no
# Windows copy of it, thus the port downloads the release that vcpkg's directx-dxc port uses, and puts it where the
# bgfx build expects it. The port does not use directx-dxc itself, because that port puts release files in the debug
# folders.
if("tools" IN_LIST FEATURES AND VCPKG_TARGET_IS_WINDOWS)
  vcpkg_download_distfile(DXC_ARCHIVE
    URLS "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip"
    FILENAME "dxc_2025_07_14.zip"
    SHA512 69dd74ee2bc8a7ac569fa71351209412f74ef901c58dc59d413277e3129ce78d31dbca0996c2910048bfd0575e7a10655d89d361b5a2ed2963c831385711380f
  )
  vcpkg_extract_source_archive(DXC_PATH ARCHIVE "${DXC_ARCHIVE}" NO_REMOVE_ONE_LEVEL)
  file(COPY "${DXC_PATH}/bin/x64/dxcompiler.dll" "${DXC_PATH}/bin/x64/dxil.dll"
    DESTINATION "${SOURCE_PATH}/bgfx/tools/bin/windows")
endif()

if(VCPKG_LIBRARY_LINKAGE STREQUAL dynamic)
  set(BGFX_LIBRARY_TYPE "SHARED")
else()
  set(BGFX_LIBRARY_TYPE "STATIC")
endif()

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DBGFX_LIBRARY_TYPE=${BGFX_LIBRARY_TYPE}
    -DBGFX_AMALGAMATED=ON
    -DBGFX_BUILD_EXAMPLES=OFF
    -DBGFX_BUILD_TOOLS_GEOMETRY=OFF
    -DBGFX_BUILD_TOOLS_TEXTURE=OFF
    -DBGFX_CONFIG_VIDEO=OFF
    -DBGFX_OPENGLES_VERSION=30
    "-DBGFX_ADDITIONAL_TOOL_PATHS=${CURRENT_INSTALLED_DIR}/../${HOST_TRIPLET}/tools/bgfx"
    ${FEATURE_OPTIONS}
  OPTIONS_DEBUG
    -DBGFX_BUILD_TOOLS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/${PORT}")
vcpkg_copy_pdbs()

if("tools" IN_LIST FEATURES)
  vcpkg_copy_tools(TOOL_NAMES bin2c shaderc AUTO_CLEAN)
  # shaderc finds the DirectX shader compiler next to its own file.
  foreach(_runtime dxcompiler.dll libdxcompiler.so)
    if(EXISTS "${CURRENT_PACKAGES_DIR}/bin/${_runtime}")
      file(RENAME "${CURRENT_PACKAGES_DIR}/bin/${_runtime}" "${CURRENT_PACKAGES_DIR}/tools/${PORT}/${_runtime}")
    endif()
  endforeach()
  if(VCPKG_TARGET_IS_WINDOWS)
    file(COPY "${SOURCE_PATH}/bgfx/tools/bin/windows/dxil.dll" DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}")
  endif()
endif()

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
if(VCPKG_LIBRARY_LINKAGE STREQUAL static)
  file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()
