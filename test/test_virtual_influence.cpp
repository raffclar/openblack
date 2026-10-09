/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/VirtualInfluence.h"

using namespace openblack;
using namespace openblack::virtual_influence;

namespace
{
constexpr Settings k_Settings {.maxDistance = 50.0f, .maxTurns = 100, .chantsToDouble = 1000.0f};

TurnInputs Inside(glm::vec3 hand, uint32_t turn)
{
	return {.hand = hand, .turn = turn, .handShielded = false, .handInInfluence = true, .influencePower = 0.0f, .chants = 0.0f};
}

TurnInputs Outside(glm::vec3 hand, uint32_t turn, float power = 0.0f, float chants = 0.0f)
{
	return {
	    .hand = hand, .turn = turn, .handShielded = false, .handInInfluence = false, .influencePower = power, .chants = chants};
}

/// What a turn out takes, as the rule is stated: one less the product of how near and how fresh
float Loss(float distance, uint32_t turnsOut, float power, float chants)
{
	const float timeScale = 1.0f / (chants / k_Settings.chantsToDouble + 1.0f);
	const float nearness = gutils::GetDistanceModifier(distance, 0.2f * power + k_Settings.maxDistance);
	const float freshness = gutils::GetDistanceModifier(timeScale * static_cast<float>(turnsOut),
	                                                    0.2f * power + static_cast<float>(k_Settings.maxTurns));
	return 1.0f - freshness * nearness;
}
} // namespace

TEST(VirtualInfluence, InInfluenceTheHandMarksThePlaceAndWinsATenthBack)
{
	State state {.fraction = 0.45f};
	ProcessTurn(state, Inside({100.0f, 0.0f, 200.0f}, 30), k_Settings);
	ASSERT_TRUE(state.anchor.has_value());
	EXPECT_EQ(*state.anchor, glm::vec3(100.0f, 0.0f, 200.0f));
	EXPECT_EQ(state.lastInsideTurn, 30u);
	EXPECT_FLOAT_EQ(state.fraction, 0.55f);
	for (uint32_t turn = 31; turn < 40; ++turn)
	{
		ProcessTurn(state, Inside({100.0f, 0.0f, 200.0f}, turn), k_Settings);
	}
	EXPECT_EQ(state.fraction, 1.0f);
}

TEST(VirtualInfluence, OutsideItWanesWithDistanceAndTime)
{
	State state;
	ProcessTurn(state, Inside({100.0f, 0.0f, 100.0f}, 10), k_Settings);
	// A step past the border, the turn after: hardly anything goes
	const glm::vec3 near {100.0f, 0.0f, 105.0f};
	ProcessTurn(state, Outside(near, 11), k_Settings);
	const float distance =
	    gutils::GetDistanceInMetres(map_coords::FromMetres({100.0f, 100.0f}), map_coords::FromMetres({100.0f, 105.0f}));
	float expected = 1.0f - Loss(distance, 1, 0.0f, 0.0f);
	EXPECT_FLOAT_EQ(state.fraction, expected);
	EXPECT_GT(state.fraction, 0.9f);
	// Further and longer out it goes faster, and what is under a tenth is gone at once
	float before = state.fraction;
	for (uint32_t turn = 12; turn < 200 && state.fraction != 0.0f; ++turn)
	{
		ProcessTurn(state, Outside({100.0f, 0.0f, 140.0f}, turn), k_Settings);
		EXPECT_LT(state.fraction, before);
		EXPECT_TRUE(state.fraction == 0.0f || state.fraction >= 0.1f);
		before = state.fraction;
	}
	EXPECT_EQ(state.fraction, 0.0f);
}

