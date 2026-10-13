/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureRoute.h"

using namespace openblack;
using namespace openblack::creature_obstacles;
using openblack::creature_route::Circle;

namespace
{
constexpr float k_Tolerance = 1e-3f;
constexpr float k_Pi = std::numbers::pi_v<float>;

/// A creature 15 units tall reaching 2 units round its middle
Walker AWalker()
{
	return {
	    .position = {0.0f, 0.0f}, .height = 15.0f, .radius = 2.0f, .scriptControlled = false, .owner = PlayerNames::PLAYER_ONE};
}

/// A thing of a kind, its box half sizes given, scale one, at the origin facing along x
Thing AThing(Avoidance avoidance, Shape shape, glm::vec3 halfSize)
{
	return {.avoidance = avoidance,
	        .shape = shape,
	        .position = {0.0f, 0.0f},
	        .yAngle = 0.0f,
	        .scale = 1.0f,
	        .boxCentre = glm::vec3(0.0f),
	        .boxHalfSize = halfSize,
	        .height = ModelHeight(halfSize, 1.0f),
	        .width = ModelWidth(halfSize, 1.0f),
	        .burning = false,
	        .hasFire = false,
	        .justBegun = false,
	        .walking = false,
	        .playerAlignment = 0.0f,
	        .worshipSites = {},
	        .owner = PlayerNames::NEUTRAL};
}

} // namespace

TEST(CreatureObstacles, BuildingsATenthOfTheCreaturesHeightAreWalkedRound)
{
	const auto walker = AWalker();
	auto house = AThing(Avoidance::Building, Shape::ModelBox, {5.0f, 0.75f, 5.0f});
	EXPECT_TRUE(MustAvoid(house, walker));
	house.height = 1.49f;
	EXPECT_FALSE(MustAvoid(house, walker));
	house.burning = true;
	EXPECT_TRUE(MustAvoid(house, walker));
	// Only just begun on its site, it is walked over until it burns
	auto site = AThing(Avoidance::Building, Shape::ModelBox, {5.0f, 4.0f, 5.0f});
	site.justBegun = true;
	EXPECT_FALSE(MustAvoid(site, walker));
	site.burning = true;
	EXPECT_TRUE(MustAvoid(site, walker));
}

TEST(CreatureObstacles, EachKindHasItsOwnRule)
{
	const auto walker = AWalker();
	// Trees one and a half times its height
	auto tree = AThing(Avoidance::Tree, Shape::Trunk, {2.0f, 11.0f, 2.0f});
	EXPECT_FALSE(MustAvoid(tree, walker));
	tree.height = 22.5f;
	EXPECT_TRUE(MustAvoid(tree, walker));
	// Forests, fields and fragments only burning; fields not for a creature a script moves
	auto forest = AThing(Avoidance::Forest, Shape::ModelBox, {50.0f, 50.0f, 50.0f});
	EXPECT_FALSE(MustAvoid(forest, walker));
	forest.burning = true;
	EXPECT_TRUE(MustAvoid(forest, walker));
	auto field = AThing(Avoidance::BurningUnlessScripted, Shape::ModelBox, {5.0f, 1.0f, 5.0f});
	field.burning = true;
	EXPECT_TRUE(MustAvoid(field, walker));
	auto scripted = walker;
	scripted.scriptControlled = true;
	EXPECT_FALSE(MustAvoid(field, scripted));
	// Scenery always, the people never
	EXPECT_TRUE(MustAvoid(AThing(Avoidance::Always, Shape::Scenery, {0.1f, 0.1f, 0.1f}), walker));
	EXPECT_FALSE(MustAvoid(AThing(Avoidance::Never, Shape::AtPlace, {50.0f, 50.0f, 50.0f}), walker));
	// Other creatures only while they aren't walking along routes of their own, however small
	auto other = AThing(Avoidance::Creature, Shape::Creature, {0.0f, 0.0f, 0.0f});
	EXPECT_TRUE(MustAvoid(other, walker));
	other.walking = true;
	EXPECT_FALSE(MustAvoid(other, walker));
	// A vortex out of the land is kept clear of, however low, unless it may take the creature
	const auto vortexOut = AThing(Avoidance::VortexOutOfTheLand, Shape::AtPlace, {0.1f, 0.1f, 0.1f});
	EXPECT_TRUE(MustAvoid(vortexOut, walker));
	auto taken = walker;
	taken.mayBeTakenByVortex = true;
	EXPECT_FALSE(MustAvoid(vortexOut, taken));
	// Shields only another player's, and not for a creature a script moves
	auto shield = AThing(Avoidance::Shield, Shape::ModelBox, {10.0f, 10.0f, 10.0f});
	shield.owner = PlayerNames::PLAYER_TWO;
	EXPECT_TRUE(MustAvoid(shield, walker));
	EXPECT_FALSE(MustAvoid(shield, scripted));
	shield.owner = walker.owner;
	EXPECT_FALSE(MustAvoid(shield, walker));
}

