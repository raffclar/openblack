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

#include <array>
#include <optional>
#include <span>
#include <vector>

/// The words of the creature's body that the mind file and the physique file keep: the marks on its skin, its tattoos,
/// and the block of the player's own pictures that its tattoos use. Game code changes these words only through the
/// functions here.
namespace openblack::creaturemind
{

/// A wound or a drop of blood on the creature's skin
struct MindMark
{
	/// Where, in texels of a skin 256 wide, and which of the base mesh's skins, 0 to 3
	uint8_t u {0};
	uint8_t v {0};
	uint8_t skin {0};
	/// How many steps of healing it has been through, 0 to 63
	uint8_t age {0};
	/// A wound's kind, 0 to 7, the row of the damage atlas it is drawn from, and its column, 0 to 7
	uint8_t type {0};
	uint8_t column {0};

	bool operator==(const MindMark&) const = default;
};

/// The file keeps a mark as one word. The lowest byte is the texel across and the next byte is the texel down. The
/// third byte has the skin in its top two bits and the age in the other bits. For a wound, the top byte has the column
/// over the kind, and the game always sets its top two bits. The file has 0 in the top byte of a drop of blood, and the
/// game does not read it.
[[nodiscard]] uint32_t WoundToWord(const MindMark& wound);
[[nodiscard]] MindMark WoundFromWord(uint32_t word);
[[nodiscard]] uint32_t BloodToWord(const MindMark& blood);
[[nodiscard]] MindMark BloodFromWord(uint32_t word);

/// The number of tattoo slots of a creature, and the number of designs
constexpr size_t k_TattooSlots = 8;
constexpr size_t k_TattooDesigns = 16;
/// The site of an empty slot
constexpr uint8_t k_NoTattooSite = 0xF;

/// A tattoo in a slot: the design, 0 to 15, the site, 0 to 7 (or k_NoTattooSite), and the colour, 0 to 255 a channel
struct MindTattoo
{
	uint8_t design {0};
	uint8_t site {k_NoTattooSite};
	uint8_t red {0};
	uint8_t green {0};
	uint8_t blue {0};

	bool operator==(const MindTattoo&) const = default;
};

/// The file keeps a tattoo as one word: the design in the lowest four bits, the site in the next four, then the blue,
/// green and red bytes of the colour
[[nodiscard]] MindTattoo TattooFromWord(uint32_t word);
[[nodiscard]] uint32_t TattooToWord(const MindTattoo& tattoo);

/// The side of a picture of the player's own, in texels
constexpr uint32_t k_TattooPictureSide = 64;
/// A picture of the player's own as the block keeps it: a level of 0 to 15 for each texel, a row at a time from the top
using TattooPicture = std::array<uint8_t, static_cast<size_t>(k_TattooPictureSide) * k_TattooPictureSide>;

/// The block of the player's own pictures that a creature keeps with its tattoos. The block has each picture that a
/// tattoo uses, so that the creature keeps the tattoo when the player does not have the picture any more.
///
/// The block starts with its size in bytes, a byte with the number of pictures, and a byte with a bit for each slot that
/// uses a picture. Two bytes of 0 follow, then one offset for each of the eight slots. Each picture follows at its
/// offset, two levels to a byte, with the first level in the low four bits. Slots with the same design use one picture.
///
/// `slotDesigns` has, for each slot that uses a picture, the design that the slot shows. `pictures` has the picture of
/// each design. A slot that has a design but no picture of that design does not use a picture.
[[nodiscard]] std::vector<uint8_t> WriteTattooPictures(std::span<const std::optional<uint8_t>, k_TattooSlots> slotDesigns,
                                                       std::span<const std::optional<TattooPicture>, k_TattooDesigns> pictures);

/// The picture that a slot uses, when the bit of the slot is set and the block has all of the picture
[[nodiscard]] std::optional<TattooPicture> ReadTattooPicture(std::span<const uint8_t> block, size_t slot);

} // namespace openblack::creaturemind
