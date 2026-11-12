/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <numbers>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GameHeading.h"
#include "Creature/CreatureCameraTurn.h"
#include "Creature/CreatureIdleMind.h"

using namespace openblack;
namespace camera_turn = openblack::creature_camera_turn;
using camera_turn::Outcome;
using camera_turn::Phase;
using camera_turn::Watcher;

namespace
{
/// A body that starts a turn only when it is free. It keeps the headings that it was asked to turn to, and counts the
/// points that it was told to end.
struct FakeBody
{
	bool free {true};
	std::vector<GameHeading> asked;
	size_t pointsEnded {0};
};

Outcome StepWith(Phase& phase, const camera_turn::View& view, FakeBody& body, Watcher watcher = Watcher::ItsPlayer)
{
	return camera_turn::Step(
	    watcher, phase, view,
	    [&body](GameHeading heading) {
		    body.asked.push_back(heading);
		    return body.free;
	    },
	    [&body] { ++body.pointsEnded; });
}

/// A creature at the middle of the map, with the camera 50 metres to its +x side
camera_turn::View ViewWithCamera()
{
	return {.camera = glm::vec2(1050.0f, 1000.0f), .creature = glm::vec2(1000.0f, 1000.0f)};
}
} // namespace

// A creature without a player does not turn. The start and the check each take one turn.
TEST(CreatureCameraTurn, WithoutAPlayerTheStepTakesTwoTurnsAndDoesNotTurn)
{
	Phase phase {Phase::Start};
	FakeBody body;
	camera_turn::View view {.bodyBusy = true};
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	EXPECT_EQ(phase, Phase::Check);
	EXPECT_EQ(Outcome::Done, StepWith(phase, view, body));
	EXPECT_TRUE(body.asked.empty());
	EXPECT_EQ(body.pointsEnded, 0u);
}

// The start does not wait for the body: a busy body refuses the turn, and the creature ends its point and asks again
TEST(CreatureCameraTurn, TheStartAsksAgainAndEndsThePointWhileTheBodyRefuses)
{
	Phase phase {Phase::Start};
	FakeBody body {.free = false};
	auto view = ViewWithCamera();
	view.bodyBusy = true;
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	EXPECT_EQ(phase, Phase::Start);
	EXPECT_EQ(body.asked.size(), 2u);
	EXPECT_EQ(body.pointsEnded, 2u);
	body.free = true;
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	EXPECT_EQ(phase, Phase::Check);
	EXPECT_EQ(body.pointsEnded, 2u);
	// A camera on the +x side is a quarter turn round
	EXPECT_EQ(body.asked.back(), GameHeading::Of(50.0f, 0.0f));
	EXPECT_NEAR(body.asked.back().Radians(), std::numbers::pi_v<float> / 2.0f, 1e-6f);
}

// The start always asks for a turn, also when the creature faces the camera already
TEST(CreatureCameraTurn, TheStartTurnsAlsoWhenTheCreatureFacesTheCamera)
{
	Phase phase {Phase::Start};
	FakeBody body;
	auto view = ViewWithCamera();
	view.heading = GameHeading::Of(50.0f, 0.0f);
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	EXPECT_EQ(body.asked.size(), 1u);
	EXPECT_EQ(phase, Phase::Check);
}

// With the camera right above it, the creature goes on with no turn
TEST(CreatureCameraTurn, ACameraRightAboveNeedsNoTurn)
{
	Phase phase {Phase::Start};
	FakeBody body {.free = false};
	camera_turn::View view {.camera = glm::vec2(1000.05f, 1000.05f), .creature = glm::vec2(1000.0f, 1000.0f)};
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	EXPECT_EQ(phase, Phase::Check);
	EXPECT_EQ(Outcome::Done, StepWith(phase, view, body));
	EXPECT_TRUE(body.asked.empty());
	EXPECT_EQ(body.pointsEnded, 0u);
}

