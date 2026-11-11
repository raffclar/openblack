/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlayerAgendas.h"
#include "Creature/CreaturePlayerAttention.h"
#include "ECS/Components/HandStillness.h"
#include "Hand/HandStillness.h"

using namespace openblack;
using namespace openblack::creature_player_attention;
using creature_mind::ObjectOrder;
using creature_mind::Step;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
uint32_t AlwaysFirst(uint32_t /*range*/)
{
	return 0;
}

uint32_t NeverFirst(uint32_t range)
{
	return range - 1;
}

/// Gives the numbers of a list in turn, and records the ranges asked for
struct Scripted
{
	std::vector<uint32_t> values;
	std::vector<uint32_t> ranges;
	size_t next {0};
	uint32_t operator()(uint32_t range)
	{
		ranges.push_back(range);
		return values.at(next++);
	}
};

map_coords::MapCoords At(float x, float z)
{
	return {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z)};
}
} // namespace

TEST(CreaturePlayerAttention, ItIsCrossOnlyBelowATenthUnderNothing)
{
	EXPECT_FALSE(IsCross(-0.1f));
	EXPECT_TRUE(IsCross(-0.1001f));
	EXPECT_FALSE(IsCross(0.5f));
}

// The hand is away when it is further than an eighth of the view width at the focus: tan(0.875) x the focus distance / 8
TEST(CreaturePlayerAttention, AwayFromTheHandIsAnEighthOfTheViewWidthAtTheFocus)
{
	const glm::vec3 origin {0.0f, 100.0f, 0.0f};
	const glm::vec3 focus {0.0f, 0.0f, 0.0f};
	const auto limit = static_cast<float>(std::tan(0.875) * 100.0 * 0.125);
	EXPECT_FALSE(AwayFromHand({1000.0f, 0.0f, 1000.0f}, {1000.0f + limit - 0.01f, 0.0f, 1000.0f}, origin, focus));
	EXPECT_TRUE(AwayFromHand({1000.0f, 0.0f, 1000.0f}, {1000.0f + limit + 0.01f, 0.0f, 1000.0f}, origin, focus));
	// The height of the hand does not count
	EXPECT_FALSE(AwayFromHand({1000.0f, 0.0f, 1000.0f}, {1000.0f, 500.0f, 1000.0f}, origin, focus));
}

