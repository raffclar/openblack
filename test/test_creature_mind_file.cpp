/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <system_error>
#include <vector>

#include <MindBody.h>
#include <MindFile.h>
#include <PhysiqueFile.h>
#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureMindModel.h"

using namespace openblack;
using namespace openblack::creaturemind;

namespace
{
/// A small mind of the current version, every section filled
MindFileData SampleMind()
{
	MindFileData mind;
	mind.version = k_CurrentVersion;
	mind.speciesRow = 2;
	mind.savedAs = {'t', 'e', 's', 't', 0};
	mind.profile = {'P', 0, 'l', 0, 0, 0};
	mind.name = u"Tester";
	mind.counters = {1, 2, 3, 4, 5, 6, 7};
	for (int d = 0; d < 3; ++d)
	{
		mind.desires.push_back({.activated = d % 2,
		                        .value = 0.1f * static_cast<float>(d),
		                        .max = 1.0f,
		                        .increaseSeconds = 20.0f,
		                        .sources = {{.value = 0.5f, .threshold = 0.4f, .type = static_cast<uint32_t>(d)}}});
	}
	mind.treeDesireCount = 1;
	mind.trees = {
	    {.type = 0,
	     .desire = 0,
	     .desireAgain = 0,
	     .episodes = {{.kind = 0,
	                   .desire = 0,
	                   .action = 0,
	                   .belief = {.type = 6, .x = 100, .z = -200, .attributes = {1, 0, 1, 7, 0, 0, 6, 0, 3, 1, 0}},
	                   .feedback = -0.5f}}},
	    {.type = 1, .desire = 0, .desireAgain = 0},
	};
	mind.opinions = std::vector<float> {0.0f, 0.25f, -1.0f};
	mind.desireMemories = {{.value = 1.0f, .count = 2}, {}, {}};
	mind.actionsSeen = {{.count = 3, .turn = 10}};
	mind.miraclesSeen = {{.count = 1, .turn = 20}, {.count = 0, .turn = 0}};
	mind.attitudeToPlayer = 0.3f;
	mind.turnsLookedAt = 1234;
	mind.playerDesires = std::vector<float>(40, 0.5f);
	mind.townDesires = std::vector<float>(17, 0.25f);
	mind.known = {std::vector<MindKnownAction> {{.id = 0}}, std::vector<MindKnownAction> {{.id = 1}}};
	mind.unknown1 = 1;
	mind.alignment = -0.2f;
	mind.developmentTimer = 99;
	mind.developmentPhase = 13;
	mind.physique = {.age = 40,
	                 .strength = 0.6f,
	                 .energy = 0.7f,
	                 .scratch = 0.1f,
	                 .flags = 3,
	                 .needs = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
	                 .unknown3 = 4,
	                 .size = 1.5f,
	                 .blood = {1, 2},
	                 .wounds = {3}};
	mind.database = std::vector<MindDatabaseEntry> {
	    {.id = 5, .time = 6, .count = 3, .pairs = {{.number = 1, .mood = 0.5f, .fear = 0.25f}, {.number = 3, .mood = 1.0f}}}};
	mind.unknown2 = 8;
	mind.unknown3 = std::array<uint32_t, 4> {1, 2, 3, 4};
	mind.tattooHeader = std::array<uint32_t, 8> {};
	mind.tattoo = std::vector<uint8_t> {9, 8, 7};
	mind.learningCounts = std::vector<MindLearningCount>(45, {.a = 1, .b = 2, .c = 3});
	mind.drawnScale = 1.25f;
	mind.unknown4 = 0.5f;
	mind.unknown5 = std::array<int32_t, 2> {7, -7};
	return mind;
}

/// A folder of real creature saves to check against, given by OPENBLACK_CREATURE_SAVES; none by default
std::filesystem::path ReferenceFolder()
{
	const char* folder = std::getenv("OPENBLACK_CREATURE_SAVES");
	return folder != nullptr ? std::filesystem::path(folder) : std::filesystem::path {};
}

/// The creature mind folder of the game, given by OPENBLACK_GAME_PATH; none when it is not on this machine
std::optional<std::filesystem::path> GameMindFolder()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	auto folder = std::filesystem::path(game) / "Scripts" / "CreatureMind";
	std::error_code error;
	if (!std::filesystem::is_directory(folder, error))
	{
		return std::nullopt;
	}
	return folder;
}
} // namespace

