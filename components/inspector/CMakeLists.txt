file(GLOB SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")

add_library(inspector STATIC ${SOURCES} ${HEADERS})

target_include_directories(
  inspector PUBLIC $<INSTALL_INTERFACE:include>
                   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(inspector PUBLIC nlohmann_json)
# Built without exceptions: the JSON library must not throw for its users either
target_compile_definitions(inspector PUBLIC JSON_NOEXCEPTION)
if (WIN32)
  target_link_libraries(inspector PRIVATE ws2_32)
endif ()

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(inspector PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  # The standard library keeps its exception support as the game and tests have it: with it off here, the standard
  # exceptions the JSON library makes (and never throws) when text doesn't parse are laid out differently from the
  # same classes in the programs linking it, and freeing them corrupts the heap
  target_compile_options(inspector PRIVATE /W4 /WX)
else ()
  target_compile_options(
    inspector PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
  )
endif ()

set_property(TARGET inspector PROPERTY FOLDER "components")
