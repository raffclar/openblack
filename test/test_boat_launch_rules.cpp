/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "ECS/BoatLaunchRules.h"

using namespace openblack;
namespace rules = openblack::ecs::boat_launch;

namespace
{
/// The clips' lengths as the game's files have them
constexpr rules::ClipLengths k_Lengths {.launch = 15833, .sailing = 4466, .sailingLoops = true};

/// The launch clip's first keyframe: the boat lying across the beach, mirrored, at the dock
glm::mat4 FirstLaunchKeyframe()
{
	glm::mat4 m(1.0f);
	m[0] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
	m[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	m[2] = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
	return m;
}

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-4f)
{
	EXPECT_NEAR(a.x, b.x, tolerance);
	EXPECT_NEAR(a.y, b.y, tolerance);
	EXPECT_NEAR(a.z, b.z, tolerance);
}
} // namespace

TEST(BoatLaunchRules, TheBoatWaitsThreeSecondsForTheSailorsFirstPush)
{
	rules::Clocks clocks;
	int32_t frames = 0;
	while (clocks.boatTime == 0)
	{
		ASSERT_EQ(rules::Advance(clocks, 16, k_Lengths), rules::Step::Continues);
		++frames;
	}
	// It moves on the first frame more than three seconds in
	EXPECT_EQ(frames, 188);
	EXPECT_EQ(clocks.elapsed, 3008);
	EXPECT_EQ(clocks.boatTime, 16);
	EXPECT_EQ(clocks.previousBoatTime, 0);
	EXPECT_TRUE(clocks.pushing);
}

TEST(BoatLaunchRules, TheSailorsStopPushingOnceTheBoatIsAwayAndTheirClocksStartAgain)
{
	rules::Clocks clocks {.elapsed = 4000, .boatTime = 840};
	EXPECT_EQ(rules::Advance(clocks, 16, k_Lengths), rules::Step::Continues);
	EXPECT_EQ(clocks.boatTime, 856);
	EXPECT_FALSE(clocks.pushing);
	EXPECT_EQ(clocks.elapsed, 0);
	// From then on the boat runs on with the clock, and the sailors' clocks run on too
	EXPECT_EQ(rules::Advance(clocks, 16, k_Lengths), rules::Step::Continues);
	EXPECT_EQ(clocks.boatTime, 872);
	EXPECT_EQ(clocks.elapsed, 16);
}

TEST(BoatLaunchRules, TheLaunchEndsFourHundredMillisecondsBeforeItsClipAndTheSailingStarts)
{
	rules::Clocks clocks {.elapsed = 10000, .boatTime = 15420, .pushing = false, .sinceSpray = 150};
	EXPECT_EQ(rules::Advance(clocks, 13, k_Lengths), rules::Step::Continues);
	EXPECT_EQ(clocks.boatTime, 15433);
	EXPECT_EQ(rules::Advance(clocks, 16, k_Lengths), rules::Step::SailingBegan);
	EXPECT_EQ(clocks.stage, rules::Stage::Sailing);
	EXPECT_EQ(clocks.elapsed, 0);
	EXPECT_EQ(clocks.boatTime, 0);
	EXPECT_EQ(clocks.sinceSpray, 0);
	// The wake already moved on that frame
	EXPECT_EQ(clocks.wakes, (std::array<int32_t, 5> {16, 1216, 2416, 3616, 4816}));
}

TEST(BoatLaunchRules, TheSailingLastsAMinuteWithItsClipComingRound)
{
	rules::Clocks clocks {.stage = rules::Stage::Sailing, .elapsed = 59990, .boatTime = 4460};
	EXPECT_EQ(rules::Advance(clocks, 10, k_Lengths), rules::Step::Continues);
	EXPECT_EQ(clocks.boatTime, 4);
	EXPECT_EQ(clocks.elapsed, 60000);
	EXPECT_EQ(rules::Advance(clocks, 1, k_Lengths), rules::Step::Ended);
}

TEST(BoatLaunchRules, AClipThatDoesNotLoopHoldsItsLastMillisecond)
{
	rules::Clocks clocks {.stage = rules::Stage::Sailing, .boatTime = 4460};
	EXPECT_EQ(rules::Advance(clocks, 100, {.launch = 15833, .sailing = 4466, .sailingLoops = false}), rules::Step::Continues);
	EXPECT_EQ(clocks.boatTime, 4465);
}

TEST(BoatLaunchRules, TheWakeSpritesRunRoundTheirLives)
{
	rules::Clocks clocks {.stage = rules::Stage::Sailing, .wakes = {5990, 0, 1, 2, 3}};
	EXPECT_EQ(rules::Advance(clocks, 20, k_Lengths), rules::Step::Continues);
	EXPECT_EQ(clocks.wakes, (std::array<int32_t, 5> {10, 20, 21, 22, 23}));
}

