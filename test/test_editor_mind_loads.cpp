/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>

#include "Editor/EditorMindLoads.h"

using namespace openblack;
using namespace openblack::editor::mind_loads;
using player_commands::EditorMindLoad;

namespace
{

/// A mind file of the current version with no learning: the bytes that the creature tools write
std::vector<uint8_t> EmptyMind()
{
	return creaturemind::Write(creaturemind::MindFileData {});
}

/// A world with one creature, which keeps each mind that it takes and each new creature that it makes
struct FakeWorld
{
	entt::entity creature {static_cast<entt::entity>(5)};
	entt::entity next {static_cast<entt::entity>(9)};
	std::vector<std::pair<entt::entity, std::shared_ptr<const creaturemind::MindFileData>>> loads;
	std::vector<player_commands::EditorCreatureSpawn> spawns;

	MindLoadWorld World()
	{
		return {
		    .isCreature = [this](entt::entity entity) { return entity == creature; },
		    .spawn =
		        [this](const EditorMindLoad& load) {
			        spawns.push_back(load.spawn);
			        return next;
		        },
		    .loadMind = [this](entt::entity entity,
		                       std::shared_ptr<const creaturemind::MindFileData> mind) { loads.emplace_back(entity, mind); },
		};
	}
};

} // namespace

TEST(EditorMindLoads, TheHashIsFnv1aOfTheBytes)
{
	// The FNV-1a hash of no bytes is the offset basis, and of "a" it is the known value
	EXPECT_EQ(HashOf({}), 14695981039346656037ULL);
	const std::vector<uint8_t> a {'a'};
	EXPECT_EQ(HashOf(a), 0xAF63DC4C8601EC8CULL);
	const std::vector<uint8_t> b {'b'};
	EXPECT_NE(HashOf(a), HashOf(b));
}

TEST(EditorMindLoads, TheStoreKeepsEachFileOneTimeInTheOrderThatItCame)
{
	MindFileStore store;
	const std::vector<uint8_t> first {1, 2, 3};
	const std::vector<uint8_t> second {4, 5};
	const auto [added, hash] = store.Add(first);
	EXPECT_EQ(added, MindFileStore::Added::New);
	EXPECT_EQ(hash, HashOf(first));
	EXPECT_EQ(store.Add(second).first, MindFileStore::Added::New);
	EXPECT_EQ(store.Add(first).first, MindFileStore::Added::Known);
	ASSERT_EQ(store.Files().size(), 2U);
	EXPECT_EQ(store.Files().front().first, HashOf(first));
	const auto found = store.Find(HashOf(second));
	EXPECT_EQ(std::vector<uint8_t>(found.begin(), found.end()), second);
	EXPECT_TRUE(store.Find(HashOf(first) + 1).empty());
	// An empty file is no mind file
	EXPECT_EQ(store.Add({}).first, MindFileStore::Added::Refused);
	store.Clear();
	EXPECT_TRUE(store.Files().empty());
}

TEST(EditorMindLoads, TheStoreRefusesANewFileWhenItKeepsTheMostFiles)
{
	MindFileStore store;
	for (size_t i = 0; i < k_MostKeptFiles; ++i)
	{
		ASSERT_EQ(store.Add({static_cast<uint8_t>(i), 1}).first, MindFileStore::Added::New);
	}
	EXPECT_EQ(store.Add({0xFF, 1}).first, MindFileStore::Added::Refused);
	// A file that the store keeps is still known
	EXPECT_EQ(store.Add({0, 1}).first, MindFileStore::Added::Known);
	EXPECT_EQ(store.Files().size(), k_MostKeptFiles);
}

TEST(EditorMindLoads, OnlyBytesThatReadAreAMind)
{
	EXPECT_NE(ReadMind(EmptyMind()), nullptr);
	EXPECT_EQ(ReadMind({}), nullptr);
	const std::vector<uint8_t> noise {1, 2, 3, 4, 5, 6, 7};
	EXPECT_EQ(ReadMind(noise), nullptr);
}

TEST(EditorMindLoads, TheRuleTakesAKeptFileForACreatureOrAGoodNewCreature)
{
	const EditorMindLoad load {.use = EditorMindLoad::Use::LoadInto, .file = 1, .creature = static_cast<entt::entity>(3)};
	EXPECT_FALSE(MindLoadRefusal(load, true).has_value());
	EXPECT_TRUE(MindLoadRefusal(load, false).has_value());
	EXPECT_TRUE(MindLoadRefusal({.use = EditorMindLoad::Use::LoadInto, .file = 1}, true).has_value());

	EditorMindLoad spawn {.use = EditorMindLoad::Use::Spawn, .file = 1, .spawn = {.species = 2, .owner = 1}};
	EXPECT_FALSE(MindLoadRefusal(spawn, true).has_value());
	EXPECT_TRUE(MindLoadRefusal(spawn, false).has_value());
	// The new creature has the limits of the creature tools
	spawn.spawn.alignment = 2.0f;
	EXPECT_TRUE(MindLoadRefusal(spawn, true).has_value());
}

TEST(EditorMindLoads, AtTheTurnTheCreatureTakesTheMindOfTheFile)
{
	FakeWorld fake;
	const auto file = EmptyMind();
	const auto result = ApplyMindLoad({.use = EditorMindLoad::Use::LoadInto, .file = HashOf(file), .creature = fake.creature},
	                                  file, fake.World());
	EXPECT_EQ(result.creature, fake.creature);
	EXPECT_TRUE(result.refusal.empty());
	ASSERT_EQ(fake.loads.size(), 1U);
	EXPECT_EQ(fake.loads.front().first, fake.creature);
	ASSERT_NE(fake.loads.front().second, nullptr);
	EXPECT_EQ(fake.loads.front().second->version, creaturemind::k_CurrentVersion);
	EXPECT_TRUE(fake.spawns.empty());
}

TEST(EditorMindLoads, AtTheTurnANewCreatureTakesTheMindOfTheFile)
{
	FakeWorld fake;
	const auto file = EmptyMind();
	const player_commands::EditorCreatureSpawn spawn {.species = 3, .owner = 1, .position = {10.0f, 2.0f, 20.0f}};
	const auto result =
	    ApplyMindLoad({.use = EditorMindLoad::Use::Spawn, .file = HashOf(file), .spawn = spawn}, file, fake.World());
	EXPECT_EQ(result.creature, fake.next);
	ASSERT_EQ(fake.spawns.size(), 1U);
	EXPECT_EQ(fake.spawns.front(), spawn);
	ASSERT_EQ(fake.loads.size(), 1U);
	EXPECT_EQ(fake.loads.front().first, fake.next);
}

TEST(EditorMindLoads, AComputerWithNoFileOrABadFileChangesNothing)
{
	FakeWorld fake;
	const EditorMindLoad load {.use = EditorMindLoad::Use::LoadInto, .file = 7, .creature = fake.creature};
	// No file with the hash came to this computer
	EXPECT_TRUE(ApplyMindLoad(load, {}, fake.World()).creature == entt::null);
	// The bytes do not read
	const std::vector<uint8_t> noise {1, 2, 3};
	EXPECT_TRUE(ApplyMindLoad(load, noise, fake.World()).creature == entt::null);
	// The thing is not a creature
	const auto file = EmptyMind();
	auto other = load;
	other.creature = static_cast<entt::entity>(6);
	EXPECT_TRUE(ApplyMindLoad(other, file, fake.World()).creature == entt::null);
	EXPECT_TRUE(fake.loads.empty());
	EXPECT_TRUE(fake.spawns.empty());
}
