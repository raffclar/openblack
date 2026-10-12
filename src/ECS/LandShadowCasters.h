/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

class Registry;

/// How a thing casts its shadow into the land's textures (see graphics::shape_shadows and graphics::tree_shadows)
enum class LandShadow : uint8_t
{
	/// It casts none
	None,
	/// Its triangles that face the sun cast, and those drawn from both sides
	Shape,
	/// Every one of its triangles casts: the temple's
	ShapeBothSides,
	/// Its triangles cast through its textures' alpha: the trees' and the big forests'
	Leaves,
};

/// How a thing casts its shadow into the land. Buildings cast once they are built (a football pitch and a worship site
/// from the start), and so do the island's features, rocks, teleport stones, spell dispensers, balls, rewards and the
/// other things lying about. Fields, animated scenery, flowers, dead trees, pots and piles, the village totem, miracle
/// shields, seeds and vortices, street lights and anything living cast none.
[[nodiscard]] LandShadow LandShadowOf(const Registry& registry, entt::entity entity);

} // namespace openblack::ecs
