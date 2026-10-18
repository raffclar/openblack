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
#include <deque>
#include <functional>
#include <numbers>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlockRules.h"
#include "ECS/ScriptFlocks.h"

using namespace openblack;
using namespace openblack::ecs;
using map_coords::MapCoords;
namespace rules = openblack::ecs::script_flock_rules;

namespace
{
/// Draws taken from a list, in order
struct Draws
{
	std::deque<float> values;
	float operator()(float /*limit*/)
	{
		const auto value = values.front();
		values.pop_front();
		return value;
	}
};

MapCoords Metres(float x, float z)
{
	return map_coords::FromMetres({x, z});
}

entt::entity Living(Registry& registry, float x, float z)
{
	const auto living = registry.Create();
	registry.Assign<components::Transform>(living).position = glm::vec3(x, 0.0f, z);
	registry.Assign<components::WallHug>(living);
	return living;
}
} // namespace

TEST(ScriptFlockRules, ANewcomerGoesBeforeTheFirstWhosePlaceIsNotBelowItsOwn)
{
	const std::array<uint8_t, 3> orders {0, 5, 6};
	EXPECT_EQ(rules::InsertAt(orders, 0), 0u);
	EXPECT_EQ(rules::InsertAt(orders, 3), 1u);
	EXPECT_EQ(rules::InsertAt(orders, 7), 3u);
	EXPECT_EQ(rules::LeaderOrder(std::nullopt), 5);
	EXPECT_EQ(rules::LeaderOrder(6), 7);
	// The place wraps round past 255
	EXPECT_EQ(rules::LeaderOrder(255), 0);
}

TEST(ScriptFlockRules, ARandomPointDrawsTheAngleThenTheDistance)
{
	// A quarter turn and 10 metres: straight along +z
	Draws draws {{std::numbers::pi_v<float> / 2.0f, 6.0f}};
	const auto point =
	    rules::CalcRandomPos(Metres(100, 200), 4.0f, 20.0f, std::ref(draws), [](const MapCoords&) { return true; });
	EXPECT_NEAR(map_coords::ToMetres(point.x), 100.0f, 0.001f);
	EXPECT_NEAR(map_coords::ToMetres(point.z), 210.0f, 0.001f);
	EXPECT_TRUE(draws.values.empty());
}

TEST(ScriptFlockRules, ARandomPointHasTheGameBits)
{
	// Each step rounds to a float. The values come from the game's instructions with exact rounding at each step
	Draws draws {{2.945753574371338f, 3.9451653957366943f}};
	const MapCoords centre {.x = 18245586, .z = 3421693, .altitude = 0.0f};
	const auto point = rules::CalcRandomPos(centre, 4.0f, 20.0f, std::ref(draws), [](const MapCoords&) { return true; });
	EXPECT_EQ(point.x, 18194512);
	EXPECT_EQ(point.z, 3431824);
}

TEST(ScriptFlockRules, APointThatWillNotDoSpiralsThenFallsBackToTheCentre)
{
	// Only the cell one to -x of the drawn one will do: the spiral's first step
	const auto centre = Metres(105, 105);
	Draws draws {{0.0f, 0.0f}};
	const auto found = rules::CalcRandomPos(centre, 0.0f, 0.0f, std::ref(draws), [](const MapCoords& coords) {
		return map_coords::CellX(coords) == 9 && map_coords::CellZ(coords) == 10;
	});
	EXPECT_EQ(map_coords::CellX(found), 9);
	EXPECT_EQ(map_coords::CellZ(found), 10);
	// Nothing at all: two draws of 25 cells each, then the centre
	int asked = 0;
	Draws twice {{0.0f, 0.0f, 0.0f, 0.0f}};
	const auto fallback = rules::CalcRandomPos(centre, 0.0f, 0.0f, std::ref(twice), [&asked](const MapCoords&) {
		++asked;
		return false;
	});
	EXPECT_EQ(asked, 50);
	EXPECT_EQ(fallback, centre);
}

TEST(ScriptFlockRules, MembersWithinTheDomainStayPut)
{
	const rules::FlockView flock {.place = Metres(0, 0), .domainRadius = 10, .flockDistance = 5, .leader = Metres(0, 0)};
	Draws none;
	const auto step = rules::MoveInFlock(Metres(6, 8), flock, std::ref(none), [](const MapCoords&) { return true; });
	EXPECT_EQ(step.result, 1u);
	EXPECT_FALSE(step.goal.has_value());
}

TEST(ScriptFlockRules, TheLeaderOutsideTheDomainWalksBackIntoIt)
{
	const rules::FlockView flock {.place = Metres(0, 0), .domainRadius = 10, .isLeader = true};
	Draws draws {{0.0f, 4.0f}};
	const auto step = rules::MoveInFlock(Metres(20, 0), flock, std::ref(draws), [](const MapCoords&) { return true; });
	EXPECT_EQ(step.result, 0x23u);
	ASSERT_TRUE(step.goal.has_value());
	EXPECT_NEAR(map_coords::ToMetres(step.goal->x), 4.0f, 0.001f);
}

