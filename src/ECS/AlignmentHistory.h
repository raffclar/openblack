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
#include <string_view>

#include "ECS/Components/AlignmentHistory.h"

/// The rules that keep the totals of the changes to the alignment of the player at this computer
namespace openblack::ecs::alignment_history
{

/// The labels of the causes in the debug window, in the order of the causes
constexpr std::array<std::string_view, components::k_AlignmentCauseCount> k_Labels {
    "Town    ", "Effect  ", "Reaction", "Creature", "Script  ", "Update  ", "Death   ", "Storage ", "Tree    ", "Villager",
};

/// Adds a change to the total of its cause and counts it. A change of 0 does not count. A change from a script counts
/// as it is. A change from a different cause counts multiplied by the scale of the player.
inline void Add(components::AlignmentHistory& history, components::AlignmentCause cause, float change, float playerScale)
{
	if (change == 0.0f)
	{
		return;
	}
	if (cause != components::AlignmentCause::Script)
	{
		change = playerScale * change;
	}
	const auto index = static_cast<std::size_t>(cause);
	history.totals.at(index) = change + history.totals.at(index);
	++history.counts.at(index);
}

} // namespace openblack::ecs::alignment_history
