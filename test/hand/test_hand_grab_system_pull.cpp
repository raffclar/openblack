/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "HandGrabSystemFixture.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace hand_grab_test;

TEST_F(HandGrabSystemWithWorld, APressOnAThingAScriptFixedInPlaceIsAPressOnTheLand)
{
	const auto stone = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->registry.Assign<CannotBePickedUp>(stone);
	_world->underCursor = stone;
	_commandTurn = _now / 100;
	EXPECT_FALSE(Press());
	// The press does not click or tap the stone, in the influence or out of it
	EXPECT_FALSE(_world->registry.AllOf<HandClicked>(_world->hand) &&
	             _world->registry.Get<HandClicked>(_world->hand).thing != entt::null);
	_world->influence = false;
	EXPECT_FALSE(Press());
	EXPECT_FALSE(_world->registry.AllOf<HandClicked>(_world->hand) &&
	             _world->registry.Get<HandClicked>(_world->hand).thing != entt::null);
	EXPECT_TRUE(_world->tapped.empty());
	// Only the release clicks it, so one click of the button clicks it once
	_system->ClickReleased();
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, stone);
	// A thing that takes taps, or any thing with the leash loose in the hand, is clicked by the press
	_world->registry.Get<HandClicked>(_world->hand).thing = entt::null;
	_world->tappable.insert(stone);
	EXPECT_FALSE(Press());
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, stone);
	_world->tappable.erase(stone);
	_world->registry.Get<HandClicked>(_world->hand).thing = entt::null;
	_world->looseLeash = true;
	EXPECT_FALSE(Press());
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, stone);
}

// A tap on a stone that a script fixed in place is only a click, which the script reads. The creature gets no deed,
// no reaction and no help event from it, so it can not learn from the taps of the singing stones.
TEST_F(HandGrabSystemWithWorld, ATapOnAFixedStoneGivesTheCreatureNothingToSee)
{
	const auto stone = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->registry.Assign<CannotBePickedUp>(stone);
	_world->underCursor = stone;
	_commandTurn = _now / 100;
	EXPECT_FALSE(Press());
	Frame(50);
	EXPECT_FALSE(Release().has_value());
	_system->ClickReleased();
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, stone);
	EXPECT_TRUE(_world->tapped.empty());
	EXPECT_TRUE(_world->reactions.empty());
	EXPECT_TRUE(_world->potReactionsSetUp.empty());
	EXPECT_TRUE(_world->helpEvents.empty());
}

TEST(HandGrabRules, APressTapsAThingTheHandCouldHoldOrThatTakesTaps)
{
	const hand_grab::Gate holdable {.valid = true};
	const hand_grab::Gate fixed {.valid = true, .cannotBePickedUp = true};
	const hand_grab::Gate notHoldable {.valid = false};
	EXPECT_TRUE(hand_grab::PressTapsThing(holdable, false, false));
	EXPECT_FALSE(hand_grab::PressTapsThing(fixed, false, false));
	EXPECT_FALSE(hand_grab::PressTapsThing(notHoldable, false, false));
	EXPECT_TRUE(hand_grab::PressTapsThing(fixed, true, false));
	EXPECT_TRUE(hand_grab::PressTapsThing(notHoldable, false, true));
}

TEST_F(HandGrabSystemWithWorld, ATapClicksWhatIsUnderTheHandAsItIsLetGo)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	// Taking hold isn't a click
	EXPECT_FALSE(_world->registry.AllOf<HandClicked>(_world->hand) &&
	             _world->registry.Get<HandClicked>(_world->hand).thing != entt::null);
	Frame(50);
	const auto other = _world->AddRock({5.0f, 0.0f, 0.0f});
	_world->underCursor = other;
	Release();
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, other);
}

