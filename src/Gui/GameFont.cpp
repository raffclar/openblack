/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameFont.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <utility>

#include "TextDatabase.h"
#include "TextWrap.h"

using namespace openblack::gui;

namespace
{
constexpr size_t k_NameOffset = 4;
constexpr size_t k_NameSize = 0x100;
constexpr size_t k_CountOffset = k_NameOffset + k_NameSize;
constexpr size_t k_GlyphsOffset = k_CountOffset + 4;
constexpr size_t k_GlyphSize = 0x1C;
/// Width of the atlas and the clear pixels around each glyph in it
constexpr uint16_t k_AtlasWidth = 1024;
constexpr uint16_t k_FullAtlasWidth = 2048;
constexpr uint16_t k_Padding = 1;

/// The smallest power of two at least as big
uint16_t PowerOfTwoAtLeast(uint32_t value)
{
	uint16_t power = 1;
	while (power < value)
	{
		power = static_cast<uint16_t>(power * 2);
	}
	return power;
}

template <typename T>
T Read(std::span<const uint8_t> data, size_t offset)
{
	T value;
	std::memcpy(&value, data.data() + offset, sizeof(T));
	return value;
}

/// A glyph's bitmap, a byte a pixel, set pixels 1
std::optional<std::vector<uint8_t>> DecodeBitmap(std::span<const uint8_t> runs, size_t pixels)
{
	std::vector<uint8_t> bitmap(pixels, 0);
	size_t position = 0;
	size_t i = 0;
	bool set = false;
	while (position < pixels && i < runs.size())
	{
		size_t run = runs[i++];
		if (run == 0xFF)
		{
			if (i + 2 > runs.size())
			{
				return std::nullopt;
			}
			run = runs[i] | (runs[i + 1] << 8);
			i += 2;
		}
		run = std::min(run, pixels - position);
		if (set)
		{
			std::fill_n(bitmap.begin() + static_cast<ptrdiff_t>(position), run, uint8_t {1});
		}
		position += run;
		set = !set;
	}
	return bitmap;
}

/// The alpha of a pixel of the half height glyph, by how many of the four pixels it is made of are set
constexpr std::array<uint8_t, 5> k_HalfGlyphAlpha {0x0, 0x4, 0x8, 0xC, 0xF};
} // namespace

GameFont::HalfGlyph GameFont::MakeHalfGlyph(std::span<const uint8_t> bitmap, uint16_t width)
{
	// The game reads the bitmap with two clear rows after it. Past them it reads memory that is not part of the bitmap;
	// here all of that reads as clear.
	const auto set = [bitmap](size_t i) -> uint32_t { return i < bitmap.size() && bitmap[i] != 0 ? 1 : 0; };
	HalfGlyph half {.width = static_cast<uint16_t>((width + 1) / 2), .count = {}};
	half.count.assign(static_cast<size_t>(half.width) * HalfGlyph::k_Height, 0);
	// Each row of the half glyph starts where the last one stopped plus one bitmap row. A glyph of an odd width has
	// one column more than half its bitmap, so each row starts one pixel further on in the bitmap than the row before.
	size_t rowStart = 0;
	for (uint16_t y = 0; y < HalfGlyph::k_Height; ++y)
	{
		size_t at = rowStart;
		for (uint16_t x = 0; x < half.width; ++x, at += 2)
		{
			const auto bottom = at + width;
			const auto count = set(at) + set(at + 1) + set(bottom) + set(bottom + 1);
			half.count[(static_cast<size_t>(y) * half.width) + x] = static_cast<uint8_t>(count);
		}
		rowStart = at + width;
	}
	return half;
}

uint8_t GameFont::HalfGlyphAlpha(uint8_t count)
{
	return k_HalfGlyphAlpha.at(count);
}

