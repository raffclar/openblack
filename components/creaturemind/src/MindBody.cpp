/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MindBody.h"

#include <bit>

using namespace openblack::creaturemind;

namespace
{
constexpr uint32_t k_AgeBits = 0x3Fu;
constexpr uint32_t k_SkinShift = 6;
constexpr uint32_t k_WoundTopBits = 0xC0u;
constexpr uint32_t k_KindBits = 0x7u;
constexpr uint32_t k_ColumnShift = 3;

constexpr uint32_t k_OffsetsStart = 8;
constexpr uint32_t k_BlockHeader = k_OffsetsStart + (4 * k_TattooSlots);
constexpr uint32_t k_PackedPicture = (k_TattooPictureSide * k_TattooPictureSide) / 2;

uint32_t TexelWord(const MindMark& mark)
{
	const auto third = ((static_cast<uint32_t>(mark.skin) & 0x3u) << k_SkinShift) | (mark.age & k_AgeBits);
	return static_cast<uint32_t>(mark.u) | (static_cast<uint32_t>(mark.v) << 8u) | (third << 16u);
}

MindMark TexelMark(uint32_t word)
{
	const auto third = (word >> 16u) & 0xFFu;
	return {.u = static_cast<uint8_t>(word & 0xFFu),
	        .v = static_cast<uint8_t>((word >> 8u) & 0xFFu),
	        .skin = static_cast<uint8_t>(third >> k_SkinShift),
	        .age = static_cast<uint8_t>(third & k_AgeBits)};
}
} // namespace

uint32_t openblack::creaturemind::WoundToWord(const MindMark& wound)
{
	const auto fourth = k_WoundTopBits | ((wound.column & k_KindBits) << k_ColumnShift) | (wound.type & k_KindBits);
	return TexelWord(wound) | (fourth << 24u);
}

MindMark openblack::creaturemind::WoundFromWord(uint32_t word)
{
	auto wound = TexelMark(word);
	const auto fourth = word >> 24u;
	wound.type = static_cast<uint8_t>(fourth & k_KindBits);
	wound.column = static_cast<uint8_t>((fourth >> k_ColumnShift) & k_KindBits);
	return wound;
}

uint32_t openblack::creaturemind::BloodToWord(const MindMark& blood)
{
	return TexelWord(blood);
}

MindMark openblack::creaturemind::BloodFromWord(uint32_t word)
{
	return TexelMark(word);
}

MindTattoo openblack::creaturemind::TattooFromWord(uint32_t word)
{
	return {
	    .design = static_cast<uint8_t>(word & 0xFu),
	    .site = static_cast<uint8_t>((word >> 4u) & 0xFu),
	    .red = static_cast<uint8_t>(word >> 24u),
	    .green = static_cast<uint8_t>(word >> 16u),
	    .blue = static_cast<uint8_t>(word >> 8u),
	};
}

uint32_t openblack::creaturemind::TattooToWord(const MindTattoo& tattoo)
{
	return (tattoo.design & 0xFu) | ((tattoo.site & 0xFu) << 4u) | (static_cast<uint32_t>(tattoo.blue) << 8u) |
	       (static_cast<uint32_t>(tattoo.green) << 16u) | (static_cast<uint32_t>(tattoo.red) << 24u);
}

std::vector<uint8_t>
openblack::creaturemind::WriteTattooPictures(std::span<const std::optional<uint8_t>, k_TattooSlots> slotDesigns,
                                             std::span<const std::optional<TattooPicture>, k_TattooDesigns> pictures)
{
	const auto usesPicture = [&pictures](const std::optional<uint8_t>& design) {
		return design.has_value() && *design < pictures.size() && pictures[*design].has_value();
	};
	uint32_t designs = 0;
	for (const auto& design : slotDesigns)
	{
		if (usesPicture(design))
		{
			designs |= 1u << *design;
		}
	}
	const auto count = static_cast<uint32_t>(std::popcount(designs));
	const auto size = (count * k_PackedPicture) + k_BlockHeader;
	std::vector<uint8_t> block(size, 0);
	const auto putWord = [&block](size_t at, uint32_t value) {
		for (size_t i = 0; i < 4; ++i)
		{
			block.at(at + i) = static_cast<uint8_t>(value >> (8 * i));
		}
	};
	putWord(0, size);
	block.at(4) = static_cast<uint8_t>(count);
	std::array<uint32_t, k_TattooDesigns> offsets {};
	uint32_t next = k_BlockHeader;
	for (size_t i = 0; i < slotDesigns.size(); ++i)
	{
		const auto& design = slotDesigns[i];
		if (!usesPicture(design))
		{
			continue;
		}
		block.at(5) = static_cast<uint8_t>(block.at(5) | (1u << i));
		auto& offset = offsets.at(*design);
		if (offset == 0)
		{
			// The first slot with the picture puts it in the block, two levels to a byte
			offset = next;
			next += k_PackedPicture;
			const auto& picture = *pictures[*design];
			for (uint32_t byte = 0; byte < k_PackedPicture; ++byte)
			{
				block.at(offset + byte) =
				    static_cast<uint8_t>(((picture.at((byte * 2) + 1) & 0xFu) << 4u) | (picture.at(byte * 2) & 0xFu));
			}
		}
		putWord(k_OffsetsStart + (i * 4), offset);
	}
	return block;
}

std::optional<TattooPicture> openblack::creaturemind::ReadTattooPicture(std::span<const uint8_t> block, size_t slot)
{
	if (block.size() < k_BlockHeader || slot >= k_TattooSlots || ((block[5] >> slot) & 1u) == 0)
	{
		return std::nullopt;
	}
	uint32_t offset = 0;
	for (size_t i = 0; i < 4; ++i)
	{
		offset |= static_cast<uint32_t>(block[k_OffsetsStart + (slot * 4) + i]) << (8 * i);
	}
	// A block that does not have all of the picture gives no picture
	if (offset < k_BlockHeader || offset > block.size() || block.size() - offset < k_PackedPicture)
	{
		return std::nullopt;
	}
	TattooPicture levels {};
	for (uint32_t byte = 0; byte < k_PackedPicture; ++byte)
	{
		const auto packed = block[offset + byte];
		levels.at(byte * 2) = packed & 0x0Fu;
		levels.at((byte * 2) + 1) = packed >> 4u;
	}
	return levels;
}
