/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureFart.h"

using namespace openblack;
using namespace openblack::creature_fart;
using creature_layers::BodyAction;

namespace
{
BodyAction Squat(BodyAction::Phase phase, size_t loop = creature_layers::animations::k_Poo)
{
	BodyAction body;
	body.kind = BodyAction::Kind::Sequence;
	body.phase = phase;
	body.animations = {creature_layers::animations::k_StartPoo, loop, creature_layers::animations::k_EndPoo};
	return body;
}
} // namespace

TEST(CreatureFart, OnlyTheSquatsLoopCounts)
{
	EXPECT_TRUE(PlaysSquatLoop(Squat(BodyAction::Phase::Loop)));
	EXPECT_FALSE(PlaysSquatLoop(Squat(BodyAction::Phase::Start)));
	EXPECT_FALSE(PlaysSquatLoop(Squat(BodyAction::Phase::End)));
	EXPECT_FALSE(PlaysSquatLoop(Squat(BodyAction::Phase::Loop, creature_layers::animations::k_Puke)));
	EXPECT_FALSE(PlaysSquatLoop(BodyAction {}));
}

TEST(CreatureFart, TheWindComesOutEveryFifthGameTurn)
{
	uint32_t turns = 0;
	std::vector<int> bursts;
	for (int turn = 1; turn <= 12; ++turn)
	{
		if (CountTurn(turns))
		{
			bursts.push_back(turn);
		}
	}
	EXPECT_EQ(bursts, (std::vector<int> {5, 10}));
	EXPECT_EQ(turns, 2u);
}

TEST(CreatureFart, TheCountOfTheLastSquatContinuesInTheNextSquat)
{
	// The last squat stopped two turns after its last lump. The next squat makes its first lump on its third turn
	uint32_t turns = 2;
	EXPECT_FALSE(CountTurn(turns));
	EXPECT_FALSE(CountTurn(turns));
	EXPECT_TRUE(CountTurn(turns));
	EXPECT_EQ(turns, 0u);
}

TEST(CreatureFart, AFartMakesACloudAndAPooALumpOnEachBurst)
{
	EXPECT_EQ(BurstOf(true), Burst::Cloud);
	EXPECT_EQ(BurstOf(false), Burst::Lump);
}

TEST(CreatureFart, TheCloudBlowsOutBehindByItsSize)
{
	// Each random number is half the limit. Thus there is no spread, and each puff moves straight back
	std::vector<float> limits;
	const auto random = [&limits](float limit) {
		limits.push_back(limit);
		return limit * 0.5f;
	};
	const auto velocities = PuffVelocities(glm::vec2(0.0f, -1.0f), 2.0f, random);
	ASSERT_EQ(limits.size(), 3 * k_Puffs);
	EXPECT_FLOAT_EQ(limits.front(), 30.0f);
	for (const auto& velocity : velocities)
	{
		EXPECT_FLOAT_EQ(velocity.x, 0.0f);
		EXPECT_FLOAT_EQ(velocity.y, 0.0f);
		EXPECT_FLOAT_EQ(velocity.z, 30.0f);
	}
}

TEST(CreatureFart, EachPuffDrawsAlongZThenUpThenAlongX)
{
	std::vector<float> draws {1.0f, 2.0f, 3.0f};
	size_t next = 0;
	const auto random = [&](float) { return draws[next++ % draws.size()]; };
	const auto velocities = PuffVelocities(glm::vec2(1.0f, 0.0f), 1.0f, random);
	// The spread is 15, thus each offset is the random number minus 7.5. The puff moves 15 back along -x
	EXPECT_FLOAT_EQ(velocities[0].z, 1.0f - 7.5f);
	EXPECT_FLOAT_EQ(velocities[0].y, 2.0f - 7.5f);
	EXPECT_FLOAT_EQ(velocities[0].x, 3.0f - 7.5f - 15.0f);
}

TEST(CreatureFart, OnlyAnEvilLookingBodyStinks)
{
	EXPECT_TRUE(Stinks(-0.51f));
	EXPECT_FALSE(Stinks(-0.5f));
	EXPECT_FALSE(Stinks(0.0f));
	EXPECT_FALSE(Stinks(1.0f));
}

TEST(CreatureFart, TheSmokeContinuesOnlyWhileThePlanThatMadeItContinues)
{
	// The creature does the plan that made the smoke
	EXPECT_TRUE(SmokeContinues(7, true, 7));
	// The plan is complete and the creature does no plan
	EXPECT_FALSE(SmokeContinues(7, false, 7));
	// A different plan replaced the plan that made the smoke
	EXPECT_FALSE(SmokeContinues(7, true, 8));
}
