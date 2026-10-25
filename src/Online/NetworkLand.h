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
#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <GameSocket.h>

#include "Story/NewGameChoice.h"

/// The name of the land of a network game on the game socket. The server sends the path of its land under the Scripts
/// folder of the game, and each client loads the land at the same path under its own Scripts folder. These rules keep
/// no state, so that the server, the game and the tests use them all.
namespace openblack::network_land
{

/// The name of the folder of the land scripts
inline constexpr std::string_view k_ScriptsFolder = "scripts";

/// Whether two names are the same when the case of the letters is ignored
[[nodiscard]] inline bool SameName(std::string_view a, std::string_view b) noexcept
{
	return std::ranges::equal(a, b, [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}

/// The name of a land on the game socket: the path of the land after the last Scripts folder of its path, with "/"
/// between the folders (for example "Land1.txt" or "Playgrounds/construct.txt"). A land that is not in a Scripts folder
/// gives the name of its file.
[[nodiscard]] inline std::string WireName(const std::filesystem::path& land)
{
	std::filesystem::path afterScripts;
	bool found = false;
	for (const auto& part : land)
	{
		if (SameName(part.generic_string(), k_ScriptsFolder))
		{
			afterScripts.clear();
			found = true;
		}
		else if (found)
		{
			afterScripts /= part;
		}
	}
	if (!found || afterScripts.empty())
	{
		return land.filename().generic_string();
	}
	return afterScripts.generic_string();
}

/// The path of a land of the game socket under a Scripts folder. None when the name is empty, has a root, or goes out
/// of the Scripts folder: a client loads only the land scripts of the game.
[[nodiscard]] inline std::optional<std::filesystem::path> PathOf(std::string_view wireName,
                                                                 const std::filesystem::path& scriptsFolder)
{
	const std::filesystem::path name(wireName);
	if (name.empty() || name.has_root_name() || name.has_root_directory())
	{
		return std::nullopt;
	}
	for (const auto& part : name)
	{
		if (part == "..")
		{
			return std::nullopt;
		}
	}
	return scriptsFolder / name;
}

/// Whether a loaded land is the land of a name of the game socket
[[nodiscard]] inline bool IsLand(const std::filesystem::path& loaded, std::string_view wireName)
{
	const auto name = WireName(loaded);
	return SameName(name, wireName);
}

/// The name of the land of a start, for the log
[[nodiscard]] inline std::string DisplayName(const game_socket::Start& start)
{
	if (start.testbed)
	{
		return "the testbed";
	}
	if (start.godsRingMap)
	{
		return "the network map for 32 gods";
	}
	return start.land;
}

/// The land, the seed and the start of the world that this computer started before it joined a game server
struct OwnWorld
{
	std::filesystem::path land;
	bool testbed {false};
	bool godsRingMap {false};
	uint32_t seed {0};
	std::optional<new_game_choice::NewGameStart> opening;
};

/// The values of this computer's world that are not the same as the server's start, as "land", "seed" and "start"
/// joined by ", ". Empty when all are the same. The server's values always win, so this text is only for the log.
[[nodiscard]] inline std::string OwnValuesThatDiffer(const OwnWorld& own, const game_socket::Start& start)
{
	const bool sameLand = own.testbed == start.testbed && own.godsRingMap == start.godsRingMap &&
	                      (start.testbed || start.godsRingMap || IsLand(own.land, start.land));
	const auto serverOpening = start.opening.empty() ? std::nullopt : new_game_choice::ParseNewGameStart(start.opening);
	std::vector<std::string_view> names;
	if (!sameLand)
	{
		names.emplace_back("land");
	}
	if (own.seed != start.seed)
	{
		names.emplace_back("seed");
	}
	if (own.opening != serverOpening)
	{
		names.emplace_back("start");
	}
	std::string text;
	for (const auto name : names)
	{
		text += text.empty() ? "" : ", ";
		text += name;
	}
	return text;
}

} // namespace openblack::network_land
