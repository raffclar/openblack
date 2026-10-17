/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureSaveState.h"
#include "Creature/CreatureScriptAgendas.h"

using namespace openblack;
using namespace openblack::creature_save_state;
using creature_spells::Phase;
using creature_spells::Spell;

TEST(CreatureSaveState, AReferenceIsTheEntitysKeyAndNoneIsNoObject)
{
	EXPECT_TRUE(ToRef(entt::null).Empty());
	EXPECT_TRUE(ToRef(std::optional<entt::entity> {}).Empty());
	EXPECT_EQ(ToRef(static_cast<entt::entity>(0)).key, gamesave::EntityKey(0));
	EXPECT_EQ(FromRef(ToRef(static_cast<entt::entity>(41))), static_cast<entt::entity>(41));
	EXPECT_FALSE(FromRef({}).has_value());
	// A number of a file of the game is no entity
	EXPECT_FALSE(FromRef(gamesave::RefTo(gamesave::NumberKey(7))).has_value());
}

TEST(CreatureSaveState, OnlyAScriptAFightOrADanceKeepsThePlan)
{
	EXPECT_FALSE(KeepsPlan(false, false, 0));
	EXPECT_TRUE(KeepsPlan(true, false, 0));
	EXPECT_TRUE(KeepsPlan(false, true, 0));
	EXPECT_TRUE(KeepsPlan(false, false, 2));
}

TEST(CreatureSaveState, ASpellSlotKeepsItsPhaseTurnsValueAndMiracle)
{
	const creature_spells::Slot slot {
	    .phase = Phase::Holding, .turnsLeft = 12, .holdTurns = -1, .miracle = static_cast<entt::entity>(5), .before = 1.25f};
	const auto saved = ToSlot(slot, 7);
	EXPECT_EQ(saved.spell, 7u);
	EXPECT_EQ(saved.phase, 3u);
	EXPECT_EQ(saved.turnsLeft, 12);
	EXPECT_EQ(saved.holdTurns, -1);
	EXPECT_EQ(saved.found, 1.25f);
	EXPECT_EQ(saved.miracle.key, gamesave::EntityKey(5));
	const auto back = FromSlot(saved);
	EXPECT_EQ(back.phase, Phase::Holding);
	EXPECT_EQ(back.miracle, slot.miracle);
	EXPECT_EQ(back.before, 1.25f);
	EXPECT_EQ(FromSlot({.phase = 9}).phase, Phase::Off);
}

TEST(CreatureSaveState, AQueuedSpellKeepsItsSecondsAndGetsItsTurnsBack)
{
	const creature_spells::Waiting waiting {.spell = Spell::Big, .holdTurns = 305, .miracle = entt::null, .seconds = 30.5f};
	const auto saved = ToQueued(waiting);
	EXPECT_EQ(saved.spell, 2u);
	EXPECT_EQ(saved.seconds, 30.5f);
	EXPECT_TRUE(saved.miracle.Empty());
	const auto back = FromQueued(saved, 10.0f);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->holdTurns, 305);
	EXPECT_EQ(back->spell, Spell::Big);
	// No time at all holds for all time
	EXPECT_EQ(FromQueued({.spell = 0, .seconds = -1.0f}, 10.0f)->holdTurns, -1);
	EXPECT_FALSE(FromQueued({.spell = 16}, 10.0f).has_value());
}

TEST(CreatureSaveState, TheFizzIsSavedOnlyWhileTheCreatureIsInvisible)
{
	const creature_fizz::Fizz fizz {.now = 0.4f, .target = 1.0f, .perSecond = 0.5f, .goesForGood = true};
	creature_spells::Spells spells;
	const auto none = FizzToSave(fizz, spells);
	EXPECT_EQ(none.now, 0.0f);
	EXPECT_EQ(none.target, 0.0f);
	EXPECT_EQ(none.perSecond, 0.0f);
	EXPECT_TRUE(none.goesForGood);
	spells[Spell::Invisible].phase = Phase::Holding;
	EXPECT_EQ(FizzToSave(fizz, spells), fizz);
}

