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

namespace openblack::ecs::components
{
/// An object whose part under the water is drawn cut by the plane (0, -1, 0, 0) before the sea (animated or static),
/// like the sharks (with a dark blue specular colour), the swimming SuperVillagers and the fish puzzle's net.
/// Renderer::DrawCutBelowWater draws it into the reflection target, mirrored like everything the sea shows through.
///
/// The part above the water (the same call with the plane (0, 1, 0, 0), the shark's draw in the colour of the land
/// light table[255]) is Renderer::DrawCutByPlane with keep = 1: the owner of the object calls it instead of its
/// normal draw.
struct CutByPlane
{
	uint32_t belowColour {0xFF303070u}; ///< 0xAARRGGBB of the specular colour (the sharks' dark blue)
	/// the owner draws the part above the water too, instead of the whole model (the sharks' draw): the normal pass
	/// skips it and Renderer::DrawCutAboveWater draws it with keep = 1 and table[255]
	bool drawAbove {false};
};
} // namespace openblack::ecs::components
