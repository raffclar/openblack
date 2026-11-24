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

#include <optional>

namespace openblack::simulation
{

/// The time in milliseconds that the computer minds of the game can use in one frame, when the developer gives no other
/// time. The other work of the frame then has the rest of the 10 milliseconds of a frame at 100 frames a second.
inline constexpr float k_DefaultMindFrameBudgetMs = 8.0f;

/// The budget of the computer minds in each frame of a game. When the developer gives a time, the game uses it, and 0
/// gives no budget. When the developer gives no time, a run with a fixed frame time has no budget, because the budget
/// follows the machine's clock and the frames of two runs then are not the same. Other games have the default budget.
[[nodiscard]] constexpr std::optional<float> FrameBudgetMs(std::optional<float> givenMs, bool fixedFrameTime) noexcept
{
	if (givenMs.has_value())
	{
		return *givenMs > 0.0f ? givenMs : std::nullopt;
	}
	if (fixedFrameTime)
	{
		return std::nullopt;
	}
	return k_DefaultMindFrameBudgetMs;
}

/// The time that the turns of the players, with their computer minds, can use in one frame. When the players of a turn
/// use all of it, the turn stops between two players and continues in the next frame (deviation row 288). It keeps no game
/// state, and it never changes the world: it changes only the frame in which each player does its turn.
///
/// The frame that ends a turn also does the steps after the players, as the living and the scripts. Thus, in the first
/// k_LastFrameWithRoom frames of a turn, the budget keeps room for the time of these steps in the last whole turn.
class MindFrameBudget
{
public:
	/// The last frame of a turn in which the budget keeps room for the steps after the players. Thus the room adds
	/// no more than one frame to a turn.
	static constexpr uint32_t k_LastFrameWithRoom = 2;

	/// A new frame starts with all of its budget. With no budget, the turns never stop.
	constexpr void StartFrame(std::optional<float> budgetMs) noexcept
	{
		_budgetMs = budgetMs;
		_usedMs = 0.0f;
		_players = 0;
		_frameOfTurn = 1;
	}

	/// The frame of the turn that this frame plays: 1 for the frame that starts the turn
	constexpr void SetFrameOfTurn(uint32_t frame) noexcept { _frameOfTurn = frame; }

	/// The whole turn of a player, with its computer mind, used this time
	constexpr void AddMindTime(float milliseconds) noexcept
	{
		_usedMs += milliseconds;
		++_players;
	}

	/// The steps after the players took this time in the last whole turn
	constexpr void NoteStepsAfterPlayers(float milliseconds) noexcept { _afterPlayersMs = milliseconds; }

	/// True when the frame has a budget
	[[nodiscard]] constexpr bool HasBudget() const noexcept { return _budgetMs.has_value(); }

	/// True when the minds used all of the budget of this frame. Then the turn stops before the next player.
	[[nodiscard]] constexpr bool Used() const noexcept { return _budgetMs.has_value() && _usedMs >= *_budgetMs; }

	/// True when the turn stops before the next player. These are the conditions:
	/// - The minds used all of the budget of this frame.
	/// - Or the players that are left can end their turns in this budget, but the steps after the players then have no
	///   room. The players of this frame give the time of each player that is left. The next frame then ends the turn.
	[[nodiscard]] constexpr bool StopsBefore(size_t playersLeft) const noexcept
	{
		if (!_budgetMs.has_value())
		{
			return false;
		}
		if (_usedMs >= *_budgetMs)
		{
			return true;
		}
		// The budget keeps no room when the steps after the players need all of it
		if (_frameOfTurn > k_LastFrameWithRoom || _players == 0 || playersLeft == 0 || _afterPlayersMs >= *_budgetMs)
		{
			return false;
		}
		const float playersLeftMs = (_usedMs / static_cast<float>(_players)) * static_cast<float>(playersLeft);
		return _usedMs + _afterPlayersMs >= *_budgetMs && _usedMs + playersLeftMs < *_budgetMs;
	}

	/// The time that the minds used in this frame
	[[nodiscard]] constexpr float UsedMs() const noexcept { return _usedMs; }

private:
	std::optional<float> _budgetMs;
	float _usedMs {0.0f};
	/// The players that ended their turns in this frame
	uint32_t _players {0};
	uint32_t _frameOfTurn {1};
	float _afterPlayersMs {0.0f};
};

} // namespace openblack::simulation
