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
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <GameSocket.h>

#include "Online/OnlineMapLand.h"
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

/// The most bytes of the name of a land on the game socket
inline constexpr size_t k_MostWireNameBytes = 260;

/// The letters that a path, a drive, a stream of a file or a wildcard uses on some system
inline constexpr std::string_view k_RefusedLetters = "\\:*?\"<>|";
/// The names of the devices of Windows, which Windows opens in place of a file of that name
inline constexpr std::array<std::string_view, 6> k_DeviceNames {"con", "prn", "aux", "nul", "conin$", "conout$"};

/// Whether a part of the name of a land is a plain name of a file or a folder on every system: not empty, not "." or
/// "..", no character that a path, a drive or a stream of a file uses, no control character, no space or full stop at
/// the end, and not the name of a device of Windows (such as CON, NUL, COM1 or LPT1, with or without an extension).
[[nodiscard]] inline bool IsPlainPart(std::string_view part) noexcept
{
	if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
	{
		return false;
	}
	const bool refused = std::ranges::any_of(part, [](char letter) {
		const auto byte = static_cast<unsigned char>(letter);
		return byte < 0x20 || byte == 0x7F || k_RefusedLetters.find(letter) != std::string_view::npos;
	});
	if (refused)
	{
		return false;
	}
	// Windows opens a device for a device name, also with an extension or spaces before it
	auto stem = part.substr(0, part.find('.'));
	while (!stem.empty() && stem.back() == ' ')
	{
		stem.remove_suffix(1);
	}
	if (std::ranges::any_of(k_DeviceNames, [stem](std::string_view device) { return SameName(stem, device); }))
	{
		return false;
	}
	const bool port = stem.size() == 4 && (SameName(stem.substr(0, 3), "com") || SameName(stem.substr(0, 3), "lpt")) &&
	                  std::isdigit(static_cast<unsigned char>(stem[3])) != 0;
	return !port;
}

/// Whether a name of a land that a server gives is a plain relative name: parts that IsPlainPart accepts, with "/"
/// between them, and at most k_MostWireNameBytes bytes. Such a name has no root, no drive, no network share and no
/// device, and it does not go up a folder. A client opens no file for any other name.
[[nodiscard]] inline bool IsPlainWireName(std::string_view name) noexcept
{
	if (name.empty() || name.size() > k_MostWireNameBytes)
	{
		return false;
	}
	size_t start = 0;
	while (true)
	{
		const auto end = name.find('/', start);
		if (!IsPlainPart(name.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start)))
		{
			return false;
		}
		if (end == std::string_view::npos)
		{
			return true;
		}
		start = end + 1;
	}
}

/// The last part of a path that a server gives: the text after its last "/" or "\"
[[nodiscard]] inline std::string_view LastPart(std::string_view path) noexcept
{
	const auto slash = path.find_last_of("/\\");
	return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

/// Whether the land and the landscape that the world state of a server names are the land and the landscape that are
/// open on this computer. The world state keeps the paths of the server's own files. This computer never opens them: the
/// name of each file must be a plain name (IsPlainPart) and the name of the open file. A land that the game builds has
/// no landscape file, and then neither side has one.
[[nodiscard]] inline bool NamesOpenLand(std::string_view serverLand, std::string_view serverLandscape,
                                        const std::filesystem::path& openLand, const std::filesystem::path& openLandscape)
{
	const auto sameFile = [](std::string_view server, const std::filesystem::path& open) {
		const auto name = LastPart(server);
		return IsPlainPart(name) && SameName(name, open.filename().generic_string());
	};
	if (openLand.empty() || !sameFile(serverLand, openLand))
	{
		return false;
	}
	if (serverLandscape.empty() || openLandscape.empty())
	{
		return serverLandscape.empty() && openLandscape.empty();
	}
	return sameFile(serverLandscape, openLandscape);
}

/// The name of a land on the game socket: the path of the land after the last Scripts folder of its path, with "/"
/// between the folders (for example "Land1.txt" or "Playgrounds/construct.txt"). A land of a network game is a map file
/// of the "Online Maps" folder of the game, not of the Scripts folder: its name starts with that folder (for example
/// "Online Maps/mpm_2p_1.map"). A land that is not in a Scripts folder gives the name of its file.
[[nodiscard]] inline std::string WireName(const std::filesystem::path& land)
{
	if (online_map_land::IsOnlineMap(land.generic_string()))
	{
		// The part of the path after the last "Online Maps" folder, or the file name when the path has no such folder
		std::filesystem::path afterFolder;
		bool inFolder = false;
		for (const auto& part : land)
		{
			if (SameName(part.generic_string(), online_map_land::k_Folder))
			{
				afterFolder.clear();
				inFolder = true;
			}
			else if (inFolder)
			{
				afterFolder /= part;
			}
		}
		if (!inFolder || afterFolder.empty())
		{
			afterFolder = land.filename();
		}
		return (std::filesystem::path(online_map_land::k_Folder) / afterFolder).generic_string();
	}
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

/// The path of a land of the game socket: under a Scripts folder, or for a land of a network game (a name that starts
/// with the "Online Maps" folder) in that folder of the game, which the file system finds as it finds the Scripts
/// folder. None when the name is empty, has a root, or goes out of the folder: a client loads only the lands of the
/// game. A map file outside the "Online Maps" folder is not a land. A name that is not plain (IsPlainWireName) gives none.
[[nodiscard]] inline std::optional<std::filesystem::path> PathOf(std::string_view wireName,
                                                                 const std::filesystem::path& scriptsFolder)
{
	if (!IsPlainWireName(wireName))
	{
		return std::nullopt;
	}
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
	if (online_map_land::IsOnlineMap(wireName))
	{
		std::filesystem::path inFolder(online_map_land::k_Folder);
		bool first = true;
		for (const auto& part : name)
		{
			if (first && !SameName(part.generic_string(), online_map_land::k_Folder))
			{
				return std::nullopt;
			}
			if (!first)
			{
				inFolder /= part;
			}
			first = false;
		}
		return inFolder == std::filesystem::path(online_map_land::k_Folder) ? std::nullopt
		                                                                    : std::optional<std::filesystem::path>(inFolder);
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
