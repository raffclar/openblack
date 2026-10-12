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
#include <numbers>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFight.h"
#include "Creature/FightMarks.h"
#include "ECS/PosedModel.h"

using namespace openblack::creature_fight;
using namespace openblack::creature_fight::marks;
using openblack::ecs::posed_model::SkinPointPosition;

namespace
{
constexpr float k_Tolerance = 1e-4f;

Mark OnOpponent(uint32_t triangle)
{
	return {.anchor = Anchor::Opponent, .firstIndex = triangle};
}

void ExpectNear(const glm::vec3& actual, const glm::vec3& expected)
{
	EXPECT_NEAR(actual.x, expected.x, k_Tolerance);
	EXPECT_NEAR(actual.y, expected.y, k_Tolerance);
	EXPECT_NEAR(actual.z, expected.z, k_Tolerance);
}
} // namespace

TEST(FightMarks, ColoursByPlaceInTheQueue)
{
	// The front one's blue spills into its green: yellow, then paler, then orange, red and dark red
	constexpr std::array<uint32_t, MarkList::k_Capacity> k_Expected {
	    0xA0FFFF06, 0xA0FFFFC6, 0xA0FFFF85, 0xA0FFFF45, 0xA0FFFF05, 0xA0FFC300,
	    0xA0FF8300, 0xA0FF4200, 0xA0FF0200, 0xA0C10000, 0xA0800000, 0xA0400000,
	};
	for (size_t place = 0; place < k_Expected.size(); ++place)
	{
		EXPECT_EQ(ColourOf(place), k_Expected.at(place)) << "place " << place;
	}
}

TEST(FightMarks, RgbaTakesTheBytes)
{
	const auto colour = Rgba(0xA0FF8300);
	EXPECT_FLOAT_EQ(colour.r, 1.0f);
	EXPECT_FLOAT_EQ(colour.g, 131.0f / 255.0f);
	EXPECT_FLOAT_EQ(colour.b, 0.0f);
	EXPECT_FLOAT_EQ(colour.a, 160.0f / 255.0f);
}

TEST(FightMarks, FrameMovesOnByWholeEighthsOfTheCharge)
{
	EXPECT_EQ(NextFrame(0, 0.0f), 0u);
	EXPECT_EQ(NextFrame(0, 149.9f), 0u);
	EXPECT_EQ(NextFrame(0, 150.0f), 1u);
	EXPECT_EQ(NextFrame(3, 600.0f), 7u);
	EXPECT_EQ(NextFrame(10, 1200.0f), 2u);
	EXPECT_EQ(NextFrame(15, 5000.0f), 7u);
}

TEST(FightMarks, FrameUvIsEightToARow)
{
	const auto first = FrameUv(0);
	EXPECT_FLOAT_EQ(first.x, 0.0f);
	EXPECT_FLOAT_EQ(first.y, 0.0f);
	EXPECT_FLOAT_EQ(first.z, 0.125f);
	EXPECT_FLOAT_EQ(first.w, 0.125f);
	const auto last = FrameUv(15);
	EXPECT_FLOAT_EQ(last.x, 0.875f);
	EXPECT_FLOAT_EQ(last.y, 0.125f);
}

TEST(FightMarks, DrawnHalfWayToTheCamera)
{
	ExpectNear(DrawnAt({10.0f, 0.0f, 0.0f}, {30.0f, 20.0f, -10.0f}), {20.0f, 10.0f, -5.0f});
}

TEST(FightMarks, ListHoldsTwelve)
{
	MarkList list;
	for (size_t i = 0; i < MarkList::k_Capacity; ++i)
	{
		EXPECT_TRUE(list.Add(OnOpponent(i)));
	}
	EXPECT_TRUE(list.Full());
	EXPECT_FALSE(list.Add(OnOpponent(99)));
	EXPECT_EQ(list.Size(), MarkList::k_Capacity);
}

TEST(FightMarks, PopFrontShiftsTheRest)
{
	MarkList list;
	list.Add(OnOpponent(1));
	list.Add(OnOpponent(2));
	list.Add(OnOpponent(3));
	list.PopFront();
	ASSERT_EQ(list.Size(), 2u);
	EXPECT_EQ(list.Marks()[0].firstIndex, 2u);
	EXPECT_EQ(list.Marks()[1].firstIndex, 3u);
	list.DropNewest();
	ASSERT_EQ(list.Size(), 1u);
	EXPECT_EQ(list.Marks()[0].firstIndex, 2u);
}

TEST(FightMarks, KeepNewestKeepsTheFrontFrame)
{
	MarkList list;
	list.Add(OnOpponent(1));
	list.Charge(1200.0f);
	list.Animate();
	ASSERT_EQ(list.Marks()[0].frame, 8u);
	list.Add({.anchor = Anchor::Ground, .firstIndex = 7});
	list.Charge(300.0f);
	list.KeepNewest();
	ASSERT_EQ(list.Size(), 1u);
	EXPECT_EQ(list.Marks()[0].anchor, Anchor::Ground);
	EXPECT_EQ(list.Marks()[0].firstIndex, 7u);
	EXPECT_FLOAT_EQ(list.Marks()[0].chargeMs, 300.0f);
	EXPECT_EQ(list.Marks()[0].frame, 8u);
}

