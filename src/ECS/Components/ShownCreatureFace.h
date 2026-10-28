/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

#include "Creature/CreatureLayers.h"

namespace openblack::ecs::components
{

/// A face that only this computer shows on a creature: the face that a stroke of this computer's hand pulls. The world
/// does not read it, and the other computers do not have it. The drawn body shows this face in place of the face of the
/// world, until the world pulls or relaxes a face of its own.
struct ShownCreatureFace
{
	/// The face that this computer shows
	creature_layers::FaceLayer face {};
	/// The face of the world when this computer last posed the shown face. A different face of the world at the next
	/// pose tells that the world changed its face.
	creature_layers::FaceLayer worldFace {};
	/// The bones in the world with the shown face, posed at the end of each of the last two game turns, the older pose
	/// first. They are empty until a turn poses them.
	std::vector<glm::mat4> olderTurnPose;
	std::vector<glm::mat4> newerTurnPose;
};

} // namespace openblack::ecs::components
