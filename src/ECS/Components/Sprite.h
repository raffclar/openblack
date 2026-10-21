/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace openblack::ecs::components
{
struct Sprite
{
	/// The id of the texture in the texture cache. A saved world keeps this id, which names the same texture in each run.
	/// The handle of the graphics card changes from run to run, thus the sprite does not keep it.
	entt::id_type texture {};
	glm::vec2 uvMin;
	glm::vec2 uvExtent;
	glm::vec4 tint;
	/// Adds light to what is behind, like a glow. Otherwise blends over it by the tint's and texture's alpha.
	bool additive = true;
	/// Turns to face the camera. Otherwise it lies as its transform turns it, in the transform's x and y.
	bool facesCamera = true;
	/// The id of the alpha of a texture of colours, which the game loads from beside it as "<name>a.raw". Without it,
	/// the texture is a single channel of alpha that is also the sprite's brightness.
	std::optional<entt::id_type> alpha;
};

} // namespace openblack::ecs::components
