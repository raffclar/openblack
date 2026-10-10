/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Scenery/AnimatedStaticRules.h"
#include "Scenery/ObjectDrawList.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/AnimatedStaticSystem.h"

using namespace openblack;
using namespace openblack::animated_static;
using namespace openblack::ecs::components;

namespace
{
// A clip like the gate's: a hundred keyframes over ten seconds
constexpr uint32_t k_PlayTime = 10000;
constexpr size_t k_Frames = 100;
} // namespace

TEST(AnimatedStaticRules, AnOpenClipRestsTwoKeyframesShortOfItsEnd)
{
	EXPECT_EQ(OpenRestingPlace(k_PlayTime, k_Frames), 9800u);
	// The phone box's ten keyframes over a second
	EXPECT_EQ(OpenRestingPlace(1000, 10), 800u);
	// The cave entrance's 21 over 2133 ms, in whole milliseconds
	EXPECT_EQ(OpenRestingPlace(2133, 21), 1929u);
	EXPECT_EQ(OpenRestingPlace(k_PlayTime, 0), 0u);
	EXPECT_EQ(OpenRestingPlace(k_PlayTime, 1), 0u);
}

TEST(AnimatedStaticRules, OpenItPlaysOnToItsRestAndClosedBackToTheStart)
{
	const auto rest = OpenRestingPlace(k_PlayTime, k_Frames);
	EXPECT_EQ(StepClip(k_Open, 0, 30, rest), 30u);
	EXPECT_EQ(StepClip(k_Open, 9790, 30, rest), rest);
	EXPECT_EQ(StepClip(k_Open, rest, 30, rest), rest);
	EXPECT_EQ(StepClip(k_Closed, rest, 30, rest), rest - 30);
	EXPECT_EQ(StepClip(k_Closed, 20, 30, rest), 0u);
	EXPECT_EQ(StepClip(k_Closed, 0, 30, rest), 0u);
	// Any other word plays it back as closing does
	EXPECT_EQ(StepClip(2, 100, 30, rest), 70u);
	// Paused, nothing moves
	EXPECT_EQ(StepClip(k_Open, 500, 0, rest), 500u);
}

TEST(AnimatedStaticRules, ItIsMovingUnlessAtRestForItsState)
{
	const auto rest = OpenRestingPlace(k_PlayTime, k_Frames);
	EXPECT_FALSE(IsMoving(k_Closed, 0, rest));
	EXPECT_TRUE(IsMoving(k_Closed, 500, rest));
	// Closed but still standing open: about to close
	EXPECT_TRUE(IsMoving(k_Closed, rest, rest));
	EXPECT_FALSE(IsMoving(k_Open, rest, rest));
	EXPECT_TRUE(IsMoving(k_Open, 500, rest));
	// Opened but not yet begun
	EXPECT_TRUE(IsMoving(k_Open, 0, rest));
	// Another word rests at either end
	EXPECT_FALSE(IsMoving(2, 0, rest));
	EXPECT_FALSE(IsMoving(2, rest, rest));
	EXPECT_TRUE(IsMoving(2, 500, rest));
}

TEST(AnimatedStaticRules, TheCarvedStonesAreGateStonesAndTheRockIsNot)
{
	EXPECT_TRUE(IsGateStoneKind(MobileStaticInfo::GateTotemApe));
	EXPECT_FALSE(IsGateStoneKind(MobileStaticInfo::GateTotemBlank));
	EXPECT_FALSE(IsGateStoneKind(MobileStaticInfo::GateTotemCow));
	EXPECT_FALSE(IsGateStoneKind(MobileStaticInfo::RockChalk));
}

TEST(AnimatedStaticRules, StonesFillTheSlotsInOrderAndCountByTheirCarving)
{
	GateStones stones {};
	EXPECT_EQ(GateStoneValue(stones), 0u);
	EXPECT_TRUE(AddGateStone(stones, MeshId::ObjectGateTotemTiger));
	EXPECT_EQ(GateStoneValue(stones), 2u);
	EXPECT_TRUE(AddGateStone(stones, MeshId::ObjectGateTotemApe));
	EXPECT_EQ(GateStoneValue(stones), 3u);
	EXPECT_TRUE(AddGateStone(stones, MeshId::ObjectGateTotemCow));
	EXPECT_EQ(GateStoneValue(stones), 7u);
	EXPECT_EQ(stones[0], MeshId::ObjectGateTotemTiger);
	EXPECT_EQ(stones[1], MeshId::ObjectGateTotemApe);
	EXPECT_EQ(stones[2], MeshId::ObjectGateTotemCow);
	// Full, it takes no more
	EXPECT_FALSE(AddGateStone(stones, MeshId::ObjectGateTotemApe));
	EXPECT_EQ(GateStoneValue(stones), 7u);
	// A model that isn't a carved stone is worth nothing
	GateStones blank {MeshId::ObjectGateTotemBlank};
	EXPECT_EQ(GateStoneValue(blank), 0u);
}

