/*******************************************************************************
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
/// The pose a flying physics object is drawn at this frame (written into its 3D object's matrix):
/// between the body's pose at the start of the last game turn and at its end, by the turn fraction. Only on objects
/// that move in the physics (ECS/Physics/PhysicsObjects), and on the held object while the hand's release prediction
/// is drawn (PhysicsObjects::SetPrediction); the drawing takes it before DrawPosition
/// and Transform. The scale is Transform's.
struct PhysicsDrawPose
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
};
} // namespace openblack::ecs::components
