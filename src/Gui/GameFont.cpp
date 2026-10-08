/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameFont.h"

#include <string>

#include "Graphics/GameFont.h"

namespace openblack::gui
{
namespace
{
bool IsSpace(char16_t c)
{
	return c == u' ' || c == u'\t';
}
} // namespace

std::optional<GameFont> GameFont::From(std::shared_ptr<const graphics::GameFont> font)
{
	if (font == nullptr)
	{
		return std::nullopt;
	}
	GameFont result;
	result._font = std::move(font);
	return result;
}

float GameFont::GetWidth(std::u16string_view text, float size) const
{
	return _font->GetStringWidth(text, size);
}

const graphics::Texture2D& GameFont::GetTexture() const
{
	return _font->GetTexture();
}

const GameFont::Glyph* GameFont::Find(char16_t character) const
{
	return _font->FindGlyph(character);
}

GameFont::SmallGlyph GameFont::GetSmallGlyph(const Glyph& glyph) const
{
	return _font->GetSmallGlyph(glyph);
}

glm::u16vec2 GameFont::GetAtlasSize() const
{
	return _font->GetAtlasSize();
}

std::vector<std::u16string_view> GameFont::Wrap(std::u16string_view text, float size, float width) const
{
	std::vector<std::u16string_view> lines;
	while (!text.empty())
	{
		auto end = text.find_first_of(u"\r\n");
		auto line = text.substr(0, end);
		size_t next = end == std::u16string_view::npos ? text.size() : end + 1;
		if (end != std::u16string_view::npos && text[end] == u'\r' && end + 1 < text.size() && text[end + 1] == u'\n')
		{
			++next;
		}
		if (GetWidth(line, size) > width)
		{
			size_t fits = 0;
			while (fits < line.size() && GetWidth(line.substr(0, fits + 1), size) <= width)
			{
				++fits;
			}
			size_t breakAt = fits;
			while (breakAt > 0 && !IsSpace(line[breakAt]) && line[breakAt - 1] != u'-')
			{
				--breakAt;
			}
			if (breakAt == 0)
			{
				breakAt = std::max<size_t>(fits, 1);
			}
			next = breakAt;
			line = line.substr(0, breakAt);
			while (next < text.size() && IsSpace(text[next]))
			{
				++next;
			}
		}
		while (!line.empty() && IsSpace(line.back()))
		{
			line.remove_suffix(1);
		}
		lines.push_back(line);
		text.remove_prefix(next);
	}
	return lines;
}

} // namespace openblack::gui
