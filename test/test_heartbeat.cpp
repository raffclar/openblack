/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <numbers>

#include <gtest/gtest.h>

#include "ECS/Heartbeat.h"

using namespace openblack::ecs::heartbeat;
using openblack::PlayerNames;

namespace
{
constexpr float k_TurnSeconds = 0.1f;
} // namespace

TEST(Heartbeat, ItIsMeasuredEveryTenTurns)
{
	EXPECT_TRUE(IsMeasureTurn(0));
	EXPECT_FALSE(IsMeasureTurn(9));
	EXPECT_TRUE(IsMeasureTurn(10));
}

TEST(Heartbeat, ACalmHeartBeatsThreeQuartersOfATimeASecond)
{
	State state;
	for (int i = 0; i < 10; ++i)
	{
		Step(state, k_TurnSeconds);
	}
	EXPECT_FLOAT_EQ(state.rate, k_CalmRate);
	// Ten turns of a second at 0.75 beats a second
	EXPECT_NEAR(state.phase, 0.75f, 1e-4f);
	EXPECT_NEAR(state.pulse, (1.0f - std::cos(0.75f * 2.0f * std::numbers::pi_v<float>)) * 0.5f, 1e-4f);
}

TEST(Heartbeat, TheRateEasesATenthOfTheWayToTheDangerEachTurn)
{
	State state;
	state.stress = 1.0f;
	Step(state, k_TurnSeconds);
	EXPECT_FLOAT_EQ(state.rate, 30.0f + (100.0f - 30.0f) * 0.1f);
}

TEST(Heartbeat, ThePulseIsTakenBetweenTheLastTurnAndThisOne)
{
	State state;
	state.previousPulse = 0.2f;
	state.pulse = 0.6f;
	EXPECT_FLOAT_EQ(Pulse(state, 0.0f), 0.2f);
	EXPECT_FLOAT_EQ(Pulse(state, 0.5f), 0.4f);
	EXPECT_FLOAT_EQ(Pulse(state, 1.0f), 0.6f);
}

TEST(Heartbeat, TheBeatWrapsRoundAfterAWholeBeat)
{
	State state;
	state.phase = 0.99f;
	state.rate = 100.0f;
	Step(state, k_TurnSeconds);
	EXPECT_GE(state.phase, 0.0f);
	EXPECT_LE(state.phase, 1.0f);
	// The calm heart's rate eases from 100 to 93 first
	EXPECT_NEAR(state.phase, 0.99f + 93.0f * 0.025f * 0.1f - 1.0f, 1e-4f);
}

TEST(Heartbeat, TheDestructionHoldsTheBeat)
{
	State state;
	state.destructionBeats = 2.0f;
	Step(state, k_TurnSeconds);
	EXPECT_FLOAT_EQ(state.rate * k_BeatsPerRate, 2.0f);
	EXPECT_FLOAT_EQ(DestructionBeats(-1.0f), 2.0f / std::numbers::pi_v<float>);
	EXPECT_FLOAT_EQ(DestructionBeats(0.0f), 2.0f / std::numbers::pi_v<float>);
	EXPECT_FLOAT_EQ(DestructionBeats(19.0f), 4.0f / std::numbers::pi_v<float>);
	EXPECT_FLOAT_EQ(DestructionBeats(22.0f), (4.0f + 8.5f * 0.5f) / std::numbers::pi_v<float>);
	EXPECT_FLOAT_EQ(DestructionBeats(30.0f), 12.5f / std::numbers::pi_v<float>);
}

TEST(Heartbeat, TheDangerAddsTheTownsWishForProtectionKeptBetweenNoneAndAll)
{
	State state;
	state.believerShare = 0.5f;
	state.influenceShare = 2.0f;
	Measure(state, {.protectionDesire = 0.4f, .believerShare = 0.5f, .influenceShare = 2.0f});
	EXPECT_NEAR(state.stress, 0.4f, 1e-6f);
	Measure(state, {.protectionDesire = 3.0f, .believerShare = 0.5f, .influenceShare = 2.0f});
	EXPECT_FLOAT_EQ(state.stress, 1.0f);
	Measure(state, {.protectionDesire = -3.0f, .believerShare = 0.5f, .influenceShare = 2.0f});
	EXPECT_FLOAT_EQ(state.stress, 0.0f);
}

TEST(Heartbeat, AFallingShareOfThePeopleAddsToTheDangerAndTheSmoothedShareFollows)
{
	State state;
	state.believerShare = 0.5f;
	state.influenceShare = 2.0f;
	Measure(state, {.believerShare = 0.25f, .influenceShare = 2.0f});
	EXPECT_NEAR(state.stress, (0.501f / 0.251f) - 1.0f > 1.0f ? 1.0f : (0.501f / 0.251f) - 1.0f, 1e-5f);
	EXPECT_FLOAT_EQ(state.believerShare, 0.5f + (0.25f - 0.5f) * 0.1f);
}

TEST(Heartbeat, AnEnemyCreatureNearThePlayersTownAddsToTheDanger)
{
	EXPECT_FLOAT_EQ(CreatureDanger(0), 1.0f);
	EXPECT_FLOAT_EQ(CreatureDanger(100), 1.0f);
	EXPECT_FLOAT_EQ(CreatureDanger(300), 0.5f);
	EXPECT_FLOAT_EQ(CreatureDanger(399), 1.0f - 299.0f / 400.0f);
	EXPECT_FLOAT_EQ(CreatureDanger(400), 0.0f);
	State state;
	state.believerShare = 0.5f;
	state.influenceShare = 2.0f;
	const std::array<uint32_t, 2> creatures {300, 1000};
	Measure(state, {.believerShare = 0.5f, .influenceShare = 2.0f, .creatureDistances = creatures});
	EXPECT_NEAR(state.stress, 0.5f, 1e-6f);
}

TEST(Heartbeat, CellsAreMeasuredTheLongerWayAndHalfTheShorter)
{
	EXPECT_EQ(CellDistance({0, 0}, {10, 3}), 11u);
	EXPECT_EQ(CellDistance({0, 0}, {3, -10}), 11u);
	EXPECT_EQ(CellDistance({5, 5}, {5, 5}), 0u);
}

TEST(Heartbeat, AKeptHeartIsHandedOverOnceToItsOwnPlayer)
{
	KeptHearts kept;
	State racing;
	racing.rate = 85.0f;
	racing.phase = 0.4f;
	racing.believerShare = 0.6f;
	kept.Keep(PlayerNames::PLAYER_ONE, racing);

	EXPECT_FALSE(kept.Take(PlayerNames::PLAYER_TWO).has_value());
	const auto taken = kept.Take(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(taken.has_value());
	EXPECT_FLOAT_EQ(taken->rate, 85.0f);
	EXPECT_FLOAT_EQ(taken->phase, 0.4f);
	EXPECT_FLOAT_EQ(taken->believerShare, 0.6f);
	EXPECT_FALSE(kept.Take(PlayerNames::PLAYER_ONE).has_value());
}

TEST(Heartbeat, ForgettingTheKeptHeartsStartsEveryoneCalm)
{
	KeptHearts kept;
	kept.Keep(PlayerNames::PLAYER_ONE, State {.rate = 70.0f});
	kept.Forget();
	EXPECT_FALSE(kept.Take(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FLOAT_EQ(State {}.rate, k_CalmRate);
}
