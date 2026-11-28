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

TEST_F(HandGrabShortTreePress, InASinglePlayerGameAShortPressTakesALightTree)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	EXPECT_EQ(PressEightFrames(tree), std::optional(tree));
	EXPECT_TRUE(_world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabShortTreePress, InANetworkGameTheSameShortPressTakesTheTreeAtTheSameFrame)
{
	_world->networkGame = true;
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	EXPECT_EQ(PressEightFrames(tree), std::optional(tree));
	EXPECT_TRUE(_world->registry.AllOf<InHand>(tree));
}

TEST_F(HandGrabShortTreePress, APressOnATreeOutsideTheInfluenceIsAClickAndDoesNotPull)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	_world->influence = false;
	EXPECT_FALSE(Press());
	for (int i = 0; i < 8; ++i)
	{
		Frame(20, {0.0f, 50.0f, 0.0f});
	}
	EXPECT_FALSE(_system->GetPullPose().has_value());
	EXPECT_FALSE(_world->registry.AllOf<HandPulled>(tree));
	EXPECT_TRUE(_world->holes.empty());
	EXPECT_FALSE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, InANetworkGameTheInterfaceAsksTheInfluenceOfItsOwnHand)
{
	_world->networkGame = true;
	_world->ownHandPlayer = PlayerNames::PLAYER_TWO;
	_world->influenceOnlyOf = PlayerNames::PLAYER_TWO;
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	// What the hand offers and why it would not take a thing are of the hand of this computer, not of the first player
	EXPECT_TRUE(_system->IsInInfluence());
	EXPECT_EQ(_system->WhyNotTake(tree), "");
	EXPECT_FALSE(_system->IsInInfluenceOf(PlayerNames::PLAYER_ONE));
	_world->influenceOnlyOf = PlayerNames::PLAYER_ONE;
	EXPECT_FALSE(_system->IsInInfluence());
	EXPECT_NE(_system->WhyNotTake(tree), "");
}

TEST_F(HandGrabSystemWithWorld, ALightTreeIsUprootedAndCreaks)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->holes, std::vector<entt::entity> {tree});
	EXPECT_EQ(_world->uprooted, std::vector<entt::entity> {tree});
	// The hand makes the land's draw list again only for a living thing that it pulls free
	EXPECT_EQ(_world->drawListsRemade, 0);
	// A tree pulled out of the ground creaks rather than making the pick-up sound
	ASSERT_EQ(_world->samples.size(), 1u);
	EXPECT_EQ(_world->samples.front(), 32u);
}

TEST_F(HandGrabSystemWithWorld, AVillagerPulledFreeMakesTheDrawListAgain)
{
	const auto villager = _world->registry.Create();
	_world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Villager>(villager);
	_world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	_world->underCursor = villager;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->drawListFrames, std::vector<uint32_t> {1u});
}

TEST_F(HandGrabSystemWithWorld, AVillagerInTheHandKeepsItsWoodAndFood)
{
	const auto villager = _world->registry.Create();
	_world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& person = _world->registry.Assign<Villager>(villager);
	person.woodHeld = 250;
	person.foodHeld = 120;
	person.woodGraphic = 2;
	_world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	_world->underCursor = villager;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 30 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10, {0.0f, 50.0f, 0.0f});
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	const auto& held = _world->registry.Get<const Villager>(villager);
	EXPECT_EQ(held.woodHeld, 250);
	EXPECT_EQ(held.foodHeld, 120);
	EXPECT_EQ(held.woodGraphic, 2);
}

TEST_F(HandGrabSystemWithWorld, AVillagerLetGoBeforeItComesFreeMakesTheDrawListAgainForTwoFrames)
{
	const auto villager = _world->registry.Create();
	_world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Villager>(villager);
	_world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	_world->underCursor = villager;
	EXPECT_TRUE(Press());
	Frame(10);
	ASSERT_FALSE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->drawListFrames.empty());
	// The player lets go of the button before the villager comes free
	Release();
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->drawListFrames, std::vector<uint32_t> {2u});
}

