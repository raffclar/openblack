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
#include <vector>

#include <HelpStatsFile.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFight.h"
#include "Graphics/InterfaceCreatures.h"
#include "Help/HelpProfile.h"
#include "Help/HelpProfileFile.h"

using namespace openblack;
namespace fight = openblack::creature_fight;
namespace profile = openblack::help::profile;
namespace views = openblack::graphics::interface_creatures;

namespace
{
constexpr float k_Near = 1e-4f;
} // namespace

TEST(CreatureFightHelpEvents, EachMoveCountsAsItsKind)
{
	EXPECT_EQ(fight::HelpEventOf(fight::Move::Kind::Block), profile::k_FightBlock);
	EXPECT_EQ(fight::HelpEventOf(fight::Move::Kind::Spell), profile::k_FightCast);
	EXPECT_EQ(fight::HelpEventOf(fight::Move::Kind::Animation), profile::k_FightStep);
	EXPECT_EQ(fight::HelpEventOf(fight::Move::Kind::High), profile::k_FightAttack);
	EXPECT_EQ(fight::HelpEventOf(fight::Move::Kind::Mid), profile::k_FightAttack);
	EXPECT_EQ(fight::HelpEventOf(fight::Move::Kind::Low), profile::k_FightAttack);
	EXPECT_EQ(fight::HelpEventOf(fight::Move::Kind::Special), profile::k_FightAttack);
}

TEST(CreatureFightHelpEvents, OnlyAMoveFromTheStanceInRangeCounts)
{
	EXPECT_TRUE(fight::CountsHelpEvent(fight::State::Stance, true, true));
	EXPECT_FALSE(fight::CountsHelpEvent(fight::State::Stance, false, true));
	EXPECT_FALSE(fight::CountsHelpEvent(fight::State::Stance, true, false));
	// The move that ends a block counts nothing
	EXPECT_FALSE(fight::CountsHelpEvent(fight::State::Block, true, true));
}

TEST(CreatureFightHelpEvents, TheFightEventsCountAsInterfaceActionsAndNotGestures)
{
	profile::HelpProfile record;
	record.Trigger(profile::k_FightAttack);
	EXPECT_EQ(record.Count(profile::k_FightAttack).Total(), 1u);
	EXPECT_EQ(record.Count(43).Total(), 1u);
	EXPECT_EQ(record.Count(profile::k_AnyEvent).Total(), 1u);
	EXPECT_EQ(record.Count(24).Total(), 0u);
	// At most once a turn
	record.Trigger(profile::k_FightAttack);
	EXPECT_EQ(record.Count(profile::k_FightAttack).Total(), 1u);
	record.EndTurn();
	record.Trigger(profile::k_FightAttack);
	EXPECT_EQ(record.Count(profile::k_FightAttack).Total(), 2u);
}

TEST(HelpStatsFile, TheFileIsTheFortyNineCountsWithNoHeader)
{
	EXPECT_EQ(helpstats::k_FileSize, 13132u);
	EXPECT_FALSE(helpstats::Read(std::vector<uint8_t>(helpstats::k_FileSize - 1)).has_value());
	EXPECT_FALSE(helpstats::Read(std::vector<uint8_t>(helpstats::k_FileSize + 1)).has_value());

	std::vector<uint8_t> bytes(helpstats::k_FileSize, 0);
	// The second count: a total of 267, its next slot 11, 64 times kept, its first time 132200
	const size_t second = sizeof(helpstats::EventRecord);
	bytes[second + 0] = 0x0B;
	bytes[second + 1] = 0x01;
	bytes[second + 8] = 11;
	bytes[second + 9] = 64;
	bytes[second + 12] = 0x68;
	bytes[second + 13] = 0x04;
	bytes[second + 14] = 0x02;
	const auto stats = helpstats::Read(bytes);
	ASSERT_TRUE(stats.has_value());
	EXPECT_EQ(stats->at(1).total, 267u);
	EXPECT_EQ(stats->at(1).timeHead, 11u);
	EXPECT_EQ(stats->at(1).timeCount, 64u);
	EXPECT_EQ(stats->at(1).times[0], 132200u);
	EXPECT_EQ(helpstats::Write(*stats), bytes);
}

TEST(HelpStatsFile, TheProfileKeepsEveryCountThroughTheFile)
{
	profile::HelpProfile record;
	record.Trigger(profile::k_FightBlock);
	record.EndTurn();
	record.EndTurn();
	record.Trigger(profile::k_FightBlock);
	record.Trigger(profile::k_CameraDrag);

	const auto bytes = helpstats::Write(profile::ToHelpStats(record));
	const auto stats = helpstats::Read(bytes);
	ASSERT_TRUE(stats.has_value());
	EXPECT_EQ(stats->at(profile::k_FightBlock).total, 2u);
	EXPECT_EQ(stats->at(profile::k_FightBlock).timeHead, 2u);
	EXPECT_EQ(stats->at(profile::k_FightBlock).timeCount, 2u);
	EXPECT_EQ(stats->at(profile::k_FightBlock).triggeredThisTurn, 1u);
	EXPECT_EQ(stats->at(profile::k_FightBlock).times[1], 200u);

	profile::HelpProfile loaded;
	profile::FromHelpStats(loaded, *stats);
	for (uint32_t event = 0; event < profile::k_EventCount; ++event)
	{
		EXPECT_EQ(loaded.Count(event).Total(), record.Count(event).Total()) << event;
		EXPECT_FLOAT_EQ(loaded.Count(event).SmoothedRate(), record.Count(event).SmoothedRate()) << event;
	}
	// The count of this turn holds: another block in the same turn does not count
	loaded.Trigger(profile::k_FightBlock);
	EXPECT_EQ(loaded.Count(profile::k_FightBlock).Total(), 2u);
}

