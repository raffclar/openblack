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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>

#include "Enums.h"
#include "Scenery/AnimatedStaticRules.h"

namespace openblack::ecs::components
{

/// Scenery a script opens and closes: a gate, the gate stone plinth, the piper's cave entrance or the phone box
struct AnimatedStatic
{
	AnimatedStaticInfo type;
	/// The word the scripts last set: open (1) or closed (0). It picks the model the thing collides with and which way
	/// its clip plays.
	int32_t openState {animated_static::k_Closed};
	/// The gate stones laid in a plinth; always empty for anything else
	animated_static::GateStones gateStones {};
};

/// How an animated static is drawn: its place in its clip in milliseconds, and its bones posed from it. With no bones it
/// is drawn in the pose its model rests in.
struct AnimatedStaticPose
{
	uint32_t place {0};
	std::vector<glm::mat4> bones;
	/// The models of the gate stones drawn on a plinth this frame
	std::vector<entt::entity> stones;
};

/// A gate stone drawn on a plinth: one of the models of the stones laid in it, placed each frame, found by the cursor as
/// the plinth itself while it stands closed
struct PlinthStone
{
	entt::entity plinth {entt::null};
	bool pickable {false};
};

} // namespace openblack::ecs::components
