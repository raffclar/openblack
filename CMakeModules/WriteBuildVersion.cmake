# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
# Writes the openblack commit of the source tree into a text file next to the game. Each build runs this script:
#
#   cmake -DSOURCE_DIR=<source tree> -DOUTPUT=<file> [-DGIT_EXECUTABLE=<git>] -P WriteBuildVersion.cmake
#
# The file has one line: the full commit hash, with "-dirty" when tracked files of the tree have changes, or "unknown"
# when git or the .git folder is not there. The script writes the file only when its text changes.

set(version "unknown")
if (GIT_EXECUTABLE AND EXISTS "${SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse HEAD
    RESULT_VARIABLE result
    OUTPUT_VARIABLE hash
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
  )
  if (result EQUAL 0 AND hash MATCHES "^[0-9a-f]+$")
    set(version "${hash}")
    # Changes of tracked files only, as git describe --dirty: build output and other new files do not count
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" status --porcelain
              --untracked-files=no
      RESULT_VARIABLE result
      OUTPUT_VARIABLE changes
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
    )
    if (NOT result EQUAL 0 OR NOT changes STREQUAL "")
      set(version "${version}-dirty")
    endif ()
  endif ()
endif ()

set(old "")
if (EXISTS "${OUTPUT}")
  file(READ "${OUTPUT}" old)
endif ()
if (NOT old STREQUAL "${version}\n")
  file(WRITE "${OUTPUT}" "${version}\n")
endif ()
