/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Device/RampedStops.h"

// The ramped stops that fade out in the background: four gain steps of 5 ms down to silence, then the flush, with the
// channel free from the start. A fake target records what the fades do to their sources; the time is given by hand.

using namespace openblack::audio;
using namespace std::chrono_literals;

namespace
{
using Clock = RampedStops::Clock;

/// What the fades did, in order
class FakeTarget final: public RampedStops::Target
{
public:
	std::vector<std::pair<RampedStops::SourceId, float>> gains;
	std::vector<RampedStops::SourceId> flushed;

	void SetGain(RampedStops::SourceId source, float gain) override { gains.emplace_back(source, gain); }
	void Flush(RampedStops::SourceId source) override { flushed.push_back(source); }
};

const Clock::time_point k_T0 {};
} // namespace

TEST(RampedStops, BeginReturnsWithoutWaiting)
{
	RampedStops stops;
	FakeTarget target;
	const auto before = Clock::now();
	stops.Begin(3, 7, 0.8f, Clock::now(), target);
	// the first step only: nothing waits for the rest of the fade
	EXPECT_LT(Clock::now() - before, 5ms);
	ASSERT_EQ(target.gains.size(), 1u);
	EXPECT_EQ(target.gains[0].first, 7u);
	EXPECT_FLOAT_EQ(target.gains[0].second, 0.6f);
	EXPECT_TRUE(target.flushed.empty());
	EXPECT_EQ(stops.Fading(), 1u);
}

TEST(RampedStops, FadeReachesZeroThenFlushes)
{
	RampedStops stops;
	FakeTarget target;
	stops.Begin(0, 5, 1.0f, k_T0, target);
	EXPECT_EQ(stops.Advance(k_T0 + 4ms, target), k_T0 + 5ms);
	EXPECT_EQ(target.gains.size(), 1u);
	EXPECT_EQ(stops.Advance(k_T0 + 5ms, target), k_T0 + 10ms);
	EXPECT_EQ(stops.Advance(k_T0 + 10ms, target), k_T0 + 15ms);
	EXPECT_EQ(stops.Advance(k_T0 + 15ms, target), k_T0 + 20ms);
	// the same shape as the stop that waited: 3/4, 1/2, 1/4, then silence for the last 5 ms
	const std::vector<std::pair<RampedStops::SourceId, float>> expected {{5, 0.75f}, {5, 0.5f}, {5, 0.25f}, {5, 0.0f}};
	EXPECT_EQ(target.gains, expected);
	EXPECT_TRUE(target.flushed.empty());
	EXPECT_EQ(stops.Advance(k_T0 + 19ms, target), k_T0 + 20ms);
	EXPECT_TRUE(target.flushed.empty());
	// 20 ms: flushed, and the source is a spare
	EXPECT_FALSE(stops.Advance(k_T0 + 20ms, target).has_value());
	EXPECT_EQ(target.flushed, std::vector<RampedStops::SourceId> {5});
	EXPECT_EQ(stops.Fading(), 0u);
	EXPECT_EQ(stops.TakeSpare(), 5u);
	EXPECT_FALSE(stops.TakeSpare().has_value());
}

TEST(RampedStops, LateAdvanceJumpsToTheDueStep)
{
	RampedStops stops;
	FakeTarget target;
	stops.Begin(0, 5, 1.0f, k_T0, target);
	stops.Advance(k_T0 + 12ms, target);
	ASSERT_EQ(target.gains.size(), 2u);
	EXPECT_FLOAT_EQ(target.gains[1].second, 0.25f);
	// past the 20 ms: straight to the flush
	stops.Advance(k_T0 + 40ms, target);
	EXPECT_EQ(target.gains.size(), 2u);
	EXPECT_EQ(target.flushed, std::vector<RampedStops::SourceId> {5});
}

TEST(RampedStops, RestartDuringTheFade)
{
	RampedStops stops;
	FakeTarget target;
	stops.Begin(2, 9, 1.0f, k_T0, target);
	// the channel starts again at once: the fading source is not given out, the channel takes a new one
	EXPECT_FALSE(stops.TakeSpare().has_value());
	constexpr RampedStops::SourceId k_NewSource = 10;
	// the fade goes on to its end and touches its own source only, so the new sound neither fades nor is cut
	stops.Advance(k_T0 + 5ms, target);
	stops.Advance(k_T0 + 15ms, target);
	stops.Advance(k_T0 + 20ms, target);
	for (const auto& step : target.gains)
	{
		EXPECT_NE(step.first, k_NewSource);
	}
	EXPECT_EQ(target.flushed, std::vector<RampedStops::SourceId> {9});
	// a second stop of the channel during the first fade fades on its own
	RampedStops twice;
	FakeTarget other;
	twice.Begin(2, 9, 1.0f, k_T0, other);
	twice.Begin(2, k_NewSource, 0.5f, k_T0 + 8ms, other);
	EXPECT_EQ(twice.Advance(k_T0 + 20ms, other), k_T0 + 23ms);
	EXPECT_EQ(other.flushed, std::vector<RampedStops::SourceId> {9});
	EXPECT_FLOAT_EQ(other.gains.back().second, 0.125f); // its third step
	EXPECT_EQ(twice.Fading(), 1u);
}

TEST(RampedStops, CutDuringTheFadeFlushesAtOnce)
{
	RampedStops stops;
	FakeTarget target;
	stops.Begin(1, 4, 1.0f, k_T0, target);
	stops.Begin(6, 8, 1.0f, k_T0, target);
	// StopAll: the channel's fade is flushed now, the other one goes on
	stops.Cut(1, target);
	EXPECT_EQ(target.flushed, std::vector<RampedStops::SourceId> {4});
	EXPECT_EQ(stops.Fading(), 1u);
	stops.Cut(3, target);
	EXPECT_EQ(stops.Fading(), 1u);
	stops.Advance(k_T0 + 5ms, target);
	EXPECT_EQ(target.gains.back(), (std::pair<RampedStops::SourceId, float> {8, 0.5f}));
	EXPECT_EQ(stops.TakeSpare(), 4u);
}

TEST(RampedStops, TakeAllHandsBackEverySource)
{
	RampedStops stops;
	FakeTarget target;
	stops.Begin(0, 1, 1.0f, k_T0, target);
	stops.Begin(1, 2, 1.0f, k_T0, target);
	stops.Advance(k_T0 + 20ms, target);
	stops.Begin(2, 3, 1.0f, k_T0 + 20ms, target);
	auto all = stops.TakeAll();
	std::ranges::sort(all);
	EXPECT_EQ(all, (std::vector<RampedStops::SourceId> {1, 2, 3}));
	EXPECT_EQ(stops.Fading(), 0u);
	EXPECT_EQ(stops.Spares(), 0u);
	// the fade taken back is not flushed by the fades any more (the owner deletes it)
	EXPECT_EQ(target.flushed.size(), 2u);
	EXPECT_FALSE(stops.Advance(k_T0 + 40ms, target).has_value());
}