TEST_F(HandGrabSystemWithWorld, AQuickClickOnAVillagerMakesTheDrawListAgainForTwoFrames)
{
	const auto villager = _world->registry.Create();
	_world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Villager>(villager);
	_world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	_world->underCursor = villager;
	EXPECT_TRUE(Press());
	Release();
	EXPECT_EQ(_world->drawListFrames, std::vector<uint32_t> {2u});
}

TEST_F(HandGrabSystemWithWorld, ABigForestLetGoBeforeItGivesATreeDoesNotMakeTheDrawListAgain)
{
	const auto forest = _world->AddBigForest({0.0f, 0.0f, 0.0f});
	_world->underCursor = forest;
	EXPECT_TRUE(Press());
	Frame(100);
	Release();
	EXPECT_TRUE(_world->drawListFrames.empty());
}

TEST_F(HandGrabSystemWithWorld, ABigForestGivesATreeOnceTheButtonHasBeenHeldAWhile)
{
	const auto forest = _world->AddBigForest({0.0f, 0.0f, 0.0f});
	_world->underCursor = forest;
	EXPECT_TRUE(Press());
	// It isn't pulled at: the hand waits, as for a thing in flight
	EXPECT_FALSE(_system->GetPullPose().has_value());
	Frame(100);
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->treesGiven.empty());
	Frame(130);
	ASSERT_EQ(_world->treesGiven.size(), 1u);
	const auto tree = _world->treesGiven.front().second;
	EXPECT_EQ(_world->treesGiven.front().first, forest);
	// The hand holds the tree, never the forest, and took it as it takes a standing tree: it creaks and costs alignment,
	// but leaves no hole
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(*_system->GetHeld(), tree);
	EXPECT_TRUE(_world->registry.AllOf<InHand>(tree));
	EXPECT_FALSE(_world->registry.AllOf<InHand>(forest));
	EXPECT_EQ(_world->leftWorld, std::vector<entt::entity> {tree});
	EXPECT_EQ(_world->uprooted, std::vector<entt::entity> {tree});
	EXPECT_EQ(_world->samples, std::vector<uint32_t> {32u});
	EXPECT_TRUE(_world->holes.empty());
	ASSERT_EQ(_world->reactions.size(), 1u);
	EXPECT_EQ(_world->reactions.front().type, Reaction::ReactToHandPickUp);
}

TEST_F(HandGrabSystemWithWorld, AShortPressOnABigForestGivesNoTree)
{
	const auto forest = _world->AddBigForest({0.0f, 0.0f, 0.0f});
	_world->underCursor = forest;
	EXPECT_TRUE(Press());
	Frame(100);
	EXPECT_EQ(Release(), std::optional(forest));
	EXPECT_TRUE(_world->treesGiven.empty());
	EXPECT_EQ(_world->tapped, std::vector<entt::entity> {forest});
}

