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
  target_compile_definitions(inspector PRIVATE _HAS_EXCEPTIONS=0)
  target_compile_options(inspector PRIVATE /W4 /WX /EHs-c-)
else ()
  target_compile_options(
    inspector PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
  )
endif ()

set_property(TARGET inspector PROPERTY FOLDER "components")
