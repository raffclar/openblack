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
#include <set>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Common/RandomNumberManager.h"

#define LOCATOR_IMPLEMENTATIONS
#include <Common/RandomNumberManagerProduction.h>
#include <Common/RandomNumberManagerTesting.h>
#include <ECS/Systems/Implementations/TimeSystem.h>

using namespace openblack;
using openblack::ecs::systems::TimeSystem;

namespace
{
constexpr auto k_Streams = static_cast<size_t>(RandomStream::Count);

std::vector<uint32_t> Draws(std::mt19937& generator, int count = 8)
{
	std::vector<uint32_t> draws;
	for (int i = 0; i < count; ++i)
	{
		draws.push_back(generator());
	}
	return draws;
}

std::vector<int> GeneralDraws(RandomNumberManagerInterface& rng, int count = 8)
{
	std::vector<int> draws;
	for (int i = 0; i < count; ++i)
	{
		draws.push_back(rng.NextValue(0, 1'000'000));
	}
	return draws;
}
} // namespace

// Every stream starts from its own seed, made from the run's: the same for the same run seed, different between streams
// and between close run seeds
TEST(RunSeed, EachStreamHasItsOwnSeedFromTheRunsSeed)
{
	std::set<uint32_t> seeds;
	for (size_t stream = 0; stream < k_Streams; ++stream)
	{
		seeds.insert(run_seed::StreamSeed(42, static_cast<RandomStream>(stream)));
		seeds.insert(run_seed::StreamSeed(43, static_cast<RandomStream>(stream)));
	}
	EXPECT_EQ(seeds.size(), 2 * k_Streams);
	static_assert(run_seed::StreamSeed(42, RandomStream::CreatureMind) == run_seed::StreamSeed(42, RandomStream::CreatureMind));
}

// The same seed gives the same draws in every stream and in the general draws, however much was drawn before
TEST(RunSeed, TheSameSeedDrawsTheSameNumbers)
{
	RandomNumberManagerProduction rng;
	rng.SetRunSeed(42);
	EXPECT_EQ(rng.GetRunSeed(), 42u);
	std::array<std::vector<uint32_t>, k_Streams> first;
	for (size_t stream = 0; stream < k_Streams; ++stream)
	{
		first.at(stream) = Draws(rng.Stream(static_cast<RandomStream>(stream)));
	}
	const auto general = GeneralDraws(rng);

	// Drawn differently meanwhile, then seeded again
	Draws(rng.Stream(RandomStream::CreatureFight), 100);
	GeneralDraws(rng, 7);
	rng.SetRunSeed(42);
	for (size_t stream = 0; stream < k_Streams; ++stream)
	{
		EXPECT_EQ(Draws(rng.Stream(static_cast<RandomStream>(stream))), first.at(stream)) << stream;
	}
	EXPECT_EQ(GeneralDraws(rng), general);

	// Streams don't share their draws: one drawing more doesn't move another
	EXPECT_NE(first.at(static_cast<size_t>(RandomStream::CreatureMind)),
	          first.at(static_cast<size_t>(RandomStream::CreatureFight)));
	rng.SetRunSeed(43);
	EXPECT_NE(GeneralDraws(rng), general);
}

// The general draws of another thread start again from the run's seed too, once it is seeded
TEST(RunSeed, OtherThreadsGeneralDrawsFollowTheSeed)
{
	RandomNumberManagerProduction rng;
	rng.SetRunSeed(7);
	std::vector<int> before;
	std::thread([&rng, &before] { before = GeneralDraws(rng); }).join();
	rng.SetRunSeed(7);
	std::vector<int> after;
	std::thread([&rng, &after] { after = GeneralDraws(rng); }).join();
	EXPECT_EQ(before, after);
	EXPECT_EQ(after, GeneralDraws(rng));
}

// The testing manager seeds alike
TEST(RunSeed, TheTestingManagerSeedsAlike)
{
	RandomNumberManagerTesting testing;
	RandomNumberManagerProduction production;
	testing.SetRunSeed(9);
	production.SetRunSeed(9);
	EXPECT_EQ(Draws(testing.Stream(RandomStream::CreatureMind)), Draws(production.Stream(RandomStream::CreatureMind)));
	EXPECT_EQ(GeneralDraws(testing), GeneralDraws(production));
}

// Without the game's random numbers (a system made alone in a test) a stream draws from a generator of its own on the
// standard seed, so the same each time
TEST(RunSeed, AStreamSourceWithoutTheGameDrawsTheStandardSequence)
{
	RandomStreamSource source(RandomStream::CreatureLocomotion);
	std::mt19937 standard;
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_EQ(source(), standard());
	}
}

namespace
{
struct Clock
{
	uint32_t now = 123456;
	TimeSystem time {[this] { return now; }};
};
} // namespace

// The machine's ticks as the game reads them count from the clock's start, and from 0 again when restarted
TEST(RunSeed, TicksCountFromTheStartAndARestart)
{
	Clock clock;
	clock.time.Start();
	EXPECT_EQ(clock.time.GetTicks(), 0u);
	clock.now += 250;
	EXPECT_EQ(clock.time.GetTicks(), 250u);
	clock.time.RestartClock(std::nullopt);
	EXPECT_EQ(clock.time.GetTicks(), 0u);
	EXPECT_FALSE(clock.time.GetPinnedDate().has_value());
}

// With a fixed frame time the ticks move on by it each frame, whatever the wall clock does, so that two runs read the
// same ticks at the same frame
TEST(RunSeed, TicksFollowAFixedFrameTime)
{
	Clock clock;
	clock.time.Start();
	clock.time.SetFixedFrameTime(std::chrono::milliseconds(16));
	clock.time.RestartClock(978350400);
	for (int frame = 0; frame < 5; ++frame)
	{
		clock.now += 1000 + static_cast<uint32_t>(frame * 37);
		clock.time.Update();
	}
	EXPECT_EQ(clock.time.GetTicks(), 80u);
}

// A pinned date moves on with the ticks; without one the date is the wall clock's
TEST(RunSeed, APinnedDateMovesOnWithTheTicks)
{
	Clock clock;
	clock.time.Start();
	clock.time.RestartClock(978350400);
	EXPECT_EQ(clock.time.GetPinnedDate(), 978350400);
	EXPECT_EQ(clock.time.GetUnixTime(), 978350400);
	clock.now += 2500;
	EXPECT_EQ(clock.time.GetUnixTime(), 978350402);
	clock.time.RestartClock(std::nullopt);
	// The wall clock's date: well after the pinned one
	EXPECT_GT(clock.time.GetUnixTime(), 1'600'000'000);
}
