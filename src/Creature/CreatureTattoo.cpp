/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTattoo.h"

#include <cassert>

#include <algorithm>
#include <bit>

using namespace openblack;
using namespace openblack::creature_tattoo;

namespace
{
/// The masks' sizes by how big the tattoo is, in 64ths of the skin's width: at least 48, 24, 12 or 6, or smaller
constexpr std::array<int32_t, k_MaskLevels - 1> k_LevelThresholds {48, 24, 12, 6};
constexpr float k_SizeSteps = 64.0f;
/// The skin's channels, 4 bits each, blue the lowest
constexpr uint16_t k_Blue = 0x000F;
constexpr uint16_t k_Green = 0x00F0;
constexpr uint16_t k_Red = 0x0F00;
constexpr uint16_t k_Opaque = 0xF000;
/// A colour becomes a grey after each channel goes to the middle of its step of eight
constexpr uint32_t k_StepMask = 0xF8;
constexpr uint32_t k_StepMiddles = 3 * 4;
/// The block of pictures: its size, the number of pictures, the slots' bits, two spare bytes, then eight offsets
constexpr uint32_t k_OffsetsStart = 8;
constexpr uint32_t k_BlockHeader = k_OffsetsStart + (4 * k_SlotCount);
/// A picture in the block keeps two levels to a byte
constexpr uint32_t k_PackedPicture = (k_DesignSize * k_DesignSize) / 2;

Mask Halved(const Mask& mask)
{
	Mask half {.size = mask.size / 2, .levels = {}};
	half.levels.resize(static_cast<size_t>(half.size) * half.size);
	for (uint32_t y = 0; y < half.size; ++y)
	{
		for (uint32_t x = 0; x < half.size; ++x)
		{
			const auto sum = mask.At(x * 2, y * 2) + mask.At((x * 2) + 1, y * 2) + mask.At(x * 2, (y * 2) + 1) +
			                 mask.At((x * 2) + 1, (y * 2) + 1);
			half.levels[(y * half.size) + x] = static_cast<uint8_t>(sum / 4);
		}
	}
	return half;
}
} // namespace

Design creature_tattoo::DesignFromAtlas(std::span<const std::array<uint8_t, 3>> atlas, uint32_t atlasWidth, uint32_t design,
                                        bool lowDetail)
{
	std::vector<uint8_t> levels(static_cast<size_t>(k_DesignSize) * k_DesignSize, 0);
	const auto level = [&atlas, atlasWidth](uint32_t x, uint32_t y) -> uint8_t {
		const auto index = (static_cast<size_t>(y) * atlasWidth) + x;
		return x < atlasWidth && index < atlas.size() ? static_cast<uint8_t>(atlas[index][2] >> 4u) : uint8_t {0};
	};
	if (!lowDetail)
	{
		const auto left = (design % k_DesignsPerRow) * k_DesignSize;
		const auto top = (design / k_DesignsPerRow) * k_DesignSize;
		for (uint32_t y = 0; y < k_DesignSize; ++y)
		{
			for (uint32_t x = 0; x < k_DesignSize; ++x)
			{
				levels[(y * k_DesignSize) + x] = level(left + x, top + y);
			}
		}
		return DesignFromLevels(levels);
	}
	// The half atlas keeps every second texel of every second row. The mask's rows step through it at half the width of a
	// row, so each odd row of the mask reads half a row further on.
	const auto halfWidth = atlasWidth / 2;
	const auto halfHeight = atlasWidth > 0 ? static_cast<uint32_t>(atlas.size() / atlasWidth) / 2 : 0;
	constexpr auto k_HalfCell = k_DesignSize / 2;
	const auto start = ((design / k_DesignsPerRow) * k_HalfCell * halfWidth) + ((design % k_DesignsPerRow) * k_HalfCell);
	for (uint32_t y = 0; y < k_DesignSize && halfWidth > 0; ++y)
	{
		for (uint32_t x = 0; x < k_DesignSize; ++x)
		{
			const auto at = start + (((halfWidth * y) + x) / 2);
			const auto row = at / halfWidth;
			if (row < halfHeight)
			{
				levels[(y * k_DesignSize) + x] = level((at % halfWidth) * 2, row * 2);
			}
		}
	}
	return DesignFromLevels(levels);
}

