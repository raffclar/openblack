/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <map>

#include <gtest/gtest.h>

#include "Creature/CaveCreature.h"
#include "Creature/CreatureLocomotion.h"

using namespace openblack;
using namespace openblack::cave_creature;
namespace animations = openblack::creature_layers::animations;

namespace
{
constexpr Places k_Places {.seat = {10.0f, -24.0f, 0.0f}, .fire = {10.0f, -24.0f, -20.0f}, .back = {30.0f, -20.0f, 0.0f}};

/// Draws a fixed number for each range, and 1 for any other: no yawn, no coin
Random Draws(std::map<uint32_t, uint32_t> draws)
{
	return [draws = std::move(draws)](uint32_t range) {
		const auto it = draws.find(range);
		return it != draws.end() ? it->second : 1u;
	};
}

/// A creature at the seat facing the fire
State FacingTheFire(Rest rest = Rest::Sit)
{
	return {.rest = rest, .heading = HeadingTowards(k_Places.seat, k_Places.fire), .idleMs = 30000.0f};
}

Frame NotZoomed()
{
	return {.zoomed = false, .fire = k_Places.fire, .position = k_Places.seat, .milliseconds = 16.0f};
}

Frame Zoomed()
{
	auto frame = NotZoomed();
	frame.zoomed = true;
	frame.eye = k_Places.seat + glm::vec3(-15.0f, 7.5f, 0.0f);
	frame.focus = k_Places.seat + glm::vec3(0.0f, 5.0f, 0.0f);
	return frame;
}
} // namespace

TEST(CaveCreature, SizeIsHeldWithinTheDrawnSizes)
{
	EXPECT_FLOAT_EQ(SizeOf(0.01f), 0.05f);
	EXPECT_FLOAT_EQ(SizeOf(1.5f), 1.5f);
	EXPECT_FLOAT_EQ(SizeOf(9.0f), 4.0f);
}

TEST(CaveCreature, OnlyASmallCreatureSleeps)
{
	EXPECT_EQ(RestFor(1.0f, true), Rest::Sleep);
	EXPECT_EQ(RestFor(1.0f, false), Rest::Sit);
	EXPECT_EQ(RestFor(1.25f, true), Rest::Sit);
	EXPECT_EQ(RestFor(3.0f, true), Rest::Sit);
}

TEST(CaveCreature, TheSmallerTheFurtherBack)
{
	// From size 2 up it is at the seat, at size 0 at the place further back; always at the seat's height
	EXPECT_EQ(StandsAt(k_Places, 2.0f, Rest::Sit), glm::vec3(10.0f, -24.0f, 0.0f));
	EXPECT_EQ(StandsAt(k_Places, 4.0f, Rest::Sit), glm::vec3(10.0f, -24.0f, 0.0f));
	EXPECT_EQ(StandsAt(k_Places, 1.0f, Rest::Sit), glm::vec3(20.0f, -24.0f, 0.0f));
	EXPECT_EQ(StandsAt(k_Places, 0.0f, Rest::Sit), glm::vec3(30.0f, -24.0f, 0.0f));
	// A sleeper goes twice as far less half the way
	EXPECT_EQ(StandsAt(k_Places, 1.0f, Rest::Sleep), glm::vec3(10.0f, -24.0f, 0.0f));
	EXPECT_EQ(StandsAt(k_Places, 0.5f, Rest::Sleep), glm::vec3(20.0f, -24.0f, 0.0f));
}

TEST(CaveCreature, FacingTheFireItSitsLookingIntoIt)
{
	auto state = FacingTheFire();
	Body body;
	const auto turn = Step(state, body, NotZoomed(), Draws({{2, 1}}));
	EXPECT_FALSE(turn.has_value());
	EXPECT_EQ(body.body.kind, creature_layers::BodyAction::Kind::Sequence);
	EXPECT_EQ(body.body.animations, (std::array<size_t, 3> {animations::k_StartSit, animations::k_Sit, animations::k_EndSit}));
	EXPECT_TRUE(body.body.mirrored);
	EXPECT_EQ(body.lookAt, k_Places.fire);
}

TEST(CaveCreature, ASleeperLiesDownAndLooksAtNothing)
{
	auto state = FacingTheFire(Rest::Sleep);
	Body body;
	(void)Step(state, body, NotZoomed(), Draws({{2, 0}}));
	EXPECT_EQ(body.body.animations,
	          (std::array<size_t, 3> {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}));
	EXPECT_FALSE(body.body.mirrored);
	EXPECT_FALSE(body.lookAt.has_value());
}

