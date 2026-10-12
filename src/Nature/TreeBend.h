/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>

#include <optional>

#include <glm/vec3.hpp>

// What bends the trees the hand passes among. The empty hand bends nothing: only the thing it holds does, from where its
// model is drawn, as far as the model's size reaches.

namespace openblack::nature::tree_bend
{

/// A point trees within its radius lean away from
struct Source
{
	glm::vec3 position {0.0f};
	float radius {0.0f};
};

/// The model of the thing the hand holds, as it is drawn
struct HeldModel
{
	glm::vec3 position {0.0f};
	float scale {1.0f};
	/// Half the diagonal of its mesh's bounding box
	float halfDiagonal {0.0f};
};

/// What the hand bends the trees with: the thing it holds, reaching its drawn size; nothing with an empty hand
[[nodiscard]] inline std::optional<Source> HandSource(const std::optional<HeldModel>& held)
{
	if (!held.has_value())
	{
		return std::nullopt;
	}
	return Source {.position = held->position, .radius = std::abs(held->scale) * held->halfDiagonal};
}

} // namespace openblack::nature::tree_bend
