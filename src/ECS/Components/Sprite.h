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

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Graphics/GraphicsHandle.h"

namespace openblack::ecs::components
{
/// Drawn as a screen sprite with angle 0 and no origin (Renderer.cpp drawSprite): the entity's Transform gives the
/// position and the half width / half height (scale x / y); Transform::rotation is ignored (the sprite only has a
/// roll). Only the temple's glows (RendererTemple.cpp) read facesCamera and alpha.
struct Sprite
{
	graphics::TextureHandle texture;
	glm::vec2 uvMin;
	glm::vec2 uvExtent;
	glm::vec4 tint;
	/// Additive (glows, the default) or normal alpha blending with a premultiplied tint (e.g. dust).
	bool additive {true};
	/// Turns to face the camera. Otherwise it lies as its transform turns it, in the transform's x and y.
	bool facesCamera {true};
	/// The alpha of a texture of colours, which the game loads from beside it as "<name>a.raw". Without it, the texture
	/// is a single channel of alpha that is also the sprite's brightness.
	std::optional<graphics::TextureHandle> alpha;
};

} // namespace openblack::ecs::components
