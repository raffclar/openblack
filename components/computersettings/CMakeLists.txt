file(GLOB SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")

add_library(computersettings STATIC ${SOURCES} ${HEADERS})

target_include_directories(
  computersettings
  PUBLIC $<INSTALL_INTERFACE:include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(computersettings PRIVATE nlohmann_json)
# The JSON library must not throw, as for the inspector, which uses the same library in the same program
target_compile_definitions(computersettings PUBLIC JSON_NOEXCEPTION)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(
      computersettings PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY}
    )
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  # The standard library keeps its exception support, as for the inspector: the layout of the standard exceptions that
  # the JSON library makes must be the same as in the programs that link it
  target_compile_options(computersettings PRIVATE /W4 /WX)
else ()
  target_compile_options(
    computersettings PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
  )
endif ()

set_property(TARGET computersettings PROPERTY FOLDER "components")