TEST(ScriptFlockRules, AFollowerKeepsWithinTheFlockDistanceOfTheLeader)
{
	const rules::FlockView flock {.place = Metres(0, 0), .domainRadius = 10, .flockDistance = 5, .leader = Metres(8, 0)};
	const auto any = [](const MapCoords&) { return true; };
	// Near enough the leader: it waits
	Draws none;
	EXPECT_EQ(rules::MoveInFlock(Metres(12, 0), flock, std::ref(none), any).result, 0u);
	// Too far: a point near the leader inside the domain
	Draws inside {{std::numbers::pi_v<float>, 2.0f}};
	const auto step = rules::MoveInFlock(Metres(20, 0), flock, std::ref(inside), any);
	EXPECT_EQ(step.result, 0x23u);
	ASSERT_TRUE(step.goal.has_value());
	EXPECT_NEAR(map_coords::ToMetres(step.goal->x), 6.0f, 0.001f);
	// A point outside the domain while the leader is inside it: it waits for another turn
	Draws outside {{0.0f, 4.0f}};
	EXPECT_EQ(rules::MoveInFlock(Metres(20, 0), flock, std::ref(outside), any).result, 0u);
}

TEST(ScriptFlockRules, TheLivingsOwnPickChoosesThePoint)
{
	struct Asked
	{
		MapCoords centre;
		float least;
		float most;
	};
	std::vector<Asked> asked;
	const auto pick = [&asked](const MapCoords& centre, float least, float most) {
		asked.push_back({.centre = centre, .least = least, .most = most});
		return centre;
	};
	// The leader picks a point within the domain about the flock's own place
	const rules::FlockView leader {.place = Metres(0, 0), .domainRadius = 12, .flockDistance = 5, .isLeader = true};
	const auto back = rules::MoveInFlock(Metres(30, 0), leader, pick);
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_EQ(asked[0].centre, Metres(0, 0));
	EXPECT_EQ(asked[0].least, 0.0f);
	EXPECT_EQ(asked[0].most, 12.0f);
	EXPECT_EQ(back.result, 0x23u);
	EXPECT_EQ(back.goal, Metres(0, 0));
	// A member picks a point within the flock distance of the leader, a villager who leads the sheep
	const rules::FlockView member {.place = Metres(0, 0), .domainRadius = 12, .flockDistance = 5, .leader = Metres(3, 0)};
	const auto follow = rules::MoveInFlock(Metres(30, 0), member, pick);
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[1].centre, Metres(3, 0));
	EXPECT_EQ(asked[1].most, 5.0f);
	EXPECT_EQ(follow.result, 0x23u);
	EXPECT_EQ(follow.goal, Metres(3, 0));
	// Within the domain, it picks nothing
	EXPECT_EQ(rules::MoveInFlock(Metres(2, 0), member, pick).result, 1u);
	EXPECT_EQ(asked.size(), 2u);
}

TEST(ScriptFlocks, NewcomersGoToTheFrontAndTheLeaderToTheBack)
{
	Registry registry;
	const auto flock = script_flocks::Create(registry, Metres(50, 50));
	const auto first = Living(registry, 1, 1);
	const auto second = Living(registry, 2, 2);
	const auto leader = Living(registry, 3, 3);
	EXPECT_TRUE(script_flocks::AddLiving(registry, flock, first));
	EXPECT_TRUE(script_flocks::AddLiving(registry, flock, second));
	// The first in stays at the back, leading
	EXPECT_EQ(script_flocks::Leader(registry, flock), first);
	script_flocks::AddLeader(registry, flock, leader);
	EXPECT_EQ(script_flocks::Leader(registry, flock), leader);
	EXPECT_EQ(registry.Get<components::Flock>(flock).members, (std::vector {second, first, leader}));
	// Where the flock is: its leader's position
	EXPECT_EQ(script_flocks::Position(registry, flock), Metres(3, 3));
}

TEST(ScriptFlocks, AFlockLeftEmptyGoesOnlyWhenAskedTo)
{
	Registry registry;
	const auto kept = script_flocks::Create(registry, Metres(0, 0));
	const auto living = Living(registry, 1, 1);
	script_flocks::AddLiving(registry, kept, living);
	script_flocks::Remove(registry, living, false);
	EXPECT_TRUE(script_flocks::IsFlock(registry, kept));
	EXPECT_EQ(script_flocks::FlockOf(registry, living), static_cast<entt::entity>(entt::null));
	// Joining another flock takes it out of its old one, which goes once empty
	script_flocks::AddLiving(registry, kept, living);
	const auto other = script_flocks::Create(registry, Metres(0, 0));
	EXPECT_TRUE(script_flocks::AddLiving(registry, other, living));
	EXPECT_FALSE(script_flocks::IsFlock(registry, kept));
	EXPECT_EQ(script_flocks::FlockOf(registry, living), other);
}

TEST(ScriptFlocks, MovingAFlockMovesItsPlaceAndItsLeadersGoal)
{
	Registry registry;
	const auto flock = script_flocks::Create(registry, Metres(0, 0));
	const auto leader = Living(registry, 1, 1);
	script_flocks::AddLiving(registry, flock, leader);
	script_flocks::MoveTo(registry, flock, Metres(30, 40));
	EXPECT_EQ(registry.Get<components::Flock>(flock).place, Metres(30, 40));
	EXPECT_NEAR(registry.Get<components::WallHug>(leader).goal.x, 30.0f, 0.001f);
}

TEST(ScriptFlocks, ARandomMemberPassesOverTheOneLeftOut)
{
	Registry registry;
	const auto flock = script_flocks::Create(registry, Metres(0, 0));
	const auto a = Living(registry, 1, 1);
	const auto b = Living(registry, 2, 2);
	script_flocks::AddLiving(registry, flock, a);
	script_flocks::AddLiving(registry, flock, b);
	// Line: b, a. Leaving a out, one draw from 1
	uint32_t asked = 0;
	const auto picked = script_flocks::RandomMember(registry, flock, a, [&asked](uint32_t n) {
		asked = n;
		return 0u;
	});
	EXPECT_EQ(asked, 1u);
	EXPECT_EQ(picked, b);
}
