/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

/// The shapes the cursor picks a thing by, beyond the model it is drawn with: the invisible balls some kinds of thing
/// carry, and how a one-shot bubble's model is moved for the pick. Each ball is tested by its own circle on the screen
/// and counted at its middle's depth; a thing with a model and a ball is picked by whichever of the two is nearer.
/// Pure, for the picking system and its tests.
namespace openblack::ecs::pick_shapes
{

/// An invisible ball: its middle in the world and its radius
struct Ball
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
};

/// How wide a thing is across the land: the larger of its model's half extents across x and z, scaled
[[nodiscard]] float FlatRadius(glm::vec3 halfExtents, float scale);

/// How tall a thing is: its model's height, scaled
[[nodiscard]] float Height(glm::vec3 halfExtents, float scale);

/// A miracle dispenser's ball, round the bubble it floats: eight tenths of the building's height above where it stands,
/// half as wide again as the building. It picks the dispenser itself.
constexpr float k_DispenserBallHeight = 0.8f;
constexpr float k_DispenserBallWidth = 1.5f;
[[nodiscard]] Ball DispenserBall(glm::vec3 position, glm::vec3 halfExtents, float scale);

/// A one-shot bubble's ball: about its model's middle, 1.2 times the bubble's scale (twice its miracle's picture,
/// which is six tenths of the bubble's scale)
constexpr float k_BubbleBallPerScale = 2.0f * 0.6f;
[[nodiscard]] Ball BubbleBall(glm::vec3 modelMiddle, float scale);

/// Where a one-shot bubble's model is for the pick: moved from where it is drawn towards the camera by its width across
/// the land, so that its near side wins over what stands just in front of it
[[nodiscard]] glm::vec3 BubblePickOffset(glm::vec3 modelMiddle, glm::vec3 camera, glm::vec3 halfExtents, float scale);

} // namespace openblack::ecs::pick_shapes
