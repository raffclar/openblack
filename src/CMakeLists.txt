# CONFIGURE_DEPENDS: an added, removed or renamed source, header or shader makes the build re-run CMake by itself
file(GLOB_RECURSE OPENBLACK_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
# the mod loader is a library of its own (src/ModLoader), never linked into the game
list(FILTER OPENBLACK_SOURCES EXCLUDE REGEX "/src/ModLoader/")
if (NOT ANDROID)
  list(REMOVE_ITEM OPENBLACK_SOURCES "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
endif ()
if (NOT OPENBLACK_USE_BINK)
  list(FILTER OPENBLACK_SOURCES EXCLUDE REGEX "/src/(Video/Bink[A-Za-z]+|Debug/VideoViewer)[.]cpp$")
endif ()
file(GLOB_RECURSE OPENBLACK_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.h")
list(FILTER OPENBLACK_HEADERS EXCLUDE REGEX "/src/ModLoader/")
if (APPLE)
  file(GLOB_RECURSE OPENBLACK_OBJC_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.mm")
  list(APPEND OPENBLACK_SOURCES "${OPENBLACK_OBJC_SOURCES}")
endif ()

set(OPENBLACK_RC ${CMAKE_SOURCE_DIR}/assets/resource/icon/openblack.rc)

file(GLOB OPENBLACK_IMGUI_VS_SHADERS CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/assets/shaders/imgui/vs_*.sc"
)
file(GLOB OPENBLACK_IMGUI_FS_SHADERS CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/assets/shaders/imgui/fs_*.sc"
)
bgfx_compile_shaders(
  AS_HEADERS
  TYPE VERTEX
  SHADERS ${OPENBLACK_IMGUI_VS_SHADERS}
  VARYING_DEF ${CMAKE_SOURCE_DIR}/assets/shaders/imgui/varying.def.sc
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders/imgui
)
bgfx_compile_shaders(
  AS_HEADERS
  TYPE FRAGMENT
  SHADERS ${OPENBLACK_IMGUI_FS_SHADERS}
  VARYING_DEF ${CMAKE_SOURCE_DIR}/assets/shaders/imgui/varying.def.sc
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders/imgui
)

file(GLOB OPENBLACK_VERTEX_SHADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/shaders/vs_*.sc")
file(GLOB OPENBLACK_FRAGMENT_SHADERS CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/assets/shaders/fs_*.sc"
)
file(GLOB OPENBLACK_VARYING_DEF_FILE CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/assets/shaders/varying.def.sc"
)

bgfx_compile_shaders(
  AS_HEADERS
  TYPE VERTEX
  SHADERS ${OPENBLACK_VERTEX_SHADERS}
  VARYING_DEF ${OPENBLACK_VARYING_DEF_FILE}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
)
bgfx_compile_shaders(
  AS_HEADERS
  TYPE FRAGMENT
  SHADERS ${OPENBLACK_FRAGMENT_SHADERS}
  VARYING_DEF ${OPENBLACK_VARYING_DEF_FILE}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
)

# bgfx_compile_shaders only knows each shader's .sc and varying.def. The files a shader includes with #include "..."
# (the .sh helpers and the .sc another .sc builds on, followed through their own includes) become dependencies of that
# shader, so an edit to one rebuilds exactly the shaders that reach it. The shaders and what they include are also
# configure dependencies: an edit that adds an #include re-runs CMake, which finds the new file
function(openblack_shader_include_depends OUTPUT_DIR)
  foreach (SHADER ${ARGN})
    set(PENDING ${SHADER})
    set(INCLUDED "")
    while (PENDING)
      list(POP_FRONT PENDING CURRENT)
      get_filename_component(CURRENT_DIR ${CURRENT} DIRECTORY)
      file(STRINGS ${CURRENT} INCLUDE_LINES REGEX "^[ \t]*#[ \t]*include[ \t]*\"")
      foreach (LINE ${INCLUDE_LINES})
        string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*\"([^\"]+)\".*$" "\\1" NAME "${LINE}")
        get_filename_component(INCLUDE_FILE ${NAME} ABSOLUTE BASE_DIR ${CURRENT_DIR})
        if (EXISTS ${INCLUDE_FILE} AND NOT INCLUDE_FILE IN_LIST INCLUDED)
          list(APPEND INCLUDED ${INCLUDE_FILE})
          list(APPEND PENDING ${INCLUDE_FILE})
        endif ()
      endforeach ()
    endwhile ()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SHADER} ${INCLUDED})
    if (INCLUDED)
      # the first output of the shader's command (bgfx_compile_shaders: <OUTPUT_DIR>/glsl/<name>.bin.h)
      get_filename_component(SHADER_NAME ${SHADER} NAME)
      add_custom_command(OUTPUT ${OUTPUT_DIR}/glsl/${SHADER_NAME}.bin.h APPEND DEPENDS ${INCLUDED})
    endif ()
  endforeach ()
endfunction ()
openblack_shader_include_depends(
  ${CMAKE_BINARY_DIR}/include/generated/shaders/imgui ${OPENBLACK_IMGUI_VS_SHADERS} ${OPENBLACK_IMGUI_FS_SHADERS}
)
openblack_shader_include_depends(
  ${CMAKE_BINARY_DIR}/include/generated/shaders ${OPENBLACK_VERTEX_SHADERS} ${OPENBLACK_FRAGMENT_SHADERS}
)

list(APPEND OPENBLACK_SHADERS ${OPENBLACK_IMGUI_VS_SHADERS})
list(APPEND OPENBLACK_SHADERS ${OPENBLACK_IMGUI_FS_SHADERS})
list(APPEND OPENBLACK_SHADERS
     ${CMAKE_SOURCE_DIR}/assets/shaders/imgui/varying.def.sc
)
list(APPEND OPENBLACK_SHADERS ${OPENBLACK_VERTEX_SHADERS})
list(APPEND OPENBLACK_SHADERS ${OPENBLACK_FRAGMENT_SHADERS})
list(APPEND OPENBLACK_SHADERS ${OPENBLACK_VARYING_DEF_FILE})

if (ANDROID)
  add_library(openblack_lib SHARED)
  set_target_properties(
    openblack_lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
  )
else ()
  add_library(openblack_lib STATIC)
endif ()

target_sources(
  openblack_lib
  PRIVATE ${OPENBLACK_SOURCES}
          ${OPENBLACK_HEADERS}
          ${OPENBLACK_RC}
          ${OPENBLACK_SHADERS}
          ${OPENBLACK_VERTEX_SHADERS}
          ${OPENBLACK_FRAGMENT_SHADERS}
          ${OPENBLACK_VARYING_DEF_FILE}
)
set_target_properties(openblack_lib PROPERTIES LINKER_LANGUAGE CXX)

# The script API's natives that are still stubs, which the debug Scripts panel shows
set(OPENBLACK_UNIMPLEMENTED_NATIVES
    ${CMAKE_BINARY_DIR}/include/generated/scripting/UnimplementedNatives.h
)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/generated/scripting)
add_custom_command(
  OUTPUT ${OPENBLACK_UNIMPLEMENTED_NATIVES}
  COMMAND
    ${CMAKE_COMMAND} -DSOURCE=${CMAKE_CURRENT_LIST_DIR}/CHLApi.cpp
    -DOUTPUT=${OPENBLACK_UNIMPLEMENTED_NATIVES} -P
    ${CMAKE_SOURCE_DIR}/CMakeModules/GenerateUnimplementedNatives.cmake
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/CHLApi.cpp
          ${CMAKE_SOURCE_DIR}/CMakeModules/GenerateUnimplementedNatives.cmake
  COMMENT "Listing the script API's unwritten natives"
)
target_sources(openblack_lib PRIVATE ${OPENBLACK_UNIMPLEMENTED_NATIVES})
# openblack's own Bink video decoder (OPENBLACK_USE_BINK): the debug GUI's Video window
if (OPENBLACK_USE_BINK)
  target_compile_definitions(openblack_lib PUBLIC OPENBLACK_USE_BINK)
endif ()
# The audio decoders give the samples of the decoder they replaced only with single precision operations as written:
# no fused multiply-adds (MSVC's /fp:precise does not contract them)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/Audio/Codec/MpegAudio.cpp PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
endif ()

target_include_directories(
  openblack_lib
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}
         ${CMAKE_BINARY_DIR}/include # For shaders and other bin2c
         ${BULLET_INCLUDE_DIR} ${Stb_INCLUDE_DIR}
)
if (NOT VCPKG_TARGET_ANDROID)
  target_include_directories(
    openblack_lib PUBLIC ${BULLET_ROOT_DIR}/${BULLET_INCLUDE_DIR}
  )
