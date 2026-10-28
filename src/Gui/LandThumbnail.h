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

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "DialogPainter.h"

namespace openblack::onlinemap
{
struct ThumbnailFile;
}

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui::land_thumbnail
{

/// The game paints the thumbnail and the clan logos into textures of 256 by 256 texels, 16 bits a texel: 4 bits each of
/// alpha, red, green and blue, from the top bit
inline constexpr int32_t k_TextureSide = 256;
using Texels = std::vector<uint16_t>;

/// The place of the thumbnail in the Landscape tab, between the list of the lands and the land's information
inline constexpr DialogRect k_Place {.min = {340, 410}, .max = {460, 530}};

/// The thumbnail of a land, painted as the game paints it
struct Picture
{
	/// The width and the height of the land's picture, from the top left of the texels
	int32_t side {0};
	/// k_TextureSide by k_TextureSide texels, a row at a time from the top
	Texels texels;
};

/// The texel of a pixel of the land's picture. A black pixel is clear. The game packs the other pixels with signed
/// bytes, so a channel of 128 or more changes the four bits above its own.
[[nodiscard]] uint16_t TexelOf(uint8_t red, uint8_t green, uint8_t blue) noexcept;

/// The texel of a player's colour (0xAARRGGBB), fully opaque
[[nodiscard]] uint16_t TexelOfColour(uint32_t colour) noexcept;

/// The thumbnail of a land from its ".thm" file. The picture fills the top left of the texels, and the rest is clear.
/// Each town has a mark of radius 1 and each citadel a mark of radius 2, in the colour of its owner (the last colour
/// for none). The marks go on in the order of the picture's rows, then its columns. At a place, the town goes on first.
/// @param colours the colour of each player, 0xAARRGGBB
[[nodiscard]] Picture Paint(const onlinemap::ThumbnailFile& file, std::span<const uint32_t, 8> colours);

/// The part of the texture that the thumbnail shows: from the top left to the side of the land's picture
[[nodiscard]] constexpr glm::vec2 UvMax(int32_t side) noexcept
{
	return glm::vec2(static_cast<float>(side) / static_cast<float>(k_TextureSide));
}

/// A land's thumbnail and text, as the resource cache keeps them for each ".thm" file
struct LandThumbnail
{
	/// The texture of Picture::texels; none when there is no renderer
	std::shared_ptr<graphics::Texture2D> texture;
	int32_t side {0};
	/// The description of the land in the file
	std::string description;
};

/// The clan logo shows in cell 5 of a sheet of 4 by 4 cells of 64 texels. The sheet's other cells are for the teams'
/// clans.
inline constexpr int32_t k_LogoCell = 5;
inline constexpr int32_t k_LogoCellSide = 64;

/// The sheet with a logo of 64 by 64 opacities from 0 to 15 in its logo cell. The logo is white, and each opacity is
/// the alpha of its texel. The other cells are clear.
[[nodiscard]] Texels LogoSheet(std::span<const uint8_t> logo);

/// The part of the sheet that the logo cell fills
[[nodiscard]] constexpr std::array<glm::vec2, 2> LogoCellUv() noexcept
{
	constexpr float k_Cell = 0.25f;
	const auto min = glm::vec2(static_cast<float>(k_LogoCell & 3), static_cast<float>(k_LogoCell / 4)) * k_Cell;
	return {min, min + k_Cell};
}

/// The place of the logo: a square in the middle of a rectangle, 10 less than the rectangle's smaller side
[[nodiscard]] constexpr DialogRect LogoPlace(const DialogRect& rect) noexcept
{
	const auto centre = (rect.min + rect.max) / 2;
	const auto half = (std::min(rect.Width(), rect.Height()) - 10) / 2;
	return {.min = centre - half, .max = centre + half};
}

/// The opacity of the logo: a quarter of the box's opacity, in steps of 1/255 as the game keeps it
[[nodiscard]] float LogoAlpha(float boxAlpha) noexcept;

} // namespace openblack::gui::land_thumbnail
