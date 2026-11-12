/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <LobbyApi.h>

/// The map package of the network map for 32 gods (deviation 249). The package holds only what openblack makes: the
/// landscape and the frame of the script. It has no line of the game data. The player's game adds the village of its
/// own land "Online Maps/mpm_4p_1.map" and gets the same ".map" file as the game server. The lobby's list gives the
/// SHA-256 and the start checksum of that file, and the download checks the file against them.
/// Everything here is pure, so that the tests can give their own small lands.
namespace openblack::gods_ring_map_package
{

/// Why a package does not give the map
enum class Failure : uint8_t
{
	/// The bytes are not a map package, or the package is damaged
	NotAPackage,
	/// The bytes are not the package that the lobby's list names
	WrongPackage,
	/// The package is of another map, or needs another land of the game data
	OtherMap,
	/// The player's game data has no land that the package needs
	NoGameData,
	/// The land of the player's game data is not a land of a network game, or has no village for the god
	GameDataDoesNotMatch,
};

/// The bytes of the package of the map. The same code always gives the same bytes, so the name of the package (its
/// SHA-256) changes only when the map changes.
[[nodiscard]] std::vector<uint8_t> Make();

/// The name of the file of a package: its SHA-256 as 64 hex digits and the extension ".obmap"
[[nodiscard]] std::string FileName(std::span<const uint8_t> package);

/// The bytes of the ".map" file of the map, from a package and the bytes of the land that gives the village
[[nodiscard]] std::variant<std::vector<uint8_t>, Failure> Assemble(std::span<const uint8_t> package,
                                                                   std::span<const uint8_t> templateMap);

/// The bytes of the ".map" file of a map of the lobby's list, from the package that the client downloaded and the land
/// of the player's game data (nothing when the game data does not have it). The package must have the SHA-256 that the
/// list gives. The download then checks the map's own SHA-256 and start checksum.
[[nodiscard]] std::variant<std::vector<uint8_t>, Failure>
AssembleDownloaded(const lobby_api::OnlineMap& map, std::span<const uint8_t> package,
                   std::optional<std::span<const uint8_t>> templateMap);

/// The text for the log of a failure
[[nodiscard]] std::string_view TextOf(Failure failure);

} // namespace openblack::gods_ring_map_package
