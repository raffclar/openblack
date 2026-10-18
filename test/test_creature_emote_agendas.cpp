/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <deque>
#include <memory>

#include <gtest/gtest.h>

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_emote;
using creature_mind::Effect;
using creature_mind::Movement;
using creature_mind::Step;

namespace
{
constexpr float k_TurnSeconds = 0.1f;

/// A random source that gives the numbers it was handed, in order, and 0 after them
creature_mind::Random Draws(std::deque<uint32_t> draws)
{
	auto queue = std::make_shared<std::deque<uint32_t>>(std::move(draws));
	return [queue](uint32_t range) {
		if (queue->empty())
		{
			return 0u;
		}
		const auto value = queue->front();
		queue->pop_front();
		return range == 0 ? 0u : value % range;
	};
}

creature_mind::Senses StandingSenses()
{
	return {.seconds = k_TurnSeconds};
}
} // namespace

TEST(CreatureEmoteAgendas, AFaceStepPullsTheFaceForItsSecondsAndEndsAtOnce)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {{.kind = Step::Kind::Face, .seconds = 5.0f, .animation = 18, .effect = Effect::Completed},
	                     {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	const auto commands = creature_mind::Think(mind, StandingSenses(), Draws({}));
	ASSERT_TRUE(commands.face.has_value());
	EXPECT_EQ(commands.face->face, creature_face::Face::Growl);
	EXPECT_FLOAT_EQ(commands.face->milliseconds, 5000.0f);
	EXPECT_EQ(commands.effect, Effect::Completed);
	// The next step starts on the next turn
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.stepStarted);
}

TEST(CreatureEmoteAgendas, FollowingTheCameraEndsWithNoPlayerOrWithItsTimeUp)
{
	uint32_t turns = 3;
	EXPECT_EQ(FollowCameraTurn({.focus = std::nullopt}, turns, 10.0f), FollowCameraAction::Done);
	turns = 0;
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(100.0f, 0.0f, 100.0f)}, turns, 10.0f), FollowCameraAction::Done);
}

TEST(CreatureEmoteAgendas, StandingItWalksToTheFocusUntilWithinReach)
{
	uint32_t turns = 10;
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(100.0f, 5.0f, 0.0f), .position = {0.0f, 0.0f}}, turns, 10.0f),
	          FollowCameraAction::MoveAgain);
	EXPECT_EQ(turns, 9u);
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(5.0f, 5.0f, 0.0f), .position = {0.0f, 0.0f}}, turns, 10.0f),
	          FollowCameraAction::Done);
	// A focus at the map's origin ends it as well
	EXPECT_EQ(FollowCameraTurn({.focus = glm::vec3(0.0f), .position = {500.0f, 500.0f}}, turns, 10.0f),
	          FollowCameraAction::Done);
}

TEST(CreatureEmoteAgendas, MovingItWalksAgainOnlyWhenTheFocusIsNoLongerAhead)
{
	uint32_t turns = 10;
	const FollowCameraView ahead {.focus = glm::vec3(100.0f, 0.0f, 0.0f), .moving = true, .focusAhead = true};
	EXPECT_EQ(FollowCameraTurn(ahead, turns, 10.0f), FollowCameraAction::Continue);
	auto behind = ahead;
	behind.focusAhead = false;
	EXPECT_EQ(FollowCameraTurn(behind, turns, 10.0f), FollowCameraAction::MoveAgain);
}

TEST(CreatureEmoteAgendas, TheFollowStepCountsWholeTurnsAndMovesShortOfTheReach)
{
	creature_mind::IdleMind mind;
	const float reach = FollowCameraReach(10.0f);
	EXPECT_FLOAT_EQ(reach, 22.0f);
	creature_mind::Plan(mind, creature_mind::Activity::Planned, BePathetic({0.0f, 50.0f}, 10.0f));
	auto senses = StandingSenses();
	senses.cameraFocus = glm::vec3(200.0f, 0.0f, 0.0f);
	// The first turn sets the time: thirty seconds of turns
	static_cast<void>(creature_mind::Think(mind, senses, Draws({})));
	EXPECT_EQ(mind.followCameraTurns, 300u);
	const auto commands = creature_mind::Think(mind, senses, Draws({}));
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::ToPoint);
	EXPECT_EQ(commands.move->point, glm::vec2(200.0f, 0.0f));
	EXPECT_FLOAT_EQ(commands.move->maxDistance, reach - k_FollowCameraStopShort);
	EXPECT_EQ(mind.followCameraTurns, 299u);
	// Within reach, it turns to the camera next
	senses.cameraFocus = glm::vec3(1.0f, 0.0f, 0.0f);
	static_cast<void>(creature_mind::Think(mind, senses, Draws({})));
	EXPECT_EQ(mind.step, 1u);
}

TEST(CreatureEmoteAgendas, PatheticFollowsTurnsAndPlaysPickMe)
{
	const auto agenda = BePathetic({3.0f, 4.0f}, 5.0f);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::FollowCamera);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->camera);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[2].animation, k_PickMeAnimation);
	for (const auto& step : agenda)
	{
		EXPECT_EQ(step.face, creature_face::Cue::None);
	}
}

