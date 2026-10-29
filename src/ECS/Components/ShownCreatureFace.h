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
/// world. The game has one face slot, so a change to the face of the world also changes this face in the same way: a
/// face that the world pulls starts from this face. The component goes when this face is the same as the face of the
/// world.
struct ShownCreatureFace
{
	/// The world save and the world parts that the server sends do not keep it, as the game does not save the face
	static constexpr bool k_LocalOnly = true;

	/// The face that this computer shows
	creature_layers::FaceLayer face {};
	/// The face of the world after the last pose or the last change to the face of the world. A different face of the
	/// world at the next pose tells that the world changed its face in a different way.
	creature_layers::FaceLayer worldFace {};
	/// The bones in the world with the shown face, posed at the end of each of the last two game turns, the older pose
	/// first. They are empty until a turn poses them.
	std::vector<glm::mat4> olderTurnPose;
	std::vector<glm::mat4> newerTurnPose;
};

} // namespace openblack::ecs::components
