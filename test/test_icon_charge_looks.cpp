/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Magic/IconChargeLooks.h"

using namespace openblack::magic::icon_charge_looks;

namespace
{
/// The red player's colour, without its alpha
constexpr uint32_t k_Red = 0xFF4646u;
} // namespace

TEST(IconChargeLooks, TheClockGoesRoundOnceInThreePointThreeThreeSeconds)
{
	const auto second = Advance({}, 1000);
	EXPECT_FLOAT_EQ(second.phase, 1.0f / 3.33f);
	EXPECT_EQ(second.wraps, 0u);
	// It goes back to the start when it gets past 1
	const auto wrapped = Advance({.phase = 0.9f, .wraps = 4}, 1000);
	EXPECT_NEAR(wrapped.phase, 0.9f + (1.0f / 3.33f) - 1.0f, 1e-6f);
	EXPECT_EQ(wrapped.wraps, 5u);
	// A paused game gives the clock no time
	EXPECT_FLOAT_EQ(Advance({.phase = 0.4f}, 0).phase, 0.4f);
}

TEST(IconChargeLooks, TheSecondsSinceAMarkCountFromTheMarkRoundTheClock)
{
	EXPECT_NEAR(SecondsSince(0.5f, 0.1f), 0.4f * 3.33f, 1e-5f);
	// At the mark or before it, the clock last went past the mark one round before
	EXPECT_NEAR(SecondsSince(0.05f, 0.1f), 0.95f * 3.33f, 1e-5f);
	EXPECT_NEAR(SecondsSince(0.1f, 0.1f), 3.33f, 1e-5f);
}

TEST(IconChargeLooks, TheRingHasTenPointsRoundTheIconsFootTheLastOnTheFirst)
{
	const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 6.0f, 7.0f));
	const auto points = RingPoints(model);
	ASSERT_EQ(points.size(), k_RingPoints);
	EXPECT_FLOAT_EQ(points.front().x, 6.0f);
	EXPECT_FLOAT_EQ(points.front().y, 6.0f);
	EXPECT_FLOAT_EQ(points.front().z, 7.0f);
	EXPECT_NEAR(points.back().x, points.front().x, 1e-5f);
	EXPECT_NEAR(points.back().z, points.front().z, 1e-5f);
	// The third point is two ninths of the way round, from the first axis to the third
	const float angle = 2.0f * 6.2831855f / 9.0f;
	EXPECT_NEAR(points[2].x, 5.0f + std::cos(angle), 1e-5f);
	EXPECT_NEAR(points[2].z, 7.0f + std::sin(angle), 1e-5f);
	for (const auto& point : points)
	{
		EXPECT_FLOAT_EQ(point.y, 6.0f);
	}
}

TEST(IconChargeLooks, TheRingShowsTheStoreAndAHeldSeedOrAFullStoreThrobs)
{
	const auto held = ShownCharge({.seedHeld = true, .store = 0.0f, .required = 1000.0f});
	EXPECT_FLOAT_EQ(held.fraction, 1.0f);
	EXPECT_TRUE(held.throbs);
	const auto half = ShownCharge({.store = 500.0f, .required = 1000.0f});
	EXPECT_FLOAT_EQ(half.fraction, 0.5f);
	EXPECT_FALSE(half.throbs);
	const auto full = ShownCharge({.store = 1000.0f, .required = 1000.0f});
	EXPECT_FLOAT_EQ(full.fraction, 1.0f);
	EXPECT_TRUE(full.throbs);
	const auto empty = ShownCharge({.store = 0.0f, .required = 1000.0f});
	EXPECT_FLOAT_EQ(empty.fraction, 0.0f);
	EXPECT_FALSE(empty.throbs);
}

TEST(IconChargeLooks, WhileTheIconChargesTheRingStartsAtASixth)
{
	EXPECT_FLOAT_EQ(ShownCharge({.required = 1000.0f, .charging = true}).fraction, 0.2f / 1.2f);
	EXPECT_FLOAT_EQ(ShownCharge({.store = 1000.0f, .required = 1000.0f, .charging = true}).fraction, 1.0f);
}

