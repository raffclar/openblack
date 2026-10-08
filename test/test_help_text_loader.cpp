/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/HelpText.h"
#include "Resources/ResourcesInterface.h"

using openblack::helptext::LanguageIndex;
using openblack::resources::HelpTextLoader;

namespace
{
/// A fake script in UTF-16 little endian with its byte order mark
std::vector<uint8_t> Utf16(const std::u16string& text)
{
	std::vector<uint8_t> bytes = {0xFF, 0xFE};
	for (const char16_t c : text)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	return bytes;
}
} // namespace

TEST(HelpTextLoader, ParsesTheTextsOfAScript)
{
	const auto bytes = Utf16(u"HELP_TEXT_NARRATOR_GOOD_SPIRIT = 2\n"
	                         u"ADD_TEXT(0, HELP_TEXT_NARRATOR_GOOD_SPIRIT, \"HELP_TEXT_ONE\", \"Hello\")\n"
	                         u"ADD_TEXT(1, HELP_TEXT_NARRATOR_GOOD_SPIRIT, \"HELP_TEXT_TWO\", \"World\")\n");
	const auto texts = HelpTextLoader {}(HelpTextLoader::FromBufferTag {}, bytes);
	ASSERT_NE(texts, nullptr);
	ASSERT_EQ(texts->size(), 2u);
	EXPECT_EQ((*texts)[1].name, "HELP_TEXT_TWO");
	EXPECT_EQ((*texts)[1].text, u"World");
	EXPECT_EQ((*texts)[1].narrator, openblack::helptext::k_NarratorGoodSpirit);
}

TEST(HelpTextLoader, NoBytesIsAnEmptyDatabase)
{
	const auto texts = HelpTextLoader {}(HelpTextLoader::FromBufferTag {}, {});
	ASSERT_NE(texts, nullptr);
	EXPECT_TRUE(texts->empty());
}

TEST(HelpTextLanguage, TheFirstTwoCharactersInTheTable)
{
	const auto index = [](const std::string& text) { return LanguageIndex(std::vector<uint8_t>(text.begin(), text.end())); };
	EXPECT_EQ(index("uk"), 0u);
	EXPECT_EQ(index("es"), 5u);
	EXPECT_EQ(index("jp"), 6u);
	EXPECT_EQ(index("th"), 14u);
	EXPECT_EQ(index("FR\r\n"), 2u); // case insensitive, the rest ignored
	EXPECT_EQ(index("dex"), 3u);
}

TEST(HelpTextLanguage, NoMatchIsZero)
{
	const auto index = [](const std::vector<uint8_t>& bytes) { return LanguageIndex(bytes); };
	EXPECT_EQ(index({}), 0u);
	EXPECT_EQ(index({'z', 'z'}), 0u);
	EXPECT_EQ(index({'u'}), 0u);
	EXPECT_EQ(index({'u', 0, 's'}), 0u); // stops at a zero
}
