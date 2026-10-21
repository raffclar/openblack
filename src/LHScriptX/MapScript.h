/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>
#include <vector>

/// Reads the map script (Scripts/map.txt) that the game runs when a new game starts.
/// The shipped map script has one command: LOAD_FEATURE_SCRIPT, which names the land script of the first land.
namespace openblack::lhscriptx::map_script
{

/// The name of the map script in the Scripts folder
constexpr std::string_view k_FileName = "map.txt";

/// The command that names a land script to load
constexpr std::string_view k_LoadFeatureScript = "LOAD_FEATURE_SCRIPT";

/// Gives the path of each LOAD_FEATURE_SCRIPT command in the source, in the order of the source.
/// The paths are as the script writes them, for example ".\scripts\land1.txt".
/// The function ignores all other commands and the REM lines.
[[nodiscard]] std::vector<std::string> FeatureScripts(const std::string& source);

} // namespace openblack::lhscriptx::map_script
