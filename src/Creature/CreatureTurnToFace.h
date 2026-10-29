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

// How a creature turns to face a thing, one game turn at a time, as the game does it:
// 1. It stops and asks its body to turn on the spot. The body refuses while it moves, turns or plays an action. Then the
//    creature asks again on the next turn. Right on top of the thing, it does not turn.
// 2. It waits while the body is busy. Then it checks that it faces the thing. If not, it turns again.
// 3. It holds the turn for a number of turns. On each of these turns it waits while the body is busy, and turns again
//    if it does not face the thing.
//
// Pure, tested with made-up bodies.

namespace openblack::creature_turn_to_face
{

/// Where the creature is in its turn to face a thing
struct Turn
{
	enum class Phase : uint8_t
	{
		/// It does not turn yet
		Idle,
		/// It asks its body to start the turn
		Start,
		/// It waits for the turn to end, then checks that it faces the thing
		Check,
		/// It holds the turn
		Hold,
		/// The turn is done
		Done,
	};
	Phase phase {Phase::Idle};
	/// The turns that it still holds the turn
	uint32_t holdTurns {0};
};

/// What the creature finds on this turn
struct Senses
{
	/// It is right on top of the thing
	bool onTop {false};
	/// It faces the thing
	bool facing {false};
	/// Its body moves, turns or plays an action
	bool bodyBusy {false};
};

/// The turns that it holds the turn for: the seconds times the turns in a second, with the part after the point cut off
[[nodiscard]] constexpr uint32_t HoldTurns(float seconds, uint32_t turnsPerSecond)
{
	return static_cast<uint32_t>(seconds * static_cast<float>(turnsPerSecond));
}

/// Does one turn of the turn to face a thing. `startTurn` asks the body to turn on the spot and gives true when the body
/// starts the turn. `holdTurns` is the number of turns that it holds the turn when it faces the thing.
template <typename StartTurn>
void Step(Turn& turn, const Senses& senses, uint32_t holdTurns, StartTurn&& startTurn)
{
	using Phase = Turn::Phase;
	switch (turn.phase)
	{
	case Phase::Idle:
	case Phase::Done:
		return;
	case Phase::Start:
		if (senses.onTop || startTurn())
		{
			turn.phase = Phase::Check;
		}
		return;
	case Phase::Check:
		if (senses.bodyBusy)
		{
			return;
		}
		if (senses.onTop)
		{
			turn.holdTurns = 0;
			turn.phase = Phase::Hold;
		}
		else if (!senses.facing)
		{
			startTurn();
		}
		else
		{
			turn.holdTurns = holdTurns;
			turn.phase = Phase::Hold;
		}
		return;
	case Phase::Hold:
		if (senses.bodyBusy)
		{
			return;
		}
		if (turn.holdTurns == 0)
		{
			turn.phase = Phase::Done;
			return;
		}
		--turn.holdTurns;
		if (senses.onTop)
		{
			turn.phase = Phase::Done;
		}
		else if (!senses.facing)
		{
			startTurn();
		}
		return;
	}
}

} // namespace openblack::creature_turn_to_face
