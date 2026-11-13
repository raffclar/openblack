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

#include <algorithm>
#include <bit>

#include <glm/vec3.hpp>

/// The order in which the game draws the things that it sorts: all of them together, after the land's draw list, the
/// farthest from the camera first, whatever they are. Sprites, mists and clouds, the hand and the models marked to be
/// sorted go through it. A model that fades or has alpha and is not marked comes at once, at its place in the list.
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

/// The depth of the see-through copy of a scaffold at its yard place. The game sorts all of the copies of a workshop's
/// yard as one thing, by the key of the workshop's model. The sum of this key adds the height second, as Key does.
[[nodiscard]] inline uint32_t YardCopyDepth(const glm::vec3& workshop, const glm::vec3& camera) noexcept
{
	return Depth(workshop, camera);
}

/// The order of the see-through copies of one depth. The game draws the copies of a workshop's yard in one go, in the
/// order of the workshop's scaffold list, the newest scaffold first. Two workshops of the same key draw in the order of
/// the land's draw list.
[[nodiscard]] constexpr uint64_t YardCopyOrder(uint32_t workshopListPlace, uint32_t scaffoldIndex) noexcept
{
	return (static_cast<uint64_t>(workshopListPlace) << 32u) | scaffoldIndex;
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

/// The most places of the land's draw list that get a depth of their own. The list of the game holds fewer things.
constexpr uint32_t k_ListPlaces = 0x10000u;
/// The place of a thing that is not in the land's draw list
constexpr uint32_t k_Unlisted = k_ListPlaces;

/// The game draws a model that blends and is not sorted at once, at its place in the land's draw list: a model that fades
/// out in the distance, or a see-through copy. Thus it comes after the solid models and before every sorted thing, such
/// as the dome of a globe. As a bgfx depth: above k_AfterSolidBeforeSorted and under every key of BeforeSorted, the
/// first place the greatest. A thing that is not in the list comes after the things of the list.
/// A thing that comes before a fizzing creature in the list takes a depth in its segment (see
/// fizz_draw_order::Order::AtOnceDepth).
[[nodiscard]] constexpr uint32_t AtOnce(uint32_t listPlace) noexcept
{
	return k_AfterSolidBeforeSorted + 1u + (k_ListPlaces - std::min(listPlace, k_ListPlaces));
}

} // namespace openblack::graphics::zsort
