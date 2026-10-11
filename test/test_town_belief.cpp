/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Magic/TownBelief.h"

using namespace openblack;
using namespace openblack::magic::town_belief;

TEST(TownBelief, PendingBeliefIsBelievedAtTheTownsTurnByItsScale)
{
	Belief town;
	town.scale = 2.0f;
	Add(town, PlayerNames::PLAYER_ONE, 0.25f);
	Add(town, PlayerNames::PLAYER_ONE, 0.25f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.0f);
	const auto gained = Turn(town, 0.997f);
	ASSERT_EQ(gained.size(), 1u);
	EXPECT_EQ(gained[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(gained[0].amount, 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.0f);
	EXPECT_FLOAT_EQ(town.pending.at(0), 0.0f);
	EXPECT_FLOAT_EQ(town.recent.at(0), 0.5f * 0.997f);
	EXPECT_TRUE(Turn(town, 0.997f).empty());
}

TEST(TownBelief, CappedAboveOnly)
{
	Belief town;
	Add(town, PlayerNames::PLAYER_TWO, 15.0f);
	(void)Turn(town, 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(1), k_DefaultCap);
	Add(town, PlayerNames::PLAYER_TWO, -20.0f);
	(void)Turn(town, 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(1), -10.0f);
}

TEST(TownBelief, NeutralTownIsTakenOnceAPlayerBelievesAsMuchAsItDoesInTheNeutralPlayer)
{
	Belief town;
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.3f);
	EXPECT_FALSE(Ownership(town, PlayerNames::NEUTRAL, 2.0f).has_value());
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.3f);
	Add(town, PlayerNames::PLAYER_ONE, 0.29f);
	(void)Turn(town, 1.0f);
	EXPECT_FALSE(Ownership(town, PlayerNames::NEUTRAL, 2.0f).has_value());
	Add(town, PlayerNames::PLAYER_ONE, 0.02f);
	(void)Turn(town, 1.0f);
	const auto taker = Ownership(town, PlayerNames::NEUTRAL, 2.0f);
	ASSERT_TRUE(taker.has_value());
	EXPECT_EQ(*taker, PlayerNames::PLAYER_ONE);
	// The new owner's belief is multiplied by the claimed-town multiplier
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.62f);
	EXPECT_FALSE(Ownership(town, PlayerNames::PLAYER_ONE, 2.0f).has_value());
}

TEST(TownBelief, TiesGoToTheLaterPlayerTheNeutralPlayerLast)
{
	Belief town;
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.5f);
	Set(town, PlayerNames::PLAYER_ONE, 0.5f);
	// Player one holds it, but the neutral player believed in as much comes later and takes it
	const auto taker = Ownership(town, PlayerNames::PLAYER_ONE, 1.0f);
	ASSERT_TRUE(taker.has_value());
	EXPECT_EQ(*taker, PlayerNames::NEUTRAL);
	Set(town, PlayerNames::PLAYER_TWO, 0.5f);
	Set(town, PlayerNames::PLAYER_THREE, 0.5f);
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.0f);
	EXPECT_EQ(Ownership(town, PlayerNames::PLAYER_ONE, 1.0f), PlayerNames::PLAYER_THREE);
}

TEST(TownBelief, NeutralBeliefIsSetBackEveryTurn)
{
	Belief town;
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.5f);
	Set(town, PlayerNames::NEUTRAL, 3.0f);
	(void)Ownership(town, PlayerNames::NEUTRAL, 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.5f);
}

TEST(TownBelief, ClaimedBeliefIsCapped)
{
	Belief town;
	Set(town, PlayerNames::PLAYER_TWO, 8.0f);
	EXPECT_EQ(Ownership(town, PlayerNames::NEUTRAL, 2.0f), PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(town.belief.at(1), k_DefaultCap);
}

TEST(TownBelief, LosingATownLowersBeliefByTheMultiplierAndTheLandsScale)
{
	Belief town;
	Set(town, PlayerNames::PLAYER_ONE, 4.0f);
	LostTown(town, PlayerNames::PLAYER_ONE, 0.5f, 0.75f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.5f);
}

TEST(TownBelief, ScriptSetsBeliefAsAShareOfBeliefInTheOwner)
{
	Belief town;
	Set(town, PlayerNames::PLAYER_TWO, 2.0f);
	Set(town, PlayerNames::PLAYER_ONE, 1.0f);
	SetRelativeToOwner(town, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 0.25f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.5f);
	SetRelativeToOwner(town, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.0f);
}

TEST(TownBelief, TakingATownHalvesTheMostBelievedAndGivesTheTakerMore)
{
	constexpr std::array k_Players {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::NEUTRAL};
	Belief town;
	Set(town, PlayerNames::NEUTRAL, 0.5f);
	Set(town, PlayerNames::PLAYER_TWO, 2.0f);
	TakeTown(town, PlayerNames::PLAYER_ONE, 1.0f, k_Players);
	EXPECT_FLOAT_EQ(town.belief.at(1), 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 3.0f);
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.5f);
	TakeTown(town, PlayerNames::NEUTRAL, 1.0f, k_Players);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(1), 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.0f);
}
