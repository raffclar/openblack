file(GLOB_RECURSE OPENBLACK_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
if (NOT ANDROID)
  list(REMOVE_ITEM OPENBLACK_SOURCES "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
endif ()
file(GLOB_RECURSE OPENBLACK_HEADERS "${CMAKE_CURRENT_LIST_DIR}/*.h")
# The debug inspector's game side is built only with the option
if (NOT OPENBLACK_INSPECTOR)
  list(FILTER OPENBLACK_SOURCES EXCLUDE REGEX
       "/Inspector/|/InspectorSystem[.]cpp$"
  )
  list(FILTER OPENBLACK_HEADERS EXCLUDE REGEX
       "/Inspector/|/InspectorSystem[.]h$"
  )
endif ()
if (APPLE)
  file(GLOB_RECURSE OPENBLACK_OBJC_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.mm")
  list(APPEND OPENBLACK_SOURCES "${OPENBLACK_OBJC_SOURCES}")
endif ()

set(OPENBLACK_RC ${CMAKE_SOURCE_DIR}/assets/resource/icon/openblack.rc)

file(GLOB OPENBLACK_IMGUI_VS_SHADERS
     "${CMAKE_SOURCE_DIR}/assets/shaders/imgui/vs_*.sc"
)
file(GLOB OPENBLACK_IMGUI_FS_SHADERS
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

file(GLOB OPENBLACK_VERTEX_SHADERS "${CMAKE_SOURCE_DIR}/assets/shaders/vs_*.sc")
file(GLOB OPENBLACK_FRAGMENT_SHADERS
     "${CMAKE_SOURCE_DIR}/assets/shaders/fs_*.sc"
)
file(GLOB OPENBLACK_VARYING_DEF_FILE
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

# The script API's natives that are still stubs, which the editor's Scripts panel shows
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

target_include_directories(
  openblack_lib
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}
         ${CMAKE_BINARY_DIR}/include # For shaders and other bin2c
         ${Stb_INCLUDE_DIR} ${DRLIBS_INCLUDE_DIRS}
)
# The game's videos: the container is always read, so that a build without the decoder keeps their timing
target_link_libraries(openblack_lib PUBLIC bink)
# Videos decode their frames on a thread of their own
find_package(Threads REQUIRED)
target_link_libraries(openblack_lib PUBLIC Threads::Threads)
if (OPENBLACK_BINK_DECODER)
  target_compile_definitions(openblack_lib PUBLIC OPENBLACK_BINK_DECODER)
endif ()
target_link_libraries(
  openblack_lib
  PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:-SAFESEH:NO>"
          l3d
          cam
          edt
          exc
          dance
          pack
          lnd
          anm
          glw
          morph
          helpdude
          psys
          physconst
          sas
          gestures
          rawimage
          imgui::imgui
          imgui_user
          ScriptLibrary
          lhvmdecompiler
          lhvmcompiler
          glm::glm
          EnTT::EnTT
          bgfx::bgfx
          OpenAL::OpenAL
          minizip::minizip
  PUBLIC spdlog::spdlog
         # The mind file's types are part of the creature mind component
         creaturemind
)
target_include_directories(openblack_lib PRIVATE ${CXXOPTS_INCLUDE_DIRS})

if (WIN32)
  # The crash handler's stack traces and minidumps
  target_link_libraries(openblack_lib PRIVATE dbghelp)
  # The shell's file dialogs
  target_link_libraries(openblack_lib PRIVATE ole32 shell32 uuid)
endif ()

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

if (OPENBLACK_INSPECTOR)
  target_link_libraries(openblack_lib PRIVATE inspector)
  target_compile_definitions(openblack_lib PUBLIC OPENBLACK_INSPECTOR)
  # The worktree the game is built from, which the inspector names it by wherever the build lives
  target_compile_definitions(
    openblack_lib PRIVATE OPENBLACK_BUILT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )
  # The running game tells tools how it was built
  target_compile_definitions(
    openblack_lib PRIVATE OPENBLACK_BUILD_TYPE="$<CONFIG>"
  )
  # Every component's reflection in one file outgrows an object file's sections
  if (MSVC)
    set_source_files_properties(
      Inspector/ComponentFields.cpp PROPERTIES COMPILE_OPTIONS /bigobj
    )
  endif ()
endif ()

# The library's headers do their maths left-handed in radians, so everything that includes them must too: glm's inline
# functions built either way in one program would leave the linker to pick one at random
target_compile_definitions(
  openblack_lib
  PUBLIC "GLM_FORCE_LEFT_HANDED" "GLM_FORCE_RADIANS"
  PRIVATE
    "GLM_ENABLE_EXPERIMENTAL"
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
    PRIVATE
      # Level 4 warnings
      /W4
      # Disable nonstandard extension used: nameless struct/union from glm
      /wd4201
      # Multi-threaded
      /MP
      # Enables stricter C++ standards conformance checks
      /permissive-
      # Set correct version of c++
      /Zc:__cplusplus
      # Use utf-8 for std::print and fmt/spdlog
      /utf-8
      # The service locator makes every system and resource cache, more sections than an object file holds
      /bigobj
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