// The velocity of the hand follows six tenths of the way to the movement of the turn made a velocity, so the hand moves
// for many turns after it stops
TEST(CreaturePlayerAttention, TheHandMovesForManyTurnsAfterItStops)
{
	openblack::ecs::components::HandStillness hand;
	EXPECT_FALSE(IsHandMoving(hand.velocity));
	openblack::hand_stillness::Step(hand, {1.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(hand.velocity.x, 6.0f);
	EXPECT_TRUE(IsHandMoving(hand.velocity));
	for (int turn = 0; turn < 50; ++turn)
	{
		openblack::hand_stillness::Step(hand, {1.0f, 0.0f, 0.0f});
	}
	EXPECT_TRUE(IsHandMoving(hand.velocity));
	for (int turn = 0; turn < 500; ++turn)
	{
		openblack::hand_stillness::Step(hand, {1.0f, 0.0f, 0.0f});
	}
	EXPECT_FALSE(IsHandMoving(hand.velocity));
}

// The angles are compared straight: across the zero angle two near directions count as far apart, as in the game
TEST(CreaturePlayerAttention, TheDirectionIsCloseWithinASixthOfAHalfTurnNotRoundTheCircle)
{
	const auto creature = At(1000.0f, 1000.0f);
	EXPECT_TRUE(IsDirectionClose(creature, At(1100.0f, 1000.0f), 0));
	EXPECT_TRUE(IsDirectionClose(creature, At(1100.0f, 1000.0f), 340));
	EXPECT_FALSE(IsDirectionClose(creature, At(1100.0f, 1000.0f), 341));
	// Just below the zero angle the point is a few steps round, but counts as far
	EXPECT_FALSE(IsDirectionClose(creature, At(1100.0f, 999.0f), 10));
	// In the creature's own map cell it is close whatever the angle
	EXPECT_TRUE(IsDirectionClose(At(1001.0f, 1001.0f), At(1002.0f, 1003.0f), 1024));
}

TEST(CreaturePlayerAttention, FollowingTheHandLastsThirtySecondsAndStopsFourPastTheRadius)
{
	EXPECT_EQ(FollowHandTurns(10), 300u);
	EXPECT_FLOAT_EQ(FollowHandReach(6.0f), 10.0f);
}

TEST(CreaturePlayerAttention, AFollowHandTurnWalksUntilNearThenIsDone)
{
	uint32_t turns = 3;
	FollowHandView view {.hasPlayer = true, .handInMap = true, .distance = 50.0f, .reach = 10.0f};
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::WalkToHand);
	EXPECT_EQ(turns, 2u);
	// While the body does an action, a walk included, it walks again only when the hand is not ahead
	view.bodyBusy = true;
	view.directionClose = true;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Continue);
	view.directionClose = false;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::WalkToHand);
	EXPECT_EQ(turns, 0u);
	// With the turns used up it is done
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Done);
	turns = 5;
	view.bodyBusy = false;
	view.distance = 9.9f;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Done);
	// A hand at the origin of the map on the ground ends the step when the body is free
	view.distance = 50.0f;
	view.handAtOrigin = true;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Done);
	view.handAtOrigin = false;
	view.handInMap = false;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Fail);
	view.hasPlayer = false;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Done);
}

TEST(CreaturePlayerAttention, AFollowHandTurnIsNotDoneNearTheHandWhileTheCreatureWalks)
{
	uint32_t turns = 10;
	// It walks to the hand, and the hand is ahead: the walk continues, also within the distance where it stops
	FollowHandView view {
	    .hasPlayer = true, .handInMap = true, .distance = 5.0f, .reach = 10.0f, .bodyBusy = true, .directionClose = true};
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Continue);
	// The hand is not ahead: it walks to the hand again
	view.directionClose = false;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::WalkToHand);
	// The walk ended: it is near the hand
	view.bodyBusy = false;
	EXPECT_EQ(FollowHandTurn(view, turns), FollowHandAction::Done);
}

TEST(CreaturePlayerAttention, WatchingThePlayerWaitsWhileTheBodyTurns)
{
	WatchPlayerView view {.waitingForOrders = true, .hasPlayer = true, .bodyBusy = true, .handMovement = 0.0f};
	view.camera = WatchPlayerView::Target {.point = {3.0f, 4.0f}, .distance = 50.0f, .turn = -2.0f};
	const auto action = WatchPlayerTurn(view);
	EXPECT_FALSE(action.done);
	EXPECT_FALSE(action.turnTo.has_value());
}

TEST(CreaturePlayerAttention, WatchingThePlayerTurnsToTheHandWhenItMovedMuchElseToTheCamera)
{
	WatchPlayerView view {.waitingForOrders = true, .hasPlayer = true, .handMovement = 31.0f};
	view.hand = WatchPlayerView::Target {.point = {1.0f, 2.0f}, .distance = 5.0f, .turn = 2.0f};
	view.camera = WatchPlayerView::Target {.point = {3.0f, 4.0f}, .distance = 50.0f, .turn = -2.0f};
	auto action = WatchPlayerTurn(view);
	ASSERT_TRUE(action.turnTo.has_value());
	EXPECT_EQ(*action.turnTo, glm::vec2(1.0f, 2.0f));
	view.handMovement = 30.0f;
	action = WatchPlayerTurn(view);
	ASSERT_TRUE(action.turnTo.has_value());
	EXPECT_EQ(*action.turnTo, glm::vec2(3.0f, 4.0f));
	// Within a quarter turn it does not turn
	view.camera->turn = 1.5f;
	EXPECT_FALSE(WatchPlayerTurn(view).turnTo.has_value());
	EXPECT_FALSE(WatchPlayerTurn(view).done);
	// Done once it waits for orders no more
	view.waitingForOrders = false;
	EXPECT_TRUE(WatchPlayerTurn(view).done);
}

