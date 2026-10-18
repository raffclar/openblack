/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureRouteSearch.h"
#include "ECS/FootpathPlanning.h"

using namespace openblack;
using namespace openblack::ecs;
namespace planning = openblack::ecs::footpath_planning;
namespace route = openblack::creature_route;
namespace obstacles = openblack::creature_obstacles;

namespace
{

obstacles::Thing ThingOfKind(obstacles::Avoidance avoidance, obstacles::Shape shape)
{
	return obstacles::Thing {.avoidance = avoidance,
	                         .shape = shape,
	                         .position = {100.0f, 100.0f},
	                         .yAngle = 0.0f,
	                         .scale = 1.0f,
	                         .boxCentre = {0.0f, 0.0f, 0.0f},
	                         .boxHalfSize = {2.0f, 3.0f, 2.0f},
	                         .height = 6.0f,
	                         .width = 2.0f,
	                         .burning = false,
	                         .hasFire = true,
	                         .justBegun = true,
	                         .walking = false,
	                         .playerAlignment = 0.0f,
	                         .worshipSites = {true, true, true, true, true, true},
	                         .owner = PlayerNames::PLAYER_ONE};
}

/// A planned path from one point to another round the circles given, as the game lays one at once
std::optional<std::vector<glm::ivec2>> Plan(glm::vec2 start, glm::vec2 goal, const std::vector<route::Circle>& circles)
{
	route::Obstacles held;
	held.ResetWithoutWalker(0.0f, [&circles](glm::ivec2 cell, route::Obstacles& into) {
		if (cell != glm::ivec2(10, 10))
		{
			return;
		}
		for (uint32_t i = 0; i < circles.size(); ++i)
		{
			into.Add(i, circles[i].centre, circles[i].radius);
		}
	});
	held.LeaveOutCirclesHolding(goal, start);
	route::Search search(held, {.point = start}, {.point = goal, .mostCost = planning::MostCost(start, goal)});
	for (int32_t i = 0; i < planning::k_MostStepsAtOnce && search.GetStatus() == route::Search::Status::Searching; ++i)
	{
		search.Step(held);
	}
	if (search.GetStatus() != route::Search::Status::Found)
	{
		return std::nullopt;
	}
	return planning::RoutePlaces(search.GetBest(), held.All(), start, goal);
}

} // namespace

TEST(FootpathPlanning, AStraightRouteGivesItsTwoEnds)
{
	const auto places = Plan({100.0f, 100.0f}, {150.0f, 120.0f}, {});
	ASSERT_TRUE(places.has_value());
	ASSERT_EQ(places->size(), 2u);
	EXPECT_EQ(places->front(), planning::ToPlace({100.0f, 100.0f}));
	EXPECT_EQ(places->back(), planning::ToPlace({150.0f, 120.0f}));
}

TEST(FootpathPlanning, ARouteGoesRoundACircleInTheWay)
{
	const route::Circle rock {.centre = {125.0f, 101.0f}, .radius = 6.0f};
	const auto places = Plan({100.0f, 100.0f}, {150.0f, 100.0f}, {rock});
	ASSERT_TRUE(places.has_value());
	// Nodes stepped round the circle, none of them inside it
	EXPECT_GT(places->size(), 4u);
	for (const auto& place : *places)
	{
		EXPECT_GE(glm::distance(planning::ToPoint(place), rock.centre), rock.radius - 0.01f);
	}
}

TEST(FootpathPlanning, ACircleHoldingAnEndIsNotGoneRound)
{
	// The building at the start holds the start: the path goes straight out of it
	const route::Circle building {.centre = {101.0f, 100.0f}, .radius = 5.0f};
	const auto places = Plan({100.0f, 100.0f}, {150.0f, 100.0f}, {building});
	ASSERT_TRUE(places.has_value());
	EXPECT_EQ(places->size(), 2u);
}

TEST(FootpathPlanning, NoCircleIsLeftOutForAWalkerThatIsNotThere)
{
	// A creature's walk leaves out a circle about where it stands; a route no one walks keeps it whole
	route::Obstacles walked;
	walked.Reset({100.0f, 100.0f}, 0.0f);
	EXPECT_FALSE(walked.Add(1u, {100.05f, 100.0f}, 3.0f));
	route::Obstacles unwalked;
	unwalked.ResetWithoutWalker(0.0f);
	EXPECT_TRUE(unwalked.Add(1u, {100.05f, 100.0f}, 3.0f));
	EXPECT_FLOAT_EQ(unwalked.All().back().circle.radius, 3.0f);
}

TEST(FootpathPlanning, LandWaterIsGoneRoundOnlyWhenAsked)
{
	std::vector<route::Ground> cells(static_cast<size_t>(route::k_CellsPerSide * route::k_CellsPerSide), route::Ground::Open);
	cells.at(static_cast<size_t>(20 * route::k_CellsPerSide + 20)) = route::Ground::Water;
	const auto land = route::WalkableLand::OfCells(cells);
	route::Obstacles creature;
	creature.ResetWithoutWalker(0.0f);
	route::AddLandCircles(land, {20, 20}, creature);
	EXPECT_TRUE(creature.All().empty());
	route::Obstacles footpath;
	footpath.ResetWithoutWalker(0.0f);
	route::AddLandCircles(land, {20, 20}, footpath, true);
	ASSERT_EQ(footpath.All().size(), 1u);
	EXPECT_EQ(footpath.All().front().circle.centre, glm::vec2(205.0f, 205.0f));
}

