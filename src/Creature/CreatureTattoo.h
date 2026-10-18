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
#include <optional>
#include <span>
#include <vector>

#include <glm/gtc/type_precision.hpp>
#include <glm/vec3.hpp>

/// A creature can wear up to eight tattoos. Each is one of sixteen designs, in a colour, painted at one of the places its
/// species has for tattoos: a point on one of its skins, at a size, turned and perhaps flipped. A design is a mask of
/// sixteen levels, from Data/Textures/OriginalChooseSymbol.raw, painted over the skin a 4-bit channel at a time. The tattoos
/// are painted after the skin has been blended towards evil or good.
namespace openblack::creature_tattoo
{
/// The places on a body a tattoo can go
struct Site
{
	bool enabled {false};
	/// The centre, in texels of a skin 256 wide
	uint8_t u {0};
	uint8_t v {0};
	/// Which of the base mesh's skins, in the order it lists them
	uint8_t skin {0};
	/// How wide a tattoo is, a fraction of the skin's width
	float size {0.0f};
	/// Flipped left to right, after it is turned
	bool mirror {false};
	/// Quarter turns
	uint8_t rotation {0};
};
constexpr size_t k_SlotCount = 8;
using Sites = std::array<Site, k_SlotCount>;
/// The site of an empty slot
constexpr uint8_t k_NoSite = 0xF;

/// A tattoo a creature wears
struct Slot
{
	/// The design, 0 to 15
	uint8_t design {0};
	/// The site, 0 to 7, or k_NoSite for an empty slot
	uint8_t site {k_NoSite};
	/// 0 to 255 a channel
	glm::u8vec3 colour {0};

	[[nodiscard]] bool Empty() const { return site >= k_SlotCount; }
	bool operator==(const Slot&) const = default;
};
using Slots = std::array<Slot, k_SlotCount>;

/// The designs, four to a row of 64 by 64 texel cells
constexpr uint32_t k_DesignCount = 16;
constexpr uint32_t k_DesignSize = 64;
constexpr uint32_t k_DesignsPerRow = 4;
/// A design's mask is kept at 64, 32, 16, 8 and 4 texels across
constexpr size_t k_MaskLevels = 5;
/// The most a mask's level can be: the tattoo's colour alone
constexpr uint8_t k_MaxLevel = 15;

/// A square mask of levels, 0 (the skin) to 15 (the tattoo), a row at a time
struct Mask
{
	uint32_t size {0};
	std::vector<uint8_t> levels;

