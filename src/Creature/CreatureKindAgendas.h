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

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions a creature does with particular kinds of thing: giving a tree to a town's store as wood,
/// and planting a tree somewhere else.
namespace openblack::creature_mind
{
/// The tree an action takes up first, when its hand doesn't hold a tree already
struct TreeToTake
{
	uint32_t tree {0};
	glm::vec2 point {0.0f};
	float radius {0.0f};
};

/// Giving a tree to a store as wood: unless it holds a tree, walking to the tree until within a fifth more than both
/// their radii and pulling it up; then going to where it can throw into the store, turning to face it a tenth of a
/// second (where the action counts as done), and throwing the tree in, with a look of compassion until it throws
[[nodiscard]] std::vector<Step> GiveWoodToStore(const std::optional<TreeToTake>& take, uint32_t store, float radius,
                                                float height);
constexpr float k_TreeReach = 1.2f;
constexpr float k_FaceStoreForWoodSeconds = 0.1f;

/// Planting a tree by a thing: unless it holds a tree, picking the tree up; then going right up to the thing, turning
/// to face it, and putting the tree down there, where it takes root
[[nodiscard]] std::vector<Step> PlantTreeBy(std::optional<uint32_t> tree, uint32_t by);

/// The most trees a creature remembers planting others by
constexpr size_t k_PlantedRemembered = 10;

} // namespace openblack::creature_mind