TEST(CreatureMindFile, EncryptionRoundTrips)
{
	std::vector<uint8_t> bytes {'H', 'e', 'l', 'l', 'o', 0};
	const auto plain = bytes;
	EncryptBlock(bytes);
	EXPECT_NE(bytes, plain);
	DecryptBlock(bytes);
	EXPECT_EQ(bytes, plain);
}

TEST(CreatureMindFile, CurrentVersionRoundTrips)
{
	const auto mind = SampleMind();
	const auto bytes = Write(mind);
	MindFileData read;
	ASSERT_EQ(Read(bytes, read), MindResult::Success);
	EXPECT_EQ(read.name, u"Tester");
	EXPECT_EQ(read.SavedAsText(), "test");
	EXPECT_EQ(read.ProfileText(), u"Pl");
	ASSERT_EQ(read.desires.size(), 3u);
	EXPECT_FLOAT_EQ(read.desires[2].value, 0.2f);
	ASSERT_EQ(read.trees.size(), 2u);
	ASSERT_EQ(read.trees[0].episodes.size(), 1u);
	EXPECT_EQ(read.trees[0].episodes[0].belief.z, -200);
	EXPECT_EQ(read.trees[0].episodes[0].belief.attributes.size(), 11u);
	ASSERT_TRUE(read.database.has_value());
	ASSERT_EQ(read.database->size(), 1u);
	EXPECT_EQ(read.database->at(0).pairs.size(), 2u);
	EXPECT_EQ(read.physique.size, 1.5f);
	EXPECT_EQ(Write(read), bytes);
}

TEST(CreatureMindFile, OlderVersionsLeaveOutTheirFields)
{
	auto mind = SampleMind();
	mind.version = 12;
	// Version 12 has no seventh counter, no player desires, no tattoo and no body size
	const auto bytes = Write(mind);
	MindFileData read;
	ASSERT_EQ(Read(bytes, read), MindResult::Success);
	EXPECT_EQ(read.counters.size(), 6u);
	EXPECT_FALSE(read.playerDesires.has_value());
	EXPECT_FALSE(read.tattoo.has_value());
	EXPECT_FALSE(read.physique.size.has_value());
	EXPECT_TRUE(read.opinions.has_value());
	EXPECT_TRUE(read.desires[0].before15.has_value());
	EXPECT_EQ(Write(read), bytes);

	mind.version = 8;
	// Version 8 keeps no sources, no opinions, and a number for each desire only versions 6 to 9 have
	const auto older = Write(mind);
	ASSERT_EQ(Read(older, read), MindResult::Success);
	EXPECT_TRUE(read.desires[0].sources.empty());
	EXPECT_TRUE(read.desires[0].from6To9.has_value());
	EXPECT_FALSE(read.opinions.has_value());
	EXPECT_TRUE(read.from6To9.has_value());
	EXPECT_TRUE(read.before11.has_value());
	EXPECT_EQ(Write(read), older);
}

TEST(CreatureMindFile, RejectsWhatIsNotAMind)
{
	MindFileData read;
	EXPECT_EQ(Read(std::vector<uint8_t> {1, 2, 3}, read), MindResult::ErrTruncated);
	auto bytes = Write(SampleMind());
	// A species row past the game's 17, as the version 17 templates have where there is no row
	bytes[4] = 200;
	EXPECT_EQ(Read(bytes, read), MindResult::ErrNotAMindFile);
	bytes = Write(SampleMind());
	bytes[0] = 99;
	EXPECT_EQ(Read(bytes, read), MindResult::ErrUnsupportedVersion);
	bytes = Write(SampleMind());
	bytes.resize(bytes.size() - 5);
	EXPECT_EQ(Read(bytes, read), MindResult::ErrTruncated);
}

TEST(CreatureMindFile, KeepsTrailingBytes)
{
	auto bytes = Write(SampleMind());
	bytes.push_back(0xab);
	MindFileData read;
	ASSERT_EQ(Read(bytes, read), MindResult::Success);
	EXPECT_EQ(read.trailing.size(), 1u);
	EXPECT_EQ(Write(read), bytes);
}