TEST(CreatureEmoteAgendas, HowlTurnsToTheCameraAndHowls)
{
	const auto agenda = HowlAtPlayer({3.0f, 4.0f});
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(3.0f, 4.0f));
	EXPECT_EQ(agenda[1].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[1].animation, 217u);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::None);
}

TEST(CreatureEmoteAgendas, SillyFacesDrawThePlayThenThreeFaces)
{
	// 1 of 3 plays the playful clip; the faces are 16 + 4, 16 + 9 and 16 + 0
	const auto agenda = PullSillyFaces(7, 10.0f, Draws({1, 4, 9, 0}));
	ASSERT_EQ(agenda.size(), 6u);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 20.0f);
	EXPECT_EQ(agenda[2].animation, creature_layers::animations::k_FeelPlayful);
	EXPECT_EQ(agenda[3].animation, 20u);
	EXPECT_EQ(agenda[4].animation, 25u);
	EXPECT_EQ(agenda[5].animation, 16u);
	EXPECT_EQ(agenda[4].effect, Effect::Completed);
	EXPECT_EQ(agenda[3].effect, Effect::None);
	// 0 of 3 leaves the clip out
	EXPECT_EQ(PullSillyFaces(7, 10.0f, Draws({0})).size(), 5u);
}

TEST(CreatureEmoteAgendas, SadWaitsWholeTurnsLookingDownThenCriesOrMopes)
{
	// The float draw of half the range gives a little less than 1.5 more seconds, so the wait is 34 whole turns
	const auto agenda = BeSad(9, 10.0f, Draws({0x7FFF, 1}));
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Wait);
	EXPECT_NEAR(agenda[0].seconds, 3.4f, 1e-5f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Sad);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[0].gaze->object, 9u);
	EXPECT_TRUE(agenda[0].gaze->bottom);
	EXPECT_EQ(agenda[1].animation, k_CryAnimation);
	EXPECT_EQ(BeSad(9, 10.0f, Draws({0, 0}))[1].animation, creature_layers::animations::k_Sad);
}

TEST(CreatureEmoteAgendas, RunningAwayStartsFrightenedOneTimeInTwoAndLooksBack)
{
	const auto both = RunAway({.point = {10.0f, 0.0f}, .object = 5u}, Draws({0, 0}));
	ASSERT_EQ(both.size(), 3u);
	EXPECT_EQ(both[0].animation, k_FrightenedAnimation);
	EXPECT_EQ(both[0].heldEyes, creature_mind::Eyes::Wide);
	EXPECT_EQ(both[1].movement.kind, Movement::Kind::FleeFrom);
	EXPECT_EQ(both[1].face, creature_face::Cue::Fear);
	EXPECT_EQ(both[1].effect, Effect::RanAway);
	ASSERT_TRUE(both[1].gaze.has_value());
	EXPECT_TRUE(both[1].gaze->runningAway);
	EXPECT_EQ(both[1].gaze->object, 5u);
	EXPECT_EQ(both[1].heldEyes, creature_mind::Eyes::Wide);
	// From a thing it runs; from the hand it keeps the speed that it had
	EXPECT_TRUE(both[1].movement.run);
	const auto fromHand = RunAway({.point = {10.0f, 0.0f}}, Draws({1, 1}));
	ASSERT_EQ(fromHand.size(), 1u);
	EXPECT_TRUE(fromHand[0].gaze->hand);
	EXPECT_FALSE(fromHand[0].heldEyes.has_value());
	EXPECT_FALSE(fromHand[0].movement.run);
}

TEST(CreatureEmoteAgendas, TheRunLooksAheadThenBackInTurns)
{
	RunLookBack state;
	// Ahead for more than 1 s of turns (10), then back for more than 0.5 s (5)
	int back = 0;
	for (int turn = 0; turn < 11; ++turn)
	{
		back += RunLooksBack(state, 10.0f, 0.5f, 1.0f) ? 1 : 0;
	}
	EXPECT_EQ(back, 0);
	EXPECT_TRUE(state.lookingBack);
	for (int turn = 0; turn < 6; ++turn)
	{
		EXPECT_TRUE(RunLooksBack(state, 10.0f, 0.5f, 1.0f));
	}
	EXPECT_FALSE(state.lookingBack);
}

TEST(CreatureEmoteAgendas, StrokePicksUpStrokesHealsAndPutsDown)
{
	const auto agenda = StrokeAndHeal(8, false);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.animation, k_StrokeAnimation);
	EXPECT_EQ(agenda[1].effect, Effect::Completed);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Heal);
	EXPECT_EQ(agenda[2].object, 8u);
	EXPECT_EQ(agenda[3].order.animation, k_PutDownAnimation);
	for (size_t i = 1; i < agenda.size(); ++i)
	{
		EXPECT_EQ(agenda[i].face, creature_face::Cue::Compassion);
	}
	EXPECT_EQ(StrokeAndHeal(8, true).size(), 3u);
}

