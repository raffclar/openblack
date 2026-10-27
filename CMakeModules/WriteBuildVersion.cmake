# Writes the source file of openblack's build version: the semantic version of the project and the git commit of the
# sources. The script runs at each build, and it writes the file only when its text changes, so that an unchanged
# commit compiles nothing again.
#
# cmake -DSOURCE_DIR=<worktree> -DOUTPUT=<file.cpp> -DVERSION=<x.y.z> -P WriteBuildVersion.cmake

set(commit "unknown")
find_package(Git QUIET)
if (GIT_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --short=12 HEAD
    OUTPUT_VARIABLE git_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE git_result
    ERROR_QUIET
  )
  if (git_result EQUAL 0 AND NOT git_commit STREQUAL "")
    set(commit "${git_commit}")
  endif ()
endif ()

set(text
    "// Written by CMakeModules/WriteBuildVersion.cmake at each build. Do not change it.
#include \"Common/BuildVersion.h\"

std::string_view openblack::build_version::Semantic() noexcept
{
	return \"${VERSION}\";
}

std::string_view openblack::build_version::Commit() noexcept
{
	return \"${commit}\";
}

std::string_view openblack::build_version::Text() noexcept
{
	return \"${VERSION}+${commit}\";
}
"
)

set(old "")
if (EXISTS "${OUTPUT}")
  file(READ "${OUTPUT}" old)
endif ()
if (NOT old STREQUAL text)
  file(WRITE "${OUTPUT}" "${text}")
endif ()
