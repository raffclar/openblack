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

} // namespace openblack::creature_file_load
