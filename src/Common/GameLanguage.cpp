/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameLanguage.h"

#include <algorithm>
#include <iterator>

namespace openblack::game_language
{

namespace
{

/// The lower-case form of a letter from A to Z. The game does not change other characters.
[[nodiscard]] constexpr char Lower(uint8_t character)
{
	return (character >= 'A' && character <= 'Z') ? static_cast<char>(character - 'A' + 'a') : static_cast<char>(character);
}

} // namespace

GameLanguage FromCountryFile(std::optional<std::span<const uint8_t>> contents)
{
	// A file with less than two characters can not be the same as a code. A zero character ends the text, and no code
	// has a zero character, so the compare also fails for it.
	if (!contents.has_value() || contents->size() < 2)
	{
		return GameLanguage::UkEnglish;
	}
	const std::array<char, 2> read {Lower((*contents)[0]), Lower((*contents)[1])};
	const auto found =
	    std::ranges::find_if(k_Codes, [&read](std::string_view code) { return code[0] == read[0] && code[1] == read[1]; });
	if (found == k_Codes.end())
	{
		return GameLanguage::UkEnglish;
	}
	return static_cast<GameLanguage>(std::distance(k_Codes.begin(), found));
}

bool NeedsBiggerText(GameLanguage language)
{
	switch (language)
	{
	case GameLanguage::Japanese:
	case GameLanguage::SimplifiedChinese:
	case GameLanguage::TraditionalChinese:
	case GameLanguage::Korean:
	case GameLanguage::Thai:
		return true;
	default:
		return false;
	}
}

bool ShowsSingAlong(GameLanguage language)
{
	switch (language)
	{
	case GameLanguage::BrazilianPortuguese:
	case GameLanguage::SimplifiedChinese:
	case GameLanguage::TraditionalChinese:
	case GameLanguage::Korean:
	case GameLanguage::Thai:
		return false;
	default:
		return true;
	}
}

std::string_view Code(GameLanguage language)
{
	const auto index = static_cast<size_t>(language);
	return index < k_Codes.size() ? k_Codes[index] : k_Codes[0];
}

} // namespace openblack::game_language