TEST(CreaturePlayerAgendas, GoingToTheHandFollowsItThenIsHappyOneTimeInTwo)
{
	const auto happy = GoToHand(AlwaysFirst);
	ASSERT_EQ(happy.size(), 2u);
	EXPECT_EQ(happy[0].kind, Step::Kind::FollowHand);
	EXPECT_TRUE(happy[0].gaze->hand);
	EXPECT_EQ(happy[1].animation, k_HappyAnimation);
	EXPECT_EQ(GoToHand(NeverFirst).size(), 1u);
}

TEST(CreaturePlayerAgendas, PointingAtTheHandTurnsToTheCameraThenPointsWhereTheHandIs)
{
	const auto agenda = PointAtHand({5.0f, 6.0f}, {1.0f, 2.0f, 3.0f});
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_TRUE(agenda[0].gaze->camera);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(agenda[1].order.point, glm::vec2(1.0f, 3.0f));
	EXPECT_EQ(agenda[1].order.pointHeight, 2.0f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Amazed);
}

TEST(CreaturePlayerAgendas, HowNiceItThinksThePlayerIsIsAWaveOrTheSadClip)
{
	EXPECT_EQ(ShowHowNice({}, 0.1f)[1].animation, k_FriendlyWaveAnimation);
	EXPECT_EQ(ShowHowNice({}, 0.0f)[1].animation, k_SadAnimation);
	EXPECT_EQ(PrayToPlayer({})[1].animation, k_PrayAnimation);
}

TEST(CreaturePlayerAgendas, BeingCrossFollowsTheCameraThenMakesARudeSignOrTaunts)
{
	const auto rude = BeCrossWithPlayer(glm::vec2 {}, 10.0f, NeverFirst);
	ASSERT_EQ(rude.size(), 3u);
	EXPECT_EQ(rude[0].kind, Step::Kind::FollowCamera);
	EXPECT_EQ(rude[2].animation, k_RudeSignAnimation);
	EXPECT_EQ(BeCrossWithPlayer(glm::vec2 {}, 10.0f, AlwaysFirst)[2].animation, k_TauntAnimation);
	// Without a camera the game still draws for the sign first, and the creature makes it with no turn to the camera
	std::vector<uint32_t> draws;
	const auto noCamera = BeCrossWithPlayer(std::nullopt, 10.0f, [&draws](uint32_t range) {
		draws.push_back(range);
		return NeverFirst(range);
	});
	EXPECT_EQ(draws, std::vector<uint32_t> {2});
	ASSERT_EQ(noCamera.size(), 2u);
	EXPECT_EQ(noCamera[0].kind, Step::Kind::FollowCamera);
	EXPECT_EQ(noCamera[1].animation, k_RudeSignAnimation);
	EXPECT_EQ(FollowPlayer({}, 10.0f)[2].animation, k_LookAtMeAnimation);
	EXPECT_EQ(GoToMiddleOfScreen({}, 10.0f).size(), 2u);
}

TEST(CreaturePlayerAgendas, ShowingAThingPicksItUpUnlessHeldThenPutsItDown)
{
	const auto agenda = ShowPlayerAnObject(7u, {}, 10.0f);
	ASSERT_EQ(agenda.size(), 4u);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].kind, Step::Kind::FollowCamera);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::AttitudeToPlayer);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(agenda[3].order.animation, k_PutDownAnimation);
	EXPECT_EQ(ShowPlayerAnObject(std::nullopt, {}, 10.0f).size(), 3u);
}

