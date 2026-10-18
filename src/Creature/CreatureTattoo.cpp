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
#include <utility>

#include <MindBody.h>

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
	std::array<std::optional<uint8_t>, k_SlotCount> slotDesigns {};
	for (size_t i = 0; i < slots.size(); ++i)
	{
		if (slots.at(i).site < k_SlotCount)
		{
			slotDesigns.at(i) = slots.at(i).design;
		}
	}
	// The block keeps the top four bits of each grey
	std::array<std::optional<creaturemind::TattooPicture>, k_DesignCount> levels {};
	for (size_t design = 0; design < pictures.size(); ++design)
	{
		if (pictures.at(design).has_value())
		{
			auto& picture = levels.at(design).emplace();
			std::ranges::transform(*pictures.at(design), picture.begin(),
			                       [](uint8_t grey) { return static_cast<uint8_t>(grey >> 4u); });
		}
	}
	return creaturemind::WriteTattooPictures(slotDesigns, levels);
}

std::optional<Design> creature_tattoo::DesignInBlock(std::span<const uint8_t> block, size_t slot)
{
	const auto picture = creaturemind::ReadTattooPicture(block, slot);
	if (!picture.has_value())
	{
		return std::nullopt;
	}
	return DesignFromLevels(*picture);
}

void creature_tattoo::RemakeMasks(Masks& masks, const Slots& slots, std::span<const uint8_t> block,
                                  std::span<const Design, k_DesignCount> sheet)
{
	if (block.empty())
	{
		return;
	}
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const auto& slot = slots.at(i);
		if (slot.Empty())
		{
			continue;
		}
		auto own = DesignInBlock(block, i);
		masks.at(i) = own.has_value() ? std::move(own) : std::optional(sheet[slot.design % k_DesignCount]);
	}
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
	// The game moves along the mask by a step in float for each texel and truncates the sum. The sum can differ from
	// the exact product, so a texel can come from the next texel of the mask.
	const auto step = static_cast<float>(mask.size) / static_cast<float>(extent);
	float maskY = 0.0f;
	for (int32_t y = 0; y < extent; ++y)
	{
		const auto row = static_cast<uint32_t>(maskY) * mask.size;
		float maskX = 0.0f;
		for (int32_t x = 0; x < extent; ++x)
		{
			const auto from = row + static_cast<uint32_t>(maskX);
			maskX += step;
			const auto index = static_cast<size_t>(((top + y) * k_Size) + left + x);
			// A sum at the end of a row reads the first texel of the next row. A texel after the mask stays as it is.
			if (index >= skin.size() || from >= mask.levels.size())
			{
				continue;
			}
			auto& texel = skin[index];
			texel = PaintTexel(texel, mask.levels[from], colour);
		}
		maskY += step;
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
	const auto tattoo = creaturemind::TattooFromWord(word);
	return {.design = tattoo.design, .site = tattoo.site, .colour = {tattoo.red, tattoo.green, tattoo.blue}};
}

uint32_t creature_tattoo::ToWord(const Slot& slot)
{
	return creaturemind::TattooToWord(
	    {.design = slot.design, .site = slot.site, .red = slot.colour.r, .green = slot.colour.g, .blue = slot.colour.b});
}
