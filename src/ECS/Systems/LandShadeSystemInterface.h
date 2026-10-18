/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandShade.h"

namespace openblack::ecs::systems
{

/// What a drawn frame shades the land with
struct LandShadeFrame
{
	/// The camera of the frame: a block is in view when its box is (see object_draw_list::BlockInView)
	glm::mat4 viewProjection {1.0f};
	float nearPlane {1.0f};
	/// The land is mirrored in the sea, so a block's box goes as far down as up
	bool landReflection {true};
	/// The detail level draws the clouds' shadows
	bool cloudShadows {true};
	/// The alpha of the sky's clouds, of 255
	uint32_t skyAlpha {255};
	/// The land's colour of the frame (0xRRGGBB): the lights come on when it is dark
	uint32_t landColour {0xFFFFFF};
	/// The green of the land's light at full luminosity
	uint8_t fullLightGreen {0xFF};
	/// Where the hand is, when it shows
	std::optional<glm::vec3> hand;
	/// The hour of script time, which the village lights follow
	float scriptHour {12.0f};
	/// The stamps of light and shadow that the frame already has before the clouds': the particles' light maps and the
	/// flashes of lightning
	size_t stampsBefore {0};
};

/// The land's luminosity of the frame as one picture, laid out as the land's luminosity map: a texel for each cell of
/// the blocks from the land's first block, row by row along z. Each frame tells the rows that changed.
struct LandShadeMap
{
	std::span<const uint8_t> texels;
	glm::u16vec2 size {0};
	/// The rows from the first to the one before the last that changed this frame; none when no row changed
	uint16_t firstChangedRow {0};
	uint16_t lastChangedRow {0};
};

/// The land's luminosity while a frame is drawn (see land_shade): laid, darkened by the shadows of the sky's clouds and
/// the storms, and at night brightened by the lights of the hand and the villages. Things lit as the frame is drawn
/// read it; the land's luminosity map for the GPU is a copy of it.
class LandShadeSystemInterface
{
public:
	virtual ~LandShadeSystemInterface() = default;
	/// Puts the land back as it was laid, then shades it for this frame
	virtual void Shade(const LandShadeFrame& frame) = 0;
	/// Forgets the land: the next frame lays it again
	virtual void Clear() = 0;
	[[nodiscard]] virtual const land_shade::ShadedLand& Land() const = 0;
	[[nodiscard]] virtual LandShadeMap Map() const = 0;
	/// The shadows and the lights of the last frame
	[[nodiscard]] virtual size_t ShadowCount() const = 0;
	[[nodiscard]] virtual size_t LightCount() const = 0;
};

} // namespace openblack::ecs::systems