Design creature_tattoo::DesignFromLevels(std::span<const uint8_t> levels)
{
	Design result {};
	auto& full = result.front();
	full.size = k_DesignSize;
	full.levels.assign(static_cast<size_t>(k_DesignSize) * k_DesignSize, 0);
	std::copy_n(levels.begin(), std::min(levels.size(), full.levels.size()), full.levels.begin());
	for (size_t level = 1; level < result.size(); ++level)
	{
		result.at(level) = Halved(result.at(level - 1));
	}
	return result;
}

Picture creature_tattoo::PictureFromImage(std::span<const std::array<uint8_t, 3>> image, uint32_t width, uint32_t height)
{
	Picture picture {};
	const auto right = std::min(width, k_DesignSize);
	const auto bottom = std::min(height, k_DesignSize);
	for (uint32_t y = 0; y < bottom; ++y)
	{
		for (uint32_t x = 0; x < right; ++x)
		{
			const auto index = (static_cast<size_t>(y) * width) + x;
			if (index >= image.size())
			{
				continue;
			}
			// Each channel goes to the middle of its step of eight, then to the nearest grey, which is the mean rounded
			const auto& colour = image[index];
			const auto sum = (colour[0] & k_StepMask) + (colour[1] & k_StepMask) + (colour[2] & k_StepMask) + k_StepMiddles;
			picture.at((y * k_DesignSize) + x) = static_cast<uint8_t>((sum + 1) / 3);
		}
	}
	return picture;
}

void creature_tattoo::PutPicture(std::span<std::array<uint8_t, 3>> atlas, uint32_t atlasWidth, uint32_t design,
                                 const Picture& picture)
{
	const auto left = (design % k_DesignsPerRow) * k_DesignSize;
	const auto top = (design / k_DesignsPerRow) * k_DesignSize;
	for (uint32_t y = 0; y < k_DesignSize; ++y)
	{
		for (uint32_t x = 0; x < k_DesignSize; ++x)
		{
			const auto index = (static_cast<size_t>(top + y) * atlasWidth) + left + x;
			if (left + x < atlasWidth && index < atlas.size())
			{
				const auto grey = picture.at((y * k_DesignSize) + x);
				atlas[index] = {grey, grey, grey};
			}
		}
	}
}

