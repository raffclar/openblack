/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>

namespace openblack::ecs::components
{

/// The number of players that a record of each player has a place for. A network land can have 32 gods, so each record
/// keeps 32 places, by player number. The game kept 8 (deviation 150).
inline constexpr size_t k_PlayerRecordSize = 32;

/// How strongly the player is the ally of each player, by player number. A script sets the strength of an alliance
/// for the two players together. A strength above 0 makes an ally, and an ally above 0.1 lends its influence.
struct PlayerAllies
{
	std::array<float, k_PlayerRecordSize> strength {};
};

/// What the computer mind of the player thinks of each player, by player number. The mind keeps twice the value that
/// a script gives, and a script reads back half of the kept value. Each player starts with a kept value of 1.
struct ComputerAttitudes
{
	static constexpr float k_StartValue = 1.0f;

	std::array<float, k_PlayerRecordSize> kept = [] {
		std::array<float, k_PlayerRecordSize> start {};
		start.fill(k_StartValue);
		return start;
	}();
};

/// The influence power of the player as the player's last turn found it, and its running average. All questions about
/// the power read these values. Thus a mind sees the players before it in this turn and the players after it in the
/// last turn.
struct PlayerInfluencePower
{
	float power {0.0f};
	float average {0.0f};
};

} // namespace openblack::ecs::components
