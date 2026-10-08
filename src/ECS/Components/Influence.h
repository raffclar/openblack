/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

namespace openblack::ecs::components
{
/// The influence fields of a Town (kept apart from components::Town, whose `owner` is the only player whose influence
/// counts it). The town's turn recomputes the radius every turn (influence::ProcessTowns).
struct TownInfluence
{
	float radius {0.0f};      ///< base + abodes, x townInfluenceMultiplier; inside it the town gives 1
	bool noInfluence {false}; ///< The town constructor's last argument (false for CREATE_TOWN): no base, no abodes
	/// The radius of the last 3D influence update (written whether or not it made a circle), which the town's turn
	/// compares with radius (influence::NoteInfluence)
	float drawnRadius {0.0f};
};

/// The citadel's influence, fixed when its CitadelHeart is made: scale x the heart's story influence for the land. The
/// citadel's influence is it times playerInfluenceMultiplier.
struct CitadelInfluence
{
	float reach {0.0f};
	/// The citadel's influence at the last 3D influence update (always written), which the citadel's turn compares with
	/// the current one (influence::NoteInfluence)
	float drawnRadius {0.0f};
};
} // namespace openblack::ecs::components
