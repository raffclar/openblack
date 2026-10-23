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
# The code that connects the game to a browser page is built only for the web
if (NOT EMSCRIPTEN)
  list(FILTER OPENBLACK_SOURCES EXCLUDE REGEX "/Web/")
  list(FILTER OPENBLACK_HEADERS EXCLUDE REGEX "/Web/")
endif ()
# The HTTPS client and the token stores of the desktop. The web build uses the fetch of the browser and its cookies.
if (EMSCRIPTEN)
  list(FILTER OPENBLACK_SOURCES EXCLUDE REGEX "/Online/Desktop/")
  list(FILTER OPENBLACK_HEADERS EXCLUDE REGEX "/Online/Desktop/")
endif ()
# The sockets, the TLS and the signatures of the game server. The game server is not in the web or Android builds.
if (EMSCRIPTEN OR ANDROID)
  list(FILTER OPENBLACK_SOURCES EXCLUDE REGEX "/Server/Desktop/")
  list(FILTER OPENBLACK_HEADERS EXCLUDE REGEX "/Server/Desktop/")
endif ()
if (APPLE)
  file(GLOB_RECURSE OPENBLACK_OBJC_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.mm")
  list(APPEND OPENBLACK_SOURCES "${OPENBLACK_OBJC_SOURCES}")
endif ()

# The systems are a static library of their own. The Debug objects of all sources together are bigger than the 4 GB that
# the librarian can hold in one library file, as every object has its own copy of the debug information.
set(OPENBLACK_ALL_SOURCES ${OPENBLACK_SOURCES})
set(OPENBLACK_SYSTEM_SOURCES ${OPENBLACK_SOURCES})
list(FILTER OPENBLACK_SYSTEM_SOURCES INCLUDE REGEX "/ECS/Systems/")
if (NOT ANDROID)
  list(REMOVE_ITEM OPENBLACK_SOURCES ${OPENBLACK_SYSTEM_SOURCES})
endif ()

# The simulation library, openblack_sim, has the world that the game and the dedicated server (apps/openblack-server)
# share: the ECS and its components, the turn, the script machine, the physics, the creatures' minds, the land's data and
# the save. The systems library is the systems part of it. The renderer, the GUI, the audio, the input, the windows and
# the videos stay in the game's library, openblack_lib. Some files of the simulation still use the game's library, so the
# two libraries use each other's symbols for now.
set(OPENBLACK_SIM_SOURCES ${OPENBLACK_SOURCES})
list(
  FILTER
  OPENBLACK_SIM_SOURCES
  INCLUDE
  REGEX
  "/src/(Animals|Blast|Commands|Common|Creature|ECS|FileSystem|Fire|Hand|LHScriptX|Magic|Nature|Parsers|Physics|Scenery|ScriptHeaders|Serializer|Simulation|Story)/|/src/(CHLApi|InfoConstants|Level|Profiler)[.]cpp$"
)
if (NOT ANDROID)
  list(REMOVE_ITEM OPENBLACK_SOURCES ${OPENBLACK_SIM_SOURCES})
endif ()

set(OPENBLACK_RC ${CMAKE_SOURCE_DIR}/assets/resource/icon/openblack.rc)

file(GLOB OPENBLACK_IMGUI_VS_SHADERS
     "${CMAKE_SOURCE_DIR}/assets/shaders/imgui/vs_*.sc"
)
file(GLOB OPENBLACK_IMGUI_FS_SHADERS
     "${CMAKE_SOURCE_DIR}/assets/shaders/imgui/fs_*.sc"
)
include(ShaderCompile)

openblack_compile_shaders(
  TYPE VERTEX
  SHADERS ${OPENBLACK_IMGUI_VS_SHADERS}
  VARYING_DEF ${CMAKE_SOURCE_DIR}/assets/shaders/imgui/varying.def.sc
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders/imgui
  OUT_FILES_VAR OPENBLACK_IMGUI_VS_SHADER_HEADERS
)
openblack_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${OPENBLACK_IMGUI_FS_SHADERS}
  VARYING_DEF ${CMAKE_SOURCE_DIR}/assets/shaders/imgui/varying.def.sc
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders/imgui
  OUT_FILES_VAR OPENBLACK_IMGUI_FS_SHADER_HEADERS
)

