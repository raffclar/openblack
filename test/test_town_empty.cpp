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

#include <gtest/gtest.h>

#include "ECS/TownEmpty.h"
#include "Magic/TownBelief.h"

using namespace openblack;
using namespace openblack::ecs;

TEST(TownEmpty, TheLastPersonToLeaveStartsTheCountdown)
{
	EXPECT_EQ(town_empty::AfterLeaving(0, 0), 50u);
	EXPECT_EQ(town_empty::AfterLeaving(7, 0), 50u);
	EXPECT_EQ(town_empty::AfterLeaving(0, 3), 0u);
	EXPECT_EQ(town_empty::AfterLeaving(7, 3), 7u);
}

TEST(TownEmpty, TheTownBecomesEmptyAtItsFiftiethTurn)
{
	uint32_t countdown = town_empty::AfterLeaving(0, 0);
	for (uint32_t turn = 1; turn < 50; ++turn)
	{
		EXPECT_FALSE(town_empty::Turn(countdown, 0)) << turn;
	}
	EXPECT_TRUE(town_empty::Turn(countdown, 0));
	EXPECT_EQ(countdown, 0u);
	EXPECT_FALSE(town_empty::Turn(countdown, 0));
}

TEST(TownEmpty, PeopleComingBackStopTheCountdown)
{
	uint32_t countdown = 50;
	EXPECT_FALSE(town_empty::Turn(countdown, 0));
	EXPECT_EQ(countdown, 49u);
	EXPECT_FALSE(town_empty::Turn(countdown, 2));
	EXPECT_EQ(countdown, 0u);
}

TEST(TownEmpty, TheLastTurnEmptiesTheTownEvenWithPeople)
{
	// The countdown goes down before the town counts its people
	uint32_t countdown = 1;
	EXPECT_TRUE(town_empty::Turn(countdown, 4));
}

TEST(TownEmpty, AnEmptyTownBelievesInEveryPlayerWhatItKeepsForTheNeutralPlayer)
{
	using namespace magic::town_belief;
	Belief town;
	town.neutral = 0.5f;
	town.belief.fill(3.0f);
	town.pending.fill(1.0f);
	town.recent.fill(0.25f);
	town.cap.fill(2.0f);
	std::array<DesireDecay, k_DesireCount> table {};
	table.at(0).levelAfterGain = 0.75f;
	ResetForEmptyTown(town, table);
	for (size_t p = 0; p < k_PlayerCount; ++p)
	{
		EXPECT_EQ(town.belief.at(p), 0.5f) << p;
		EXPECT_EQ(town.pending.at(p), 0.0f) << p;
		EXPECT_EQ(town.cap.at(p), k_DefaultCap) << p;
		// Its recent tallies stay
		EXPECT_EQ(town.recent.at(p), 0.25f) << p;
	}
	ASSERT_TRUE(town.desireLevel.has_value());
	EXPECT_EQ(town.desireLevel->at(0), 0.75f);
}

TEST(TownEmpty, TheNeutralBeliefStaysWithinTheDefaultCap)
{
	using namespace magic::town_belief;
	Belief town;
	town.neutral = 12.0f;
	ResetForEmptyTown(town, std::array<DesireDecay, k_DesireCount> {});
	// Every belief is kept within the default cap, the belief in the neutral player too
	EXPECT_EQ(town.belief.at(static_cast<size_t>(PlayerNames::NEUTRAL)), k_DefaultCap);
	EXPECT_EQ(town.belief.at(0), k_DefaultCap);
}