TEST(CreatureMindFile, ModelKeepsAFileItWasLoadedFrom)
{
	// Loaded into a mind and written back without anything learnt since, a file comes out the same
	auto mind = SampleMind();
	mind.desires.resize(40, mind.desires[0]);
	mind.desireMemories.resize(40);
	mind.treeDesireCount = 40;
	mind.trees.clear();
	for (int32_t d = 0; d < 40; ++d)
	{
		mind.trees.push_back({.type = 0, .desire = d, .desireAgain = d});
		mind.trees.push_back({.type = 1, .desire = d, .desireAgain = d});
	}
	mind.trees[2].episodes.push_back({.desire = 1,
	                                  .belief = {.type = 8, .x = 1, .z = 2, .attributes = {1, 0, 1, 0, 0, 12, 8, 2, 3, 23, 29}},
	                                  .feedback = 0.5f});
	mind.opinions = std::vector<float>(3, 0.5f);
	creature_desires::Desires fresh;
	for (auto& desire : fresh.desires)
	{
		desire.sources = {{.type = 0}};
	}
	creature_watching::Knowledge knowledge {
	    .skillsSeen = std::vector<creature_watching::Sighting>(1),
	    .miraclesSeen = std::vector<creature_watching::Sighting>(2),
	    .skillsKnown = std::vector<bool>(1, false),
	    .miraclesKnown = std::vector<bool>(2, false),
	};
	const auto freshLearnt = creature_mind_model::Fresh(fresh, std::vector<float>(3, 0.0f), knowledge);
	const auto loaded = creature_mind_model::FromFile(mind, fresh, freshLearnt);
	EXPECT_EQ(loaded.learnt.name, u"Tester");
	EXPECT_EQ(loaded.developmentPhase, 13u);
	// The turns its player looked at it come back with it
	EXPECT_EQ(loaded.turnsLookedAt, 1234u);
	EXPECT_TRUE(loaded.learnt.knowledge.skillsKnown[0]);
	EXPECT_TRUE(loaded.learnt.knowledge.miraclesKnown[1]);
	// The first tree of desire 1 in the file is its tree of which thing to use
	EXPECT_EQ(loaded.learnt.episodes[static_cast<size_t>(creature_mind_model::TreeKind::Use)][1].size(), 1u);
	const auto saved = creature_mind_model::ToFile(loaded.desires, loaded.learnt, loaded.developmentPhase,
	                                               loaded.attitudeToPlayer, loaded.turnsLookedAt, loaded.speciesRow);
	EXPECT_EQ(Write(saved), Write(mind));
}

TEST(CreatureMindFile, FileExamplesComeAfterTheStartingLesson)
{
	// A creature made from a file has the starting lesson first; the file's examples come after it, up to 16
	MindFileData mind;
	for (int32_t d = 0; d < 5; ++d)
	{
		mind.trees.push_back({.type = 0, .desire = d, .desireAgain = d});
		mind.trees.push_back({.type = 1, .desire = d, .desireAgain = d});
	}
	// The second tree of the hunger desire is its tree of which thing to act on
	for (int32_t i = 0; i < 20; ++i)
	{
		mind.trees[9].episodes.push_back(
		    {.desire = 4, .belief = {.type = 7, .attributes = {0, 0, 1, 0, 0, 0, 7}}, .feedback = -0.5f});
	}
	creature_desires::Desires fresh;
	auto freshLearnt = creature_mind_model::Fresh(fresh, std::vector<float>(3, 0.0f), {});
	const std::array k_Allowed {creature_tree::Attribute::Type};
	creature_mind_model::TeachStartingLesson(freshLearnt, k_Allowed);
	const auto loaded = creature_mind_model::FromFile(mind, fresh, freshLearnt);
	const auto& episodes = loaded.learnt.episodes[static_cast<size_t>(creature_mind_model::TreeKind::ActOn)][4];
	ASSERT_EQ(episodes.size(), creature_tree::k_MaxEpisodes);
	EXPECT_EQ(episodes[0].belief.type, creature_tree::belief_types::k_Villager);
	EXPECT_FLOAT_EQ(episodes[0].feedback, 0.8f);
	EXPECT_EQ(episodes[1].belief.type, 7u);
	EXPECT_FLOAT_EQ(episodes.back().feedback, -0.5f);
}

