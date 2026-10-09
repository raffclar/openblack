/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <memory>

#include <MindFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureCarryOver.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreaturePhysiology.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/Registry.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/CreatureCarryOverSystem.h"

using namespace openblack;
using namespace openblack::creature_carry_over;

namespace
{
creature_marks::Mark Wound()
{
	return {.u = 0x12, .v = 0x34, .skin = 2, .age = 5, .type = 6, .column = 3};
}

creature_mind_body::Body GrownBody()
{
	creature_mind_body::Body body {.species = CreatureType::Leopard, .name = u"Spot"};
	body.alignment = -0.5f;
	body.strength = 0.7f;
	body.size = 1.8f;
	body.fatness = 0.8f;
	body.shownFatness = 0.6f;
	body.needs = creature_physiology::Kept {.age = 40, .turns = 144000, .energy = 0.9f, .exhaustion = 0.25f};
	creature_tattoo::Slots tattoos {};
	tattoos.at(0) = creature_tattoo::FromWord(0x04080C21);
	body.tattoos = tattoos;
	body.wounds = {Wound()};
	body.blood = {{.u = 1, .v = 2, .skin = 1, .age = 10}};
	return body;
}
} // namespace

TEST(CreatureCarryOver, AWoundIsKeptAsAWordOfItsTexelSkinAgeColumnAndKind)
{
	EXPECT_EQ(creature_marks::WoundToWord(Wound()), 0xDE853412u);
	EXPECT_EQ(creature_marks::WoundFromWord(0xDE853412u), Wound());
}

TEST(CreatureCarryOver, BloodIsKeptAsAWordOfItsTexelSkinAndAge)
{
	const creature_marks::Mark blood {.u = 7, .v = 9, .skin = 3, .age = 63};
	EXPECT_EQ(creature_marks::BloodToWord(blood), 0x00FF0907u);
	// What the top byte of a drop of blood holds isn't read
	EXPECT_EQ(creature_marks::BloodFromWord(0xAAFF0907u), blood);
}

TEST(CreatureCarryOver, TheBodyGoesThroughTheFileAsItWas)
{
	creaturemind::MindFileData file;
	file.speciesRow = 3;
	file.name = u"Spot";
	creature_mind_body::ToMindFile(GrownBody(), file);
	EXPECT_EQ(file.physique.fatness, 0.8f);
	EXPECT_EQ(file.physique.shownFatness, 0.6f);
	EXPECT_EQ(file.physique.needs.at(1), 0.25f);

	const auto body = creature_mind_body::FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	const auto grown = GrownBody();
	EXPECT_EQ(body->species, grown.species);
	EXPECT_EQ(body->alignment, grown.alignment);
	EXPECT_EQ(body->strength, grown.strength);
	EXPECT_EQ(body->size, grown.size);
	EXPECT_EQ(body->fatness, grown.fatness);
	EXPECT_EQ(body->shownFatness, grown.shownFatness);
	EXPECT_EQ(body->needs, grown.needs);
	ASSERT_TRUE(body->tattoos.has_value());
	EXPECT_EQ(body->tattoos->at(0), grown.tattoos->at(0));
	EXPECT_EQ(body->wounds, grown.wounds);
	EXPECT_EQ(body->blood, grown.blood);
}

TEST(CreatureCarryOver, ABodyNotYetLookedAfterLeavesTheFilesNeedsAlone)
{
	creaturemind::MindFileData file;
	file.physique.age = 12;
	file.physique.energy = 0.4f;
	auto body = GrownBody();
	body.needs.reset();
	creature_mind_body::ToMindFile(body, file);
	EXPECT_EQ(file.physique.age, 12u);
	EXPECT_EQ(file.physique.energy, 0.4f);
}

TEST(CreatureCarryOver, AKeptBodyStartsFromItsSpeciesWithWhatItKept)
{
	const creature_physiology::Species species {.startEnergy = 0.5f, .startWarmth = 0.2f};
	const auto needs = creature_physiology::Start(species, {.age = 40, .turns = 7, .energy = 0.9f, .exhaustion = 0.3f});
	EXPECT_EQ(needs.age, 40u);
	EXPECT_EQ(needs.turns, 7u);
	EXPECT_EQ(needs.energy, 0.9f);
	EXPECT_EQ(needs.exhaustion, 0.3f);
	// Its life, warmth and the rest start again
	EXPECT_EQ(needs.warmth, 0.2f);
	EXPECT_EQ(needs.life, 1.0f);
	EXPECT_EQ(needs.poo, 0.0f);
}

