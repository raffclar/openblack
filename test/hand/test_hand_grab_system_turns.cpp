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

// A hand that must let go puts the thing down from where it is: it asks for no start and goes to none
TEST_F(HandGrabSystemWithWorld, AForcedDropHasNoStart)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	_system->ForceDrop();
	EXPECT_TRUE(_world->startsAsked.empty());
	EXPECT_TRUE(_world->startedFrom.empty());
	ASSERT_EQ(_world->releasedSpins.size(), 1u);
	EXPECT_EQ(_world->releasedSpins.front(), glm::vec3(0.0f));
}

// A short press sends the tap. The world taps the thing at the turn that does the command.
TEST_F(HandGrabSystemWithWorld, ATapWaitsForTheTurnThatDoesItsCommand)
{
	_deferCommands = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	Frame(50);
	Release();
	EXPECT_TRUE(_world->tapped.empty());
	// The tap and the click of the thing under the hand go to the turn
	ASSERT_EQ(_world->sent.size(), 2u);
	EXPECT_EQ(std::ranges::count_if(
	              _world->sent, [](const auto& action) { return std::holds_alternative<player_commands::HandTap>(action); }),
	          1);
	EXPECT_EQ(std::ranges::count_if(
	              _world->sent, [](const auto& action) { return std::holds_alternative<player_commands::HandClick>(action); }),
	          1);
	RunWaiting();
	ASSERT_EQ(_world->tapped.size(), 1u);
	EXPECT_EQ(_world->tapped.front(), rock);
}

// The turn does the tap at the place of the hand that the last report gave, not where the game draws the hand
TEST_F(HandGrabSystemWithWorld, ATapUsesThePlaceOfTheHandThatTheLastReportGave)
{
	_deferCommands = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	Frame(50);
	Release();
	ASSERT_TRUE(_world->tapped.empty());
	// A report comes before the turn of the tap. The game draws the hand at a different place.
	const glm::vec3 reported {12.0f, 3.0f, -34.0f};
	_world->reportedPlaces[_world->hand] = reported;
	ASSERT_NE(_world->PoseOf(_world->hand).origin, reported);
	RunWaiting();
	ASSERT_EQ(_world->tapPlaces.size(), 1u);
	EXPECT_EQ(_world->tapPlaces.front(), reported);
}

// The hand's work of the turn takes a thing in flight once the button was held long enough, also with no frame between
TEST_F(HandGrabSystemWithWorld, TheTurnWorkOfTheHandTakesAFlyingThingAfterTheWait)
{
	_deferCommands = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->flying.insert(rock);
	_world->underCursor = rock;
	Press();
	_system->InterfaceTurn(_now + 100, (_now + 100) / 100);
	EXPECT_TRUE(_world->sent.empty());
	_system->InterfaceTurn(_now + 300, (_now + 300) / 100);
	ASSERT_EQ(_world->sent.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<player_commands::HandTake>(_world->sent.front()));
	RunWaiting();
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
}

// A new land clears the hand: what it held is let go
TEST_F(HandGrabSystemWithWorld, ANewLandLetsGoOfWhatTheHandHeld)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_system->DoTake(_world->HandPlayer(), rock);
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	_system->Reset();
	EXPECT_FALSE(_system->GetHeld().has_value());
}

// A take that the hand no longer waits for does nothing at its turn
// A new land clears the hand and its thing goes with the old land: the take that waits does nothing
TEST_F(HandGrabSystemWithWorld, ATakeOfAThingThatWentWithTheLandDoesNothing)
{
	_deferCommands = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->flying.insert(rock);
	_world->underCursor = rock;
	Press();
	_system->InterfaceTurn(_now + 300, (_now + 300) / 100);
	ASSERT_EQ(_world->sent.size(), 1u);
	_system->Reset();
	_world->registry.Destroy(rock);
	RunWaiting();
	EXPECT_FALSE(_system->GetHeld().has_value());
}

// A command of another player does not move this hand
TEST_F(HandGrabSystemWithWorld, TheHandDoesOnlyTheCommandsOfItsOwnPlayer)
{
	_deferCommands = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->flying.insert(rock);
	_world->underCursor = rock;
	Press();
	_system->InterfaceTurn(_now + 300, (_now + 300) / 100);
	_system->DoTake(PlayerNames::PLAYER_TWO, rock);
	EXPECT_FALSE(_system->GetHeld().has_value());
	RunWaiting();
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
}

