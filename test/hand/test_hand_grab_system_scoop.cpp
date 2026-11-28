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

TEST_F(HandGrabSystemWithWorld, AVillagerOfTheHandsPlayerAlarmsThoseAboutIt)
{
	const auto villager = _world->registry.Create();
	_world->registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Villager>(villager);
	_world->sizes[villager] = {.radius = 0.5f, .height = 2.0f};
	_world->underCursor = villager;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->villagersInHand, std::vector<entt::entity> {villager});
	const bool alarmed = std::ranges::any_of(_world->reactions, [villager](const auto& reaction) {
		return reaction.type == Reaction::ReactToVillagerInHand && reaction.initiator == villager;
	});
	EXPECT_TRUE(alarmed);
}

TEST_F(HandGrabSystemWithWorld, WhatIsHeldIsHeatedEachTurnAndDroppedOnceGone)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	// A rock taken stops being its town's artefact
	EXPECT_EQ(_world->artefactsTaken, std::vector<entt::entity> {rock});
	Turn();
	EXPECT_EQ(_world->heated, std::vector<entt::entity> {rock});
	_world->registry.Destroy(rock);
	Turn();
	EXPECT_FALSE(_system->IsBusy());
}

TEST_F(HandGrabSystemWithWorld, WhatIsHeldOutsideTheInfluenceIsNotHeated)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	_world->influence = false;
	Turn();
	EXPECT_TRUE(_world->heated.empty());
}

TEST_F(HandGrabSystemWithWorld, AHandfulScoopedFromAPoisonedPileIsPoisoned)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile, .poisoned = true});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_TRUE(_world->registry.Get<const Pot>(*handful).poisoned);
}

TEST_F(HandGrabSystemWithWorld, AHandfulThatTakesAllOfAPoisonedPileIsClean)
{
	// The last of a pile loses its poison as it is taken, so the food in the hand is clean
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 20, .maxAmount = 2000, .type = PotInfo::FoodPile, .poisoned = true});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 20u);
	EXPECT_FALSE(_world->registry.Get<const Pot>(*handful).poisoned);
}

TEST_F(HandGrabSystemWithWorld, APileThatIsPoisonedWhileItIsScoopedPoisonsTheHandful)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_FALSE(_world->registry.Get<const Pot>(*handful).poisoned);
	// Each turn's scoop brings the poison of the pile into the handful, and the handful stays poisoned
	_world->registry.Get<Pot>(pile).poisoned = true;
	Turn();
	EXPECT_TRUE(_world->registry.Get<const Pot>(*handful).poisoned);
	_world->registry.Get<Pot>(pile).poisoned = false;
	Turn();
	EXPECT_TRUE(_world->registry.Get<const Pot>(*handful).poisoned);
}

