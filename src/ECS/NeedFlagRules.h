/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The flag beside a building that shows how much the building needs a thing, for example the wood flag of a workshop.
/// The flag flies higher the more the building needs the thing. It eases up and down, and turns to face the camera. It
/// shows only while the need is more than the least need that its row of the info gives.
namespace openblack::ecs::need_flag
{

/// The row of the info for the wood flag of a workshop
inline constexpr size_t k_WoodRow = 3;
/// The special point of the workshop's model where the wood flag flies
inline constexpr size_t k_WorkshopPoint = 9;

/// The values of a row of the info for a flag
struct Row
{
	/// The need at which the flag flies at its full height
	float maxNeed {1.0f};
	/// The flag shows only for a need more than this
	float showsAfter {0.0f};
	/// The height of the flag over the ground at the full need, in metres
	float maxHeight {0.0f};
};

/// Whether a flag shows for a need
[[nodiscard]] bool Shows(float need, const Row& row);

/// The share of its full height that a flag goes to for a need, never more than 1
[[nodiscard]] float Fill(float need, const Row& row);

/// The size of a flag that goes to a share of its full height. The flag grows from nothing to its full size over the
/// first part of the room above the least need that shows.
[[nodiscard]] float DrawnScale(float fill, const Row& row);

/// The x and z of the place where a flag flies, on the land: the special point of the building's model, moved through
/// the building's matrix, then kept as a map position
[[nodiscard]] glm::vec2 Place(const glm::vec3& point, const glm::mat4& building);

} // namespace openblack::ecs::need_flag
