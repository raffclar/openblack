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

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

class Registry;

/// How a thing casts its moving shadow while the physics moves it (see graphics::shadow_texture)
enum class MovingShadow : uint8_t
{
	/// It casts none
	None,
	/// Its triangles cast, as the villagers' and the creatures' do (see graphics::GroundShadow)
	Shape,
	/// Its triangles cast through its textures' alpha, and the shadow is softened
	Texture,
};

/// How a thing casts its moving shadow while it flies or falls. Things whose land shadows go through their textures'
/// alpha (trees, big forests, dead trees, pots and fields) cast through the alpha. Things that cast a shadow into the
/// land, and the villagers and the animals, cast with their triangles. Other things cast none.
[[nodiscard]] MovingShadow MovingShadowOf(const Registry& registry, entt::entity entity);

} // namespace openblack::ecs