TEST_F(HandGrabSystemWithWorld, AHeldPressIsNoClick)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	Frame(300);
	Release();
	const auto* clicked = _world->registry.TryGet<const HandClicked>(_world->hand);
	EXPECT_TRUE(clicked == nullptr || clicked->thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, LettingGoOverTheLandClicksThePlace)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	_commandTurn = 7;
	_system->ClickReleased();
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, rock);
	_world->underCursor.reset();
	Frame(10, {}, {20.0f, 0.0f, 30.0f});
	_commandTurn = 8;
	_system->ClickReleased();
	const auto& clicked = _world->registry.Get<HandClicked>(_world->hand);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.place.x, map_coords::ToFixed(20.0f));
	EXPECT_EQ(clicked.place.z, map_coords::ToFixed(30.0f));
	EXPECT_EQ(clicked.placeTurn, 8u);
}

TEST_F(HandGrabSystemWithWorld, TheTurnWritesTheClicks)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	_commandTurn = 12;
	_system->ClickThing(rock);
	ASSERT_EQ(_world->sent.size(), 1u);
	EXPECT_EQ(std::get<player_commands::HandClick>(_world->sent[0]).thing, rock);
	// The click has the number of the turn that wrote it, not of the frame that sent it
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, rock);
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thingTurn, 12u);
	// A let go over the land sends the place
	_world->underCursor.reset();
	Frame(10, {}, {20.0f, 0.0f, 30.0f});
	_commandTurn = 13;
	_system->ClickReleased();
	const auto& clicked = _world->registry.Get<HandClicked>(_world->hand);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.place.x, map_coords::ToFixed(20.0f));
	EXPECT_EQ(clicked.placeTurn, 13u);
	// A thing that is gone by the turn is not clicked
	const auto gone = _world->AddRock({5.0f, 0.0f, 0.0f});
	_world->underCursor = gone;
	_waiting.clear();
	_world->onCommand = [this](const player_commands::Action& action) { _waiting.push_back(action); };
	_system->ClickReleased();
	_world->registry.Destroy(gone);
	RunWaiting();
	EXPECT_TRUE(_world->registry.Get<HandClicked>(_world->hand).thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, ARewardWithTheLeashLooseInTheHandIsTheLand)
{
	const auto chest = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->registry.Assign<Reward>(chest);
	_world->underCursor = chest;
	_world->looseLeash = true;
	Frame(10, {}, {20.0f, 0.0f, 30.0f});
	_commandTurn = 8;
	_system->ClickReleased();
	const auto& clicked = _world->registry.Get<HandClicked>(_world->hand);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.placeTurn, 8u);
	_world->looseLeash = false;
	_system->ClickReleased();
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, chest);
}

TEST_F(HandGrabSystemWithWorld, ClicksAreForgottenAfterFifteenSeconds)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	_commandTurn = _now / 100;
	_system->ClickReleased();
	// 149 turns of 100 ms is under 15 seconds; 150 is just over, as a thousandth in single precision is a little more.
	// The turn forgets the clicks, not the frames.
	Frame(14900);
	_system->InterfaceTurn(_now, _now / 100);
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, rock);
	Frame(100);
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).thing, rock);
	_system->InterfaceTurn(_now, _now / 100);
	EXPECT_TRUE(_world->registry.Get<HandClicked>(_world->hand).thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, InANetworkGameEveryComputerForgetsTheClicksOfAllHands)
{
	// This computer moves the hands of the second player. The first player clicked a place, and the second a thing.
	_world->networkGame = true;
	_world->secondPlayerHasHands = true;
	_world->ownHandPlayer = PlayerNames::PLAYER_TWO;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_system->DoClick(PlayerNames::PLAYER_ONE,
	                 player_commands::HandClick {.thing = entt::null,
	                                             .release = true,
	                                             .hasPlace = true,
	                                             .placeX = map_coords::ToFixed(20.0f),
	                                             .placeZ = map_coords::ToFixed(30.0f)},
	                 10);
	_system->DoClick(PlayerNames::PLAYER_TWO, player_commands::HandClick {.thing = rock, .release = false}, 10);
	ASSERT_EQ(_world->registry.Get<HandClicked>(_world->hand).place.x, map_coords::ToFixed(20.0f));
	ASSERT_EQ(_world->registry.Get<HandClicked>(_world->secondHand).thing, rock);
	// Before 15 seconds, the turn keeps both clicks
	_system->InterfaceTurn(15900, 159);
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).place.x, map_coords::ToFixed(20.0f));
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->secondHand).thing, rock);
	// After 15 seconds, the turn forgets the click of the first player too, not only the click of this computer's hand
	_system->InterfaceTurn(16000, 160);
	EXPECT_EQ(_world->registry.Get<HandClicked>(_world->hand).place.x, 0);
	EXPECT_TRUE(_world->registry.Get<HandClicked>(_world->secondHand).thing == entt::null);
}

