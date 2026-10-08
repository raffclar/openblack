/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <span>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLook.h"

using namespace openblack;
using namespace openblack::creature_look;

namespace
{
constexpr float k_Tolerance = 1e-4f;
/// One game turn a second, so that a turn watched is a second watched
constexpr float k_TurnsPerSecond = 1.0f;

Viewer AtOrigin(float size = 1.0f)
{
	return {.position = {0.0f, 0.0f, 0.0f}, .ahead = {0.0f, 0.0f, -1.0f}, .size = size};
}
} // namespace

TEST(CreatureLook, DovesAndTheCitadelCatchTheEyeMostAndFixedThingsLeast)
{
	EXPECT_GT(InterestOf(Interest::Dove), InterestOf(Interest::Creature));
	EXPECT_GT(InterestOf(Interest::Creature), InterestOf(Interest::Animal));
	EXPECT_GT(InterestOf(Interest::Animal), InterestOf(Interest::Villager));
	EXPECT_GT(InterestOf(Interest::Villager), InterestOf(Interest::Abode));
	EXPECT_GT(InterestOf(Interest::Abode), InterestOf(Interest::Tree));
	EXPECT_GT(InterestOf(Interest::Tree), InterestOf(Interest::Fixed));
	EXPECT_FLOAT_EQ(InterestOf(Interest::Dove), InterestOf(Interest::Citadel));
}

TEST(CreatureLook, BiggerCreaturesSeeFurther)
{
	// An odd number of cells of ten metres each: the root of 160 is 12.6, six cells either side of its own
	EXPECT_FLOAT_EQ(LookRange(0.0f), 130.0f);
	EXPECT_GT(LookRange(2.0f), LookRange(0.0f));
	EXPECT_GE(LookRange(4.0f), LookRange(2.0f));
	// A size below zero leaves a range, rather than the root of a negative number
	EXPECT_GT(LookRange(-10.0f), 0.0f);
}

TEST(CreatureLook, ThingsWithinHalfTheRangeAreAsInterestingAsTheyGet)
{
	EXPECT_FLOAT_EQ(DistanceFactor(0.0f, 100.0f), 1.0f);
	EXPECT_FLOAT_EQ(DistanceFactor(49.0f, 100.0f), 1.0f);
	EXPECT_FLOAT_EQ(DistanceFactor(50.0f, 100.0f), 0.5f);
	EXPECT_FLOAT_EQ(DistanceFactor(100.0f, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(DistanceFactor(10.0f, 0.0f), 0.0f);
}

TEST(CreatureLook, CreaturesSeeAheadOfThemAndWhateverIsOnTopOfThem)
{
	const auto viewer = AtOrigin();
	EXPECT_TRUE(CanSee(viewer, {0.0f, 0.0f, -50.0f}));
	// Anything within a cell is seen, even behind
	EXPECT_TRUE(CanSee(viewer, {0.0f, 0.0f, 5.0f}));
	// Behind, and beyond the range
	EXPECT_FALSE(CanSee(viewer, {0.0f, 0.0f, 50.0f}));
	EXPECT_FALSE(CanSee(viewer, {0.0f, 0.0f, -10000.0f}));
	// A creature facing nowhere sees nothing it isn't standing on
	const Viewer lost {.position = {0.0f, 0.0f, 0.0f}, .ahead = {0.0f, 0.0f, 0.0f}, .size = 1.0f};
	EXPECT_FALSE(CanSee(lost, {0.0f, 0.0f, -50.0f}));
	EXPECT_TRUE(CanSee(lost, {0.0f, 0.0f, -5.0f}));
}

TEST(CreatureLook, TheNearerAndTheMoreInterestingWins)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 2> candidates {
	    Candidate {.id = 1, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}},
	    Candidate {.id = 2, .kind = Interest::Villager, .point = {0.0f, 0.0f, -20.0f}},
	};
	const auto target = LookAbout({}, candidates, viewer, k_TurnsPerSecond);
	EXPECT_EQ(target.id, 2u);
	EXPECT_EQ(target.kind, Interest::Villager);
	EXPECT_EQ(target.watchedTurns, 0u);

	// The same thing further off catches the eye less
	const std::array<Candidate, 1> near {Candidate {.id = 3, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}}};
	const std::array<Candidate, 1> far {Candidate {.id = 3, .kind = Interest::Tree, .point = {0.0f, 0.0f, -120.0f}}};
	EXPECT_GT(Priority(viewer, near[0].kind, near[0].point), Priority(viewer, far[0].kind, far[0].point));
}

