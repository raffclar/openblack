/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureLead.h"
#include "Creature/CreatureReach.h"

using namespace openblack;
namespace lead = openblack::creature_lead;

namespace
{
/// 655.36 units are one metre a second
constexpr uint16_t k_OneMetreASecond = 655;

lead::Living LivingAt(glm::vec2 place, uint16_t speed, uint16_t angle)
{
	return {.mapPosition = {map_coords::ToFixed(place.x), map_coords::ToFixed(place.y)},
	        .height = 0.0f,
	        .speed = speed,
	        .gameAngle = angle,
	        .moved = true};
}
} // namespace

TEST(CreatureLead, ASlowLivingThingStaysWhereItIs)
{
	// 65 units are just under a tenth of a metre a second
	const auto living = LivingAt({100.0f, 200.0f}, 65, 300);
	EXPECT_EQ(lead::WhereAfter(living, 10.0f), lead::PlaceOf(living));
}

TEST(CreatureLead, ALivingThingGoesOnAtItsSpeed)
{
	const auto living = LivingAt({100.0f, 200.0f}, k_OneMetreASecond * 4, 700);
	const auto after = lead::WhereAfter(living, 2.0f);
	EXPECT_NEAR(glm::distance(glm::xz(after), glm::xz(lead::PlaceOf(living))), 8.0f, 0.02f);
	EXPECT_EQ(after.y, 0.0f);
}

TEST(CreatureLead, TheWalkGoesAheadOfALivingThingThatWalksAway)
{
	const auto living = LivingAt({100.0f, 100.0f}, k_OneMetreASecond * 3, 0);
	const lead::Walker walker {.position = {100.0f, 0.0f, 140.0f}, .size = 1.0f, .runSpeed = 22.5f, .fraction = 0.4f};
	const auto aim = lead::WalkTo(walker, living);
	const auto place = lead::PlaceOf(living);
	EXPECT_GT(glm::distance(glm::xz(aim), glm::xz(place)), 1.0f);
	// The lead point is on the line that the thing walks along
	const auto ahead = lead::WhereAfter(living, 1.0f);
	const auto along = glm::normalize(glm::xz(ahead) - glm::xz(place));
	const auto towards = glm::normalize(glm::xz(aim) - glm::xz(place));
	EXPECT_NEAR(glm::dot(along, towards), 1.0f, 1e-4f);
}

TEST(CreatureLead, TheTimeOfTheWalkCountsNoMoreThanThirtyTimesTheSize)
{
	const auto living = LivingAt({100.0f, 100.0f}, k_OneMetreASecond * 2, 512);
	const lead::Walker near {.position = {100.0f, 0.0f, 100.0f + 40.0f}, .size = 1.0f, .runSpeed = 22.5f, .fraction = 0.4f};
	auto far = near;
	far.position.z = 100.0f + 400.0f;
	// Both distances are more than 30 units, so both walks take the same time
	EXPECT_EQ(lead::WalkTo(near, living), lead::WalkTo(far, living));
}

TEST(CreatureLead, TheReachTakesTheMillisecondsTimesTheRootOfTwiceTheSize)
{
	EXPECT_NEAR(lead::ReachSeconds(1.0f, 1000), std::sqrt(2.0f), 1e-5f);
	EXPECT_NEAR(lead::ReachSeconds(2.0f, 500), 1.0f, 1e-5f);
	EXPECT_LT(lead::ReachSeconds(1.0f, -100), 0.0f);
}

TEST(CreatureLead, TheMovingReachIsTheMiddleDepthOfTheCorners)
{
	const creature_reach::Points points {glm::vec3 {-1.0f, 0.0f, -1.0f}, glm::vec3 {1.0f, 0.0f, -1.0f},
	                                     glm::vec3 {-1.0f, 0.0f, -3.0f}, glm::vec3 {1.0f, 0.0f, -3.0f}};
	EXPECT_FLOAT_EQ(creature_reach::MovingReach(points, 1.0f), 2.0f);
	EXPECT_FLOAT_EQ(creature_reach::MovingReach(points, 2.5f), 5.0f);
}
