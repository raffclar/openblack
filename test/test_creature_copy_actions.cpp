/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureDeeds.h"
#include "Creature/CreatureWatching.h"

using namespace openblack;
using namespace openblack::creature_watching;

namespace
{
uint32_t NoExtra(uint32_t /*range*/)
{
	return 0;
}

std::vector<MimicRule> Rules()
{
	return {
	    {.name = "Food", .chance = 0.4f, .needsLearningLeash = false, .copyActions = {245, 300}, .stageSteps = 2},
	    {.name = "Rocks", .chance = 0.4f, .needsLearningLeash = false, .copyActions = {266}, .stageSteps = 3},
	};
}
} // namespace

TEST(CreatureCopyActions, MayStartIsTheTestOfStart)
{
	const auto rules = Rules();
	const MimicConditions grown {.phase = 13};
	std::optional<Mimicry> mimicry;
	EXPECT_TRUE(MayStartMimicry(mimicry, 1, rules, grown));
	EXPECT_FALSE(mimicry.has_value());
	EXPECT_FALSE(MayStartMimicry(mimicry, 1, rules, {.phase = 2}));
	EXPECT_FALSE(MayStartMimicry(mimicry, 5, rules, grown));
	ASSERT_TRUE(StartMimicry(mimicry, 1, rules, grown, 7u, 10, NoExtra));
	// The same deed again does not start again
	EXPECT_FALSE(MayStartMimicry(mimicry, 1, rules, grown));
	// A deed of the same rank starts over it
	EXPECT_TRUE(MayStartMimicry(mimicry, 0, rules, grown));
}

TEST(CreatureCopyActions, BeginStartsWithNoTest)
{
	const auto rules = Rules();
	std::optional<Mimicry> mimicry;
	BeginMimicry(mimicry, 1, rules, 9u, 40, NoExtra);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::Notice);
	EXPECT_EQ(mimicry->limit, 3u);
	EXPECT_EQ(mimicry->stageStart, 40u);
	EXPECT_EQ(mimicry->object, 9u);
	BeginMimicry(mimicry, 4, rules, std::nullopt, 50, NoExtra);
	EXPECT_EQ(mimicry->rule, 1u);
}

TEST(CreatureCopyActions, CastDeedGateCountsWhatIsStillNeeded)
{
	// Ten sightings needed at a multiplier of 1
	EXPECT_EQ(CastDeedGate(0, 10, 1.0f), CastDeedStep::CountAndStop);
	EXPECT_EQ(CastDeedGate(8, 10, 1.0f), CastDeedStep::CountAndStop);
	EXPECT_EQ(CastDeedGate(9, 10, 1.0f), CastDeedStep::CountThenRoll);
	EXPECT_EQ(CastDeedGate(10, 10, 1.0f), CastDeedStep::Copy);
	EXPECT_EQ(CastDeedGate(30, 10, 1.0f), CastDeedStep::Copy);
	// The still needed count is cut towards zero: 1.5 still needed is one
	EXPECT_EQ(CastDeedGate(5, 10, 0.65f), CastDeedStep::CountThenRoll);
	// 0.9 still needed is none
	EXPECT_EQ(CastDeedGate(6, 10, 0.69f), CastDeedStep::Copy);
	// 10 x 0.7 rounds to 7 as a float, so with 5 seen 2 are still needed
	EXPECT_EQ(CastDeedGate(5, 10, 0.7f), CastDeedStep::CountAndStop);
}

TEST(CreatureCopyActions, CopyActionsOfAMiracleDeedUseTheMiracle)
{
	const auto rules = Rules();
	const std::vector<MiracleRule> miracles {
	    {.name = "None"},
	    {.name = "Fireball", .action = 45},
	    {.name = "No action"},
	};
	EXPECT_EQ(CopyActionsOf(rules[1], 37, 0, miracles), std::vector<uint32_t> {266});
	// A deed of another kind keeps its list even with a miracle
	EXPECT_EQ(CopyActionsOf(rules[0], 33, 1, miracles), (std::vector<uint32_t> {245, 300}));
	EXPECT_EQ(CopyActionsOf(rules[0], k_DamageWithMagicDeed, 1, miracles), std::vector<uint32_t>(k_CopySlots, 45));
	EXPECT_TRUE(CopyActionsOf(rules[0], k_DamageWithMagicDeed, 2, miracles).empty());
	EXPECT_TRUE(CopyActionsOf(rules[0], k_DamageWithMagicDeed, 7, miracles).empty());
}

TEST(CreatureCopyActions, CopyingAgainStartsHalfWayIntoCopyingTheAction)
{
	const auto rules = Rules();
	const auto again = CopyObservedDeedAgain({.rule = 1, .object = 12u, .magic = 0}, rules, 500, NoExtra);
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ(again->stage, MimicStage::CopyAction);
	EXPECT_EQ(again->limit, 3u);
	EXPECT_EQ(again->done, 1u);
	EXPECT_EQ(again->stageStart, 500u);
	EXPECT_EQ(again->object, 12u);
	EXPECT_TRUE(StageContinues(*again, 500, 10.0f));
	const auto even = CopyObservedDeedAgain({.rule = 0, .magic = 4}, rules, 0, NoExtra);
	ASSERT_TRUE(even.has_value());
	EXPECT_EQ(even->done, 1u);
	EXPECT_EQ(even->magic, 4u);
	EXPECT_FALSE(even->object.has_value());
	// A deed with no row gives nothing to copy
	EXPECT_FALSE(CopyObservedDeedAgain({.rule = 46}, rules, 0, NoExtra).has_value());
}

TEST(CreatureCopyActions, TheHandsSacrificeIsCopiedOnlyFromTheLearningLeash)
{
	// The table's row for the hand's sacrifice at the altar: the creature copies it with its own Sacrifice action, for
	// the desire of anger, and only while it is on the learning leash
	constexpr uint32_t k_CreatureSacrifice = 282;
	constexpr uint32_t k_Anger = 2;
	auto rules = Rules();
	rules.resize(creature_deeds::k_Sacrifice + 1);
	rules[creature_deeds::k_Sacrifice] = {.name = "Sacrifice",
	                                      .chance = 0.9f,
	                                      .needsLearningLeash = true,
	                                      .noticeAction = 243,
	                                      .copyActions = {k_CreatureSacrifice},
	                                      .desire = k_Anger,
	                                      .stageSteps = 5};
	std::optional<Mimicry> mimicry;
	EXPECT_FALSE(MayStartMimicry(mimicry, creature_deeds::k_Sacrifice, rules, {.phase = 13}));
	EXPECT_FALSE(MayStartMimicry(mimicry, creature_deeds::k_Sacrifice, rules,
	                             {.phase = 13, .learningLeashInHand = true, .canSee = false}));
	const MimicConditions leashed {.phase = 13, .learningLeashInHand = true};
	// It starts over the copy of a deed that ranks lower
	ASSERT_TRUE(StartMimicry(mimicry, 1, rules, leashed, 7u, 10, NoExtra));
	ASSERT_TRUE(StartMimicry(mimicry, creature_deeds::k_Sacrifice, rules, leashed, 9u, 20, NoExtra));
	EXPECT_EQ(mimicry->rule, creature_deeds::k_Sacrifice);
	EXPECT_EQ(mimicry->stage, MimicStage::Notice);
	EXPECT_EQ(mimicry->limit, 5u);
	EXPECT_EQ(CopyActionsOf(rules[creature_deeds::k_Sacrifice], creature_deeds::k_Sacrifice, 0, {}),
	          std::vector<uint32_t> {k_CreatureSacrifice});
}
