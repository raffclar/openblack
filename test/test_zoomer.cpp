/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>

#include <Common/Zoomer.h>
#include <gtest/gtest.h>

// The Zoomer (SetDestinationWithSpeedAndTime with the matrix inverse Inverse, and Update) and Zoomer3. The
// expected bits are those of the original's float operations in its order (single precision); test_camera's
// ZoomerMatchesRecording checks the same code against every zoomer recorded from the original game.

using openblack::Zoomer;
using openblack::Zoomer3;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

Zoomer StepFrom(float value, float speed, float destination, float destinationSpeed, float seconds)
{
	Zoomer zoomer;
	zoomer.value = value;
	zoomer.speed = speed;
	zoomer.SetDestinationWithSpeedAndTime(destination, destinationSpeed, seconds);
	return zoomer;
}
} // namespace

TEST(TestZoomer, SetPositionClearsEverything)
{
	Zoomer zoomer = StepFrom(1.0f, 2.0f, 5.0f, 1.0f, 1.0f);
	zoomer.Update(0.25f);
	zoomer.SetPosition(3.0f);
	EXPECT_EQ(zoomer.value, 3.0f);
	EXPECT_EQ(zoomer.destination, 3.0f);
	EXPECT_EQ(zoomer.startValue, 3.0f);
	EXPECT_EQ(zoomer.speed, 0.0f);
	EXPECT_EQ(zoomer.startSpeed, 0.0f);
	EXPECT_EQ(zoomer.destinationSpeed, 0.0f);
	EXPECT_EQ(zoomer.time, 0.0f);
	EXPECT_EQ(zoomer.duration, 0.0f);
	EXPECT_EQ(zoomer.c2, 0.0f);
	EXPECT_EQ(zoomer.c3, 0.0f);
	EXPECT_EQ(zoomer.c4, 0.0f);
	EXPECT_FALSE(zoomer.IsMoving());
}

TEST(TestZoomer, BelowAMillisecondJumps)
{
	// T < 0.001 (and an unordered compare) is SetPosition(destination)
	for (const float seconds : {0.0f, 0.000999f, -1.0f, std::numeric_limits<float>::quiet_NaN()})
	{
		Zoomer zoomer = StepFrom(1.0f, 2.0f, 7.0f, 4.0f, seconds);
		EXPECT_EQ(zoomer.value, 7.0f) << seconds;
		EXPECT_EQ(zoomer.speed, 0.0f) << seconds;
		EXPECT_EQ(zoomer.destinationSpeed, 0.0f) << seconds;
		EXPECT_EQ(zoomer.duration, 0.0f) << seconds;
	}
	Zoomer zoomer = StepFrom(1.0f, 0.0f, 7.0f, 0.0f, 0.001f);
	EXPECT_EQ(zoomer.duration, 0.001f); // 0.001 itself is a curve
	EXPECT_EQ(zoomer.value, 1.0f);
}

TEST(TestZoomer, StepFromRest)
{
	// 0 -> 10 in 2.5 s: c2, c3, c4 and the value at a quarter, a half and 0.99 of the time
	const Zoomer zoomer = StepFrom(0.0f, 0.0f, 10.0f, 0.0f, 2.5f);
	EXPECT_EQ(zoomer.startValue, 0.0f);
	EXPECT_EQ(zoomer.duration, 2.5f);
	EXPECT_EQ(zoomer.c2, 19.200000762939453f);
	EXPECT_EQ(zoomer.c3, -30.720003128051758f);
	EXPECT_EQ(zoomer.c4, 18.432003021240234f);
	const auto at = [&](float t) {
		Zoomer z = zoomer;
		z.Update(t);
		return z.value;
	};
	EXPECT_EQ(Bits(at(2.5f * 0.25f)), 0x40278000u); // 2.6171875
	EXPECT_EQ(Bits(at(2.5f * 0.5f)), 0x40DC0000u);  // 6.875
	EXPECT_EQ(Bits(at(2.5f * 0.99f)), 0x411FFFD4u); // 9.99995804
}

