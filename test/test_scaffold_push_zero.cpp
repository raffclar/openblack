/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A thing straight to the west or the south of a scaffold that clears its ground has a push with a larger part of 0.
// openblack then divides by the larger size, so the thing flies straight away from the scaffold. A push with two parts
// of 0 still divides by 0. These tests also pin each step of what the game does with a speed that is not a number: the
// start of the body, the steps of the body and the place of the thing. Each value is the same on each target.

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Common/X87.h"
#include "ECS/WorkshopRules.h"
#include "Physics/Body.h"
#include "Physics/Ground.h"
#include "Physics/TurnRules.h"

using namespace openblack;
using namespace openblack::physics;

namespace
{
constexpr float k_Infinity = std::numeric_limits<float>::infinity();

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// Open sea everywhere: the land is at the level of the sea
class FakeSea final: public Ground
{
public:
	[[nodiscard]] float HeightAt(glm::vec2) const override { return 0.0f; }
	[[nodiscard]] glm::vec3 NormalAt(glm::vec2) const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] bool IsSeaCell(glm::vec2) const override { return true; }
};

constexpr Material k_Wood {
    .density = 0.6f, .springK = 15.0f, .dampK = 1.0f, .friction = 0.8f, .spinKeptPerSecond = 0.6f, .drag = 0.01f};

Shape Cube(float half)
{
	Shape shape;
	for (int i = 0; i < 8; ++i)
	{
		const float x = ((i & 1) ^ ((i >> 1) & 1)) != 0 ? half : -half;
		const float y = (i & 2) != 0 ? half : -half;
		const float z = (i & 4) != 0 ? half : -half;
		shape.points.emplace_back(x, y, z);
	}
	shape.faces = {{0, 3, 2}, {0, 2, 1}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
	               {3, 7, 6}, {3, 6, 2}, {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5}};
	shape.radius = glm::length(glm::vec3(half));
	return shape;
}

StepResult Step(Body& body, const Ground& ground)
{
	body.ClearForces();
	body.TouchGround(ground);
	body.ApplyContacts();
	return body.Integrate();
}
} // namespace

TEST(ScaffoldPushZero, TheFpuDivideGivesItsNaNAndSignedInfinities)
{
	EXPECT_EQ(Bits(x87::DivideAsFpu(0.0f, 0.0f)), x87::k_IndefiniteBits);
	EXPECT_EQ(x87::DivideAsFpu(-2.0f, 0.0f), -k_Infinity);
	EXPECT_EQ(x87::DivideAsFpu(2.0f, 0.0f), k_Infinity);
	EXPECT_EQ(x87::DivideAsFpu(2.0f, -0.0f), -k_Infinity);
	EXPECT_EQ(x87::DivideAsFpu(3.0f, 2.0f), 1.5f);
	EXPECT_EQ(Bits(x87::DivideAsFpu(x87::Indefinite(), 0.0f)), x87::k_IndefiniteBits);
}

TEST(ScaffoldPushZero, TheFpuComparesPassWithANaN)
{
	const float nan = x87::Indefinite();
	EXPECT_TRUE(x87::LessOrUnordered(nan, 1.0f));
	EXPECT_TRUE(x87::LessOrUnordered(1.0f, nan));
	EXPECT_TRUE(x87::LessOrUnordered(1.0f, 2.0f));
	EXPECT_FALSE(x87::LessOrUnordered(2.0f, 2.0f));
	EXPECT_TRUE(x87::EqualOrUnordered(nan, 0.0f));
	EXPECT_TRUE(x87::EqualOrUnordered(0.0f, -0.0f));
	EXPECT_FALSE(x87::EqualOrUnordered(0.2f, 0.0f));
	EXPECT_EQ(Bits(x87::CanonicalNaN(std::numeric_limits<float>::quiet_NaN())), x87::k_IndefiniteBits);
	EXPECT_EQ(x87::CanonicalNaN(4.0f), 4.0f);
}

TEST(ScaffoldPushZero, AThingStraightWestFliesStraightWest)
{
	const auto push = ecs::workshop::PushOutOfWay({10.0f, 20.0f}, {9.0f, 20.0f});
	EXPECT_EQ(push.offset, glm::vec2(-2.0f, 0.0f));
	EXPECT_EQ(push.velocity, glm::vec3(-2.0f, 0.2f, 0.0f));
	EXPECT_EQ(Bits(push.velocity.z), 0u);
}

TEST(ScaffoldPushZero, AThingStraightSouthFliesStraightSouth)
{
	const auto push = ecs::workshop::PushOutOfWay({10.0f, 20.0f}, {10.0f, 19.0f});
	EXPECT_EQ(push.offset, glm::vec2(0.0f, -2.0f));
	EXPECT_EQ(push.velocity, glm::vec3(0.0f, 0.2f, -2.0f));
	EXPECT_EQ(Bits(push.velocity.x), 0u);
}

TEST(ScaffoldPushZero, APushWithALargerPartThatIsNotZeroKeepsTheSignedDivide)
{
	// The thing straight east and the thing straight north keep the push of the game
	EXPECT_EQ(ecs::workshop::PushOutOfWay({10.0f, 20.0f}, {11.0f, 20.0f}).velocity, glm::vec3(2.0f, 0.2f, 0.0f));
	EXPECT_EQ(ecs::workshop::PushOutOfWay({10.0f, 20.0f}, {10.0f, 21.0f}).velocity, glm::vec3(0.0f, 0.2f, 2.0f));
	// A thing at low x and low z still flies back over the scaffold
	EXPECT_EQ(ecs::workshop::PushOutOfWay({0.0f, 0.0f}, {-1.0f, -2.0f}).velocity, glm::vec3(2.0f, 0.2f, 4.0f));
}

