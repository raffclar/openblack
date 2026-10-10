/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <string>
#include <string_view>

// The text splitter of the help text: the word splitter and the character tests and skips it uses. One splitter for
// the word count of the reading time and for the display (measure and draw, HelpTextDisplay). The escape codes change
// the display state ($C colour, $F font, $M control icon); the counting caller passes a throwaway state and flag 0.

namespace openblack::help
{

/// The fonts of the help text, chosen by DrawState::SetFont. The original game really does give each advisor a font of
/// its own: every dialogue entry starts in its narrator's font and colour, so the good advisor's lines are in f1, the
/// evil advisor's in f3 and everyone else's in j0 (checked against the executable, see the advisors' research notes).
/// The three fonts have the same height and are drawn at the same line height.
enum class TextFont : uint8_t
{
	J0, ///< Data\j0 "Ocean Sans MM": every narrator but the two advisors
	F1, ///< Data\f1 "Footlight MT": the good advisor (narrator 2), j0 when its files are missing
	F3, ///< Data\f3 "Orange LET": the evil advisor (narrator 3), j0 when its files are missing
};

namespace text_splitter
{

/// '~' in the help texts' script becomes this when loaded: a blank that adds no space
constexpr char16_t k_Tilde = 0xF8FE;
/// A word stops after 0x2F characters; the rest is the next piece
constexpr size_t k_MaxWord = 0x2F;

/// Space, tab, CR, LF or the tilde
[[nodiscard]] bool IsBlank(char16_t c);
/// '$' or a backslash
[[nodiscard]] bool IsEscape(char16_t c);
/// '0'..'9'
[[nodiscard]] bool IsDigit(char16_t c);
/// A digit, C/c or one of DFMNPdfmnp
[[nodiscard]] bool IsCode(char16_t c);

/// The piece types Splitter::Next returns
enum class Piece : int
{
	Word = 0,    ///< a word (or nothing) followed by a non-blank
	NewLine = 1, ///< a blank run containing '\n', or $N
	Stop = 2,    ///< $1
	Spaced = 3,  ///< a word followed by blanks (or a code followed by blanks): a space before the next piece
	Percent = 4, ///< $P: the number with "%3.3f%%"
	Number = 5,  ///< $D: the number with "%3.3f"
	Tilde = 6,   ///< a word followed by the tilde: no space
};

/// The help text state that the escape codes change while a text is measured and drawn: the current font and the
/// colour of the entry being drawn
struct DrawState
{
	TextFont font {TextFont::J0};
	uint8_t r {0xFF};
	uint8_t g {0xFF};
	uint8_t b {0xFF};
	int32_t narrator {0};
	bool f1Loaded {true};
	bool f3Loaded {true};
	/// The last $M<n> seen with the flag set; -1 when none. The control icon is not drawn yet
	int controlIcon {-1};

	/// 2 -> f1 (or j0 when not loaded), 3 -> f3 (or j0), else j0
	void SetFont(int32_t n);
	/// c != 0 -> b = c >> 16, g = c >> 8, r = c (c as 0xBBGGRR); c == 0 -> the narrator default: 2 -> (235,235,183),
	/// 3 -> (255,180,180), else white
	void SetColour(int32_t c);
	/// The font and colour defaults of the entry being drawn
	void SetEntry(int32_t entryNarrator);
};

/// The word splitter as an object walking one text
class Splitter
{
public:
	explicit Splitter(std::u16string_view text);

	/// The end of the text (the callers' `while (*text)`)
	[[nodiscard]] bool AtEnd() const { return At(_pos) == 0; }

	/// The next piece: its type (Piece) and its word (empty for codes and new lines). Only with the flag does $M act
	Piece Next(std::u16string& word, DrawState& state, bool flag);

private:
	[[nodiscard]] char16_t At(size_t i) const { return i < _text.size() ? _text[i] : u'\0'; }

	/// _wtoi from position i (approximate: the CRT white space set)
	[[nodiscard]] int32_t ParseNumberAt(size_t i) const;
	/// Past the blanks; 1 when it went past a line feed (stops right after it), 3 otherwise
	Piece SkipBlanks();
	/// At an escape: the code's piece type and its effect on the state
	Piece Code(DrawState& state, bool flag) const;
	/// Past the escape, its code and the code's digits (and one more character after C<digits>)
	void SkipCode();

	std::u16string_view _text;
	size_t _pos {0};
};

} // namespace text_splitter

/// The number of words of a text, as its reading time counts them: the non-empty pieces the splitter returns
[[nodiscard]] uint32_t CountWords(std::u16string_view text);

} // namespace openblack::help
