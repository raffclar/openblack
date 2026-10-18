/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreaturePuke.h"

using namespace openblack;

namespace
{
/// Gives the queued numbers in turn and records each limit it was asked for
struct QueuedRandom
{
	std::vector<uint32_t> numbers;
	std::vector<int32_t> limits;
	creature_puke::LocalRandom Get()
	{
		return [this](int32_t limit) {
			const auto value = numbers.at(limits.size() % numbers.size());
			limits.push_back(limit);
			return value;
		};
	}
};
} // namespace

TEST(CreaturePuke, EachDropDrawsItsSpeedThenItsShade)
{
	QueuedRandom random {.numbers = {10, 20, 30, 40, 50, 25}};
	const auto drops = creature_puke::DropsOf(2.0f, 0xFFFFFFFFu, random.Get());
	ASSERT_EQ(random.limits.size(), 6u * creature_puke::k_DropsPerTurn);
	EXPECT_EQ(random.limits[0], 100);
	EXPECT_EQ(random.limits[1], 100);
	EXPECT_EQ(random.limits[2], 100);
	EXPECT_EQ(random.limits[3], 100);
	EXPECT_EQ(random.limits[4], 80);
	EXPECT_EQ(random.limits[5], 30);
	const float step = 2.0f * creature_puke::k_SpeedStep;
	const float least = 2.0f * creature_puke::k_SpeedLeast;
	// The first draw is along z, the second down and the third along x
	EXPECT_FLOAT_EQ(drops[0].velocity.z, 10.0f * step - least);
	EXPECT_FLOAT_EQ(drops[0].velocity.y, -(20.0f * step));
	EXPECT_FLOAT_EQ(drops[0].velocity.x, 30.0f * step - least);
	// White light: red 255 x (40 + 80) / 255, green 255 x (50 + 60) / 255, blue 255 x (25 + 20) / 255
	EXPECT_EQ(drops[0].argb, 0xFF786E2Du);
}

TEST(CreaturePuke, TheShadeIsCutTowardsZero)
{
	EXPECT_EQ(creature_puke::ShadeChannel(100, 179), (100u * 179u) / 255u);
	EXPECT_EQ(creature_puke::ShadeChannel(0, 179), 0u);
	EXPECT_EQ(creature_puke::ShadeChannel(255, 49), 49u);
	EXPECT_FLOAT_EQ(creature_puke::DropSize(2.0f), 2.0f * creature_puke::k_SizePerSize);
	QueuedRandom random {.numbers = {0}};
	// Dark light gives dark sick, and the alpha of the light stays
	const auto drops = creature_puke::DropsOf(1.0f, 0x80102030u, random.Get());
	EXPECT_EQ(drops[0].argb, 0x80000000u | ((0x10u * 80u / 255u) << 16u) | ((0x20u * 60u / 255u) << 8u) | (0x30u * 20u / 255u));
}
