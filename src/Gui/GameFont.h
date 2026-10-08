/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// raffclar's gui::GameFont turned into an adapter over our graphics::GameFont (the exact
// port of the original's fonts): the menus measure and draw with it.

#include <cstdint>

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Graphics/GameFont.h"

namespace openblack::graphics
{
class Texture2D;
} // namespace openblack::graphics

namespace openblack::gui
{

class GameFont
{
public:
	/// A font of the font cache (resources::LoadGameFont); null for none
	static std::optional<GameFont> From(std::shared_ptr<const graphics::GameFont> font);

	/// The .met's glyph height (80 for j0): sizes are line heights in pixels
	[[nodiscard]] uint16_t GetHeight() const noexcept { return 80; }
	/// graphics::GameFont::GetStringWidth
	[[nodiscard]] float GetWidth(std::u16string_view text, float size) const;
	/// raffclar's line breaking (after spaces and hyphens, at line breaks; a word too long broken where it ends), on our
	/// widths. (pending) the original's own wrapping rule
	[[nodiscard]] std::vector<std::u16string_view> Wrap(std::u16string_view text, float size, float width) const;

	/// A glyph's metrics, and the glyph at a quarter of its height, which the temple's scrolls write their text with
	using Glyph = graphics::GameFont::GlyphMetrics;
	using SmallGlyph = graphics::GameFont::SmallGlyph;
	/// The glyph of a character, or of '?' when the font has none, null if it has neither
	[[nodiscard]] const Glyph* Find(char16_t character) const;
	[[nodiscard]] SmallGlyph GetSmallGlyph(const Glyph& glyph) const;
	/// The size of the font's texture, which a glyph's place in it is in
	[[nodiscard]] glm::u16vec2 GetAtlasSize() const;

	[[nodiscard]] const graphics::GameFont& Get() const { return *_font; }
	[[nodiscard]] const graphics::Texture2D& GetTexture() const;

private:
	std::shared_ptr<const graphics::GameFont> _font;
};

} // namespace openblack::gui