TEST(AnimatedStaticRules, EachThingCollidesWithTheModelForItsState)
{
	const GateStones none {};
	const GateStones one {MeshId::ObjectGateTotemTiger};
	const GateStones two {MeshId::ObjectGateTotemTiger, MeshId::ObjectGateTotemApe};
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::NorseGate, k_Closed, none), MeshId::NorseGatePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::NorseGate, k_Open, none), MeshId::NorseGatePhys2);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Closed, none), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Closed, one), MeshId::GateTotemPlinthePhys2);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Closed, two), MeshId::GateTotemPlinthePhys3);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Open, two), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::PhoneBox, k_Closed, none), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::PiperCaveEntrance, k_Open, none), MeshId::PiperEntrancePhys1);
	EXPECT_FALSE(CollisionModel(AnimatedStaticInfo::ChessKingTeamA, k_Closed, none).has_value());
}

TEST(AnimatedStaticRules, ClosedThePlinthStacksItsStonesOnItsTop)
{
	const PlinthLook look {.openState = k_Closed,
	                       .moving = false,
	                       .place = 0,
	                       .playTime = k_PlayTime,
	                       .plinthHalfHeight = 1.5f,
	                       .stoneHalfHeights = {1.0f, std::nullopt, 0.5f}};
	const auto draws = PlinthStoneDraws(look);
	ASSERT_EQ(draws.size(), 2u);
	// On the plinth's top, sat 0.2 into it; the third slot rises by twice its own height
	EXPECT_EQ(draws[0].slot, 0u);
	EXPECT_FLOAT_EQ(draws[0].lift, 2.8f);
	EXPECT_TRUE(draws[0].pickable);
	EXPECT_EQ(draws[1].slot, 2u);
	EXPECT_FLOAT_EQ(draws[1].lift, 4.8f);
	EXPECT_TRUE(draws[1].pickable);
}

TEST(AnimatedStaticRules, OpeningThePlinthSinksItsStonesWithItsClip)
{
	PlinthLook look {.openState = k_Open,
	                 .moving = true,
	                 .place = 0,
	                 .playTime = k_PlayTime,
	                 .plinthHalfHeight = 1.5f,
	                 .stoneHalfHeights = {1.0f, 1.0f, std::nullopt}};
	auto draws = PlinthStoneDraws(look);
	ASSERT_EQ(draws.size(), 2u);
	EXPECT_FLOAT_EQ(draws[0].lift, 2.8f);
	EXPECT_FLOAT_EQ(draws[1].lift, 4.8f);
	EXPECT_FALSE(draws[0].pickable);
	// Half way through the sinking, each has gone down by 7.15 of its own heights
	look.place = static_cast<uint32_t>((k_PlayTime - 3) / 2);
	draws = PlinthStoneDraws(look);
	const float half = static_cast<float>(look.place) / static_cast<float>(k_PlayTime - 3);
	EXPECT_FLOAT_EQ(draws[0].lift, 3.0f - (7.15f * 2.0f * half) - 0.2f);
	EXPECT_FLOAT_EQ(draws[1].lift, (3.0f + (2.0f - (7.15f * 2.0f * half))) - 0.2f);
	// At rest open, they are gone
	look.moving = false;
	look.place = OpenRestingPlace(k_PlayTime, k_Frames);
	EXPECT_TRUE(PlinthStoneDraws(look).empty());
}

