/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/HandStillness.h"
#include "Hand/GodHand.h"

using openblack::ecs::components::HandStillness;
namespace god_hand = openblack::god_hand;

// The citadel puts the hand of the god at the citadel. The input of the first turn moves it to the origin of the world,
// and that movement counts: the hand is not still and has a velocity.
TEST(GodHand, TheFirstInputMovesTheHandFromTheCitadelToTheOrigin)
{
	HandStillness hand;
	const glm::vec3 citadel {1000.0f, 20.0f, 2000.0f};
	god_hand::MindReport(hand, citadel);
	EXPECT_EQ(hand.last, citadel);
	god_hand::InputTurn(hand);
	EXPECT_EQ(hand.last, glm::vec3(0.0f));
	EXPECT_EQ(hand.stillTurns, 0u);
	EXPECT_FLOAT_EQ(hand.velocity.x, -citadel.x * 10.0f * 0.6f);
	EXPECT_FLOAT_EQ(hand.velocity.z, -citadel.z * 10.0f * 0.6f);
}

// A god with no mind and no citadel keeps its hand at the origin: each input counts one more still turn
TEST(GodHand, AHandThatStaysAtTheOriginIsStill)
{
	HandStillness hand;
	for (int turn = 0; turn < 3; ++turn)
	{
		god_hand::InputTurn(hand);
	}
	EXPECT_EQ(hand.last, glm::vec3(0.0f));
	EXPECT_EQ(hand.stillTurns, 3u);
	EXPECT_EQ(hand.velocity, glm::vec3(0.0f));
}

// In a turn with a step, the mind puts the hand where it was before the step. The next input moves it back to the
// origin, so a hand that a mind keeps away from the origin is never still.
TEST(GodHand, AHandThatTheMindKeepsAwayFromTheOriginIsNeverStill)
{
	HandStillness hand;
	const glm::vec3 place {500.0f, 30.0f, 600.0f};
	for (int turn = 0; turn < 5; ++turn)
	{
		god_hand::InputTurn(hand);
		god_hand::MindReport(hand, place);
		EXPECT_EQ(hand.last, place);
	}
	god_hand::InputTurn(hand);
	EXPECT_EQ(hand.stillTurns, 0u);
	EXPECT_EQ(hand.last, glm::vec3(0.0f));
	EXPECT_LT(hand.velocity.x, 0.0f);
}

// The report of the mind changes only the place. The stillness and the velocity change at the next input.
TEST(GodHand, TheReportOfTheMindChangesOnlyThePlace)
{
	HandStillness hand {
	    .stillTurns = 4, .drift = {0.1f, 0.0f, 0.0f}, .last = {0.0f, 0.0f, 0.0f}, .velocity = {1.0f, 0.0f, 0.0f}};
	god_hand::MindReport(hand, {7.0f, 8.0f, 9.0f});
	EXPECT_EQ(hand.stillTurns, 4u);
	EXPECT_EQ(hand.drift, glm::vec3(0.1f, 0.0f, 0.0f));
	EXPECT_EQ(hand.velocity, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(hand.last, glm::vec3(7.0f, 8.0f, 9.0f));
}

// A person's player has its reported hands. Every other god has one hand, also at the origin of the world.
TEST(GodHand, APersonHasTheReportedHandsAndEachOtherGodHasOneHand)
{
	const god_hand::Hands reported {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f)};
	EXPECT_EQ(god_hand::HandsOf(true, reported, glm::vec3(9.0f)), reported);
	const auto god = god_hand::HandsOf(false, reported, glm::vec3(0.0f));
	ASSERT_TRUE(god[0].has_value());
	EXPECT_EQ(*god[0], glm::vec3(0.0f));
	EXPECT_FALSE(god[1].has_value());
	// A player that the world does not have has no hands
	const auto none = god_hand::HandsOf(false, reported, std::nullopt);
	EXPECT_FALSE(none[0].has_value());
	EXPECT_FALSE(none[1].has_value());
}

// The nearest hand wins. Of two hands at the same distance, the first wins.
TEST(GodHand, TheNearestHandWinsAndTheFirstOfEqualHands)
{
	const glm::vec3 point {10.0f, 0.0f, 10.0f};
	const std::array<std::optional<glm::vec3>, 2> hands {glm::vec3(20.0f, 0.0f, 10.0f), glm::vec3(12.0f, 0.0f, 10.0f)};
	EXPECT_EQ(god_hand::NearestOf(hands, point), glm::vec3(12.0f, 0.0f, 10.0f));
	const std::array<std::optional<glm::vec3>, 2> equal {glm::vec3(15.0f, 0.0f, 10.0f), glm::vec3(5.0f, 0.0f, 10.0f)};
	EXPECT_EQ(god_hand::NearestOf(equal, point), glm::vec3(15.0f, 0.0f, 10.0f));
	const std::array<std::optional<glm::vec3>, 2> one {std::nullopt, glm::vec3(0.0f)};
	EXPECT_EQ(god_hand::NearestOf(one, point), glm::vec3(0.0f));
	EXPECT_FALSE(god_hand::NearestOf(std::array<std::optional<glm::vec3>, 2> {}, point).has_value());
}