TEST(CreatureCarryOver, ALoadedCreatureSparklesIntoSightOverThreeSeconds)
{
	auto fizz = ArrivalFizz();
	EXPECT_EQ(fizz.now, 1.0f);
	EXPECT_EQ(fizz.target, 0.0f);
	EXPECT_FLOAT_EQ(fizz.perSecond, -1.0f / 3.0f);
	for (int turn = 0; turn < 15; ++turn)
	{
		fizz = StepFizz(fizz, 100.0f);
	}
	EXPECT_NEAR(fizz.now, 0.5f, 1e-5f);
	for (int turn = 0; turn < 15; ++turn)
	{
		fizz = StepFizz(fizz, 100.0f);
	}
	EXPECT_EQ(fizz.now, 0.0f);
	EXPECT_EQ(fizz.perSecond, 0.0f);
	EXPECT_EQ(StepFizz(fizz, 100.0f), fizz);
}

TEST(CreatureCarryOver, AFizzOverNoSecondsIsThereAtOnce)
{
	const auto fizz = SetFizz({.now = 0.3f}, 2.0f, 0.0f);
	EXPECT_EQ(fizz.now, 1.0f);
	EXPECT_EQ(fizz.perSecond, 0.0f);
	EXPECT_FLOAT_EQ(SetFizz({}, 1.0f, 2.0f).perSecond, 0.5f);
}

TEST(CreatureCarryOver, OnlyThePlacesCellIsTakenAndTheCreatureStandsInItsMiddle)
{
	const auto coords = ArrivalCoords({1234.4f, 456.9f});
	EXPECT_EQ(map_coords::CellX(coords), 123);
	EXPECT_EQ(map_coords::CellZ(coords), 45);
	EXPECT_FLOAT_EQ(map_coords::ToMetres(coords.x), 1235.0f);
	EXPECT_FLOAT_EQ(map_coords::ToMetres(coords.z), 455.0f);
	EXPECT_EQ(coords.altitude, 0.0f);
}

class CreatureBodyFile: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static entt::entity MakeCreature(bool bodyStarted)
	{
		using namespace ecs::components;
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Creature>(entity, PlayerNames::PLAYER_ONE, true, CreatureType::Leopard, entt::id_type {0});
		registry.Assign<CreatureMorph>(entity);
		auto& needs = registry.Assign<CreatureNeeds>(entity);
		needs.started = bodyStarted;
		registry.Assign<CreatureTattoos>(entity);
		registry.Assign<CreatureMarks>(entity);
		return entity;
	}
};

TEST_F(CreatureBodyFile, ACreatureMadeFromAFileTakesUpTheBodyItKept)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto grown = MakeCreature(true);
	ecs::creature_body_file::Apply(registry, grown, GrownBody());
	const auto kept = ecs::creature_body_file::Capture(registry, grown);
	EXPECT_EQ(kept.alignment, -0.5f);
	EXPECT_EQ(kept.size, 1.8f);
	EXPECT_EQ(kept.fatness, 0.8f);
	EXPECT_EQ(kept.shownFatness, 0.6f);
	EXPECT_EQ(kept.needs, GrownBody().needs);
	EXPECT_EQ(kept.wounds, GrownBody().wounds);

	// A new creature, whose body starts on its first turn, starts from what was kept
	const auto loaded = MakeCreature(false);
	ecs::creature_body_file::Apply(registry, loaded, kept);
	const auto& body = registry.Get<Creature>(loaded);
	EXPECT_EQ(body.alignment, -0.5f);
	EXPECT_EQ(body.strength, 0.7f);
	EXPECT_EQ(body.fatness, 0.8f);
	EXPECT_EQ(body.size, 1.8f);
	EXPECT_EQ(registry.Get<CreatureMorph>(loaded).shownFatness, 0.6f);
	EXPECT_EQ(registry.Get<CreatureNeeds>(loaded).kept, GrownBody().needs);
	EXPECT_EQ(registry.Get<CreatureTattoos>(loaded).slots.at(0), GrownBody().tattoos->at(0));
	EXPECT_EQ(registry.Get<CreatureMarks>(loaded).marks.blood, GrownBody().blood);
	// Its kept needs are still what it has, though its body hasn't started yet
	EXPECT_EQ(ecs::creature_body_file::Capture(registry, loaded).needs, GrownBody().needs);
}

class CreatureCarryOverSystem: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The system reports what it keeps and loads to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	ecs::systems::CreatureCarryOverSystem _carryOver;
};

TEST_F(CreatureCarryOverSystem, WithNoCreatureWhatWasKeptBeforeStays)
{
	auto file = std::make_shared<creaturemind::MindFileData>();
	file->name = u"Spot";
	_carryOver.Keep(file);
	_carryOver.KeepPlayersCreature();
	_carryOver.Keep(nullptr);
	ASSERT_NE(_carryOver.Kept(), nullptr);
	EXPECT_EQ(_carryOver.Kept()->name, u"Spot");

	auto next = std::make_shared<creaturemind::MindFileData>();
	next->name = u"Rex";
	_carryOver.Keep(next);
	EXPECT_EQ(_carryOver.Kept()->name, u"Rex");
}

TEST_F(CreatureCarryOverSystem, NothingIsLoadedWhenNoCreatureWasKept)
{
	EXPECT_FALSE(_carryOver.LoadPlayersCreature({100.0f, 100.0f}).has_value());
}
