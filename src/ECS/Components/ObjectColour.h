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

#include <array>

namespace openblack::ecs::components
{

/// An object's colour set in place of the land light: the power-up bands (the spell graphic in the player's colour,
/// the hand effect's bands). Its alpha goes in components::Alpha. RenderingSystem packs both with
/// argb_colour::PackInstanceColour / PackInstanceSpecular like the PSys mesh atoms' (vs_object: the colour x the model
/// light, no haze, plus the specular).
struct ObjectColour
{
	std::array<uint8_t, 3> rgb {255, 255, 255};
	uint32_t specular {0}; ///< a D3DCOLOR (its alpha unused)
};

} // namespace openblack::ecs::components