TEST(CreatureEmoteAgendas, TheSkyLookTurnsPointsAndAnEvilOneGrowlsAtTheMoon)
{
	const auto sun = LookAtSky(k_SunPoint, false, Draws({0}));
	ASSERT_EQ(sun.size(), 2u);
	EXPECT_EQ(sun[0].movement.point, glm::vec2(-50000.0f, -50000.0f));
	EXPECT_EQ(sun[1].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_EQ(sun[1].order.seconds, k_PointAtSkySeconds);
	EXPECT_EQ(LookAtSky(k_SunPoint, false, Draws({1})).size(), 1u);
	const auto moon = LookAtSky({4000.0f, 900.0f, 0.0f}, true, Draws({0}));
	ASSERT_EQ(moon.size(), 3u);
	EXPECT_EQ(moon[1].effect, Effect::Completed);
	EXPECT_EQ(moon[2].animation, k_AngryAnimation);
	EXPECT_EQ(moon[2].face, creature_face::Cue::Amazed);
	// Without the point, the growl is the last step, which completes the action
	EXPECT_EQ(LookAtSky({4000.0f, 900.0f, 0.0f}, true, Draws({1}))[0].effect, Effect::None);
}

TEST(CreatureEmoteAgendas, SleepingOnTheSpotWalksToTheClearPlaceWithoutFacingDownTheSlope)
{
	const auto agenda = SleepAtClearPlace({3.0f, 4.0f}, 2.0f, Draws({0}));
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_Tired);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(3.0f, 4.0f));
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 2.0f);
	EXPECT_TRUE(agenda[2].untilRested);
	EXPECT_EQ(agenda[3].animation, creature_layers::animations::k_Confused);
	EXPECT_EQ(SleepAtClearPlace({3.0f, 4.0f}, 2.0f, Draws({1})).size(), 3u);
}

TEST(CreatureEmoteAgendas, SittingByAThingSitsTenToFifteenSeconds)
{
	// The extra time is a draw of the game from 0 to 5 seconds, not cut to whole seconds
	const auto agenda = SitDownBy(3, {1.0f, 2.0f}, 4.0f, 4.5f);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[3].seconds, 14.5f);
	EXPECT_EQ(agenda[3].sequence[1], creature_layers::animations::k_Sit);
}

TEST(CreatureEmoteAgendas, TheRulesOfDayNightHomeAndTheHand)
{
	EXPECT_TRUE(SunVisible(false));
	EXPECT_FALSE(SunVisible(true));
	EXPECT_TRUE(MoonVisible(true));
	EXPECT_TRUE(NotTooFarFromHome(true, 999.0f));
	EXPECT_FALSE(NotTooFarFromHome(true, 1000.0f));
	EXPECT_FALSE(NotTooFarFromHome(true, std::nullopt));
	EXPECT_TRUE(LeashedOrTooFarFromHome(true, true, 10.0f));
	EXPECT_TRUE(LeashedOrTooFarFromHome(false, true, std::nullopt));
	EXPECT_FALSE(LeashedOrTooFarFromHome(false, true, 140.0f));
	EXPECT_TRUE(LeashedOrTooFarFromHome(false, true, 140.5f));
	EXPECT_TRUE(PlayerNearbyAndFrightening(9.0f, -0.1f));
	EXPECT_FALSE(PlayerNearbyAndFrightening(9.0f, 0.0f));
	EXPECT_FALSE(PlayerNearbyAndFrightening(10.0f, -0.5f));
	EXPECT_FALSE(PlayerNearbyAndFrightening(std::nullopt, -0.5f));
}

// With no temple of its player, a creature never sleeps at home, and it may sleep on the spot or by a thing near its home
TEST(CreatureEmoteAgendas, WithNoTempleItIsAsIfItHadNoHome)
{
	EXPECT_FALSE(NotTooFarFromHome(false, 10.0f));
	EXPECT_FALSE(NotTooFarFromHome(false, std::nullopt));
	EXPECT_TRUE(LeashedOrTooFarFromHome(false, false, 10.0f));
	EXPECT_TRUE(LeashedOrTooFarFromHome(false, false, 0.0f));
	EXPECT_FALSE(LeashedOrTooFarFromHome(false, true, 0.0f));
}

TEST(CreatureEmoteAgendas, RecentlyCountsWholeSecondsAndNeverForZero)
{
	EXPECT_FALSE(DoneWithin(0, 10, 2.0f));
	EXPECT_TRUE(DoneWithin(1, 10, 2.0f));
	// 19 turns are 1 whole second; 20 and 29 turns are 2 whole seconds, not fewer than 2
	EXPECT_TRUE(DoneWithin(19, 10, 2.0f));
	EXPECT_FALSE(DoneWithin(20, 10, 2.0f));
	EXPECT_FALSE(DoneWithin(29, 10, 2.0f));
	EXPECT_FALSE(ShowedOffRecently(0, 50));
	EXPECT_TRUE(ShowedOffRecently(100, 300));
	EXPECT_FALSE(ShowedOffRecently(100, 301));
}
