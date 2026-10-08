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

#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace openblack::graphics
{
class Texture2D;

/// A font of the original (Data\j0 / f1 / f3: .met metrics + .fnt glyphs), rasterised like the original's glyph
/// cache and laid out like its text drawing.
class GameFont
{
public:
	struct Vertex
	{
		float x, y, u, v; ///< pixels from the top left of the screen, atlas uv
		uint32_t abgr;
	};

	GameFont();
	~GameFont();

	/// Parses a font's .met (glyph metrics) and .fnt (glyph bitmaps); false if they are not a font. `name` names its
	/// texture and the log lines
	bool Load(const std::vector<uint8_t>& met, const std::vector<uint8_t>& fnt, std::string_view name);
	[[nodiscard]] bool IsLoaded() const { return _texture != nullptr; }
	[[nodiscard]] const Texture2D& GetTexture() const { return *_texture; }

	/// The sum of (left + width + right) x size / 80
	[[nodiscard]] float GetStringWidth(std::u16string_view text, float size) const;
	/// Two triangles per glyph at (x, y) (top left) of `size` pixels; rgba 0..1. clipTop / clipBottom: its height
	/// clip (the help text passes the box's top and bottom)
	void AddText(std::vector<Vertex>& out, const std::u16string& text, float x, float y, float size, const glm::vec4& rgba,
	             float clipTop = -std::numeric_limits<float>::max(),
	             float clipBottom = std::numeric_limits<float>::max()) const;

	/// What a glyph is, to read: its character, the width of its bitmap at the full height, and its left bearing,
	/// inked width and right bearing at the full height (the pen moves on left + ink + right). atlasMin and atlasMax
	/// are where its glyph at half height is in the texture, in texels.
	struct GlyphMetrics
	{
		char16_t character;
		uint16_t width;
		float left;
		float ink;
		float right;
		glm::u16vec2 atlasMin;
		glm::u16vec2 atlasMax;
	};
	/// The glyph text draws a character with: its own, or '?' for one the font has not; null with neither
	[[nodiscard]] const GlyphMetrics* FindGlyph(char16_t character) const;

	/// A glyph at a quarter of its height, which text smaller than 26 pixels is drawn from: each texel's coverage from
	/// 0 to 15, row by row, with a clear column either side. It views the font's own copy.
	struct SmallGlyph
	{
		static constexpr uint16_t k_Height = 20;
		uint16_t width;
		std::span<const uint8_t> alpha;
		/// 0 outside of the glyph
		[[nodiscard]] uint8_t At(int32_t x, int32_t y) const
		{
			return x < 0 || y < 0 || x >= width || y >= k_Height ? uint8_t {0} : alpha[(static_cast<size_t>(y) * width) + x];
		}
	};
	/// A glyph FindGlyph gave, at a quarter of its height
	[[nodiscard]] SmallGlyph GetSmallGlyph(const GlyphMetrics& glyph) const;
	/// The size of the texture the glyphs are in, in texels
	[[nodiscard]] glm::u16vec2 GetAtlasSize() const { return glm::u16vec2(_atlasSize); }

private:
	struct Glyph
	{
		GlyphMetrics metrics;
		uint32_t slotX; ///< texel column of the glyph's slot in the atlas
		uint32_t line;  ///< 64-texel line of the atlas
		/// The glyph at a quarter of its height (SmallGlyph), its width and coverage
		uint16_t smallWidth;
		std::vector<uint8_t> smallAlpha;
	};
	[[nodiscard]] const Glyph* Find(char16_t code) const;

	static constexpr uint32_t k_CellHeight = 80;
	std::unordered_map<char16_t, Glyph> _glyphs;
	float _spaceAdvance {15.0f};
	glm::uvec2 _atlasSize {0, 0};
	std::unique_ptr<Texture2D> _texture;
};

} // namespace openblack::graphics