TEST(IconChargeLooks, TheChargeSecondsCountTheTurnsAndThePartOfThisTurn)
{
	EXPECT_FLOAT_EQ(ChargeSeconds(15, 10, 0.5f), 0.55f);
	EXPECT_FLOAT_EQ(ChargeSeconds(10, 10, 0.0f), 0.0f);
}

TEST(IconChargeLooks, AThrobShinesFromATenthToSixTenthsOfThePlayersColour)
{
	// At the start of the clock the throb is half way: 89 of 256
	EXPECT_EQ(Tint(k_Red, {.fraction = 1.0f, .throbs = true}, false, 0.0f, 0.0f), 0x581818u);
	// A throb comes before the flash of a charge
	EXPECT_EQ(Tint(k_Red, {.fraction = 1.0f, .throbs = true}, true, 0.0f, 0.0f), 0x581818u);
	// At the top of the throb: 153 of 256
	const float top = 0.25f / 2.0f;
	EXPECT_EQ(Tint(k_Red, {.throbs = true}, false, top, 0.0f), 0x982929u);
}

TEST(IconChargeLooks, AChargeFlashesAndThenStaysAtFiftyOf256)
{
	const Shown shown {.fraction = 0.5f};
	EXPECT_EQ(Tint(k_Red, shown, true, 0.0f, 0.0f), 0x310D0Du);
	EXPECT_EQ(Tint(k_Red, shown, true, 0.0f, 1.5f), 0x310D0Du);
	EXPECT_EQ(Tint(k_Red, shown, true, 0.0f, 20.0f), 0x310D0Du);
	// The flash is at its top when the cosine is -1
	const float flashTop = 3.14159265f / 8.0f;
	EXPECT_EQ(Tint(k_Red, shown, true, 0.0f, flashTop), 0xC83636u);
	// An icon that does not charge or throb adds no colour
	EXPECT_EQ(Tint(k_Red, shown, false, 0.3f, 0.0f), 0u);
}

TEST(IconChargeLooks, ThePulseLastsOneSecondInEachRoundOfTheClock)
{
	EXPECT_FALSE(PulseShare(0.1f).has_value());
	EXPECT_FALSE(PulseShare(0.05f).has_value());
	EXPECT_FALSE(PulseShare(0.5f).has_value());
	ASSERT_TRUE(PulseShare(0.2f).has_value());
	EXPECT_NEAR(*PulseShare(0.2f), 0.1f * 3.33f, 1e-5f);
	ASSERT_TRUE(PulseShare(0.4f).has_value());
	EXPECT_NEAR(*PulseShare(0.4f), 0.3f * 3.33f, 1e-5f);
}

TEST(IconChargeLooks, ThePulseModelShrinksIntoTheIconAndBecomesOpaque)
{
	auto icon = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 6.0f, 7.0f));
	icon = glm::scale(icon, glm::vec3(2.0f));
	const auto start = PulseModel(icon, 2.0f, 0.0f);
	EXPECT_FLOAT_EQ(start[0][0], 6.0f);
	EXPECT_FLOAT_EQ(start[1][1], 6.0f);
	EXPECT_FLOAT_EQ(start[2][2], 6.0f);
	// It stands a tenth of the model's height up the icon's up axis, scale included
	EXPECT_FLOAT_EQ(start[3][0], 5.0f);
	EXPECT_FLOAT_EQ(start[3][1], 6.4f);
	EXPECT_FLOAT_EQ(start[3][2], 7.0f);
	EXPECT_FLOAT_EQ(start[3][3], 1.0f);
	const auto end = PulseModel(icon, 2.0f, 1.0f);
	EXPECT_FLOAT_EQ(end[0][0], 0.0f);
	EXPECT_FLOAT_EQ(end[3][1], 6.4f);
	EXPECT_EQ(PulseAlpha(0.0f), 0);
	EXPECT_EQ(PulseAlpha(0.5f), 127);
	EXPECT_EQ(PulseAlpha(1.0f), 255);
}