TEST_F(HandGrabSystemWithWorld, AThingInFlightIsCaughtOnlyAfterTheWait)
{
	const auto rock = _world->AddRock({0.0f, 5.0f, 0.0f});
	_world->underCursor = rock;
	_world->flying.insert(rock);
	EXPECT_TRUE(Press());
	Frame(100);
	EXPECT_FALSE(_system->GetHeld().has_value());
	Frame(130);
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(*_system->GetHeld(), rock);
}

// The hand takes a thing from the press until the turn puts the thing in the hand, also while it waits for a thing in
// flight and does not pull it. Both buttons and the loss of the thing end the interface lock in all of this time.
TEST_F(HandGrabSystemWithWorld, TheHandTakesAThingFromThePressUntilTheTurnPutsItInTheHand)
{
	const auto rock = _world->AddRock({0.0f, 5.0f, 0.0f});
	_world->underCursor = rock;
	_world->flying.insert(rock);
	EXPECT_FALSE(_system->GetTaking().has_value());
	_deferCommands = true;
	EXPECT_TRUE(Press());
	EXPECT_EQ(_system->GetTaking(), std::optional(rock));
	EXPECT_FALSE(_system->IsPulling());
	// After the wait the hand sends the take, and it takes the thing until the turn does the take
	Frame(230);
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_EQ(_system->GetTaking(), std::optional(rock));
	EXPECT_EQ(_world->lockEnds, 0);
	RunWaiting();
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	EXPECT_FALSE(_system->GetTaking().has_value());
	// The thing of the press is in the hand now, which ends the interface lock
	EXPECT_EQ(_world->lockEnds, 1);
}

TEST_F(HandGrabSystemWithWorld, ATapTakesNothing)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	EXPECT_EQ(_system->GetTaking(), std::optional(rock));
	Release();
	EXPECT_FALSE(_system->GetTaking().has_value());
}

TEST_F(HandGrabSystemWithWorld, AFlyingThingThatLandsDuringTheWaitIsTakenAtOnce)
{
	const auto rock = _world->AddRock({0.0f, 5.0f, 0.0f});
	_world->underCursor = rock;
	_world->flying.insert(rock);
	EXPECT_TRUE(Press());
	Frame(50);
	_world->flying.erase(rock);
	Frame(10);
	EXPECT_TRUE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AHeavyTreeIsNeverPulledFree)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	_world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 100; ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->holes.empty());
	// While it pulls the hand takes the pose a tree is held with
	const auto pose = _system->GetPullPose();
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(pose->hold, HoldType::Tree);
	// Let go before it came free, it stays where it is
	EXPECT_FALSE(Release().has_value());
	EXPECT_FALSE(_system->GetPullPose().has_value());
	EXPECT_FALSE(_world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabSystemWithWorld, ATreeLetGoBeforeItComesFreeIsDrawnLeaningAndKeepsItsPlaceInTheWorld)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	const auto before = _world->registry.Get<const Transform>(tree);
	_world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 100; ++i)
	{
		Frame(10, {20.0f, 30.0f, 0.0f});
	}
	// The frames draw the lean and the stretch; the world keeps the tree as it stood
	const auto* pulled = _world->registry.TryGet<const HandPulled>(tree);
	ASSERT_NE(pulled, nullptr);
	EXPECT_TRUE(pulled->rotation != before.rotation || pulled->scale != before.scale);
	EXPECT_FALSE(Release().has_value());
	const auto& after = _world->registry.Get<const Transform>(tree);
	EXPECT_EQ(after.position, before.position);
	EXPECT_EQ(after.rotation, before.rotation);
	EXPECT_EQ(after.scale, before.scale);
}