TEST(CreatureSaveState, TheKnownLeashesAndTheLeashTypesKeepTheGamesNumbers)
{
	std::bitset<creature_leash::k_Types.size()> known;
	known.set(*creature_leash::IndexOf(LeashType::Rope));
	const auto saved = ToKnown(known);
	EXPECT_FALSE(saved.evil);
	EXPECT_TRUE(saved.rope);
	EXPECT_FALSE(saved.good);
	EXPECT_EQ(FromKnown(saved), known);
	EXPECT_EQ(LeashTypeNumber(LeashType::Evil), 1u);
	EXPECT_EQ(LeashTypeNumber(LeashType::Good), 3u);
	EXPECT_EQ(LeashTypeFromNumber(2), LeashType::Rope);
	EXPECT_FALSE(LeashTypeFromNumber(0).has_value());
	EXPECT_EQ(PlayerNumber(std::nullopt), 0xFFu);
	EXPECT_EQ(PlayerFromNumber(1), PlayerNames::PLAYER_TWO);
	// A creature that belongs to no one has no player in the game's files
	EXPECT_EQ(PlayerNumber(PlayerNames::NEUTRAL), 0xFFu);
	EXPECT_EQ(PlayerFromNumber(0xFF), PlayerNames::NEUTRAL);
}

TEST(CreatureSaveState, ALoadStartsTheCurrentStepAgain)
{
	const auto agenda = AgendaAfterLoad({.word08 = 0, .current = 2, .state = 3, .count = 4, .word20 = 0});
	EXPECT_EQ(agenda.word08, 1u);
	EXPECT_EQ(agenda.current, 2u);
	EXPECT_EQ(agenda.state, 0u);
	EXPECT_EQ(agenda.word20, 1u);
}

TEST(CreatureSaveState, OnlyAMindFileThatOpensAndIsNotEmptyPutsTheCreatureBackOnTheMap)
{
	EXPECT_FALSE(ResolvesOnLoad(std::nullopt));
	EXPECT_FALSE(ResolvesOnLoad(0));
	EXPECT_TRUE(ResolvesOnLoad(1));
	EXPECT_TRUE(ResolvesOnLoad(4096));
}

TEST(CreatureSaveState, ASaveKeepsTheValuesOfTheGamesFileThatOpenblackDidNotChange)
{
	gamesave::CreatureObject loaded;
	loaded.creature.strength = 0.6892248f;
	loaded.creature.size = 0.24f;
	loaded.living.birthTurn = 0xFFFF8AD8;
	loaded.move.speed = 0x05C3;
	loaded.position.position = {.x = 0x00DF71B0, .z = 0x013ACEB2, .altitude = 0.0f};
	loaded.creature.plan = gamesave::KeptPlan {.desire = 40};
	// openblack keeps the position in metres, so the position that it writes after a load is a little apart
	auto baseline = gamesave::CreatureObject {};
	baseline.creature.strength = 0.6892248f;
	baseline.creature.size = 0.24f;
	baseline.position.position = {.x = 0x00DF71AF, .z = 0x013ACEB2, .altitude = 0.0001f};
	auto now = baseline;
	EXPECT_EQ(MergeOnto(loaded, baseline, now), loaded);

	// The values that openblack changed go in, and only those
	now.creature.strength = 0.7f;
	now.position.position.x += 0x10000;
	const auto merged = MergeOnto(loaded, baseline, now);
	EXPECT_EQ(merged.creature.strength, 0.7f);
	EXPECT_EQ(merged.position.position, now.position.position);
	EXPECT_EQ(merged.living.birthTurn, 0xFFFF8AD8u);
	EXPECT_EQ(merged.move.speed, 0x05C3u);
	EXPECT_EQ(merged.creature.size, 0.24f);
	EXPECT_EQ(merged.creature.plan, loaded.creature.plan);
}

