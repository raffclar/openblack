/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::graphics
{

/// The trees' moving shadows on the land, drawn each frame when they are asked for in place of the shadows the game
/// bakes into the land through their leaves (see LandShadowBake). Each tree's triangles are cast from the game's fixed
/// sun onto the flat plane through its origin, wherever the land under it lies, as it sways and bends, into a texture over
/// the whole island which darkens the land: a fully covered texel is half as bright.
struct ObjectShadows
{
	/// The game's default sun, from the object's origin: so far away that its shadows all fall the same way, as long
	/// along the land's x and z as the object is high
	static constexpr glm::vec3 k_Sun {-500000.0f, 500000.0f, -500000.0f};
	/// Under a fully covered texel the land's colour is multiplied by (255 - 255 / 2) / 256
	static constexpr float k_MaxDarkness = 0.5f;

	/// Where a point of an object with its origin at origin falls on the plane through the origin. Points
	/// under the origin fall straight down.
	[[nodiscard]] static glm::vec3 Project(glm::vec3 point, glm::vec3 origin);
};

} // namespace openblack::graphics
