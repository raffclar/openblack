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

#include "Enums.h"

/// The rules for a creature that a script makes from a mind file for a player
namespace openblack::creature_file_load
{

/// The creature from the file grows up fully at once
constexpr uint32_t k_LoadedStage = 13;
/// The creature faces this angle about the vertical axis, in radians. The game gives it no angle, and no step of the
/// load turns it: the angle of a new object is 0.
constexpr float k_StartYaw = 0.0f;

/// The script message when the player has a creature already. The new creature becomes the creature of the player.
constexpr const char* k_OverwriteMessage = "Player has already his creature loaded, but I overwrite it";
/// The script message when the player is not in the game
constexpr const char* k_NoPlayerMessage = "Player has not a tocomputer player field";

/// Why the script makes no creature
enum class Refusal : uint8_t
{
	None,
	/// The player is not in the game
	NoPlayer,
	/// The mind file does not open
	NoFile,
	/// The place is not on the map
	OffMap,
	/// The species number is not a row of the creature table
	UnknownSpecies,
	/// The god has no citadel, so it brings no creature into the network game
	NoCitadel,
	/// The god has a creature already. The game makes no second creature for a god.
	HasCreature,
};

/// What the script asks for, as the game finds it
struct Request
{
	bool playerInGame {false};
	bool fileOpens {false};
	bool onMap {false};
	std::optional<CreatureType> species;
};

/// Why the game makes no creature for the request, or Refusal::None when it makes one. The checks occur in this sequence.
[[nodiscard]] Refusal WhyNot(const Request& request);

/// What the game finds for a god that starts a network game. Before the start, the player of the god sent a creature
/// file to all the players. The god brings the creature of that file into the game, at the creature home of its citadel.
struct BroughtRequest
{
	bool citadel {false};
	/// The player sent a file that is not empty. A player with no creature sends an empty file.
	bool file {false};
	/// The game can read the mind in the file
	bool readable {false};
	/// The home of the citadel is on the map
	bool onMap {false};
	/// The species of the file
	std::optional<CreatureType> species;
	/// The god has a creature on the land. A saved skirmish keeps the creature, and the load brings the profile's
	/// creature again.
	bool hasCreature {false};
};

/// Why the god brings no creature, or Refusal::None when it brings one. The checks occur in this sequence. A god with
/// no creature file plays the game with no creature: the game gives it no other creature.
[[nodiscard]] Refusal WhyNotBrought(const BroughtRequest& request);

/// What a computer knows about a creature that a god brings into a network game
struct BroughtCreature
{
	/// The god is the god of this computer's player
	bool ownGod {false};
	/// The start data of the god was the order to make a new clan creature
	bool clanOrder {false};
	/// The god of this computer's player plays for a clan
	bool ownGodPlaysForClan {false};
};

/// Whether this computer keeps the creature in the files of its player's profile, while it lives and at the end of the
/// game. Only the creature of this computer's own god can go there, and only when that god plays for no clan: the
/// creature of a clan god belongs to the clan. A new clan creature never goes to the files.
[[nodiscard]] bool KeptByThisComputer(const BroughtCreature& creature);

} // namespace openblack::creature_file_load