TEST(CaveCreature, NotFacingTheFireItGetsUpThenTurns)
{
	auto state = FacingTheFire();
	state.heading += 0.5f;
	Body body;
	body.body = *creature_layers::PlaySequence(body.body, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	body.body.phase = creature_layers::BodyAction::Phase::Loop;
	// Still sitting, the turn waits
	EXPECT_FALSE(Step(state, body, NotZoomed(), Draws({})).has_value());
	EXPECT_TRUE(body.body.endWanted);
	EXPECT_TRUE(state.wantedHeading.has_value());
	// Up again, it turns to the fire
	body.body = {};
	const auto turn = Step(state, body, NotZoomed(), Draws({}));
	ASSERT_TRUE(turn.has_value());
	EXPECT_FLOAT_EQ(*turn, HeadingTowards(k_Places.seat, k_Places.fire));
	EXPECT_FALSE(state.wantedHeading.has_value());
}

TEST(CaveCreature, AHairOffIsFacing)
{
	EXPECT_FALSE(MustTurn(1.0f, 1.0099f));
	EXPECT_TRUE(MustTurn(1.0f, 1.0101f));
}

TEST(CaveCreature, ATurnEndsFacingTheNewWay)
{
	State state {.heading = 0.0f};
	state.turn = State::Turn {
	    .from = 0.0f, .to = 1.0f, .timeMs = 0.0f, .durationMs = 100.0f, .spin = 3, .blendedWith = 4, .blendWeight = 0.5f};
	Body body;
	EXPECT_TRUE(Busy(state, body));
	AdvanceTurn(state, 60.0f);
	EXPECT_FLOAT_EQ(state.heading, 0.0f);
	AdvanceTurn(state, 40.0f);
	EXPECT_FLOAT_EQ(state.heading, 1.0f);
	EXPECT_FALSE(state.turn.has_value());
	EXPECT_FALSE(Busy(state, body));
}

TEST(CaveCreature, ZoomedOntoItTurnsToTheCamera)
{
	auto state = FacingTheFire();
	Body body;
	const auto frame = Zoomed();
	const auto turn = Step(state, body, frame, Draws({}));
	ASSERT_TRUE(turn.has_value());
	EXPECT_FLOAT_EQ(*turn, HeadingTowards(frame.focus, frame.eye));
	EXPECT_TRUE(state.zoomed);
	// Only as the zoom starts
	EXPECT_FALSE(Step(state, body, frame, Draws({})).has_value());
}

TEST(CaveCreature, AButtonHeldBrieflyHasItLookAtThePointer)
{
	auto state = FacingTheFire();
	Body body;
	auto frame = Zoomed();
	frame.buttonHeld = true;
	frame.pointer = {1.0f, 2.0f, 3.0f};
	frame.milliseconds = 1000.0f;
	(void)Step(state, body, frame, Draws({}));
	EXPECT_EQ(body.lookAt, frame.pointer);
	(void)Step(state, body, frame, Draws({}));
	EXPECT_EQ(body.lookAt, frame.pointer);
	// A second and a half on it looks away
	(void)Step(state, body, frame, Draws({}));
	EXPECT_FALSE(body.lookAt.has_value());
	frame.buttonHeld = false;
	(void)Step(state, body, frame, Draws({}));
	EXPECT_FALSE(state.heldMs.has_value());
}

TEST(CaveCreature, ItYawnsNowAndThenWhileSitting)
{
	auto state = FacingTheFire();
	Body body;
	(void)Step(state, body, NotZoomed(), Draws({{k_YawnRange, k_YawnDraw}}));
	EXPECT_EQ(body.gesture.animation, animations::k_Yawn);

	auto sleeper = FacingTheFire(Rest::Sleep);
	Body sleeping;
	(void)Step(sleeper, sleeping, NotZoomed(), Draws({{k_YawnRange, k_YawnDraw}}));
	EXPECT_FALSE(sleeping.gesture.animation.has_value());
}

TEST(CaveCreature, ItFidgetsWhileTheEditorIsOpen)
{
	auto state = FacingTheFire();
	state.zoomed = true;
	StartIdle(state, Draws({{k_IdleRandomMs, 5000}}));
	EXPECT_FLOAT_EQ(state.idleMs, 25000.0f);
	Body body;
	auto frame = Zoomed();
	frame.milliseconds = 25000.0f;
	(void)Step(state, body, frame, Draws({{3, 0}, {k_IdleRandomMs, 0}}));
	EXPECT_EQ(body.body.kind, creature_layers::BodyAction::Kind::Once);
	EXPECT_EQ(creature_layers::CurrentAnimation(body.body), animations::k_Impress);
	EXPECT_EQ(state.fidgets, 1u);
	EXPECT_FLOAT_EQ(state.idleMs, k_IdleBaseMs);

	// From the tenth it sits down
	body = {};
	state.fidgets = k_IdleFidgets - 1;
	frame.milliseconds = k_IdleBaseMs;
	(void)Step(state, body, frame, Draws({{k_IdleRandomMs, 0}}));
	EXPECT_EQ(creature_layers::CurrentAnimation(body.body), animations::k_StartSit);
}

TEST(CaveCreature, TheLeftButtonStartsTheFidgetsAgain)
{
	auto state = FacingTheFire();
	state.zoomed = true;
	state.idleMs = 3000.0f;
	state.fidgets = 4;
	Body body;
	body.body = *creature_layers::PlaySequence(body.body, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	auto frame = Zoomed();
	frame.leftHeld = true;
	frame.buttonHeld = true;
	(void)Step(state, body, frame, Draws({{k_IdleRandomMs, 100}}));
	EXPECT_EQ(state.fidgets, 0u);
	EXPECT_FLOAT_EQ(state.idleMs, 20100.0f);
	EXPECT_TRUE(body.body.endWanted);
}
