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
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/CrecheRules.h"

using namespace openblack;
using namespace openblack::creche;

namespace
{
/// Draws that hand out set values in turn, remembering the limits asked for
class Draws
{
public:
	explicit Draws(std::vector<uint32_t> values)
	    : _values(std::move(values))
	{
	}

	[[nodiscard]] IntRandom Int()
	{
		return [this](uint32_t limit) {
			limits.push_back(limit);
			return _next < _values.size() ? _values[_next++] : 0u;
		};
	}

	std::vector<uint32_t> limits;

private:
	std::vector<uint32_t> _values;
	size_t _next {0};
};

constexpr uint32_t k_Paths = 8;
} // namespace

TEST(CrecheRules, AModelsPointsMakeWholePathsOfFive)
{
	EXPECT_EQ(PathsIn(0), 0u);
	EXPECT_EQ(PathsIn(4), 0u);
	EXPECT_EQ(PathsIn(5), 1u);
	EXPECT_EQ(PathsIn(40), 8u);
	EXPECT_EQ(PathsIn(44), 8u);
}

TEST(CrecheRules, AChildAtTheDoorTakesAPathAtRandomFromItsStart)
{
	Draws draws({3});
	const auto leg = NextStrollLeg({.path = 6, .leg = 7}, k_Paths, true, draws.Int());
	EXPECT_EQ(draws.limits, std::vector<uint32_t> {k_Paths});
	EXPECT_EQ(leg.stroll.path, 3);
	EXPECT_EQ(leg.stroll.leg, 0);
	ASSERT_TRUE(leg.point.has_value());
	EXPECT_EQ(*leg.point, 15u);
}

TEST(CrecheRules, AStrollGoesOutAlongThePathsFivePointsAndBack)
{
	Draws draws({});
	Stroll stroll {.path = 2, .leg = 0};
	std::vector<size_t> points;
	for (int i = 0; i < 9; ++i)
	{
		const auto leg = NextStrollLeg(stroll, k_Paths, false, draws.Int());
		stroll = leg.stroll;
		ASSERT_TRUE(leg.point.has_value());
		points.push_back(*leg.point);
	}
	// Legs 1 to 9: out to the fifth point, then back to the first
	EXPECT_EQ(points, (std::vector<size_t> {11, 12, 13, 14, 14, 13, 12, 11, 10}));
	EXPECT_TRUE(draws.limits.empty());
}

TEST(CrecheRules, AfterTheLastLegBackAChildStartsAfreshOnAPathAtRandom)
{
	Draws draws({5});
	const auto leg = NextStrollLeg({.path = 2, .leg = 9}, k_Paths, false, draws.Int());
	EXPECT_EQ(draws.limits, std::vector<uint32_t> {k_Paths});
	EXPECT_EQ(leg.stroll.path, 5);
	EXPECT_EQ(leg.stroll.leg, 0);
	EXPECT_EQ(*leg.point, 25u);
}

TEST(CrecheRules, ADrawingOutsideThePathsTakesTheFirst)
{
	Draws draws({9});
	EXPECT_EQ(NextStrollLeg({}, k_Paths, true, draws.Int()).stroll.path, 0);
}

TEST(CrecheRules, APathPastThoseThereAreIsTheLastAndABackwardLegStartsTheStroll)
{
	Draws draws({});
	const auto past = NextStrollLeg({.path = 20, .leg = 1}, k_Paths, false, draws.Int());
	EXPECT_EQ(past.stroll.path, 7);
	EXPECT_EQ(past.stroll.leg, 2);
	EXPECT_EQ(*past.point, 37u);
	const auto backward = NextStrollLeg({.path = 1, .leg = -5}, k_Paths, false, draws.Int());
	EXPECT_EQ(backward.stroll.leg, 0);
	EXPECT_EQ(*backward.point, 5u);
}