TEST(BoatLaunchRules, TheLaunchSoundsPlayAsTheBoatPassesTheirTimes)
{
	EXPECT_EQ(rules::SoundsPassed(90, 110), std::vector<int32_t> {0x3E});
	EXPECT_TRUE(rules::SoundsPassed(100, 116).empty());
	EXPECT_TRUE(rules::SoundsPassed(84, 100).empty());
	EXPECT_EQ(rules::SoundsPassed(1490, 1506), std::vector<int32_t> {0x3D});
	EXPECT_EQ(rules::SoundsPassed(3890, 3906), std::vector<int32_t> {0x3C});
	EXPECT_EQ(rules::SoundsPassed(0, 5000), (std::vector<int32_t> {0x3E, 0x3D, 0x3C}));
}

TEST(BoatLaunchRules, SandFliesDownTheBeachThenWaterAsItMeetsTheSea)
{
	EXPECT_FALSE(rules::SprayAt(1130).has_value());
	const auto sand = rules::SprayAt(1131);
	ASSERT_TRUE(sand.has_value());
	EXPECT_EQ(sand->size, 5.0f);
	EXPECT_EQ(sand->colour, 0xB88C38u);
	ExpectNear(sand->offset, glm::vec3(0.0f));
	EXPECT_EQ(rules::SprayAt(3900)->colour, 0xB88C38u);
	const auto water = rules::SprayAt(3901);
	ASSERT_TRUE(water.has_value());
	EXPECT_EQ(water->size, 7.0f);
	EXPECT_EQ(water->colour, 0xFFFFFFu);
	ExpectNear(water->offset, glm::vec3(-10.0f, 7.0f, 0.0f));
	EXPECT_TRUE(rules::SprayAt(6499).has_value());
	EXPECT_FALSE(rules::SprayAt(6500).has_value());
}

TEST(BoatLaunchRules, SprayIsDueOnceMoreThanTwoHundredMillisecondsHavePassed)
{
	rules::Clocks clocks {.sinceSpray = 190};
	EXPECT_EQ(rules::Advance(clocks, 10, k_Lengths), rules::Step::Continues);
	EXPECT_FALSE(rules::TakeSpray(clocks));
	EXPECT_EQ(rules::Advance(clocks, 1, k_Lengths), rules::Step::Continues);
	EXPECT_TRUE(rules::TakeSpray(clocks));
	EXPECT_EQ(clocks.sinceSpray, 0);
}

TEST(BoatLaunchRules, TheRootBoneIsMixedElementByElement)
{
	const glm::mat4 from(1.0f);
	const auto to = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f)), glm::radians(90.0f),
	                            glm::vec3(0.0f, 1.0f, 0.0f));
	const auto half = rules::RootBetween(from, to, 0.5f);
	// Not turned by half: its axes shrink between the two
	EXPECT_NEAR(half[0][0], 0.5f, 1e-6f);
	EXPECT_NEAR(half[0][2], -0.5f, 1e-6f);
	EXPECT_NEAR(half[3][0], 5.0f, 1e-6f);
}

TEST(BoatLaunchRules, OnItsFirstKeyframeTheBoatStandsSquareAtTheDock)
{
	const auto boat = rules::LaunchMatrix(FirstLaunchKeyframe(), rules::k_LaunchPlace, 0.5f);
	ExpectNear(glm::vec3(boat[0]), {1.0f, 0.0f, 0.0f});
	ExpectNear(glm::vec3(boat[1]), {0.0f, 1.0f, 0.0f});
	ExpectNear(glm::vec3(boat[2]), {0.0f, 0.0f, 1.0f});
	ExpectNear(glm::vec3(boat[3]), rules::k_LaunchPlace + glm::vec3(0.0f, 0.5f, 0.0f));
	ExpectNear(rules::LaunchPlace(FirstLaunchKeyframe(), rules::k_LaunchPlace), rules::k_LaunchPlace);
}

TEST(BoatLaunchRules, TheClipCarriesTheBoatAwayFromTheDock)
{
	auto root = FirstLaunchKeyframe();
	root[3] = glm::vec4(-89.5f, -4.2f, 5.57f, 1.0f);
	const auto boat = rules::LaunchMatrix(root, rules::k_LaunchPlace, -2.0f);
	ExpectNear(glm::vec3(boat[3]), rules::k_LaunchPlace + glm::vec3(-89.5f, -6.2f, 5.57f));
}

TEST(BoatLaunchRules, AtSeaTheBoatSailsSouthWestAtFiveMetresASecond)
{
	const auto root = FirstLaunchKeyframe();
	const auto start = rules::SailingMatrix(root, rules::k_SailingStart, 0);
	ExpectNear(glm::vec3(start[3]), rules::k_SailingStart);
	const auto later = rules::SailingMatrix(root, rules::k_SailingStart, 10000);
	const auto moved = glm::vec3(later[3] - start[3]);
	EXPECT_NEAR(glm::length(moved), 50.0f, 0.01f);
	EXPECT_LT(moved.x, 0.0f);
	EXPECT_LT(moved.z, 0.0f);
	EXPECT_NEAR(moved.x, moved.z, 1e-3f);
	EXPECT_EQ(moved.y, 0.0f);
	// Turned an eighth from square, the right way round
	EXPECT_GT(glm::determinant(glm::mat3(later)), 0.0f);
}