TEST(CreatureMindFile, ASwappedMindKeepsItsOwnExamplesAndAddsTheFileExamples)
{
	// The other creature's mind file has 10 examples for the same tree
	MindFileData other;
	for (int32_t d = 0; d < 3; ++d)
	{
		other.trees.push_back({.type = 0, .desire = d, .desireAgain = d});
		other.trees.push_back({.type = 1, .desire = d, .desireAgain = d});
	}
	for (int32_t i = 0; i < 10; ++i)
	{
		other.trees[4].episodes.push_back(
		    {.desire = 2, .belief = {.type = 7, .attributes = {0, 0, 1, 0, 0, 0, 7}}, .feedback = -0.5f});
	}
	// The creature's own mind has 9 examples of its own, and one of them knows a skill
	creature_desires::Desires own;
	auto ownLearnt = creature_mind_model::Fresh(own, std::vector<float>(3, 0.0f), {});
	ownLearnt.knowledge.skillsKnown.assign(2, false);
	ownLearnt.knowledge.skillsKnown[1] = true;
	for (int32_t i = 0; i < 9; ++i)
	{
		ownLearnt.episodes[1][2].push_back({.belief = {.type = 3}, .feedback = 0.25f});
	}
	const auto loaded = creature_mind_model::FromFile(other, own, ownLearnt);
	const auto& episodes = loaded.learnt.episodes[1][2];
	// Its own 9 examples come first, then the file's examples until the tree holds 16
	ASSERT_EQ(episodes.size(), creature_tree::k_MaxEpisodes);
	EXPECT_TRUE(std::ranges::all_of(episodes.begin(), episodes.begin() + 9,
	                                [](const auto& episode) { return episode.belief.type == 3u; }));
	EXPECT_TRUE(std::ranges::all_of(episodes.begin() + 9, episodes.end(),
	                                [](const auto& episode) { return episode.belief.type == 7u; }));
	// The skills that it knew stay known
	EXPECT_TRUE(loaded.learnt.knowledge.skillsKnown[1]);
}

TEST(CreatureMindFile, FreshMindWritesACurrentFile)
{
	creature_desires::Desires desires;
	const auto learnt = creature_mind_model::Fresh(desires, std::vector<float>(328, 0.0f), {});
	const auto file = creature_mind_model::ToFile(desires, learnt, 2, 0.0f, 0, 3);
	const auto bytes = Write(file);
	MindFileData read;
	ASSERT_EQ(Read(bytes, read), MindResult::Success);
	EXPECT_EQ(read.version, k_CurrentVersion);
	EXPECT_EQ(read.desires.size(), 40u);
	EXPECT_EQ(read.trees.size(), 80u);
	EXPECT_EQ(read.opinions->size(), 328u);
	EXPECT_EQ(read.developmentPhase, 2);
}

/// Real creature saves are players' own files, not part of the project: this only runs on a folder of them given by
/// OPENBLACK_CREATURE_SAVES
TEST(CreatureMindFile, ReferenceSavesParseAndRoundTrip)
{
	const auto folder = ReferenceFolder();
	if (folder.empty() || !std::filesystem::is_directory(folder))
	{
		GTEST_SKIP() << "Set OPENBLACK_CREATURE_SAVES to a folder of creature saves to check them";
	}
	size_t files = 0;
	for (const auto& entry : std::filesystem::directory_iterator(folder))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}
		const auto bytes = ReadFileBytes(entry.path()).value_or(std::vector<uint8_t> {});
		MindFileData mind;
		ASSERT_EQ(Read(bytes, mind), MindResult::Success) << entry.path();
		EXPECT_EQ(mind.version, k_CurrentVersion) << entry.path();
		EXPECT_EQ(mind.desires.size(), 40u);
		EXPECT_EQ(mind.trees.size(), 80u);
		EXPECT_EQ(Write(mind), bytes) << entry.path() << " doesn't write back the same";
		++files;
	}
	EXPECT_GT(files, 0u);
}

/// The game's own minds and the creature files beside them read and write back the same, byte for byte. The game cannot
/// load the four template minds in the old version 17 layout, which has no species row, so the read refuses them too.
TEST(CreatureMindFile, ShippedFilesWriteBackTheSame)
{
	const auto folder = GameMindFolder();
	if (!folder.has_value())
	{
		GTEST_SKIP() << "Set OPENBLACK_GAME_PATH to the game's folder to check its creature files";
	}
	size_t minds = 0;
	size_t physiques = 0;
	for (const auto& entry : std::filesystem::directory_iterator(*folder))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}
		const auto bytes = ReadFileBytes(entry.path());
		ASSERT_TRUE(bytes.has_value()) << entry.path();
		if (IsPhysiqueFileName(entry.path().filename().string()))
		{
			const auto physique = ReadPhysique(*bytes);
			ASSERT_TRUE(physique.has_value()) << entry.path();
			EXPECT_EQ(WritePhysique(*physique), *bytes) << entry.path() << " does not write back the same";
			++physiques;
			continue;
		}
		MindFileData mind;
		const auto result = Read(*bytes, mind);
		if (result == MindResult::ErrNotAMindFile)
		{
			ASSERT_GE(bytes->size(), 4u);
			EXPECT_EQ((*bytes)[0], 17u) << entry.path() << " is not in the old version 17 layout";
			continue;
		}
		ASSERT_EQ(result, MindResult::Success) << entry.path();
		EXPECT_EQ(Write(mind), *bytes) << entry.path() << " does not write back the same";
		++minds;
	}
	EXPECT_GT(minds, 0u);
	EXPECT_GT(physiques, 0u);
}