TEST(ScaffoldPushZero, AThingAtTheMiddleOfTheScaffoldGetsTwoNaNSpeeds)
{
	const auto push = ecs::workshop::PushOutOfWay({10.0f, 20.0f}, {10.0f, 20.0f});
	EXPECT_EQ(Bits(push.velocity.x), x87::k_IndefiniteBits);
	EXPECT_EQ(Bits(push.velocity.z), x87::k_IndefiniteBits);
}

TEST(ScaffoldPushZero, TheStartOfTheBodyMakesEachPartOfANaNSpeedANaN)
{
	// The compare with the most speed passes with a NaN, and the scale by the length gives a NaN in each part
	const auto west = StartVelocity({-k_Infinity, 0.2f, x87::Indefinite()});
	EXPECT_EQ(Bits(west.x), x87::k_IndefiniteBits);
	EXPECT_EQ(Bits(west.y), x87::k_IndefiniteBits);
	EXPECT_EQ(Bits(west.z), x87::k_IndefiniteBits);
	const auto south = StartVelocity({x87::Indefinite(), 0.2f, -k_Infinity});
	EXPECT_EQ(Bits(south.y), x87::k_IndefiniteBits);
	// Parts that are only 0 or a NaN stay as they are
	const auto still = StartVelocity({x87::Indefinite(), 0.0f, x87::Indefinite()});
	EXPECT_EQ(still.y, 0.0f);
	// A normal speed above the most speed scales down to it, and a slow speed stays
	EXPECT_EQ(StartVelocity({0.0f, 0.0f, 200.0f}), glm::vec3(0.0f, 0.0f, k_MaxSpeed));
	EXPECT_EQ(StartVelocity({1.0f, 2.0f, 3.0f}), glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(ScaffoldPushZero, ABodyWithANaNSpeedFliesForEverAndCountsAsInTheSea)
{
	const FakeSea sea;
	Body bush({.scale = 1.0f, .halfHeight = 0.5f, .mass = 50.0f, .material = k_Wood}, Cube(0.5f));
	bush.SetUpPose({.axes = glm::mat3(1.0f), .origin = {100.0f, 5.0f, 200.0f}});
	bush.velocity = StartVelocity({-k_Infinity, 0.2f, x87::Indefinite()});
	// The body never stops and the game never removes it under the sea: each step moves it
	for (int i = 0; i < 20 * 100; ++i)
	{
		ASSERT_EQ(Step(bush, sea), StepResult::Moved) << "step " << i;
	}
	EXPECT_TRUE(std::isnan(bush.Centre().x));
	EXPECT_TRUE(std::isnan(bush.Centre().y));
	EXPECT_TRUE(std::isnan(bush.Centre().z));
	// With its place not a number, each point counts as under the water
	EXPECT_TRUE(bush.inWater);
	EXPECT_TRUE(bush.touched);
	EXPECT_EQ(bush.Contacts(), 0);
}

TEST(ScaffoldPushZero, AThingWithABodyPlaceThatIsNotANumberGoesToTheCornerOfTheMap)
{
	const auto place = PlaceOfThing({x87::Indefinite(), std::numeric_limits<float>::quiet_NaN(), -k_Infinity});
	EXPECT_EQ(place.x, 0.0f);
	EXPECT_EQ(place.z, 0.0f);
	EXPECT_EQ(Bits(place.y), x87::k_IndefiniteBits);
	EXPECT_EQ(PlaceOfThing({1.5f, 2.5f, 3.5f}), glm::vec3(1.5f, 2.5f, 3.5f));
}

TEST(ScaffoldPushZero, ABodyThatIsNotANumberWakesOnlyTheCornerCell)
{
	const auto cells = turn::WakeCells(glm::vec3(x87::Indefinite()), glm::vec3(x87::Indefinite()), 1.0f);
	EXPECT_EQ(cells.low, glm::ivec2(0, 0));
	EXPECT_EQ(cells.high, glm::ivec2(0, 0));
	// A place known with a speed that is not a number also gives the corner cell
	const auto known = turn::WakeCells(glm::vec3(500.0f, 0.0f, 500.0f), glm::vec3(x87::Indefinite()), 1.0f);
	EXPECT_EQ(known.low, glm::ivec2(0, 0));
	EXPECT_EQ(known.high, glm::ivec2(0, 0));
}

TEST(ScaffoldPushZero, TheCameraAndTheBobComparesPassWithANaN)
{
	const float nan = x87::Indefinite();
	// The distance after the step passes, but a NaN speed does not
	EXPECT_FALSE(turn::PassesCamera(glm::vec3(11.0f, 0, 0), glm::vec3(nan), glm::vec3(0.0f), glm::vec3(nan)));
	EXPECT_TRUE(turn::PassesCamera(glm::vec3(11.0f, 0, 0), glm::vec3(nan), glm::vec3(0.0f), glm::vec3(-21.0f, 0, 0)));
	EXPECT_TRUE(turn::Bobbed(0.5f, nan));
}
