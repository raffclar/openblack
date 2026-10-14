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

/// The rules for how the plan of a creature moves its alignment. On each game turn, the plan that the creature does
/// adds a change to the alignment change that waits. The table of the desire of the plan gives the change. A plan to eat
/// a villager uses the alignment table weight for the kind of the villager instead. Then the alignment of the creature
/// makes the change smaller or larger, as for a miracle.
namespace openblack::creature_deed_alignment
{
/// What the creature does on this turn, for its alignment
struct Deed
{
	/// The change for each turn that the table of the desire of the plan gives
	float desireChange {0.0f};
	/// For a plan to eat a villager: the weight of the kind of the villager in the first row (burn) of the effect
	/// alignment table. This value replaces the change of the desire
	std::optional<float> eatenVillagerWeight;
};

/// The change that the deed adds to the alignment change that waits. The species scale multiplies the change. If the
/// change goes in the direction of the alignment, it becomes smaller. If not, it becomes larger.
[[nodiscard]] float TurnChange(const Deed& deed, float speciesScale, float alignment);
} // namespace openblack::creature_deed_alignment