TEST(CreatureMindFile, FileWritesAndReadsBack)
{
	const auto path = std::filesystem::path(TEST_BINARY_DIR) / "creature_mind_file_round_trip.erc";
	const auto mind = SampleMind();
	ASSERT_EQ(WriteFile(path, mind), MindResult::Success);
	const auto bytes = ReadFileBytes(path);
	ASSERT_TRUE(bytes.has_value());
	EXPECT_EQ(*bytes, Write(mind));
	MindFileData read;
	ASSERT_EQ(ReadFile(path, read), MindResult::Success);
	EXPECT_EQ(Write(read), *bytes);
	std::filesystem::remove(path);
	EXPECT_EQ(ReadFile(path, read), MindResult::ErrCantOpen);
}

TEST(CreatureMindFile, PhysiqueFileSitsBesideTheMind)
{
	EXPECT_EQ(PhysiqueFileName("C4a4f6e63.erc"), "PhysiqueC4a4f6e63.erc");
	EXPECT_TRUE(IsPhysiqueFileName("PhysiqueC4a4f6e63.erc"));
	EXPECT_FALSE(IsPhysiqueFileName("C4a4f6e63.erc"));
	EXPECT_FALSE(IsPhysiqueFileName("LethysCreature"));

	const auto path = std::filesystem::path(TEST_BINARY_DIR) / PhysiqueFileName("creature_mind_file_test.erc");
	const PhysiqueFileData physique {.speciesRow = 3,
	                                 .drawnSize = 1.5f,
	                                 .strength = 0.25f,
	                                 .fatness = 0.75f,
	                                 .alignment = -0.5f,
	                                 .blood = {0x00011234u},
	                                 .wounds = {0xC5000102u, 0xC6000304u}};
	ASSERT_TRUE(WritePhysiqueFile(path, physique));
	EXPECT_EQ(ReadPhysiqueFile(path), physique);
	std::filesystem::remove(path);
	EXPECT_FALSE(ReadPhysiqueFile(path).has_value());
}

TEST(CreatureMindFile, MarkWordsRoundTrip)
{
	const MindMark wound {.u = 0x12, .v = 0x34, .skin = 2, .age = 5, .type = 6, .column = 3};
	EXPECT_EQ(WoundToWord(wound), 0xDE853412u);
	EXPECT_EQ(WoundFromWord(WoundToWord(wound)), wound);
	const MindMark blood {.u = 7, .v = 9, .skin = 1, .age = 63};
	EXPECT_EQ(BloodToWord(blood), 0x007F0907u);
	EXPECT_EQ(BloodFromWord(BloodToWord(blood)), blood);
}

TEST(CreatureMindFile, TattooWordsRoundTrip)
{
	const MindTattoo tattoo {.design = 1, .site = 2, .red = 0x04, .green = 0x08, .blue = 0x0C};
	EXPECT_EQ(TattooToWord(tattoo), 0x04080C21u);
	EXPECT_EQ(TattooFromWord(0x04080C21u), tattoo);
	EXPECT_EQ(TattooFromWord(TattooToWord({})).site, k_NoTattooSite);
}

TEST(CreatureMindFile, TattooPicturesShareOnePictureForADesign)
{
	std::array<std::optional<TattooPicture>, k_TattooDesigns> pictures {};
	auto& picture = pictures.at(5).emplace();
	for (size_t i = 0; i < picture.size(); ++i)
	{
		picture.at(i) = static_cast<uint8_t>(i % 16);
	}
	std::array<std::optional<uint8_t>, k_TattooSlots> slots {};
	slots.at(1) = 5;
	slots.at(4) = 5;
	// A design with no picture of the player's own uses no picture
	slots.at(6) = 2;
	const auto block = WriteTattooPictures(slots, pictures);
	constexpr size_t k_Header = 8 + (4 * k_TattooSlots);
	ASSERT_EQ(block.size(), k_Header + (picture.size() / 2));
	EXPECT_EQ(block.at(0), static_cast<uint8_t>(block.size()));
	EXPECT_EQ(block.at(1), static_cast<uint8_t>(block.size() >> 8u));
	EXPECT_EQ(block.at(4), 1u);
	EXPECT_EQ(block.at(5), (1u << 1u) | (1u << 4u));
	EXPECT_EQ(block.at(k_Header), 0x10u);
	EXPECT_EQ(ReadTattooPicture(block, 1), picture);
	EXPECT_EQ(ReadTattooPicture(block, 4), picture);
	EXPECT_FALSE(ReadTattooPicture(block, 6).has_value());
	EXPECT_FALSE(ReadTattooPicture(std::span(block).first(block.size() - 1), 1).has_value());
}

