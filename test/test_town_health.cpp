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

#include <gtest/gtest.h>

#include "ECS/TownHealth.h"

using namespace openblack::ecs;

TEST(TownHealth, ATownWithNothingHasNoHealth)
{
	EXPECT_EQ(town_health::Total({}, {}), 0.0f);
}

TEST(TownHealth, AddsEachBuildingWithItsPeopleThenThePeopleWithoutAHome)
{
	const std::array<float, 2> firstPeople {1.0f, 0.5f};
	const std::array<float, 1> secondPeople {0.25f};
	const std::array<town_health::AbodeLives, 3> abodes {{
	    {.residents = firstPeople, .life = 1.0f},
	    {.residents = secondPeople, .life = 0.75f},
	    {.residents = {}, .life = 0.5f},
	}};
	const std::array<float, 2> homeless {1.0f, 0.125f};
	EXPECT_EQ(town_health::Total(abodes, homeless), 5.125f);
}

TEST(TownHealth, AddsThePeopleOfABuildingBeforeTheyJoinTheTotal)
{
	// The sum of the people of a building rounds on its own before it joins the total. Then the large total does not
	// swallow each small life one at a time.
	const std::vector<float> people(4, 0.75f);
	const std::array<town_health::AbodeLives, 1> abodes {{{.residents = people, .life = 0.0f}}};
	const std::array<float, 1> homeless {0.0f};
	const std::array<town_health::AbodeLives, 2> big {{{.residents = {}, .life = 16777216.0f}, abodes[0]}};
	EXPECT_EQ(town_health::Total(big, homeless), 16777216.0f + 3.0f);
	EXPECT_EQ(town_health::ResidentsTotal(people), 3.0f);
}
