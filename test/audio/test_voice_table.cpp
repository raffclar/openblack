/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"

// The help texts with their narrator, first argument and name, and the original help text voice table rebuilt from the
// wave names. The installation tests need OPENBLACK_TEST_BW_ROOT (the folder with Scripts\ and Audio\); without it they
// skip.

using namespace openblack;
using namespace openblack::audio;

namespace
{
std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = Lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (Lower(entry.path().filename().string()) == wanted)
			{
				found = entry.path();
				break;
			}
		}
		if (!found)
		{
			return std::nullopt;
		}
		current = *found;
	}
	return current;
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> Utf16(std::u16string_view text)
{
	std::vector<uint8_t> bytes = {0xFF, 0xFE};
	for (const auto c : text)
	{
		bytes.push_back(static_cast<uint8_t>(c & 0xFF));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	return bytes;
}

std::optional<VoiceTable::SampleNames> BankNames(const std::filesystem::path& root, SfxBank bank)
{
	const auto path = FindNoCase(root, SfxBankPath(bank));
	if (!path)
	{
		return std::nullopt;
	}
	std::ifstream stream(*path, std::ios::binary);
	pack::PackFile file;
	if (file.ReadFile(stream) != pack::PackResult::Success)
	{
		return std::nullopt;
	}
	VoiceTable::SampleNames names;
	for (const auto& header : file.GetAudioSampleHeaders())
	{
		names.emplace_back(header.name.begin(), std::find(header.name.begin(), header.name.end(), '\0'));
	}
	return names;
}
} // namespace

TEST(HelpTextDatabase, ConvertScriptText)
{
	EXPECT_EQ(helptext::ConvertScriptText(u"a\\nb"), u"a\nb");
	EXPECT_EQ(helptext::ConvertScriptText(u"a ~b"), u"a\xF8FE"
	                                                u"b");
	EXPECT_EQ(helptext::ConvertScriptText(u"a~ b"), u"a\xF8FE"
	                                                u"b");
	EXPECT_EQ(helptext::ConvertScriptText(u"a~b"), u"a\xF8FE"
	                                               u"b");
	EXPECT_EQ(helptext::ConvertScriptText(u"a\\b $m2"), u"a\\b $m2");
}

TEST(HelpTextDatabase, ParseArguments)
{
	const auto entries =
	    helptext::Parse(Utf16(u"SLONG    HELP_TEXT_NARRATOR_NONE\r\n"
	                          u"HELP_TEXT_NARRATOR_NONE =  0\r\n"
	                          u"HELP_TEXT_NARRATOR_GOOD_SPIRIT =  2\r\n"
	                          u"NUM_ENTRIES( HELP_TEXT_LAST )\r\n"
	                          u"ADD_TEXT( 0, HELP_TEXT_NARRATOR_NONE, \"HELP_TEXT_NONE\", \"Invalid\")\r\n"
	                          u"ADD_TEXT( 1, HELP_TEXT_NARRATOR_GOOD_SPIRIT, \"HELP_TEXT_A\", \"x, y\\nz\")\r\n"
	                          u"ADD_TEXT( 1, HELP_TEXT_NARRATOR_GUIDE, \"HELP_TEXT_B\", \"\")\r\n"));
	ASSERT_EQ(entries.size(), 3u);
	EXPECT_EQ(entries[0].arg0, 0);
	EXPECT_EQ(entries[0].narrator, 0);
	EXPECT_EQ(entries[0].name, "HELP_TEXT_NONE");
	EXPECT_EQ(entries[0].text, u"Invalid");
	EXPECT_EQ(entries[1].arg0, 1);
	EXPECT_EQ(entries[1].narrator, helptext::k_NarratorGoodSpirit);
	EXPECT_EQ(entries[1].name, "HELP_TEXT_A");
	EXPECT_EQ(entries[1].text, u"x, y\nz");
	EXPECT_EQ(entries[2].narrator, helptext::k_NarratorUndeclared);
	EXPECT_EQ(entries[2].text, u"");
}

TEST(VoiceTable, SampleKey)
{
	EXPECT_EQ(VoiceSampleKey("K:\\4frosty\\Spanish\\1622p\\HELP_TEXT_LAND_2_WORKSHOP_10.wav"), "HELP_TEXT_LAND_2_WORKSHOP_10");
	EXPECT_EQ(VoiceSampleKey("help_text_x.WAV"), "HELP_TEXT_X");
	EXPECT_EQ(VoiceSampleKey("a/b/Help_Text_Y"), "HELP_TEXT_Y");
}