namespace
{
/// The tree of which thing to act on of the hunger desire, the second tree of desire 4 in a file
constexpr size_t k_HungerActOnTree = 9;

/// The starting lesson as a mind file keeps it, as the game's own files have it
MindEpisode FileStartingLesson()
{
	return {.kind = 2,
	        .desire = 4,
	        .action = 0,
	        .belief = {.type = 6, .x = 0, .z = 0, .attributes = {1, 0, 0, 0, 0, 0, 6, 0, 0, 1, 1}},
	        .feedback = 0.8f};
}

/// The mind file that the game writes for a creature after a load of a saved game, with no turn between. The load makes a
/// new creature, which has the starting lesson, and puts the file over it:
/// - each example of the file keeps no kind of lesson (0), and follows the examples that the tree has, up to 16
/// - the word after the known actions is 1
MindFileData AsTheGameSavesItAfterALoad(MindFileData file)
{
	file.unknown1 = 1;
	for (auto& tree : file.trees)
	{
		for (auto& episode : tree.episodes)
		{
			episode.kind = 0;
		}
	}
	auto& hunger = file.trees.at(k_HungerActOnTree).episodes;
	hunger.insert(hunger.begin(), FileStartingLesson());
	for (auto& tree : file.trees)
	{
		if (tree.episodes.size() > creature_tree::k_MaxEpisodes)
		{
			tree.episodes.resize(creature_tree::k_MaxEpisodes);
		}
	}
	return file;
}

/// What openblack writes for a creature that a load of a saved game gives the file to: a new mind with the starting
/// lesson, the file over it, then the mind and the body written back as the save writes them
std::vector<uint8_t> LoadedAndSaved(const MindFileData& file)
{
	creature_desires::Desires fresh;
	creature_watching::Knowledge knowledge {
	    .skillsSeen = std::vector<creature_watching::Sighting>(file.actionsSeen.size()),
	    .miraclesSeen = std::vector<creature_watching::Sighting>(file.miraclesSeen.size()),
	    .skillsKnown = std::vector<bool>(file.actionsSeen.size(), false),
	    .miraclesKnown = std::vector<bool>(file.miraclesSeen.size(), false),
	};
	const auto opinions = file.opinions.has_value() ? file.opinions->size() : 0;
	auto freshLearnt = creature_mind_model::Fresh(fresh, std::vector<float>(opinions, 0.0f), knowledge);
	const std::array k_Allowed {creature_tree::Attribute::Type};
	creature_mind_model::TeachStartingLesson(freshLearnt, k_Allowed);
	const auto loaded = creature_mind_model::FromFile(file, fresh, freshLearnt);
	auto saved = creature_mind_model::ToFile(loaded.desires, loaded.learnt, loaded.developmentPhase, loaded.attitudeToPlayer,
	                                         loaded.turnsLookedAt, loaded.speciesRow);
	const auto body = creature_mind_body::FromMindFile(file);
	if (body.has_value())
	{
		creature_mind_body::ToMindFile(*body, saved);
		saved.name = body->name;
	}
	return Write(saved);
}

/// A mind with the 40 desires and 80 trees of a current file, and one example in each tree of two desires
MindFileData FullSampleMind()
{
	auto mind = SampleMind();
	// A wound word of the game always has the two top bits
	mind.physique.wounds = {0xC0000003u};
	mind.desires.resize(40, mind.desires[0]);
	mind.desireMemories.resize(40);
	mind.treeDesireCount = 40;
	mind.trees.clear();
	for (int32_t d = 0; d < 40; ++d)
	{
		mind.trees.push_back({.type = 0, .desire = d, .desireAgain = d});
		mind.trees.push_back({.type = 1, .desire = d, .desireAgain = d});
	}
	const MindEpisode example {.kind = 1,
	                           .desire = 2,
	                           .action = 17,
	                           .belief = {.type = 7, .x = 5, .z = -9, .attributes = {0, 0, 1, 0, 0, 0, 7}},
	                           .feedback = -0.25f};
	mind.trees[4].episodes.push_back(example);
	mind.trees[5].episodes.push_back(example);
	mind.trees[k_HungerActOnTree].episodes.push_back(FileStartingLesson());
	mind.opinions = std::vector<float>(3, 0.5f);
	mind.unknown1 = 0;
	return mind;
}

/// Every mind file of a saved game in a copy of a Profiles folder (OPENBLACK_VANILLA_SAVES), and every mind of the game
/// (OPENBLACK_GAME_PATH)
std::vector<std::filesystem::path> RealMindFiles()
{
	std::vector<std::filesystem::path> files;
	std::error_code error;
	if (const char* saves = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	    saves != nullptr && std::filesystem::is_directory(saves, error))
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(saves, error))
		{
			if (entry.is_regular_file() && entry.path().filename().string().starts_with("SavedCreature"))
			{
				files.push_back(entry.path());
			}
		}
	}
	if (const auto folder = GameMindFolder(); folder.has_value())
	{
		for (const auto& entry : std::filesystem::directory_iterator(*folder, error))
		{
			if (entry.is_regular_file() && !IsPhysiqueFileName(entry.path().filename().string()))
			{
				files.push_back(entry.path());
			}
		}
	}
	return files;
}
} // namespace