TEST(CrecheRules, WithNoPathsAChildMakesForTheDoor)
{
	Draws draws({});
	const auto atDoor = NextStrollLeg({}, 0, true, draws.Int());
	EXPECT_FALSE(atDoor.point.has_value());
	// The game still asks for a drawing below none
	EXPECT_EQ(draws.limits, std::vector<uint32_t> {0});
	EXPECT_FALSE(NextStrollLeg({.path = 0, .leg = 3}, 0, false, draws.Int()).point.has_value());
}

TEST(CrecheRules, AChildStraysUpToHalfAMetreEachWayDrawnAlongTheSecondAxisFirst)
{
	std::vector<float> limits;
	const std::array<float, 2> values {0.25f, 0.75f};
	size_t next = 0;
	const auto strayed = Strayed({10.0f, 20.0f}, [&](float limit) {
		limits.push_back(limit);
		return values.at(next++);
	});
	EXPECT_EQ(limits, (std::vector<float> {1.0f, 1.0f}));
	EXPECT_FLOAT_EQ(strayed.y, 19.75f);
	EXPECT_FLOAT_EQ(strayed.x, 10.25f);
}

TEST(CrecheRules, ChildrenStrollUntilNightFalls)
{
	EXPECT_TRUE(IsDaytime(2.0f));
	EXPECT_TRUE(IsDaytime(1.0f));
	EXPECT_TRUE(IsDaytime(0.8f));
	EXPECT_FALSE(IsDaytime(0.79f));
	EXPECT_FALSE(IsDaytime(0.0f));
}

TEST(CrecheRules, WhatAChildAtTheCrecheDoes)
{
	EXPECT_EQ(AtTheCreche(true, true, true, false, false), AtCreche::Stroll);
	EXPECT_EQ(AtTheCreche(true, true, false, false, false), AtCreche::Stroll);
	// At night, or by day at a crèche that doesn't work, a child with a home waits for the first of its home to be in
	EXPECT_EQ(AtTheCreche(false, true, true, true, false), AtCreche::GoHome);
	EXPECT_EQ(AtTheCreche(false, true, true, false, true), AtCreche::Wait);
	EXPECT_EQ(AtTheCreche(true, false, true, true, false), AtCreche::GoHome);
	// A homeless one strolls on quietly while it touches the crèche
	EXPECT_EQ(AtTheCreche(false, true, false, false, true), AtCreche::StrollQuietly);
	EXPECT_EQ(AtTheCreche(false, true, false, false, false), AtCreche::Wait);
}

TEST(CrecheRules, AnyCivicBuildingButTheWonderTakesOverAsTheCreche)
{
	EXPECT_TRUE(TakesOverAsCreche(AbodeType::Creche));
	EXPECT_TRUE(TakesOverAsCreche(AbodeType::StoragePit));
	EXPECT_TRUE(TakesOverAsCreche(AbodeType::Graveyard));
	EXPECT_TRUE(TakesOverAsCreche(AbodeType::TownCentre));
	EXPECT_TRUE(TakesOverAsCreche(AbodeType::Field));
	EXPECT_TRUE(TakesOverAsCreche(AbodeType::Totem));
	EXPECT_FALSE(TakesOverAsCreche(AbodeType::LivingQuarters));
	EXPECT_FALSE(TakesOverAsCreche(AbodeType::Windmill));
	EXPECT_FALSE(TakesOverAsCreche(AbodeType::Wonder));
}

TEST(CrecheRules, LosingTheTownsCrecheHandsOverToTheFirstWorkingBuildingThatTakesOver)
{
	const std::array<OtherBuilding, 4> others {{
	    {.working = true, .takesOver = false},
	    {.working = false, .takesOver = true},
	    {.working = true, .takesOver = true},
	    {.working = true, .takesOver = true},
	}};
	EXPECT_EQ(CrecheAfterLoss(true, others), std::optional<size_t> {2});
	EXPECT_EQ(CrecheAfterLoss(true, std::span(others).first(2)), std::nullopt);
	// Losing a spare leaves the town without one
	EXPECT_EQ(CrecheAfterLoss(false, others), std::nullopt);
}