TEST(VoiceTable, RuleOrder)
{
	// villagers before HelpSprites before Guidance; the first sample of a name; 1-based samples
	const auto table = VoiceTable::Build(
	    {"HELP_TEXT_NONE", "HELP_TEXT_DUP", "help_text_spirit", "HELP_TEXT_GUIDE"}, {"x\\NULL.wav", "x\\HELP_TEXT_DUP.wav"},
	    {"y\\HELP_TEXT_DUP.wav", "y\\HELP_TEXT_SPIRIT.wav", "y\\HELP_TEXT_SPIRIT.wav"}, {"z\\HELP_TEXT_GUIDE.wav"});
	ASSERT_EQ(table.Size(), 4u);
	EXPECT_FALSE(table.Get(0).HasVoice());
	EXPECT_EQ(table.Get(1).bank, SfxBank::Villagers);
	EXPECT_EQ(table.Get(1).sample, 2u);
	EXPECT_EQ(table.Get(2).bank, SfxBank::HelpSprites);
	EXPECT_EQ(table.Get(2).sample, 2u);
	EXPECT_EQ(table.Get(3).bank, SfxBank::Guidance);
	EXPECT_EQ(table.Get(3).sample, 1u);
	EXPECT_FALSE(table.Get(4).HasVoice());
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(VoiceTable, InstalledGame)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto script = FindNoCase(*root, "Scripts/InfoScript2.txt");
	ASSERT_TRUE(script);
	const auto entries = helptext::Parse(ReadAll(*script));
	// HELP_TEXT_LAST = 6974, the size of the voice table
	ASSERT_EQ(entries.size(), helptext::k_TextCount);
	EXPECT_EQ(entries[0].name, "HELP_TEXT_NONE");
	EXPECT_EQ(entries[0].narrator, 0);
	EXPECT_EQ(entries[0].arg0, 0);

	size_t undeclared = 0;
	size_t arg0One = 0;
	std::vector<std::string> names;
	for (const auto& entry : entries)
	{
		names.push_back(entry.name);
		undeclared += entry.narrator == helptext::k_NarratorUndeclared ? 1 : 0;
		arg0One += entry.arg0 == 1 ? 1 : 0;
		EXPECT_TRUE(entry.narrator >= helptext::k_NarratorUndeclared && entry.narrator <= 12) << entry.name;
	}
	EXPECT_EQ(undeclared, 118u + 24u); // HELP_TEXT_NARRATOR_GUIDE and _MONK
	EXPECT_EQ(arg0One, 6214u);

	const auto villagers = BankNames(*root, SfxBank::Villagers);
	const auto helpSprites = BankNames(*root, SfxBank::HelpSprites);
	const auto guidance = BankNames(*root, SfxBank::Guidance);
	ASSERT_TRUE(villagers && helpSprites && guidance);
	EXPECT_EQ(villagers->size(), 1328u);
	EXPECT_EQ(helpSprites->size(), 1923u);
	EXPECT_EQ(guidance->size(), 227u);

	const auto table = VoiceTable::Build(names, *villagers, *helpSprites, *guidance);
	ASSERT_EQ(table.Size(), helptext::k_TextCount);
	size_t perBank[static_cast<size_t>(SfxBank::_COUNT)] = {};
	// FNV-1a 64 over {u32 bank, u32 sample} of the 6974 entries of the original voice table (little endian)
	uint64_t hash = 0xCBF29CE484222325ull;
	for (uint32_t i = 0; i < table.Size(); ++i)
	{
		const auto voice = table.Get(i);
		++perBank[static_cast<size_t>(voice.bank)];
		for (const uint32_t value : {static_cast<uint32_t>(voice.bank), voice.sample})
		{
			for (int k = 0; k < 4; ++k)
			{
				hash ^= (value >> (8 * k)) & 0xFF;
				hash *= 0x100000001B3ull;
			}
		}
	}
	EXPECT_EQ(perBank[static_cast<size_t>(SfxBank::HelpSprites)], 1922u);
	EXPECT_EQ(perBank[static_cast<size_t>(SfxBank::Villagers)], 1328u);
	EXPECT_EQ(perBank[static_cast<size_t>(SfxBank::Guidance)], 227u);
	EXPECT_EQ(perBank[0], 3497u);
	EXPECT_EQ(hash, 0xCD886474B759EFCCull);

	// the first text with a voice (entry 1009) and the name in two banks
	EXPECT_EQ(table.Get(1009).bank, SfxBank::HelpSprites);
	EXPECT_EQ(table.Get(1009).sample, 1724u);
	const auto workshop = std::find(names.begin(), names.end(), "HELP_TEXT_LAND_2_WORKSHOP_10");
	ASSERT_NE(workshop, names.end());
	const auto voice = table.Get(static_cast<uint32_t>(workshop - names.begin()));
	EXPECT_EQ(voice.bank, SfxBank::Villagers);
	EXPECT_EQ(voice.sample, 399u);
	EXPECT_EQ(VoiceSampleKey((*helpSprites)[801 - 1]), "HELP_TEXT_LAND_2_WORKSHOP_10");
}
