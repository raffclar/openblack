/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameFont.h"

#include <cstring>

#include <array>

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "Graphics/Argb4444.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/Texture2D.h"

using namespace openblack::graphics;

namespace
{
template <typename T>
T Read(const std::vector<uint8_t>& data, size_t offset)
{
	T value {};
	if (offset + sizeof(T) <= data.size())
	{
		std::memcpy(&value, &data[offset], sizeof(T));
	}
	return value;
}

// the number of set pixels of a 2 x 2 block -> alpha of the ARGB4444 texel (n / 15)
constexpr std::array<uint8_t, 5> k_AlphaTable = {0x0, 0x4, 0x8, 0xC, 0xF};
constexpr uint32_t k_LineRows = 64; // 40 rows full resolution + 20 rows quarter resolution, per line
} // namespace

GameFont::GameFont() = default;
GameFont::~GameFont() = default;

bool GameFont::Load(const std::vector<uint8_t>& met, const std::vector<uint8_t>& fnt, std::string_view name)
{
	if (met.size() < 264 || fnt.empty() || Read<uint32_t>(met, 0) != k_CellHeight)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Font {}: missing or not a font", name);
		return false;
	}
	// .met: u32 cell height, wchar[128] name, u32 count, then 28-byte records {u16 code, u16 bitmap width, s16, u16,
	// f32 left, f32 width, f32 right, u32 .fnt offset, u32 .fnt size}
	const auto count = Read<uint32_t>(met, 260);
	struct Record
	{
		uint16_t code, width;
		float left, advance, right;
		uint32_t offset, size;
	};
	std::vector<Record> records;
	for (uint32_t i = 0; i < count && 264 + 28 * (i + 1) <= met.size(); ++i)
	{
		const size_t at = 264 + 28 * i;
		records.push_back({Read<uint16_t>(met, at), Read<uint16_t>(met, at + 2), Read<float>(met, at + 8),
		                   Read<float>(met, at + 12), Read<float>(met, at + 16), Read<uint32_t>(met, at + 20),
		                   Read<uint32_t>(met, at + 24)});
	}

	// Atlas: lines of 64 texels, each glyph a slot of bitmapWidth / 2 + 2 texels (like the 256 x 256 cache pages,
	// only wider so that every glyph fits at once)
	constexpr uint32_t k_AtlasWidth = 1024;
	uint32_t x = 0;
	uint32_t line = 0;
	for (const auto& record : records)
	{
		const uint32_t slot = record.width / 2u + 2u;
		if (x + slot > k_AtlasWidth)
		{
			++line;
			x = 0;
		}
		// The metrics as the file has them, the half height glyph's place in the atlas, and the quarter height glyph
		// with its clear column either side
		const auto code = static_cast<char16_t>(record.code);
		const auto smallWidth = static_cast<uint16_t>(((record.width + 3u) / 4u) + 2u);
		_glyphs[code] = {
		    .metrics =
		        {
		            .character = code,
		            .width = record.width,
		            .left = record.left,
		            .ink = record.advance,
		            .right = record.right,
		            .atlasMin = glm::u16vec2(x + 1u, line * k_LineRows),
		            .atlasMax = glm::u16vec2(x + 1u + ((record.width + 1u) / 2u), (line * k_LineRows) + (k_CellHeight / 2u)),
		        },
		    .slotX = x,
		    .line = line,
		    .smallWidth = smallWidth,
		    .smallAlpha = std::vector<uint8_t>(static_cast<size_t>(smallWidth) * SmallGlyph::k_Height, 0),
		};
		if (record.code == u' ')
		{
			_spaceAdvance = record.left + record.advance + record.right;
		}
		x += slot;
	}
	uint32_t height = 64;
	while (height < (line + 1) * k_LineRows)
	{
		height *= 2;
	}
	_atlasSize = {k_AtlasWidth, height};
	std::vector<uint8_t> alpha(static_cast<size_t>(k_AtlasWidth) * height, 0);
	const auto put = [&](uint32_t tx, uint32_t ty, uint8_t nibble) {
		if (tx < k_AtlasWidth && ty < height)
		{
			alpha[static_cast<size_t>(ty) * k_AtlasWidth + tx] = argb4444::Expand(nibble);
		}
	};

	for (const auto& record : records)
	{
		auto& glyph = _glyphs[static_cast<char16_t>(record.code)];
		const uint32_t w = record.width;
		if (w == 0)
		{
			continue;
		}
		// .fnt: 1-bit run lengths, alternating 0 / 1 from 0; a byte, or 0xFF + u16
		std::vector<uint8_t> bits(static_cast<size_t>(w) * (k_CellHeight + 2), 0);
		const size_t total = static_cast<size_t>(w) * k_CellHeight;
		size_t pos = 0;
		size_t p = record.offset;
		const size_t end = std::min<size_t>(fnt.size(), static_cast<size_t>(record.offset) + record.size);
		uint8_t value = 0;
		while (pos < total && p < end)
		{
			size_t run = fnt[p++];
			if (run == 0xFF && p + 1 < end + 1)
			{
				run = static_cast<size_t>(fnt[p]) | (static_cast<size_t>(fnt[p + 1]) << 8);
				p += 2;
			}
			run = std::min(run, total - pos);
			if (value != 0)
			{
				std::fill_n(bits.begin() + static_cast<std::ptrdiff_t>(pos), run, uint8_t {1});
			}
			pos += run;
			value ^= 1;
		}
		// rows 0..39: a transparent texel, (w + 1) / 2 texels of 2 x 2 blocks, a transparent texel
		const uint32_t top = glyph.line * k_LineRows;
		const uint32_t half = (w + 1) / 2;
		std::vector<uint8_t> level0(static_cast<size_t>(k_CellHeight / 2) * (half + 2), 0);
		for (uint32_t r = 0; r < k_CellHeight / 2; ++r)
		{
			for (uint32_t k = 0; k < half; ++k)
			{
				const auto bit = [&](uint32_t row, uint32_t col) {
					return col < w ? bits[static_cast<size_t>(row) * w + col] : uint8_t {0};
				};
				const uint32_t n =
				    bit(2 * r, 2 * k) + bit(2 * r, 2 * k + 1) + bit(2 * r + 1, 2 * k) + bit(2 * r + 1, 2 * k + 1);
				const auto a = k_AlphaTable.at(n);
				level0[static_cast<size_t>(r) * (half + 2) + k + 1] = a;
				put(glyph.slotX + 1 + k, top + r, a);
			}
		}
		// rows 40..59: quarter resolution, the sum of four level-0 nibbles >> 2
		const uint32_t quarter = (w + 3) / 4;
		for (uint32_t r = 0; r < 20; ++r)
		{
			for (uint32_t k = 0; k < quarter; ++k)
			{
				uint32_t sum = 0;
				for (const auto& [rr, cc] : std::array<std::pair<uint32_t, uint32_t>, 4> {
				         {{2 * r, 1 + 2 * k}, {2 * r, 2 + 2 * k}, {2 * r + 1, 1 + 2 * k}, {2 * r + 1, 2 + 2 * k}}})
				{
					sum += cc < half + 2 ? level0[static_cast<size_t>(rr) * (half + 2) + cc] : 0u;
				}
				const auto coverage = static_cast<uint8_t>(std::min(sum >> 2, 15u));
				put(glyph.slotX + 1 + k, top + 40 + r, coverage);
				glyph.smallAlpha[(static_cast<size_t>(r) * glyph.smallWidth) + 1 + k] = coverage;
			}
		}
	}
	_texture = std::make_unique<Texture2D>(std::string(name));
	// bilinear, no mips (the cache pages); R8 coverage, the colour comes from the vertices
	_texture->Create(static_cast<uint16_t>(k_AtlasWidth), static_cast<uint16_t>(height), 1, TextureFormat::R8,
	                 Wrapping::ClampEdge, Filter::Linear, bgfx::copy(alpha.data(), static_cast<uint32_t>(alpha.size())));
	SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Font {}: {} glyphs", name, _glyphs.size());
	return true;
}

