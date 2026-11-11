file(GLOB SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/landgen/*.h")

add_library(landgen STATIC ${SOURCES} ${HEADERS})

target_include_directories(
  landgen PUBLIC $<INSTALL_INTERFACE:include>
                 $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(landgen PUBLIC lnd)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(landgen PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

# The same seed and settings must give the same land on every platform. The generator uses only additions,
# multiplications, divisions and square roots, which IEEE 754 rounds the same everywhere. The compiler must not fuse a
# multiplication and an addition into one instruction, because a fused result rounds differently. MSVC does not fuse
# them with /fp:precise.
if (MSVC)
  target_compile_definitions(landgen PRIVATE _HAS_EXCEPTIONS=0)
  target_compile_options(landgen PRIVATE /W4 /WX /EHs-c- /fp:precise)
else ()
  target_compile_options(
    landgen PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
                    -ffp-contract=off
  )
endif ()

set_property(TARGET landgen PROPERTY FOLDER "components")
