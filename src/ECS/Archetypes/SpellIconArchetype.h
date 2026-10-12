/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
/// The icons that show the miracles towns offer
class SpellIconArchetype
{
public:
	/// How many of a village centre's hands its model has places for
	[[nodiscard]] static size_t CentreHandPlaces(entt::entity centre);
	/// A stone hand on a village centre, showing a miracle: at the centre's model's point for the hand, turned
	/// as the point is and scaled as the centre is; none when the model has no such point
	static entt::entity CreateCentreHand(entt::entity centre, entt::entity town, size_t hand, SpellSeedType seed);
	/// A worship site's icon of a miracle, at the first of the site's icon places with no icon on it, further out from the
	/// site while they are all taken; none when the site has no model to place it by
	static entt::entity CreateSiteIcon(entt::entity site, SpellSeedType seed);
};
} // namespace openblack::ecs::archetypes