TEST(FootpathPlanning, WhatAPathGoesRound)
{
	using obstacles::Avoidance;
	using obstacles::Shape;
	for (const auto avoidance :
	     {Avoidance::Always, Avoidance::Building, Avoidance::Object, Avoidance::Tree, Avoidance::VortexOutOfTheLand})
	{
		EXPECT_TRUE(obstacles::MustAvoidWithoutWalker(ThingOfKind(avoidance, Shape::AtPlace)));
	}
	for (const auto avoidance : {Avoidance::Never, Avoidance::Burning, Avoidance::BurningUnlessScripted, Avoidance::Creature,
	                             Avoidance::Shield, Avoidance::Forest})
	{
		EXPECT_FALSE(obstacles::MustAvoidWithoutWalker(ThingOfKind(avoidance, Shape::AtPlace)));
	}
	auto forest = ThingOfKind(Avoidance::Forest, Shape::ModelBox);
	forest.burning = true;
	EXPECT_TRUE(obstacles::MustAvoidWithoutWalker(forest));
}

TEST(FootpathPlanning, ItsCirclesTakeTheWholeMarginAndNoFire)
{
	using obstacles::Avoidance;
	using obstacles::Shape;
	const auto atPlace = obstacles::CirclesWithoutWalker(ThingOfKind(Avoidance::Object, Shape::AtPlace), 0.5f);
	ASSERT_EQ(atPlace.size(), 1u);
	EXPECT_FLOAT_EQ(atPlace.front().radius, 2.5f);
	const auto trunk = obstacles::CirclesWithoutWalker(ThingOfKind(Avoidance::Tree, Shape::Trunk), 0.0f);
	ASSERT_EQ(trunk.size(), 1u);
	EXPECT_FLOAT_EQ(trunk.front().radius, 0.2f);
	const auto box = obstacles::CirclesWithoutWalker(ThingOfKind(Avoidance::Building, Shape::ModelBox), 0.5f);
	ASSERT_EQ(box.size(), 1u);
	EXPECT_FLOAT_EQ(box.front().radius, 2.5f);
	// No circles over the temple's worship sites: the middle and the spokes only
	auto temple = ThingOfKind(Avoidance::Building, Shape::Temple);
	const auto templeCircles = obstacles::CirclesWithoutWalker(temple, 0.0f);
	const auto withSites = obstacles::TempleCircles(temple.position, temple.yAngle, 0.0f, temple.worshipSites);
	EXPECT_EQ(templeCircles.size() + 6, withSites.size());
	EXPECT_TRUE(obstacles::CirclesWithoutWalker(ThingOfKind(Avoidance::Creature, Shape::Creature), 0.0f).empty());
}

TEST(FootpathPlanning, LaidNodesReplaceThoseBetweenAndKeepTheHiddenOnes)
{
	const std::vector<planning::PlanNode> nodes {
	    {.place = {0, 0}}, {.place = {1, 0}, .planned = true}, {.place = {2, 0}, .hidden = true},
	    {.place = {3, 0}}, {.place = {4, 0}, .rejoin = true},  {.place = {5, 0}},
	    {.place = {6, 0}},
	};
	const std::array<glm::ivec2, 2> route {glm::ivec2 {10, 1}, glm::ivec2 {11, 1}};
	const auto laid = planning::LayBetween(nodes, 0, 5, route, glm::ivec2 {12, 1});
	const std::vector<planning::PlanNode> expected {
	    {.place = {0, 0}},
	    {.place = {10, 1}, .planned = true},
	    {.place = {11, 1}, .planned = true},
	    {.place = {12, 1}, .planned = true, .rejoin = true},
	    {.place = {2, 0}, .hidden = true},
	    {.place = {4, 0}, .rejoin = true},
	    {.place = {5, 0}},
	    {.place = {6, 0}},
	};
	EXPECT_EQ(laid, expected);
}

TEST(FootpathPlanning, ASettledPathKeepsOnlyItsHiddenNodesPlanned)
{
	std::vector<planning::PlanNode> nodes {
	    {.place = {0, 0}, .planned = true},
	    {.place = {1, 0}, .planned = true, .hidden = true},
	    {.place = {2, 0}, .planned = true, .rejoin = true},
	};
	planning::Settle(nodes);
	EXPECT_FALSE(nodes[0].planned);
	EXPECT_TRUE(nodes[1].planned);
	EXPECT_TRUE(nodes[2].planned);
}

TEST(FootpathPlanning, AnEndInsideItsBuildingMovesToTheEdge)
{
	const std::array<route::Circle, 1> circles {route::Circle {.centre = {100.0f, 100.0f}, .radius = 4.9f}};
	const auto end = planning::ClearOfBuilding({102.0f, 100.0f}, {100.0f, 100.0f}, {200.0f, 100.0f}, circles);
	EXPECT_NEAR(end.x, 105.0f, 1e-4f);
	EXPECT_FLOAT_EQ(end.y, 100.0f);
	// One already outside stays
	const auto outside = planning::ClearOfBuilding({106.0f, 100.0f}, {100.0f, 100.0f}, {200.0f, 100.0f}, circles);
	EXPECT_EQ(outside, glm::vec2(106.0f, 100.0f));
}
