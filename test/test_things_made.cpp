/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <limits>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/ThingsMadeSystem.h"

using openblack::ecs::systems::ThingsMadeSystem;

// Each new thing takes the number of the things made before it, in the order they are made
TEST(ThingsMade, EachThingTakesTheNextPlace)
{
	uint32_t count = 0;
	ThingsMadeSystem made([&count]() -> uint32_t& { return count; });
	EXPECT_EQ(made.Made(), 0u);
	EXPECT_EQ(made.TakeNext(), 0u);
	EXPECT_EQ(made.TakeNext(), 1u);
	EXPECT_EQ(made.TakeNext(), 2u);
	EXPECT_EQ(made.Made(), 3u);
	EXPECT_EQ(count, 3u);
}

// The count is kept with the world: a new land starts it again at 0, and a loaded game goes on from its saved count
TEST(ThingsMade, TheCountFollowsTheWorld)
{
	uint32_t count = 0;
	ThingsMadeSystem made([&count]() -> uint32_t& { return count; });
	static_cast<void>(made.TakeNext());
	static_cast<void>(made.TakeNext());
	// A new land
	count = 0;
	EXPECT_EQ(made.TakeNext(), 0u);
	// A saved game with 4710 things made
	count = 4710;
	EXPECT_EQ(made.Made(), 4710u);
	EXPECT_EQ(made.TakeNext(), 4710u);
	EXPECT_EQ(made.Made(), 4711u);
}

// The count is a whole number of 32 bits that goes round to 0 after its largest value, as the game's does
TEST(ThingsMade, TheCountWraps)
{
	uint32_t count = std::numeric_limits<uint32_t>::max();
	ThingsMadeSystem made([&count]() -> uint32_t& { return count; });
	EXPECT_EQ(made.TakeNext(), std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(made.TakeNext(), 0u);
}
