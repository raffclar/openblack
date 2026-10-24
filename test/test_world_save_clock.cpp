/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <random>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/WorldSave.h"

using namespace openblack;
using openblack::ecs::world_save::ClockState;

namespace
{

/// A generator that has drawn some numbers, so that its place is not the place of its seed
std::mt19937 DrawnGenerator(uint32_t seed, int draws)
{
	std::mt19937 generator(seed);
	generator.discard(static_cast<unsigned long long>(draws));
	return generator;
}

ClockState PlayedClock()
{
	return {
	    .speed = 1.5f,
	    .paused = true,
	    .remainderMs = 42,
	    .seeds = {.synced = 0x1234ABCDu, .local = 0x00C0FFEEu},
	    .particles = ParticleRandomStream::Synced,
	    .crtSeed = 77u,
	    .runSeed = 9u,
	    .streams = {DrawnGenerator(1, 10), DrawnGenerator(2, 700), DrawnGenerator(3, 12345)},
	    .general = DrawnGenerator(4, 99),
	};
}

} // namespace

// A save keeps where each stream of random numbers is, and a load puts each one back at the same place
TEST(WorldSaveClock, ALoadPutsBackTheSeedsAndThePlaceOfEachStream)
{
	const auto saved = PlayedClock();
	const auto loaded = ecs::world_save::ReadClock(ecs::world_save::WriteClock(saved));
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(loaded->speed, saved.speed);
	EXPECT_EQ(loaded->paused, saved.paused);
	EXPECT_EQ(loaded->remainderMs, saved.remainderMs);
	EXPECT_EQ(loaded->seeds, saved.seeds);
	EXPECT_EQ(loaded->particles, saved.particles);
	EXPECT_EQ(loaded->crtSeed, saved.crtSeed);
	EXPECT_EQ(loaded->runSeed, saved.runSeed);
	ASSERT_EQ(loaded->streams.size(), saved.streams.size());
	for (size_t i = 0; i < saved.streams.size(); ++i)
	{
		EXPECT_EQ(loaded->streams.at(i), saved.streams.at(i)) << "stream " << i;
	}
	ASSERT_TRUE(loaded->general.has_value());
	EXPECT_EQ(*loaded->general, *saved.general);
}

// After a load each stream draws the numbers that it would have drawn without the save
TEST(WorldSaveClock, TheStreamsDrawTheSameNumbersAfterALoad)
{
	auto saved = PlayedClock();
	auto loaded = ecs::world_save::ReadClock(ecs::world_save::WriteClock(saved));
	ASSERT_TRUE(loaded.has_value());
	for (size_t i = 0; i < saved.streams.size(); ++i)
	{
		for (int draw = 0; draw < 100; ++draw)
		{
			ASSERT_EQ(loaded->streams.at(i)(), saved.streams.at(i)()) << "stream " << i << " draw " << draw;
		}
	}
	for (int draw = 0; draw < 100; ++draw)
	{
		ASSERT_EQ((*loaded->general)(), (*saved.general)()) << "draw " << draw;
	}
}

// A clock with no C library seed and no general stream reads back with none
TEST(WorldSaveClock, AClockWithNoGeneralStreamReadsBackWithNone)
{
	auto saved = PlayedClock();
	saved.crtSeed.reset();
	saved.general.reset();
	saved.streams.clear();
	const auto loaded = ecs::world_save::ReadClock(ecs::world_save::WriteClock(saved));
	ASSERT_TRUE(loaded.has_value());
	EXPECT_FALSE(loaded->crtSeed.has_value());
	EXPECT_FALSE(loaded->general.has_value());
	EXPECT_TRUE(loaded->streams.empty());
	EXPECT_EQ(loaded->seeds, saved.seeds);
}

// A cut block does not read, so the load keeps the clock that the land has
TEST(WorldSaveClock, ACutBlockDoesNotRead)
{
	const auto bytes = ecs::world_save::WriteClock(PlayedClock());
	ASSERT_GT(bytes.size(), 8u);
	EXPECT_FALSE(ecs::world_save::ReadClock(std::span(bytes).first(6)).has_value());
}

// A world state keeps the count of the synced draws, which the checksum of each turn has
TEST(WorldSaveClock, ALoadPutsBackTheCountOfTheSyncedDraws)
{
	auto saved = PlayedClock();
	saved.syncedDraws = 15519u;
	const auto loaded = ecs::world_save::ReadClock(ecs::world_save::WriteClock(saved));
	ASSERT_TRUE(loaded.has_value());
	ASSERT_TRUE(loaded->syncedDraws.has_value());
	EXPECT_EQ(*loaded->syncedDraws, 15519u);
}