TEST_F(HandGrabSystemWithWorld, TheSpringTakesHoldTheFrameAfterThePressAndThrows)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	// Held at rest, the hand rises for the rock without lagging
	const auto held = Frame(10, {0.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(held.x, 0.0f);
	// Pressed again it makes ready to throw: this frame the hand is where it should be, then the spring drags it
	Press();
	const auto first = Frame(10, {5.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(first.x, 5.0f);
	const auto dragged = Frame(10, {10.0f, 0.0f, 0.0f});
	EXPECT_GT(dragged.x, 5.0f);
	EXPECT_LT(dragged.x, 10.0f);
	Release();
	ASSERT_EQ(_world->released.size(), 1u);
	EXPECT_EQ(_world->released.front().first, rock);
	EXPECT_GT(_world->released.front().second.x, 0.0f);
	EXPECT_FALSE(_system->GetHeld().has_value());
}

// A hand demonstration gives the hand the throw it recorded with each button: let go before the spring moves the hand
// again, the throw flies at the recorded velocity; once the spring has moved it, the spring's own throw counts again
TEST_F(HandGrabSystemWithWorld, ARecordedThrowStandsUntilTheSpringNextMovesTheHand)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	Frame(10);
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	const glm::vec3 recorded {73.9941f, 5.2013f, -52.1485f};
	_system->SetRecordedThrow(recorded);
	Release();
	ASSERT_EQ(_world->released.size(), 1u);
	EXPECT_EQ(_world->released.front().second, recorded);

	// Taken and made ready again, a recorded throw given before the spring moves is replaced by the spring's
	_world->released.clear();
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	Frame(10);
	Press();
	_system->SetRecordedThrow(recorded);
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(_world->released.size(), 1u);
	EXPECT_NE(_world->released.front().second, recorded);
	EXPECT_GT(_world->released.front().second.x, 0.0f);
}

TEST_F(HandGrabSystemWithWorld, LettingGoOntoTheLandCountsAsATurnForTheInfluenceKeptPastTheBorder)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	// Taking it up isn't using it on the land
	EXPECT_TRUE(_world->usedOnLand.empty());
	_world->underCursor.reset();
	Frame(10);
	// Let go out of the influence the hand keeps hold, and nothing counts
	Press();
	Frame(10);
	_world->influence = false;
	Frame(10);
	Release();
	EXPECT_TRUE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->usedOnLand.empty());
	// Let go inside it counts once, as it is thrown
	_world->influence = true;
	Frame(10);
	Press();
	Frame(10);
	Release();
	EXPECT_EQ(_world->usedOnLand, std::vector<PlayerNames> {PlayerNames::PLAYER_ONE});
	ASSERT_EQ(_world->released.size(), 1u);
}

TEST_F(HandGrabSystemWithWorld, APressOutsideTheInfluenceDoesNotMakeReadyToThrow)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	_world->underCursor.reset();
	_world->influence = false;
	Frame(10);
	Press();
	Frame(10);
	// Back inside before the button is let go: still held, not thrown
	_world->influence = true;
	Frame(10);
	Release();
	EXPECT_TRUE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->released.empty());
	EXPECT_TRUE(_world->usedOnLand.empty());
}

TEST_F(HandGrabSystemWithWorld, WhatIsLetGoGetsItsTwistAFifthOfASecondLater)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	Frame(10);
	Press();
	Frame(10);
	Release();
	_world->bodies.insert(rock);
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	EXPECT_TRUE(_world->twists.empty());
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	ASSERT_EQ(_world->twists.size(), 1u);
	EXPECT_EQ(_world->twists.front().first, rock);
}

TEST_F(HandGrabSystemWithWorld, TheTwistIsACommandThatTheTurnDoes)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	Frame(10);
	Press();
	Frame(10);
	Release();
	_world->bodies.insert(rock);
	// The commands wait for their turn from here
	_world->onCommand = [this](const player_commands::Action& action) { _waiting.push_back(action); };
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	Frame(100, {}, {1.0f, 0.0f, 0.0f});
	EXPECT_TRUE(_world->twists.empty());
	ASSERT_FALSE(_waiting.empty());
	EXPECT_TRUE(std::holds_alternative<player_commands::HandTwist>(_waiting.back()));
	RunWaiting();
	ASSERT_EQ(_world->twists.size(), 1u);
	EXPECT_EQ(_world->twists.front().first, rock);
}

