/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
/// The pose an object the drawn hand holds (from the press) is drawn at while the hand's logic does not hold it yet
/// (the pick-up waits): its model posed by the hand's state (Tug / Holding draw the held object out of the map), its
/// logic (Transform, map cells) where it is. The drawing takes it before PhysicsDrawPose, DrawPosition and Transform.
/// The scale is Transform's. Set and removed by the hand (HandSystem::UpdateRenderHandHeldPose)
struct HandDrawPose
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	/// The tug: the drawn matrix's up row times the tug's stretch (at most 1.3), only in DrawnModel; rotation stays a
	/// rotation for the hand and the physics. (inferred) the shadows and the fire, which go through DrawnModel
	/// (FireGraphic.cpp), take the stretch too. 1 for every other pose
	float upStretch {1.0f};
};
} // namespace openblack::ecs::components
