/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Sound/FarThunder.h"

using namespace openblack::audio::far_thunder;

namespace
{
/// The maximum distance of the thunder samples of the game's spell bank
constexpr float k_SampleReach = 300.0f;

/// The loudest value of a sine wave of a pitch, after the low-pass filter. The first part, where the filter starts,
/// does not count.
float FilteredPeak(float pitch, float cutOff, int sampleRate)
{
	std::vector<int16_t> samples(static_cast<size_t>(sampleRate));
	for (size_t i = 0; i < samples.size(); ++i)
	{
		const float time = static_cast<float>(i) / static_cast<float>(sampleRate);
		samples[i] = static_cast<int16_t>(10000.0f * std::sin(2.0f * std::numbers::pi_v<float> * pitch * time));
	}
	LowPass(samples, 1, sampleRate, cutOff);
	int peak = 0;
	for (size_t i = samples.size() / 2; i < samples.size(); ++i)
	{
		peak = std::max(peak, std::abs(static_cast<int>(samples[i])));
	}
	return static_cast<float>(peak);
}
} // namespace

// Up to the maximum distance of its sample, thunder has the volume of the game, and it is sharp
TEST(FarThunder, NearThunderIsAsTheGamePlaysIt)
{
	for (const float distance : {0.0f, 80.0f, 200.0f, k_SampleReach})
	{
		EXPECT_EQ(HearingAt(distance, k_SampleReach), (Hearing {.gain = 1.0f, .cutOff = std::nullopt})) << distance;
	}
}

// Past the maximum distance, thunder falls off as from a line to the channel length, then as from a point
TEST(FarThunder, FarThunderGetsQuieterAsRealThunderDoes)
{
	EXPECT_LT(HearingAt(301.0f, k_SampleReach).gain, 1.0f);
	EXPECT_GT(HearingAt(301.0f, k_SampleReach).gain, 0.99f);
	EXPECT_NEAR(HearingAt(1200.0f, k_SampleReach).gain, 0.5f, 1e-5f);
	const float atChannel = std::sqrt(k_SampleReach / k_ChannelLength);
	EXPECT_NEAR(HearingAt(k_ChannelLength, k_SampleReach).gain, atChannel, 1e-5f);
	EXPECT_NEAR(HearingAt(10000.0f, k_SampleReach).gain, atChannel * 0.5f, 1e-5f);
	float last = 1.0f;
	for (float distance = k_SampleReach; distance < k_SilentDistance; distance += 250.0f)
	{
		const float gain = HearingAt(distance, k_SampleReach).gain;
		EXPECT_LE(gain, last) << distance;
		last = gain;
	}
}

// From the fade distance the thunder goes quiet, and nobody hears it past the silent distance
TEST(FarThunder, ThunderGoesQuietFarAway)
{
	const float atFade = std::sqrt(k_SampleReach / k_ChannelLength) * k_ChannelLength / k_FadeDistance;
	EXPECT_NEAR(HearingAt(k_FadeDistance, k_SampleReach).gain, atFade, 1e-5f);
	EXPECT_NEAR(HearingAt(20000.0f, k_SampleReach).gain, atFade * 0.75f * 0.5f, 1e-5f);
	EXPECT_EQ(HearingAt(k_SilentDistance, k_SampleReach).gain, 0.0f);
	EXPECT_EQ(HearingAt(40000.0f, k_SampleReach).gain, 0.0f);
	EXPECT_GT(HearingAt(k_SilentDistance - 1.0f, k_SampleReach).gain, 0.0f);
}

// The whole land hears thunder, also from one corner to the other
TEST(FarThunder, TheWholeLandHearsThunder)
{
	const float side = static_cast<float>(openblack::map_coords::k_MapCells) * openblack::map_coords::k_CellSize;
	const float corners = std::sqrt(2.0f) * side;
	EXPECT_LT(corners, k_FadeDistance);
	EXPECT_GT(HearingAt(corners, k_SampleReach).gain, 0.1f);
}