TEST(HelpStatsFile, ASlotPastTheLastIsKeptToTheSlots)
{
	helpstats::HelpStats stats {};
	stats.at(3).timeHead = 70;
	stats.at(3).timeCount = 200;
	profile::HelpProfile loaded;
	profile::FromHelpStats(loaded, stats);
	const auto kept = loaded.Count(3).GetKept();
	EXPECT_EQ(kept.head, 6u);
	EXPECT_EQ(kept.kept, profile::k_TimesKept);
}

TEST(InterfaceCreatures, TheCameraGoesRoundAtSeventyAndFortyHigh)
{
	const auto eye = views::Eye(0.0f);
	EXPECT_NEAR(eye.x, 70.0f, k_Near);
	EXPECT_NEAR(eye.y, 40.0f, k_Near);
	EXPECT_NEAR(eye.z, 0.0f, k_Near);
	const auto quarter = views::Eye(glm::half_pi<float>());
	EXPECT_NEAR(quarter.x, 0.0f, 1e-3f);
	EXPECT_NEAR(quarter.z, 70.0f, k_Near);
	EXPECT_NEAR(views::Focus().y, 20.0f, k_Near);
}

TEST(InterfaceCreatures, TheDepthMakesACreatureFillTheSamePartOfItsBox)
{
	// A creature of size 1 in a box 230 pixels high
	EXPECT_NEAR(views::Depth(1.0f, 230, 1.0f), 900.0f * 15.0f / 230.0f, 1e-3f);
	// Twice as big, twice as far
	EXPECT_NEAR(views::Depth(2.0f, 230, 1.0f), 2.0f * views::Depth(1.0f, 230, 1.0f), 1e-3f);
	// The share of the screen that the interface takes divides it
	EXPECT_NEAR(views::Depth(1.0f, 230, 0.5f), 2.0f * views::Depth(1.0f, 230, 1.0f), 1e-3f);
	EXPECT_NEAR(views::Depth(1.0f, 230, 0.0f), views::Depth(1.0f, 230, 1.0f), 1e-3f);
	// A box smaller than the creature's height keeps the share as the depth
	EXPECT_NEAR(views::Depth(1.0f, 10, 1.0f), 10.0f / 15.0f, 1e-5f);
	// A creature with almost no height is at that height
	EXPECT_NEAR(views::Depth(0.001f, 230, 1.0f), 0.015f, 1e-6f);
}

TEST(InterfaceCreatures, ThePointIsUnderThePixelAtTheDepth)
{
	const auto view = views::View(0.3f);
	const auto projection = glm::perspective(glm::radians(60.0f), 4.0f / 3.0f, 1.0f, 10000.0f);
	const glm::vec2 screen {800.0f, 600.0f};
	const auto eye = views::Eye(0.3f);
	const auto forward = glm::normalize(views::Focus() - eye);

	// The middle of the screen is straight ahead
	const auto middle = views::PointFromScreen(view, projection, screen, {400.0f, 300.0f}, 50.0f);
	EXPECT_NEAR(glm::distance(middle, eye + (forward * 50.0f)), 0.0f, 1e-2f);

	// Any pixel is at the depth along the camera, and projects back onto the pixel
	const glm::vec2 pixel {155.0f, 507.0f};
	const auto point = views::PointFromScreen(view, projection, screen, pixel, 80.0f);
	EXPECT_NEAR(glm::dot(point - eye, forward), 80.0f, 1e-2f);
	auto clip = projection * view * glm::vec4(point, 1.0f);
	clip /= clip.w;
	EXPECT_NEAR((clip.x + 1.0f) * 0.5f * screen.x, pixel.x, 1e-2f);
	EXPECT_NEAR((1.0f - clip.y) * 0.5f * screen.y, pixel.y, 1e-2f);
}

TEST(InterfaceCreatures, TheBonesMiddleAndTheFeetGoToThePoint)
{
	const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, 200.0f));
	const std::array<glm::mat4, 2> bones {glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 7.0f, 1.0f)),
	                                      glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 9.0f, 3.0f))};
	const glm::vec3 point {10.0f, 30.0f, -20.0f};
	const auto move = views::MoveToPoint(point, model, bones, 5.0f);
	// The bones' middle across the land is at (101, 202), the feet at 5
	EXPECT_NEAR(move.x, 10.0f - 101.0f, k_Near);
	EXPECT_NEAR(move.y, 30.0f - 5.0f, k_Near);
	EXPECT_NEAR(move.z, -20.0f - 202.0f, k_Near);
	// Without bones the middle is the origin of the world
	const auto alone = views::MoveToPoint(point, model, {}, 5.0f);
	EXPECT_NEAR(alone.x, 10.0f, k_Near);
	EXPECT_NEAR(alone.z, -20.0f, k_Near);
}

TEST(InterfaceCreatures, TheSpaceKeepsTheWidthOfTheWorldView)
{
	const auto world = glm::perspective(glm::radians(50.0f), 16.0f / 9.0f, 1.0f, 1000.0f);
	const auto space = views::SpaceProjection(world);
	EXPECT_FLOAT_EQ(space[0][0], world[0][0]);
	EXPECT_FLOAT_EQ(space[1][1], world[0][0] * 4.0f / 3.0f);
	EXPECT_FLOAT_EQ(space[2][2], world[2][2]);
	EXPECT_FLOAT_EQ(space[3][2], world[3][2]);
}
