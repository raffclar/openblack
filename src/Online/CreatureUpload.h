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

#include <filesystem>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <LobbyApi.h>

/// The creature upload, as the creature upload tool of the game did it: the package of the creature's web page
/// (creature.lhp, which the game writes each time that it writes the creature's files), the creature's mind file and its
/// physique file go to the lobby, which shows the creature on a web page. Also the creature of a clan, which the game
/// server gets and puts back as the mind file, the physique file and the alignment.
namespace openblack::creature_upload
{

/// Where the files of the player's creature are
struct Paths
{
	std::filesystem::path package;
	std::filesystem::path mind;
	std::filesystem::path physique;
};

/// Why the files cannot go
enum class Problem : uint8_t
{
	/// The game did not write the package yet: the creature lived too short a time, or the player has no creature
	NoPackage,
	/// The package is not a package of the game
	BadPackage,
	NoMind,
	BadMind,
	NoPhysique,
	BadPhysique,
};

/// The files of the player's creature, read and checked, ready for the upload
[[nodiscard]] std::variant<lobby_api::CreatureUpload, Problem> Gather(const Paths& paths);

/// The creature of a clan from the files of a creature. None when a file is not a file of the game.
[[nodiscard]] std::optional<lobby_api::ClanCreature> PackClanCreature(float alignment, std::span<const uint8_t> mind,
                                                                      std::span<const uint8_t> physique);
/// Whether the files of a clan creature that the lobby gave are files of the game
[[nodiscard]] bool IsValidClanCreature(const lobby_api::ClanCreature& creature);

/// The files of the creature of the profile picked now, from the services of the game. None without a profile or a
/// folder for the files.
[[nodiscard]] std::optional<Paths> PlayersCreaturePaths();

} // namespace openblack::creature_upload
