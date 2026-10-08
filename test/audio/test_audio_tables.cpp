/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/Game/BankTables.h"

// The original's sound effect bank paths and music bank table, and the music flag of the bank info read by PackFile.
// The installation tests need OPENBLACK_TEST_BW_ROOT (the game folder with Audio\); without it they skip.
// BankInfoMusicFlagSynthetic checks the same bank info reading on small banks built in memory.

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

// The original runs on a case-insensitive file system: resolve each component without case
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

void AppendBlock(std::vector<uint8_t>& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.insert(out.end(), blockName.begin(), blockName.end());
	const auto size = static_cast<uint32_t>(data.size());
	const auto* sizeBytes = reinterpret_cast<const uint8_t*>(&size);
	out.insert(out.end(), sizeBytes, sizeBytes + sizeof(size));
	out.insert(out.end(), data.begin(), data.end());
}

/// A .sad in memory with one 4-byte sample; `info` is the body of its bank info block (none when nullopt)
std::vector<uint8_t> MakeBank(const std::optional<std::vector<uint8_t>>& info)
{
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	if (info)
	{
		AppendBlock(file, "LHFileSegmentBankInfo", *info);
	}
	AppendBlock(file, "LHAudioWaveData", std::vector<uint8_t>(4, 0));
	openblack::pack::AudioBankSampleHeader header;
	std::memset(&header, 0, sizeof(header));
	header.id = 1;
	header.size = 4;
	header.offset = 0;
	std::vector<uint8_t> table(4 + sizeof(header), 0);
	const uint16_t count = 1;
	std::memcpy(table.data(), &count, sizeof(count));
	std::memcpy(table.data() + 4, &header, sizeof(header));
	AppendBlock(file, "LHAudioBankSampleTable", table);
	return file;
}

/// The bank info block as the game writes it: three u32, then a title, 532 bytes in all
std::vector<uint8_t> BankInfo(uint32_t first, uint32_t second, uint32_t isMusic)
{
	std::vector<uint8_t> info(532, 0);
	const std::array<uint32_t, 3> words = {first, second, isMusic};
	std::memcpy(info.data(), words.data(), sizeof(words));
	return info;
}
} // namespace

TEST(AudioTables, SfxBankPaths)
{
	// 11 entries, the first empty
	ASSERT_EQ(k_SfxBankPaths.size(), 11u);
	EXPECT_TRUE(k_SfxBankPaths[0].empty());
	EXPECT_EQ(SfxBankPath(SfxBank::InGame), "audio/sfx/game/ingame.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Editor), "audio/sfx/game/editor.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Spells), "audio/sfx/game/spells.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Creature), "audio/sfx/creature/creature.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::ScriptSfx), "audio/sfx/script/scriptsfx.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::HelpSprites), "audio/dialogue/HelpSprites.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Villagers), "audio/dialogue/Villagers.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::VillagersBanter), "audio/dialogue/VillagersBanter.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::SpellDialogue), "audio/dialogue/SpellDialogue.sad");
	EXPECT_EQ(SfxBankPath(SfxBank::Guidance), "audio/dialogue/Guidance.sad");
}

