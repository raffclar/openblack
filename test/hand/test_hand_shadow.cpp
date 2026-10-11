/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/GroundShadow.h"
#include "Graphics/HandShadow.h"

using openblack::DayNightTimes;
using openblack::graphics::GroundShadow;
using openblack::graphics::HandShadow;

namespace
{
constexpr DayNightTimes k_Times {.nightFull = 4.0f, .duskStart = 5.0f, .duskEnd = 6.0f, .dayFull = 7.0f};
} // namespace

TEST(HandShadow, SunCrossesTheSky)
{
	const auto midday = HandShadow::SunlightDirection(12.0f, k_Times);
	EXPECT_LT(midday.y, -0.8f);
	// The midday sun comes from the side of Black & White's light at (-500000, 500000, -500000)
	EXPECT_GT(midday.x, 0.0f);
	EXPECT_GT(midday.z, 0.0f);
	EXPECT_NEAR(midday.x, midday.z, 0.001f);

	const auto morning = HandShadow::SunlightDirection(7.0f, k_Times);
	const auto evening = HandShadow::SunlightDirection(17.0f, k_Times);
	EXPECT_GT(morning.y, midday.y);
	EXPECT_NEAR(morning.y, evening.y, 0.001f);
	// Morning and evening shadows point opposite ways
	EXPECT_LT(glm::dot(glm::vec2(morning.x, morning.z), glm::vec2(evening.x, evening.z)), 0.0f);
}

TEST(HandShadow, NoShadowAtNight)
{
	EXPECT_FLOAT_EQ(HandShadow::Daylight(2.0f, k_Times), 0.0f);
	EXPECT_FLOAT_EQ(HandShadow::Daylight(23.0f, k_Times), 0.0f);
	EXPECT_FLOAT_EQ(HandShadow::Daylight(5.5f, k_Times), 0.25f);
	EXPECT_FLOAT_EQ(HandShadow::Daylight(12.0f, k_Times), 1.0f);
	// The fade at night leaves the hand no shadow
	EXPECT_EQ(GroundShadow::ScaledAlpha(255.0f * HandShadow::Daylight(2.0f, k_Times)), 0);
}

TEST(HandShadow, IsCastFromFarUpTheSunlight)
{
	const glm::vec3 ground {100.0f, 10.0f, 100.0f};
	const auto light = HandShadow::LightPoint(ground, 9.0f, k_Times);
	const auto towards = glm::normalize(light - ground);
	EXPECT_NEAR(glm::dot(towards, -HandShadow::SunlightDirection(9.0f, k_Times)), 1.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(light, ground), glm::length(GroundShadow::k_Sun), 1.0f);
	// Laid on the ground beneath the hand, a point of it held 10 up falls 10 times the sun's slope away from the sun
	const GroundShadow::Cast cast {.light = light, .level = ground.y, .ground = ground.y};
	const auto direction = HandShadow::SunlightDirection(9.0f, k_Times);
	const auto held = GroundShadow::OnLevel({.position = ground + glm::vec3(0.0f, 10.0f, 0.0f), .keptAboveLevel = false}, cast);
	EXPECT_NEAR(held.x - ground.x, 10.0f * direction.x / -direction.y, 1e-2f);
	EXPECT_NEAR(held.y - ground.z, 10.0f * direction.z / -direction.y, 1e-2f);
}
