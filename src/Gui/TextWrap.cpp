/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextWrap.h"

#include <algorithm>

using namespace openblack::gui;

namespace
{
/// A line grows by this many characters at a time while it is measured
constexpr size_t k_GrowBy = 4;
/// The game goes on measuring past a line break, into whatever follows the text, until the line is too wide. Past the
/// text's end every character reads as a NUL; this only stops a font whose NUL takes no room from measuring for ever.
constexpr size_t k_MaxPastTheEnd = 4096;

bool IsLineBreak(char16_t c)
{
	return c == u'\r' || c == u'\n';
}

bool IsBreak(char16_t c)
{
	return text_wrap::IsSpace(c) || c == text_wrap::k_Tilde;
}

/// The widths of the first characters of a line, the characters past its end read as NULs
class LineWidths
{
public:
	LineWidths(std::u16string_view text, float scale, const text_wrap::AdvanceFn& advance)
	    : _text(text)
	    , _scale(scale)
	    , _advance(advance)
	{
		_sums.push_back(0.0f);
	}

	[[nodiscard]] char16_t At(size_t i) const { return i < _text.size() ? _text[i] : u'\0'; }

	/// How wide the first `count` characters are: their advances added up in order, then scaled
	float Width(size_t count)
	{
		while (_sums.size() <= count)
		{
			_sums.push_back(_sums.back() + _advance(At(_sums.size() - 1)));
		}
		return _sums[count] * _scale;
	}

private:
	std::u16string_view _text;
	float _scale;
	const text_wrap::AdvanceFn& _advance;
	std::vector<float> _sums;
};
} // namespace

bool text_wrap::IsSpace(char16_t c)
{
	if (c < 0x100)
	{
		return c == u' ' || (c >= u'\t' && c <= u'\r');
	}
	// Windows' spaces above Latin-1: the Unicode space separators and the line and paragraph separators
	return c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F ||
	       c == 0x3000;
}

std::vector<std::u16string_view> text_wrap::BreakLines(std::u16string_view text, float width, float scale,
                                                       const AdvanceFn& advance)
{
	text = text.substr(0, text.find(u'\0'));
	std::vector<std::u16string_view> lines;
	while (!text.empty())
	{
		LineWidths line(text, scale, advance);

		// Grow the line a few characters at a time until it is too wide. On a line break the count goes on one
		// character at a time past it.
		size_t count = 0;
		size_t at = 0;
		float lineWidth = 0.0f;
		while (line.At(at) != u'\0' && lineWidth <= width && count < text.size() + k_MaxPastTheEnd)
		{
			for (size_t grown = 0;;)
			{
				if (IsLineBreak(line.At(at)))
				{
					++count;
					break;
				}
				++at;
				++count;
				if (line.At(at) == u'\0' || ++grown >= k_GrowBy)
				{
					break;
				}
			}
			lineWidth = line.Width(count);
		}

		// Too wide: back to the nearest place to break before the end, again and again until it fits
		size_t next = count;
		while (lineWidth > width && count > 0)
		{
			const auto before = count;
			size_t q = count - 1;
			if (!IsSpace(line.At(q)))
			{
				while (line.At(q) != k_Tilde && q > 0)
				{
					--q;
					if (line.At(q) == u'-' || IsSpace(line.At(q)))
					{
						break;
					}
				}
			}
			if (line.At(q) == u'-')
			{
				// The hyphen stays on the line
				count = q + 1;
				next = count;
			}
			else if (IsBreak(line.At(q)))
			{
				// The next line starts after this one blank; the blanks before it are trimmed
				next = q + 1;
				while (IsBreak(line.At(q)) && q > 0)
				{
					--q;
				}
				count = q + 1;
			}
			else
			{
				// Nowhere to break: one character fewer
				--count;
				next = count;
			}
			lineWidth = line.Width(count);
			// A line of a blank wider than the whole width can't get shorter (the game would try for ever)
			if (count == before)
			{
				count = 0;
			}
		}

		// Not even one character fits: the text ends here
		if (count == 0)
		{
			break;
		}
		// The first line break before where the next line starts ends this one, the blanks before it kept
		next = std::min(next, text.size());
		if (const auto breakAt = text.substr(0, next).find_first_of(u"\r\n"); breakAt != std::u16string_view::npos)
		{
			count = std::min(count, breakAt);
			next = breakAt + 1;
		}
		lines.push_back(text.substr(0, count));
		text.remove_prefix(next);
	}
	return lines;
}