TEST_F(HandGrabSystemWithWorld, ATakeOfAnotherComputerTakesWithNoPressOfThisHand)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	// A server, or a computer that did not press: its hand takes at the turn all the same
	_system->DoTake(_world->HandPlayer(), rock);
	ASSERT_EQ(_system->GetHeld(), rock);
	// And lets go of what it holds, though its own hand never let go of the button
	_system->DoLetGo(_world->HandPlayer(), player_commands::HandLetGo {.object = rock, .velocity = glm::vec3(0.0f)});
	EXPECT_FALSE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, TheTurnHasAHeldThingAtTheReportedHandAtItsOwnTurnAndSize)
{
	const auto rock = _world->AddRock({5.0f, 0.0f, 5.0f});
	_system->DoTake(_world->HandPlayer(), rock);
	ASSERT_EQ(_system->GetHeld(), rock);
	auto& transform = _world->registry.Get<Transform>(rock);
	const auto held = transform;
	// The frames draw it elsewhere, turned
	transform.position = {50.0f, 9.0f, 50.0f};
	transform.rotation = glm::mat3(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	_system->PlaceHeldForTurn();
	EXPECT_EQ(transform.position, _world->ReportedHandPlace());
	EXPECT_EQ(transform.rotation, held.rotation);
	EXPECT_EQ(transform.scale, held.scale);
}

TEST_F(HandGrabSystemWithWorld, TheTurnHasAHeldThingAtTheMapPositionOfTheReportedHand)
{
	const auto rock = _world->AddRock({5.0f, 0.0f, 5.0f});
	_system->DoTake(_world->HandPlayer(), rock);
	ASSERT_EQ(_system->GetHeld(), rock);
	_world->heldPlaceStep = {0.25f, -0.5f, 0.125f};
	_system->PlaceHeldForTurn();
	EXPECT_EQ(_world->registry.Get<const Transform>(rock).position, _world->ReportedHandPlace() + _world->heldPlaceStep);
}

TEST_F(HandGrabSystemWithWorld, TheTurnHasAThingInTheHandOfAComputerMindWhereTheMindPutTheHandAfterItsLastStep)
{
	const auto rock = _world->AddRock({5.0f, 0.0f, 5.0f});
	_system->DoTake(_world->HandPlayer(), rock);
	ASSERT_EQ(_system->GetHeld(), rock);
	// The last report gives the place before the step. The input of the turn moves the hand to the place after it.
	_world->reportedPlaces[_world->hand] = glm::vec3(10.0f, 4.0f, 10.0f);
	_world->mindTurnPlaces[_world->hand] = glm::vec3(30.0f, 6.0f, 20.0f);
	_world->heldPlaceStep = {0.25f, -0.5f, 0.125f};
	_system->PlaceHeldForTurn();
	EXPECT_EQ(_world->registry.Get<const Transform>(rock).position, glm::vec3(30.0f, 6.0f, 20.0f) + _world->heldPlaceStep);
}

TEST_F(HandGrabSystemWithWorld, TheFramesDrawAThingInTheHandOfAComputerMindWithNoMovementInATurnWithAStep)
{
	auto& registry = _world->registry;
	const auto rock = _world->AddRock({5.0f, 0.0f, 5.0f});
	registry.Assign<InHand>(rock, InHand {.hand = _world->secondHand});
	_world->otherHands.insert(_world->secondHand);
	_world->reportedPlaces[_world->secondHand] = glm::vec3(40.0f, 5.0f, 60.0f);
	// In a turn with a step, the world has the hand before the step, and the movement starts there too
	_world->mindDrawnPlaces[_world->secondHand] = glm::vec3(20.0f, 5.0f, 30.0f);
	registry.Assign<HeldDrawTrack>(_world->secondHand,
	                               HeldDrawTrack {.from = glm::vec3(20.0f, 5.0f, 30.0f), .elapsedMs = 0, .intervalMs = 100});
	for (int frame = 0; frame < 5; ++frame)
	{
		_system->PlaceOthersHeldForFrame(20);
		const auto& drawn = registry.Get<const InHand>(rock).drawn;
		ASSERT_TRUE(drawn.has_value());
		EXPECT_EQ(drawn->position, glm::vec3(20.0f, 5.0f, 30.0f));
	}
	// The seed in the hand of the mind is drawn at the same place
	const auto seed = registry.Create();
	registry.Assign<Transform>(seed, glm::vec3(1.0f, 2.0f, 3.0f), glm::mat3(2.0f), glm::vec3(1.0f));
	registry.Assign<HandHeldSeed>(_world->secondHand, HandHeldSeed {.seed = seed});
	_system->PlaceOthersHeldForFrame(20);
	EXPECT_EQ(registry.Get<const Transform>(seed).position, glm::vec3(20.0f, 5.0f, 30.0f));
}

TEST_F(HandGrabSystemWithWorld, TheFramesMoveAThingInTheHandOfAComputerMindToThePlaceAfterTheStepInTheNextTurns)
{
	auto& registry = _world->registry;
	const auto rock = _world->AddRock({5.0f, 0.0f, 5.0f});
	registry.Assign<InHand>(rock, InHand {.hand = _world->secondHand});
	_world->otherHands.insert(_world->secondHand);
	// A turn with no step has the hand after the last step. The frames of the turn of the step had 60 milliseconds, and
	// the frames between the two last steps had 100.
	_world->mindDrawnPlaces[_world->secondHand] = glm::vec3(40.0f, 5.0f, 30.0f);
	registry.Assign<HeldDrawTrack>(_world->secondHand,
	                               HeldDrawTrack {.from = glm::vec3(20.0f, 5.0f, 30.0f), .elapsedMs = 60, .intervalMs = 100});
	_system->PlaceOthersHeldForFrame(20);
	EXPECT_EQ(registry.Get<const InHand>(rock).drawn->position, glm::vec3(36.0f, 5.0f, 30.0f));
	_system->PlaceOthersHeldForFrame(20);
	EXPECT_EQ(registry.Get<const InHand>(rock).drawn->position, glm::vec3(40.0f, 5.0f, 30.0f));
	_system->PlaceOthersHeldForFrame(20);
	EXPECT_EQ(registry.Get<const InHand>(rock).drawn->position, glm::vec3(40.0f, 5.0f, 30.0f));
}

TEST_F(HandGrabSystemWithWorld, TheFramesDrawAThingInTheHandOfAComputerMindAtItsPlaceBeforeTheFirstReport)
{
	auto& registry = _world->registry;
	const auto rock = _world->AddRock({5.0f, 0.0f, 5.0f});
	registry.Assign<InHand>(rock, InHand {.hand = _world->secondHand});
	_world->otherHands.insert(_world->secondHand);
	_world->mindDrawnPlaces[_world->secondHand] = glm::vec3(20.0f, 5.0f, 30.0f);
	_system->PlaceOthersHeldForFrame(20);
	EXPECT_EQ(registry.Get<const InHand>(rock).drawn->position, glm::vec3(20.0f, 5.0f, 30.0f));
}

TEST_F(HandGrabSystemWithWorld, AHandThatHoldsTwoThingsMovesOnceInAFrame)
{
	auto& registry = _world->registry;
	const auto first = _world->AddRock({5.0f, 0.0f, 5.0f});
	const auto second = _world->AddRock({6.0f, 0.0f, 5.0f});
	registry.Assign<InHand>(first, InHand {.hand = _world->secondHand});
	registry.Assign<InHand>(second, InHand {.hand = _world->secondHand});
	_world->otherHands.insert(_world->secondHand);
	_world->reportedPlaces[_world->secondHand] = glm::vec3(10.0f, 0.0f, 0.0f);
	registry.Assign<HeldDrawTrack>(_world->secondHand,
	                               HeldDrawTrack {.from = glm::vec3(0.0f), .elapsedMs = 0, .intervalMs = 100});
	_system->PlaceOthersHeldForFrame(20);
	EXPECT_EQ(registry.Get<const InHand>(first).drawn->position, glm::vec3(2.0f, 0.0f, 0.0f));
	EXPECT_EQ(registry.Get<const InHand>(second).drawn->position, glm::vec3(2.0f, 0.0f, 0.0f));
	EXPECT_EQ(registry.Get<const HeldDrawTrack>(_world->secondHand).elapsedMs, 20);
}
