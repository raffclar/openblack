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

/// The rules by which a house that nobody lives in falls into ruin. Each time its town examines its buildings, an empty
/// house moves its neglect timer forward. When the timer is full, the house loses some of its life. These rules do not
/// use the state of the game, thus you can test them alone.
namespace openblack::ecs::abode_neglect
{

/// The part of the neglect timer that one examination of the town adds
inline constexpr float k_NeglectStep = 0.001f;

/// What the neglect rule reads of a building
struct NeglectFacts
{
	/// The adults and the children that live in it, and the places for each in its row of the buildings' table
	uint32_t adults {0};
	uint32_t children {0};
	uint32_t maxAdults {0};
	uint32_t maxChildren {0};
	/// The builders finished it
	bool built {false};
	/// A script holds a reference to it
	bool inScript {false};
	/// Its town is a place where people cannot live
	bool townUninhabitable {false};
};

/// The neglect timer after one examination, and whether the building loses life now
struct NeglectStep
{
	float timer {0.0f};
	bool losesLife {false};
};

/// Whether the building counts as empty. A building with no places for adults or children is never empty. The share
/// of child places uses whole numbers, thus a building that has only children, and a free child place, is empty.
[[nodiscard]] bool CountsAsEmpty(const NeglectFacts& facts);

/// One examination of the building by its town. An empty, built building that no script holds, in a town where people
/// can live, adds a step to its timer. When the timer gets to one, the building loses life and the timer starts again.
[[nodiscard]] NeglectStep Examine(float timer, const NeglectFacts& facts);

} // namespace openblack::ecs::abode_neglect