TEST_F(HandGrabSystemWithWorld, APressOnAPileSendsTheLockAndTheTurnStartsTheScoop)
{
	_deferCommands = true;
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	// The press sends the lock and takes nothing from the pile
	EXPECT_TRUE(Press());
	ASSERT_EQ(_waiting.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandLock>(_waiting.front()));
	EXPECT_EQ(std::get<player_commands::HandLock>(_waiting.front()).object, pile);
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->registry.Get<const Pot>(pile).amount, 1000u);
	// Let go before the turn, the end of the lock follows the lock
	Release();
	ASSERT_EQ(_waiting.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandUnlock>(_waiting.back()));
	EXPECT_EQ(std::get<player_commands::HandUnlock>(_waiting.back()).object, pile);
	// The turn takes the first handful, then ends the scoop: the hand keeps its handful
	RunWaiting();
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_EQ(_world->registry.Get<const Pot>(pile).amount, 975u);
	Turn();
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u);
	EXPECT_EQ(_system->TakeAmountToolTip(), std::optional(25u));
}

TEST_F(HandGrabSystemWithWorld, TheLockAndItsEndOfAnotherPlayerScoopWithThatPlayersHand)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->secondPlayerHasHands = true;
	_system->DoLock(PlayerNames::PLAYER_TWO, pile);
	// The second player's hand holds the handful, and the first player's hand holds nothing
	EXPECT_FALSE(_system->GetHeld().has_value());
	const auto* second = _world->registry.TryGet<const HandGrab>(_world->secondHand);
	ASSERT_NE(second, nullptr);
	ASSERT_TRUE(_world->registry.Valid(second->object));
	const auto handful = second->object;
	EXPECT_EQ(_world->registry.Get<const Pot>(handful).amount, 25u);
	EXPECT_EQ(second->scoopSource, pile);
	// The end of the lock of the first player leaves the second player's scoop
	_system->DoUnlock(PlayerNames::PLAYER_ONE, pile);
	EXPECT_EQ(_world->registry.Get<const HandGrab>(_world->secondHand).scoopSource, pile);
	_system->DoUnlock(PlayerNames::PLAYER_TWO, pile);
	EXPECT_TRUE(_world->registry.Get<const HandGrab>(_world->secondHand).scoopSource == entt::null);
	EXPECT_FALSE(_world->acting.has_value());
}

// The questions of this computer are about the hand of its own god; the turn asks about the hand of a player by name
TEST_F(HandGrabSystemWithWorld, ThisComputerAsksAboutTheHandOfItsOwnGod)
{
	_world->networkGame = true;
	_world->secondPlayerHasHands = true;
	_world->ownHandPlayer = PlayerNames::PLAYER_TWO;
	const auto first = _world->AddRock({0.0f, 0.0f, 0.0f});
	_system->DoTake(PlayerNames::PLAYER_ONE, first);
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_system->DoLock(PlayerNames::PLAYER_TWO, pile);
	const auto handful = _world->registry.Get<const HandGrab>(_world->secondHand).object;
	EXPECT_EQ(_system->GetHeld(), std::optional(handful));
	EXPECT_EQ(_system->GetLockedOn(), std::optional(pile));
	EXPECT_EQ(_system->GetHeldOf(PlayerNames::PLAYER_ONE), std::optional(first));
	EXPECT_FALSE(_system->GetLockedOnOf(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_EQ(_system->GetLockedOnOf(PlayerNames::PLAYER_TWO), std::optional(pile));
	// The questions leave no acting player behind them
	EXPECT_FALSE(_world->acting.has_value());
}

// A creature takes a gift from the hand that holds it, whichever god's hand that is
TEST_F(HandGrabSystemWithWorld, ACreatureTakesTheGiftFromTheHandOfTheGodThatHoldsIt)
{
	_world->secondPlayerHasHands = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_system->DoTake(PlayerNames::PLAYER_TWO, rock);
	ASSERT_EQ(_system->GetHeldOf(PlayerNames::PLAYER_TWO), std::optional(rock));
	EXPECT_TRUE(_system->HandOver(rock));
	EXPECT_FALSE(_system->GetHeldOf(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_FALSE(_world->acting.has_value());
}

TEST_F(HandGrabSystemWithWorld, TheEndOfTheLockOfAnotherThingLeavesTheScoop)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	_system->DoUnlock(_world->HandPlayer(), *handful);
	Turn();
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
}

TEST_F(HandGrabSystemWithWorld, TheSeedInTheHandOfAnotherGodIsDrawnAtTheHandItReported)
{
	auto& registry = _world->registry;
	const auto seed = registry.Create();
	registry.Assign<Transform>(seed, glm::vec3(1.0f, 2.0f, 3.0f), glm::mat3(2.0f), glm::vec3(1.0f));
	registry.Assign<HandHeldSeed>(_world->secondHand, HandHeldSeed {.seed = seed});
	const auto ownSeed = registry.Create();
	registry.Assign<Transform>(ownSeed, glm::vec3(7.0f, 8.0f, 9.0f), glm::mat3(2.0f), glm::vec3(1.0f));
	registry.Assign<HandHeldSeed>(_world->hand, HandHeldSeed {.seed = ownSeed});
	_world->otherHands.insert(_world->secondHand);
	_world->reportedPlaces[_world->secondHand] = glm::vec3(40.0f, 5.0f, 60.0f);
	_system->PlaceOthersHeldForFrame(20);
	// Upright at the reported hand, as another god's held thing is
	const auto& placed = registry.Get<const Transform>(seed);
	EXPECT_EQ(placed.position, glm::vec3(40.0f, 5.0f, 60.0f));
	EXPECT_EQ(placed.rotation, glm::mat3(1.0f));
	// The seed of the own hand is where the hand's pose puts it
	EXPECT_EQ(registry.Get<const Transform>(ownSeed).position, glm::vec3(7.0f, 8.0f, 9.0f));
}

// The scoop continues in the input of the turn, and the hand step of the turn takes no handful
TEST_F(HandGrabSystemWithWorld, TheScoopContinuesInTheInputOfTheTurnAndNotInTheHandStep)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	EXPECT_TRUE(Press());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	_system->ProcessTurn();
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u);
	_system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
	// Out of the influence, the scoop stops in the input of the turn
	_world->influence = false;
	_system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_world->registry.Get<const Pot>(*handful).amount, 25u + 8u);
	EXPECT_FALSE(_world->cursorPinned);
	EXPECT_FALSE(_world->acting.has_value());
}