TEST(CreatureMindFile, TreesGoByTheirPlaceInTheFile)
{
	// For each desire the first tree in the file is the tree of which thing to use, the second of which thing to act on,
	// whatever kind number and desire the file gives them
	auto mind = FullSampleMind();
	mind.trees[4].type = 5;
	mind.trees[4].desire = 30;
	mind.trees[4].desireAgain = 31;
	creature_desires::Desires fresh;
	const auto freshLearnt = creature_mind_model::Fresh(fresh, std::vector<float>(3, 0.0f), {});
	const auto loaded = creature_mind_model::FromFile(mind, fresh, freshLearnt);
	const auto use = static_cast<size_t>(creature_mind_model::TreeKind::Use);
	const auto actOn = static_cast<size_t>(creature_mind_model::TreeKind::ActOn);
	EXPECT_EQ(loaded.learnt.episodes[use][2].size(), 1u);
	EXPECT_EQ(loaded.learnt.episodes[actOn][2].size(), 1u);
	EXPECT_EQ(loaded.learnt.episodes[actOn][4].size(), 1u);
	EXPECT_TRUE(loaded.learnt.episodes[use][4].empty());
	ASSERT_TRUE(loaded.learnt.treeHeaders[use][2].has_value());
	// The game reads the desire twice into one place, so the second one stays
	EXPECT_EQ(loaded.learnt.treeHeaders[use][2]->type, 5);
	EXPECT_EQ(loaded.learnt.treeHeaders[use][2]->desire, 31);
	const auto saved = creature_mind_model::ToFile(loaded.desires, loaded.learnt, loaded.developmentPhase,
	                                               loaded.attitudeToPlayer, loaded.turnsLookedAt, loaded.speciesRow);
	EXPECT_EQ(saved.trees[4].type, 5);
	EXPECT_EQ(saved.trees[4].desire, 31);
	EXPECT_EQ(saved.trees[4].desireAgain, 31);
	EXPECT_EQ(saved.trees[5].type, 1);
}

TEST(CreatureMindFile, ANewMindWritesItsUseTreeFirst)
{
	creature_desires::Desires desires;
	auto learnt = creature_mind_model::Fresh(desires, std::vector<float>(3, 0.0f), {});
	const std::array k_Allowed {creature_tree::Attribute::Type};
	creature_mind_model::TeachStartingLesson(learnt, k_Allowed);
	const auto file = creature_mind_model::ToFile(desires, learnt, 0, 0.0f, 0, 3);
	ASSERT_EQ(file.trees.size(), 80u);
	for (size_t i = 0; i < file.trees.size(); ++i)
	{
		EXPECT_EQ(file.trees[i].type, static_cast<int32_t>(i % 2)) << i;
		EXPECT_EQ(file.trees[i].desire, static_cast<int32_t>(i / 2)) << i;
	}
	// The starting lesson is in the second tree of the hunger desire, as in the game's own files
	const auto& hunger = file.trees[k_HungerActOnTree];
	ASSERT_EQ(hunger.episodes.size(), 1u);
	const auto lesson = FileStartingLesson();
	EXPECT_EQ(hunger.episodes[0].kind, lesson.kind);
	EXPECT_EQ(hunger.episodes[0].desire, lesson.desire);
	EXPECT_EQ(hunger.episodes[0].belief.type, lesson.belief.type);
	EXPECT_EQ(hunger.episodes[0].belief.attributes, lesson.belief.attributes);
	EXPECT_EQ(hunger.episodes[0].feedback, lesson.feedback);
}

