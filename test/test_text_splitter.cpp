/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Help/HelpSystem.h"
#include "Help/TextSplitter.h"

// The shared splitter (Help/TextSplitter.h) against the counting splitter HelpSystem.cpp had before it was shared
// (copied below verbatim as OldSplitter): CountWords must give the same counts.

using namespace openblack::help;

namespace
{
namespace old
{
bool IsBlank(char16_t c)
{
	return c == 0x20 || c == 0x09 || c == 0x0D || c == 0x0A || c == 0xF8FE;
}

bool IsEscape(char16_t c)
{
	return c == u'$' || c == u'\\';
}

bool IsDigit(char16_t c)
{
	return c >= u'0' && c <= u'9';
}

// a digit, or C/c or one of DFMNPdfmnp
bool IsCode(char16_t c)
{
	return IsDigit(c) || std::u16string_view(u"CDFMNPcdfmnp").find(c) != std::u16string_view::npos;
}

class OldSplitter
{
public:
	explicit OldSplitter(std::u16string_view text)
	    : _text(text)
	{
	}

	[[nodiscard]] bool AtEnd() const { return At(_pos) == 0; }

	/// The length of the next piece (0 = empty)
	uint32_t Next()
	{
		uint32_t length = 0;
		if (IsBlank(At(_pos)) && SkipBlanks() != 3)
		{
			return 0;
		}
		if (IsEscape(At(_pos)))
		{
			if (IsEscape(At(_pos + 1))) // the second one is a character of the word
			{
				_pos += 2;
				length = 1;
			}
			else
			{
				const int result = CodeResult();
				SkipCode();
				if (result == 0 && IsBlank(At(_pos)))
				{
					SkipBlanks();
				}
				return 0;
			}
		}
		// the word itself, up to 47 characters
		while (!IsBlank(At(_pos)) && !IsEscape(At(_pos)) && At(_pos) != 0 && length < 0x2F)
		{
			++length;
			++_pos;
		}
		return length;
	}

private:
	[[nodiscard]] char16_t At(size_t i) const { return i < _text.size() ? _text[i] : u'\0'; }

	/// 1 when it went past a line feed, 3 otherwise
	int SkipBlanks()
	{
		if (!IsBlank(At(_pos)))
		{
			return 3;
		}
		while (IsBlank(At(_pos)))
		{
			if (At(_pos) == u'\n')
			{
				++_pos;
				return 1;
			}
			++_pos;
		}
		return 3;
	}

	/// At an escape: 1 for N, 4 for P, 5 for D, 2 for the number 1, 0 for C, F, M, other numbers or
	/// no code (the calls it makes to set colours and fonts change only the display)
	[[nodiscard]] int CodeResult() const
	{
		const char16_t code = At(_pos + 1);
		if (!IsCode(code))
		{
			return 0;
		}
		switch (code)
		{
		case u'N':
		case u'n':
			return 1;
		case u'P':
		case u'p':
			return 4;
		case u'D':
		case u'd':
			return 5;
		case u'C':
		case u'c':
		case u'F':
		case u'f':
		case u'M':
		case u'm':
			return 0;
		default:
		{
			// the digits' value (2 for 1)
			int value = 0;
			for (size_t i = _pos + 1; IsDigit(At(i)); ++i)
			{
				value = value * 10 + (At(i) - u'0');
			}
			return value == 1 ? 2 : 0;
		}
		}
	}

	/// Past the escape, its code and the code's digits (and one more character after C)
	void SkipCode()
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

	std::u16string_view _text;
	size_t _pos {0};
};

/// The old CountWords
uint32_t CountWords(std::u16string_view text)
{
	OldSplitter splitter(text);
	uint32_t words = 0;
	while (!splitter.AtEnd())
	{
		if (splitter.Next() != 0)
		{
			++words;
		}
	}
	return words;
}
} // namespace old

struct Piece
{
	text_splitter::Piece type;
	std::u16string word;
};

std::vector<Piece> Pieces(std::u16string_view text, text_splitter::DrawState& state, bool flag = false)
{
	text_splitter::Splitter splitter(text);
	std::vector<Piece> out;
	std::u16string word;
	while (!splitter.AtEnd())
	{
		const text_splitter::Piece type = splitter.Next(word, state, flag);
		out.push_back({type, word});
	}
	return out;
}
} // namespace