// The check waits for the body, then reads where the camera is now and turns again until it faces it
TEST(CreatureCameraTurn, TheCheckFollowsTheCameraWhereItIsNow)
{
	Phase phase {Phase::Check};
	FakeBody body;
	auto view = ViewWithCamera();
	view.heading = GameHeading::Of(50.0f, 0.0f);
	view.bodyBusy = true;
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	EXPECT_TRUE(body.asked.empty());
	// The camera moved to the -z side while the body turned
	view.bodyBusy = false;
	view.camera = glm::vec2(1000.0f, 950.0f);
	EXPECT_EQ(Outcome::Running, StepWith(phase, view, body));
	ASSERT_EQ(body.asked.size(), 1u);
	EXPECT_EQ(body.asked.back(), GameHeading::Of(0.0f, -50.0f));
	EXPECT_EQ(phase, Phase::Check);
	// Now it faces the camera
	view.heading = body.asked.back();
	EXPECT_EQ(Outcome::Done, StepWith(phase, view, body));
	EXPECT_EQ(body.asked.size(), 1u);
}

// Less than 0.2 of a radian off counts as facing; 0.2 or more does not
TEST(CreatureCameraTurn, TheCreatureFacesTheCameraWithinAFifthOfARadian)
{
	const GameHeading target {1.0f};
	EXPECT_TRUE(camera_turn::Facing(GameHeading(1.19f), target));
	EXPECT_TRUE(camera_turn::Facing(GameHeading(0.81f), target));
	EXPECT_FALSE(camera_turn::Facing(GameHeading(1.2f), target));
	EXPECT_FALSE(camera_turn::Facing(GameHeading(0.2f), GameHeading(0.0f)));
	EXPECT_FALSE(camera_turn::Facing(GameHeading(-0.2f), GameHeading(0.0f)));
}

// The difference does not go round the circle: just each side of the half turn the creature does not face the camera
TEST(CreatureCameraTurn, TheDifferenceDoesNotGoRoundTheCircle)
{
	EXPECT_FALSE(camera_turn::Facing(GameHeading(3.1f), GameHeading(-3.1f)));
	EXPECT_TRUE(camera_turn::Facing(GameHeading(3.1f), GameHeading(3.05f)));
}

// The creature's place becomes a map position first. The camera's place does not.
TEST(CreatureCameraTurn, TheWayToTheCameraStartsAtTheMapPosition)
{
	const glm::vec2 creature {1000.00001f, 2000.00002f};
	const glm::vec2 camera {1000.00004f, 2000.00008f};
	const auto offset = camera_turn::OffsetToCamera(creature, camera);
	const auto map = map_coords::ToMetres(map_coords::FromMetres(creature));
	EXPECT_EQ(offset.x, camera.x - map.x);
	EXPECT_EQ(offset.y, camera.y - map.y);
	EXPECT_FLOAT_EQ(camera_turn::GroundLength({3.0f, 4.0f}), 5.0f);
}

// The turn to the camera starts as the step starts, also while the body is busy. The step is a move that the creature's
// movement oversees.
TEST(CreatureCameraTurn, TheTurnStepStartsWhileTheBodyIsBusy)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned,
	                    {creature_mind::TurnToCamera(), {.kind = creature_mind::Step::Kind::Action, .animation = 55}});
	creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = true};
	const auto random = [](uint32_t) { return 0u; };
	const auto start = creature_mind::Think(mind, senses, random);
	ASSERT_TRUE(start.move.has_value());
	EXPECT_EQ(start.move->kind, creature_mind::Movement::Kind::TurnToFaceCamera);
	ASSERT_TRUE(start.gaze.has_value());
	EXPECT_TRUE(start.gaze->camera);
	EXPECT_TRUE(mind.stepStarted);
	// The movement tells the mind when the turn is done. The next step starts on the turn after that.
	static_cast<void>(creature_mind::Think(mind, senses, random));
	EXPECT_EQ(mind.step, 0u);
	senses.subMove = creature_mind::SubMove::Done;
	static_cast<void>(creature_mind::Think(mind, senses, random));
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.stepStarted);
}

// The turn to the giver's camera fails at its start when no player gave the creature a thing
TEST(CreatureCameraTurn, WithoutAGiverTheTurnToTheGiverFailsAtOnce)
{
	Phase phase {Phase::Start};
	FakeBody body;
	const camera_turn::View view {};
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Failed);
	EXPECT_EQ(phase, Phase::Start);
	EXPECT_TRUE(body.asked.empty());
}

