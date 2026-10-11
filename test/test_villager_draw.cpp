/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <numbers>

#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/VillagerPose.h"
#include "ECS/VillagerDrawRules.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;

void ExpectNear(const glm::vec3& a, const glm::vec3& b)
{
	EXPECT_NEAR(a.x, b.x, 1e-4f);
	EXPECT_NEAR(a.y, b.y, 1e-4f);
	EXPECT_NEAR(a.z, b.z, 1e-4f);
}
} // namespace

TEST(VillagerDraw, AWalkerGlidesFromWhereTheTurnBeganToWhereItStands)
{
	// The boy skipping away from his parents: 0.298 m a turn
	const glm::vec3 start {1566.540f, 11.670f, 2192.354f};
	const glm::vec3 now {1566.350f, 11.673f, 2192.125f};
	ExpectNear(villager_draw::DrawnPosition(start, now, 0.0f, true), start);
	ExpectNear(villager_draw::DrawnPosition(start, now, 0.5f, true), (start + now) * 0.5f);
	ExpectNear(villager_draw::DrawnPosition(start, now, 0.99f, true), (start * 0.01f) + (now * 0.99f));
}

TEST(VillagerDraw, OneNotGlidingIsDrawnWhereItStands)
{
	const glm::vec3 start {10.0f, 0.0f, 10.0f};
	const glm::vec3 now {11.0f, 0.0f, 10.0f};
	ExpectNear(villager_draw::DrawnPosition(start, now, 0.5f, false), now);
}

TEST(VillagerDraw, OnlyAMovingStateWithAClipPlayedByTheGroundGlides)
{
	EXPECT_TRUE(villager_draw::Glides(true, false));
	EXPECT_FALSE(villager_draw::Glides(true, true));
	EXPECT_FALSE(villager_draw::Glides(false, false));
}

TEST(VillagerDraw, AnglesWrapByOneTurnAtMost)
{
	EXPECT_FLOAT_EQ(villager_draw::Wrapped(4.0f), 4.0f - (2.0f * k_Pi));
	EXPECT_FLOAT_EQ(villager_draw::Wrapped(-4.0f), -4.0f + (2.0f * k_Pi));
	EXPECT_FLOAT_EQ(villager_draw::Wrapped(1.0f), 1.0f);
}

TEST(VillagerDraw, ASmallTurnIsDrawnAtThreeRadiansASecond)
{
	// 10 ms at 0.003 a millisecond: 0.03 towards the way it faces, either way round
	EXPECT_NEAR(villager_draw::EasedHeading(0.0f, 1.0f, 10), 0.03f, 1e-6f);
	EXPECT_NEAR(villager_draw::EasedHeading(0.0f, -1.0f, 10), -0.03f, 1e-6f);
	// Within the step it faces its way at once
	EXPECT_FLOAT_EQ(villager_draw::EasedHeading(0.0f, 0.02f, 10), 0.02f);
}

TEST(VillagerDraw, ABigTurnIsDrawnQuicker)
{
	// A half turn less a little: four times its size over a half turn, times the ordinary rate
	const float size = 3.0f;
	const float expected = 10.0f * (size * (2.0f / k_Pi) * 2.0f) * 0.003f;
	EXPECT_NEAR(villager_draw::EasedHeading(0.0f, size, 10), expected, 1e-5f);
}

TEST(VillagerDraw, ATurnGoesTheShortWayRound)
{
	// From just under a half turn to just over: the short way crosses the half turn
	const float drawn = k_Pi - 0.1f;
	const float facing = -k_Pi + 0.1f;
	const float eased = villager_draw::EasedHeading(drawn, facing, 10);
	EXPECT_NEAR(villager_draw::Wrapped(eased - drawn), 0.03f, 1e-5f);
}

TEST(VillagerDraw, NoTimeNoTurn)
{
	EXPECT_FLOAT_EQ(villager_draw::EasedHeading(0.5f, 1.0f, 0), 0.5f);
}

TEST(VillagerDraw, AHighDetailBodyTurnsAfterItsDrawnHeading)
{
	// 3.927 radians a second for 10 ms
	EXPECT_NEAR(villager_draw::EasedDetailedHeading(0.0f, 1.0f, 10, false), 0.03927f, 1e-6f);
	EXPECT_NEAR(villager_draw::EasedDetailedHeading(0.0f, -1.0f, 10, false), -0.03927f, 1e-6f);
	EXPECT_FLOAT_EQ(villager_draw::EasedDetailedHeading(0.0f, 0.03f, 10, false), 0.03f);
}

TEST(VillagerDraw, AHighDetailBodyToldToTurnAtOnceDoes)
{
	EXPECT_FLOAT_EQ(villager_draw::EasedDetailedHeading(0.0f, 2.0f, 10, true), 2.0f);
}

TEST(VillagerDraw, HeadingsStartFacingItsWay)
{
	const auto first = villager_draw::StepHeadings(std::nullopt, std::nullopt, 1.0f, 10, true, false);
	EXPECT_FLOAT_EQ(first.eased, 1.0f);
	ASSERT_TRUE(first.detailed.has_value());
	EXPECT_FLOAT_EQ(*first.detailed, 1.0f);
	EXPECT_FLOAT_EQ(first.drawn, 1.0f);
}

