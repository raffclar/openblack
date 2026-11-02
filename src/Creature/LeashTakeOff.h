/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/LeashRules.h"
#include "ECS/Components/CreatureMind.h"

namespace openblack::creature_leash
{

/// What the mind of the creature forgets when the leash comes off it:
/// - the learning leash is no longer in the hand, and a miracle that the creature sees teaches it at the usual weight;
/// - the leash no longer leads the plan, so a forced plan stops the reactions again;
/// - the actions that the area that held it refused are possible again.
inline void ForgetLeash(ecs::components::CreatureMindState& mind)
{
	mind.leash.learningInHand = false;
	mind.leash.miracleSightingWeight = MiracleSightingWeight(false);
	mind.ledByLeash = false;
	mind.leftAreaActions.clear();
}

} // namespace openblack::creature_leash