// The start of the turn to the giver's camera does not wait for the body. While the body refuses, it asks again on the
// next turn, and it does not end a point.
TEST(CreatureCameraTurn, TheTurnToTheGiverAsksAgainAndDoesNotEndThePoint)
{
	Phase phase {Phase::Start};
	FakeBody body {.free = false};
	auto view = ViewWithCamera();
	view.bodyBusy = true;
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Running);
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Running);
	EXPECT_EQ(phase, Phase::Start);
	EXPECT_EQ(body.asked.size(), 2u);
	EXPECT_EQ(body.pointsEnded, 0u);
	body.free = true;
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Running);
	EXPECT_EQ(phase, Phase::Check);
	EXPECT_EQ(body.asked.back(), GameHeading::Of(50.0f, 0.0f));
}

// The check of the turn to the giver's camera waits for the body before it examines the giver. With the body free and
// the giver gone, the step fails.
TEST(CreatureCameraTurn, TheTurnToTheGiverExaminesTheBodyBeforeTheGiver)
{
	Phase phase {Phase::Check};
	FakeBody body;
	camera_turn::View view {.bodyBusy = true};
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Running);
	view.bodyBusy = false;
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Failed);
	EXPECT_TRUE(body.asked.empty());
}

// With a giver, the check of the turn to the giver's camera is the same as the check of the turn to its own camera
TEST(CreatureCameraTurn, TheTurnToTheGiverFollowsTheCameraUntilItFacesIt)
{
	Phase phase {Phase::Check};
	FakeBody body;
	auto view = ViewWithCamera();
	// It faces the +x side, and the camera is on the -z side
	view.heading = GameHeading::Of(50.0f, 0.0f);
	view.camera = glm::vec2(1000.0f, 950.0f);
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Running);
	ASSERT_EQ(body.asked.size(), 1u);
	EXPECT_EQ(body.asked.back(), GameHeading::Of(0.0f, -50.0f));
	view.heading = body.asked.back();
	EXPECT_EQ(StepWith(phase, view, body, Watcher::Giver), Outcome::Done);
	// A camera right above the creature also ends the step
	Phase start {Phase::Start};
	view.camera = glm::vec2(1000.05f, 1000.05f);
	EXPECT_EQ(StepWith(start, view, body, Watcher::Giver), Outcome::Running);
	EXPECT_EQ(start, Phase::Check);
	EXPECT_EQ(StepWith(start, view, body, Watcher::Giver), Outcome::Done);
	EXPECT_EQ(body.asked.size(), 1u);
}

// A creature that takes a thing from the hand first turns to the giver's camera. That turn starts while the body is busy.
// The take follows when the turn is done. When the turn fails, the creature gives up the agenda.
TEST(CreatureCameraTurn, TheTakeFromTheHandTurnsToTheGiverFirst)
{
	const auto agenda = creature_mind::TakeFromHand(7, true);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].kind, creature_mind::Step::Kind::Move);
	EXPECT_EQ(agenda[0].movement.kind, creature_mind::Movement::Kind::TurnToFaceGiverCamera);
	EXPECT_EQ(agenda[1].kind, creature_mind::Step::Kind::Object);
	EXPECT_EQ(agenda[1].order.kind, creature_mind::ObjectOrder::Kind::TakeFromHand);

	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, agenda);
	creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = true};
	const auto random = [](uint32_t) { return 0u; };
	const auto start = creature_mind::Think(mind, senses, random);
	ASSERT_TRUE(start.move.has_value());
	EXPECT_EQ(start.move->kind, creature_mind::Movement::Kind::TurnToFaceGiverCamera);
	EXPECT_TRUE(mind.stepStarted);
	senses.subMove = creature_mind::SubMove::Failed;
	static_cast<void>(creature_mind::Think(mind, senses, random));
	EXPECT_TRUE(mind.gaveUp);
	EXPECT_EQ(mind.step, agenda.size());
}
