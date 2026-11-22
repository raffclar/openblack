/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "ECS/SwimmerRules.h"

using namespace openblack;
using namespace openblack::ecs::swimmer_rules;

TEST(SwimmerRules, OnlyAVillagerInHighDetailThatPlaysTheSwimClipSwims)
{
	EXPECT_TRUE(Swims(true, AnimId::PSwim2));
	EXPECT_FALSE(Swims(false, AnimId::PSwim2));
	EXPECT_FALSE(Swims(true, AnimId::PFisherman));
}

TEST(SwimmerRules, ARingComesOnceTheFramesPassOneSecondAndTheTimerStartsAgain)
{
	int32_t timer = 0;
	int angles = 0;
	const auto angle = [&angles] {
		++angles;
		return 2.5f;
	};
	// Up to a whole second of frames: no ring, and no random angle
	for (int i = 0; i < 10; ++i)
	{
		EXPECT_FALSE(Ring(timer, 100, {1.0f, 2.0f, 3.0f}, angle).has_value());
	}
	EXPECT_EQ(timer, 1000);
	EXPECT_EQ(angles, 0);
	// The next frame passes it: a ring, and the timer starts again at 0, not at what is left over
	const auto ring = Ring(timer, 40, {1.0f, 2.0f, 3.0f}, angle);
	ASSERT_TRUE(ring.has_value());
	EXPECT_EQ(timer, 0);
	EXPECT_EQ(angles, 1);
	// On the water over the bone's place across the land, a little above the sea
	EXPECT_EQ(ring->position.x, 1.0f);
	EXPECT_EQ(ring->position.y, std::bit_cast<float>(0x3D4CCCCDu));
	EXPECT_EQ(ring->position.z, 3.0f);
	EXPECT_EQ(ring->angle, 2.5f);
	EXPECT_EQ(ring->growth, 1.0f);
	EXPECT_EQ(ring->aspect, 1.0f);
	EXPECT_EQ(ring->rate, 1.0f);
	EXPECT_EQ(ring->cell, 0x30);
	EXPECT_EQ(ring->argb, 0xFFFFFFFFu);
	EXPECT_EQ(ring->age, 0u);
}

TEST(SwimmerRules, TheRingComesAtTheBoneWhereTheSwimmerIsDrawn)
{
	// A swimmer drawn twice its size at (100, 0, 200), turned a quarter turn about the vertical
	const glm::mat4 model {
	    glm::vec4(0.0f, 0.0f, -2.0f, 0.0f),
	    glm::vec4(0.0f, 2.0f, 0.0f, 0.0f),
	    glm::vec4(2.0f, 0.0f, 0.0f, 0.0f),
	    glm::vec4(100.0f, 0.0f, 200.0f, 1.0f),
	};
	// The bone is 1 along x, 0.5 up and 3 along z in the pose
	glm::mat4 bone(1.0f);
	bone[3] = glm::vec4(1.0f, 0.5f, 3.0f, 1.0f);
	const auto place = RingPlace(model, bone);
	EXPECT_EQ(place.x, 106.0f);
	EXPECT_EQ(place.y, 1.0f);
	EXPECT_EQ(place.z, 198.0f);

	// The ring lies on the water at that place, not at the bone's place in the pose
	int32_t timer = 1000;
	const auto ring = Ring(timer, 1, place, [] { return 0.0f; });
	ASSERT_TRUE(ring.has_value());
	EXPECT_EQ(ring->position.x, 106.0f);
	EXPECT_EQ(ring->position.z, 198.0f);
}
