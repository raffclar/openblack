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

/// The rules for a villager that a hunter brought down. The hunter leaves it with a small life. The villager falls,
/// then the hunter eats it for a number of turns, then it dies. The count of these turns is a part of the state "being
/// eaten". Thus a villager that goes out of this state (for example, in the hand) stays alive. Pure, tested on made-up
/// values.
namespace openblack::ecs::villager_eaten_rules
{

/// The life that a villager has after a hunter brings it down
inline constexpr float k_BroughtDownLife = 0.05f;
/// The turns that a hunter eats a villager before it dies
inline constexpr uint16_t k_BeingEatenTurns = 300;
/// The count starts again from this number when the turns end
inline constexpr uint16_t k_EatenAgainTurns = 50;

/// What a turn of "being eaten" does to the villager
enum class EatenTurn : uint8_t
{
	/// The hunter continues to eat it
	Eaten,
	/// The turns ended, but nothing can kill the villager: it gets up as after a fall
	GetsUp,
	/// The turns ended: the villager dies
	Dies,
};

struct EatenStep
{
	/// The turns that are left after this turn
	uint16_t turnsLeft;
	EatenTurn result;
};

/// Counts one turn of "being eaten". The count goes down by one each turn. At zero, the count starts again from 50 and
/// the villager dies or gets up. A count that starts at zero goes round to the highest number.
[[nodiscard]] constexpr EatenStep BeingEatenTurn(uint16_t turnsLeft, bool indestructible)
{
	const auto left = static_cast<uint16_t>(turnsLeft - 1);
	if (left != 0)
	{
		return {.turnsLeft = left, .result = EatenTurn::Eaten};
	}
	return {.turnsLeft = k_EatenAgainTurns, .result = indestructible ? EatenTurn::GetsUp : EatenTurn::Dies};
}

} // namespace openblack::ecs::villager_eaten_rules
