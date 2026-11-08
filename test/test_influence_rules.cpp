/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <filesystem>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/AllyInfluence.h"
#include "ECS/InfluenceRules.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "Enums.h"
#include "ScriptHeaders/ScriptInfluence.h"

using openblack::PlayerNames;
using openblack::influence_rules::CanHaveInfluence;
namespace influence_rules = openblack::ecs::systems::influence_rules;

namespace
{

/// The influence of four players at one place, and the count of the questions
struct FakeInfluence
{
	std::array<float, 4> influence {};
	size_t asked {0};

	float operator()(PlayerNames player)
	{
		++asked;
		return influence.at(static_cast<size_t>(player));
	}
};

constexpr std::array k_Players {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE,
                                PlayerNames::PLAYER_FOUR};

/// The answer of the rule that asks each player
PlayerNames MostOf(const std::array<float, 4>& influence)
{
	std::vector<influence_rules::PlayerInfluenceAt> players;
	for (const auto player : k_Players)
	{
		players.push_back({.player = player, .influence = influence.at(static_cast<size_t>(player))});
	}
	return influence_rules::MostInfluentialPlayer(players);
}

} // namespace

TEST(InfluenceRules, APlayerWithNoTempleHasNoInfluenceInANetworkGame)
{
	EXPECT_FALSE(CanHaveInfluence(true, false));
	EXPECT_TRUE(CanHaveInfluence(true, true));
}

TEST(InfluenceRules, EveryPlayerCanHaveInfluenceInAGameAlone)
{
	EXPECT_TRUE(CanHaveInfluence(false, false));
	EXPECT_TRUE(CanHaveInfluence(false, true));
}

TEST(InfluenceRules, TheTestOfOnePlayerAgreesWithTheMostInfluentialPlayer)
{
	// Each set of four values from a short list, with ties, zeros, negative values and a value that is not a number
	const std::array values {0.0f, 0.25f, 0.5f, 1.0f, -0.5f, std::numeric_limits<float>::quiet_NaN()};
	std::array<size_t, 4> digits {};
	size_t sets = 0;
	bool done = false;
	while (!done)
	{
		FakeInfluence fake;
		for (size_t i = 0; i < digits.size(); ++i)
		{
			fake.influence.at(i) = values.at(digits.at(i));
		}
		const auto most = MostOf(fake.influence);
		for (const auto player : k_Players)
		{
			EXPECT_EQ(influence_rules::IsMostInfluentialPlayer(player, k_Players, fake), most == player)
			    << "set " << sets << " player " << static_cast<int>(player);
		}
		++sets;
		size_t place = 0;
		while (place < digits.size() && ++digits.at(place) == values.size())
		{
			digits.at(place) = 0;
			++place;
		}
		done = place == digits.size();
	}
	EXPECT_EQ(sets, values.size() * values.size() * values.size() * values.size());
}

TEST(InfluenceRules, APlayerWithNoInfluenceAsksNoOtherPlayer)
{
	FakeInfluence fake {.influence = {0.0f, 1.0f, 1.0f, 1.0f}};
	EXPECT_FALSE(influence_rules::IsMostInfluentialPlayer(PlayerNames::PLAYER_ONE, k_Players, fake));
	EXPECT_EQ(fake.asked, 1u);
}

TEST(InfluenceRules, APlayerThatIsNotInTheListIsNeverTheMostInfluential)
{
	FakeInfluence fake {.influence = {0.0f, 0.0f, 0.0f, 1.0f}};
	const std::array players {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE};
	EXPECT_FALSE(influence_rules::IsMostInfluentialPlayer(PlayerNames::PLAYER_FOUR, players, fake));
	EXPECT_FALSE(influence_rules::IsMostInfluentialPlayer(PlayerNames::NEUTRAL, players, fake));
	EXPECT_EQ(fake.asked, 0u);
}

