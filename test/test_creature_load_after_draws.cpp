/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <MindFile.h>
#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreaturePhysiology.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"

using namespace openblack;

// A creature now draws its body and its mind as it is made, also when a load makes it. These tests check that what the
// load puts back does not depend on those draws.
namespace
{
/// A draw on the game's own generator from a seed, as a new creature draws
creature_desire_sources::UpTo DrawsFrom(uint32_t& seed)
{
	return [&seed](float most) { return game_random::FloatRand(most, seed); };
}

std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> DesireSetups()
{
	std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> setup {};
	for (auto& desire : setup)
	{
		desire.decayMin = 0.9f;
		desire.decayMax = 1.0f;
		desire.increaseSeconds = 100.0f;
		desire.sources.push_back({.type = 14, .value = 0.5f, .threshold = 0.5f});
		desire.spreads.push_back({.value = 0.2f, .threshold = 0.2f});
	}
	return setup;
}

creaturemind::MindFileData FileWithDesires()
{
	creaturemind::MindFileData file;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto share = static_cast<float>(d) / static_cast<float>(creature_desires::k_DesireCount);
		file.desires.push_back({.activated = static_cast<int32_t>(d % 2),
		                        .value = share,
		                        .max = 1.0f - (share * 0.5f),
		                        .increaseSeconds = 40.0f + static_cast<float>(d),
		                        .sources = {{.value = share, .threshold = 1.0f - share, .type = 14}}});
	}
	return file;
}
} // namespace

// A saved game and a mind file opened in the spawner put the body of their file over the body that the new creature drew
TEST(CreatureLoadAfterDraws, TheBodyOfTheFileReplacesTheDrawnBody)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	uint32_t seed = game_random::k_InitialSeed;
	const auto upTo = DrawsFrom(seed);
	const auto strength = creature_physiology::SpreadStart(0.6f, upTo);
	const auto fatness = creature_physiology::SpreadStart(0.4f, upTo);
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.fatness = fatness, .strength = strength});
	ASSERT_NE(seed, game_random::k_InitialSeed);

	creature_mind_body::Body saved {.species = CreatureType::Tiger};
	saved.alignment = -0.25f;
	saved.strength = 0.8125f;
	saved.fatness = 0.3125f;
	saved.shownFatness = 0.3f;
	ecs::creature_body_file::Apply(registry, creature, saved);
	const auto& body = registry.Get<const ecs::components::Creature>(creature);
	EXPECT_EQ(body.strength, saved.strength);
	EXPECT_EQ(body.fatness, saved.fatness);
	EXPECT_EQ(body.alignment, *saved.alignment);
}

// A mind file keeps every value that it holds, whatever the new creature drew before the file came: two minds made from
// different draws take up the same file to the same values
TEST(CreatureLoadAfterDraws, TheMindOfTheFileDoesNotDependOnTheDrawsBeforeIt)
{
	const auto setup = DesireSetups();
	uint32_t firstSeed = game_random::k_InitialSeed;
	uint32_t secondSeed = 0x13579BDF;
	const auto first = creature_desires::Create(setup, DrawsFrom(firstSeed));
	const auto second = creature_desires::Create(setup, DrawsFrom(secondSeed));
	ASSERT_NE(first.desires.at(0).decay, second.desires.at(0).decay);
	const auto file = FileWithDesires();
	const auto fromFirst = creature_mind_model::FromFile(file, first, creature_mind_model::Fresh(first, {}, {}));
	const auto fromSecond = creature_mind_model::FromFile(file, second, creature_mind_model::Fresh(second, {}, {}));
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto& saved = file.desires.at(d);
		for (const auto* loaded : {&fromFirst.desires.desires.at(d), &fromSecond.desires.desires.at(d)})
		{
			EXPECT_EQ(loaded->activated, saved.activated != 0);
			EXPECT_EQ(loaded->value, saved.value);
			EXPECT_EQ(loaded->max, saved.max);
			EXPECT_EQ(loaded->increaseSeconds, saved.increaseSeconds);
			ASSERT_EQ(loaded->sources.size(), 1u);
			EXPECT_EQ(loaded->sources.front().value, saved.sources.front().value);
			EXPECT_EQ(loaded->sources.front().threshold, saved.sources.front().threshold);
		}
	}
}

// A save of the whole world keeps the creature's drawn body and mind as they are, and its load draws nothing: the
// loaded world saves again to the same bytes
TEST(CreatureLoadAfterDraws, TheWholeWorldKeepsTheDrawnBodyAndMind)
{
	entt::meta_ctx context;
	ecs::reflection::RegisterComponents(context);
	entt::registry original;
	const auto creature = original.create();
	uint32_t seed = game_random::k_InitialSeed;
	const auto upTo = DrawsFrom(seed);
	auto& body = original.emplace<ecs::components::Creature>(creature);
	body.species = CreatureType::Tiger;
	body.strength = creature_physiology::SpreadStart(0.6f, upTo);
	body.fatness = creature_physiology::SpreadStart(0.4f, upTo);
	auto& mind = original.emplace<ecs::components::CreatureMindState>(creature);
	mind.desires = creature_desires::Create(DesireSetups(), upTo);
	mind.innate = creature_desire_sources::SpreadTraits(creature_desire_sources::InnateTraits {.kindness = 0.2f}, upTo);
	const auto seedAfterDraws = seed;

	const auto first = ecs::reflection::SaveWorld(original, context);
	entt::registry loaded;
	const auto result = ecs::reflection::LoadWorld(loaded, context, first, ecs::reflection::WorldLoadMode::Exact);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(ecs::reflection::SaveWorld(loaded, context), first);
	EXPECT_EQ(seed, seedAfterDraws);
	const auto& back = loaded.get<ecs::components::Creature>(creature);
	EXPECT_EQ(back.strength, body.strength);
	EXPECT_EQ(back.fatness, body.fatness);
	const auto& backMind = loaded.get<ecs::components::CreatureMindState>(creature);
	ASSERT_TRUE(backMind.desires.has_value());
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		EXPECT_EQ(backMind.desires->desires.at(d).decay, mind.desires->desires.at(d).decay);
		EXPECT_EQ(backMind.desires->desires.at(d).increaseSeconds, mind.desires->desires.at(d).increaseSeconds);
	}
	EXPECT_EQ(backMind.innate.kindness, mind.innate.kindness);
}
