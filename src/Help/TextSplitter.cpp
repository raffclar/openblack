/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextSplitter.h"

namespace openblack::help::text_splitter
{

bool IsBlank(char16_t c)
{
	return c == 0x20 || c == 0x09 || c == 0x0D || c == 0x0A || c == k_Tilde;
}

bool IsEscape(char16_t c)
{
	return c == u'$' || c == u'\\';
}

bool IsDigit(char16_t c)
{
	return c >= u'0' && c <= u'9';
}

bool IsCode(char16_t c)
{
	return IsDigit(c) || std::u16string_view(u"CDFMNPcdfmnp").find(c) != std::u16string_view::npos;
}

void DrawState::SetFont(int32_t n)
{
	if (n == 2)
	{
		font = f1Loaded ? TextFont::F1 : TextFont::J0;
	}
	else if (n == 3)
	{
		font = f3Loaded ? TextFont::F3 : TextFont::J0;
	}
	else
	{
		font = TextFont::J0;
	}
}

void DrawState::SetColour(int32_t c)
{
	if (c != 0)
	{
		b = static_cast<uint8_t>((c >> 16) & 0xFF);
		g = static_cast<uint8_t>((c >> 8) & 0xFF);
		r = static_cast<uint8_t>(c & 0xFF);
		return;
	}
	if (narrator == 2) // the good spirit
	{
		r = 0xEB;
		g = 0xEB;
		b = 0xB7;
	}
	else if (narrator == 3) // the evil spirit
	{
		r = 0xFF;
		g = 0xB4;
		b = 0xB4;
	}
	else
	{
		r = 0xFF;
		g = 0xFF;
		b = 0xFF;
	}
}

void DrawState::SetEntry(int32_t entryNarrator)
{
	narrator = entryNarrator;
	SetFont(entryNarrator);
	SetColour(0);
}

Splitter::Splitter(std::u16string_view text)
    : _text(text)
{
}

Piece Splitter::Next(std::u16string& word, DrawState& state, bool flag)
{
	word.clear();
	Piece result = Piece::Word;
	if (IsBlank(At(_pos)))
	{
		const Piece blanks = SkipBlanks(); // 1 or 3 (the original's test for 6 never matches)
		if (blanks != Piece::Spaced && blanks != Piece::Tilde)
		{
			return blanks; // a new line, empty word
		}
		result = Piece::Word;
	}
	if (IsEscape(At(_pos)))
	{
		if (IsEscape(At(_pos + 1))) // "$$" is the second character as a word character
		{
			word.push_back(At(_pos + 1));
			_pos += 2;
		}
		else
		{
			const Piece code = Code(state, flag);
			SkipCode();
			if (code != Piece::Word)
			{
				return code;
			}
			if (IsBlank(At(_pos))) // the blanks after a code are part of this (empty) piece
			{
				return SkipBlanks();
			}
			return Piece::Word;
		}
	}
	// the word, up to a blank, an escape, the end or 0x2F characters
	while (!IsBlank(At(_pos)) && !IsEscape(At(_pos)) && At(_pos) != 0 && word.size() < k_MaxWord)
	{
		word.push_back(At(_pos));
		++_pos;
	}
	// followed by blanks -> 3, by the tilde -> 6 (the blanks are skipped by the next call)
	if (IsBlank(At(_pos)))
	{
		result = At(_pos) == k_Tilde ? Piece::Tilde : Piece::Spaced;
	}
	return result;
}

int32_t Splitter::ParseNumberAt(size_t i) const
{
	while (At(i) == u' ' || (At(i) >= 0x09 && At(i) <= 0x0D))
	{
		++i;
	}
	bool negative = false;
	if (At(i) == u'-' || At(i) == u'+')
	{
		negative = At(i) == u'-';
		++i;
	}
	int32_t value = 0;
	while (IsDigit(At(i)))
	{
		value = value * 10 + (At(i) - u'0');
		++i;
	}
	return negative ? -value : value;
}

Piece Splitter::SkipBlanks()
{
	if (!IsBlank(At(_pos)))
	{
		return Piece::Spaced;
	}
	while (IsBlank(At(_pos)))
	{
		if (At(_pos) == u'\n')
		{
			++_pos;
			return Piece::NewLine;
		}
		++_pos;
	}
	return Piece::Spaced;
}

Piece Splitter::Code(DrawState& state, bool flag) const
{
	const char16_t code = At(_pos + 1);
	if (!IsCode(code)) // unknown codes ($I, $s, $g...) only lose the escape (SkipCode)
	{
		return Piece::Word;
	}
	switch (code)
	{
	case u'C':
	case u'c':
		state.SetColour(ParseNumberAt(_pos + 2));
		return Piece::Word;
	case u'F':
	case u'f':
		state.SetFont(ParseNumberAt(_pos + 2));
		return Piece::Word;
	case u'M':
	case u'm':
		// with the flag, the help system shows control icon n, an InputPromptIcon at the
		// right of the box. (pending) not ported: only recorded in DrawState::controlIcon
		if (flag)
		{
			state.controlIcon = ParseNumberAt(_pos + 2);
		}
		return Piece::Word;
	case u'N':
	case u'n':
		return Piece::NewLine;
	case u'P':
	case u'p':
		return Piece::Percent;
	case u'D':
	case u'd':
		return Piece::Number;
	default:
		// the digits' value: 2 (stop) for 1, else 0
		return ParseNumberAt(_pos + 1) == 1 ? Piece::Stop : Piece::Word;
	}
}

void Splitter::SkipCode()
{
	++_pos;
	const char16_t code = At(_pos);
	if (code == u'C' || code == u'c')
	{
		++_pos;
		while (IsDigit(At(_pos)))
		{
			++_pos;
		}
		// skips one more character (the original reads past a terminating 0 here; not done)
		if (_pos < _text.size())
		{
			++_pos;
		}
		return;
	}
	if (!IsCode(code)) // only the escape is skipped
	{
		return;
	}
	++_pos;
	while (IsDigit(At(_pos)))
	{
		++_pos;
	}
}

} // namespace openblack::help::text_splitter