// Each of the four rounds draws three in four, then one in two, then the face
TEST(CreaturePlayerAgendas, BeingSillyDoesFourRoundsOfTalkPlayLaughAndAFace)
{
	Scripted random {.values = {1, 0, 0, 0, 1, 9, 2, 1, 0, 3, 0, 5}};
	const auto agenda = BeSillyWithPlayer({}, std::ref(random));
	EXPECT_EQ(random.ranges, (std::vector<uint32_t> {4, 2, 10, 4, 2, 10, 4, 2, 10, 4, 2, 10}));
	// Round one: turn, talk, playful, laugh, talk, face 16
	EXPECT_EQ(agenda[0].movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Overlay);
	EXPECT_TRUE(agenda[1].flutterEyelids);
	EXPECT_EQ(agenda[2].animation, k_FeelPlayfulAnimation);
	EXPECT_EQ(agenda[3].animation, k_LaughAnimation);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Overlay);
	EXPECT_EQ(agenda[5].kind, Step::Kind::Face);
	EXPECT_EQ(agenda[5].animation, 16u);
	EXPECT_FLOAT_EQ(agenda[5].seconds, 5.0f);
	// Round two: turn, talk, face 25 (no playful clip, no laugh)
	EXPECT_EQ(agenda[8].kind, Step::Kind::Face);
	EXPECT_EQ(agenda[8].animation, 25u);
	EXPECT_EQ(agenda.size(), 6u + 3u + 4u + 6u);
}

TEST(CreaturePlayerAttention, ThePlanActionsAskForTheirRules)
{
	plan_actions::Situation situation;
	const auto& goToHand = *plan_actions::For("GoToHand");
	const auto& lookAtHand = *plan_actions::For("LookAtHand");
	const auto& cross = *plan_actions::For("BeCrossWithPlayer");
	const auto& middle = *plan_actions::For("GoToMiddleOfScreen");
	const auto& follow = *plan_actions::For("FollowPlayer");
	EXPECT_FALSE(plan_actions::Possible(goToHand, situation));
	EXPECT_FALSE(plan_actions::Possible(lookAtHand, situation));
	EXPECT_FALSE(plan_actions::Possible(cross, situation));
	EXPECT_FALSE(plan_actions::Possible(middle, situation));
	EXPECT_TRUE(plan_actions::Possible(follow, situation));
	situation.awayFromHand = true;
	situation.handMoving = true;
	situation.attitudeToPlayer = -0.5f;
	situation.hasPlayer = true;
	situation.lookedAt = true;
	EXPECT_TRUE(plan_actions::Possible(goToHand, situation));
	EXPECT_TRUE(plan_actions::Possible(lookAtHand, situation));
	EXPECT_TRUE(plan_actions::Possible(cross, situation));
	EXPECT_TRUE(plan_actions::Possible(middle, situation));
	EXPECT_FALSE(plan_actions::Possible(follow, situation));
}

// The step follows the hand from the turn after it starts, and walks to it while it is far
TEST(CreaturePlayerAttention, TheIdleMindFollowsTheHandUntilNear)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, GoToHand(NeverFirst));
	creature_mind::Senses senses {.seconds = 0.1f};
	senses.followHand =
	    FollowHandView {.hasPlayer = true, .handInMap = true, .hand = {50.0f, 0.0f}, .distance = 50.0f, .reach = 10.0f};
	auto commands = creature_mind::Think(mind, senses, AlwaysFirst);
	EXPECT_FALSE(commands.move.has_value());
	EXPECT_EQ(mind.followHandTurns, 300u);
	commands = creature_mind::Think(mind, senses, AlwaysFirst);
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->point, glm::vec2(50.0f, 0.0f));
	EXPECT_FLOAT_EQ(commands.move->maxDistance, 10.0f);
	senses.followHand->distance = 5.0f;
	commands = creature_mind::Think(mind, senses, AlwaysFirst);
	EXPECT_EQ(mind.step, 1u);
}