TEST_F(HandGrabSystemWithWorld, APileIsScoopedIntoAHandfulThatGrowsWhileTheButtonIsHeld)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	// The first scoop is taken at once
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_EQ(_world->registry.Get<const Pot>(pile).amount, 975u);
	EXPECT_EQ(_world->streams, 1u);
	// The cursor is pinned while it scoops
	EXPECT_TRUE(_world->cursorPinned);
	// Each game turn takes more, from 8 a turn
	Turn();
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
	Turn();
	EXPECT_GT(_world->registry.Get<const Pot>(*handful).amount, 33u);
	EXPECT_EQ(_world->scoopSounds.size(), 2u);
	// The scooping sound is asked for every turn but its loop starts only once
	EXPECT_EQ(_world->scoopSoundStarts, 1u);
	EXPECT_EQ(_world->scoopSoundStops, 0u);
	// The hand stays above the pile. Above the height of the pile, it rises at least 1.9 m.
	const auto hovering = Frame(10, {30.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(hovering.x, 0.0f);
	EXPECT_FLOAT_EQ(hovering.y, 3.0f + 1.9f);
	// A larger handful hangs further below the hand, and the hand rises by that distance
	_world->sizes[*handful] = {.radius = 1.0f, .height = 4.0f};
	Frame(20, {30.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(Frame(30, {30.0f, 0.0f, 0.0f}).y, 3.0f + 0.7f * 4.0f);
	// Let go, the scoop ends, its stream stops and the cursor is free
	Release();
	EXPECT_EQ(_world->stopped, std::vector<uint32_t> {1u});
	EXPECT_FALSE(_world->cursorPinned);
	// Its sound stops with it, and isn't asked for again
	EXPECT_EQ(_world->scoopSoundStops, 1u);
	EXPECT_FALSE(_world->scoopSoundPlaying);
	Turn();
	EXPECT_EQ(_world->scoopSounds.size(), 2u);
	EXPECT_TRUE(_system->GetHeld().has_value());
}

// The new handful of a scoop leaves the map's cells as the hand takes it, as a thing taken from the world does. The
// pile stays in its cells.
TEST_F(HandGrabSystemWithWorld, AScoopedHandfulLeavesTheMapCells)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_EQ(_world->leftWorld, std::vector<entt::entity> {*handful});
}

// The scoop of another player's hand takes from the pile on every computer, but only the computer of that hand pins
// its cursor, shows the stream, plays the scooping sound and shows the amount that the hand holds
TEST_F(HandGrabSystemWithWorld, TheScoopOfAHandThatIsNotTheLocalOneShowsAndSoundsNothing)
{
	_world->localHand = false;
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_system->DoLock(PlayerNames::PLAYER_ONE, pile);
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_EQ(_world->streams, 0u);
	EXPECT_FALSE(_world->cursorPinned);
	// Each turn still takes more from the pile
	Turn();
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
	EXPECT_EQ(_world->registry.Get<const Pot>(pile).amount, 1000u - 25u - 8u);
	EXPECT_TRUE(_world->scoopSounds.empty());
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	// A pinned cursor of the local hand stays pinned when the other hand's scoop ends
	_world->cursorPinned = true;
	_system->DoUnlock(PlayerNames::PLAYER_ONE, pile);
	EXPECT_TRUE(_world->cursorPinned);
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	EXPECT_EQ(_world->scoopSoundStops, 0u);
	EXPECT_TRUE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AHandfulPressedOntoAStoreGoesIntoIt)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 100, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	Press();
	Release();
	const auto handful = *_system->GetHeld();
	const auto store = _world->registry.Create();
	_world->stores.insert(store);
	_world->underCursor = store;
	EXPECT_TRUE(Press());
	EXPECT_FALSE(_system->IsBusy());
	EXPECT_EQ(_world->stored[store], 25u);
	EXPECT_EQ(_world->usedUp, std::vector<entt::entity> {handful});
	EXPECT_TRUE(_world->released.empty());
}

// The use of the held thing on a thing makes the interface wait for the let go of the button, also when the hand waits
// for its last send. The let go and the end of the interface lock end the wait.
TEST_F(HandGrabSystemWithWorld, TheUseOfTheHeldThingWaitsForTheLetGoOfTheButton)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	EXPECT_FALSE(_system->WaitsForUseLetGo());
	const auto store = _world->registry.Create();
	_world->stores.insert(store);
	_world->underCursor = store;
	_deferCommands = true;
	ASSERT_TRUE(Press());
	EXPECT_TRUE(_system->WaitsForUseLetGo());
	EXPECT_FALSE(_system->GetGiveWait().has_value());
	Release();
	EXPECT_FALSE(_system->WaitsForUseLetGo());
	_world->sendWaits = true;
	ASSERT_TRUE(Press());
	EXPECT_TRUE(_system->WaitsForUseLetGo());
	_system->StopActionAtLockEnd();
	EXPECT_FALSE(_system->WaitsForUseLetGo());
}

TEST_F(HandGrabSystemWithWorld, ATreePressedOntoAStoreGoesIntoItWhole)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	const auto store = _world->registry.Create();
	_world->stores.insert(store);
	_world->underCursor = store;
	EXPECT_TRUE(Press());
	ASSERT_EQ(_world->takenWhole.size(), 1u);
	EXPECT_EQ(_world->takenWhole.front().first, store);
	EXPECT_EQ(_world->takenWhole.front().second, tree);
	EXPECT_FALSE(_system->GetHeld().has_value());
}

// The help counts the use of a held thing on a store for the hand of this computer
TEST_F(HandGrabSystemWithWorld, TheHelpCountsTheUseOfAHeldThingByTheLocalHand)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	const auto store = _world->registry.Create();
	_world->stores.insert(store);
	_world->underCursor = store;
	EXPECT_TRUE(Press());
	ASSERT_EQ(_world->takenWhole.size(), 1u);
	EXPECT_EQ(_world->usedHelpEvents, std::vector<uint32_t> {altar_sacrifice::k_UsedOnThingHelpEvent});
}

// The hand of another player uses its held thing on a store on every computer, but the help of this computer does not
// count it
TEST_F(HandGrabSystemWithWorld, TheHelpCountsNoUseOfAHeldThingByAHandThatIsNotTheLocalOne)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	_world->localHand = false;
	const auto store = _world->registry.Create();
	_world->stores.insert(store);
	_world->underCursor = store;
	EXPECT_TRUE(Press());
	ASSERT_EQ(_world->takenWhole.size(), 1u);
	EXPECT_TRUE(_world->usedHelpEvents.empty());
}