	[[nodiscard]] uint8_t At(uint32_t x, uint32_t y) const { return levels.at((y * size) + x); }
};

/// A design at each of its sizes, the largest first
using Design = std::array<Mask, k_MaskLevels>;

/// A design cut from the atlas of designs, an image of red, green and blue a row at a time: its level at each texel is
/// the top four bits of the blue. Each smaller mask averages squares of four of the one before, rounded down.
///
/// At the two lowest detail levels the game keeps the atlas at half its size, with every second texel of every second
/// row. It then reads the largest mask with a mistake: an even row of the mask reads a row of the design at half size,
/// each texel two times. An odd row reads the same row, but 64 texels to the right in the half atlas, so it shows a part
/// of another design. A texel after the end of the half atlas is skin.
[[nodiscard]] Design DesignFromAtlas(std::span<const std::array<uint8_t, 3>> atlas, uint32_t atlasWidth, uint32_t design,
                                     bool lowDetail);

/// A design from the levels of its largest mask, 64 by 64, a row at a time. The smaller masks average squares of four.
[[nodiscard]] Design DesignFromLevels(std::span<const uint8_t> levels);

/// A picture of the player's own, 64 by 64 grey values, a row at a time from the top. The player puts it in their
/// profile's folder as an image, and it takes the place of one of the sixteen designs.
using Picture = std::array<uint8_t, static_cast<size_t>(k_DesignSize) * k_DesignSize>;
using Pictures = std::array<std::optional<Picture>, k_DesignCount>;

/// The picture made from an image of red, green and blue, a row at a time from the top. The picture takes the top left
/// 64 by 64 texels of the image, and texels that the image does not have are black. Each colour becomes the grey nearest
/// to it, after each channel goes to the middle of its step of eight, as Windows changes a colour to a grey.
[[nodiscard]] Picture PictureFromImage(std::span<const std::array<uint8_t, 3>> image, uint32_t width, uint32_t height);

/// Puts the picture in the cell of a design in the atlas of designs, the same grey in the red, the green and the blue
void PutPicture(std::span<std::array<uint8_t, 3>> atlas, uint32_t atlasWidth, uint32_t design, const Picture& picture);

/// The block of pictures that a creature keeps with its tattoos, which its saved file also keeps. The block has each
/// picture of the player's own that a tattoo on a site uses (see creaturemind::WriteTattooPictures for its layout).
[[nodiscard]] std::vector<uint8_t> PicturesBlock(const Slots& slots, const Pictures& pictures);

/// The design that a slot paints from the block of pictures, when the slot's bit is set and the block holds the picture
[[nodiscard]] std::optional<Design> DesignInBlock(std::span<const uint8_t> block, size_t slot);

/// The masks that the slots of a player's creatures paint with, one for each slot. The game makes them again only at
/// some moments (see RemakeMasks). A slot paints with its mask, also when the slot now names another design.
using Masks = std::array<std::optional<Design>, k_SlotCount>;

/// Where the masks get the designs from
enum class Sheet : uint8_t
{
	/// The designs that the game ships with: a creature file that loads uses these
	Shipped,
	/// The designs with the player's own pictures in place of some of them: the tattoo editor uses these
	OwnPictures,
};

/// Makes the masks again from a creature's tattoos and its block of pictures. When the block is empty, nothing changes.
/// A slot on a site gets the picture of its bit in the block, else its design in the sheet. A slot without a site keeps
/// its old mask.
void RemakeMasks(Masks& masks, const Slots& slots, std::span<const uint8_t> block,
                 std::span<const Design, k_DesignCount> sheet);

/// Which of a design's masks a tattoo of a size is painted from: the largest for a quarter of the skin's width or more,
/// smaller for smaller tattoos
[[nodiscard]] size_t MaskLevel(float size);

/// A mask turned by quarter turns, clockwise as the skin is seen, then flipped left to right
[[nodiscard]] Mask Oriented(const Mask& mask, uint8_t rotation, bool mirror);

/// A texel of the skin, 4 bits a channel with blue the lowest, with the tattoo's colour painted over it at a level of
/// 15: each channel moved towards the top four bits of the colour's, rounded down. The texel comes out opaque.
[[nodiscard]] uint16_t PaintTexel(uint16_t skin, uint8_t level, const glm::u8vec3& colour);

/// The skin's width and height in texels
constexpr uint32_t k_SkinSize = 256;

/// A tattoo painted onto a skin of 256 by 256 texels: the design's mask for its size, turned and flipped, stretched to
/// the size times the largest mask's width (64 texels at size 1) and centred on the site. A tattoo that would run over
/// an edge isn't painted.
void Paint(std::span<uint16_t> skin, const Design& design, const glm::u8vec3& colour, const Site& site);

/// The tattoo palette, Data/tattoocols.raw, is 32 columns by 128 rows of colours
constexpr uint32_t k_PaletteColumns = 32;
constexpr uint32_t k_PaletteRows = 128;

/// A colour at a brightness, 0 to 1: a half leaves it as it is, more draws it towards white, less towards black, a
/// channel at a time in 256ths
[[nodiscard]] glm::u8vec3 Brightened(const glm::u8vec3& colour, float brightness);

/// A colour of the palette at a brightness, 0 to 1: a half leaves it as it is, more draws it towards white, less towards
/// black
[[nodiscard]] glm::u8vec3 PaletteColour(std::span<const std::array<uint8_t, 3>> palette, uint32_t column, uint32_t row,
                                        float brightness);

/// The slot a design put on a site goes in: the slot with that design on that site already, then the first empty one,
/// then the one with something else on that site; none when every slot holds another site's tattoo
[[nodiscard]] std::optional<size_t> SlotFor(const Slots& slots, uint8_t site, uint8_t design);

/// A tattoo as a saved creature keeps it, one word a slot (see creaturemind::TattooFromWord for its layout)
[[nodiscard]] Slot FromWord(uint32_t word);
[[nodiscard]] uint32_t ToWord(const Slot& slot);
} // namespace openblack::creature_tattoo