TEST(AudioTables, MusicBanks)
{
	// 85 entries
	ASSERT_EQ(k_MusicBanks.size(), 85u);
	EXPECT_TRUE(MusicBankFor(MusicType::None).path.empty());
	EXPECT_EQ(MusicBankFor(MusicType::None).name, "MUSIC_TYPE_NONE");
	for (size_t i = 1; i < k_MusicBanks.size(); ++i)
	{
		EXPECT_FALSE(k_MusicBanks[i].path.empty()) << i;
		EXPECT_TRUE(k_MusicBanks[i].name.starts_with("MUSIC_TYPE_")) << i;
	}
	EXPECT_EQ(MusicBankFor(MusicType::ScriptCreatureEndSequence).name, "MUSIC_TYPE_SCRIPT_CREATURE_END_SEQUENCE");
	EXPECT_EQ(MusicBankFor(MusicType::GenericGood).path, "audio/music/align/good.sad");
	EXPECT_EQ(MusicBankFor(MusicType::ScriptIntro).path, "audio/music/intro/intro.sad");
	EXPECT_EQ(MusicBankFor(MusicType::Outro).path, "audio/music/outro/outro.sad");

	// Norse town music is the Celtic one (same string pointers)
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownEvil).path, MusicBankFor(MusicType::CelticTownEvil).path);
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownNeutral).path, MusicBankFor(MusicType::CelticTownNeutral).path);
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownGood).path, MusicBankFor(MusicType::CelticTownGood).path);
	EXPECT_EQ(MusicBankFor(MusicType::NorseTownGood).name, "MUSIC_TYPE_NORSE_TOWN_GOOD");
	// one citadel.sad for the 3 alignments
	EXPECT_EQ(MusicBankFor(MusicType::CitadelEvil).path, "audio/music/citadel/citadel.sad");
	EXPECT_EQ(MusicBankFor(MusicType::CitadelNeutral).path, MusicBankFor(MusicType::CitadelEvil).path);
	EXPECT_EQ(MusicBankFor(MusicType::CitadelGood).path, MusicBankFor(MusicType::CitadelEvil).path);
	// the missionaries' verses live in Dialogue
	EXPECT_EQ(MusicBankFor(MusicType::ScriptMissionariesVerse1).path, "audio/dialogue/MissionariesVerse1.sad");
	EXPECT_EQ(MusicBankFor(MusicType::ScriptMissionariesVerse3).path, "audio/dialogue/MissionariesVerse3.sad");
	EXPECT_EQ(MusicBankFor(MusicType::ScriptWelcomeDance).path, "audio/music/script/FollowUsWelcome.sad");

	// 79 different paths: 84 non-null minus the 3 Norse and the 2 extra citadel entries
	std::set<std::string> distinct;
	for (const auto& entry : k_MusicBanks)
	{
		if (!entry.path.empty())
		{
			distinct.insert(Lower(std::string(entry.path)));
		}
	}
	EXPECT_EQ(distinct.size(), 84u - 3u - 2u);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(AudioTables, PathsExistInInstallation)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	for (size_t i = 1; i < k_SfxBankPaths.size(); ++i)
	{
		EXPECT_TRUE(FindNoCase(*root, k_SfxBankPaths[i]).has_value()) << k_SfxBankPaths[i];
	}
	for (size_t i = 1; i < k_MusicBanks.size(); ++i)
	{
		const bool exists = FindNoCase(*root, k_MusicBanks[i].path).has_value();
		// WELCOME_DANCE is the only one without a file
		EXPECT_EQ(exists, static_cast<MusicType>(i) != MusicType::ScriptWelcomeDance) << k_MusicBanks[i].path;
	}
	// MissionariesSad.sad is on disk but not in the table
	EXPECT_TRUE(FindNoCase(*root, "audio/music/script/MissionariesSad.sad").has_value());
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(AudioTables, BankInfoMusicFlag)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	const auto audio = FindNoCase(*root, "audio");
	ASSERT_TRUE(audio.has_value());

	int music = 0;
	int other = 0;
	bool trailer = false;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(*audio))
	{
		if (!entry.is_regular_file() || Lower(entry.path().extension().string()) != ".sad")
		{
			continue;
		}
		const auto relative = Lower(std::filesystem::relative(entry.path(), *audio).generic_string());
		const bool expectMusic = relative.starts_with("music/") || relative.starts_with("dialogue/missionariesverse");

		openblack::pack::PackFile pack;
		ASSERT_EQ(pack.Open(entry.path()), openblack::pack::PackResult::Success) << relative;
		EXPECT_EQ(pack.IsAudioMusicBank(), expectMusic) << relative;
		EXPECT_EQ(pack.GetAudioBankInfo().isMusic, expectMusic ? 1u : 0u) << relative;
		(expectMusic ? music : other)++;
		trailer = trailer || relative == "music/intro/trailer.sad";
		if (relative == "sfx/atmos/ocean.sad")
		{
			// the only bank with the first two u32 set
			EXPECT_EQ(pack.GetAudioBankInfo().unknown0, 7u);
			EXPECT_EQ(pack.GetAudioBankInfo().unknown1, 6u);
		}
	}
	EXPECT_TRUE(trailer);
	// 77 in Music\ + MissionariesVerse1..3; 33 effect, atmos, creature and dialogue banks
	EXPECT_EQ(music, 80);
	EXPECT_EQ(other, 33);
}

TEST(AudioTables, BankInfoMusicFlagSynthetic)
{
	using openblack::pack::PackFile;
	using openblack::pack::PackResult;
	// the third word of the bank info is the music flag
	{
		PackFile pack;
		ASSERT_EQ(pack.Open(MakeBank(BankInfo(0, 0, 1))), PackResult::Success);
		EXPECT_TRUE(pack.IsAudioMusicBank());
		EXPECT_EQ(pack.GetAudioBankInfo().isMusic, 1u);
	}
	{
		PackFile pack;
		ASSERT_EQ(pack.Open(MakeBank(BankInfo(0, 0, 0))), PackResult::Success);
		EXPECT_FALSE(pack.IsAudioMusicBank());
		EXPECT_EQ(pack.GetAudioBankInfo().isMusic, 0u);
	}
	// any non-zero value counts as music
	{
		PackFile pack;
		ASSERT_EQ(pack.Open(MakeBank(BankInfo(0, 0, 2))), PackResult::Success);
		EXPECT_TRUE(pack.IsAudioMusicBank());
	}
	// the first two words are read as they are
	{
		PackFile pack;
		ASSERT_EQ(pack.Open(MakeBank(BankInfo(7, 6, 0))), PackResult::Success);
		EXPECT_EQ(pack.GetAudioBankInfo().unknown0, 7u);
		EXPECT_EQ(pack.GetAudioBankInfo().unknown1, 6u);
		EXPECT_FALSE(pack.IsAudioMusicBank());
	}
	// without the block the bank still loads, all zero
	{
		PackFile pack;
		ASSERT_EQ(pack.Open(MakeBank(std::nullopt)), PackResult::Success);
		EXPECT_FALSE(pack.IsAudioMusicBank());
		EXPECT_EQ(pack.GetAudioBankInfo().unknown0, 0u);
		EXPECT_EQ(pack.GetAudioBankInfo().unknown1, 0u);
	}
	// a block shorter than the three words is refused
	{
		PackFile pack;
		EXPECT_EQ(pack.Open(MakeBank(std::vector<uint8_t>(8, 0))), PackResult::ErrFileTooSmall);
	}
}