TEST(VirtualInfluence, InfluencePowerAndChantsMakeItLast)
{
	State weak;
	ProcessTurn(weak, Inside({0.0f, 0.0f, 0.0f}, 0), k_Settings);
	State strong = weak;
	State chanting = weak;
	ProcessTurn(weak, Outside({0.0f, 0.0f, 30.0f}, 40), k_Settings);
	ProcessTurn(strong, Outside({0.0f, 0.0f, 30.0f}, 40, 500.0f), k_Settings);
	ProcessTurn(chanting, Outside({0.0f, 0.0f, 30.0f}, 40, 0.0f, 3000.0f), k_Settings);
	EXPECT_GT(strong.fraction, weak.fraction);
	EXPECT_GE(chanting.fraction, weak.fraction);
	const float distance =
	    gutils::GetDistanceInMetres(map_coords::FromMetres({0.0f, 0.0f}), map_coords::FromMetres({0.0f, 30.0f}));
	const float expected = 1.0f - Loss(distance, 40, 500.0f, 0.0f);
	EXPECT_FLOAT_EQ(strong.fraction, expected < 0.1f ? 0.0f : expected);
}

TEST(VirtualInfluence, AShieldTakesItAllAndAScriptCanTurnItOff)
{
	State state;
	ProcessTurn(state, Inside({0.0f, 0.0f, 0.0f}, 0), k_Settings);
	auto shielded = Outside({0.0f, 0.0f, 5.0f}, 1);
	shielded.handShielded = true;
	ProcessTurn(state, shielded, k_Settings);
	EXPECT_EQ(state.fraction, 0.0f);

	State off {.fraction = 0.5f, .disabled = true};
	ProcessTurn(off, Inside({0.0f, 0.0f, 0.0f}, 3), k_Settings);
	EXPECT_EQ(off.fraction, 0.5f);
	EXPECT_FALSE(off.anchor.has_value());
}

TEST(VirtualInfluence, TheHumPlaysOutsideWhileTheLastPlaceInIsStillIn)
{
	State state {.anchor = glm::vec3(0.0f), .fraction = 0.8f};
	EXPECT_TRUE(HumPlays(state, false, false, false, true));
	// Not inside, nor under a shield, nor once the last place in has fallen out of influence
	EXPECT_FALSE(HumPlays(state, false, true, false, true));
	EXPECT_FALSE(HumPlays(state, true, false, false, true));
	EXPECT_FALSE(HumPlays(state, false, false, true, true));
	EXPECT_FALSE(HumPlays(state, false, false, false, false));
	// Nor with nothing left, nor before the hand has been in influence, nor turned off
	EXPECT_FALSE(HumPlays(State {.anchor = glm::vec3(0.0f), .fraction = 0.0f}, false, false, false, true));
	EXPECT_FALSE(HumPlays(State {.fraction = 0.8f}, false, false, false, true));
	EXPECT_FALSE(HumPlays(State {.anchor = glm::vec3(0.0f), .fraction = 0.8f, .disabled = true}, false, false, false, true));
}

TEST(VirtualInfluence, TheHumsPitchIsTheStrengthLeft)
{
	State state {.fraction = 0.734f, .soundFraction = 1.0f};
	EXPECT_EQ(HumPitchPercent(state), 73u);
	state.fraction = 0.1f;
	EXPECT_EQ(HumPitchPercent(state), 10u);
	// Never above what it began at
	state.fraction = 0.9f;
	state.soundFraction = 0.6f;
	EXPECT_EQ(HumPitchPercent(state), 60u);
}

TEST(VirtualInfluence, NearTheHandThePlaceIsInInfluenceAsMuchAsTheStrength)
{
	const auto hand = map_coords::FromMetres({500.0f, 500.0f});
	State state;
	state.fraction = 0.5f;
	// Half strength reaches five metres, the edge itself not counted
	const auto near = Grant(state, hand, map_coords::FromMetres({503.0f, 500.0f}));
	ASSERT_TRUE(near.has_value());
	EXPECT_FLOAT_EQ(*near, 0.5f);
	const float edge = gutils::GetDistanceInMetres(hand, map_coords::FromMetres({505.0f, 500.0f}));
	EXPECT_EQ(Grant(state, hand, map_coords::FromMetres({505.0f, 500.0f})).has_value(), edge < 5.0f);
	EXPECT_FALSE(Grant(state, hand, map_coords::FromMetres({506.0f, 500.0f})).has_value());
}

TEST(VirtualInfluence, AtTheHandAnyStrengthCountsAndNoneDoesNot)
{
	const auto hand = map_coords::FromMetres({120.0f, 80.0f});
	State state;
	state.fraction = 0.1f;
	EXPECT_TRUE(Grant(state, hand, hand).has_value());
	state.fraction = 0.0f;
	EXPECT_FALSE(Grant(state, hand, hand).has_value());
}

