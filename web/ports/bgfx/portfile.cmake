if(VCPKG_TARGET_IS_WINDOWS)
  vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

# Experiment webgpu-1: bgfx 1.166 has the WebGPU renderer. The release archive of bgfx.cmake holds bx, bimg and bgfx
# with all their third-party sources (glslang, SPIRV-Cross, SPIRV-Tools and Tint for the WGSL output of shaderc).
# This port builds the third-party sources that the archive holds, so that shaderc gets the versions it expects.
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

function(openblack_replace file from to)
  file(READ "${file}" _text)
  string(FIND "${_text}" "${from}" _at)
  if(_at EQUAL -1)
    message(FATAL_ERROR "The bgfx source changed: '${from}' is not in ${file}")
  endif()
  string(REPLACE "${from}" "${to}" _text "${_text}")
  file(WRITE "${file}" "${_text}")
endfunction()

set(OPENBLACK_EMDAWNWEBGPU_PORT "${CMAKE_CURRENT_LIST_DIR}/../../third_party/emdawnwebgpu_pkg/emdawnwebgpu.port.py")
cmake_path(NORMAL_PATH OPENBLACK_EMDAWNWEBGPU_PORT)

if(VCPKG_TARGET_IS_EMSCRIPTEN)
  # The WebGPU renderer is ready for Emscripten, but bgfx does not turn it on by default there.
  openblack_replace("${SOURCE_PATH}/bgfx/src/config.h" "/*|| BX_PLATFORM_EMSCRIPTEN*/" "|| BX_PLATFORM_EMSCRIPTEN")
  # bgfx compiles against the Dawn header that it keeps, also for the browser. The emdawnwebgpu package in
  # web/third_party is the Dawn release of the same days (2026-06-23): its structures and values are the same.
  # The port gives the JavaScript bindings at the link.
  openblack_replace("${SOURCE_PATH}/cmake/bgfx/bgfx.cmake"
    "target_link_options(bgfx PUBLIC \"--use-port=emdawnwebgpu\")"
    "target_compile_options(bgfx PRIVATE \"--use-port=${OPENBLACK_EMDAWNWEBGPU_PORT}\")\n\t\ttarget_link_options(bgfx PUBLIC \"--use-port=${OPENBLACK_EMDAWNWEBGPU_PORT}\")")

  # A canvas in the browser gives the same texture until the browser shows it, and the texture is not valid after that.
  # bgfx takes the next texture right after it presents, before the browser shows the frame. In the browser bgfx must
  # take the texture at the start of each frame.
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.h"
    "WGPUTextureView createTextureView();\n\t\tvoid present();"
    "WGPUTextureView createTextureView();\n\t\tvoid present();\n\t\tvoid acquire();")
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\tvoid SwapChainWGPU::present()\n\t{\n\t\twgpuRelease(m_textureView);\n\t\twgpuRelease(m_texture);\n"
    "\tvoid SwapChainWGPU::acquire()\n\t{\n\t\tif (NULL != m_texture || NULL == m_surface)\n\t\t{\n\t\t\treturn;\n\t\t}\n\n\t\tWGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;\n\t\twgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);\n\t\tm_texture = surfaceTexture.texture;\n\n\t\tif (NULL != m_texture)\n\t\t{\n\t\t\tm_textureView = createTextureView();\n\t\t}\n\t}\n\n\tvoid SwapChainWGPU::present()\n\t{\n\t\twgpuRelease(m_textureView);\n\t\twgpuRelease(m_texture);\n#if BX_PLATFORM_EMSCRIPTEN\n\t\tm_textureView = NULL;\n\t\tm_texture = NULL;\n\t\treturn;\n#endif // BX_PLATFORM_EMSCRIPTEN\n")
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\t\tif (_render->m_capture)\n"
    "#if BX_PLATFORM_EMSCRIPTEN\n\t\tfor (uint16_t ii = 0; ii < m_numWindows; ++ii)\n\t\t{\n\t\t\tgetFrameBuffer(m_windows[ii]).m_swapChain.acquire();\n\t\t}\n#endif // BX_PLATFORM_EMSCRIPTEN\n\n\t\tif (_render->m_capture)\n")

  # bgfx.cmake 1.166 forces the single-threaded configuration on Emscripten, which has no locks. openblack makes
  # resources on its loading threads, so the web build needs the locks.
  openblack_replace("${SOURCE_PATH}/CMakeLists.txt"
    "cmake_dependent_option(BGFX_CONFIG_MULTITHREADED \"Build bgfx with multithreaded configuration\" ON \"NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten\" OFF)"
    "option(BGFX_CONFIG_MULTITHREADED \"Build bgfx with multithreaded configuration\" ON)")

  # A draw that leaves a texture of its shader unset gets a small black texture there, as WebGPU needs every binding
  # of the layout. The layout then takes that binding as a filterable float texture.
  file(READ "${CMAKE_CURRENT_LIST_DIR}/unbound_helpers.inl" _unbound_helpers)
  file(READ "${CMAKE_CURRENT_LIST_DIR}/unbound_bind.inl" _unbound_bind)
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\t\tuint8_t fillBindGroupLayoutEntry("
    "${_unbound_helpers}\t\tuint8_t fillBindGroupLayoutEntry(")
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\t\t\t\t\t\tTextureWGPU& texture = m_textures[bind.m_idx];\n\n\t\t\t\t\t\tconst uint32_t resolvedFlags = 0 == (BGFX_SAMPLER_INTERNAL_DEFAULT & bind.m_samplerFlags)\n\t\t\t\t\t\t\t? bind.m_samplerFlags\n\t\t\t\t\t\t\t: uint32_t(texture.m_flags)\n\t\t\t\t\t\t\t;\n\n\t\t\t\t\t\tWGPUTextureSampleType sampleType = WGPUTextureSampleType_Depth != shaderBind.sampleType\n\t\t\t\t\t\t\t? s_textureFormat[texture.m_textureFormat].m_samplerType\n\t\t\t\t\t\t\t: shaderBind.sampleType\n\t\t\t\t\t\t\t;"
    "\t\t\t\t\t\tconst bool unbound = kInvalidHandle == bind.m_idx;\n\t\t\t\t\t\tTextureWGPU& texture = m_textures[unbound ? 0 : bind.m_idx];\n\n\t\t\t\t\t\tconst uint32_t resolvedFlags = unbound ? 0 : 0 == (BGFX_SAMPLER_INTERNAL_DEFAULT & bind.m_samplerFlags)\n\t\t\t\t\t\t\t? bind.m_samplerFlags\n\t\t\t\t\t\t\t: uint32_t(texture.m_flags)\n\t\t\t\t\t\t\t;\n\n\t\t\t\t\t\tWGPUTextureSampleType sampleType = unbound ? WGPUTextureSampleType_Float : WGPUTextureSampleType_Depth != shaderBind.sampleType\n\t\t\t\t\t\t\t? s_textureFormat[texture.m_textureFormat].m_samplerType\n\t\t\t\t\t\t\t: shaderBind.sampleType\n\t\t\t\t\t\t\t;")
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\t\t\t\t\t\t\t\t: m_textures[bind.m_idx].m_viewDimension\n"
    "\t\t\t\t\t\t\t\t: unbound ? WGPUTextureViewDimension_2D : m_textures[bind.m_idx].m_viewDimension\n")
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t}\n\n\t\t\tBX_ASSERT(_program.m_numBindings <= entryCount, \"\");\n\n\t\t\tWGPUBindGroupDescriptor bindGroupDesc ="
    "\t\t\t\t\t}\n\t\t\t\t}\n${_unbound_bind}\t\t\t}\n\n\t\t\tBX_ASSERT(_program.m_numBindings <= entryCount, \"\");\n\n\t\t\tWGPUBindGroupDescriptor bindGroupDesc =")

  # A shader input that no vertex stream gives reads the start of the last stream. WebGPU refuses an input that is
  # longer than the vertex, so the input takes only as many floats as the vertex holds.
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\t\t\t\t\t\telem->format = WGPUVertexFormat_Float32x3;\n\t\t\t\t\t\telem->offset = 0;"
    "\t\t\t\t\t\telem->format = _layout.m_stride >= 12 ? WGPUVertexFormat_Float32x3\n\t\t\t\t\t\t\t: _layout.m_stride >= 8 ? WGPUVertexFormat_Float32x2\n\t\t\t\t\t\t\t: WGPUVertexFormat_Float32\n\t\t\t\t\t\t\t;\n\t\t\t\t\t\telem->offset = 0;")

  # A read of a texture or a buffer waits for the map of a GPU buffer. In the browser the map completes only when the
  # thread gives control back to the browser, so the wait must yield (with JSPI) and not only spin.
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "#include \"bgfx_p.h\"\n"
    "#include \"bgfx_p.h\"\n#if BX_PLATFORM_EMSCRIPTEN\n#	include <emscripten.h>\n#endif // BX_PLATFORM_EMSCRIPTEN\n")
  openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
    "\t\t\twhile (!s_done)\n\t\t\t{\n\t\t\t\twgpuInstanceProcessEvents(m_instance);\n\t\t\t}"
    "\t\t\twhile (!s_done)\n\t\t\t{\n#if BX_PLATFORM_EMSCRIPTEN\n\t\t\t\temscripten_sleep(0);\n#endif // BX_PLATFORM_EMSCRIPTEN\n\t\t\t\twgpuInstanceProcessEvents(m_instance);\n\t\t\t}")

  # The WebGL 2 renderer stays as the fallback. These are the changes of the web-build-2 port; here they only warn
  # when the source changed.
  file(READ "${SOURCE_PATH}/bgfx/src/renderer_gl.cpp" _renderer_gl)
  string(REPLACE "|| (m_gles3 && !BX_ENABLED(BX_PLATFORM_EMSCRIPTEN) )" "|| m_gles3" _renderer_gl "${_renderer_gl}")
  string(REPLACE "\"#define texture2DLod    textureLod\\n\""
                 "\"#define texture2DLod    textureLod\\n\"\n\t\t\t\t\t\t  \"#define texture2DArrayLodEXT textureLod\\n\""
                 _renderer_gl "${_renderer_gl}")
  string(REGEX REPLACE
    "(case TextureFormat::R8: // Luminance\r?\n)([\t ]*case TextureFormat::(B5G6R5|R5G6B5|BGRA4|RGBA4|BGR5A1|RGB5A1):\r?\n)+"
    "\\1" _renderer_gl "${_renderer_gl}")
  file(WRITE "${SOURCE_PATH}/bgfx/src/renderer_gl.cpp" "${_renderer_gl}")