TEST(BoatLaunchRules, FiveSailorsPushBesideTheDockThenRestOrCheer)
{
	const rules::Clocks pushing {.elapsed = 100, .boatTime = 0};
	const auto first = rules::LaunchSailor(0, pushing, rules::k_LaunchPlace);
	EXPECT_NEAR(first.xz.x, rules::k_LaunchPlace.x + 5.2f, 1e-3f);
	EXPECT_NEAR(first.xz.y, rules::k_LaunchPlace.z + 10.0f - 7.5f + 1.0f, 1e-3f);
	EXPECT_EQ(first.clip, AnimId::PPushObject);
	EXPECT_EQ(first.clipTime, 105);
	EXPECT_NEAR(first.heading, -1.5707964f, 1e-6f);
	const auto last = rules::LaunchSailor(4, pushing, rules::k_LaunchPlace);
	EXPECT_NEAR(last.xz.y, rules::k_LaunchPlace.z + 10.0f + 4.5f - 0.2f, 1e-3f);
	EXPECT_EQ(last.clipTime, 2100);

	const rules::Clocks away {.elapsed = 0, .boatTime = 851, .pushing = false};
	EXPECT_EQ(rules::LaunchSailor(0, away, rules::k_LaunchPlace).clip, AnimId::POverworked1);
	EXPECT_EQ(rules::LaunchSailor(1, away, rules::k_LaunchPlace).clip, AnimId::PCrowdWon_2);
	EXPECT_EQ(rules::LaunchSailor(2, away, rules::k_LaunchPlace).clip, AnimId::POverworked2);
	EXPECT_EQ(rules::LaunchSailor(3, away, rules::k_LaunchPlace).clip, AnimId::POverworked1);
	EXPECT_EQ(rules::LaunchSailor(4, away, rules::k_LaunchPlace).clip, AnimId::PCrowdWon_2);
}

TEST(BoatLaunchRules, TheDeckCarriesTwoCowsGrainAndSixPeople)
{
	const auto& parts = rules::DeckParts();
	EXPECT_EQ(std::ranges::count(parts, MeshId::AnimalCow1, &rules::DeckPart::mesh), 2);
	EXPECT_EQ(std::ranges::count(parts, MeshId::PersonNorseSailor, &rules::DeckPart::mesh), 4);
	EXPECT_EQ(std::ranges::count(parts, MeshId::PersonNorseFemaleA1, &rules::DeckPart::mesh), 1);
	EXPECT_FALSE(parts.at(2).clip.has_value());
	EXPECT_NEAR(parts.at(2).local[0][0], 0.26f, 1e-6f);
	// The pair at the bow, arms out, half a second apart
	EXPECT_EQ(parts.at(3).clip, AnimId::PTitanic);
	EXPECT_EQ(parts.at(4).clip, AnimId::PTitanic);
	EXPECT_EQ(parts.at(4).clipOffset, 500);
	// The sitters face aft
	EXPECT_NEAR(parts.at(5).local[0][0], -1.0f, 1e-6f);
	EXPECT_NEAR(parts.at(5).local[2][2], -1.0f, 1e-6f);

	const auto boat = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 0.0f, 200.0f));
	ExpectNear(glm::vec3(rules::OnDeck(parts.at(3), boat)[3]), {99.86f, 13.213f, 180.343f});
}

TEST(BoatLaunchRules, TheWakeRunsBackFromTheBowAndFades)
{
	const auto boat = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, -4.0f, 200.0f));
	const auto fresh = rules::WakeAt(0, boat);
	EXPECT_FALSE(fresh.shown);
	EXPECT_EQ(fresh.alpha, 255);
	EXPECT_EQ(fresh.halfWidth, 10.0f);
	ExpectNear(fresh.position, {100.0f, 0.2f, 185.0f});

	const auto half = rules::WakeAt(3000, boat);
	EXPECT_TRUE(half.shown);
	EXPECT_EQ(half.alpha, 127);
	EXPECT_NEAR(half.halfWidth, 25.0f, 1e-4f);
	ExpectNear(half.position, {100.0f, 0.2f, 235.0f});
	EXPECT_EQ(half.aspect, 0.5f);
	EXPECT_EQ(half.cell, 0x31);
	EXPECT_NEAR(half.angle, 3.9269910f, 1e-6f);

	// It shows once past a fifth of its life, which a fifth in single precision is, just
	EXPECT_FALSE(rules::WakeAt(1199, boat).shown);
	EXPECT_TRUE(rules::WakeAt(1200, boat).shown);
}
