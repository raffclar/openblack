/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <numbers>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/HandAnimation.h"
#include "3D/HandNavigationPose.h"
#include "Creature/FightMarks.h"
#include "Hand/HandFeel.h"
#include "Magic/HandMotion.h"

using namespace openblack;
namespace marks = openblack::creature_fight::marks;

namespace
{

[[nodiscard]] marks::Mark MarkOn(marks::Anchor anchor)
{
	return marks::Mark {.anchor = anchor};
}

} // namespace

TEST(HandFeelRest2, TheHandGoesToNoSparkleWithoutAPressHeld)
{
	const std::array list {MarkOn(marks::Anchor::Opponent)};
	EXPECT_FALSE(marks::HandMark(list, false).has_value());
}

TEST(HandFeelRest2, TheHandGoesToTheLastSparkleOnTheOpponent)
{
	const std::array list {MarkOn(marks::Anchor::Opponent), MarkOn(marks::Anchor::Self), MarkOn(marks::Anchor::Opponent),
	                       MarkOn(marks::Anchor::Ground)};
	EXPECT_EQ(marks::HandMark(list, true), 2u);
}

TEST(HandFeelRest2, APressOnTheOwnCreatureSendsTheHandToAnEarlierSparkleOnTheOpponent)
{
	const std::array list {MarkOn(marks::Anchor::Opponent), MarkOn(marks::Anchor::Self)};
	EXPECT_EQ(marks::HandMark(list, true), 0u);
}

TEST(HandFeelRest2, NoSparkleOnTheOpponentLeavesTheHandOnTheCursor)
{
	const std::array list {MarkOn(marks::Anchor::Self), MarkOn(marks::Anchor::Ground)};
	EXPECT_FALSE(marks::HandMark(list, true).has_value());
}

TEST(HandFeelRest2, TheRingUpLeansTowardsTheFocus)
{
	const auto up = hand_navigation_pose::UpTowardsFocusOnRing({0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 4.0f});
	// The lean is three tenths of the distance of five
	const glm::vec3 expected = glm::vec3(3.0f, 1.5f, 4.0f) / std::sqrt(9.0f + 2.25f + 16.0f);
	EXPECT_NEAR(up.x, expected.x, 1e-6f);
	EXPECT_NEAR(up.y, expected.y, 1e-6f);
	EXPECT_NEAR(up.z, expected.z, 1e-6f);
}

TEST(HandFeelRest2, TheRingUpAddsTheSquaresYZX)
{
	const glm::vec3 from {0.1f, 0.2f, 0.3f};
	const glm::vec3 focus {123.456f, -7.89f, 98.765f};
	const float dx = focus.x - from.x;
	const float dy = focus.y - from.y;
	const float dz = focus.z - from.z;
	const float lean = std::sqrt((dy * dy + dz * dz) + dx * dx) * 0.3f + dy;
	const float share = 1.0f / std::sqrt((lean * lean + dz * dz) + dx * dx);
	const auto up = hand_navigation_pose::UpTowardsFocusOnRing(from, focus);
	EXPECT_EQ(up.x, share * dx);
	EXPECT_EQ(up.y, share * lean);
	EXPECT_EQ(up.z, dz * share);
}

TEST(HandFeelRest2, TheRingUpIsZeroAtTheFocus)
{
	const auto up = hand_navigation_pose::UpTowardsFocusOnRing({5.0f, 1.0f, 2.0f}, {5.0f, 1.0f, 2.0f});
	EXPECT_EQ(up, glm::vec3(0.0f));
}

TEST(HandFeelRest2, NoTiltLeavesTheHandStraightUp)
{
	const auto up = hand_feel::PourUp({0.0f, 50.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, 0.0f);
	EXPECT_NEAR(up.x, 0.0f, 1e-6f);
	EXPECT_NEAR(up.y, 1.0f, 1e-6f);
	EXPECT_NEAR(up.z, 0.0f, 1e-6f);
}

TEST(HandFeelRest2, AQuarterTiltLaysTheHandOverAboutTheLineToTheCamera)
{
	// The camera is along z from the hand, so the hand turns in the plane of x and y
	const auto up = hand_feel::PourUp({0.0f, 0.0f, 100.0f}, {0.0f, 0.0f, 0.0f}, std::numbers::pi_v<float> / 2.0f);
	EXPECT_NEAR(up.x, 1.0f, 1e-6f);
	EXPECT_NEAR(up.y, 0.0f, 1e-6f);
	EXPECT_NEAR(up.z, 0.0f, 1e-6f);
}

TEST(HandFeelRest2, ThePourAddsTheLastTurnToTheChangeTimesTheFraction)
{
	magic::PourState pour;
	pour.previous = {.raise = 2.5f, .tilt = 0.1f};
	pour.current = {.raise = 7.25f, .tilt = 0.7f};
	const float fraction = 0.3f;
	const auto pose = magic::PourPoseAt(pour, fraction);
	EXPECT_EQ(pose.tilt, ((0.7f - 0.1f) * fraction) + 0.1f);
	EXPECT_EQ(pose.raise, ((7.25f - 2.5f) * fraction) + 2.5f);
}

TEST(HandFeelRest2, TheDrawnScaleDividesTheStandardHeightFirst)
{
	const float rest = 3.7f;
	const float size = 1.3f;
	EXPECT_EQ(HandAnimation::DrawnScale(rest, size), (3.2f / rest) * size);
}