endif()

# WebGPU can read every texture that it samples in a vertex shader, but bgfx does not give the vertex flag for any
# format. openblack reads the creature morph targets from a float texture in the vertex shader.
openblack_replace("${SOURCE_PATH}/bgfx/src/renderer_webgpu.cpp"
  "\t\t\t\t| BGFX_CAPS_FORMAT_TEXTURE_CUBE\n"
  "\t\t\t\t| BGFX_CAPS_FORMAT_TEXTURE_CUBE\n\t\t\t\t| BGFX_CAPS_FORMAT_TEXTURE_VERTEX\n")

vcpkg_check_features(
  OUT_FEATURE_OPTIONS FEATURE_OPTIONS
  FEATURES
    tools         BGFX_BUILD_TOOLS
    multithreaded BGFX_CONFIG_MULTITHREADED
)

if(VCPKG_TARGET_IS_EMSCRIPTEN)
  list(APPEND FEATURE_OPTIONS -DBGFX_CONFIG_MULTITHREADED=ON -DBGFX_CONFIG_RENDERER_WEBGPU=ON)
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
    -DBGFX_BUILD_TESTS=OFF
    -DBGFX_CONFIG_VIDEO=OFF
    -DBGFX_CUSTOM_TARGETS=OFF
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
  vcpkg_copy_tools(TOOL_NAMES bin2c shaderc geometryc geometryv texturec texturev AUTO_CLEAN)
  # shaderc of bgfx 1.166 makes the DXIL shaders for Direct3D 12 with the DXC compiler, which it loads at run time
  if(VCPKG_TARGET_IS_WINDOWS)
    file(COPY "${SOURCE_PATH}/bgfx/tools/bin/windows/dxcompiler.dll" "${SOURCE_PATH}/bgfx/tools/bin/windows/dxil.dll"
         DESTINATION "${CURRENT_PACKAGES_DIR}/tools/${PORT}")
  endif()
endif ()

file(GLOB _licence "${CURRENT_PACKAGES_DIR}/share/licences/${PORT}/LICENSE")
if(_licence)
  vcpkg_install_copyright(FILE_LIST "${_licence}")
else()
  vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/bgfx/LICENSE")
endif()

file(REMOVE_RECURSE
  "${CURRENT_PACKAGES_DIR}/share/licences"
  "${CURRENT_PACKAGES_DIR}/debug/include"
  "${CURRENT_PACKAGES_DIR}/debug/share"
)
