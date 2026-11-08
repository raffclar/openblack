/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <variant>
#include <vector>

#include <Inspector/StartupBoxInput.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;
using namespace openblack::inspector::startup_box;

namespace
{

std::vector<Input> Made(std::string_view query, const Json& params)
{
	auto made = FromRequest(query, params);
	EXPECT_TRUE(std::holds_alternative<std::vector<Input>>(made)) << std::get<std::string>(made);
	return std::holds_alternative<std::vector<Input>>(made) ? std::get<std::vector<Input>>(made) : std::vector<Input> {};
}

std::string Refused(std::string_view query, const Json& params)
{
	auto made = FromRequest(query, params);
	EXPECT_TRUE(std::holds_alternative<std::string>(made));
	return std::holds_alternative<std::string>(made) ? std::get<std::string>(made) : std::string();
}

} // namespace

TEST(StartupBoxInput, TextGoesAsOneTextEvent)
{
	const auto inputs = Made("input.text", {{"text", "Gavin"}});
	ASSERT_EQ(inputs.size(), 1u);
	EXPECT_EQ(std::get<Text>(inputs[0]).text, "Gavin");
}

TEST(StartupBoxInput, LongTextGoesInPiecesCutBetweenCharacters)
{
	// 29 letters, then a two-byte letter that does not fit in the first piece of 30 bytes
	const std::string text = std::string(29, 'a') + "\xC3\xA9" + "b";
	const auto inputs = Made("input.text", {{"text", text}});
	ASSERT_EQ(inputs.size(), 2u);
	EXPECT_EQ(std::get<Text>(inputs[0]).text, std::string(29, 'a'));
	EXPECT_EQ(std::get<Text>(inputs[1]).text, "\xC3\xA9"
	                                          "b");
}

TEST(StartupBoxInput, TextPiecesKeepACharacterLongerThanAPieceWhole)
{
	const auto pieces = TextPieces("\xF0\x9F\x98\x80x", 2);
	ASSERT_EQ(pieces.size(), 2u);
	EXPECT_EQ(pieces[0], "\xF0\x9F\x98\x80");
	EXPECT_EQ(pieces[1], "x");
}

TEST(StartupBoxInput, EmptyOrTooLongTextIsRefused)
{
	EXPECT_EQ(Refused("input.text", {{"text", ""}}), "text is 1 to 256 bytes of UTF-8");
	EXPECT_EQ(Refused("input.text", {{"text", std::string(257, 'a')}}), "text is 1 to 256 bytes of UTF-8");
	EXPECT_EQ(Refused("input.text", Json::object()), "text is 1 to 256 bytes of UTF-8");
}

TEST(StartupBoxInput, AKeyTapGoesDownThenUp)
{
	const auto inputs = Made("input.key", {{"key", "Return"}});
	ASSERT_EQ(inputs.size(), 2u);
	EXPECT_EQ(std::get<Key>(inputs[0]).name, "Return");
	EXPECT_TRUE(std::get<Key>(inputs[0]).down);
	EXPECT_FALSE(std::get<Key>(inputs[1]).down);
}

TEST(StartupBoxInput, AKeyPressOrReleaseGoesOnce)
{
	const auto press = Made("input.key", {{"key", "Backspace"}, {"how", "press"}});
	ASSERT_EQ(press.size(), 1u);
	EXPECT_TRUE(std::get<Key>(press[0]).down);
	const auto release = Made("input.key", {{"key", "Backspace"}, {"how", "release"}});
	ASSERT_EQ(release.size(), 1u);
	EXPECT_FALSE(std::get<Key>(release[0]).down);
}

TEST(StartupBoxInput, ActionsAndOtherQueriesAreRefused)
{
	EXPECT_EQ(Refused("input.key", {{"action", "Pause"}}),
	          "the box before the first land takes keys by their names, not actions");
	EXPECT_EQ(Refused("input.key", Json::object()), "input.key needs a key");
	EXPECT_EQ(Refused("input.key", {{"key", "A"}, {"how", "hold"}}), "how is press, release or tap");
	EXPECT_EQ(Refused("input.button", Json::object()),
	          "the box before the first land takes only input.text and input.key, not input.button");
}
