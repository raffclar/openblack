/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// A worship site going up as a building site of the towns asked to build it: where its wood is piled, where what it is
// given goes. Pure rules, free of the game.

namespace openblack::ecs::worship_site_construction
{

/// Where on the site's model the one pile of its builders' wood stands, made when the first wood is brought
inline constexpr glm::vec3 k_PilePoint {9.0f, 0.0f, -50.0f};

/// Where a site's pile of wood stands on the land: the model's pile point, the site set down at a spot and turned to
/// face some way (in radians)
[[nodiscard]] glm::vec2 PilePlace(glm::vec2 spot, float facing);

/// Where something given to a worship site goes
enum class Destination : uint8_t
{
	/// Onto its building site's pile of wood
	SitePile,
	/// Into its food pot
	FoodPot,
	/// Nowhere: the site takes none of it
	Nowhere,
};

/// What a worship site does with something given to it. While it goes up its building site has first say over wood and
/// over anything given as any resource, and takes only wood; food always goes into the pot.
[[nodiscard]] Destination WhereGiven(ResourceType type, bool goingUp);

/// Whether a worship site stores a resource: food (or any) always, wood while it goes up
[[nodiscard]] bool Stores(ResourceType type, bool goingUp);

} // namespace openblack::ecs::worship_site_construction