TEST(FightMarks, ChargeGoesToTheNewestUpToFull)
{
	MarkList list;
	list.Add(OnOpponent(1));
	list.Add(OnOpponent(2));
	list.Charge(700.0f);
	list.Charge(700.0f);
	EXPECT_FLOAT_EQ(list.Marks()[0].chargeMs, 0.0f);
	EXPECT_FLOAT_EQ(list.Marks()[1].chargeMs, 1200.0f);
}

TEST(FightMarks, OnlyBlowsOnTheOpponentFlicker)
{
	MarkList list;
	list.Add({.anchor = Anchor::Self});
	list.Charge(1200.0f);
	list.Add(OnOpponent(0));
	list.Charge(600.0f);
	list.Animate();
	list.Animate();
	EXPECT_EQ(list.Marks()[0].frame, 0u);
	EXPECT_EQ(list.Marks()[1].frame, 8u);
}

TEST(FightMarks, ASkinPointFollowsItsTriangle)
{
	// A line pressed onto a triangle, as a click on the opponent's skin is tested
	const std::vector<glm::vec3> corners {{0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 0.0f}};
	const std::vector<uint16_t> indices {0, 1, 2};
	const auto hit =
	    openblack::screen_pick::NearestSkinIntersection(corners, indices, {1.0f, 2.0f, -10.0f}, {0.0f, 0.0f, 1.0f});
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->s, 0.25f, k_Tolerance);
	EXPECT_NEAR(hit->t, 0.5f, k_Tolerance);
	ExpectNear(SkinPointPosition({corners[0], corners[1], corners[2]}, hit->s, hit->t), {1.0f, 2.0f, 0.0f});
	// The body moves and bends: the point stays at the same place on the triangle
	const std::array<glm::vec3, 3> moved {glm::vec3(10.0f, 0.0f, 5.0f), glm::vec3(10.0f, 0.0f, 13.0f),
	                                      glm::vec3(10.0f, 4.0f, 5.0f)};
	ExpectNear(SkinPointPosition(moved, hit->s, hit->t), {10.0f, 2.0f, 7.0f});
}

TEST(FightMarks, PointsGoInAndOutOfAFrame)
{
	const auto frame = GroundFrame({100.0f, 5.0f, 50.0f}, std::numbers::pi_v<float> / 2.0f);
	const glm::vec3 point {104.0f, 5.0f, 47.0f};
	const auto local = ToLocal(frame, point);
	ExpectNear(ToWorld(frame, local), point);
	// The creature turned on: the point turns with it
	const auto turned = ToWorld(GroundFrame({100.0f, 5.0f, 50.0f}, std::numbers::pi_v<float>), local);
	EXPECT_NEAR(glm::distance(turned, glm::vec3(100.0f, 5.0f, 50.0f)), 5.0f, k_Tolerance);
	EXPECT_GT(glm::distance(turned, point), 1.0f);
}

TEST(FightMarks, PaceStepsAtMostSixtyTimesASecond)
{
	Pace pace;
	EXPECT_TRUE(pace.Advance(0.0f));
	int steps = 0;
	for (int frame = 0; frame < 1010; ++frame)
	{
		steps += pace.Advance(1.0f) ? 1 : 0;
	}
	EXPECT_EQ(steps, 60);
	// A slow frame steps once, owing nothing after it
	Pace slow;
	EXPECT_TRUE(slow.Advance(0.0f));
	EXPECT_TRUE(slow.Advance(40.0f));
	EXPECT_FALSE(slow.Advance(1.0f));
}

TEST(FightMarks, TheFighterKeepsMarksInStepWithTheQueue)
{
	Fighter fighter;
	fighter.state = State::Stance;
	fighter.marks.Add({.anchor = Anchor::Self});
	PlayerMove(fighter, BlockMove(), false);
	fighter.marks.Add({.anchor = Anchor::Ground});
	PlayerMove(fighter, StepMove(Step::Forward), false);
	ASSERT_EQ(fighter.marks.Size(), 2u);
	// The block is made: its sparkle goes
	ASSERT_TRUE(TakeOrder(fighter, true).has_value());
	ASSERT_EQ(fighter.marks.Size(), 1u);
	EXPECT_EQ(fighter.marks.Marks()[0].anchor, Anchor::Ground);
	// A move in place of the queue keeps only its own sparkle
	fighter.marks.Add(OnOpponent(4));
	PlayerMove(fighter, AttackMove(Band::High), true);
	ASSERT_EQ(fighter.marks.Size(), 1u);
	EXPECT_EQ(fighter.marks.Marks()[0].firstIndex, 4u);
}
