/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameLanguage.h"
#include "Gui/DialogPainter.h"

using openblack::GameLanguage;
using openblack::gui::DialogPainter;
namespace game_language = openblack::game_language;

namespace
{

GameLanguage FromText(std::string_view text)
{
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	return game_language::FromCountryFile(std::span<const uint8_t>(bytes));
}

} // namespace

TEST(GameLanguage, EachCodeGivesItsLanguageInTheOrderOfTheList)
{
	for (size_t i = 0; i < game_language::k_Codes.size(); ++i)
	{
		const auto code = game_language::k_Codes.at(i);
		EXPECT_EQ(FromText(code), static_cast<GameLanguage>(i)) << code;
	}
	EXPECT_EQ(FromText("jp"), GameLanguage::Japanese);
	EXPECT_EQ(FromText("th"), GameLanguage::Thai);
	EXPECT_EQ(FromText("se"), GameLanguage::Swedish);
	EXPECT_EQ(FromText("br"), GameLanguage::BrazilianPortuguese);
}

TEST(GameLanguage, TheCaseOfTheLettersDoesNotChangeTheLanguage)
{
	EXPECT_EQ(FromText("UK"), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText("De"), GameLanguage::German);
	EXPECT_EQ(FromText("kR"), GameLanguage::Korean);
}

TEST(GameLanguage, OnlyTheFirstTwoCharactersAreRead)
{
	// The install's file has a line end after the code
	EXPECT_EQ(FromText("UK\r\n"), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText("frxyz"), GameLanguage::French);
	EXPECT_EQ(FromText("jpn"), GameLanguage::Japanese);
}

TEST(GameLanguage, NoFileOrNoKnownCodeGivesUkEnglish)
{
	EXPECT_EQ(game_language::FromCountryFile(std::nullopt), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText(""), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText("j"), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText("ja"), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText("gb"), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText(" us"), GameLanguage::UkEnglish);
	EXPECT_EQ(FromText(std::string_view("j\0p", 3)), GameLanguage::UkEnglish);
	// Only the letters A to Z change case
	EXPECT_EQ(FromText("\xC4\xCB"), GameLanguage::UkEnglish);
}

TEST(GameLanguage, FiveLanguagesNeedBiggerText)
{
	for (size_t i = 0; i < game_language::k_Codes.size(); ++i)
	{
		const auto language = static_cast<GameLanguage>(i);
		const bool expected = language == GameLanguage::Japanese || language == GameLanguage::SimplifiedChinese ||
		                      language == GameLanguage::TraditionalChinese || language == GameLanguage::Korean ||
		                      language == GameLanguage::Thai;
		EXPECT_EQ(game_language::NeedsBiggerText(language), expected) << game_language::Code(language);
	}
}

TEST(GameLanguage, FiveLanguagesShowNoSingAlong)
{
	for (size_t i = 0; i < game_language::k_Codes.size(); ++i)
	{
		const auto language = static_cast<GameLanguage>(i);
		const bool expected = language != GameLanguage::BrazilianPortuguese && language != GameLanguage::SimplifiedChinese &&
		                      language != GameLanguage::TraditionalChinese && language != GameLanguage::Korean &&
		                      language != GameLanguage::Thai;
		EXPECT_EQ(game_language::ShowsSingAlong(language), expected) << game_language::Code(language);
	}
}

TEST(GameLanguage, BiggerTextMakesTheMiddleAndSmallSizesOneMore)
{
	EXPECT_EQ(DialogPainter::MidTextSizeFor(false), 22);
	EXPECT_EQ(DialogPainter::SmallTextSizeFor(false), 20);
	EXPECT_EQ(DialogPainter::MidTextSizeFor(true), 23);
	EXPECT_EQ(DialogPainter::SmallTextSizeFor(true), 21);
	EXPECT_EQ(DialogPainter::k_BigTextSize, 35);
}

TEST(GameLanguage, TheCodeOfEachLanguageIsTwoLetters)
{
	EXPECT_EQ(game_language::Code(GameLanguage::UkEnglish), "uk");
	EXPECT_EQ(game_language::Code(GameLanguage::TraditionalChinese), "tc");
	EXPECT_EQ(game_language::Code(GameLanguage::Thai), "th");
}
