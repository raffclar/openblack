set(CREATUREUPLOAD main.cpp CreatureUploadApp.h Win32Dialog.cpp Win32Dialog.h
                   LookPictures.cpp resource.h
)
if (WIN32)
  list(APPEND CREATUREUPLOAD creatureupload.rc)
endif ()

source_group(apps\\creatureupload FILES ${CREATUREUPLOAD})

add_executable(creatureupload ${CREATUREUPLOAD})

# The tool uses the creature files, the profiles and the HTTPS client of the game's library. It keeps exceptions on, as
# the library uses them.
target_link_libraries(creatureupload PRIVATE openblack_lib)
target_include_directories(creatureupload PRIVATE ${CXXOPTS_INCLUDE_DIRS})

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    if (MSVC)
      set_target_properties(
        creatureupload PROPERTIES CXX_CLANG_TIDY
                                  "${CLANG_TIDY};--extra-arg=/EHsc"
      )
    else ()
      set_target_properties(
        creatureupload PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY}
      )
    endif ()
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_definitions(
    creatureupload PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN SDL_MAIN_HANDLED
  )
  target_compile_options(
    creatureupload PRIVATE /W4 /WX /EHsc /permissive- /Zc:__cplusplus /utf-8
  )
  # The tool is a window as the game's tool was. With --console it writes to the console that started it.
  target_link_libraries(creatureupload PRIVATE comctl32)
  set_target_properties(
    creatureupload PROPERTIES LINK_FLAGS
                              "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
  )
elseif (WIN32)
  target_link_libraries(creatureupload PRIVATE comctl32)
else ()
  target_compile_options(creatureupload PRIVATE -Wall -Wextra -pedantic -Werror)
endif ()

set_property(TARGET creatureupload PROPERTY FOLDER "apps")
