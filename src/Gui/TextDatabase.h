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

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace openblack::gui
{

/// The game's text, looked up by the names its scripts give it (HELP_TEXT_DIALOG_CONTINUEGAME and so on).
///
/// Black & White keeps its text in the ADD_TEXT lines of UTF-16 scripts: InfoScript2.txt, InfoScriptPatch2.txt and
/// InfoScriptMultiplayer2.txt. The game numbers the texts in the order the lines come in (HelpTextDataBase and
/// MultiplayerTextDataBase); the names say the same and don't depend on the order.
// NOLINTNEXTLINE(bugprone-exception-escape): MSVC's unordered_map allocates when it is moved
class TextDatabase
{
public:
	/// Adds the ADD_TEXT lines of a script, little endian UTF-16 with or without a byte order mark. A text replaces any
	/// earlier one of the same name. Returns how many texts the script has.
	size_t AddScript(std::span<const uint8_t> script);
	/// Adds the help texts' script, whose texts the scripts also refer to by number: the order they come in
	size_t AddHelpScript(std::span<const uint8_t> script);

	/// The text of a name, empty when there is none. "\n" in the script is a line break.
	[[nodiscard]] std::u16string_view Get(std::string_view name) const;
	[[nodiscard]] size_t GetCount() const noexcept { return _texts.size(); }
	/// The names of the help texts by their numbers
	[[nodiscard]] std::span<const std::string> GetHelpNames() const noexcept { return _helpNames; }

	/// A help text, which the scripts refer to by its number
	struct HelpText
	{
		/// Who says it, a value of the script's narrators: 2 the good advisor, 3 the evil one
		int32_t narrator {0};
		/// Shown even when the player has only the important texts shown (the first argument of its line)
		bool important {false};
		/// As the script writes it, its codes ($ and backslash) left for the help text's display to read
		std::u16string text;
	};
	/// The help text of a number. Number 0 and numbers past the last give the first text.
	[[nodiscard]] const HelpText& GetHelpText(uint32_t number) const;
	[[nodiscard]] size_t GetHelpTextCount() const noexcept { return _helpTexts.size(); }

private:
	size_t Add(std::span<const uint8_t> script, std::vector<std::string>* names, std::vector<HelpText>* helpTexts);

	std::unordered_map<std::string, std::u16string> _texts;
	std::vector<std::string> _helpNames;
	std::vector<HelpText> _helpTexts;
};

/// Converts text for logs and the like
std::string ToUtf8(std::u16string_view text);
/// Converts text to show in the game
std::u16string ToUtf16(std::string_view text);

} // namespace openblack::gui