// The air takes nothing up to the sharp distance. Past it, the cut-off falls quickly to the low pitches of a rumble.
TEST(FarThunder, TheCrackBecomesARumble)
{
	EXPECT_FALSE(HearingAt(k_SharpDistance, k_SampleReach).cutOff.has_value());
	// 30 m further, the air takes more than 3 dB only above 8 kHz
	EXPECT_FALSE(HearingAt(k_SharpDistance + 30.0f, k_SampleReach).cutOff.has_value());
	// 500 m further, it takes 6 dB a kilometre: between the 1 kHz and the 2 kHz bands
	const auto at2km = HearingAt(2000.0f, k_SampleReach).cutOff;
	ASSERT_TRUE(at2km.has_value());
	EXPECT_NEAR(*at2km, 1000.0f * std::pow(2.0f, std::log(6.0f / 5.0f) / std::log(9.0f / 5.0f)), 0.5f);
	float last = 20000.0f;
	for (float distance = 1600.0f; distance < k_SilentDistance; distance += 250.0f)
	{
		const auto cutOff = HearingAt(distance, k_SampleReach).cutOff;
		ASSERT_TRUE(cutOff.has_value()) << distance;
		EXPECT_LE(*cutOff, last) << distance;
		last = *cutOff;
	}
	// Far away, only the rumble below about 100 Hz is left
	EXPECT_LT(*HearingAt(7000.0f, k_SampleReach).cutOff, 200.0f);
	EXPECT_LT(last, 100.0f);
	EXPECT_GE(last, k_AirAbsorption.front().first);
}

// The pitch of a loss is the band's pitch at the band's loss, and between bands on a log scale
TEST(FarThunder, ThePitchOfALossComesFromTheTable)
{
	for (const auto& [pitch, loss] : k_AirAbsorption)
	{
		if (loss < k_AirAbsorption.back().second)
		{
			EXPECT_NEAR(*PitchOfAbsorption(loss), pitch, pitch * 1e-4f) << pitch;
		}
	}
	EXPECT_FALSE(PitchOfAbsorption(k_AirAbsorption.back().second).has_value());
	EXPECT_EQ(*PitchOfAbsorption(0.01f), k_AirAbsorption.front().first);
}

// The low-pass filter keeps a low pitch and takes a high pitch
TEST(FarThunder, TheFilterKeepsTheRumbleAndTakesTheCrack)
{
	constexpr int k_Rate = 22050;
	EXPECT_GT(FilteredPeak(60.0f, 500.0f, k_Rate), 9500.0f);
	// Five octaves above the cut-off, a filter of the second order takes about 60 dB
	EXPECT_LT(FilteredPeak(8000.0f, 250.0f, k_Rate), 30.0f);
	// At the cut-off, the filter takes 3 dB
	EXPECT_NEAR(FilteredPeak(1000.0f, 1000.0f, k_Rate), 10000.0f / std::numbers::sqrt2_v<float>, 150.0f);
}

// A cut-off at or above half the sample rate changes nothing
TEST(FarThunder, ACutOffAboveTheSampleRateChangesNothing)
{
	std::vector<int16_t> samples {100, -2000, 3000, 32767, -32768, 7};
	const auto before = samples;
	LowPass(samples, 1, 22050, 11025.0f);
	EXPECT_EQ(samples, before);
	LowPass(samples, 0, 22050, 100.0f);
	EXPECT_EQ(samples, before);
}

// Each channel of a stereo sample has its own filter
TEST(FarThunder, EachChannelHasItsOwnFilter)
{
	std::vector<int16_t> samples(2000);
	for (size_t i = 0; i < samples.size(); i += 2)
	{
		samples[i] = 10000;
	}
	LowPass(samples, 2, 22050, 200.0f);
	EXPECT_NEAR(samples[samples.size() - 2], 10000, 5);
	EXPECT_EQ(samples.back(), 0);
}
