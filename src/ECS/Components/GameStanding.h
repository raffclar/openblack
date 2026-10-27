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
#include <cstdint>

#include <array>
#include <optional>

namespace openblack::ecs::components
{

/// One winning condition of a network game for a player: how far the player got, the value to reach, and whether the
/// player reached it. A condition that the player reached stays reached.
struct WinCondition
{
	int32_t current {0};
	int32_t target {0};
	bool completed {false};
};

/// Why a god is out of a skirmish or a network game
enum class OutReason : uint8_t
{
	/// The god lost the game: it lost its temple, or another god won
	Defeated = 0,
	/// The human of the god left the network game
	Left = 1,
};

/// On a player: where the god stands in a skirmish or a network game
struct GameStanding
{
	/// The number of the winning conditions of the game
	static constexpr size_t k_ConditionCount = 15;
	/// The number of the values that the history of the god's score keeps
	static constexpr size_t k_ScoreHistoryCount = 500;

	/// The winning conditions of the network game, by condition number. A condition that the game does not use is empty.
	std::array<std::optional<WinCondition>, k_ConditionCount> conditions {};
	/// The share of the winning conditions that the god completed, from 0 to 1
	float share {0.0f};
	/// The god is out of the game
	bool out {false};
	/// The game finished its count of the god being out
	bool finished {false};
	/// The place of the god at the end: 1 is the first god out. None for a god that has no place yet.
	std::optional<int32_t> place;
	/// Why the god is out
	OutReason reason {OutReason::Defeated};
	/// The history of the god's score, a value each ten turns: its share of the winning conditions, or its share of the
	/// influence. The first value stays 0. When the history is full, the oldest value goes.
	std::array<float, k_ScoreHistoryCount> scoreHistory {};
	/// The place of the last value in the history. The values before it are the history that the game shows.
	int32_t scoreHistoryIndex {0};
};

} // namespace openblack::ecs::components