endif ()
target_link_directories(
  openblack_lib PRIVATE ${BULLET_ROOT_DIR}/${BULLET_LIBRARY_DIRS}
)
target_compile_definitions(openblack_lib PRIVATE ${BULLET_DEFINITIONS})
target_link_libraries(
  openblack_lib
  PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:-SAFESEH:NO>"
          l3d
          pack
          lnd
          anm
          glw
          morph
          rawimage
          cam
          imgui::imgui
          imgui_user
          ScriptLibrary
          glm::glm
          EnTT::EnTT
          bgfx::bgfx
          OpenAL::OpenAL
          Bullet3Common
          BulletDynamics
          BulletCollision
          BulletSoftBody
          LinearMath
          minizip::minizip
  PUBLIC spdlog::spdlog
         # The mind file's types are part of the creature code's interfaces
         creaturemind
)
target_include_directories(openblack_lib PRIVATE ${CXXOPTS_INCLUDE_DIRS})

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    if (MSVC)
      # as for now, /EHsc has to be passed manually to clang-tidy
      # see https://gitlab.kitware.com/cmake/cmake/-/issues/20512
      set_target_properties(
        openblack_lib PROPERTIES CXX_CLANG_TIDY
                                 "${CLANG_TIDY};--extra-arg=/EHsc"
      )
    else ()
      set_target_properties(
        openblack_lib PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY}
      )
    endif ()
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (ANDROID)
  target_link_libraries(openblack_lib PRIVATE EGL)