file(GLOB OPENBLACK_VERTEX_SHADERS "${CMAKE_SOURCE_DIR}/assets/shaders/vs_*.sc")
file(GLOB OPENBLACK_FRAGMENT_SHADERS
     "${CMAKE_SOURCE_DIR}/assets/shaders/fs_*.sc"
)
file(GLOB OPENBLACK_VARYING_DEF_FILE
     "${CMAKE_SOURCE_DIR}/assets/shaders/varying.def.sc"
)

openblack_compile_shaders(
  TYPE VERTEX
  SHADERS ${OPENBLACK_VERTEX_SHADERS}
  VARYING_DEF ${OPENBLACK_VARYING_DEF_FILE}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  OUT_FILES_VAR OPENBLACK_VERTEX_SHADER_HEADERS
)
openblack_compile_shaders(
  TYPE FRAGMENT
  SHADERS ${OPENBLACK_FRAGMENT_SHADERS}
  VARYING_DEF ${OPENBLACK_VARYING_DEF_FILE}
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
  OUT_FILES_VAR OPENBLACK_FRAGMENT_SHADER_HEADERS
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
          # Compiled shaders, built before the sources that embed them
          ${OPENBLACK_IMGUI_VS_SHADER_HEADERS}
          ${OPENBLACK_IMGUI_FS_SHADER_HEADERS}
          ${OPENBLACK_VERTEX_SHADER_HEADERS}
          ${OPENBLACK_FRAGMENT_SHADER_HEADERS}
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

# The version of the build: the project's semantic version and the git commit
# of the sources. The script runs at each build and changes the file only when
# the commit changes.
set(OPENBLACK_BUILD_VERSION_SOURCE
    ${CMAKE_BINARY_DIR}/include/generated/BuildVersion.cpp
)
set(OPENBLACK_BUILD_VERSION_COMMAND
    ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DOUTPUT=${OPENBLACK_BUILD_VERSION_SOURCE} -DVERSION=${PROJECT_VERSION} -P
    ${CMAKE_SOURCE_DIR}/CMakeModules/WriteBuildVersion.cmake
)
execute_process(COMMAND ${OPENBLACK_BUILD_VERSION_COMMAND})
add_custom_target(
  openblack_build_version
  COMMAND ${OPENBLACK_BUILD_VERSION_COMMAND}
  BYPRODUCTS ${OPENBLACK_BUILD_VERSION_SOURCE}
  COMMENT "Writing the build version"
  VERBATIM
)
target_sources(openblack_lib PRIVATE ${OPENBLACK_BUILD_VERSION_SOURCE})
add_dependencies(openblack_lib openblack_build_version)

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
          hnd
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
          helpstats
          creaturehtml
          lhp
          onlinemap
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
         # The JSON shapes of the lobby's API are part of the lobby client
         lobbyapi
         # The mind file's types are part of the creature mind component
         creaturemind
         # The saved game's files are part of the saved game component
         gamesave
)
target_include_directories(openblack_lib PRIVATE ${CXXOPTS_INCLUDE_DIRS})
if (EMSCRIPTEN)
  # The user folder of the web build is a back end of the WebAssembly file system of Emscripten. It needs the headers of
  # that file system, which are not in the public headers of Emscripten.
  target_include_directories(
    openblack_lib PRIVATE "${EMSCRIPTEN_ROOT_PATH}/system/lib/wasmfs"
  )
endif ()

