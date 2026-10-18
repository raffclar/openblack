/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/CloudLightning.h"

namespace lightning = openblack::graphics::cloud_lightning;

namespace
{

// The number of separate pulses: the times where the light starts to rise again after it fell
uint32_t CountRises(uint32_t seed)
{
	uint32_t rises = 0;
	float before = 0.0f;
	bool falling = true;
	for (float t = 0.0f; t < 1.5f; t += 0.001f)
	{
		const float now = lightning::FlickerEnvelope(t, seed);
		if (now > before + 1e-4f && falling)
		{
			++rises;
			falling = false;
		}
		else if (now < before - 1e-4f)
		{
			falling = true;
		}
		before = now;
	}
	return rises;
}

} // namespace

TEST(CloudLightning, FlickerIsDarkBeforeTheStrikeAndAfterTheLastPulse)
{
	for (uint32_t seed = 1; seed < 50; ++seed)
	{
		EXPECT_EQ(lightning::FlickerEnvelope(-0.01f, seed), 0.0f);
		const float end = lightning::FlickerSeconds(seed) + 4.0f * lightning::k_PulseFade;
		EXPECT_EQ(lightning::FlickerEnvelope(end + 0.01f, seed), 0.0f);
	}
}

TEST(CloudLightning, FirstPulseIsFullAndQuick)
{
	for (uint32_t seed = 1; seed < 50; ++seed)
	{
		EXPECT_NEAR(lightning::FlickerEnvelope(lightning::k_PulseRise, seed), 1.0f, 0.2f);
		for (float t = 0.0f; t < 1.5f; t += 0.01f)
		{
			const float value = lightning::FlickerEnvelope(t, seed);
			EXPECT_GE(value, 0.0f);
			EXPECT_LE(value, 1.0f);
		}
	}
}

TEST(CloudLightning, FlickerHasSeveralPulsesInItsTime)
{
	for (uint32_t seed = 1; seed < 50; ++seed)
	{
		const float seconds = lightning::FlickerSeconds(seed);
		EXPECT_GE(seconds, lightning::k_ShortestFlicker);
		EXPECT_LE(seconds, lightning::k_LongestFlicker);
		const uint32_t pulses = lightning::PulseCount(seed);
		EXPECT_GE(pulses, lightning::k_FewestPulses);
		EXPECT_LE(pulses, lightning::k_MostPulses);
		// Two pulses can start at almost the same time and show as one
		EXPECT_GE(CountRises(seed), 2u);
		EXPECT_LE(CountRises(seed), pulses);
	}
}

TEST(CloudLightning, FlickerIsTheSameForTheSameSeed)
{
	EXPECT_EQ(lightning::FlickerEnvelope(0.3f, 77u), lightning::FlickerEnvelope(0.3f, 77u));
	EXPECT_EQ(lightning::SeedAt({120.0f, -40.0f}), lightning::SeedAt({120.4f, -39.6f}));
	EXPECT_NE(lightning::SeedAt({120.0f, -40.0f}), lightning::SeedAt({-40.0f, 120.0f}));
}

TEST(CloudLightning, FarFlashesAreSheetLightning)
{
	EXPECT_EQ(lightning::KindAt(1000.0f), lightning::FlashKind::Bolt);
	EXPECT_EQ(lightning::KindAt(lightning::k_SheetDistance + 1.0f), lightning::FlashKind::Sheet);

	const lightning::FlashEvent flash {
	    .position = {0.0f, 0.0f}, .place = {0.0f, 0.0f}, .since = 0.008f, .strength = 1.0f, .seed = 5u};
	const auto near = lightning::LightFor(flash, {0.0f, 40.0f, 500.0f}, 1000.0f, 9400.0f);
	const auto far = lightning::LightFor(flash, {0.0f, 40.0f, 20000.0f}, 1000.0f, 9400.0f);
	EXPECT_EQ(near.radius, lightning::k_BoltRadius);
	EXPECT_EQ(far.radius, lightning::k_SheetRadius);
	EXPECT_NEAR(far.brightness, near.brightness * lightning::k_SheetShare, 1e-5f);
}

TEST(CloudLightning, ChannelRisesInsideTheTowerFromItsBottom)
{
	for (uint32_t seed = 1; seed < 50; ++seed)
	{
		const lightning::FlashEvent flash {
		    .position = {10.0f, 20.0f}, .place = {10.0f, 20.0f}, .since = 0.0f, .strength = 1.0f, .seed = seed};
		const auto light = lightning::LightFor(flash, {0.0f, 40.0f, 0.0f}, 1000.0f, 9400.0f);
		// The channel starts at the bottom of the tower, over the place of the flash
		EXPECT_FLOAT_EQ(light.start.x, 10.0f);
		EXPECT_FLOAT_EQ(light.start.z, 20.0f);
		EXPECT_GT(light.start.y, 1000.0f);
		EXPECT_LE(light.start.y, 1150.0f);
		// It goes up through the tower, and stays under its top
		const glm::vec3 along = light.end - light.start;
		EXPECT_GE(glm::length(along), lightning::k_ShortestChannel - 1e-2f);
		EXPECT_LE(glm::length(along), lightning::k_LongestChannel + 1e-2f);
		EXPECT_LT(light.end.y, 1000.0f + 9400.0f);
		EXPECT_LE(glm::length(glm::vec2(along.x, along.z)), along.y * lightning::k_MostLean + 1e-2f);
	}
}

