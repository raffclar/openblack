/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A made up script and font in the game's formats, for the interface's tests

#pragma once

#include <cstdint>
#include <cstring>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Gui/GameFont.h>
#include <gtest/gtest.h>

namespace openblack::gui::test
{
inline std::vector<uint8_t> Utf16Script(std::u16string_view text, bool byteOrderMark = true)
{
	std::vector<uint8_t> bytes;
	if (byteOrderMark)
	{
		bytes.insert(bytes.end(), {0xFF, 0xFE});
	}
	for (const auto c : text)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	return bytes;
}

struct FakeGlyph
{
	char16_t character;
	uint16_t width;
	float left;
	float ink;
	float right;
	/// Rows of '#' and '.', as tall as the font
	std::vector<std::string> rows;
};

/// A font in the format of data/j0.met and data/j0.fnt
struct FakeFont
{
	uint32_t height;
	std::vector<FakeGlyph> glyphs;
	std::vector<uint8_t> met;
	std::vector<uint8_t> fnt;

	template <typename T>
	static void Append(std::vector<uint8_t>& data, T value)
	{
		const auto offset = data.size();
		data.resize(offset + sizeof(T));
		std::memcpy(data.data() + offset, &value, sizeof(T));
	}

	void Build()
	{
		met.clear();
		fnt.clear();
		Append(met, height);
		std::vector<uint8_t> name(0x100, 0);
		const std::u16string_view fontName = u"Fake Sans";
		for (size_t i = 0; i < fontName.size(); ++i)
		{
			name[i * 2] = static_cast<uint8_t>(fontName[i]);
		}
		met.insert(met.end(), name.begin(), name.end());
		Append(met, static_cast<uint32_t>(glyphs.size()));
		for (const auto& glyph : glyphs)
		{
			// Runs of clear and set pixels in turn, starting with clear
			const auto offset = static_cast<uint32_t>(fnt.size());
			bool set = false;
			uint32_t run = 0;
			auto flush = [this, &run]() {
				if (run >= 0xFF)
				{
					fnt.push_back(0xFF);
					Append(fnt, static_cast<uint16_t>(run));
				}
				else
				{
					fnt.push_back(static_cast<uint8_t>(run));
				}
				run = 0;
			};
			for (const auto& row : glyph.rows)
			{
				for (const auto pixel : row)
				{
					if ((pixel == '#') != set)
					{
						flush();
						set = !set;
					}
					++run;
				}
			}
			flush();

			Append(met, static_cast<uint16_t>(glyph.character));
			Append(met, glyph.width);
			Append(met, static_cast<uint32_t>(0xCCCC0000));
			Append(met, glyph.left);
			Append(met, glyph.ink);
			Append(met, glyph.right);
			Append(met, offset);
			Append(met, static_cast<uint32_t>(fnt.size() - offset));
		}
	}
};

/// Four pixels high: a solid A, a checkered B, a space, a question mark, a hyphen and a wide W
inline FakeFont MakeFont()
{
	FakeFont font {.height = 4, .glyphs = {}, .met = {}, .fnt = {}};
	font.glyphs = {
	    {.character = u'A', .width = 2, .left = 1.0f, .ink = 2.0f, .right = 1.0f, .rows = {"##", "##", "##", "##"}},
	    {.character = u'B', .width = 2, .left = 0.0f, .ink = 2.0f, .right = 0.0f, .rows = {"#.", ".#", "#.", ".#"}},
	    {.character = u' ', .width = 0, .left = 1.0f, .ink = 0.0f, .right = 1.0f, .rows = {"", "", "", ""}},
	    {.character = u'?', .width = 2, .left = 0.0f, .ink = 2.0f, .right = 2.0f, .rows = {"##", "..", "#.", ".."}},
	    {.character = u'-', .width = 2, .left = 0.0f, .ink = 2.0f, .right = 0.0f, .rows = {"..", "##", "..", ".."}},
	    {.character = u'W', .width = 4, .left = 0.0f, .ink = 8.0f, .right = 0.0f, .rows = {"####", "####", "####", "####"}},
	};
	font.Build();
	return font;
}

inline GameFont LoadFont()
{
	auto fake = MakeFont();
	auto font = GameFont::Load(fake.met, fake.fnt);
	EXPECT_TRUE(font.has_value());
	return std::move(*font);
}

} // namespace openblack::gui::test
