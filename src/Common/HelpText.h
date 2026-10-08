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

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::helptext
{

/// HELP_TEXT_LAST of W120 (Scripts\InfoScript2.txt): the size of the voice table and the bound that running a text and
/// saying it check text ids against
constexpr uint32_t k_TextCount = 0x1B3E;

/// HELP_TEXT_NARRATOR_* as Scripts\InfoScript2.txt W120 declares them ("NAME = value" lines); saying a text
/// compares the narrator with these two
constexpr int32_t k_NarratorGoodSpirit = 2;
constexpr int32_t k_NarratorEvilSpirit = 3;
/// A narrator name ADD_TEXT uses without declaring it (GUIDE and MONK in W120). (inferred) The value the original's
/// script reader (not read) gives such a name is unknown; it is neither 2 nor 3 in any case that matters to saying a
/// text or IsTextRead (the voices of those texts are in villagers.sad).
constexpr int32_t k_NarratorUndeclared = -1;

/// One ADD_TEXT(arg0, NARRATOR, "NAME", "text") of Scripts\InfoScript2.txt. The original keeps the narrator, arg0 and
/// a copy of the text.
struct Entry
{
	/// The 1st argument (0 or 1; meaning not read)
	int32_t arg0 {0};
	/// The 2nd argument, a HELP_TEXT_NARRATOR_* value
	int32_t narrator {0};
	/// The 3rd argument, "HELP_TEXT_...". The original does not keep it; openblack needs it to rebuild the voice table
	/// from the wave names (audio::VoiceTable).
	std::string name;
	/// The 4th argument after the reader's conversion (ConvertScriptText)
	std::u16string text;
};

/// The reader's conversion of an ADD_TEXT's name and text: " ~", "~ " and "~" become 0xF8FE,
/// the two characters "\n" become a line feed, the rest is copied
[[nodiscard]] std::u16string ConvertScriptText(std::u16string_view text);

/// Every ADD_TEXT of an InfoScript2.txt (UTF-16 little endian, with or without the BOM) in file order: a text's id is its
/// 0-based position. The narrator names are resolved with the file's own "NAME = value" lines.
[[nodiscard]] std::vector<Entry> Parse(const std::vector<uint8_t>& utf16);

/// The help text database: Scripts\InfoScript2.txt, loaded on first use.
/// Entry 0 for an id out of range (0 < id < count).
[[nodiscard]] const std::u16string& Get(uint32_t id);
/// The patch text database (Scripts\\InfoScriptPatch2.txt, its own ids by file order): entry 0 for an id at or past its
/// count
[[nodiscard]] const std::u16string& GetPatch(uint32_t id);
/// The whole entry of a text, with the same rule for ids out of range (an empty entry when nothing was loaded)
[[nodiscard]] const Entry& GetEntry(uint32_t id);
/// Number of texts loaded
[[nodiscard]] size_t Count();

/// The text formatted with one number (e.g. 0xEEA "Amount: %3.0f")
[[nodiscard]] std::u16string Format(uint32_t id, double value);
/// The text formatted with its numbers: each printf conversion takes the next value (0xEF9 "Food Stored: %3.0f
/// Wood: %3.0f"), "%%" is a "%" (0xEF3 "... %3.0f%%"); a conversion with no value left stays as it is
[[nodiscard]] std::u16string Format(uint32_t id, std::span<const double> values);

/// Tooltip ids (HELP_TEXT_TOOLTIP_*, the tooltip table starts at 0xE73; texts translated from the Spanish data)
constexpr uint32_t k_ToolTipPickUp = 0xE73;       ///< "Pick up"
constexpr uint32_t k_ToolTipAmountInHand = 0xEEA; ///< "Amount: %3.0f"
constexpr uint32_t k_ToolTipContinue = 0xE79;     ///< HELP_TEXT_TOOLTIP_07 "Continue", the click cue

/// The game's language, read at start up: the first two characters of the game folder's Country.txt in the language
/// table (uk 0, us, fr, de, se, es 5, jp 6, nl, br, it, sc 10, tc 11, pl, kr 13, th 14), case insensitive; 0 without
/// the file or a match. Read once
[[nodiscard]] uint32_t Language();
/// The language index of Country.txt's bytes: their first two characters (fewer when a zero comes first) in the language
/// table, case insensitive; 0 without a match
[[nodiscard]] uint32_t LanguageIndex(std::span<const uint8_t> countryFile);
/// The language is 6, 10, 11, 13 or 14
[[nodiscard]] bool NeedsBiggerText();

} // namespace openblack::helptext