TEST(CreatureMindFile, ALoadAndASaveChangeTheFileAsTheGameDoes)
{
	const auto mind = FullSampleMind();
	const auto bytes = LoadedAndSaved(mind);
	EXPECT_EQ(bytes, Write(AsTheGameSavesItAfterALoad(mind)));
	// Each example keeps the desire and the action that the file gave it, but not its kind of lesson
	MindFileData read;
	ASSERT_EQ(Read(bytes, read), MindResult::Success);
	EXPECT_EQ(read.trees[4].episodes[0].desire, 2);
	EXPECT_EQ(read.trees[4].episodes[0].action, 17);
	EXPECT_EQ(read.trees[4].episodes[0].kind, 0);
	EXPECT_EQ(read.trees[k_HungerActOnTree].episodes.size(), 2u);
	EXPECT_EQ(read.unknown1, std::optional<uint32_t>(1));
}

TEST(CreatureMindFile, AFullTreeLosesTheExamplesPastSixteen)
{
	auto mind = FullSampleMind();
	for (int32_t i = 0; i < 20; ++i)
	{
		mind.trees[k_HungerActOnTree].episodes.push_back({.desire = 4,
		                                                  .belief = {.type = 7, .attributes = {0, 0, 1, 0, 0, 0, 7}},
		                                                  .feedback = 0.1f * static_cast<float>(i)});
	}
	const auto expected = AsTheGameSavesItAfterALoad(mind);
	ASSERT_EQ(expected.trees[k_HungerActOnTree].episodes.size(), creature_tree::k_MaxEpisodes);
	EXPECT_EQ(LoadedAndSaved(mind), Write(expected));
}

TEST(CreatureMindFile, ALoadKeepsTheValuesAsTheFileHasThem)
{
	// Out of their usual ranges, the opinions, the attitude and the body values come back as they were
	auto mind = FullSampleMind();
	(*mind.opinions)[1] = 1.5f;
	mind.attitudeToPlayer = -3.0f;
	mind.physique.strength = 2.0f;
	mind.physique.size = 7.0f;
	mind.physique.fatness = 1.25f;
	mind.physique.energy = -0.5f;
	EXPECT_EQ(LoadedAndSaved(mind), Write(AsTheGameSavesItAfterALoad(mind)));
}

TEST(CreatureMindFile, ALoadKeepsTheAlignmentFromMinusOneToOne)
{
	EXPECT_EQ(creature_mind_body::AlignmentFromFile(0.25f), 0.25f);
	EXPECT_EQ(creature_mind_body::AlignmentFromFile(-1.5f), -1.0f);
	EXPECT_EQ(creature_mind_body::AlignmentFromFile(1.5f), 1.0f);
	EXPECT_EQ(creature_mind_body::AlignmentFromFile(std::numeric_limits<float>::quiet_NaN()), -1.0f);
	auto mind = FullSampleMind();
	mind.alignment = 4.0f;
	auto expected = AsTheGameSavesItAfterALoad(mind);
	expected.alignment = 1.0f;
	EXPECT_EQ(LoadedAndSaved(mind), Write(expected));
}

/// The real minds on this machine: the mind files of the saved games in a copy of a Profiles folder
/// (OPENBLACK_VANILLA_SAVES) and the game's own minds (OPENBLACK_GAME_PATH). Each one, loaded and saved, is what the game
/// writes after a load.
TEST(CreatureMindFile, RealMindsLoadAndSaveAsTheGameDoes)
{
	const auto files = RealMindFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES or OPENBLACK_GAME_PATH to check real mind files";
	}
	size_t checked = 0;
	for (const auto& path : files)
	{
		const auto bytes = ReadFileBytes(path);
		ASSERT_TRUE(bytes.has_value()) << path;
		MindFileData mind;
		const auto result = Read(*bytes, mind);
		if (result == MindResult::ErrNotAMindFile)
		{
			continue;
		}
		ASSERT_EQ(result, MindResult::Success) << path;
		ASSERT_EQ(Write(mind), *bytes) << path << " does not write back the same";
		// The game writes every mind of a saved game and of a player in the current version. Its older computer minds
		// come out in the current version too, with what the creature has for the fields that they lack.
		if (mind.version != k_CurrentVersion)
		{
			continue;
		}
		ASSERT_EQ(mind.trees.size(), 80u) << path;
		EXPECT_EQ(LoadedAndSaved(mind), Write(AsTheGameSavesItAfterALoad(mind)))
		    << path << " is not what the game writes after a load";
		++checked;
	}
	EXPECT_GT(checked, 0u);
}
