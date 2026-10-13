/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>

#include <ECS/FightWatchRules.h>
#include <ECS/VillagerAnimation.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace watch = openblack::ecs::fight_watch;
using openblack::ecs::creature_attitude::Attitude;

namespace
{
/// A random draw that always gives the same number, kept within its range
watch::IntRandom Always(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0u : std::min(value, range - 1); };
}
} // namespace

// The watchers' places halve the gaps round the ring: the newest at the start, then opposite, the quarters, the eighths
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(FightWatchRules, PlacesSpreadRoundTheRing)
{
	const std::array<uint16_t, 9> expected {0, 1024, 512, 1536, 256, 1280, 768, 1792, 128};
	for (uint32_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(watch::PlaceAngle(i), expected.at(i)) << i;
	}
	// The game's search for the highest bit passes over a byte of just 1, which throws the 513th place off the ring
	EXPECT_EQ(watch::PlaceAngle(512), 65024);
}

// A place stands 1.4 times the arena's radius from its middle, worked in metres and back to map units, truncated
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(FightWatchRules, PlaceOnTheRing)
{
	EXPECT_EQ(watch::PlaceOnRing({0, 0}, 10.0f, 0), glm::ivec2(91750, 0));
	EXPECT_EQ(watch::PlaceOnRing({655360, 327680}, 10.0f, 0), glm::ivec2(747110, 327680));
	EXPECT_EQ(watch::PlaceOnRing({655360, 327680}, 10.0f, 512), glm::ivec2(655360, 419430));
}

// Villagers heed an arena only while a fight is on in it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(FightWatchRules, PriorityWhileAFightIsOn)
{
	EXPECT_EQ(watch::Priority(true, 50), 50);
	EXPECT_EQ(watch::Priority(false, 50), 0);
}

// A villager supports the creature its town doesn't fear; either at random when it fears both, neither, or has no town
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(FightWatchRules, SupportsTheCreatureNotFeared)
{
	EXPECT_EQ(watch::Supports(watch::TownFeelings {Attitude::Fear, Attitude::Respect}, Always(0)), watch::Side::Second);
	EXPECT_EQ(watch::Supports(watch::TownFeelings {Attitude::Curious, Attitude::Fear}, Always(1)), watch::Side::First);
	EXPECT_EQ(watch::Supports(watch::TownFeelings {Attitude::Fear, Attitude::Fear}, Always(1)), watch::Side::Second);
	EXPECT_EQ(watch::Supports(watch::TownFeelings {Attitude::Respect, Attitude::Curious}, Always(0)), watch::Side::First);
	EXPECT_EQ(watch::Supports(std::nullopt, Always(1)), watch::Side::Second);
	EXPECT_EQ(watch::Supports(std::nullopt, Always(0)), watch::Side::First);
}

// Seconds are whole, counted from game turns
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(FightWatchRules, SecondsSinceATurn)
{
	EXPECT_EQ(watch::SecondsSince(159, 10, 10), 14u);
	EXPECT_EQ(watch::SecondsSince(160, 10, 10), 15u);
	EXPECT_EQ(watch::SecondsSince(170, 10, 10), 16u);
}

// The crowd cheers the latest blow on the other creature and despairs at one on its own, within fifteen seconds;
// otherwise it chats
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(FightWatchRules, CheersDespairsOrChats)
{
	namespace clips = openblack::ecs::villager_animation;
	EXPECT_EQ(clips::WatchFightClip(20, 3, Always(0)), AnimId::PCrowdWon);
	EXPECT_EQ(clips::WatchFightClip(20, 3, Always(1)), AnimId::PCrowdWon_2);
	EXPECT_EQ(clips::WatchFightClip(3, 20, Always(0)), AnimId::PCrowdLost);
	EXPECT_EQ(clips::WatchFightClip(15, 15, Always(1)), AnimId::PCrowdLost_2);
	EXPECT_EQ(clips::WatchFightClip(16, 300, Always(0)), AnimId::PTalkingAndPointing);
	EXPECT_EQ(clips::WatchFightClip(16, 300, Always(1)), AnimId::PTalkToNeighbour);
	EXPECT_EQ(clips::WatchFightClip(16, 16, Always(2)), AnimId::PGossipMan);
}
