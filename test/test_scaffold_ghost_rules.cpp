/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <bit>

#include <gtest/gtest.h>

#include "ECS/ScaffoldGhostRules.h"

using namespace openblack::ecs;
using openblack::ecs::components::ScaffoldOffer;
using openblack::ecs::components::ScaffoldPhantom;
using scaffold_ghost::GhostSound;

namespace
{

ScaffoldPhantom Showing(int32_t changeMs, bool grows)
{
	ScaffoldPhantom phantom;
	phantom.shown = ScaffoldOffer {};
	phantom.changeMs = changeMs;
	phantom.grows = grows;
	phantom.scale = 0.8f;
	phantom.yAngle = 1.0f;
	return phantom;
}

} // namespace

TEST(ScaffoldGhostRules, GrowsFromSmallAndPlaysTheGrowSoundOneTime)
{
	scaffold_ghost::SoundsPlayed played;
	const auto phantom = Showing(400, true);
	const auto first = scaffold_ghost::Held(phantom, 0, 0.1f, played);
	ASSERT_TRUE(first.pose.has_value());
	EXPECT_EQ(first.sound, GhostSound::Grows);
	// 400 milliseconds of change left: 1 - 400 * 0.0025 is 0
	EXPECT_FLOAT_EQ(first.pose->scale, 0.0f);
	const auto second = scaffold_ghost::Held(phantom, 50, 0.1f, played);
	EXPECT_EQ(second.sound, GhostSound::None);
	EXPECT_TRUE(played.grows);
	EXPECT_FALSE(played.shrinks);
}

TEST(ScaffoldGhostRules, TheTimeIntoTheTurnShortensTheChange)
{
	scaffold_ghost::SoundsPlayed played;
	const auto look = scaffold_ghost::Held(Showing(100, true), 60, 0.0f, played);
	ASSERT_TRUE(look.pose.has_value());
	// 40 milliseconds left
	const float part = 40.0f * scaffold_ghost::k_SizePerMs;
	const float shown = 1.0f - part;
	EXPECT_EQ(look.pose->scale, shown * 0.8f);
}

TEST(ScaffoldGhostRules, BecomesSmallAndPlaysTheShrinkSoundAfterTheGrowSound)
{
	scaffold_ghost::SoundsPlayed played {.shrinks = false, .grows = true};
	const auto look = scaffold_ghost::Held(Showing(200, false), 0, 0.0f, played);
	ASSERT_TRUE(look.pose.has_value());
	EXPECT_EQ(look.sound, GhostSound::Shrinks);
	EXPECT_FALSE(played.grows);
	const float part = 200.0f * 0.8f;
	EXPECT_EQ(look.pose->scale, part * scaffold_ghost::k_SizePerMs);
}

TEST(ScaffoldGhostRules, ShowsNothingWhenSmallAndNotGrowing)
{
	scaffold_ghost::SoundsPlayed played;
	const auto look = scaffold_ghost::Held(Showing(50, false), 70, 0.0f, played);
	EXPECT_FALSE(look.pose.has_value());
	EXPECT_EQ(look.sound, GhostSound::None);
}

TEST(ScaffoldGhostRules, ShowsTheFullSizeWhenTheGrowthIsComplete)
{
	scaffold_ghost::SoundsPlayed played;
	const auto look = scaffold_ghost::Held(Showing(0, true), 30, 0.0f, played);
	ASSERT_TRUE(look.pose.has_value());
	EXPECT_EQ(look.pose->scale, 0.8f);
	// The sound plays only while the phantom changes
	EXPECT_EQ(look.sound, GhostSound::None);
}

TEST(ScaffoldGhostRules, TurnsOnBetweenTheGameTurns)
{
	scaffold_ghost::SoundsPlayed played;
	const float turn = std::bit_cast<float>(0x3D4CCCCDU); // 0.05
	const auto look = scaffold_ghost::Held(Showing(0, true), 37, turn, played);
	ASSERT_TRUE(look.pose.has_value());
	const float share = 37.0f * scaffold_ghost::k_TurnPerMs;
	const float added = share * turn;
	EXPECT_EQ(look.pose->yAngle, added + 1.0f);
}

TEST(ScaffoldGhostRules, TheGrowSoundIsLowerForAHigherAbodeNumber)
{
	EXPECT_EQ(scaffold_ghost::GrowPitchPercent(0), 110);
	EXPECT_EQ(scaffold_ghost::GrowPitchPercent(5), 95);
	EXPECT_EQ(scaffold_ghost::GrowPitchPercent(-1), 100);
}

TEST(ScaffoldGhostRules, ThePhantomOnASiteFadesAsTheBuildingGrows)
{
	EXPECT_EQ(scaffold_ghost::SiteAlpha(0.0f), 85);
	EXPECT_EQ(scaffold_ghost::SiteAlpha(0.5f), 24);
	EXPECT_EQ(scaffold_ghost::SiteAlpha(1.0f), 14);
}

TEST(ScaffoldGhostRules, TheScaffoldOnASiteBecomesSmallToALimit)
{
	EXPECT_EQ(scaffold_ghost::ScaleOnSite(0.0f, 0.7f), 0.7f);
	const float part = 0.2f * 1.5f;
	const float left = 1.0f - part;
	EXPECT_EQ(scaffold_ghost::ScaleOnSite(0.2f, 0.7f), left * 0.7f);
	EXPECT_EQ(scaffold_ghost::ScaleOnSite(0.9f, 0.7f), scaffold_ghost::k_SmallestOnSite);
}