TEST(CreatureObstacles, LowThingsTakeMoreOfTheCreaturesRadius)
{
	EXPECT_NEAR(RadiusShare(13.0f, 15.0f), 0.4f, k_Tolerance);
	EXPECT_NEAR(RadiusShare(0.0f, 15.0f), 0.7f, k_Tolerance);
	EXPECT_NEAR(RadiusShare(6.0f, 15.0f), 0.55f, k_Tolerance);
}

TEST(CreatureObstacles, ASquareBuildingIsOneCircle)
{
	const auto walker = AWalker();
	// 14 tall: taller than four fifths of the creature, so it keeps clear by six tenths of its radius
	auto house = AThing(Avoidance::Building, Shape::ModelBox, {5.0f, 7.0f, 4.5f});
	house.position = {100.0f, 200.0f};
	house.boxCentre = {1.0f, 0.0f, 0.0f};
	house.yAngle = k_Pi / 2.0f;
	const auto circles = CirclesOf(house, walker);
	ASSERT_EQ(circles.size(), 1u);
	// The box's middle turned with the building
	EXPECT_NEAR(circles[0].centre.x, 100.0f, k_Tolerance);
	EXPECT_NEAR(circles[0].centre.y, 201.0f, k_Tolerance);
	EXPECT_NEAR(circles[0].radius, 5.0f + (2.0f * 0.6f), k_Tolerance);
	// Burning, five units further
	house.hasFire = true;
	EXPECT_NEAR(CirclesOf(house, walker)[0].radius, 5.0f + (2.0f * 0.6f) + 5.0f, k_Tolerance);
}

TEST(CreatureObstacles, ALongBuildingIsARowOfCircles)
{
	const auto walker = AWalker();
	auto hall = AThing(Avoidance::Building, Shape::ModelBox, {14.8f, 7.0f, 3.8f});
	// Half sizes with the margin: 16 by 5, so four circles 8 apart, each 5 x 1.15 wide
	auto circles = CirclesOf(hall, walker);
	ASSERT_EQ(circles.size(), 4u);
	EXPECT_NEAR(circles[0].centre.x, -12.0f, k_Tolerance);
	EXPECT_NEAR(circles[3].centre.x, 12.0f, k_Tolerance);
	EXPECT_NEAR(circles[1].centre.x - circles[0].centre.x, 8.0f, k_Tolerance);
	for (const auto& circle : circles)
	{
		EXPECT_NEAR(circle.centre.y, 0.0f, k_Tolerance);
		EXPECT_NEAR(circle.radius, 5.0f * 1.15f, k_Tolerance);
	}
	// Turned a quarter, the row runs along z
	hall.yAngle = k_Pi / 2.0f;
	circles = CirclesOf(hall, walker);
	EXPECT_NEAR(circles[0].centre.x, 0.0f, k_Tolerance);
	EXPECT_NEAR(std::abs(circles[0].centre.y), 12.0f, k_Tolerance);
	// A very long box is five circles, each a fifth of its length wide
	const auto wall = AThing(Avoidance::Object, Shape::ModelBox, {48.8f, 7.0f, 0.8f});
	circles = CirclesOf(wall, walker);
	ASSERT_EQ(circles.size(), 5u);
	EXPECT_NEAR(circles[0].radius, (50.0f / 5.0f) * 1.15f, k_Tolerance);
	EXPECT_NEAR(circles[4].centre.x - circles[0].centre.x, 80.0f, k_Tolerance);
}

TEST(CreatureObstacles, RocksAreOneCircleAtTheirPlaceAndTreesAThinTrunk)
{
	const auto walker = AWalker();
	// 2 tall: low, so the creature keeps clear by 0.7 - 2 / 12 x 0.3 = 0.65 of its radius past its width
	auto rock = AThing(Avoidance::Object, Shape::AtPlace, {3.0f, 1.0f, 2.0f});
	rock.position = {10.0f, 20.0f};
	rock.boxCentre = {5.0f, 0.0f, 0.0f};
	const auto circles = CirclesOf(rock, walker);
	ASSERT_EQ(circles.size(), 1u);
	EXPECT_NEAR(circles[0].centre.x, 10.0f, k_Tolerance);
	EXPECT_NEAR(circles[0].radius, 3.0f - (2.0f * 0.65f) + 2.0f, k_Tolerance);
	EXPECT_NEAR(ReachRadius(rock, walker), 3.0f - (2.0f * 0.65f), k_Tolerance);

	// A tree is a tenth of its width, a quarter at most, and the creature's whole radius
	auto tree = AThing(Avoidance::Tree, Shape::Trunk, {1.5f, 20.0f, 1.0f});
	EXPECT_NEAR(CirclesOf(tree, walker)[0].radius, 0.15f + 2.0f, k_Tolerance);
	tree.width = 6.0f;
	EXPECT_NEAR(CirclesOf(tree, walker)[0].radius, 0.25f + 2.0f, k_Tolerance);
}