TEST(InfluenceRules, TheReachingTownsAddInTheOrderTheyWereGained)
{
	// The order changes the sum: 1e8 + 1 rounds to 1e8, then the last town takes it back to 0
	std::array<openblack::influence_rules::ReachingTown, 3> towns {{
	    {.gained = 9, .radius = -1.0e8f},
	    {.gained = 2, .radius = 1.0e8f},
	    {.gained = 5, .radius = 1.0f},
	}};
	EXPECT_EQ(openblack::influence_rules::AddReachingTowns(0.0f, towns), 0.0f);
	EXPECT_EQ(towns[0].gained, 2u);
	EXPECT_EQ(towns[2].gained, 9u);
	// The order of the gained numbers, not the order of the list that came in, decides it
	std::array<openblack::influence_rules::ReachingTown, 3> other {{
	    {.gained = 1, .radius = -1.0e8f},
	    {.gained = 2, .radius = 1.0e8f},
	    {.gained = 3, .radius = 1.0f},
	}};
	EXPECT_EQ(openblack::influence_rules::AddReachingTowns(0.0f, other), 1.0f);
	EXPECT_EQ(openblack::influence_rules::AddReachingTowns(0.25f, {}), 0.25f);
}

TEST(InfluenceRules, TheOwnInfluenceOrTheFirstLentGivesTheAnswerOfTheScriptRule)
{
	constexpr std::array<PlayerNames, 4> k_Players {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE,
	                                                PlayerNames::PLAYER_FOUR};
	const std::array<std::array<float, 4>, 4> lentCases {{
	    {0.0f, 0.5f, 0.75f, 0.0f},
	    {-0.5f, 0.0f, 0.0f, 0.0f},
	    {0.0f, 0.0f, 0.0f, 0.25f},
	    {0.0f, 0.0f, 0.0f, 0.0f},
	}};
	const std::array<std::array<bool, 4>, 2> lendsCases {{{true, true, true, true}, {false, false, true, true}}};
	for (const float own : {0.5f, 0.0f, -0.25f})
	{
		for (const auto& lent : lentCases)
		{
			for (const auto& lends : lendsCases)
			{
				std::vector<float> lentList;
				for (size_t i = 0; i < k_Players.size(); ++i)
				{
					if (lends.at(i))
					{
						lentList.push_back(lent.at(i));
					}
				}
				size_t asked = 0;
				const auto answer = openblack::ecs::ally_influence::OwnOrFirstLent(
				    own, k_Players, [&](PlayerNames ally) { return lends.at(static_cast<size_t>(ally)); },
				    [&](PlayerNames ally) {
					    ++asked;
					    return lent.at(static_cast<size_t>(ally));
				    });
				EXPECT_EQ(answer, openblack::script::influence::Answer(own, false, lentList));
				// A player with influence of its own asks no ally
				if (own > 0.0f)
				{
					EXPECT_EQ(asked, 0u);
				}
			}
		}
	}
}

TEST(InfluenceRules, OnlyThePathOfTheTestbedIsTheTestbed)
{
	using openblack::ecs::systems::IsTestbedPath;
	EXPECT_TRUE(IsTestbedPath(std::filesystem::path("testbed")));
	EXPECT_FALSE(IsTestbedPath(std::filesystem::path("Testbed")));
	EXPECT_FALSE(IsTestbedPath(std::filesystem::path("testbed2")));
	EXPECT_FALSE(IsTestbedPath(std::filesystem::path("testbe")));
	EXPECT_FALSE(IsTestbedPath(std::filesystem::path()));
	EXPECT_FALSE(IsTestbedPath(std::filesystem::path("Scripts/Land1.txt")));
	EXPECT_FALSE(IsTestbedPath(std::filesystem::path(u8"t\u00e9stbed")));
}

