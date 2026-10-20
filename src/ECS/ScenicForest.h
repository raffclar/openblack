/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

/// How a town gathers the lone trees about it into a scenic forest of its own as the land is laid out
namespace openblack::ecs::scenic_forest
{

/// Scenic forests take in trees this far beyond the town's forest reach
inline constexpr float k_Beyond = 10.0f;
/// The walk out from the town's centre takes at most this many cells
inline constexpr int32_t k_MostCells = 99999;

/// The cells walked out from a town's centre, a spiral of single steps beginning at the centre itself, until a step
/// lies farther from the centre than the reach (so the walk ends at the first corner of a ring beyond it)
[[nodiscard]] std::vector<map_coords::MapCoords> Walk(const map_coords::MapCoords& centre, float reach);

/// A tree met in the walk
struct Tree
{
	glm::vec2 at {0.0f};
	/// The tree already stands in a forest
	bool inForest {false};
	/// That forest is another town's scenic forest, made about this centre
	std::optional<glm::vec2> scenicCentre;
};

/// Whether the town's scenic forest takes a tree it meets: a tree in no forest, or one of another town's scenic forest
/// standing strictly nearer this town's centre than that forest's
[[nodiscard]] bool Takes(const Tree& tree, glm::vec2 centre);

} // namespace openblack::ecs::scenic_forest
