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

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The creatures that the interface shows over itself, as the Creature Stats tab of the statistics box shows each
/// player's creature in its box. One camera turns round all of them. Each creature is moved for its draw to the point in
/// front of that camera that is under its place on the screen, at a depth that makes it fill about the same part of its
/// box at all sizes.
namespace openblack::graphics::interface_creatures
{

/// A creature to show, and where, in the interface's space: the 800 by 600 screen that the game's interface is laid out
/// on
struct Shown
{
	entt::entity creature {entt::null};
	/// The point under the creature's feet and the middle of its body
	glm::vec2 anchor {0.0f};
	/// The smaller side of its box
	int size {0};
};

/// The creatures to show in a frame
struct Request
{
	std::vector<Shown> creatures;
	/// The angle of the camera round them, in radians
	float turn {0.0f};
	/// Where the interface's space is on the window, in pixels: its top left corner and its size
	glm::vec2 viewOrigin {0.0f};
	glm::vec2 viewSize {0.0f};
};

/// The size of the interface's space, which the creatures are placed and sized in as the game placed them on its
/// default screen
constexpr glm::vec2 k_Space {800.0f, 600.0f};

/// The camera goes round the creatures at this distance and height, and looks at a point at the second height
constexpr float k_CameraDistance = 70.0f;
constexpr float k_CameraHeight = 40.0f;
constexpr float k_FocusHeight = 20.0f;
/// A creature of size 1 stands this tall
constexpr float k_HeightPerSize = 15.0f;
/// The depth of a creature is this number times its height over the size of its box in pixels
constexpr float k_DepthScale = 900.0f;
/// The smallest size of a box, in pixels
constexpr int k_SmallestSize = 10;

/// The eye of the camera at an angle, and the point it looks at
[[nodiscard]] glm::vec3 Eye(float turn);
[[nodiscard]] glm::vec3 Focus();
/// The camera's view at an angle
[[nodiscard]] glm::mat4 View(float turn);

/// The depth in front of the camera that a creature of a size is moved to, for a box of a size in pixels. Normally it
/// is k_DepthScale times the creature's height over the size of the box. The share of the screen that the interface
/// takes divides it.
[[nodiscard]] float Depth(float creatureSize, int boxSize, float interfaceShare);

/// The camera's projection over the interface's space: the projection of the world's camera, which keeps the width of
/// its view, made as tall as the interface's space is
[[nodiscard]] glm::mat4 SpaceProjection(const glm::mat4& worldProjection);

/// The world point under a pixel of the screen, at a depth in front of the camera
[[nodiscard]] glm::vec3 PointFromScreen(const glm::mat4& view, const glm::mat4& projection, glm::vec2 screen, glm::vec2 pixel,
                                        float depth);

/// How far a creature moves for its draw: its bones' middle across the land, and its feet's height, go to the point.
/// The bones are in the creature's own space and the model places them in the world.
[[nodiscard]] glm::vec3 MoveToPoint(const glm::vec3& point, const glm::mat4& model, std::span<const glm::mat4> bones,
                                    float feetHeight);

} // namespace openblack::graphics::interface_creatures