TEST(InfluenceRules, ABuildingAddsItsSizeOnceForItselfAndOnceForEachPerson)
{
	using openblack::influence_rules::OfBuilding;
	// Half built, twice the scale, three quarters of its life, three people
	EXPECT_EQ(OfBuilding({.built = 0.5f, .scale = 2.0f, .life = 0.75f, .influence = 25.0f, .people = 3}), 75.0f);
	// A miracle dispenser has no people: it adds its size once
	EXPECT_EQ(OfBuilding({.built = 1.0f, .scale = 1.0f, .life = 1.0f, .influence = 25.0f, .people = 0}), 25.0f);
	// A building with no life left or with nothing built adds nothing
	EXPECT_EQ(OfBuilding({.built = 1.0f, .scale = 1.0f, .life = 0.0f, .influence = 25.0f, .people = 2}), 0.0f);
	EXPECT_EQ(OfBuilding({.built = 0.0f, .scale = 1.0f, .life = 1.0f, .influence = 25.0f, .people = 2}), 0.0f);
}

TEST(InfluenceRules, TheStepsOfTheInfluenceOfABuildingRoundInTheGameOrder)
{
	// Scale times built part, then times life, then times the row's influence, then times the people and one. Each
	// step rounds to a float. Another order gives 0x40DCA4A5.
	const float influence = openblack::influence_rules::OfBuilding(
	    {.built = 0.238f, .scale = 1.044f, .life = 0.37f, .influence = 25.0f, .people = 2});
	EXPECT_EQ(std::bit_cast<uint32_t>(influence), 0x40DCA4A6u);
}

TEST(InfluenceRules, TheTownAddsItsBuildingsToItsBaseOneAtATimeThenTheMultiplier)
{
	using openblack::influence_rules::AddBuilding;
	using openblack::influence_rules::TimesMultiplier;
	// The base comes first: a large base swallows each small building alone
	float sum = 16777216.0f;
	sum = AddBuilding(sum, 1.0f);
	sum = AddBuilding(sum, 1.0f);
	EXPECT_EQ(sum, 16777216.0f);
	// The buildings first would give more
	EXPECT_EQ(AddBuilding(AddBuilding(1.0f, 1.0f), 16777216.0f), 16777218.0f);
	EXPECT_EQ(TimesMultiplier(30.0f, 0.5f), 15.0f);
}

TEST(InfluenceRules, ATownThatAScaffoldFoundedGetsItsInfluenceWhenItsCentreIsNoLongerAFreeSite)
{
	using openblack::influence_rules::FoundedTownCentre;
	using openblack::influence_rules::FoundedTownGetsInfluence;
	// Only a town centre under construction, with a scaffold that the hand can still take, keeps the town at no influence
	EXPECT_FALSE(FoundedTownGetsInfluence({.underConstruction = true, .hasScaffold = true, .scaffoldTakeable = true}));
	// The town centre is fully built
	EXPECT_TRUE(FoundedTownGetsInfluence({.underConstruction = false, .hasScaffold = true, .scaffoldTakeable = true}));
	// No scaffold is on the site
	EXPECT_TRUE(FoundedTownGetsInfluence({.underConstruction = true, .hasScaffold = false, .scaffoldTakeable = false}));
	// The first wood went in, or the workshop no longer keeps the scaffold
	EXPECT_TRUE(FoundedTownGetsInfluence({.underConstruction = true, .hasScaffold = true, .scaffoldTakeable = false}));
	EXPECT_TRUE(FoundedTownGetsInfluence(FoundedTownCentre {}));
}

TEST(InfluenceRules, TheReachOfAHeartIsItsSizeTimesTheInfluenceForTheLand)
{
	using openblack::influence_rules::HeartReach;
	EXPECT_EQ(HeartReach(1.0f, 80.0f), 80.0f);
	// A plan of size 500 gives a heart of half the size, thus half the reach
	EXPECT_EQ(HeartReach(static_cast<float>(500) * 0.001f, 80.0f), 40.0f);
	EXPECT_EQ(HeartReach(0.0f, 80.0f), 0.0f);
}
