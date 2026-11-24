/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace openblack::resources
{

/// The files that the tables of the game name, but that the game data does not have. The original game looks for each
/// of these files, finds nothing and continues without the file. It shows no message.
constexpr std::array<std::string_view, 2> k_FilesNotInGameData = {
    // The music table gives this bank to the music of the welcome dance. No script of Land 1 plays this music.
    "audio/music/script/followuswelcome.sad",
    // The dance table gives this file to the dance DancePrayInwards. The data has only DancePrayingInwards.dat, which
    // has a different name.
    "scripts/dance/danceprayinwards",
};

/// Makes a path of a game table into one form: lower case, forward slashes, and one slash between two folders
[[nodiscard]] constexpr std::string NormaliseGameDataPath(std::string_view path)
{
	std::string result;
	result.reserve(path.size());
	for (const char c : path)
	{
		const char next = c == '\\' ? '/' : (c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
		if (next == '/' && !result.empty() && result.back() == '/')
		{
			continue;
		}
		result.push_back(next);
	}
	return result;
}

/// Whether a path of a game table is a file that the original game also does not find
[[nodiscard]] constexpr bool IsFileNotInGameData(std::string_view path)
{
	const auto normalised = NormaliseGameDataPath(path);
	return std::ranges::find(k_FilesNotInGameData, std::string_view {normalised}) != k_FilesNotInGameData.end();
}

} // namespace openblack::resources