TEST(CreatureObstacles, OtherCreaturesAndSceneryHaveTheirOwnCircles)
{
	const auto walker = AWalker();
	auto other = AThing(Avoidance::Creature, Shape::Creature, {0.0f, 0.0f, 0.0f});
	other.width = 3.0f;
	EXPECT_NEAR(CirclesOf(other, walker)[0].radius, (3.0f + 2.0f) * 1.15f, k_Tolerance);
	// Scenery: seven tenths of its model's half diagonal, whatever its scale or the creature
	auto plinth = AThing(Avoidance::Always, Shape::Scenery, {3.0f, 4.0f, 12.0f});
	plinth.scale = 2.0f;
	EXPECT_NEAR(CirclesOf(plinth, walker)[0].radius, 13.0f * 0.7f, k_Tolerance);
}

TEST(CreatureObstacles, TheTempleIsAWideCircleAndSevenSpokes)
{
	const std::array<bool, 6> none {};
	const glm::vec2 centre {1000.0f, 2000.0f};
	// The good temple's other spokes have four circles, the middling three, the evil two
	EXPECT_EQ(TempleCircles(centre, 0.0f, 0.5f, none).size(), 1u + 12u + (5u * 4u));
	EXPECT_EQ(TempleCircles(centre, 0.0f, -0.5f, none).size(), 1u + 12u + (5u * 3u));
	EXPECT_EQ(TempleCircles(centre, 0.0f, -0.8f, none).size(), 1u + 12u + (5u * 2u));

	const auto circles = TempleCircles(centre, 0.0f, 0.0f, none);
	EXPECT_NEAR(circles[0].radius, 14.86f, k_Tolerance);
	EXPECT_EQ(circles[0].centre, centre);
	// The first spoke starts 15.91 out along 3.83 radians, its circles 4.3 apart, 2.365 wide
	const auto first = circles[1];
	EXPECT_NEAR(first.radius, 2.365f, k_Tolerance);
	EXPECT_NEAR(glm::distance(first.centre, centre), 15.91f, 1e-2f);
	EXPECT_NEAR(std::atan2(first.centre.y - centre.y, first.centre.x - centre.x), 3.83f - (2.0f * k_Pi), 1e-3f);
	EXPECT_NEAR(glm::distance(circles[2].centre, centre), 20.21f, 1e-2f);
	// Its last circle is bent a little round, 32.465 out
	EXPECT_NEAR(glm::distance(circles[6].centre, centre), 32.465f, 1e-2f);
	const auto bentAngle = std::atan2(circles[6].centre.y - centre.y, circles[6].centre.x - centre.x) + (2.0f * k_Pi);
	EXPECT_NEAR(bentAngle, 3.83f + 0.27f, 1e-3f);
	// The second spoke bends the other way
	const auto secondAngle = std::atan2(circles[12].centre.y - centre.y, circles[12].centre.x - centre.x);
	EXPECT_NEAR(std::remainder(secondAngle - (3.83f + 0.8975979f - 0.27f), 2.0f * k_Pi), 0.0f, 1e-3f);

	// A worship site in a place adds a wide circle beyond the spoke after it, half a radian round, 24 out
	std::array<bool, 6> sites {};
	sites[0] = true;
	const auto withSite = TempleCircles(centre, 0.0f, 0.0f, sites);
	ASSERT_EQ(withSite.size(), circles.size() + 1u);
	const auto site = withSite[13];
	EXPECT_NEAR(site.radius, 12.0f, k_Tolerance);
	EXPECT_NEAR(glm::distance(site.centre, centre), 24.0f, k_Tolerance);
	const auto siteAngle = std::atan2(site.centre.y - centre.y, site.centre.x - centre.x);
	EXPECT_NEAR(std::remainder(siteAngle - (3.83f + 0.8975979f + 0.5f), 2.0f * k_Pi), 0.0f, 1e-3f);
}

TEST(CreatureObstacles, ABodyReachesAsFarAsItsFurthestBone)
{
	const std::vector<glm::mat4> bones {glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 50.0f, 4.0f)),
	                                    glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f))};
	EXPECT_NEAR(BodyRadius(bones), 5.0f, k_Tolerance);
}

TEST(CreatureObstacles, TurnsAreReadOffTheModelsAxis)
{
	const auto rotation = glm::mat3(glm::eulerAngleY(-0.6f));
	EXPECT_NEAR(YAngleOf(rotation), 0.6f, k_Tolerance);
}
