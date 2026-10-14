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