TEST(CreatureSaveState, TheLeashOfTheGamesFileKeepsItsHolder)
{
	gamesave::LeashStatusObject loaded {
	    .thing = {}, .leash = {.on = 0, .type = 2, .length = 24.31f, .holder = gamesave::RefTo(gamesave::NumberKey(1841))}};
	gamesave::LeashStatusObject baseline {.thing = {}, .leash = {.on = 0, .type = 2}};
	auto now = baseline;
	EXPECT_EQ(MergeLeashOnto(loaded, baseline, now), loaded);
	now.leash.on = 1;
	const auto merged = MergeLeashOnto(loaded, baseline, now);
	EXPECT_EQ(merged.leash.on, 1u);
	EXPECT_EQ(merged.leash.holder.key, gamesave::NumberKey(1841));
	EXPECT_EQ(merged.leash.length, 24.31f);
}

TEST(CreatureSaveState, RekeyChangesTheKeysOfTheTableOnly)
{
	gamesave::CreatureObject creature;
	creature.creature.targetObject = gamesave::RefTo(gamesave::NumberKey(1959));
	creature.creature.arena = gamesave::RefTo(gamesave::NumberKey(300));
	creature.creature.plan = gamesave::KeptPlan {};
	creature.creature.plan->agenda.creature = gamesave::RefTo(gamesave::NumberKey(1959));
	Rekey(creature, [](gamesave::ObjectKey key) { return key == gamesave::NumberKey(1959) ? gamesave::EntityKey(4) : key; });
	EXPECT_EQ(creature.creature.targetObject.key, gamesave::EntityKey(4));
	EXPECT_EQ(creature.creature.plan->agenda.creature.key, gamesave::EntityKey(4));
	EXPECT_EQ(creature.creature.arena.key, gamesave::NumberKey(300));
	EXPECT_TRUE(creature.creature.dance.Empty());
}

TEST(CreatureSaveState, TheAgendaOfAScriptWalkKeepsTheGamesSteps)
{
	creature_mind::IdleMind idle;
	idle.agenda = creature_mind::MoveForScript({0.0f, 0.0f}, {2229.5f, 21.1f, 3146.5f}, 10.0f, 30.0f);
	idle.step = 1;
	idle.stepStarted = true;
	const auto self = ToRef(static_cast<entt::entity>(3));
	const auto agenda = AgendaToSave(idle, self);
	EXPECT_EQ(agenda.current, 1u);
	EXPECT_EQ(agenda.count, 2u);
	ASSERT_EQ(agenda.steps.size(), 1u);
	EXPECT_EQ(agenda.steps[0].action, creature_mind::k_SavedMoveToPoint);
	EXPECT_EQ(agenda.steps[0].real, 30.0f);
	EXPECT_EQ(agenda.steps[0].point[2], 3146.5f);
	EXPECT_EQ(agenda.creature, self);
	EXPECT_EQ(agenda.word1C, 0xFFFFFFFFu);

	// A step that does not keep the game's step leaves the steps out
	idle.agenda.push_back({});
	EXPECT_TRUE(AgendaToSave(idle, self).steps.empty());
}

TEST(CreatureSaveState, TheKindOfACreatureCountsDownFromTheGiantApe)
{
	EXPECT_EQ(CreatureInfo(CreatureType::GiantApe), 2517u);
	EXPECT_EQ(CreatureInfo(CreatureType::Cow), 2516u);
	EXPECT_EQ(CreatureInfo(CreatureType::Tiger), 2515u);
	EXPECT_EQ(CreatureInfo(CreatureType::BrownBear), 2508u);
	EXPECT_FALSE(CreatureInfo(CreatureType::Unknown).has_value());
}

TEST(CreatureSaveState, ALeashOrderAimsTwoMetresOverThePlaceAndTheThing)
{
	const auto onLand = TargetPoint({2199.3967f, 20.77f, 2308.9426f}, 0.0f);
	EXPECT_FLOAT_EQ(onLand.y, 22.77f);
	// x and z are as precise as a map position: a whole number of 65536ths of a cell
	EXPECT_EQ(onLand.x, map_coords::ToMetres(0x00DBF08E));
	EXPECT_EQ(onLand.z, map_coords::ToMetres(0x00E6E4EE));
	EXPECT_FLOAT_EQ(TargetPoint({100.0f, 5.0f, 100.0f}, 3.5f).y, 10.5f);
}
