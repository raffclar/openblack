/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

namespace openblack::simulation
{

/// The time in milliseconds that the computer minds of the game can use in one frame, when the developer gives no other
/// time. The other work of the frame then has the rest of the 10 milliseconds of a frame at 100 frames a second.
inline constexpr float k_DefaultMindFrameBudgetMs = 8.0f;

/// The time that the turns of the players, with their computer minds, can use in one frame. When the players of a turn
/// use all of it, the turn stops between two players and continues in the next frame (deviation row 288). It keeps no game
/// state, and it never changes the world: it changes only the frame in which each player does its turn.
class MindFrameBudget
{
public:
	/// A new frame starts with all of its budget. With no budget, the turns never stop.
	constexpr void StartFrame(std::optional<float> budgetMs) noexcept
	{
		_budgetMs = budgetMs;
		_usedMs = 0.0f;
	}

	/// The whole turn of a player, with its computer mind, used this time
	constexpr void AddMindTime(float milliseconds) noexcept { _usedMs += milliseconds; }

	/// True when the frame has a budget
	[[nodiscard]] constexpr bool HasBudget() const noexcept { return _budgetMs.has_value(); }

	/// True when the minds used all of the budget of this frame. Then the turn stops before the next player.
	[[nodiscard]] constexpr bool Used() const noexcept { return _budgetMs.has_value() && _usedMs >= *_budgetMs; }

	/// The time that the minds used in this frame
	[[nodiscard]] constexpr float UsedMs() const noexcept { return _usedMs; }

private:
	std::optional<float> _budgetMs;
	float _usedMs {0.0f};
};

} // namespace openblack::simulation