GameFont::SmallGlyph GameFont::MakeSmallGlyph(const HalfGlyph& half, uint16_t width)
{
	// The half height glyph as the game keeps it: its alpha from 0 to 15 with a clear column either side
	const auto halfAt = [&half](int32_t x, int32_t y) -> uint32_t {
		const auto column = x - 1;
		return column < 0 || column >= half.width || y < 0 || y >= HalfGlyph::k_Height
		           ? 0
		           : HalfGlyphAlpha(half.count[(static_cast<size_t>(y) * half.width) + column]);
	};

	// The small glyph at a quarter of the height, each pixel's alpha the average of four of the half height glyph's
	SmallGlyph small {.width = static_cast<uint16_t>(((width + 3) >> 2) + 2), .alpha = {}};
	small.alpha.assign(static_cast<size_t>(small.width) * SmallGlyph::k_Height, 0);
	for (int32_t y = 0; y < SmallGlyph::k_Height; ++y)
	{
		for (int32_t x = 0; x + 2 < small.width; ++x)
		{
			const int32_t column = 1 + (x * 2);
			const auto sum = halfAt(column, y * 2) + halfAt(column + 1, y * 2) + halfAt(column, (y * 2) + 1) +
			                 halfAt(column + 1, (y * 2) + 1);
			small.alpha[(static_cast<size_t>(y) * small.width) + 1 + x] = static_cast<uint8_t>(sum >> 2);
		}
	}
	return small;
}