// The scoop of another god continues with the hand of that god, and the first player's hand stays empty
TEST_F(HandGrabSystemWithWorld, TheScoopOfAnotherGodContinuesWithThatGodsHand)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->secondPlayerHasHands = true;
	_system->DoLock(PlayerNames::PLAYER_TWO, pile);
	const auto handful = _world->registry.Get<const HandGrab>(_world->secondHand).object;
	_system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_world->registry.Get<const Pot>(handful).amount, 25u);
	_system->LockTurn(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_world->registry.Get<const Pot>(handful).amount, 25u + 8u);
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_FALSE(_world->acting.has_value());
}

// A held thing that goes leaves the hand in the input of the next turn
TEST_F(HandGrabSystemWithWorld, AHeldThingThatGoesLeavesTheHandInTheInputOfTheTurn)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	_world->registry.Destroy(rock);
	_system->LockTurn(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_system->IsBusy());
}

TEST_F(HandGrabSystemGiving, WhileTheHandWaitsForItsLastSendAPressSendsNoGive)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	_deferCommands = true;
	_world->sendWaits = true;
	// The press is taken, but no give goes
	EXPECT_TRUE(Press());
	EXPECT_TRUE(_waiting.empty());
	EXPECT_EQ(_world->sendMarks, 0);
	Release();
	// After the wait, the next press sends the give and starts a wait
	_world->sendWaits = false;
	EXPECT_TRUE(Press());
	ASSERT_EQ(_waiting.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<player_commands::HandGive>(_waiting.front()));
	EXPECT_EQ(_world->sendMarks, 1);
}

// The end of the interface lock stops a take that pulls at once. The thing stays where it stands, nothing is taken, and
// the later let go of the button is not a tap.
TEST_F(HandGrabSystemWithWorld, TheEndOfTheInterfaceLockStopsAPullAndThePressIsNoTap)
{
	const auto tree = _world->AddTree({0.0f, 0.0f, 0.0f}, 1.0e6f);
	_world->underCursor = tree;
	ASSERT_TRUE(Press());
	Frame(10);
	Frame(10);
	ASSERT_EQ(_system->GetTaking(), std::optional(tree));
	_world->sent.clear();
	_system->StopActionAtLockEnd();
	EXPECT_EQ(_system->GetTaking(), std::nullopt);
	EXPECT_FALSE(_system->IsPulling());
	EXPECT_TRUE(_world->sent.empty());
	EXPECT_EQ(Release(), std::nullopt);
	EXPECT_TRUE(_world->tapped.empty());
	EXPECT_TRUE(_world->sent.empty());
	EXPECT_FALSE(_system->GetHeld().has_value());
}

