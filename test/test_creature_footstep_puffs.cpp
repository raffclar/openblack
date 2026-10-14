/******************************************************************************
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

#include "Creature/CreatureFootstepPuffs.h"

using namespace openblack;
using namespace openblack::creature_footstep_puffs;
using audio::SoundAction;

TEST(CreatureFootstepPuffs, HeavierStepsMakeMorePuffs)
{
	EXPECT_EQ(CountOf(SoundAction::FootstepLight), 3);
	EXPECT_EQ(CountOf(SoundAction::FootstepNormal), 6);
	EXPECT_EQ(CountOf(SoundAction::FootStamp), 12);
	EXPECT_EQ(CountOf(SoundAction::None), 0);
	EXPECT_EQ(CountOf(SoundAction::BreatheIn), 0);
	EXPECT_EQ(CountOf(SoundAction::Scream), 0);
}

TEST(CreatureFootstepPuffs, SizeAndColourFollowTheCreature)
{
	EXPECT_FLOAT_EQ(SizeOf(1.0f), 4.0f);
	EXPECT_FLOAT_EQ(SizeOf(2.5f), 10.0f);
	EXPECT_EQ(k_Colour, 0x40806040u);
}

TEST(CreatureFootstepPuffs, SpeedTakesTheRandomNumbersOnZThenYThenX)
{
	std::vector<int32_t> limits;
	std::vector<uint32_t> draws {0, 100, 200};
	size_t next = 0;
	const auto velocity = VelocityOf(2.0f, [&](int32_t n) {
		limits.push_back(n);
		return draws.at(next++);
	});
	EXPECT_EQ(limits, (std::vector<int32_t> {201, 201, 201}));
	// Each axis is (r - 100) times a twentieth of the shown size
	EXPECT_FLOAT_EQ(velocity.z, -10.0f);
	EXPECT_FLOAT_EQ(velocity.y, 0.0f);
	EXPECT_FLOAT_EQ(velocity.x, 10.0f);
}