TEST(TextSplitter, TrickyStringsCountAsBefore)
{
	const std::vector<std::u16string> texts {
	    u"",
	    u"   ",
	    u"Cadena de texto no v\u00E1lida",
	    u"  dos\tpalabras \r\n",
	    u"una\n\notra",
	    u"a\xF8FE"
	    u"b",
	    u"a \xF8FE b\xF8FE",
	    u"Haz clic en la Criatura para que podamos acariciarla. $m2",
	    u"uno\ndos",
	    u"$1 uno",
	    u"uno $1 dos",
	    u"$12 uno",
	    u"$C12xhola",
	    u"$C123x",
	    u"$C123 x",
	    u"$C",
	    u"$C5",
	    u"a$C",
	    u"$F2 hola $F0 adios",
	    u"$N",
	    u"a$Nb",
	    u"a $N b",
	    u"$N $N",
	    u"$P% $D",
	    u"Descubiertos: $I Retos",
	    u"Nombre: $s",
	    u"v\u00E1lido.n\\Int\u00E9ntalo con otro",
	    u"a$$b",
	    u"$$",
	    u"$$$",
	    u"$\\x",
	    u"\\\\",
	    u"a$",
	    u"a \\",
	    u"$ a",
	    u"$\n a",
	    u"$m2 \n x",
	    std::u16string(47, u'x'),
	    std::u16string(48, u'x'),
	    std::u16string(95, u'x'),
	    u"$$" + std::u16string(47, u'x'),
	    std::u16string(47, u'x') + u"$Ny",
	};
	for (const auto& text : texts)
	{
		EXPECT_EQ(CountWords(text), old::CountWords(text)) << "text #" << (&text - texts.data());
	}
	// the counts of test_help_system.cpp CountWords
	EXPECT_EQ(CountWords(u"$C12xhola"), 1u);
	EXPECT_EQ(CountWords(u"a$$b"), 2u);
	EXPECT_EQ(CountWords(std::u16string(48, u'x')), 2u);
}

TEST(TextSplitter, AllShortStringsCountAsBefore)
{
	// every string of up to 6 characters over an alphabet of blanks, escapes, codes, digits and letters
	const std::u16string alphabet = u"a$\\C1N \n\xF8FE"
	                                u"P";
	std::u16string text;
	size_t checked = 0;
	std::function<void()> walk = [&]() {
		ASSERT_EQ(CountWords(text), old::CountWords(text)) << "length " << text.size();
		++checked;
		if (text.size() == 6)
		{
			return;
		}
		for (const char16_t c : alphabet)
		{
			text.push_back(c);
			walk();
			text.pop_back();
		}
	};
	walk();
	EXPECT_EQ(checked, 1111111u);
}

TEST(TextSplitter, PieceTypes)
{
	text_splitter::DrawState state;
	// a word before blanks is 3, before 0xF8FE 6, before an escape or the end 0; "\n" in blanks is 1; the code after
	// blanks returns its own type; the blanks after a code with no type give 3 (or 1)
	auto pieces = Pieces(u"ab cd\xF8FE"
	                     u"ef$Ngh\n$1",
	                     state);
	ASSERT_EQ(pieces.size(), 7U);
	EXPECT_EQ(pieces[0].type, text_splitter::Piece::Spaced);
	EXPECT_EQ(pieces[0].word, u"ab");
	EXPECT_EQ(pieces[1].type, text_splitter::Piece::Tilde);
	EXPECT_EQ(pieces[1].word, u"cd");
	EXPECT_EQ(pieces[2].type, text_splitter::Piece::Word);
	EXPECT_EQ(pieces[2].word, u"ef");
	EXPECT_EQ(pieces[3].type, text_splitter::Piece::NewLine);
	EXPECT_TRUE(pieces[3].word.empty());
	EXPECT_EQ(pieces[4].type, text_splitter::Piece::Spaced);
	EXPECT_EQ(pieces[4].word, u"gh");
	EXPECT_EQ(pieces[5].type, text_splitter::Piece::NewLine);
	EXPECT_EQ(pieces[6].type, text_splitter::Piece::Stop);

	pieces = Pieces(u"$F2 x $P $D", state);
	ASSERT_EQ(pieces.size(), 4U);
	EXPECT_EQ(pieces[0].type, text_splitter::Piece::Spaced); // the blanks after $F2
	EXPECT_TRUE(pieces[0].word.empty());
	EXPECT_EQ(state.font, TextFont::F1);
	EXPECT_EQ(pieces[2].type, text_splitter::Piece::Percent);
	EXPECT_EQ(pieces[3].type, text_splitter::Piece::Number);
}

TEST(TextSplitter, StateCodes)
{
	text_splitter::DrawState state;
	state.SetEntry(3);
	EXPECT_EQ(state.font, TextFont::F3);
	EXPECT_EQ(state.r, 255);
	EXPECT_EQ(state.g, 180);
	static_cast<void>(Pieces(u"$C16711680x", state)); // 0xFF0000 = blue
	EXPECT_EQ(state.b, 255);
	EXPECT_EQ(state.g, 0);
	EXPECT_EQ(state.r, 0);
	static_cast<void>(Pieces(u"$C0", state)); // the narrator default
	EXPECT_EQ(state.r, 255);
	EXPECT_EQ(state.b, 180);

	// $M only with the flag
	static_cast<void>(Pieces(u"$m2", state, false));
	EXPECT_EQ(state.controlIcon, -1);
	static_cast<void>(Pieces(u"$m2", state, true));
	EXPECT_EQ(state.controlIcon, 2);
}