TEST(AnimatedStaticRules, TheGateIsARowOfFifteenSmallCirclesFromItsMiddleOutwards)
{
	// A gate 30 wide (half width 15) of size 1, across the x axis: a step of 2 between circles
	const auto circles = GateRouteCircles(glm::vec2(100.0f, 50.0f), glm::vec2(1.0f, 0.0f), 15.0f, 1.0f, false);
	ASSERT_EQ(circles.size(), k_GateRouteCircles);
	const std::array<float, k_GateRouteCircles> along {0.0f,  2.0f,  -2.0f,  4.0f,  -4.0f,  6.0f,  -6.0f, 8.0f,
	                                                   -8.0f, 10.0f, -10.0f, 12.0f, -12.0f, 14.0f, -14.0f};
	for (size_t i = 0; i < circles.size(); ++i)
	{
		EXPECT_NEAR(circles[i].centre.x, 100.0f + along.at(i), 1e-4f) << i;
		EXPECT_FLOAT_EQ(circles[i].centre.y, 50.0f) << i;
		EXPECT_FLOAT_EQ(circles[i].radius, k_GateRouteRadius) << i;
	}
}

TEST(AnimatedStaticRules, AnOpenStillGateLeavesOutItsMiddleFive)
{
	const auto circles = GateRouteCircles(glm::vec2(0.0f), glm::vec2(0.0f, 1.0f), 15.0f, 1.0f, true);
	ASSERT_EQ(circles.size(), k_GateRouteCircles - k_GateRouteGap);
	// The first left is the third step out, then they alternate on outwards
	EXPECT_NEAR(circles.front().centre.y, 6.0f, 1e-4f);
	EXPECT_NEAR(circles.at(1).centre.y, -6.0f, 1e-4f);
	EXPECT_NEAR(circles.back().centre.y, -14.0f, 1e-4f);
	for (const auto& circle : circles)
	{
		EXPECT_GE(std::abs(circle.centre.y), 6.0f - 1e-4f);
	}
}

TEST(AnimatedStaticRules, AResizedGateSpreadsItsCirclesByItsSizeTwice)
{
	// The across direction comes sized as the gate is, and the step is sized again
	const auto circles = GateRouteCircles(glm::vec2(0.0f), glm::vec2(2.0f, 0.0f), 15.0f, 2.0f, false);
	ASSERT_EQ(circles.size(), k_GateRouteCircles);
	EXPECT_NEAR(circles.at(1).centre.x, 8.0f, 1e-4f);
	EXPECT_NEAR(circles.back().centre.x, -56.0f, 1e-3f);
	EXPECT_FLOAT_EQ(circles.back().radius, k_GateRouteRadius);
}

TEST(AnimatedStaticRules, AClosingPlinthDrawsItsStonesBothSinkingAndStacked)
{
	const PlinthLook look {.openState = k_Closed,
	                       .moving = true,
	                       .place = 5000,
	                       .playTime = k_PlayTime,
	                       .plinthHalfHeight = 1.0f,
	                       .stoneHalfHeights = {1.0f, std::nullopt, std::nullopt}};
	const auto draws = PlinthStoneDraws(look);
	ASSERT_EQ(draws.size(), 2u);
	EXPECT_FALSE(draws[0].pickable);
	EXPECT_TRUE(draws[1].pickable);
	EXPECT_LT(draws[0].lift, draws[1].lift);
}

class AnimatedStaticSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_info = std::make_unique<InfoConstants>();
		// As in the game's tables, the carved stones share the ape stone's kind; the rock has its own
		for (const auto stone :
		     {MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemCow, MobileStaticInfo::GateTotemTiger})
		{
			_info->mobileStatic.at(static_cast<size_t>(stone)).mobileType = MobileStaticInfo::GateTotemApe;
		}
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemBlank)).mobileType =
		    MobileStaticInfo::GateTotemBlank;
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemApe)).meshId = MeshId::ObjectGateTotemApe;
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemCow)).meshId = MeshId::ObjectGateTotemCow;
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemTiger)).meshId = MeshId::ObjectGateTotemTiger;
		Locator::infoConstants::emplace(*_info);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	entt::entity Still(AnimatedStaticInfo type)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<AnimatedStatic>(entity, AnimatedStatic {.type = type});
		return entity;
	}
	entt::entity Stone(MobileStaticInfo type)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<MobileStatic>(entity, type);
		return entity;
	}

	std::unique_ptr<InfoConstants> _info;
	ecs::systems::AnimatedStaticSystem _system;
};

TEST_F(AnimatedStaticSystemTest, AScriptOpensAndClosesOnlyAnimatedStatics)
{
	const auto gate = Still(AnimatedStaticInfo::NorseGate);
	EXPECT_TRUE(_system.SetOpenState(gate, k_Open));
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const AnimatedStatic>(gate).openState, k_Open);
	EXPECT_TRUE(_system.SetOpenState(gate, k_Closed));
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const AnimatedStatic>(gate).openState, k_Closed);
	EXPECT_FALSE(_system.SetOpenState(Stone(MobileStaticInfo::GateTotemApe), k_Open));
	EXPECT_FALSE(_system.SetOpenState(entt::null, k_Open));
}

