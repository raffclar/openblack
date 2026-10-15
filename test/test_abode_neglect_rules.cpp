/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/AbodeNeglectRules.h"

using namespace openblack::ecs::abode_neglect;

namespace
{
/// A built house of four adult and four child places with nobody in it
NeglectFacts EmptyHouse()
{
	return {.adults = 0, .children = 0, .maxAdults = 4, .maxChildren = 4, .built = true};
}

/// The number of examinations until the house loses life
uint32_t ExaminationsUntilLoss(const NeglectFacts& facts, uint32_t limit)
{
	float timer = 0.0f;
	for (uint32_t i = 1; i <= limit; ++i)
	{
		const auto step = Examine(timer, facts);
		if (step.losesLife)
		{
			return i;
		}
		timer = step.timer;
	}
	return 0;
}
} // namespace

TEST(AbodeNeglectRules, AnEmptyHouseLosesLifeAfterOneThousandAndOneExaminations)
{
	EXPECT_EQ(ExaminationsUntilLoss(EmptyHouse(), 5000), 1001u);
}

TEST(AbodeNeglectRules, TheTimerStartsAgainAfterALoss)
{
	const auto step = Examine(0.9999f, EmptyHouse());
	EXPECT_TRUE(step.losesLife);
	EXPECT_EQ(step.timer, 0.0f);
}

TEST(AbodeNeglectRules, AHouseWithAnAdultIsNotEmpty)
{
	auto house = EmptyHouse();
	house.adults = 1;
	EXPECT_FALSE(CountsAsEmpty(house));
	EXPECT_EQ(Examine(0.5f, house).timer, 0.5f);
}

TEST(AbodeNeglectRules, AHouseWithOnlyChildrenIsEmptyUntilEachChildPlaceIsFull)
{
	auto house = EmptyHouse();
	house.children = 3;
	EXPECT_TRUE(CountsAsEmpty(house));
	house.children = 4;
	EXPECT_FALSE(CountsAsEmpty(house));
}

TEST(AbodeNeglectRules, ABuildingWithoutPlacesIsNeverEmpty)
{
	auto pit = EmptyHouse();
	pit.maxAdults = 0;
	EXPECT_FALSE(CountsAsEmpty(pit));
	auto noChildren = EmptyHouse();
	noChildren.maxChildren = 0;
	EXPECT_FALSE(CountsAsEmpty(noChildren));
}

TEST(AbodeNeglectRules, NoNeglectWhenUnbuiltHeldByAScriptOrInAnUninhabitableTown)
{
	auto unbuilt = EmptyHouse();
	unbuilt.built = false;
	auto scripted = EmptyHouse();
	scripted.inScript = true;
	auto lone = EmptyHouse();
	lone.townUninhabitable = true;
	for (const auto& facts : {unbuilt, scripted, lone})
	{
		const auto step = Examine(0.9999f, facts);
		EXPECT_FALSE(step.losesLife);
		EXPECT_EQ(step.timer, 0.9999f);
	}
}
