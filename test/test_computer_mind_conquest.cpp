/******************************************************************************
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

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindConquestRules.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindConquest, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionTakeOverTown), MindDomain::Conquest);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastSpellToImpress), MindDomain::Conquest);
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetCreaturetoHelp), MindDomain::Conquest);
}

TEST(ComputerMindConquest, TownHoldFromRivalBelief)
{
	// No belief in a rival: the god holds the town fully
	EXPECT_FLOAT_EQ(TownHold(0.0f, false), 1.0f);
	// Belief 2 in the owner: 1 - 0.4
	EXPECT_FLOAT_EQ(TownHold(2.0f, false), 1.0f - 2.0f * 0.2f);
	// Belief of 5 or more takes the hold to its least
	EXPECT_FLOAT_EQ(TownHold(5.0f, false), k_LeastHold);
	EXPECT_FLOAT_EQ(TownHold(20.0f, false), k_LeastHold);
	// A town of the god's own holds 0.75 more, not more than 1
	EXPECT_FLOAT_EQ(TownHold(4.0f, true), (1.0f - 4.0f * 0.2f) + 0.75f);
	EXPECT_FLOAT_EQ(TownHold(1.0f, true), 1.0f);
}

TEST(ComputerMindConquest, RivalBeliefOfTheOwnerOrTheStrongestRival)
{
	const std::array<float, k_BeliefPlayers> belief {0.5f, 0.2f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f};
	// In a town of another player, the belief in that player
	EXPECT_FLOAT_EQ(RivalBelief(belief, 0, 1), 0.2f);
	EXPECT_FLOAT_EQ(RivalBelief(belief, 0, 7), 0.3f);
	// In a town of the god's own, the belief in the player the most above the god
	EXPECT_FLOAT_EQ(RivalBelief(belief, 0, 0), 0.9f);
	// The first of equal differences stays
	const std::array<float, k_BeliefPlayers> equal {0.0f, 0.4f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	EXPECT_FLOAT_EQ(RivalBelief(equal, 0, 0), 0.4f);
}

TEST(ComputerMindConquest, AGodAfterTheSeventhFindsItsRivalsInTheBeliefOfTheGame)
{
	// A game of 10 player numbers: the god is player 9 and the town believes most in player 8
	const std::array<float, 10> belief {0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.6f, 0.3f};
	EXPECT_FLOAT_EQ(RivalBelief(belief, 9, 9), 0.6f);
	EXPECT_FLOAT_EQ(RivalBelief(belief, 9, 8), 0.6f);
	// A player that the belief has no place for gets none
	EXPECT_FLOAT_EQ(RivalBelief(belief, 0, 12), 0.0f);
}

TEST(ComputerMindConquest, TownWorthParts)
{
	const std::array<TownMiracleFacts, 3> miracles {
	    {{.enabled = true, .requested = true}, {.enabled = false, .requested = false}, {.enabled = false, .requested = true}}};
	EXPECT_FLOAT_EQ(TownMiracleWish(miracles), 1.5f);
	EXPECT_FLOAT_EQ(TownPeopleShare(25, 100.0f, 1.5f), 0.25f);
	// With no people of its own the god takes the miracle wish plus 1
	EXPECT_FLOAT_EQ(TownPeopleShare(25, 0.0f, 1.5f), 2.5f);
	EXPECT_FLOAT_EQ(PeopleCount(100), 100.0f);
	EXPECT_FLOAT_EQ(RivalPowerCut(true, 10.0f, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(RivalPowerCut(false, 10.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(RivalPowerCut(false, 10.0f, 1.0f), 0.85f);
	EXPECT_FLOAT_EQ(RivalPowerCut(false, 1.0f, 2.0f), 0.425f);
	// At the hand a town is fully near; far away it is little more than half near
	EXPECT_FLOAT_EQ(TownNearness(0.0f), 1.0f);
	EXPECT_GT(TownNearness(10000.0f), 0.5f);
	EXPECT_LT(TownNearness(10000.0f), 0.6f);
	EXPECT_FLOAT_EQ(TownWorth(0.25f, 1.5f, 0.5f, 0.5f, 1.0f), ((0.5f * 1.75f) * 0.5f) * 1.0f);
}

TEST(ComputerMindConquest, MostWantedTownIsTheFirstLargestAboveZero)
{
	EXPECT_FALSE(MostWantedTown(std::array<float, 2> {0.0f, -1.0f}).has_value());
	EXPECT_EQ(MostWantedTown(std::array<float, 4> {0.2f, 0.7f, 0.7f, 0.1f}), 1u);
	EXPECT_FLOAT_EQ(CappedWorth(3.0f), 1.0f);
	EXPECT_FLOAT_EQ(CappedWorth(0.3f), 0.3f);
}

TEST(ComputerMindConquest, NeedWishFeedsOtherTownsFully)
{
	EXPECT_FLOAT_EQ(RivalBeliefRatio(0.3f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(RivalBeliefRatio(0.3f, 0.6f), 0.5f);
	EXPECT_FLOAT_EQ(FeedFactor(false, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(FeedFactor(true, 0.05f), 0.5f);
	EXPECT_FLOAT_EQ(FeedFactor(true, 0.5f), 1.0f);
	EXPECT_FLOAT_EQ(NeedWish(0.4f, 0.5f), 0.2f);
	EXPECT_FLOAT_EQ(NeedWish(4.0f, 1.0f), 1.0f);
}

TEST(ComputerMindConquest, CreatureHelpWish)
{
	EXPECT_FLOAT_EQ(CreatureReach(1.0f), 1180.0f);
	// A free, rested creature at the place: wanted as much as the nearness allows
	EXPECT_FLOAT_EQ(CreatureHelpWish(1.0f, 0.0f, 0.0f, 1.0f), gutils::GetDistanceModifier(0.0f, 1180.0f));
	EXPECT_FLOAT_EQ(CreatureHelpWish(0.0f, 0.0f, 0.0f, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(CreatureHelpWish(1.0f, 1.0f, 0.0f, 1.0f), 0.0f);
	EXPECT_LT(CreatureHelpWish(1.0f, 0.0f, 600.0f, 1.0f), 1.0f);
}

TEST(ComputerMindConquest, KnownMiracleShareDividesWholeNumbers)
{
	EXPECT_FLOAT_EQ(KnownMiracleShare(0, 0), 1.0f);
	EXPECT_FLOAT_EQ(KnownMiracleShare(3, 0), 1.0f);
	// Two of three unknown still gives 1, as the game divides whole numbers
	EXPECT_FLOAT_EQ(KnownMiracleShare(3, 2), 1.0f);
	EXPECT_FLOAT_EQ(KnownMiracleShare(3, 3), 0.0f);
}

TEST(ComputerMindConquest, NeedsMiracleChoice)
{
	EXPECT_EQ(ChooseNeedsMiracle(0.5f, 0.2f), NeedsMiracle::Food);
	EXPECT_EQ(ChooseNeedsMiracle(0.2f, 0.2f), NeedsMiracle::Wood);
	EXPECT_EQ(ChooseNeedsMiracle(0.0f, 0.0f), NeedsMiracle::None);
}

TEST(ComputerMindConquest, ThrowRules)
{
	EXPECT_FLOAT_EQ(ThrowWish(0.5f, 1.0f, 0.8f), 0.4f);
	EXPECT_FLOAT_EQ(ThrowScore(0.3f, 1.0f), 1.3f);
	EXPECT_TRUE(CanBeThrownByPlayer(ThrowKind::Always, 100.0f, 0));
	EXPECT_FALSE(CanBeThrownByPlayer(ThrowKind::None, 0.0f, 10));
	EXPECT_FALSE(CanBeThrownByPlayer(ThrowKind::Pile, 0.0f, 0));
	EXPECT_TRUE(CanBeThrownByPlayer(ThrowKind::Pile, 0.0f, 1));
	EXPECT_TRUE(CanBeThrownByPlayer(ThrowKind::RockLike, 3.6f, 0));
	EXPECT_FALSE(CanBeThrownByPlayer(ThrowKind::RockLike, 3.7f, 0));
}

TEST(ComputerMindConquest, SearchCellsAroundAPlace)
{
	// 100 m is cell 10 (a cell is 10 m); 25 m either side reaches cells 7 and 12
	const auto span = SearchCells(map_coords::ToFixed(100.0f), 25.0f);
	EXPECT_EQ(span.first, 7);
	EXPECT_EQ(span.last, 12);
	// Below the corner of the map the cells are negative
	EXPECT_LT(SearchCells(0, 25.0f).first, 0);
}

TEST(ComputerMindConquest, SearchKeepsOnlyBetterThings)
{
	EXPECT_FALSE(SearchKeeps(0.5f, 0.0f, 250.0f, 1.0f).has_value());
	EXPECT_FALSE(SearchKeeps(0.0f, 0.0f, 250.0f, 0.0f).has_value());
	const auto kept = SearchKeeps(2.0f, 0.0f, 250.0f, 0.5f);
	ASSERT_TRUE(kept.has_value());
	EXPECT_FLOAT_EQ(*kept, 2.0f * gutils::GetDistanceModifier(0.0f, 250.0f));
}