TEST(TestZoomer, ShortStepsHitTheDeterminantClamp)
{
	// det(M) = -T^6 / 144 goes under 1e-10 below T = 0.0493 s: Inverse clamps it to -1e-10 and the curve barely
	// moves before Update snaps it to the destination
	const auto at = [](float seconds, float fraction) {
		Zoomer z = StepFrom(0.0f, 0.0f, 10.0f, 0.0f, seconds);
		z.Update(seconds * fraction);
		return z.value;
	};
	EXPECT_EQ(Bits(at(0.05f, 0.25f)), 0x40277FF8u); // 2.61718559: above the clamp, the same curve
	EXPECT_EQ(Bits(at(0.04f, 0.25f)), 0x3F3E93E6u); // 0.744444251, not 2.617
	EXPECT_EQ(Bits(at(0.04f, 0.5f)), 0x3FFA4FA0u);  // 1.95555496
	EXPECT_EQ(Bits(at(0.04f, 0.99f)), 0x40360B28u); // 2.84443092
	EXPECT_EQ(Bits(at(0.02f, 0.25f)), 0x3C3E93E6u); // 0.0116319414
	const Zoomer clamped = StepFrom(0.0f, 0.0f, 10.0f, 0.0f, 0.04f);
	EXPECT_EQ(clamped.c4, 80000000.0f);
	Zoomer z = clamped;
	z.Update(0.04f); // t >= duration: the destination
	EXPECT_EQ(z.value, 10.0f);
	EXPECT_EQ(z.time, 0.04f);
}

TEST(TestZoomer, StartAndEndSpeeds)
{
	// 2 -> 7 in 1.3 s, from 3.5 m/s to 0.4 m/s, at 0.65 s
	Zoomer zoomer = StepFrom(2.0f, 3.5f, 7.0f, 0.4f, 1.3f);
	EXPECT_EQ(zoomer.startSpeed, 3.5f);
	EXPECT_EQ(zoomer.c2, 17.503026962280273f);
	EXPECT_EQ(zoomer.c3, -64.8614273071289f);
	EXPECT_EQ(zoomer.c4, 79.07317352294922f);
	zoomer.Update(0.65f);
	EXPECT_EQ(zoomer.value, 5.591879844665527f);
	EXPECT_EQ(zoomer.speed, 4.79423713684082f);
	EXPECT_TRUE(zoomer.IsMoving());
	zoomer.Update(0.65f); // 0.65 + 0.65 = 1.3: at the duration, not past it
	EXPECT_EQ(zoomer.value, 7.0f);
	EXPECT_EQ(zoomer.speed, 0.4f);
	EXPECT_EQ(zoomer.time, 1.3f);
	EXPECT_FALSE(zoomer.IsMoving());
	zoomer.Update(1.0f); // no extrapolation past the end
	EXPECT_EQ(zoomer.value, 7.0f);
	EXPECT_EQ(zoomer.time, 1.3f);
}

TEST(TestZoomer, RecordedCameraSteps)
{
	// Two frames of the player's camera recorded from the original (its x): 1000 -> -368.455078 in 1.3968 s,
	// 0.1 s later a new destination -769.054199 in 1.3616 s from the current value and speed, then 0.001 s
	Zoomer zoomer = StepFrom(1000.0f, 0.0f, -368.455078f, 0.0f, 1.39680004f);
	zoomer.Update(0.1f);
	EXPECT_EQ(zoomer.value, 961.82568359375f);
	EXPECT_EQ(zoomer.speed, -725.4724731445312f);
	zoomer.SetDestinationWithSpeedAndTime(-769.054199f, 0.0f, 1.36159992f);
	EXPECT_EQ(zoomer.startValue, 961.82568359375f);
	EXPECT_EQ(zoomer.startSpeed, -725.4724731445312f);
	zoomer.Update(0.001f);
	EXPECT_EQ(zoomer.value, 961.09619140625f);
	EXPECT_EQ(zoomer.speed, -733.466064453125f);
}

TEST(TestZoomer, Zoomer3dAxes)
{
	// SetDestinationWithTime: the destination speed is 0 on the three axes
	Zoomer3 zoomer;
	zoomer.SetPosition({0.0f, 2.0f, 1000.0f});
	EXPECT_EQ(zoomer.GetCurrentValue(), glm::vec3(0.0f, 2.0f, 1000.0f));
	zoomer.SetDestinationWithTime({10.0f, 7.0f, -368.455078f}, 2.5f);
	EXPECT_EQ(zoomer.GetDestination(), glm::vec3(10.0f, 7.0f, -368.455078f));
	EXPECT_EQ(zoomer.GetStartValue(), glm::vec3(0.0f, 2.0f, 1000.0f));
	EXPECT_EQ(zoomer.GetDestinationSpeed(), glm::vec3(0.0f));
	zoomer.Update(2.5f * 0.25f);
	EXPECT_EQ(Bits(zoomer.GetCurrentValue().x), 0x40278000u);
	const Zoomer y = [] {
		Zoomer z = StepFrom(2.0f, 0.0f, 7.0f, 0.0f, 2.5f);
		z.Update(2.5f * 0.25f);
		return z;
	}();
	EXPECT_EQ(zoomer.GetCurrentValue().y, y.value);
	EXPECT_EQ(zoomer.GetSpeed().y, y.speed);
}
