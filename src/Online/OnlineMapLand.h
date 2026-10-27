/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cctype>

#include <algorithm>
#include <string>
#include <string_view>

/// The lands of network games: ".map" files in the game's "Online Maps" folder. Each file holds the land's script and its
/// landscape. The script loads the landscape by a name that is the map's name with ".lnd".
namespace openblack::online_map_land
{

namespace detail
{
/// The last part of a path, after the last forward or back slash, so that a path of the game's scripts reads the same on
/// every system
[[nodiscard]] constexpr std::string_view FileName(std::string_view path)
{
	const auto slash = path.find_last_of("/\\");
	return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

[[nodiscard]] inline std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}
} // namespace detail

/// The name of the folder of the game that holds the lands of network games
inline constexpr std::string_view k_Folder = "Online Maps";

/// The extension of a land of a network game
inline constexpr std::string_view k_Extension = ".map";

/// Whether a land's path is a land of a network game, by its extension in any case
[[nodiscard]] inline bool IsOnlineMap(std::string_view path)
{
	const auto name = detail::Lower(detail::FileName(path));
	return name.size() > k_Extension.size() && name.ends_with(k_Extension);
}

/// The name of a land of a network game: its file name with no folder and no extension, in lower case. Empty when the
/// path is not a land of a network game.
[[nodiscard]] inline std::string MapName(std::string_view path)
{
	if (!IsOnlineMap(path))
	{
		return {};
	}
	auto name = detail::Lower(detail::FileName(path));
	name.resize(name.size() - k_Extension.size());
	return name;
}

/// Whether the landscape that a map's script loads is the map's own landscape: the file name is the map's name with
/// ".lnd", in any case. The folder does not matter.
[[nodiscard]] inline bool IsOwnLandscape(std::string_view map, std::string_view landscape)
{
	if (!IsOnlineMap(map))
	{
		return false;
	}
	auto mapName = detail::Lower(detail::FileName(map));
	mapName.resize(mapName.size() - k_Extension.size());
	return detail::Lower(detail::FileName(landscape)) == mapName + ".lnd";
}

} // namespace openblack::online_map_land