std::vector<uint8_t> creature_tattoo::PicturesBlock(const Slots& slots, const Pictures& pictures)
{
	const auto usesPicture = [&pictures](const Slot& slot) {
		return slot.site < k_SlotCount && slot.design < pictures.size() && pictures.at(slot.design).has_value();
	};
	uint32_t designs = 0;
	for (const auto& slot : slots)
	{
		if (usesPicture(slot))
		{
			designs |= 1u << slot.design;
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
	std::array<uint32_t, k_DesignCount> offsets {};
	uint32_t next = k_BlockHeader;
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const auto& slot = slots.at(i);
		if (!usesPicture(slot))
		{
			continue;
		}
		block.at(5) = static_cast<uint8_t>(block.at(5) | (1u << i));
		auto& offset = offsets.at(slot.design);
		if (offset == 0)
		{
			// The first slot with the picture puts it in the block, two levels to a byte
			offset = next;
			next += k_PackedPicture;
			const auto& picture = *pictures.at(slot.design);
			for (uint32_t byte = 0; byte < k_PackedPicture; ++byte)
			{
				block.at(offset + byte) =
				    static_cast<uint8_t>((picture.at((byte * 2) + 1) & 0xF0u) | (picture.at(byte * 2) >> 4u));
			}
		}
		putWord(k_OffsetsStart + (i * 4), offset);
	}
	return block;
}

std::optional<Design> creature_tattoo::DesignInBlock(std::span<const uint8_t> block, size_t slot)
{
	if (block.size() < k_BlockHeader || slot >= k_SlotCount || ((block[5] >> slot) & 1u) == 0)
	{
		return std::nullopt;
	}
	uint32_t offset = 0;
	for (size_t i = 0; i < 4; ++i)
	{
		offset |= static_cast<uint32_t>(block[k_OffsetsStart + (slot * 4) + i]) << (8 * i);
	}
	// A block that does not hold all of the picture gives none
	if (offset < k_BlockHeader || offset > block.size() || block.size() - offset < k_PackedPicture)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> levels(static_cast<size_t>(k_DesignSize) * k_DesignSize);
	for (uint32_t byte = 0; byte < k_PackedPicture; ++byte)
	{
		const auto packed = block[offset + byte];
		levels.at(byte * 2) = packed & 0x0Fu;
		levels.at((byte * 2) + 1) = packed >> 4u;
	}
	return DesignFromLevels(levels);
}

size_t creature_tattoo::MaskLevel(float size)
{
	// Truncated, as the game converts it
	const auto steps = static_cast<int32_t>(std::min(size, 1.0f) * k_SizeSteps);
	for (size_t level = 0; level < k_LevelThresholds.size(); ++level)
	{
		if (steps >= k_LevelThresholds.at(level))
		{
			return level;
		}
	}
	return k_MaskLevels - 1;
}

Mask creature_tattoo::Oriented(const Mask& mask, uint8_t rotation, bool mirror)
{
	Mask result {.size = mask.size, .levels = std::vector<uint8_t>(mask.levels.size(), 0)};
	const auto last = mask.size - 1;
	for (uint32_t y = 0; y < mask.size; ++y)
	{
		for (uint32_t x = 0; x < mask.size; ++x)
		{
			// Where the texel at (x, y) of the turned mask comes from
			uint32_t fromX = x;
			uint32_t fromY = y;
			switch (rotation & 3u)
			{
			case 1:
				fromX = y;
				fromY = last - x;
				break;
			case 2:
				fromX = last - x;
				fromY = last - y;
				break;
			case 3:
				fromX = last - y;
				fromY = x;
				break;
			default:
				break;
			}
			const auto toX = mirror ? last - x : x;
			result.levels[(y * mask.size) + toX] = mask.At(fromX, fromY);
		}
	}
	return result;
}

uint16_t creature_tattoo::PaintTexel(uint16_t skin, uint8_t level, const glm::u8vec3& colour)
{
	const uint32_t m = std::min(level, k_MaxLevel);
	const uint32_t keep = k_MaxLevel - m;
	const auto blue = (((skin & k_Blue) * keep) + (m * (colour.b >> 4u))) / k_MaxLevel;
	const auto green = ((((skin & k_Green) * keep) + (m * (colour.g & 0xF0u))) / k_MaxLevel) & k_Green;
	const auto red = ((((skin & k_Red) * keep) + (m * (colour.r & 0xF0u) * 16u)) / k_MaxLevel) & k_Red;
	return static_cast<uint16_t>(red | green | blue | k_Opaque);
}

void creature_tattoo::Paint(std::span<uint16_t> skin, const Design& design, const glm::u8vec3& colour, const Site& site)
{
	assert(skin.size() == static_cast<size_t>(k_SkinSize) * k_SkinSize);
	if (site.size < 0.0f)
	{
		return;
	}
	const auto level = MaskLevel(std::min(site.size, 1.0f));
	const auto mask = Oriented(design.at(level), site.rotation, site.mirror);
	// The size doubles for each halving of the mask, so the tattoo spans the size times the largest mask's width
	auto scaled = std::min(site.size, 1.0f);
	for (size_t i = 0; i < level; ++i)
	{
		scaled += scaled;
	}
	const auto extent = static_cast<int32_t>(static_cast<float>(mask.size) * scaled);
	if (extent <= 0 || mask.size == 0)
	{
		return;
	}
	const auto half = extent / 2;
	const auto left = static_cast<int32_t>(site.u) - half;
	const auto top = static_cast<int32_t>(site.v) - half;
	constexpr auto k_Size = static_cast<int32_t>(k_SkinSize);
	if (left < 0 || top < 0 || site.u + half >= k_Size || site.v + half >= k_Size)
	{
		return;
	}
	for (int32_t y = 0; y < extent; ++y)
	{
		const auto maskY = static_cast<uint32_t>((y * static_cast<int32_t>(mask.size)) / extent);
		for (int32_t x = 0; x < extent; ++x)
		{
			const auto maskX = static_cast<uint32_t>((x * static_cast<int32_t>(mask.size)) / extent);
			const auto index = static_cast<size_t>(((top + y) * k_Size) + left + x);
			if (index >= skin.size())
			{
				continue;
			}
			auto& texel = skin[index];
			texel = PaintTexel(texel, mask.At(maskX, maskY), colour);
		}
	}
}

glm::u8vec3 creature_tattoo::Brightened(const glm::u8vec3& colour, float brightness)
{
	const glm::ivec3 channels {colour};
	glm::ivec3 result;
	if (brightness >= 0.5f)
	{
		// Towards white
		const auto towards = static_cast<int32_t>((brightness - 0.5f) * 2.0f * 256.0f);
		result = channels + (((glm::ivec3(255) - channels) * towards) / 256);
	}
	else
	{
		// Towards black
		const auto factor = static_cast<int32_t>(brightness * 2.0f * 256.0f);
		result = (channels * factor) / 256;
	}
	return glm::u8vec3(glm::clamp(result, 0, 255));
}

glm::u8vec3 creature_tattoo::PaletteColour(std::span<const std::array<uint8_t, 3>> palette, uint32_t column, uint32_t row,
                                           float brightness)
{
	const auto index =
	    (static_cast<size_t>(std::min(row, k_PaletteRows - 1)) * k_PaletteColumns) + std::min(column, k_PaletteColumns - 1);
	if (index >= palette.size())
	{
		return glm::u8vec3(255);
	}
	return Brightened({palette[index][0], palette[index][1], palette[index][2]}, brightness);
}

std::optional<size_t> creature_tattoo::SlotFor(const Slots& slots, uint8_t site, uint8_t design)
{
	const auto index = [&slots](auto&& predicate) -> std::optional<size_t> {
		const auto found = std::ranges::find_if(slots, predicate);
		return found != slots.end() ? std::optional(static_cast<size_t>(std::distance(slots.begin(), found))) : std::nullopt;
	};
	if (const auto same = index([site, design](const Slot& slot) { return slot.site == site && slot.design == design; }))
	{
		return same;
	}
	if (const auto empty = index([](const Slot& slot) { return slot.Empty(); }))
	{
		return empty;
	}
	return index([site](const Slot& slot) { return slot.site == site; });
}

Slot creature_tattoo::FromWord(uint32_t word)
{
	return {
	    .design = static_cast<uint8_t>(word & 0xFu),
	    .site = static_cast<uint8_t>((word >> 4u) & 0xFu),
	    .colour = {static_cast<uint8_t>(word >> 24u), static_cast<uint8_t>(word >> 16u), static_cast<uint8_t>(word >> 8u)},
	};
}

uint32_t creature_tattoo::ToWord(const Slot& slot)
{
	return (slot.design & 0xFu) | ((slot.site & 0xFu) << 4u) | (static_cast<uint32_t>(slot.colour.b) << 8u) |
	       (static_cast<uint32_t>(slot.colour.g) << 16u) | (static_cast<uint32_t>(slot.colour.r) << 24u);
}
