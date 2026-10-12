/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

namespace openblack::gui
{

/// One of Black & White's fonts (data/j0, f1, f3: a .met of glyph metrics and a .fnt of glyph bitmaps), read the way
/// the game reads them.
///
/// The .met starts with the height of the glyph bitmaps (80 for j0) and the font's name, then has a record for each
/// glyph: its character, the width of its bitmap, its left bearing, inked width and right bearing, and where its
/// bitmap is in the .fnt. A bitmap is one bit a pixel, row by row, run length encoded: runs of clear and set pixels
/// take turns, starting with clear, each a byte or 0xFF and then two bytes.
///
/// The game draws text from a cache of the glyphs at half their height, each pixel the average of four, and
/// scales them to the size of the text: a size is the height of a line in pixels. The glyphs here go into an atlas
/// at that half height in the same way.
class GameFont
{
public:
	struct Glyph
	{
		char16_t character;
		/// Width of the bitmap at the full height
		uint16_t width;
		/// The metrics at the full height: the bitmap starts left of the pen, and the pen moves on left + ink + right
		float left;
		float ink;
		float right;
		/// Where the glyph is in the atlas, in pixels
		glm::u16vec2 atlasMin;
		glm::u16vec2 atlasMax;
		/// Where the glyph is at its full height in the full atlas, in pixels
		glm::u16vec2 fullMin;
		glm::u16vec2 fullMax;
		/// Its glyph in GetSmallGlyph's cache
		uint32_t smallGlyph;
	};

	/// The game's second level of a glyph, which it draws text smaller than
	/// 26 pixels into textures from: the glyph at a quarter of its height, the alpha of each pixel from 0 to 15, with a
	/// clear column either side
	struct SmallGlyph
	{
		static constexpr uint16_t k_Height = 20;
		uint16_t width;
		/// Row by row
		std::vector<uint8_t> alpha;
		/// 0 outside of the glyph
		[[nodiscard]] uint8_t At(int32_t x, int32_t y) const
		{
			return x < 0 || y < 0 || x >= width || y >= k_Height ? uint8_t {0} : alpha[(static_cast<size_t>(y) * width) + x];
		}
	};

	/// Null when the files don't fit together
	static std::optional<GameFont> Load(std::span<const uint8_t> met, std::span<const uint8_t> fnt);

	[[nodiscard]] const std::string& GetName() const noexcept { return _name; }
	/// Height of the glyph bitmaps
	[[nodiscard]] uint16_t GetHeight() const noexcept { return _height; }
	/// The glyph of a character, or of '?' when the font has none, null if it has neither
	[[nodiscard]] const Glyph* Find(char16_t character) const;
	[[nodiscard]] const std::vector<Glyph>& GetGlyphs() const noexcept { return _glyphs; }

	/// How far a character moves the pen in the font's units: left bearing, ink and right bearing of its glyph (or of
	/// '?'). U+F8FE, the blank the temple and the help texts use, takes no room. Line breaks are glyphs like any other.
	[[nodiscard]] float Advance(char16_t c) const;
	/// How far text of a size moves the pen, as the game measures it: every character's advance, line breaks included
	[[nodiscard]] float GetWidth(std::u16string_view text, float size) const;
	/// The game's line breaking (gui::text_wrap::BreakLines): lines that fit in width at a size
	[[nodiscard]] std::vector<std::u16string_view> Wrap(std::u16string_view text, float size, float width) const;

	/// Coverage of the glyphs, one byte a pixel
	[[nodiscard]] const std::vector<uint8_t>& GetAtlas() const noexcept { return _atlas; }
	[[nodiscard]] glm::u16vec2 GetAtlasSize() const noexcept { return _atlasSize; }
	/// The glyphs at their full height, a pixel of the bitmap a pixel: for text drawn bigger than the game ever drew it
	/// on its default screen, so that scaled up text stays sharp
	[[nodiscard]] const std::vector<uint8_t>& GetFullAtlas() const noexcept { return _fullAtlas; }
	[[nodiscard]] glm::u16vec2 GetFullAtlasSize() const noexcept { return _fullAtlasSize; }
	[[nodiscard]] const SmallGlyph& GetSmallGlyph(const Glyph& glyph) const { return _smallGlyphs[glyph.smallGlyph]; }

private:
	std::string _name;
	uint16_t _height {0};
	/// By character
	std::vector<Glyph> _glyphs;
	std::vector<uint8_t> _atlas;
	glm::u16vec2 _atlasSize {0, 0};
	std::vector<uint8_t> _fullAtlas;
	glm::u16vec2 _fullAtlasSize {0, 0};
	std::vector<SmallGlyph> _smallGlyphs;
};

} // namespace openblack::gui
