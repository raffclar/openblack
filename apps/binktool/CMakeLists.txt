set(BINKTOOL binktool.cpp)

source_group(apps\\binktool FILES ${BINKTOOL})

add_executable(binktool ${BINKTOOL})

target_link_libraries(binktool PRIVATE bink)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(binktool PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_definitions(binktool PRIVATE _HAS_EXCEPTIONS=0)
  target_compile_options(binktool PRIVATE /W4 /WX /EHs- /EHc-)
else ()
  target_compile_options(
    binktool PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
  )
endif ()

set_property(TARGET binktool PROPERTY FOLDER "tools")