TEST(VirtualInfluence, TheGrantNeverExceedsFull)
{
	const auto hand = map_coords::FromMetres({120.0f, 80.0f});
	State state;
	state.fraction = 1.5f;
	EXPECT_FLOAT_EQ(Grant(state, hand, hand).value_or(0.0f), 1.0f);
	// And reaches no further than full strength does
	EXPECT_FALSE(Grant(state, hand, map_coords::FromMetres({131.0f, 80.0f})).has_value());
}

TEST(VirtualInfluence, EveryTurnNotesWhereTheHandIsEvenWhenSwitchedOff)
{
	State state;
	state.disabled = true;
	ProcessTurn(state, Outside({30.0f, 0.0f, 40.0f}, 5), k_Settings);
	ASSERT_TRUE(state.turnHand.has_value());
	EXPECT_EQ(state.turnHand->x, 30.0f);
	EXPECT_FALSE(state.anchor.has_value());
}

TEST(VirtualInfluence, TheManaPathLeadsHalfwayBackRoundingHalvesToEven)
{
	const map_coords::MapCoords hand {.x = 1000, .z = 2000, .altitude = 0.0f};
	// Even differences halve exactly
	EXPECT_EQ(ManaPathStart(hand, {.x = 3000, .z = 1000, .altitude = 0.0f}),
	          (map_coords::MapCoords {.x = 2000, .z = 1500, .altitude = 0.0f}));
	// Odd ones round to the even neighbour: 1.5 to 2, 2.5 to 2, -1.5 to -2
	EXPECT_EQ(ManaPathStart(hand, {.x = 1003, .z = 2005, .altitude = 0.0f}).x, 1002);
	EXPECT_EQ(ManaPathStart(hand, {.x = 1003, .z = 2005, .altitude = 0.0f}).z, 2002);
	EXPECT_EQ(ManaPathStart(hand, {.x = 997, .z = 2000, .altitude = 0.0f}).x, 998);
}

TEST(VirtualInfluence, AManaPathSparkIsDueOncePastTenWeighedMilliseconds)
{
	State state;
	state.fraction = 0.5f;
	// 16 ms at half strength is 8: not yet; another 4 ms makes 10, which is not past ten
	EXPECT_FALSE(EmitManaPath(state, 16.0f).has_value());
	EXPECT_FALSE(EmitManaPath(state, 4.0f).has_value());
	EXPECT_FLOAT_EQ(state.manaPathEmission, 10.0f);
	// Then one more and it is due, starting again from nothing, scaled by half of 255 truncated
	const auto scale = EmitManaPath(state, 1.0f);
	ASSERT_TRUE(scale.has_value());
	EXPECT_EQ(*scale, 127);
	EXPECT_FLOAT_EQ(state.manaPathEmission, 0.0f);
}

TEST(VirtualInfluence, AManaPathSparkAtFullStrengthIsNotDimmedPastFull)
{
	State state;
	state.fraction = 1.5f;
	const auto scale = EmitManaPath(state, 11.0f);
	ASSERT_TRUE(scale.has_value());
	EXPECT_EQ(*scale, 255);
}

TEST(VirtualInfluence, ColoursAreScaledOnEveryChannelOutOf256)
{
	EXPECT_EQ(ScaleColour(0xFFFF4646u, 255), 0xFEFE4545u);
	EXPECT_EQ(ScaleColour(0xFF47FF54u, 128), 0x7F237F2Au);
	EXPECT_EQ(ScaleColour(0xFFFFFFFFu, 0), 0u);
}

TEST(VirtualInfluence, BackInInfluenceTheManaPathIsWorkedOutAfresh)
{
	State state;
	state.manaPathStart = map_coords::MapCoords {.x = 5, .z = 6, .altitude = 0.0f};
	ProcessTurn(state, Outside({30.0f, 0.0f, 40.0f}, 5), k_Settings);
	EXPECT_TRUE(state.manaPathStart.has_value());
	ProcessTurn(state, Inside({30.0f, 0.0f, 40.0f}, 6), k_Settings);
	EXPECT_FALSE(state.manaPathStart.has_value());
}
