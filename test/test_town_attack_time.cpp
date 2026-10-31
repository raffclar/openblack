/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <limits>

#include <gtest/gtest.h>

#include "ECS/TownAggression.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr uint32_t k_MillisecondsPerTurn = 100;
constexpr float k_None = std::numeric_limits<float>::max();

uint32_t BitsOf(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

/// A record in which a player attacked once, at a turn, with an amount of aggression
town_aggression::Record AttackedOnce(PlayerNames player, float amount, uint32_t turn)
{
	town_aggression::Record record;
	town_aggression::Attacked(record, player, false, amount, 0.0f, turn);
	return record;
}
} // namespace

TEST(TownAttackTime, TheTimeIsTheTurnsSinceTheAttackInSeconds)
{
	const auto record = AttackedOnce(PlayerNames::PLAYER_TWO, 1.0f, 100);
	// 50 turns of 100 ms: 5000 times the float of a thousandth rounds to 5 exactly
	EXPECT_EQ(BitsOf(town_aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_TWO, 150, k_MillisecondsPerTurn)),
	          BitsOf(5.0f));
	EXPECT_EQ(BitsOf(town_aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_TWO, 100, k_MillisecondsPerTurn)),
	          BitsOf(0.0f));
}

TEST(TownAttackTime, TheProductRoundsToAFloatBeforeTheChangeToSeconds)
{
	// 3 turns of 100 ms: the float of 300 times the float of a thousandth
	const auto record = AttackedOnce(PlayerNames::PLAYER_ONE, 1.0f, 7);
	EXPECT_EQ(BitsOf(town_aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_ONE, 10, k_MillisecondsPerTurn)),
	          BitsOf(300.0f * 0.001f));
}

TEST(TownAttackTime, AnotherPlayerOrNoAttackGivesTheLargestFloat)
{
	const auto record = AttackedOnce(PlayerNames::PLAYER_TWO, 1.0f, 100);
	EXPECT_EQ(town_aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_THREE, 150, k_MillisecondsPerTurn), k_None);
	EXPECT_EQ(
	    town_aggression::SecondsSinceAttacked(town_aggression::Record {}, PlayerNames::PLAYER_ONE, 150, k_MillisecondsPerTurn),
	    k_None);
	EXPECT_EQ(town_aggression::SecondsSinceAttacked(record, PlayerNames::_COUNT, 150, k_MillisecondsPerTurn), k_None);
}

TEST(TownAttackTime, TheTownMustHoldMoreThanTheDoubleLimitAgainstThePlayer)
{
	// The float 0.15 is a little more than the double 0.15, so it counts
	const auto atLimit = AttackedOnce(PlayerNames::PLAYER_TWO, 0.15f, 100);
	EXPECT_NE(town_aggression::SecondsSinceAttacked(atLimit, PlayerNames::PLAYER_TWO, 110, k_MillisecondsPerTurn), k_None);

	const auto under = AttackedOnce(PlayerNames::PLAYER_TWO, 0.1499999f, 100);
	EXPECT_EQ(town_aggression::SecondsSinceAttacked(under, PlayerNames::PLAYER_TWO, 110, k_MillisecondsPerTurn), k_None);
}

TEST(TownAttackTime, TheAggressionThatFadesStopsTheTime)
{
	auto record = AttackedOnce(PlayerNames::PLAYER_TWO, 0.2f, 0);
	EXPECT_NE(town_aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_TWO, 1, k_MillisecondsPerTurn), k_None);
	uint32_t turn = 0;
	while (static_cast<double>(record.aggression.at(1)) > 0.15)
	{
		town_aggression::ProcessTurn(record, PlayerNames::PLAYER_ONE);
		++turn;
	}
	EXPECT_EQ(town_aggression::SecondsSinceAttacked(record, PlayerNames::PLAYER_TWO, turn, k_MillisecondsPerTurn), k_None);
}
