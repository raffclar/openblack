/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A colour added to the land light's specular when the villager or animal is drawn (per channel with saturation,
/// before the haze). The draw tests the whole value: the heal chakra writes alpha 0xFF, so the component stays at
/// RGB 0 too; only setting the colour to 0 (when the chakra goes) removes it.
struct SpecularColour
{
	glm::u8vec3 colour {0, 0, 0}; ///< r, g, b
};

} // namespace openblack::ecs::components