TEST(CreatureLook, WhatIsWatchedIsWatchedALittleLongerEachTurn)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 1> candidates {Candidate {.id = 7, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}}};
	auto target = LookAbout({}, candidates, viewer, k_TurnsPerSecond);
	ASSERT_EQ(target.id, 7u);
	for (uint32_t turn = 1; turn <= 5; ++turn)
	{
		target = LookAbout(target, candidates, viewer, k_TurnsPerSecond);
		EXPECT_EQ(target.watchedTurns, turn);
		EXPECT_EQ(target.id, 7u);
	}
	// It moves: the creature follows it as long as it stays in sight
	const std::array<Candidate, 1> moved {Candidate {.id = 7, .kind = Interest::Tree, .point = {10.0f, 0.0f, -30.0f}}};
	target = LookAbout(target, moved, viewer, k_TurnsPerSecond);
	EXPECT_EQ(target.id, 7u);
	EXPECT_NEAR(target.point.x, 10.0f, k_Tolerance);
}

TEST(CreatureLook, WhatGoesOutOfSightIsDropped)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 1> candidates {Candidate {.id = 7, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}}};
	auto target = LookAbout({}, candidates, viewer, k_TurnsPerSecond);
	ASSERT_EQ(target.id, 7u);
	// Gone from the candidates: it is no longer there
	target = LookAbout(target, std::span<const Candidate> {}, viewer, k_TurnsPerSecond);
	EXPECT_FALSE(target.id.has_value());
	// Still there, but behind the creature now
	target = LookAbout({}, candidates, viewer, k_TurnsPerSecond);
	ASSERT_EQ(target.id, 7u);
	const std::array<Candidate, 1> behind {Candidate {.id = 7, .kind = Interest::Tree, .point = {0.0f, 0.0f, 60.0f}}};
	target = LookAbout(target, behind, viewer, k_TurnsPerSecond);
	EXPECT_FALSE(target.id.has_value());
}

TEST(CreatureLook, BoredomLetsSomethingLessInterestingTakeOver)
{
	const auto viewer = AtOrigin();
	const std::array<Candidate, 1> villager {Candidate {.id = 1, .kind = Interest::Villager, .point = {0.0f, 0.0f, -20.0f}}};
	auto target = LookAbout({}, villager, viewer, k_TurnsPerSecond);
	ASSERT_EQ(target.id, 1u);
	// A tree does not catch its eye while the villager is fresh
	const std::array<Candidate, 2> both {
	    villager[0],
	    Candidate {.id = 2, .kind = Interest::Tree, .point = {0.0f, 0.0f, -20.0f}},
	};
	target = LookAbout(target, both, viewer, k_TurnsPerSecond);
	EXPECT_EQ(target.id, 1u);
	// Twenty seconds of watching, and nothing else catches its eye at all
	target.watchedTurns = static_cast<uint32_t>(k_BoredSeconds);
	target = LookAbout(target, both, viewer, k_TurnsPerSecond);
	EXPECT_EQ(target.id, 1u);
}

TEST(CreatureLook, WithNothingToLookAtItLooksAhead)
{
	const auto point = PointAhead(AtOrigin(2.0f));
	EXPECT_NEAR(point.x, 0.0f, k_Tolerance);
	EXPECT_NEAR(point.y, k_HeadHeight * 2.0f, k_Tolerance);
	EXPECT_NEAR(point.z, -50.0f, k_Tolerance);
	// Facing nowhere, it looks along -z
	const Viewer lost {.position = {1.0f, 0.0f, 2.0f}, .ahead = {0.0f, 1.0f, 0.0f}, .size = 1.0f};
	const auto fallback = PointAhead(lost);
	EXPECT_NEAR(fallback.x, 1.0f, k_Tolerance);
	EXPECT_NEAR(fallback.z, 2.0f - 50.0f, k_Tolerance);
}