TEST_F(HandGrabSystemWithWorld, InANetworkGameAPulledThingKeepsThePlaceThatTheTurnsGaveIt)
{
	_world->networkGame = true;
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	_world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 100; ++i)
	{
		Frame(10, {20.0f, 30.0f, 0.0f});
	}
	// The turns of every computer move the thing while this hand pulls at it, as they move a villager that walks
	auto& transform = _world->registry.Get<Transform>(tree);
	transform.position = {4.0f, 0.5f, -3.0f};
	const auto walked = transform;
	_system->PlaceHeldForTurn();
	EXPECT_EQ(transform.position, walked.position);
	EXPECT_EQ(transform.rotation, walked.rotation);
	EXPECT_EQ(transform.scale, walked.scale);
	// The let go does not put it back where the pull started
	EXPECT_FALSE(Release().has_value());
	_system->PlaceHeldForTurn();
	EXPECT_EQ(transform.position, walked.position);
	EXPECT_EQ(transform.rotation, walked.rotation);
	EXPECT_EQ(transform.scale, walked.scale);
	EXPECT_FALSE(_world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabSystemWithWorld, APullStartsFromTheWorldAxesThatTheWorldMakesAgainForTheStart)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	// The start axes differ from the turn of the thing, as axes that the game makes again from the angle can
	const glm::mat3 start(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
	_world->pullStartPoses[tree] = {.axes = start, .origin = {0.0f, 0.0f, 0.0f}};
	_world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30; ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	// The pull asks for the start one time only
	EXPECT_EQ(_world->pullStarts, std::vector<entt::entity> {tree});
	// The drawn tree has the size of the start axes, not the size 1 of its turn
	const auto* pulled = _world->registry.TryGet<const HandPulled>(tree);
	ASSERT_NE(pulled, nullptr);
	EXPECT_NEAR(pulled->scale.x, 2.0f, 1.0e-3f);
}

TEST_F(HandGrabSystemWithWorld, TheRootsHoleHasThePlaceOfTheTreeBeforeThePullNotItsLean)
{
	const auto tree = _world->AddTree({3.0f, 1.0f, -2.0f}, 1.0e6f);
	auto& transform = _world->registry.Get<Transform>(tree);
	transform.rotation = glm::mat3(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	transform.scale = glm::vec3(1.5f);
	const auto before = transform;
	_world->underCursor = tree;
	EXPECT_TRUE(Press());
	// Too heavy to come free, the tree leans towards the hand
	for (int i = 0; i < 50; ++i)
	{
		Frame(10, {20.0f, 30.0f, 0.0f});
	}
	const auto* leaned = _world->registry.TryGet<const HandPulled>(tree);
	ASSERT_NE(leaned, nullptr);
	ASSERT_TRUE(leaned->rotation != before.rotation || leaned->scale != before.scale);
	ASSERT_TRUE(_world->holes.empty());
	// Light again, it comes free and leaves its hole where it stood before the pull
	_world->weights[tree] = 10.0f;
	for (int i = 0; i < 30 && _world->holes.empty(); ++i)
	{
		Frame(10, {20.0f, 50.0f, 0.0f});
	}
	ASSERT_EQ(_world->holes, std::vector<entt::entity> {tree});
	ASSERT_EQ(_world->holePlaces.size(), 1u);
	EXPECT_EQ(_world->holePlaces.front().place, before.position);
	EXPECT_EQ(_world->holePlaces.front().rotation, before.rotation);
	EXPECT_EQ(_world->holePlaces.front().scale, 1.5f);
}

TEST_F(HandGrabSystemWithWorld, ATreeThatThePullFreedIsTakenWhenTheButtonIsLetGoBeforeTheTake)
{
	_deferCommands = true;
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && _world->holes.empty(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_EQ(_world->holes, std::vector<entt::entity> {tree});
	ASSERT_TRUE(_world->sent.empty());
	// The take comes before the release, so a short press is no tap
	EXPECT_FALSE(Release().has_value());
	ASSERT_EQ(_world->sent.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandTake>(_world->sent.front()));
	EXPECT_EQ(std::get<player_commands::HandTake>(_world->sent.front()).object, tree);
	RunWaiting();
	EXPECT_EQ(_system->GetHeld(), std::optional(tree));
}
