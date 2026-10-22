set(OPENBLACK_SERVER main.cpp)

source_group(apps\\openblack-server FILES ${OPENBLACK_SERVER})

add_executable(openblack-server ${OPENBLACK_SERVER})

# The server runs the world of the simulation library. Unlike the other tools, it keeps exceptions on, as the library
# uses them.
target_link_libraries(openblack-server PRIVATE openblack_sim)
target_include_directories(openblack-server PRIVATE ${CXXOPTS_INCLUDE_DIRS})

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    if (MSVC)
      set_target_properties(
        openblack-server PROPERTIES CXX_CLANG_TIDY
                                    "${CLANG_TIDY};--extra-arg=/EHsc"
      )
    else ()
      set_target_properties(
        openblack-server PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY}
      )
    endif ()
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_definitions(
    openblack-server PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN SDL_MAIN_HANDLED
  )
  target_compile_options(
    openblack-server PRIVATE /W4 /WX /EHsc /permissive- /Zc:__cplusplus /utf-8
  )
  # The server writes its log to the console
  set_target_properties(openblack-server PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
else ()
  target_compile_options(
    openblack-server PRIVATE -Wall -Wextra -pedantic -Werror
  )
endif ()

set_property(TARGET openblack-server PROPERTY FOLDER "apps")
