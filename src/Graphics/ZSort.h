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

#include <bit>

#include <glm/vec3.hpp>

/// The order the game draws the things that blend in the world: all of them together, the farthest from the camera
/// first, whatever they are. Models that fade or have alpha, sprites, mists and clouds, and the hand all go through it.
namespace openblack::graphics::zsort
{

/// How far a point is from the camera for the sort: its distance squared, in single precision as the game works it
[[nodiscard]] inline float Key(const glm::vec3& point, const glm::vec3& camera) noexcept
{
	const auto d = point - camera;
	return ((d.x * d.x) + (d.y * d.y)) + (d.z * d.z);
}

/// The key as a bgfx depth for a view sorted by depth, the greatest first. A float that isn't negative orders the same
/// as its bits.
[[nodiscard]] inline uint32_t Depth(const glm::vec3& point, const glm::vec3& camera) noexcept
{
	return std::bit_cast<uint32_t>(Key(point, camera));
}

/// The key of a placed model: a building, a tree, a villager or an animal. The game adds the height last, thus the
/// sum can be one step different from Key.
[[nodiscard]] inline float ModelKey(const glm::vec3& point, const glm::vec3& camera) noexcept
{
	const auto d = point - camera;
	return ((d.x * d.x) + (d.z * d.z)) + (d.y * d.y);
}

/// The model key as a bgfx depth
[[nodiscard]] inline uint32_t ModelDepth(const glm::vec3& point, const glm::vec3& camera) noexcept
{
	return std::bit_cast<uint32_t>(ModelKey(point, camera));
}

/// The key of a number that floats up from a thing. The game adds the first axis last.
[[nodiscard]] inline float FloatingNumberKey(const glm::vec3& point, const glm::vec3& camera) noexcept
{
	const auto d = point - camera;
	return ((d.y * d.y) + (d.z * d.z)) + (d.x * d.x);
}

/// The key of the floating number as a bgfx depth
[[nodiscard]] inline uint32_t FloatingNumberDepth(const glm::vec3& point, const glm::vec3& camera) noexcept
{
	return std::bit_cast<uint32_t>(FloatingNumberKey(point, camera));
}

/// The game draws a creature fizzing out of sight in its place among the solid models, before everything it sorts, and
/// those models the nearest first. As a bgfx depth: above every sorted key (a distance's bits never reach the top bit),
/// the nearest the greatest, with its lowest four bits left for the draws that follow it in turn.
[[nodiscard]] inline uint32_t BeforeSorted(const glm::vec3& point, const glm::vec3& camera) noexcept
{
	return ~Depth(point, camera) | 0xFu;
}

/// The game draws the words of the learning marks after the solid models and the creatures that fizz out of sight, but
/// before everything it sorts. As a bgfx depth: under every key of BeforeSorted and above every sorted key. Draws of
/// this depth keep the order that the canvas gets them in.
constexpr uint32_t k_AfterSolidBeforeSorted = 0x80000000u;

} // namespace openblack::graphics::zsort
