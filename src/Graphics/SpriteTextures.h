/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>

#include "ECS/Components/Sprite.h"

namespace openblack::graphics::sprite_textures
{

/// The id of the texture that gives a sprite its alpha: its own alpha texture, or the sprite's texture when it has none
[[nodiscard]] constexpr entt::id_type AlphaId(const ecs::components::Sprite& sprite) noexcept
{
	return sprite.alpha.value_or(sprite.texture);
}

} // namespace openblack::graphics::sprite_textures