const GameFont::GlyphMetrics* GameFont::FindGlyph(char16_t character) const
{
	const auto* glyph = Find(character);
	return glyph != nullptr ? &glyph->metrics : nullptr;
}

GameFont::SmallGlyph GameFont::GetSmallGlyph(const GlyphMetrics& glyph) const
{
	const auto found = _glyphs.find(glyph.character);
	if (found == _glyphs.end())
	{
		return {.width = 0, .alpha = {}};
	}
	return {.width = found->second.smallWidth, .alpha = found->second.smallAlpha};
}

const GameFont::Glyph* GameFont::Find(char16_t code) const
{
	if (const auto found = _glyphs.find(code); found != _glyphs.end())
	{
		return &found->second;
	}
	// a missing character is drawn as '?'
	const auto question = _glyphs.find(u'?');
	return question != _glyphs.end() ? &question->second : nullptr;
}

float GameFont::GetStringWidth(std::u16string_view text, float size) const
{
	float width = 0.0f;
	for (const auto c : text)
	{
		if (c == u'\n' || c == u'\r' || c == char16_t {0xF8FE})
		{
			continue;
		}
		if (c == u' ')
		{
			width += _spaceAdvance;
			continue;
		}
		if (const auto* glyph = Find(c); glyph != nullptr)
		{
			width += glyph->metrics.left + glyph->metrics.ink + glyph->metrics.right;
		}
	}
	return width * size / static_cast<float>(k_CellHeight);
}