elseif (UNIX)
  find_library(WAYLAND wayland-egl)
  if (WAYLAND)
    target_link_libraries(openblack_lib PRIVATE ${WAYLAND})
  endif ()
endif ()

if (VCPKG_TARGET_ANDROID)
  # openblack_lib need to forward the JNI exports from SDL2 so the Activity can start main
  target_link_libraries(
    openblack_lib PUBLIC -Wl,--whole-archive SDL2::SDL2 -Wl,--no-whole-archive
  )
else ()
  target_link_libraries(
    openblack_lib
    PUBLIC $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
           $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  )
endif ()

if (WIN32)
  # The crash handler's stack traces and minidumps
  target_link_libraries(openblack_lib PRIVATE dbghelp)
endif ()

target_compile_definitions(
  openblack_lib
  # glm's conventions are part of the library's interface: everything that includes its headers (the tests, the tools)
  # must expand glm's inline functions the same way, or two definitions of one function reach the linker
  PUBLIC
    "GLM_FORCE_LEFT_HANDED"
    "GLM_FORCE_RADIANS"
    "GLM_ENABLE_EXPERIMENTAL"
  PRIVATE
    # "IMGUI_DISABLE_OBSOLETE_FUNCTIONS"
    # Compile-time optimize out log messages (debug level if debug, else info)
    "SPDLOG_ACTIVE_LEVEL=$<IF:$<CONFIG:DEBUG>,SPDLOG_LEVEL_DEBUG,SPDLOG_LEVEL_INFO>"
    # Suppress WinMain() and main hijacking, provided by SDL
    "SDL_MAIN_HANDLED"
  PUBLIC "$<$<CONFIG:DEBUG>:OPENBLACK_DEBUG>"
)

