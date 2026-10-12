/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <set>

#include <gtest/gtest.h>

#include "Help/HelpMessages.h"

using namespace openblack::help_messages;

namespace
{
/// Draws in turn from a list, as the local random numbers would
struct ListedRand
{
	std::vector<uint32_t> draws;
	size_t next {0};
	uint32_t operator()(int32_t n)
	{
		const auto draw = draws.at(next++ % draws.size());
		return n > 0 ? draw % static_cast<uint32_t>(n) : 0;
	}
};
} // namespace

TEST(HelpMessages, TheSetsAreTheGames)
{
	EXPECT_EQ(Set(1).first, 3187u);
	EXPECT_EQ(Set(25).last, 3303u);
	EXPECT_EQ(Set(1).script, Script::Banter);
	EXPECT_EQ(Set(k_NotInterested).pick, Pick::Random);
	EXPECT_EQ(Set(k_NotInterested).first, 2986u);
	EXPECT_EQ(Set(k_OthersWonder).script, Script::JustTalkMaybeWithSpirits);
	EXPECT_EQ(ScriptName(Script::TownNeeds), "HelpTownNeeds");
	EXPECT_EQ(ScriptName(Script::Banter), "MultiBanter");
}

TEST(HelpMessages, ARandomSetSaysOneOfItsTexts)
{
	ListedRand rand {{3}};
	const auto texts = TextsOf(k_NotInterested, std::nullopt, std::ref(rand));
	ASSERT_TRUE(texts.has_value());
	EXPECT_EQ(texts->first, 2989u);
	EXPECT_EQ(texts->last, 2989u);
	const auto fixed = TextsOf(5, std::nullopt, std::ref(rand));
	ASSERT_TRUE(fixed.has_value());
	EXPECT_EQ(fixed->first, 3203u);
	EXPECT_EQ(fixed->last, 3204u);
	const auto thing = TextsOf(50, Texts {10, 12}, std::ref(rand));
	ASSERT_TRUE(thing.has_value());
	EXPECT_EQ(thing->first, 10u);
}

TEST(HelpMessages, BanterGoesThroughEverySetOnceThenStartsAgain)
{
	SetCounts sent {};
	uint16_t used = 0;
	ListedRand rand {{7, 0, 13, 2, 5}};
	std::set<size_t> said;
	for (size_t i = 0; i < k_BanterCount; ++i)
	{
		const auto set = PickBanter(sent, used, std::ref(rand));
		ASSERT_GE(set, k_FirstBanter);
		ASSERT_LT(set, k_FirstBanter + k_BanterCount);
		// Said, the set counts
		++sent.at(set);
		said.insert(set);
	}
	EXPECT_EQ(said.size(), k_BanterCount);
	EXPECT_EQ(used, k_BanterCount);
	// All used: the counts clear and the first is said
	EXPECT_EQ(PickBanter(sent, used, std::ref(rand)), k_FirstBanter);
	EXPECT_EQ(used, 0u);
	EXPECT_EQ(sent.at(k_FirstBanter + 3), 0u);
}

TEST(HelpMessages, BanterSkipsSetsAlreadySaid)
{
	SetCounts sent {};
	sent.at(1) = 1;
	sent.at(2) = 1;
	uint16_t used = 2;
	ListedRand rand {{0}};
	EXPECT_EQ(PickBanter(sent, used, std::ref(rand)), 3u);
	EXPECT_EQ(used, 3u);
}

TEST(HelpMessages, BanterAfterTwoQuietMinutes)
{
	EXPECT_FALSE(BanterDue(120.0f, false));
	EXPECT_TRUE(BanterDue(120.5f, false));
	EXPECT_FALSE(BanterDue(500.0f, true));
	// Never helped at all reads 0 seconds
	EXPECT_FALSE(BanterDue(0.0f, false));
}

TEST(HelpMessages, ATownsNeedIsAPercentageOfItsRawDesire)
{
	EXPECT_FLOAT_EQ(TownNeed(0.25f), 25.0f);
	EXPECT_FLOAT_EQ(TownNeed(1.7f), 100.0f);
	EXPECT_FLOAT_EQ(TownNeed(0.0f), 0.0f);
}
