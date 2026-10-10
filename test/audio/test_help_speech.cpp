/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which sample says a help text, from fake texts and banks, and telling when a voice has finished its line

#include <array>
#include <set>
#include <string>
#include <vector>

#include <Audio/HelpSpeech.h>
#include <gtest/gtest.h>

using namespace openblack::audio;

namespace
{
const std::vector<std::string> k_Texts {"HELP_TEXT_NONE",   "HELP_TEXT_FAMILY_01", "HELP_TEXT_ADVISOR_01",
                                        "HELP_TEXT_AWE_01", "HELP_TEXT_SILENT_01", "HELP_TEXT_BOTH_01"};

std::array<std::vector<SpeechBankSample>, 3> Banks()
{
	return {{
	    // Villagers
	    {{.sample = 1, .file = "C:\\Dialogue\\Characters\\help_text_family_01.wav", .sound = 101},
	     {.sample = 2, .file = "C:\\Dialogue\\Characters\\HELP_TEXT_BOTH_01.wav", .sound = 102}},
	    // HelpSprites
	    {{.sample = 1, .file = "C:\\Dialogue\\HelpSprites\\HELP_TEXT_BOTH_01.wav", .sound = 201},
	     {.sample = 7, .file = "C:/Dialogue/HelpSprites/HELP_TEXT_ADVISOR_01.wav", .sound = 207}},
	    // Guidance
	    {{.sample = 3, .file = "HELP_TEXT_AWE_01.wav", .sound = 303}},
	}};
}
} // namespace

TEST(HelpSpeech, SampleNameIsTheFileName)
{
	EXPECT_EQ(HelpSpeechTable::SampleName("C:\\Dialogue\\HelpSprites\\HELP_TEXT_X_01.wav"), "HELP_TEXT_X_01");
	EXPECT_EQ(HelpSpeechTable::SampleName("a/b/HELP_TEXT_Y.wav"), "HELP_TEXT_Y");
	EXPECT_EQ(HelpSpeechTable::SampleName("HELP_TEXT_Z"), "HELP_TEXT_Z");
}

TEST(HelpSpeech, TextsAreSpokenByTheSampleNamedAfterThem)
{
	const auto banks = Banks();
	const HelpSpeechTable table(k_Texts, banks);
	ASSERT_EQ(table.GetCount(), k_Texts.size());

	EXPECT_EQ(table.Find(1), (SpeechSample {.bank = SpeechBank::Villagers, .sample = 1, .sound = 101}));
	EXPECT_EQ(table.Find(2), (SpeechSample {.bank = SpeechBank::HelpSprites, .sample = 7, .sound = 207}));
	EXPECT_EQ(table.Find(3), (SpeechSample {.bank = SpeechBank::Guidance, .sample = 3, .sound = 303}));
	EXPECT_FALSE(table.Find(4).has_value());
	// The first and the silent text have no sample
	EXPECT_EQ(table.GetSpokenCount(), k_Texts.size() - 2);
}

TEST(HelpSpeech, TheVillagersBankComesFirst)
{
	const auto banks = Banks();
	const HelpSpeechTable table(k_Texts, banks);
	EXPECT_EQ(table.Find(5), (SpeechSample {.bank = SpeechBank::Villagers, .sample = 2, .sound = 102}));
}

TEST(HelpSpeech, NumbersBeyondTheTextsAreTheFirstText)
{
	auto banks = Banks();
	banks[0].push_back({.sample = 9, .file = "HELP_TEXT_NONE.wav", .sound = 109});
	const HelpSpeechTable table(k_Texts, banks);
	EXPECT_EQ(table.Find(1000), table.Find(0));
	EXPECT_EQ(table.Find(0xFFFFFFFF)->sound, 109);
	EXPECT_FALSE(HelpSpeechTable().Find(0).has_value());
}

TEST(HelpSpeech, AVoiceSaysItsLineUntilTheSoundStops)
{
	SpeechVoices voices;
	const SpeechSample family {.bank = SpeechBank::Villagers, .sample = 1, .sound = 101};
	const SpeechSample advisor {.bank = SpeechBank::HelpSprites, .sample = 7, .sound = 207};
	const auto first = static_cast<entt::entity>(5);
	const auto second = static_cast<entt::entity>(6);
	std::set<entt::entity> playing {first, second};
	const auto isPlaying = [&playing](entt::entity emitter) { return playing.contains(emitter); };

	voices.Add(SpeechVoice::First, family, first);
	voices.Add(SpeechVoice::Second, advisor, second);
	voices.Add(SpeechVoice::First, advisor, entt::null);
	EXPECT_EQ(voices.GetCount(), 2);

	EXPECT_TRUE(voices.IsSaying(SpeechVoice::First, family, isPlaying));
	EXPECT_FALSE(voices.IsSaying(SpeechVoice::Second, family, isPlaying));
	EXPECT_TRUE(voices.IsSaying(SpeechVoice::Second, advisor, isPlaying));
	// A sound that never played says nothing
	EXPECT_FALSE(voices.IsSaying(SpeechVoice::First, advisor, isPlaying));

	playing.erase(first);
	EXPECT_FALSE(voices.IsSaying(SpeechVoice::First, family, isPlaying));
	// Forgotten once finished, even if the emitter comes back for something else
	playing.insert(first);
	EXPECT_FALSE(voices.IsSaying(SpeechVoice::First, family, isPlaying));
}