if (NOT EMSCRIPTEN)
  # The HTTPS client of the desktop: libcurl with OpenSSL, for TLS 1.3 and its cipher suites on every platform
  find_package(CURL CONFIG REQUIRED)
  target_link_libraries(openblack_lib PRIVATE CURL::libcurl)
  if (NOT ANDROID)
    # The game server's WebSocket listener with TLS, and the Ed25519 check of the lobby's join tickets
    find_package(OpenSSL REQUIRED)
    target_link_libraries(openblack_lib PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    if (WIN32)
      target_link_libraries(openblack_lib PRIVATE ws2_32)
    endif ()
  endif ()
  if (WIN32)
    # The data protection of Windows encrypts the refresh token of the lobby
    target_link_libraries(openblack_lib PRIVATE crypt32)
  elseif (APPLE)
    # The keychain keeps the refresh token of the lobby
    target_link_libraries(
      openblack_lib PRIVATE "-framework Security" "-framework CoreFoundation"
    )
  elseif (UNIX AND NOT ANDROID)
    # The secret service keeps the refresh token of the lobby, when libsecret is on the system
    find_package(PkgConfig QUIET)
    if (PKG_CONFIG_FOUND)
      pkg_check_modules(LIBSECRET QUIET IMPORTED_TARGET libsecret-1)
    endif ()
    if (LIBSECRET_FOUND)
      target_link_libraries(openblack_lib PRIVATE PkgConfig::LIBSECRET)
      target_compile_definitions(
        openblack_lib PRIVATE OPENBLACK_HAVE_LIBSECRET
      )
    else ()
      message(
        STATUS
          "libsecret not found: the lobby session is not kept between runs"
      )
    endif ()
  endif ()
endif ()

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
  target_link_libraries(openblack_lib PUBLIC inspector)
  target_compile_definitions(openblack_lib PUBLIC OPENBLACK_INSPECTOR)
  # The worktree the game is built from, which the inspector names it by wherever the build lives
  target_compile_definitions(
    openblack_lib PRIVATE OPENBLACK_BUILT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )
  # The running game tells tools how it was built
  target_compile_definitions(
    openblack_lib PRIVATE OPENBLACK_BUILD_TYPE="$<CONFIG>"
  )
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

# Each float operation of the game's logic must round one time, as the game's FPU did. Fused multiply-add rounds two
# operations one time, so these options stop it in the game, its library and the tests (see Common/X87.h).
target_compile_options(
  openblack_lib
  PUBLIC "$<IF:$<CXX_COMPILER_ID:MSVC>,/fp:precise,-ffp-contract=off>"
)

# The systems library is built with the same options as the game's library: it takes each build property of that
# library, so a property added above reaches both. The two libraries use each other's symbols, which static libraries
# may do.
if (NOT ANDROID)
  add_library(openblack_systems STATIC ${OPENBLACK_SYSTEM_SOURCES})
  foreach (
    property
    COMPILE_DEFINITIONS
    COMPILE_OPTIONS
    INCLUDE_DIRECTORIES
    LINK_LIBRARIES
    LINKER_LANGUAGE
    CXX_CLANG_TIDY
  )
    get_target_property(value openblack_lib ${property})
    if (value)
      set_property(TARGET openblack_systems PROPERTY ${property} "${value}")
    endif ()
  endforeach ()
  # The simulation library takes the build properties of the game's library in the same way
  add_library(openblack_sim STATIC ${OPENBLACK_SIM_SOURCES})
  foreach (
    property
    COMPILE_DEFINITIONS
    COMPILE_OPTIONS
    INCLUDE_DIRECTORIES
    LINK_LIBRARIES
    LINKER_LANGUAGE
    CXX_CLANG_TIDY
  )
    get_target_property(value openblack_lib ${property})
    if (value)
      set_property(TARGET openblack_sim PROPERTY ${property} "${value}")
    endif ()
  endforeach ()
  # The headers of the simulation are found from the folder of the sources, as the game's are
  target_include_directories(
    openblack_sim PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_BINARY_DIR}/include
  )
  target_link_libraries(openblack_lib PUBLIC openblack_systems openblack_sim)
  target_link_libraries(openblack_systems PUBLIC openblack_lib)
  target_link_libraries(openblack_sim PUBLIC openblack_systems openblack_lib)