if (MSVC)
  target_compile_definitions(
    openblack_lib
    PRIVATE
      # Get rid of useless crud from windows.h
      NOMINMAX
      WIN32_LEAN_AND_MEAN
      # disable CRT warnings on windows cause they're annoying as shit and we use C
      # functions everywhere
      _CRT_SECURE_NO_WARNINGS
  )

  target_compile_options(
    openblack_lib
    PRIVATE # Level 4 warnings
            /W4
            # Disable nonstandard extension used: nameless struct/union from glm
            /wd4201
            # Enables stricter C++ standards conformance checks
            /permissive-
            # Set correct version of c++
            /Zc:__cplusplus
            # Use utf-8 for std::print and fmt/spdlog
            /utf-8
  )

  if (OPENBLACK_WARNINGS_AS_ERRORS)
    target_compile_options(openblack_lib PRIVATE /WX)
  endif ()
else ()
  target_compile_options(
    openblack_lib
    PRIVATE -Wall
            -Wextra
            -pedantic
            -Wno-unused-private-field
            -Wno-unused-variable
            -Wno-unused-parameter
            # stb_image_write.h
            -Wno-missing-field-initializers
            -Wno-deprecated-declarations
  )

  if (OPENBLACK_TRACE_TIME AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(openblack_lib PRIVATE -ftime-trace)
  endif ()
  if (OPENBLACK_WARNINGS_AS_ERRORS)
    target_compile_options(openblack_lib PRIVATE -Werror)
  endif ()
endif ()

# Precompiled header: third-party and standard headers that most sources reach through Locator.h, ECS/Registry.h and
# the components. No project headers: they change often, and a change would rebuild every source. Off with clang-tidy,
# which cannot read an MSVC precompiled header
if (OPENBLACK_USE_PCH AND NOT OPENBLACK_CLANG_TIDY_CHECKS)
  target_precompile_headers(openblack_lib PRIVATE ${OPENBLACK_PCH_HEADERS})
  if (APPLE)
    set_source_files_properties(${OPENBLACK_OBJC_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
  endif ()
endif ()

set_property(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT openblack
)
source_group(
  TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${OPENBLACK_SOURCES}
                                       ${OPENBLACK_HEADERS}
)

foreach (SHADER ${OPENBLACK_SHADERS})
  source_group("Shaders" FILES "${SHADER}")
endforeach ()

if (ANDROID)
  add_library(openblack ALIAS openblack_lib)
else ()
  add_executable(openblack main.cpp)
  target_link_libraries(openblack PRIVATE openblack_lib)
  # built with the game and loaded at start-up when it is there; not linked
  add_subdirectory(ModLoader)
  add_dependencies(openblack ModLoader)
  if (MSVC)
    # Debug builds create a debug console
    set_target_properties(
      openblack PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
    )
    set_target_properties(
      openblack PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE"
    )
    target_compile_definitions(openblack PRIVATE $<$<CONFIG:DEBUG>:_CONSOLE>)

    # Release builds also have a console, for now..
    set_target_properties(
      openblack PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE"
    )
    set_target_properties(
      openblack PROPERTIES COMPILE_DEFINITIONS_RELEASE "CONSOLE"
    )
    set_target_properties(
      openblack PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE"
    )
  endif ()

  if (EMSCRIPTEN)
    # Must be run from project root using `emrun --no_browser --serve_after_close --serve_after_exit .`
    # Browser should point to http://localhost:6931/emscripten-vcpkg/bin/Debug/openblack.html?-g&relative/path/to/game/files
    set_target_properties(
      openblack
      PROPERTIES
        COMPILE_FLAGS_DEBUG "-g3 -gsource-map"
        LINK_FLAGS_DEBUG "-g3 -gsource-map"
        COMPILE_FLAGS "-s USE_SDL=2 -fexceptions"
        #COMPILE_FLAGS "-s USE_SDL=2 -s USE_PTHREADS=1"
        LINK_FLAGS
        "-s USE_SDL=2 -s FULL_ES3=1 -s GL_ASSERTIONS=1 -s USE_WEBGL2=1 -s TOTAL_MEMORY=128MB -fexceptions --emrun"
        #LINK_FLAGS "-s USE_SDL=2 -s FULL_ES3=1 -s GL_ASSERTIONS=1 -s USE_WEBGL2=1 -s USE_PTHREADS=1 -s TOTAL_MEMORY=128MB --emrun"
        SUFFIX ".html"
    )
  elseif (APPLE)
    set(OPENBLACK_ICON_SOURCE_DIR ${CMAKE_SOURCE_DIR}/assets/resource/icon)
    set(OPENBLACK_ICON_BINARY_DIR ${CMAKE_BINARY_DIR}/assets/resource/icon)
    set(OPENBLACK_ICON_SOURCE
        ${OPENBLACK_ICON_SOURCE_DIR}/openblack-400x400.png
    )
    set(OPENBLACK_ICONSET_BINARY_DIR
        ${OPENBLACK_ICON_BINARY_DIR}/openblack.iconset
    )
    set(OPENBLACK_ICONSET_BINARY ${OPENBLACK_ICON_BINARY_DIR}/openblack.icns)
    add_custom_command(
      OUTPUT ${OPENBLACK_ICONSET_BINARY}
      COMMENT "Generate MacOSX bundle icon set"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${OPENBLACK_ICONSET_BINARY_DIR}
      COMMAND sips -z 16 16 ${OPENBLACK_ICON_SOURCE_DIR}/openblack-256x256.png
              --out ${OPENBLACK_ICONSET_BINARY_DIR}/icon_16x16.png
      COMMAND sips -z 32 32 ${OPENBLACK_ICON_SOURCE_DIR}/openblack-256x256.png
              --out ${OPENBLACK_ICONSET_BINARY_DIR}/icon_32x32.png
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${OPENBLACK_ICON_SOURCE_DIR}/openblack-64x64.png
        ${OPENBLACK_ICONSET_BINARY_DIR}/icon_64x64.png
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${OPENBLACK_ICON_SOURCE_DIR}/openblack-128x128.png
        ${OPENBLACK_ICONSET_BINARY_DIR}/icon_128x128.png
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${OPENBLACK_ICON_SOURCE_DIR}/openblack-256x256.png
        ${OPENBLACK_ICONSET_BINARY_DIR}/icon_256x256.png
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${OPENBLACK_ICON_SOURCE_DIR}/openblack-400x400.png
        ${OPENBLACK_ICONSET_BINARY_DIR}/icon_400x400.png
      COMMAND iconutil -c icns ${OPENBLACK_ICONSET_BINARY_DIR} -o
              ${OPENBLACK_ICONSET_BINARY}
    )
    set_source_files_properties(
      ${OPENBLACK_ICONSET_BINARY} PROPERTIES MACOSX_PACKAGE_LOCATION
                                             "Resources"
    )
    target_sources(openblack PRIVATE ${OPENBLACK_ICONSET_BINARY})
    set_target_properties(
      openblack
      PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER org.openblack.openblack
                 MACOSX_BUNDLE_ICON_FILE openblack.icns
                 MACOSX_BUNDLE_BUNDLE_VERSION ${openblack_VERSION}
                 MACOSX_BUNDLE_SHORT_VERSION_STRING ${openblack_VERSION}
    )
  endif ()

  # Installation configuration
  install(
    TARGETS openblack
    BUNDLE DESTINATION bundle
    RUNTIME DESTINATION bin
  )
  if (UNIX)
    install(
      FILES
        "${CMAKE_SOURCE_DIR}/assets/resource/shortcut/org.openblack.openblack.desktop"
      DESTINATION "share/applications"
    )
    foreach (RES 64 128 256)
      install(
        FILES
          "${CMAKE_SOURCE_DIR}/assets/resource/icon/openblack-${RES}x${RES}.png"
        DESTINATION "share/icons/hicolor/${RES}x${RES}/apps"
        RENAME "openblack.png"
      )
    endforeach ()
  endif ()

endif ()
