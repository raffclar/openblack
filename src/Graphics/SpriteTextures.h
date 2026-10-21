/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>

#include <optional>
#include <type_traits>

#include <entt/core/fwd.hpp>

#include "ECS/Components/Sprite.h"

namespace openblack::graphics::sprite_textures
{

/// The id of the texture that gives a sprite its alpha: its own alpha texture, or the sprite's texture when it has none
[[nodiscard]] constexpr entt::id_type AlphaId(const ecs::components::Sprite& sprite) noexcept
{
	return sprite.alpha.value_or(sprite.texture);
}

/// The two textures that a sprite draws with, as this run has them
template <typename Texture>
struct Found
{
	const Texture* texture;
	const Texture* alpha;
};

/// Finds the textures of a sprite by their ids in the cache of this run. Returns nothing when the cache does not have
/// one of the two textures: the sprite must not draw with a handle that this run did not make.
template <typename Find>
    requires std::invocable<const Find&, entt::id_type>
[[nodiscard]] auto FindTextures(const ecs::components::Sprite& sprite, const Find& find)
    -> std::optional<Found<std::remove_cvref_t<decltype(*find(entt::id_type {}))>>>
{
	const auto* texture = find(sprite.texture);
	const auto* alpha = find(AlphaId(sprite));
	if (texture == nullptr || alpha == nullptr)
	{
		return std::nullopt;
	}
	return Found<std::remove_cvref_t<decltype(*texture)>> {.texture = texture, .alpha = alpha};
}

} // namespace openblack::graphics::sprite_textures
