/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>
#include <string_view>
#include <vector>

#include <CreatureUploadCfg.h>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{

/// A UTF-16 file with the byte order mark
std::vector<uint8_t> Utf16File(std::u16string_view text)
{
	std::vector<uint8_t> bytes {0xFF, 0xFE};
	for (const auto unit : text)
	{
		bytes.push_back(static_cast<uint8_t>(unit & 0xFF));
		bytes.push_back(static_cast<uint8_t>(unit >> 8));
	}
	return bytes;
}

std::vector<uint8_t> NarrowFile(std::string_view text)
{
	return {text.begin(), text.end()};
}

} // namespace

TEST(CreatureUploadCfg, Utf16LinesGiveTheTextsWithoutTheTabsAndTheCarriageReturn)
{
	const auto texts = creatureuploadcfg::Read(Utf16File(u"#####\r\n"
	                                                     u"## Language File\r\n"
	                                                     u"IDC_APPTITLE=\t\t\t\tCreature Upload Utility\r\n"
	                                                     u"  ERROR_TITLE=\t\tError\r\n"
	                                                     u"\r\n"
	                                                     u"IDC_INFO_SENDING=\t\t\tSending creature data to server: %s\r\n"));
	ASSERT_TRUE(texts.contains("IDC_APPTITLE"));
	ASSERT_TRUE(texts.contains("ERROR_TITLE"));
	ASSERT_TRUE(texts.contains("IDC_INFO_SENDING"));
	EXPECT_FALSE(texts.contains("##"));
	EXPECT_EQ(texts.at("IDC_APPTITLE"), "Creature Upload Utility");
	EXPECT_EQ(texts.at("ERROR_TITLE"), "Error");
	EXPECT_EQ(texts.at("IDC_INFO_SENDING"), "Sending creature data to server: %s");
}

TEST(CreatureUploadCfg, Utf16TextKeepsAHashAndBecomesUtf8)
{
	const auto texts = creatureuploadcfg::Read(Utf16File(u"A=Number #1 été\r\n"));
	EXPECT_EQ(texts.at("A"), "Number #1 \xC3\xA9t\xC3\xA9");
}

TEST(CreatureUploadCfg, AHashBeforeTheTextRemovesTheLine)
{
	const auto texts = creatureuploadcfg::Read(Utf16File(u"A= #comment\r\nB=b\r\n"));
	EXPECT_FALSE(texts.contains("A"));
	EXPECT_EQ(texts.at("B"), "b");
}

TEST(CreatureUploadCfg, TheLastLineOfAKeyWins)
{
	const auto texts = creatureuploadcfg::Read(Utf16File(u"A=first\r\nA=second\r\n"));
	EXPECT_EQ(texts.at("A"), "second");
}

TEST(CreatureUploadCfg, ALastLineWithNoLineBreakGivesNoText)
{
	const auto texts = creatureuploadcfg::Read(Utf16File(u"A=a\r\nB=b"));
	EXPECT_EQ(texts.at("A"), "a");
	EXPECT_FALSE(texts.contains("B"));
	// The odd byte at the end of the game's file does not count
	auto bytes = Utf16File(u"C=c\r\n");
	bytes.push_back(0x1A);
	EXPECT_EQ(creatureuploadcfg::Read(bytes).at("C"), "c");
}

TEST(CreatureUploadCfg, NarrowTextStopsAtAHashAndKeepsTheCarriageReturn)
{
	const auto texts = creatureuploadcfg::Read(NarrowFile("A= a\r\nB=b#c\nC=c\n"));
	EXPECT_EQ(texts.at("A"), "a\r");
	EXPECT_FALSE(texts.contains("B"));
	EXPECT_EQ(texts.at("C"), "c");
}

TEST(CreatureUploadCfg, AnEmptyFileGivesNoTexts)
{
	EXPECT_TRUE(creatureuploadcfg::Read({}).empty());
	EXPECT_TRUE(creatureuploadcfg::Read(Utf16File(u"")).empty());
}
