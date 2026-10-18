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

namespace openblack::ecs::components
{

/// The causes of a change to the alignment of a player, in the order that the totals keep them
enum class AlignmentCause : uint8_t
{
	Town,
	Effect,
	Reaction,
	Creature,
	Script,
	Update,
	Death,
	Storage,
	Tree,
	Villager,
};

constexpr std::size_t k_AlignmentCauseCount = 10;

/// The totals of the changes to the alignment of the player at this computer, one total and one count for each cause.
/// The game keeps these totals but does not show them to the player. Only a debug window shows them.
struct AlignmentHistory
{
	std::array<float, k_AlignmentCauseCount> totals {};
	std::array<int32_t, k_AlignmentCauseCount> counts {};
};

} // namespace openblack::ecs::components