// The end of the interface lock after the hand sent a take lets go of the thing at once. The turn still puts the thing
// in the hand, and this take does not end the interface lock again.
TEST_F(HandGrabSystemWithWorld, TheEndOfTheInterfaceLockAfterTheTakeLetsGoOfTheThingUntilTheTurn)
{
	const auto rock = _world->AddRock({0.0f, 5.0f, 0.0f});
	_world->underCursor = rock;
	_world->flying.insert(rock);
	_deferCommands = true;
	ASSERT_TRUE(Press());
	Frame(230);
	ASSERT_EQ(_system->GetTaking(), std::optional(rock));
	ASSERT_EQ(_waiting.size(), 1u);
	_system->StopActionAtLockEnd();
	EXPECT_EQ(_system->GetTaking(), std::nullopt);
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_EQ(Release(), std::nullopt);
	EXPECT_TRUE(_world->tapped.empty());
	RunWaiting();
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
	EXPECT_EQ(_world->lockEnds, 0);
}

// The end of the interface lock in a hand that is ready to throw keeps the thing in the hand. The spring lets go of the
// hand, and the let go of the button throws nothing.
TEST_F(HandGrabSystemWithWorld, TheEndOfTheInterfaceLockKeepsAThingThatTheHandWasReadyToThrow)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	_world->sent.clear();
	Press();
	ASSERT_TRUE(_system->IsAboutToThrow());
	_system->StopActionAtLockEnd();
	EXPECT_FALSE(_system->IsAboutToThrow());
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
	Frame(10, {5.0f, 0.0f, 0.0f});
	Release();
	EXPECT_TRUE(_world->sent.empty());
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
}

namespace
{
size_t UnlocksIn(const std::vector<player_commands::Action>& sent)
{
	return static_cast<size_t>(std::ranges::count_if(
	    sent, [](const auto& action) { return std::holds_alternative<player_commands::HandUnlock>(action); }));
}
} // namespace

// The end of the interface lock in a scoop sends the end of the lock on the pile one time, and the stream of the scoop
// stops at once. The turn ends the scoop. The let go of the button and the next ends of the lock send nothing more.
TEST_F(HandGrabSystemWithWorld, TheEndOfTheInterfaceLockSendsTheEndOfAScoopOneTime)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	_deferCommands = true;
	ASSERT_TRUE(Press());
	RunWaiting();
	ASSERT_EQ(_system->GetLockedOn(), std::optional(pile));
	ASSERT_EQ(_world->streams, 1u);
	_world->sent.clear();
	_system->StopActionAtLockEnd();
	EXPECT_EQ(UnlocksIn(_world->sent), 1u);
	EXPECT_EQ(_world->stopped, std::vector<uint32_t> {1u});
	_system->StopActionAtLockEnd();
	Release();
	EXPECT_EQ(UnlocksIn(_world->sent), 1u);
	RunWaiting();
	EXPECT_EQ(_system->GetLockedOn(), std::nullopt);
	EXPECT_TRUE(_system->GetHeld().has_value());
}

// After the let go of the button sent the end of the scoop, the end of the interface lock sends it one more time
TEST_F(HandGrabSystemWithWorld, TheEndOfTheInterfaceLockAfterTheLetGoSendsTheEndOfTheScoopAgain)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	_deferCommands = true;
	ASSERT_TRUE(Press());
	RunWaiting();
	_world->sent.clear();
	Release();
	EXPECT_EQ(UnlocksIn(_world->sent), 1u);
	_system->StopActionAtLockEnd();
	EXPECT_EQ(UnlocksIn(_world->sent), 2u);
	_system->StopActionAtLockEnd();
	EXPECT_EQ(UnlocksIn(_world->sent), 2u);
}

// A lock on a pile that no turn did yet sends its end at the end of the interface lock. The let go sends nothing more.
TEST_F(HandGrabSystemWithWorld, TheEndOfTheInterfaceLockEndsALockThatNoTurnDidYet)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	_deferCommands = true;
	ASSERT_TRUE(Press());
	ASSERT_EQ(_waiting.size(), 1u);
	_world->sent.clear();
	_system->StopActionAtLockEnd();
	EXPECT_EQ(UnlocksIn(_world->sent), 1u);
	Release();
	EXPECT_EQ(UnlocksIn(_world->sent), 1u);
	// The turn does the lock and then its end
	RunWaiting();
	EXPECT_EQ(_system->GetLockedOn(), std::nullopt);
}
