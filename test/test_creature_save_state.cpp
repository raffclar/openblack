/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureSaveState.h"

using namespace openblack;
using namespace openblack::creature_save_state;
using creature_spells::Phase;
using creature_spells::Spell;

TEST(CreatureSaveState, AReferenceIsTheEntityPlusOneAndNoneIsZero)
{
	EXPECT_EQ(ToRef(entt::null), 0u);
	EXPECT_EQ(ToRef(std::optional<entt::entity> {}), 0u);
	EXPECT_EQ(ToRef(static_cast<entt::entity>(0)), 1u);
	EXPECT_EQ(FromRef(ToRef(static_cast<entt::entity>(41))), static_cast<entt::entity>(41));
	EXPECT_FALSE(FromRef(0).has_value());
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
	const auto saved = ToSlot(slot);
	EXPECT_EQ(saved.start, 16u);
	EXPECT_EQ(saved.phase, 3u);
	EXPECT_EQ(saved.turnsLeft, 12);
	EXPECT_EQ(saved.holdTurns, -1);
	EXPECT_EQ(saved.found, 1.25f);
	EXPECT_EQ(saved.miracle, 6u);
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
	EXPECT_EQ(saved.miracle, 0u);
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
	EXPECT_FALSE(PlayerFromNumber(0xFF).has_value());
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