TEST(CloudLightning, ChannelStaysInsideAShallowCloud)
{
	const lightning::FlashEvent flash {
	    .position = {0.0f, 0.0f}, .place = {0.0f, 0.0f}, .since = 0.0f, .strength = 1.0f, .seed = 3u};
	const auto light = lightning::LightFor(flash, {0.0f, 40.0f, 0.0f}, 300.0f, 1500.0f);
	EXPECT_GT(light.start.y, 300.0f);
	EXPECT_LT(light.end.y, 1800.0f);
}

TEST(CloudLightning, StrikeAwayFromTheStormLightsTheStormsCloud)
{
	const lightning::Strike inside {
	    .position = {100.0f, 0.0f}, .time = 0.0f, .strength = 1.0f, .stormCentre = {0.0f, 0.0f}, .stormRadius = 1000.0f};
	EXPECT_EQ(lightning::FlashPlace(inside, 3u), inside.position);
	const lightning::Strike outside {
	    .position = {5000.0f, 0.0f}, .time = 0.0f, .strength = 1.0f, .stormCentre = {0.0f, 0.0f}, .stormRadius = 1000.0f};
	const auto place = lightning::FlashPlace(outside, 3u);
	EXPECT_GT(place.x, 0.0f);
	EXPECT_NEAR(place.y, 0.0f, 1e-3f);
	EXPECT_LE(glm::length(place), 800.0f);
}

TEST(CloudLightning, TrackKeepsSmoothTimeAndStartsNewFlashes)
{
	lightning::FlashEvents events;
	std::vector<lightning::Strike> strikes {{.position = {0.0f, 0.0f}, .time = 0.0f, .strength = 1.0f}};
	events = lightning::TrackFlashes(events, strikes, 0.01f);
	ASSERT_EQ(events.count, 1u);
	EXPECT_FLOAT_EQ(events.events[0].since, 0.0f);
	// The storm's time steps by turns, but the flash's own time advances each frame
	events = lightning::TrackFlashes(events, strikes, 0.01f);
	events = lightning::TrackFlashes(events, strikes, 0.01f);
	ASSERT_EQ(events.count, 1u);
	EXPECT_NEAR(events.events[0].since, 0.02f, 1e-6f);
	// A younger strike at the same place is a new flash
	strikes[0].time = 0.1f;
	events = lightning::TrackFlashes(events, strikes, 0.01f);
	strikes[0].time = 0.0f;
	events = lightning::TrackFlashes(events, strikes, 0.01f);
	ASSERT_EQ(events.count, 1u);
	EXPECT_FLOAT_EQ(events.events[0].since, 0.0f);
	// With no strikes, the flash goes once its time is past
	events = lightning::TrackFlashes(events, {}, lightning::k_FlashMemory + 0.1f);
	EXPECT_EQ(events.count, 0u);
}

TEST(CloudLightning, TrackReplacesTheOldestWhenFull)
{
	lightning::FlashEvents events;
	for (size_t i = 0; i < lightning::k_MaxFlashes; ++i)
	{
		const std::array<lightning::Strike, 1> strike {
		    {{.position = {static_cast<float>(i) * 100.0f, 0.0f}, .time = 0.0f, .strength = 1.0f}}};
		events = lightning::TrackFlashes(events, strike, 0.05f);
	}
	ASSERT_EQ(events.count, lightning::k_MaxFlashes);
	const std::array<lightning::Strike, 1> late {{{.position = {9000.0f, 0.0f}, .time = 0.0f, .strength = 1.0f}}};
	events = lightning::TrackFlashes(events, late, 0.05f);
	ASSERT_EQ(events.count, lightning::k_MaxFlashes);
	bool first = false;
	bool newest = false;
	for (const auto& flash : events.All())
	{
		first = first || flash.position.x == 0.0f;
		newest = newest || flash.position.x == 9000.0f;
	}
	EXPECT_FALSE(first);
	EXPECT_TRUE(newest);
}

TEST(CloudLightning, PackPutsStartsThenEnds)
{
	const std::array<lightning::FlashLight, 1> lights {
	    {{.start = {1.0f, 2.0f, 3.0f}, .end = {4.0f, 5.0f, 6.0f}, .brightness = 0.5f, .radius = 420.0f}}};
	const auto packed = lightning::Pack(lights);
	EXPECT_EQ(packed[0], glm::vec4(1.0f, 2.0f, 3.0f, 0.5f));
	EXPECT_EQ(packed[lightning::k_MaxFlashes], glm::vec4(4.0f, 5.0f, 6.0f, 420.0f));
	EXPECT_EQ(packed[1].w, 0.0f);
}
