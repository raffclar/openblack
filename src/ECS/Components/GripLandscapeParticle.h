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

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// One sprite of the puff of dust thrown up where the hand grips the land. The turns make and remove the sprite. The
/// frames move it, so only its centre and its turns are the same on each computer of a network game.
struct GripLandscapeParticle
{
	/// The puff's centre, which the sprites spread out from as it grows
	glm::vec3 centre;
	/// Where the sprite sits in a puff of unit scale
	glm::vec3 offset;
	/// Seconds since the puff appeared
	float age = 0.0f;
	/// Animation frame, in frames since the start of the sprite sheet
	float frame = 0.0f;
	/// The steps of the turns that the sprite has before it goes, with the step of the turn that makes it
	int32_t turnsLeft = 0;
};

} // namespace openblack::ecs::components