TEST(VillagerDraw, AHighDetailVillagerIsDrawnWithItsBodysHeading)
{
	// Turned 0.5 away: the drawn heading turns 0.03, the body after it no further than that
	const auto step = villager_draw::StepHeadings(0.0f, 0.0f, 0.5f, 10, true, false);
	EXPECT_NEAR(step.eased, 0.03f, 1e-6f);
	ASSERT_TRUE(step.detailed.has_value());
	EXPECT_NEAR(*step.detailed, 0.03f, 1e-6f);
	EXPECT_FLOAT_EQ(step.drawn, *step.detailed);
	// An ordinary villager has no body heading of its own
	const auto ordinary = villager_draw::StepHeadings(0.0f, std::nullopt, 0.5f, 10, false, false);
	EXPECT_FALSE(ordinary.detailed.has_value());
	EXPECT_FLOAT_EQ(ordinary.drawn, ordinary.eased);
}

TEST(VillagerDraw, HeadingsAndModelTurnsMatch)
{
	for (const float heading : {0.0f, 1.0f, -2.5f, 3.0f})
	{
		const auto rotation = villager_draw::RotationOf(heading);
		const auto back = villager_draw::HeadingOf(rotation);
		ASSERT_TRUE(back.has_value());
		EXPECT_NEAR(villager_draw::Wrapped(*back - heading), 0.0f, 1e-5f);
		// The same turn the villagers' systems give their models
		const auto expected = glm::mat3(glm::eulerAngleY(-heading - (k_Pi * 0.5f)));
		for (int c = 0; c < 3; ++c)
		{
			for (int r = 0; r < 3; ++r)
			{
				EXPECT_NEAR(rotation[c][r], expected[c][r], 1e-5f);
			}
		}
	}
	// One tipped over has no heading
	EXPECT_FALSE(villager_draw::HeadingOf(glm::mat3(glm::eulerAngleX(0.5f))).has_value());
}

TEST(VillagerDraw, ANewClipFadesInOverThreeHundredMilliseconds)
{
	villager_draw::ClipBlendTrack track;
	villager_draw::StepClipBlend(track, AnimId::CrowFlap, 100, 10);
	EXPECT_FALSE(villager_draw::ShowsClipBlend(track.blend, false));
	villager_draw::StepClipBlend(track, AnimId::CrowFlap, 110, 10);
	// The clip changes: all of the old one shows, held where it was last drawn
	villager_draw::StepClipBlend(track, AnimId::CrowGlide, 0, 10);
	EXPECT_EQ(track.blend.from, AnimId::CrowFlap);
	EXPECT_EQ(track.blend.fromPlace, 110u);
	EXPECT_EQ(track.blend.remaining, 300);
	EXPECT_FLOAT_EQ(track.blend.weight, 1.0f);
	EXPECT_TRUE(villager_draw::ShowsClipBlend(track.blend, false));
	EXPECT_FALSE(villager_draw::ShowsClipBlend(track.blend, true));
	// Less of it each frame
	villager_draw::StepClipBlend(track, AnimId::CrowGlide, 10, 30);
	EXPECT_EQ(track.blend.remaining, 270);
	EXPECT_FLOAT_EQ(track.blend.weight, 0.9f);
	// Until none is left
	villager_draw::StepClipBlend(track, AnimId::CrowGlide, 300, 290);
	EXPECT_EQ(track.blend.remaining, 0);
	EXPECT_FALSE(villager_draw::ShowsClipBlend(track.blend, false));
}

TEST(VillagerDraw, BlendedPosesMixByTheOldClipsWeight)
{
	std::array<glm::mat4, 1> bones {glm::mat4(2.0f)};
	const std::array<glm::mat4, 1> old {glm::mat4(4.0f)};
	villager_draw::BlendPoses(bones, old, 0.25f);
	EXPECT_FLOAT_EQ(bones[0][0][0], (2.0f * 0.75f) + (4.0f * 0.25f));
}

TEST(VillagerDraw, APoseWithoutADrawingIsDrawnWhereItStands)
{
	const glm::vec3 standsAt {1.0f, 2.0f, 3.0f};
	const auto rotation = glm::mat3(glm::eulerAngleY(0.3f));
	EXPECT_EQ(components::DrawnPosition(standsAt, nullptr), standsAt);
	components::VillagerPose pose;
	EXPECT_EQ(components::DrawnPosition(standsAt, &pose), standsAt);
	EXPECT_EQ(components::DrawnRotation(rotation, &pose), rotation);
	pose.drawnAt = glm::vec3 {4.0f, 5.0f, 6.0f};
	pose.drawnHeading = 1.0f;
	EXPECT_EQ(components::DrawnPosition(standsAt, &pose), *pose.drawnAt);
	EXPECT_EQ(components::DrawnRotation(rotation, &pose), villager_draw::RotationOf(1.0f));
}