TEST_F(HandGrabSystemWithWorld, AGateStonePressedOntoThePlinthIsLaidInIt)
{
	const auto stone = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->gateStones.insert(stone);
	_world->underCursor = stone;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	const auto plinth = _world->registry.Create();
	_world->plinths.insert(plinth);
	_world->underCursor = plinth;
	EXPECT_TRUE(Press());
	ASSERT_EQ(_world->laidStones.size(), 1u);
	EXPECT_EQ(_world->laidStones.front().first, plinth);
	EXPECT_EQ(_world->laidStones.front().second, stone);
	// The stone is used up, leaving its ghost, and the hand is empty
	EXPECT_EQ(_world->usedUp, std::vector<entt::entity> {stone});
	EXPECT_TRUE(_world->takenWhole.empty());
	EXPECT_FALSE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AVillagerPressedOntoATeleportStoneLeavesTheHandOnTheStone)
{
	const auto villager = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = villager;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	const auto stone = _world->registry.Create();
	_world->teleportStones.insert(stone);
	_world->underCursor = stone;
	_world->spreadAgain.clear();
	EXPECT_TRUE(Press());
	// The villager is out of the hand before the stone takes it, and its reactions spread again
	ASSERT_EQ(_world->droppedOnStones.size(), 1u);
	EXPECT_EQ(std::get<0>(_world->droppedOnStones.front()), stone);
	EXPECT_EQ(std::get<1>(_world->droppedOnStones.front()), villager);
	EXPECT_FALSE(std::get<2>(_world->droppedOnStones.front()));
	EXPECT_EQ(_world->spreadAgain, std::vector<entt::entity> {villager});
	EXPECT_TRUE(_world->usedUp.empty());
	EXPECT_FALSE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AScaffoldPressedOntoAScaffoldJoinsIt)
{
	const auto held = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->scaffolds.insert(held);
	_world->underCursor = held;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	const auto target = _world->AddTree({20.0f, 0.0f, 0.0f}, 10.0f);
	_world->scaffolds.insert(target);
	_world->underCursor = target;
	EXPECT_TRUE(Press());
	// The hand lets go of the held scaffold before the join, and is empty after it
	ASSERT_EQ(_world->joined.size(), 1u);
	EXPECT_EQ(std::get<0>(_world->joined.front()), target);
	EXPECT_EQ(std::get<1>(_world->joined.front()), held);
	EXPECT_FALSE(std::get<2>(_world->joined.front()));
	EXPECT_TRUE(_world->takenWhole.empty());
	EXPECT_FALSE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AnythingElsePressedOntoThePlinthIsMadeReadyToThrow)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 10.0f);
	_world->underCursor = tree;
	Press();
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	const auto plinth = _world->registry.Create();
	_world->plinths.insert(plinth);
	_world->underCursor = plinth;
	EXPECT_TRUE(Press());
	EXPECT_TRUE(_world->laidStones.empty());
	EXPECT_EQ(_system->GetHeld(), std::optional<entt::entity> {tree});
}

TEST_F(HandGrabSystemWithWorld, APotLetGoOverTheLandCallsThePeopleAgain)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 100, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	Press();
	Release();
	const auto handful = *_system->GetHeld();
	_world->underCursor.reset();
	// Made ready and let go slowly, it is poured, having called the people to it first
	Press();
	Frame(10);
	Release();
	EXPECT_EQ(_world->potReactionsSetUp, std::vector<entt::entity> {handful});
}

TEST_F(HandGrabSystemWithWorld, AThingLetGoBeforeItIsTakenIsStillTheLastLetGo)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	// The first frame of the pull never pulls, so the rock is still being taken
	Frame(200);
	EXPECT_FALSE(_system->GetHeld().has_value());
	Release();
	EXPECT_EQ(_world->registry.Get<const HandGrab>(_world->hand).released, rock);
}

TEST_F(HandGrabSystemWithWorld, AScoopThatGetsNothingMoreStopsItsSoundButKeepsTheHandful)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 30, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// The pile gives its last 5 the next turn, then has nothing more
	Turn();
	EXPECT_EQ(_world->scoopSoundStarts, 1u);
	Turn();
	// Still holding the button, the sound has stopped and the hand keeps what it took
	EXPECT_FALSE(_world->scoopSoundPlaying);
	EXPECT_GE(_world->scoopSoundStops, 1u);
	const auto stops = _world->scoopSoundStops;
	const auto asked = _world->scoopSounds.size();
	Turn();
	Turn();
	EXPECT_EQ(_world->scoopSounds.size(), asked);
	EXPECT_EQ(_world->scoopSoundStops, stops);
	EXPECT_EQ(_system->GetHeld(), handful);
}

