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

#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace openblack
{

/// The language of the install. The game reads it one time as it starts, from the file Country.txt in the game folder.
enum class GameLanguage : uint8_t
{
	UkEnglish,
	UsEnglish,
	French,
	German,
	Swedish,
	Spanish,
	Japanese,
	Dutch,
	BrazilianPortuguese,
	Italian,
	SimplifiedChinese,
	TraditionalChinese,
	Polish,
	Korean,
	Thai,
};

namespace game_language
{

/// The name of the file that gives the language, in the game folder
constexpr std::string_view k_CountryFile = "Country.txt";

/// The two-letter code of each language, in the order of GameLanguage
constexpr std::array<std::string_view, 15> k_Codes {"uk", "us", "fr", "de", "se", "es", "jp", "nl",
                                                    "br", "it", "sc", "tc", "pl", "kr", "th"};

/// The language that the contents of Country.txt give. The game reads the first two characters of the file. It compares
/// them with each code, and it ignores the case of the letters. When the file is missing, or no code is the same, the
/// language is UK English.
[[nodiscard]] GameLanguage FromCountryFile(std::optional<std::span<const uint8_t>> contents);

/// Whether the language uses a bigger size for the text of the dialogs, the help text and the mouse pictures. These
/// languages are Japanese, the two Chinese languages, Korean and Thai.
[[nodiscard]] bool NeedsBiggerText(GameLanguage language);

/// Whether a sung verse puts a glow under the word that the music reached. All the languages do this, but not Brazilian
/// Portuguese, the two Chinese languages, Korean and Thai.
[[nodiscard]] bool ShowsSingAlong(GameLanguage language);

/// The two-letter code of the language
[[nodiscard]] std::string_view Code(GameLanguage language);

} // namespace game_language
} // namespace openblack
