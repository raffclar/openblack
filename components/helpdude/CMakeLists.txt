file(GLOB SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")

add_library(helpdude STATIC ${SOURCES} ${HEADERS})

target_include_directories(
  helpdude PUBLIC $<INSTALL_INTERFACE:include>
                  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(helpdude PUBLIC morph PRIVATE pack)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(helpdude PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_definitions(helpdude PRIVATE _HAS_EXCEPTIONS=0)
  target_compile_options(helpdude PRIVATE /W4 /WX /EHs-c-)
else ()
  target_compile_options(
    helpdude PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
  )
endif ()

set_property(TARGET helpdude PROPERTY FOLDER "components")