TEST_F(HandGrabSystemWithWorld, AFieldAndAFishFarmSoundAsFoodWhileTheyAreScooped)
{
	const auto field = _world->registry.Create();
	_world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fields[field] = {.food = 1000, .ripe = false};
	_world->underCursor = field;
	EXPECT_TRUE(Press());
	Turn();
	Turn();
	EXPECT_EQ(_world->scoopSoundStarts, 1u);
	EXPECT_EQ(_world->scoopSoundResources, (std::vector<ResourceType> {ResourceType::Food, ResourceType::Food}));
	Release();
	EXPECT_FALSE(_world->scoopSoundPlaying);
}

TEST_F(HandGrabSystemWithWorld, ARipeFieldGivesHalfOfEachScoop)
{
	const auto field = _world->registry.Create();
	_world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fields[field] = {.food = 1000, .ripe = true};
	_world->underCursor = field;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// The first scoop of 25 is halved, and so is each turn's 8
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 12u);
	EXPECT_EQ(_world->fields[field].food, 988u);
	Turn();
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 16u);
}

TEST_F(HandGrabSystemWithWorld, TheHandLocksOntoAFieldOnlyWhenItsCropCanGiveFood)
{
	const auto field = _world->registry.Create();
	_world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fields[field] = {.food = 1000, .lockable = false};
	// A field is a building of its town, which takes taps
	_world->tappable.insert(field);
	_world->underCursor = field;
	EXPECT_FALSE(Press());
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_FALSE(_system->GetLockedOn().has_value());
	EXPECT_EQ(_world->fields[field].food, 1000u);
	// The press taps the field instead, as a building of its town
	EXPECT_EQ(std::ranges::count(_world->tapped, field), 1);
}

TEST_F(HandGrabSystemWithWorld, TheLockOntoAScoopedThingHoldsWhileTheInfluenceHolds)
{
	const auto field = _world->registry.Create();
	_world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fields[field] = {.food = 1000};
	_world->underCursor = field;
	EXPECT_TRUE(Press());
	EXPECT_EQ(_system->GetLockedOn(), std::optional(field));
	Turn();
	EXPECT_EQ(_system->GetLockedOn(), std::optional(field));
	// The player's influence goes from under the hand: the lock ends, and the hand keeps its handful
	_world->influence = false;
	Turn();
	EXPECT_FALSE(_system->GetLockedOn().has_value());
	EXPECT_TRUE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AFishFarmGivesAFirstHandfulForNothingThenItsFishAsTheScoopRampsUp)
{
	const auto farm = _world->registry.Create();
	_world->registry.Assign<Transform>(farm, glm::vec3(5.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fishFarms[farm] = 100.0f;
	_world->underCursor = farm;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// The first 25 come out of nothing, and the fish leap from the farm into the hand
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_FLOAT_EQ(_world->fishFarms[farm], 100.0f);
	ASSERT_EQ(_world->fishStreams.size(), 1u);
	EXPECT_EQ(_world->fishStreams.front(), glm::vec3(5.0f, 0.0f, 5.0f));
	// Each turn the ramp's fish are taken from the farm
	Turn();
	const auto first = _world->registry.Get<const Pot>(*handful).amount - 25u;
	EXPECT_GT(first, 0u);
	EXPECT_FLOAT_EQ(_world->fishFarms[farm], 100.0f - static_cast<float>(first));
	// A farm with fewer left than the ramp wants still gives all of it
	_world->fishFarms[farm] = 1.0f;
	const auto before = _world->registry.Get<const Pot>(*handful).amount;
	Turn();
	EXPECT_GT(_world->registry.Get<const Pot>(*handful).amount - before, 1u);
	EXPECT_FLOAT_EQ(_world->fishFarms[farm], 0.0f);
	// An empty farm ends the scoop: its stream stops and the hand keeps its handful
	EXPECT_TRUE(_world->stopped.empty());
	Turn();
	EXPECT_FALSE(_world->stopped.empty());
	EXPECT_FALSE(_world->cursorPinned);
	EXPECT_EQ(_system->GetHeld(), handful);
}

TEST_F(HandGrabSystemWithWorld, AScoopsStreamFollowsTheHandAndAPourEndsAfterThreeQuartersOfASecond)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	Frame(10);
	EXPECT_FALSE(_world->streamPoints.empty());
	Release();
	_world->underCursor.reset();
	// Made ready and let go slowly, the handful pours, and the pour stops after 0.75 s
	Press();
	Frame(10);
	Release();
	ASSERT_EQ(_world->poured.size(), 1u);
	const auto stoppedBefore = _world->stopped.size();
	Frame(700);
	EXPECT_EQ(_world->stopped.size(), stoppedBefore);
	Frame(100);
	EXPECT_EQ(_world->stopped.size(), stoppedBefore + 1);
}
