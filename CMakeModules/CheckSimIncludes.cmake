# Stops with an error when a file of the simulation includes a header of the client or of the devices with no hardware
# (openblack_headless). src/CMakeLists.txt writes the patterns of the libraries into the file CONFIG. The check runs at
# each configure step (include) and in ctest (cmake -DCONFIG=<file> -P CheckSimIncludes.cmake).
#
# The check reads each file of src, also the files that this platform does not build. It gives each file a kind:
# - headless: a file of openblack_headless
# - client: a file of the client folders that is not in the simulation's list, or a client file in a folder of the
#   simulation, or the game's main.cpp
# - sim: all the other files
# A sim file may include only sim files. The includes are found next to the file first, then from src.

if (CMAKE_SCRIPT_MODE_FILE)
  cmake_minimum_required(VERSION 3.20)
endif ()

function(openblack_check_sim_includes config)
  include(${config})
  # Files that other work changes now: each pair is "<file>><header>", with the reason
  set(allowed
      # The particle systems play their sounds through the audio manager. The work on the particle systems moves these
      # sounds to the sound requests. The rules of the particles are in the simulation, the particle system draws.
      "ECS/Systems/Implementations/ParticleSystem.cpp>Audio/AudioManagerInterface.h"
      "ECS/Systems/Implementations/ParticleRuleSystem.cpp>Audio/AudioManagerInterface.h"
  )

  file(GLOB_RECURSE files RELATIVE ${SOURCE_DIR} "${SOURCE_DIR}/*.h"
       "${SOURCE_DIR}/*.cpp" "${SOURCE_DIR}/*.mm"
  )
  set(sims "")
  foreach (file IN LISTS files)
    string(REGEX REPLACE "[.](h|cpp|mm)$" "" stem "${file}")
    set(kind sim)
    if ("${stem}" MATCHES "^(${HEADLESS})$")
      set(kind headless)
    elseif (
      ("/src/${stem}" MATCHES "(${CLIENT_STEMS})$" AND NOT "${stem}" MATCHES
                                                       "^(${SIM_IN_CLIENT})$")
      OR "${stem}" MATCHES "^(${CLIENT_IN_SIM})$"
      OR "${file}" STREQUAL "main.cpp"
    )
      set(kind client)
    endif ()
    set(kind_${file} ${kind})
    if (kind STREQUAL "sim")
      list(APPEND sims "${file}")
    endif ()
  endforeach ()

  set(problems "")
  foreach (file IN LISTS sims)
    file(STRINGS "${SOURCE_DIR}/${file}" lines
         REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<][^\">]+[\">]"
    )
    get_filename_component(folder "${file}" DIRECTORY)
    foreach (line IN LISTS lines)
      string(REGEX REPLACE "^[ \t]*#[ \t]*include[ \t]*[\"<]([^\">]+)[\">].*$"
                           "\\1" included "${line}"
      )
      if (folder)
        set(beside "${folder}/${included}")
      else ()
        set(beside "${included}")
      endif ()
      cmake_path(NORMAL_PATH beside)
      set(header "")
      if (DEFINED kind_${beside})
        set(header "${beside}")
      elseif (DEFINED kind_${included})
        set(header "${included}")
      endif ()
      if (header AND NOT kind_${header} STREQUAL "sim")
        if (NOT "${file}>${header}" IN_LIST allowed)
          list(APPEND problems
               "${file} includes ${header} (${kind_${header}})"
          )
        endif ()
      endif ()
    endforeach ()
  endforeach ()

  if (problems)
    list(JOIN problems "\n  " text)
    message(
      FATAL_ERROR
        "A file of the simulation includes a header that is not of the simulation. Move the types that the simulation "
        "needs into a header of the simulation, or add the header to OPENBLACK_SIM_IN_CLIENT_FOLDERS in "
        "src/CMakeLists.txt when it uses no device:\n  ${text}"
    )
  endif ()
endfunction()

if (CMAKE_SCRIPT_MODE_FILE)
  openblack_check_sim_includes(${CONFIG})
endif ()
