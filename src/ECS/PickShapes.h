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

/// The ball round a miracle's picture (the seed shown in a globe, above an icon or over a chest): twice the picture's
/// size, about where the picture is, moved by the middle of the seed's own model (as the model is made, neither turned
/// nor scaled) for a seed that shows its model
constexpr float k_PictureBallPerSize = 2.0f;
[[nodiscard]] Ball PictureBall(glm::vec3 picture, float size, glm::vec3 seedModelMiddle);

/// A one-shot bubble's picture is six tenths of the bubble's scale, so its ball is 1.2 times that scale, about the
/// bubble's middle (and its seed's model's middle)
constexpr float k_BubblePictureSize = 0.6f;
[[nodiscard]] Ball BubbleBall(glm::vec3 modelMiddle, float scale, glm::vec3 seedModelMiddle);

/// An icon's picture keeps the size it is made at, 1
constexpr float k_IconPictureSize = 1.0f;

/// A vortex between the lands is picked by a ball at its middle, seven tenths as wide as its cylinder's model across the
/// land
constexpr float k_VortexBallWidth = 0.7f;
[[nodiscard]] Ball VortexBall(glm::vec3 centre, glm::vec3 cylinderHalfExtents);

/// A teleport stone is picked by a ball of 3 m about it, while its miracle is held in the hand
constexpr float k_TeleportBallRadius = 3.0f;
[[nodiscard]] Ball TeleportBall(glm::vec3 position);

/// A leash hanging at a temple is picked by a ball of 1 m about where it hangs
constexpr float k_LeashPostBallRadius = 1.0f;
[[nodiscard]] Ball LeashPostBall(glm::vec3 hangingPoint);

/// Where a one-shot bubble's model is for the pick: moved from where it is drawn towards the camera by its width across
/// the land, so that its near side wins over what stands just in front of it
[[nodiscard]] glm::vec3 BubblePickOffset(glm::vec3 modelMiddle, glm::vec3 camera, glm::vec3 halfExtents, float scale);

} // namespace openblack::ecs::pick_shapes