endif ()

set_property(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT openblack
)
source_group(
  TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${OPENBLACK_ALL_SOURCES}
                                       ${OPENBLACK_HEADERS}
)

foreach (SHADER ${OPENBLACK_SHADERS})
  source_group("Shaders" FILES "${SHADER}")
endforeach ()

if (ANDROID)
  add_library(openblack ALIAS openblack_lib)
else ()
  add_executable(openblack main.cpp)
  # The game is the simulation library and the game's own library
  target_link_libraries(openblack PRIVATE openblack_sim openblack_lib)

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
    # The web build. The game runs on a worker thread of its own (PROXY_TO_PTHREAD), so its loops that wait, its threads
    # and its file reads that wait work as on the desktop. The main thread of the page only gives the input to the game
    # and shows the canvas. The game draws into the canvas with WebGPU, or with WebGL 2 when the browser has no WebGPU.
    #
    # JSPI lets the game thread wait for the browser in the middle of its code: WebGPU gives the adapter, the device and
    # the texture reads only when the thread gives control back to the browser. The game thread also waits for the
    # browser after each frame (see src/Web/WebPlatform.cpp).
    #
    # The page makes the workers for the game threads at the start (PTHREAD_POOL_SIZE). When the game needs one thread
    # more than this set, the page makes a new worker, and the new worker loads the program again. The game uses at most
    # 25 threads at one time: the game thread, the render thread, the worker pools, the load queue and the shadow bake.
    #
    # The only part of the build that sends data over the network is the lobby client. It uses the fetch of the browser
    # (FETCH) for paths on the origin of the page only. There is no emrun and no file that the page loads before the
    # game starts. The page gives the user's game folder from the disk (see web/page).
    target_link_options(
      openblack
      PRIVATE
      -pthread
      -fwasm-exceptions
      -sWASMFS=1
      -sPROXY_TO_PTHREAD=1
      -sJSPI=1
      -sPTHREAD_POOL_SIZE=25
      -sOFFSCREENCANVAS_SUPPORT=1
      -sMIN_WEBGL_VERSION=2
      -sMAX_WEBGL_VERSION=2
      -sFULL_ES3=1
      -sINITIAL_MEMORY=1024MB
      -sALLOW_MEMORY_GROWTH=1
      -sMAXIMUM_MEMORY=4GB
      -sSTACK_SIZE=8MB
      -sDEFAULT_PTHREAD_STACK_SIZE=2MB
      -sEXIT_RUNTIME=1
      -sENVIRONMENT=web,worker
      -sFETCH=1
      "--pre-js=${CMAKE_SOURCE_DIR}/web/page/pre.js"
      "--post-js=${CMAKE_SOURCE_DIR}/web/page/post.js"
      "--post-js=${CMAKE_SOURCE_DIR}/web/page/input.js"
    )
    set_target_properties(openblack PROPERTIES SUFFIX ".js")
    set_property(
      TARGET openblack
      APPEND
      PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/page/pre.js"
               "${CMAKE_SOURCE_DIR}/web/page/post.js"
               "${CMAKE_SOURCE_DIR}/web/page/input.js"
    )
    # The page files go next to the game
    file(
      GLOB
      OPENBLACK_WEB_PAGE_FILES
      "${CMAKE_SOURCE_DIR}/web/page/*.html"
      "${CMAKE_SOURCE_DIR}/web/page/*.css"
      "${CMAKE_SOURCE_DIR}/web/page/launcher*.js"
      "${CMAKE_SOURCE_DIR}/web/page/*-worker.js"
    )
    add_custom_command(
      TARGET openblack
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENBLACK_WEB_PAGE_FILES}
              $<TARGET_FILE_DIR:openblack>
      COMMENT "Copy the web page of the game"
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