void GameFont::AddText(std::vector<Vertex>& out, const std::u16string& text, float x, float y, float size,
                       const glm::vec4& rgba, float clipTop, float clipBottom) const
{
	if (!IsLoaded())
	{
		return;
	}
	const float s = size / static_cast<float>(k_CellHeight);
	const bool small = size < 26.0f;
	// the height clip: above clipTop the cut goes off the top (y = clipTop, h -= cut); past clipBottom
	// h = clipBottom - y; nothing when h <= 0. The v range of the cell is cut in the same ratio of the
	// unclipped size: vTop = cut / size * vext, vBottom = (size - over) / size * vext
	float top = y;
	float height = size;
	float cut = 0.0f;
	float kept = size;
	if (top < clipTop)
	{
		cut = clipTop - top;
		top = clipTop;
		height -= cut;
	}
	if (top + height > clipBottom)
	{
		kept -= top + height - clipBottom;
		height = clipBottom - top;
	}
	if (height <= 0.0f)
	{
		return;
	}
	const uint32_t abgr = argb_colour::ToAbgr(rgba);
	const auto w = static_cast<float>(_atlasSize.x);
	const auto h = static_cast<float>(_atlasSize.y);
	float pen = 0.0f;
	for (const auto c : text)
	{
		if (c == u'\n' || c == u'\r' || c == char16_t {0xF8FE})
		{
			continue;
		}
		if (c == u' ')
		{
			pen += _spaceAdvance * s;
			continue;
		}
		const auto* glyph = Find(c);
		if (glyph == nullptr)
		{
			continue;
		}
		// X0 = x + pen + left s, X1 = X0 + (bitmapWidth + 2) s, the full cell height; u over (bitmapWidth + 2) / 2 texels
		// (/ 4 small), v over 39 texels from the line (19.5 from row 40 small)
		const float x0 = x + pen + glyph->metrics.left * s;
		const float x1 = x0 + (static_cast<float>(glyph->metrics.width) + 2.0f) * s;
		const float y0 = top;
		const float y1 = top + height;
		const float u0 = (static_cast<float>(glyph->slotX) + 0.5f) / w;
		const float u1 = u0 + (static_cast<float>(glyph->metrics.width) + 2.0f) / (small ? 4.0f : 2.0f) / w;
		const float vBase = (static_cast<float>(glyph->line * k_LineRows) + (small ? 40.0f : 0.0f) + 0.5f) / h;
		const float vExtent = (small ? 19.5f : 39.0f) / h;
		const float v0 = vBase + cut / size * vExtent;
		const float v1 = vBase + kept / size * vExtent;
		for (const auto& [px, py, pu, pv] :
		     {std::array {x0, y0, u0, v0}, std::array {x1, y0, u1, v0}, std::array {x1, y1, u1, v1},
		      std::array {x0, y0, u0, v0}, std::array {x1, y1, u1, v1}, std::array {x0, y1, u0, v1}})
		{
			out.push_back({px, py, pu, pv, abgr});
		}
		pen += (glyph->metrics.left + glyph->metrics.ink + glyph->metrics.right) * s;
	}
}