TEST_F(AnimatedStaticSystemTest, ThePlinthTakesTheCarvedStonesAndCountsThem)
{
	const auto plinth = Still(AnimatedStaticInfo::GateStonePlinth);
	EXPECT_EQ(_system.GateStoneValue(plinth), 0u);
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemTiger)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 2u);
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemApe)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 3u);
	// The uncarved rock isn't taken
	EXPECT_FALSE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemBlank)));
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemCow)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 7u);
	// A full plinth still takes a stone, which is lost
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemApe)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 7u);
}

TEST_F(AnimatedStaticSystemTest, OnlyThePlinthTakesStones)
{
	const auto gate = Still(AnimatedStaticInfo::NorseGate);
	EXPECT_FALSE(_system.LayGateStone(gate, Stone(MobileStaticInfo::GateTotemTiger)));
	EXPECT_EQ(_system.GateStoneValue(gate), 0u);
	EXPECT_FALSE(_system.GateStoneValue(Stone(MobileStaticInfo::GateTotemTiger)).has_value());
}

namespace
{
// A camera at (0, 50, 0) looking along +z, a 60 degree view up and down on a 2:1 screen, its near plane at 1
constexpr float k_Near = 1.0f;
constexpr float k_Aspect = 2.0f;
const glm::vec3 k_Eye {0.0f, 50.0f, 0.0f};

object_draw_list::View TestView()
{
	const auto projection = glm::perspective(glm::radians(60.0f), k_Aspect, k_Near, 10000.0f);
	const auto view = glm::lookAt(k_Eye, k_Eye + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	return {.viewProjection = projection * view,
	        .nearPlane = k_Near,
	        .focalHeight = projection[1][1],
	        .aspect = k_Aspect,
	        .eye = k_Eye};
}
} // namespace

TEST(ObjectDrawList, ABlockIsInViewUnlessWhollyBeyondOneSide)
{
	const auto view = TestView();
	// Straight ahead
	EXPECT_TRUE(object_draw_list::BlockInView(view.viewProjection, view.nearPlane, {.corner = {-80.0f, 200.0f}}));
	// Behind the eye
	EXPECT_FALSE(object_draw_list::BlockInView(view.viewProjection, view.nearPlane, {.corner = {-80.0f, -400.0f}}));
	// Far off to the side
	EXPECT_FALSE(object_draw_list::BlockInView(view.viewProjection, view.nearPlane, {.corner = {2000.0f, 200.0f}}));
	// The eye stands over a flat block: all of it lies below the view
	EXPECT_FALSE(object_draw_list::BlockInView(view.viewProjection, view.nearPlane, {.corner = {-80.0f, -80.0f}}));
	// A tall one reaches above and below it, so no one side has all of it
	EXPECT_TRUE(object_draw_list::BlockInView(view.viewProjection, view.nearPlane,
	                                          {.corner = {-80.0f, -80.0f}, .highestAltitude = 255}));
}

TEST(ObjectDrawList, ATallBlockBelowTheViewIsSeenByItsHeight)
{
	// Looking up from high: a flat block far below is out of view, a block reaching up into the view is in it
	const auto projection = glm::perspective(glm::radians(20.0f), 1.0f, k_Near, 10000.0f);
	const glm::vec3 eye {0.0f, 100.0f, 0.0f};
	const auto viewProjection = projection * glm::lookAt(eye, eye + glm::vec3(0.0f, 0.2f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const object_draw_list::Block flat {.corner = {-80.0f, 300.0f}, .highestAltitude = 0, .reflected = false};
	object_draw_list::Block tall = flat;
	tall.highestAltitude = 255;
	EXPECT_FALSE(object_draw_list::BlockInView(viewProjection, k_Near, flat));
	EXPECT_TRUE(object_draw_list::BlockInView(viewProjection, k_Near, tall));
}

TEST(ObjectDrawList, ABlocksDistanceIsToItsMiddle)
{
	// Half way up its box without the reflection, at the water with it
	const object_draw_list::Block block {.corner = {0.0f, 0.0f}, .highestAltitude = 100, .reflected = false};
	EXPECT_FLOAT_EQ(object_draw_list::BlockDistance({80.0f, 33.5f, 80.0f}, block), 0.0f);
	object_draw_list::Block mirrored = block;
	mirrored.reflected = true;
	EXPECT_FLOAT_EQ(object_draw_list::BlockDistance({80.0f, 10.0f, 80.0f}, mirrored), 10.0f);
	// Listed only within 700
	const auto view = TestView();
	EXPECT_TRUE(object_draw_list::BlockListed(view.viewProjection, view.nearPlane, view.eye, {.corner = {-80.0f, 500.0f}}));
	EXPECT_FALSE(object_draw_list::BlockListed(view.viewProjection, view.nearPlane, view.eye, {.corner = {-80.0f, 700.0f}}));
}

TEST(ObjectDrawList, ASphereIsOnScreenWhenItsSquareOverlapsTheScreen)
{
	const auto view = TestView();
	const glm::vec3 ahead {0.0f, 50.0f, 100.0f};
	EXPECT_TRUE(object_draw_list::SphereOnScreen(view, ahead, ahead, 1.0f));
	// Behind the near plane
	const glm::vec3 behind {0.0f, 50.0f, -100.0f};
	EXPECT_FALSE(object_draw_list::SphereOnScreen(view, behind, behind, 1.0f));
	// The eye inside it, though its middle is behind
	EXPECT_TRUE(object_draw_list::SphereOnScreen(view, k_Eye, behind, 150.0f));
	// Just off the right of the screen: the screen's edge at 100 ahead is 100 tan(30) x 2 across
	const float edge = 100.0f * std::tan(glm::radians(30.0f)) * k_Aspect;
	const glm::vec3 right {-(edge + 5.0f), 50.0f, 100.0f};
	EXPECT_FALSE(object_draw_list::SphereOnScreen(view, right, right, 1.0f));
	EXPECT_TRUE(object_draw_list::SphereOnScreen(view, right, right, 6.0f));
	// Up and down its square reaches twice as far on this screen: a sphere of radius 3 just off the top shows
	const float top = 100.0f * std::tan(glm::radians(30.0f));
	const glm::vec3 above {0.0f, 50.0f + top + 5.0f, 100.0f};
	EXPECT_TRUE(object_draw_list::SphereOnScreen(view, above, above, 3.0f));
	EXPECT_FALSE(object_draw_list::SphereOnScreen(view, above, above, 2.0f));
}

TEST(ObjectDrawList, TheListIsMadeAgainEveryElevenTurnsOrWhenForced)
{
	object_draw_list::Clock clock;
	const glm::vec3 eye {0.0f};
	const glm::vec3 focus {0.0f, 0.0f, 10.0f};
	// The first frame makes it
	EXPECT_TRUE(clock.Next(100, eye, focus, false).rebuilt);
	EXPECT_FALSE(clock.Next(105, eye, focus, false).rebuilt);
	EXPECT_FALSE(clock.Next(110, eye, focus, false).rebuilt);
	EXPECT_TRUE(clock.Next(111, eye, focus, false).rebuilt);
	EXPECT_TRUE(clock.Next(111, eye, focus, true).rebuilt);
	clock.Reset();
	EXPECT_TRUE(clock.Next(112, eye, focus, false).rebuilt);
}

TEST(ObjectDrawList, AStillCameraDrawsOnlyWhatWasOnScreen)
{
	object_draw_list::Clock clock;
	const glm::vec3 focus {0.0f, 0.0f, 10.0f};
	// The list is made: all drawn, and the camera's place isn't taken
	EXPECT_TRUE(clock.Next(0, glm::vec3(0.0f), focus, false).drawAll);
	// The eye has moved from where it was taken (nowhere yet): all drawn, and now it is taken
	EXPECT_TRUE(clock.Next(1, glm::vec3(0.0f, 0.0f, 0.5f), focus, false).drawAll);
	// Moving less than one from it is standing still
	const auto still = clock.Next(2, glm::vec3(0.0f, 0.0f, 1.2f), focus + glm::vec3(0.9f, 0.0f, 0.0f), false);
	EXPECT_FALSE(still.drawAll);
	EXPECT_FALSE(object_draw_list::Drawn(still, true, false));
	EXPECT_TRUE(object_draw_list::Drawn(still, true, true));
	EXPECT_FALSE(object_draw_list::Drawn(still, false, true));
	// What it looks at moving more than one moves it
	EXPECT_TRUE(clock.Next(3, glm::vec3(0.0f, 0.0f, 0.5f), focus + glm::vec3(1.1f, 0.0f, 0.0f), false).drawAll);
}
