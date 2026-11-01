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
#include <entt/entity/fwd.hpp>

/// The creature upload, as the creature upload tool of the game did it: the package of the creature's web page
/// (creature.lhp, which the game writes each time that it writes the creature's files), the creature's mind file and its
/// physique file go to the lobby, which shows the creature on a web page. Also the creature of a clan, which goes to the
/// lobby at the end of a clan game and comes back as the start data of the next clan game.
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

/// The clan creature that the game of the team leader sends at the end of a clan game: the alignment of its god, the
/// creature file of the creature and the user data of the leader. None when the creature file is not a creature file of
/// the game.
[[nodiscard]] std::optional<lobby_api::ClanCreature> PackClanCreature(float godAlignment, std::span<const uint8_t> creatureFile,
                                                                      std::span<const uint8_t> userData);
/// Whether the creature file of a clan creature that the lobby gave is a creature file of the game
[[nodiscard]] bool IsValidClanCreature(const lobby_api::ClanCreature& creature);
/// The clan creature of a god at the end of a clan game, from the world of this computer: the alignment of the god and
/// the creature file of the creature as it is now. None when the god has no alignment or the creature has no mind.
[[nodiscard]] std::optional<lobby_api::ClanCreature> ClanCreatureAtGameEnd(entt::entity god, entt::entity creature,
                                                                           std::span<const uint8_t> userData);
/// The start data of a god that plays for a clan, from the answer of the lobby:
/// - a stored creature gives its creature file;
/// - a new creature gives the order to make it;
/// - no clan creature gives none, and the god brings the creature file of its player.
[[nodiscard]] std::optional<std::vector<uint8_t>> StartData(const lobby_api::ClanCreatureReply& reply);

/// The files of the creature of the profile picked now, from the services of the game. None without a profile or a
/// folder for the files.
[[nodiscard]] std::optional<Paths> PlayersCreaturePaths();

} // namespace openblack::creature_upload
