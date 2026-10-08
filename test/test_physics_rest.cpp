/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How bodies come to rest and what a body at rest costs: representative bodies (a rock on flat land and on a slope, a
// villager, a fallen tree and a piece of a building) are run until they stop, and then held at rest as the physics holds
// them, to check that they stop by the game's rules, stay where they stopped and do no more work.

#include <cmath>

#include <array>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Physics/Body.h"
#include "Physics/BodyShapes.h"
#include "Physics/Ground.h"
#include "Physics/Materials.h"
#include "Physics/TurnRules.h"

using namespace openblack;
using namespace openblack::physics;

namespace
{
/// Flat or sloped land, never sea
class FakeLand final: public Ground
{
public:
	explicit FakeLand(float slopeX = 0.0f)
	    : _slope(slopeX)
	{
	}
	[[nodiscard]] float HeightAt(glm::vec2 xz) const override { return _slope * xz.x; }
	[[nodiscard]] glm::vec3 NormalAt(glm::vec2) const override { return glm::normalize(glm::vec3(-_slope, 1.0f, 0.0f)); }
	[[nodiscard]] bool IsSeaCell(glm::vec2) const override { return false; }

private:
	float _slope;
};

// Made-up materials, of the sort the game's table gives each kind
constexpr Material k_Stone {
    .density = 2.0f, .springK = 20.0f, .dampK = 1.0f, .friction = 1.0f, .spinKeptPerSecond = 0.7f, .drag = 0.0f};
constexpr Material k_Flesh {
    .density = 1.0f, .springK = 10.0f, .dampK = 1.0f, .friction = 1.0f, .spinKeptPerSecond = 0.5f, .drag = 0.01f};
constexpr Material k_Wood {
    .density = 0.6f, .springK = 15.0f, .dampK = 1.0f, .friction = 0.8f, .spinKeptPerSecond = 0.6f, .drag = 0.01f};

/// Twenty steps make a game turn, and a body is given at most this many turns to come to rest
constexpr int k_MostSteps = 20 * 600;

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

/// One step of a lone body, in the order the physics steps every body
StepResult Step(Body& body, const Ground& ground)
{
	body.ClearForces();
	body.TouchGround(ground);
	body.ApplyContacts();
	return body.Integrate();
}

/// What running a body until it stops came to
struct Settled
{
	/// The steps it took, none if it never stopped
	int steps {-1};
	/// Its rest counter and contacts on the step it stopped
	int counter {0};
	int contacts {0};
};

/// Steps a thrown body until it comes to rest, then holds it at rest as the physics does when a body stops
Settled RunToRest(Body& body, const Ground& ground)
{
	Settled settled;
	for (int i = 1; i <= k_MostSteps; ++i)
	{
		if (Step(body, ground) == StepResult::Stopped)
		{
			settled = {.steps = i, .counter = body.RestCounter(), .contacts = body.Contacts()};
			body.resting = true;
			return settled;
		}
	}
	return settled;
}

/// The steps a body must spend in the physics before it may stop: its rest counter starts at minus a thousand times its
/// scaled half height and rises by five a step
int StepsBeforeItMayRest(const Body& body)
{
	return -body.RestCounter() / 5;
}

/// Checks the rules a stopping body met, and that once at rest it does no more work and stays put
void ExpectRestsAndCostsNothing(Body& body, const Ground& ground)
{
	const int earliest = StepsBeforeItMayRest(body);
	const auto settled = RunToRest(body, ground);
	ASSERT_GT(settled.steps, 0) << "it never came to rest";
	// It stops only once its counter has risen above nothing, and only while it touches something
	EXPECT_GE(settled.steps, earliest);
	EXPECT_GT(settled.counter, 0);
	EXPECT_GT(settled.contacts, 0);
	// Stopping takes all its motion away
	EXPECT_EQ(body.velocity, glm::vec3(0.0f));
	EXPECT_EQ(body.angularMomentum, glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(body.Speed(), 0.0f);

	// At rest and untouched it isn't moved, nor are its points worked out again, however long it lies there
	const auto centre = body.Centre();
	const auto axes = body.Axes();
	std::vector<glm::vec3> points;
	for (const auto& point : body.Points())
	{
		points.push_back(point.world);
	}
	for (int i = 0; i < 20 * 50; ++i)
	{
		ASSERT_EQ(Step(body, ground), StepResult::None);
	}
	EXPECT_EQ(body.Centre(), centre);
	EXPECT_EQ(body.Axes(), axes);
	for (size_t i = 0; i < points.size(); ++i)
	{
		EXPECT_EQ(body.Points()[i].world, points[i]);
	}
	EXPECT_FALSE(body.touched);
}

/// A body thrown sideways and up from just above the land
void Throw(Body& body)
{
	body.velocity = {6.0f, 3.0f, 1.0f};
	body.SetAngularVelocity({1.0f, 0.5f, 2.0f});
}
} // namespace

TEST(PhysicsRest, ARockOnFlatLand)
{
	const FakeLand land;
	Body rock({.scale = 1.0f, .halfHeight = 0.5f, .mass = 200.0f, .material = k_Stone}, Cube(0.5f));
	rock.SetUpPose({.axes = glm::mat3(1.0f), .origin = {0.0f, 1.0f, 0.0f}});
	Throw(rock);
	ExpectRestsAndCostsNothing(rock, land);
	// It lies on the land, sunk in no deeper than its springs hold it
	EXPECT_NEAR(rock.Centre().y, 0.5f, 0.2f);
}

TEST(PhysicsRest, ARockOnASlope)
{
	const FakeLand slope(0.3f);
	Body rock({.scale = 1.0f, .halfHeight = 0.5f, .mass = 200.0f, .material = k_Stone}, Cube(0.5f));
	rock.SetUpPose({.axes = glm::mat3(1.0f), .origin = {0.0f, 1.0f, 0.0f}});
	Throw(rock);
	ExpectRestsAndCostsNothing(rock, slope);
	// It rests on the slope, not under or far above it
	const float ground = slope.HeightAt({rock.Centre().x, rock.Centre().z});
	EXPECT_GT(rock.Centre().y, ground);
	EXPECT_LT(rock.Centre().y, ground + 1.0f);
}

TEST(PhysicsRest, AVillager)
{
	const FakeLand land;
	Body villager({.scale = 1.0f, .halfHeight = 0.9f, .mass = 70.0f, .material = k_Flesh},
	              shapes::LivingBody(shapes::Living::Villager, 1.8f, 0.4f, 1.0f));
	villager.SetUpPose({.axes = glm::mat3(1.0f), .origin = {0.0f, 0.5f, 0.0f}});
	Throw(villager);
	ExpectRestsAndCostsNothing(villager, land);
}

TEST(PhysicsRest, AFallenTree)
{
	const FakeLand land;
	Body tree({.scale = 1.0f, .halfHeight = 4.0f, .mass = 500.0f, .material = k_Wood}, shapes::Tree(8.0f, 0.6f, 1.0f, false));
	// Lying on its side
	const glm::mat3 lying(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	tree.SetUpPose({.axes = lying, .origin = {0.0f, 1.0f, 0.0f}});
	Throw(tree);
	ExpectRestsAndCostsNothing(tree, land);
}

TEST(PhysicsRest, APieceOfABuilding)
{
	const FakeLand land;
	const std::array<std::array<glm::vec3, 3>, 2> square {{
	    {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f)},
	    {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(-1.0f, 1.0f, 0.0f)},
	}};
	const auto piece = shapes::Fragment(square);
	ASSERT_FALSE(piece.tooThin);
	Body body({.scale = 1.0f, .halfHeight = 0.0f, .mass = piece.mass, .material = k_Stone}, piece.shape);
	body.SetPoseDirect(glm::mat3(1.0f), {0.0f, 2.0f, 0.0f});
	Throw(body);
	ExpectRestsAndCostsNothing(body, land);
}

TEST(PhysicsRest, AtTheTurnsStartOnlyMovingBodiesAndThoseThatAlwaysStayAreAwake)
{
	// A resting body leaves the physics at the next turn's start unless something moving wakes it, or its kind always
	// stays (the physical shield)
	EXPECT_TRUE(turn::AwakeAtTurnStart(false, false));
	EXPECT_TRUE(turn::AwakeAtTurnStart(false, true));
	EXPECT_FALSE(turn::AwakeAtTurnStart(true, false));
	EXPECT_TRUE(turn::AwakeAtTurnStart(true, true));
}

TEST(PhysicsRest, ARestingBodyHoldsAgainstPushesBelowItsThreshold)
{
	const FakeLand land;
	Body rock({.scale = 1.0f, .halfHeight = 0.5f, .mass = 200.0f, .material = k_Stone}, Cube(0.5f));
	rock.SetUpPose({.axes = glm::mat3(1.0f), .origin = {0.0f, 0.5f, 0.0f}});
	ASSERT_GT(RunToRest(rock, land).steps, 0);
	// Something touching it sets it moving this fast: slower than a resting body's threshold, the push is
	// soaked up where it lies; faster, it is knocked loose
	rock.velocity = {3.0f, 0.0f, 0.0f};
	rock.touched = true;
	EXPECT_EQ(rock.Integrate(), StepResult::None);
	EXPECT_EQ(rock.velocity, glm::vec3(0.0f));
	rock.velocity = {4.5f, 0.0f, 0.0f};
	rock.touched = true;
	EXPECT_EQ(rock.Integrate(), StepResult::Knocked);
}
