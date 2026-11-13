/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Components/DestructionGhost.h"
#include "ECS/DestructionGhostRules.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"

using namespace openblack::ecs;

// The ghost of a thing that has gone is drawn only: it is in the list of the explosions, not in the world
TEST(DestructionGhosts, AGhostRunsDownByTheFramesOnceDrawnAndGoes)
{
	systems::ExplosionSystem explosions;
	explosions.AddGhost(components::DestructionGhost {.mesh = 7});
	ASSERT_EQ(explosions.Ghosts().size(), 1u);
	// The first frame draws it and takes nothing of its time
	explosions.Update(100.0f);
	ASSERT_EQ(explosions.Ghosts().size(), 1u);
	EXPECT_EQ(explosions.Ghosts()[0].millisecondsLeft, 500);
	explosions.Update(300.0f);
	ASSERT_EQ(explosions.Ghosts().size(), 1u);
	EXPECT_EQ(explosions.Ghosts()[0].millisecondsLeft, 200);
	explosions.Update(300.0f);
	EXPECT_TRUE(explosions.Ghosts().empty());
}

TEST(DestructionGhosts, ANewLandHasNoGhosts)
{
	systems::ExplosionSystem explosions;
	explosions.AddGhost(components::DestructionGhost {.mesh = 7});
	explosions.Reset();
	EXPECT_TRUE(explosions.Ghosts().empty());
}

TEST(DestructionGhosts, AGhostSortsByThePlaceOfTheThingItCopies)
{
	// A model put at (10, 20, 30) and made twice as big: the point is its place, not the middle of its box
	components::DestructionGhost ghost {.mesh = 1, .model = glm::mat4(1.0f)};
	ghost.model[0][0] = 2.0f;
	ghost.model[1][1] = 2.0f;
	ghost.model[2][2] = 2.0f;
	ghost.model[3] = glm::vec4(10.0f, 20.0f, 30.0f, 1.0f);
	EXPECT_EQ(components::SortPoint(ghost), glm::vec3(10.0f, 20.0f, 30.0f));
}

// A ghost shows for half a second of game time, whatever the frame rate. The first frame that draws it takes nothing.
TEST(DestructionGhosts, AGhostShowsForHalfASecondOfGameTimeAtAnyFrameRate)
{
	for (const int32_t frameMilliseconds : {1, 10, 16, 33, 100})
	{
		int32_t left = destruction_ghost::k_LifeMilliseconds;
		bool shown = false;
		int32_t gameTime = 0;
		while (!shown || left > 0)
		{
			left = destruction_ghost::TimeLeftAfter(left, frameMilliseconds, shown);
			gameTime += shown ? frameMilliseconds : 0;
			shown = true;
		}
		// The last frame can go past the end by less than one frame
		EXPECT_GE(gameTime, 500) << frameMilliseconds;
		EXPECT_LT(gameTime, 500 + frameMilliseconds) << frameMilliseconds;
	}
}

// A paused game gives frames of no game time, and the ghost stays
TEST(DestructionGhosts, AGhostStaysWhileTheGameIsPaused)
{
	EXPECT_EQ(destruction_ghost::TimeLeftAfter(300, 0, true), 300);
	EXPECT_EQ(destruction_ghost::TimeLeftAfter(300, 40, false), 300);
	EXPECT_EQ(destruction_ghost::TimeLeftAfter(30, 40, true), 0);
}

// The game's reference: 255 less the share of the time left, of 255, cut to a whole number
TEST(DestructionGhosts, ThePatternReferenceRisesFromNoneToAllAsTheGhostRunsOut)
{
	EXPECT_EQ(destruction_ghost::PatternAlphaRef(500), 0);
	EXPECT_EQ(destruction_ghost::PatternAlphaRef(250), 128);
	EXPECT_EQ(destruction_ghost::PatternAlphaRef(1), 255);
	EXPECT_EQ(destruction_ghost::PatternAlphaRef(0), 255);
	for (int32_t left = 1; left <= 500; ++left)
	{
		EXPECT_LE(destruction_ghost::PatternAlphaRef(left), destruction_ghost::PatternAlphaRef(left - 1)) << left;
	}
}

// The pattern moves by twice the cosine of the share of the time left, and by 1.7 times the sine of 0.7 times it
TEST(DestructionGhosts, ThePatternMovesByTheShareOfTheTimeLeft)
{
	const auto start = destruction_ghost::PatternScroll(500);
	EXPECT_FLOAT_EQ(start.x, static_cast<float>(2.0 * std::cos(1.0)));
	EXPECT_FLOAT_EQ(start.y, static_cast<float>(std::sin(0.7) * 1.7000000476837158));
	const auto end = destruction_ghost::PatternScroll(0);
	EXPECT_FLOAT_EQ(end.x, 2.0f);
	EXPECT_FLOAT_EQ(end.y, 0.0f);
	EXPECT_EQ(destruction_ghost::k_OwnMaterialsAlphaRef, 10);
}
