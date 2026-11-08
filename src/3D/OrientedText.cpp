/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OrientedText.h"

#include "Gui/GameFont.h"

using namespace openblack;

namespace
{
/// The game reads a glyph in the world from one and a half texels left of its first column
constexpr float k_TexelsBeforeGlyph = 1.5f;
/// From the middle of the first row, 39 rows down: to the middle of the last of the 40 rows
constexpr float k_HalfTexel = 0.5f;
constexpr float k_RowsRead = 39.0f;
} // namespace

GlyphTexels openblack::OrientedGlyphTexels(const gui::GameFont::Glyph& glyph)
{
	const glm::vec2 min {static_cast<float>(glyph.atlasMin.x) - k_TexelsBeforeGlyph,
	                     static_cast<float>(glyph.atlasMin.y) + k_HalfTexel};
	// Half the bitmap's width at full height, and one texel more
	const float across = (static_cast<float>(glyph.width) * 0.5f) + 1.0f;
	return {.min = min, .max = {min.x + across, min.y + k_RowsRead}};
}

int openblack::OrientedTextDepthAxis(int across, int down)
{
	// The game takes the first axis unless across or down is it, then the second unless either is that too
	if (across != 0 && down != 0)
	{
		return 0;
	}
	return across == 1 || down == 1 ? 2 : 1;
}

void openblack::AppendOrientedText(std::vector<OrientedTextVertex>& vertices, const gui::GameFont& font, const TextFrame& frame,
                                   int across, int down, std::u16string_view text, glm::vec3 offset, float size, float stretch,
                                   glm::u8vec4 colour)
{
	if (text.empty() || font.GetHeight() == 0)
	{
		return;
	}
	const auto& alongText = frame.axes.at(static_cast<size_t>(across));
	const auto& downLetters = frame.axes.at(static_cast<size_t>(down));
	const auto& offPage = frame.axes.at(static_cast<size_t>(OrientedTextDepthAxis(across, down)));
	// A font unit is a size high line's height's worth of the font's height, stretched along the text
	const auto unit = alongText * (size / static_cast<float>(font.GetHeight()) * stretch);
	const auto height = downLetters * size;
	auto pen = frame.origin + (alongText * offset.x) + (downLetters * offset.y) + (offPage * offset.z);

	const auto packed = static_cast<uint32_t>(colour.r) | (static_cast<uint32_t>(colour.g) << 8) |
	                    (static_cast<uint32_t>(colour.b) << 16) | (static_cast<uint32_t>(colour.a) << 24);
	const auto atlasSize = glm::vec2(font.GetAtlasSize());
	for (const auto c : text)
	{
		if (c == u'\n' || c == u'\r' || c == u'\xF8FE')
		{
			continue;
		}
		const auto* glyph = font.Find(c);
		if (glyph == nullptr)
		{
			continue;
		}
		if (c == u' ')
		{
			pen += unit * font.Advance(c);
			continue;
		}
		pen += unit * glyph->left;
		const auto topLeft = pen;
		const auto bottomLeft = pen + height;
		const auto topRight = pen + (unit * (glyph->ink + 1.0f));
		const auto bottomRight = topRight + height;
		const auto texels = OrientedGlyphTexels(*glyph);
		const auto uvMin = texels.min / atlasSize;
		const auto uvMax = texels.max / atlasSize;
		vertices.push_back({topLeft, uvMin, packed});
		vertices.push_back({bottomLeft, {uvMin.x, uvMax.y}, packed});
		vertices.push_back({topRight, {uvMax.x, uvMin.y}, packed});
		vertices.push_back({topRight, {uvMax.x, uvMin.y}, packed});
		vertices.push_back({bottomLeft, {uvMin.x, uvMax.y}, packed});
		vertices.push_back({bottomRight, uvMax, packed});
		pen += unit * (glyph->ink + glyph->right);
	}
}
