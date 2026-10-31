set(OPENBLACK_SERVER main.cpp)

source_group(apps\\openblack-server FILES ${OPENBLACK_SERVER})

add_executable(openblack-server ${OPENBLACK_SERVER})

# The server runs the world of the simulation library, with the devices that use no hardware. Unlike the other tools, it
# keeps exceptions on, as the library uses them.
target_link_libraries(openblack-server PRIVATE openblack_headless openblack_sim)
target_include_directories(openblack-server PRIVATE ${CXXOPTS_INCLUDE_DIRS})

# The server links neither the graphics library, the window system (SDL), the debug windows (Dear ImGui), the audio
# device (OpenAL) nor the game's library, which has the renderer. This check reads every library that the server links, also through other libraries,
# and stops the configure step if one of them is there. A library in a generator expression counts too.
function(openblack_server_link_closure target out out_entries)
  set(seen "")
  set(entries "")
  set(queue ${target})
  while (queue)
    list(POP_FRONT queue current)
    if (NOT TARGET ${current} OR current IN_LIST seen)
      continue()
    endif ()
    list(APPEND seen ${current})
    get_target_property(aliased ${current} ALIASED_TARGET)
    if (aliased)
      list(APPEND queue ${aliased})
    endif ()
    foreach (property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
      get_target_property(libraries ${current} ${property})
      if (NOT libraries)
        continue()
      endif ()
      foreach (library ${libraries})
        list(APPEND entries "${library}")
        string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" library
                             "${library}"
        )
        if (TARGET ${library})
          list(APPEND queue ${library})
        endif ()
      endforeach ()
    endforeach ()
  endwhile ()
  set(${out}
      ${seen}
      PARENT_SCOPE
  )
  set(${out_entries}
      ${entries}
      PARENT_SCOPE
  )
endfunction()
openblack_server_link_closure(
  openblack-server OPENBLACK_SERVER_LINKS OPENBLACK_SERVER_LINK_ENTRIES
)
foreach (
  forbidden
  bgfx::bgfx
  bgfx::bx
  bgfx::bimg
  openblack_lib
  SDL2::SDL2
  SDL2::SDL2-static
  SDL2::SDL2main
  imgui::imgui
  imgui_user
  OpenAL::OpenAL
)
  set(found FALSE)
  if (forbidden IN_LIST OPENBLACK_SERVER_LINKS)
    set(found TRUE)
  endif ()
  foreach (entry ${OPENBLACK_SERVER_LINK_ENTRIES})
    string(FIND "${entry}" "${forbidden}" position)
    if (NOT position EQUAL -1)
      set(found TRUE)
    endif ()
  endforeach ()
  if (found)
    message(FATAL_ERROR "openblack-server must not link ${forbidden}")
  endif ()
endforeach ()

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
