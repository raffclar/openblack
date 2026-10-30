/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <array>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/InfluenceRules.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "Enums.h"

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
