/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>

#include <ECS/AlignmentHistory.h>
#include <ECS/Components/AlignmentHistory.h>
#include <ECS/TownDesireAlignment.h>
#include <InfoConstants.h>
#include <Magic/AreaEffect.h>
#include <gtest/gtest.h>

namespace
{
namespace rules = openblack::ecs::alignment_history;
using openblack::ecs::components::AlignmentCause;
using openblack::ecs::components::AlignmentHistory;

constexpr auto k_Script = static_cast<std::size_t>(AlignmentCause::Script);
constexpr auto k_Tree = static_cast<std::size_t>(AlignmentCause::Tree);
} // namespace

TEST(AlignmentHistory, ScriptChangeCountsAsItIs)
{
	AlignmentHistory history;
	rules::Add(history, AlignmentCause::Script, 0.25f, 3.0f);
	rules::Add(history, AlignmentCause::Script, -0.5f, 3.0f);
	EXPECT_FLOAT_EQ(history.totals.at(k_Script), -0.25f);
	EXPECT_EQ(history.counts.at(k_Script), 2);
}

TEST(AlignmentHistory, OtherCausesUseThePlayerScale)
{
	AlignmentHistory history;
	rules::Add(history, AlignmentCause::Tree, 0.25f, 2.0f);
	EXPECT_FLOAT_EQ(history.totals.at(k_Tree), 0.5f);
	EXPECT_EQ(history.counts.at(k_Tree), 1);
	EXPECT_EQ(history.counts.at(k_Script), 0);
}

TEST(AlignmentHistory, ZeroChangeDoesNotCount)
{
	AlignmentHistory history;
	rules::Add(history, AlignmentCause::Script, 0.0f, 1.0f);
	rules::Add(history, AlignmentCause::Tree, 0.0f, 1.0f);
	EXPECT_EQ(history.counts.at(k_Script), 0);
	EXPECT_EQ(history.counts.at(k_Tree), 0);
}

TEST(AlignmentHistory, LabelsMatchTheCauses)
{
	EXPECT_EQ(rules::k_Labels.at(k_Script), "Script  ");
	EXPECT_EQ(rules::k_Labels.at(k_Tree), "Tree    ");
}

TEST(AlignmentHistory, EachKindOfEffectCountsBeforeTheDampingInTheHistoryOrder)
{
	using openblack::AlignmentType;
	using openblack::GAlignmentInfo;
	using openblack::magic::EffectKind;
	std::array<GAlignmentInfo, 7> table {};
	table.at(0).villager = -1.0f;
	table.at(1).villager = -0.5f;
	table.at(3).villager = 1.0f;
	openblack::magic::EffectValues values;
	values[EffectKind::Crush] = 2.0f;
	values[EffectKind::Heal] = 4.0f;
	// The life went down by 0.25 and the addition is 0.75, so each kind counts once its amount
	const auto terms = openblack::magic::EffectAlignmentChanges(values, 3.0f, AlignmentType::Villager, -0.25f, table, 0.75f);
	EXPECT_FLOAT_EQ(terms.at(0), -1.0f);
	EXPECT_FLOAT_EQ(terms.at(1), 0.0f);
	EXPECT_FLOAT_EQ(terms.at(2), 4.0f);
	EXPECT_FLOAT_EQ(terms.at(3), 0.0f);
	// The heat comes last, by the harm that it does
	EXPECT_FLOAT_EQ(terms.at(4), -3.0f);
}

TEST(AlignmentHistory, AnEffectThatDoesNotChangeTheLifeCountsNothing)
{
	std::array<openblack::GAlignmentInfo, 7> table {};
	table.at(1).villager = -0.5f;
	openblack::magic::EffectValues values;
	values[openblack::magic::EffectKind::Crush] = 2.0f;
	const auto terms =
	    openblack::magic::EffectAlignmentChanges(values, 0.0f, openblack::AlignmentType::Villager, 0.0f, table, 0.75f);
	for (const float term : terms)
	{
		EXPECT_EQ(term, 0.0f);
	}
}

TEST(AlignmentHistory, TheTownCountsTheChangeOfEachWantBeforeTheSumIsHeld)
{
	namespace town = openblack::ecs::town_desire_alignment;
	constexpr auto k_Count = openblack::ecs::components::k_TownDesireCount;
	std::array<std::uint32_t, k_Count> turns {};
	std::array<float, k_Count> boostA {};
	std::array<float, k_Count> desire {};
	std::array<float, k_Count> boost {};
	std::array<town::DesireRule, k_Count> rules {};
	desire.at(0) = 3.0f;
	desire.at(1) = 2.0f;
	rules.at(0) = {.affectsAfter = 0.0f, .importance = 1.0f, .maxTurns = 1};
	rules.at(1) = {.affectsAfter = 0.0f, .importance = 0.5f, .maxTurns = 1};
	std::array<float, k_Count> terms {};
	const float change =
	    town::Change(turns, {.boostA = boostA, .desire = desire, .boost = boost}, rules, {.town = 1, .all = 1}, 0.5f, terms);
	EXPECT_FLOAT_EQ(terms.at(0), 3.0f);
	EXPECT_FLOAT_EQ(terms.at(1), 1.0f);
	EXPECT_FLOAT_EQ(terms.at(2), 0.0f);
	// The sum of 4 is held to 1, then multiplied by the most change a turn
	EXPECT_FLOAT_EQ(change, 0.5f);
}