std::optional<GameFont> GameFont::Load(std::span<const uint8_t> met, std::span<const uint8_t> fnt)
{
	if (met.size() < k_GlyphsOffset)
	{
		return std::nullopt;
	}
	GameFont font;
	font._height = static_cast<uint16_t>(Read<uint32_t>(met, 0));
	const auto count = Read<uint32_t>(met, k_CountOffset);
	if (font._height == 0 || font._height > 1024 || met.size() < k_GlyphsOffset + (count * k_GlyphSize))
	{
		return std::nullopt;
	}
	std::u16string name;
	for (size_t i = k_NameOffset; i + 1 < k_CountOffset; i += 2)
	{
		const auto c = static_cast<char16_t>(met[i] | (met[i + 1] << 8));
		if (c == u'\0')
		{
			break;
		}
		name.push_back(c);
	}
	font._name = ToUtf8(name);

	// The glyphs at half height, each pixel's alpha by how many of four pixels are set, packed in rows into the atlas
	const auto cellHeight = static_cast<uint16_t>((font._height + 1) / 2);
	std::vector<std::vector<uint8_t>> cells;
	std::vector<std::vector<uint8_t>> bitmaps;
	glm::u16vec2 pen {0, 0};
	glm::u16vec2 fullPen {0, 0};
	for (uint32_t g = 0; g < count; ++g)
	{
		const auto record = k_GlyphsOffset + (g * k_GlyphSize);
		Glyph glyph {
		    .character = static_cast<char16_t>(Read<uint16_t>(met, record)),
		    .width = Read<uint16_t>(met, record + 2),
		    .left = Read<float>(met, record + 8),
		    .ink = Read<float>(met, record + 0xC),
		    .right = Read<float>(met, record + 0x10),
		    .atlasMin = {},
		    .atlasMax = {},
		    .fullMin = {},
		    .fullMax = {},
		    .smallGlyph = static_cast<uint32_t>(font._smallGlyphs.size()),
		};
		const auto offset = Read<uint32_t>(met, record + 0x14);
		const auto length = Read<uint32_t>(met, record + 0x18);
		if (offset > fnt.size() || length > fnt.size() - offset)
		{
			return std::nullopt;
		}
		const auto bitmap = DecodeBitmap(fnt.subspan(offset, length), static_cast<size_t>(glyph.width) * font._height);
		if (!bitmap)
		{
			return std::nullopt;
		}

		const auto half = MakeHalfGlyph(*bitmap, glyph.width);
		font._smallGlyphs.push_back(MakeSmallGlyph(half, glyph.width));

		const auto cellWidth = half.width;
		std::vector<uint8_t> cell(static_cast<size_t>(cellWidth) * cellHeight, 0);
		for (uint16_t y = 0; y < std::min(cellHeight, HalfGlyph::k_Height); ++y)
		{
			for (uint16_t x = 0; x < cellWidth; ++x)
			{
				// The game keeps the glyph's alpha in four bits, read back as n fifteenths of 255
				cell[(static_cast<size_t>(y) * cellWidth) + x] =
				    static_cast<uint8_t>(HalfGlyphAlpha(half.count[(static_cast<size_t>(y) * cellWidth) + x]) * 17);
			}
		}

		if (pen.x + cellWidth + (k_Padding * 2) > k_AtlasWidth)
		{
			pen = {0, pen.y + cellHeight + (k_Padding * 2)};
		}
		glyph.atlasMin = {pen.x + k_Padding, pen.y + k_Padding};
		glyph.atlasMax = {glyph.atlasMin.x + cellWidth, glyph.atlasMin.y + cellHeight};
		pen.x += cellWidth + (k_Padding * 2);

		if (fullPen.x + glyph.width + (k_Padding * 2) > k_FullAtlasWidth)
		{
			fullPen = {0, fullPen.y + font._height + (k_Padding * 2)};
		}
		glyph.fullMin = {fullPen.x + k_Padding, fullPen.y + k_Padding};
		glyph.fullMax = {glyph.fullMin.x + glyph.width, glyph.fullMin.y + font._height};
		fullPen.x += glyph.width + (k_Padding * 2);

		font._glyphs.push_back(glyph);
		cells.push_back(std::move(cell));
		bitmaps.push_back(*bitmap);
	}

	// Power of two heights keep every backend happy
	const auto atlasHeight = PowerOfTwoAtLeast(static_cast<uint32_t>(pen.y + cellHeight + (k_Padding * 2)));
	font._atlasSize = {k_AtlasWidth, atlasHeight};
	font._atlas.assign(static_cast<size_t>(k_AtlasWidth) * atlasHeight, 0);
	for (size_t g = 0; g < font._glyphs.size(); ++g)
	{
		const auto& glyph = font._glyphs[g];
		const auto cellWidth = static_cast<size_t>(glyph.atlasMax.x - glyph.atlasMin.x);
		// Spaces have nothing to draw
		for (uint16_t y = 0; cellWidth > 0 && y < cellHeight; ++y)
		{
			std::memcpy(font._atlas.data() + (static_cast<size_t>(glyph.atlasMin.y + y) * k_AtlasWidth) + glyph.atlasMin.x,
			            cells[g].data() + (y * cellWidth), cellWidth);
		}
	}

	// The full height glyphs: each set pixel of a bitmap opaque
	const auto fullHeight = PowerOfTwoAtLeast(static_cast<uint32_t>(fullPen.y + font._height + (k_Padding * 2)));
	font._fullAtlasSize = {k_FullAtlasWidth, fullHeight};
	font._fullAtlas.assign(static_cast<size_t>(k_FullAtlasWidth) * fullHeight, 0);
	for (size_t g = 0; g < font._glyphs.size(); ++g)
	{
		const auto& glyph = font._glyphs[g];
		for (uint16_t y = 0; y < font._height; ++y)
		{
			for (uint16_t x = 0; x < glyph.width; ++x)
			{
				if (bitmaps[g][(static_cast<size_t>(y) * glyph.width) + x] != 0)
				{
					font._fullAtlas[(static_cast<size_t>(glyph.fullMin.y + y) * k_FullAtlasWidth) + glyph.fullMin.x + x] = 0xFF;
				}
			}
		}
	}

	std::ranges::sort(font._glyphs, {}, &Glyph::character);
	return font;
}

const GameFont::Glyph* GameFont::Find(char16_t character) const
{
	// A character the font hasn't got shows as a question mark
	for (const auto wanted : {character, u'?'})
	{
		const auto found = std::ranges::lower_bound(_glyphs, wanted, {}, &Glyph::character);
		if (found != _glyphs.end() && found->character == wanted)
		{
			return &*found;
		}
	}
	return nullptr;
}

float GameFont::Advance(char16_t c) const
{
	// The blank the temple and the help texts' loader use takes no room; everything else is a glyph, a line break too
	if (c == text_wrap::k_Tilde)
	{
		return 0.0f;
	}
	const auto* glyph = Find(c);
	return glyph != nullptr ? glyph->right + glyph->ink + glyph->left : 0.0f;
}

float GameFont::GetWidth(std::u16string_view text, float size) const
{
	float width = 0.0f;
	for (const auto c : text)
	{
		width += Advance(c);
	}
	return width * size / static_cast<float>(_height);
}

std::vector<std::u16string_view> GameFont::Wrap(std::u16string_view text, float size, float width) const
{
	return text_wrap::BreakLines(text, width, size / static_cast<float>(_height), [this](char16_t c) { return Advance(c); });
}
