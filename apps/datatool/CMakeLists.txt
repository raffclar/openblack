# The text that datatool prints, kept apart from its main so that a test can reach it
add_library(datareport STATIC DataReport.cpp DataReport.h)
source_group(apps\\datatool FILES DataReport.cpp DataReport.h)
target_include_directories(datareport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  datareport
  PUBLIC openblack_sim
         gestures
         psys
         rawimage
         sas
         dance
         gamesave
         pack
)
set_property(TARGET datareport PROPERTY FOLDER "tools")

set(DATATOOL datatool.cpp)

source_group(apps\\datatool FILES ${DATATOOL})

add_executable(datatool ${DATATOOL})

target_link_libraries(datatool PRIVATE datareport)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(datatool PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_options(datatool PRIVATE /W4 /WX)
  target_compile_options(datareport PRIVATE /W4 /WX)
else ()
  target_compile_options(datatool PRIVATE -Wall -Wextra -pedantic -Werror)
  target_compile_options(datareport PRIVATE -Wall -Wextra -pedantic -Werror)
endif ()

set_property(TARGET datatool PROPERTY FOLDER "tools")
