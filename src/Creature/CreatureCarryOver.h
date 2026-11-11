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
#include <string_view>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

/// The player's creature carried on to the next land: kept with its file as a land is cleared, and loaded from it where
/// the next land's script says, sparkling into sight
namespace openblack::creature_carry_over
{

/// A loaded creature sparkles into sight over this many seconds, from out of sight
constexpr float k_ArrivalSeconds = 3.0f;

/// Where on the map a script's place puts a loaded creature: only the place's cell is taken, and the creature stands on
/// the ground in the middle of it
[[nodiscard]] map_coords::MapCoords ArrivalCoords(glm::vec2 place);

/// The player's creature is kept in two files named after the player's profile file: its mind under the profile's name,
/// and its physique under the name that creaturemind::PhysiqueFileName gives
struct KeptFiles
{
	std::filesystem::path mind;
	std::filesystem::path physique;
};
[[nodiscard]] KeptFiles KeptFilesIn(const std::filesystem::path& folder, std::string_view profileFile);

/// As it lives, the player's creature is written to its files each time it has lived this many more game turns
constexpr uint32_t k_SaveEveryTurns = 600;
/// Whether the creature is written to its files in this turn of the creature, from the number of turns it lived at its
/// last turn and the number it lived now. The game writes the creature in each turn where the number is a multiple of
/// 600 that is not 0. The number does not increase while the hand holds the creature, thus the game writes the
/// creature in each turn of that hold (vanilla-bugs 1720). When more than one turn goes by in one step (a faster time
/// scale for debug), a step past a multiple of 600 also writes the creature.
[[nodiscard]] bool SavesAsItLives(uint32_t turnsBefore, uint32_t turnsNow);

/// What this computer knows about a creature that it can write to the player's files
struct WriteFacts
{
	bool networkGame {false};
	/// The local player owns the creature
	bool localPlayers {false};
	/// The god of this computer brought the creature into the network game from the player's creature file
	bool broughtHere {false};
};

/// Whether this computer writes the creature to the player's files. In a game for one player, it writes the creature of
/// the local player. In a network game, it writes only the creature that the god of this computer brought. Only that
/// creature came from the files of this player, so the creatures of the other players never replace it.
[[nodiscard]] bool WritesToPlayersFiles(const WriteFacts& facts);

} // namespace openblack::creature_carry_over
