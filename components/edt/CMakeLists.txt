file(GLOB SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")

add_library(edt STATIC ${SOURCES} ${HEADERS})
target_link_libraries(edt PRIVATE pack)

target_include_directories(
  edt PUBLIC $<INSTALL_INTERFACE:include>
             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(edt PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_options(edt PRIVATE /W4 /WX)
else ()
  target_compile_options(edt PRIVATE -Wall -Wextra -pedantic -Werror)
endif ()

set_property(TARGET edt PROPERTY FOLDER "components")
