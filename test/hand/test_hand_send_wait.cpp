/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/HandSendWait.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/DyingVillager.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Hand/HandSendWait.h"

using namespace openblack;
using ecs::components::HandSendWait;

TEST(HandSendWait, ASendWaitsUntilTheTurnPassAfterTheTurnThatDidIt)
{
	HandSendWait wait;
	EXPECT_FALSE(hand_send_wait::Waits(wait));
	hand_send_wait::MarkSent(wait, 40);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	// The pass of a turn with no command of the player keeps the wait
	hand_send_wait::TurnPass(wait);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	// The turn that does the command ends the wait at its pass, not before
	hand_send_wait::MarkDone(wait, 41);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	hand_send_wait::TurnPass(wait);
	EXPECT_FALSE(hand_send_wait::Waits(wait));
	EXPECT_EQ(wait.doneTurn, 0u);
}

TEST(HandSendWait, ACommandDoneBeforeTheSendDoesNotEndTheWait)
{
	HandSendWait wait;
	hand_send_wait::MarkSent(wait, 40);
	hand_send_wait::MarkDone(wait, 39);
	hand_send_wait::TurnPass(wait);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
	// A command done in the same turn as the send ends it
	hand_send_wait::MarkDone(wait, 40);
	hand_send_wait::TurnPass(wait);
	EXPECT_FALSE(hand_send_wait::Waits(wait));
}

TEST(HandSendWait, ThePassClearsTheTurnOfACommandThatNoSendWaitedFor)
{
	HandSendWait wait;
	hand_send_wait::MarkDone(wait, 50);
	hand_send_wait::TurnPass(wait);
	EXPECT_EQ(wait.doneTurn, 0u);
	// A later send in the same turn thus waits for the next command
	hand_send_wait::MarkSent(wait, 50);
	hand_send_wait::TurnPass(wait);
	EXPECT_TRUE(hand_send_wait::Waits(wait));
}

TEST(HandSendWait, ASendAtTurnZeroDoesNotWait)
{
	HandSendWait wait;
	hand_send_wait::MarkSent(wait, 0);
	EXPECT_FALSE(hand_send_wait::Waits(wait));
}

TEST(HandSendWait, TheEndOfTheInterfaceLockEndsTheWaitAtOnce)
{
	HandSendWait wait;
	hand_send_wait::MarkSent(wait, 40);
	hand_send_wait::MarkDone(wait, 39);
	// The turn did not do the send yet, but the lock ends: the hand can send again in the same frame
	hand_send_wait::LockEnd(wait);
	EXPECT_FALSE(hand_send_wait::Waits(wait));
	EXPECT_EQ(wait.doneTurn, 0u);
}

TEST(HandSendWait, TheTurnEndsTheLockOnlyForACommandThatItCannotDo)
{
	using hand_send_wait::TurnCommand;
	using hand_send_wait::TurnEndsLock;
	const hand_send_wait::TurnFacts all {.thingThere = true, .holdsSomething = true, .heldThere = true, .thingLocked = false};
	// A command that the turn can do keeps the lock, except the call-off of a gift
	for (const auto command : {TurnCommand::LetGo, TurnCommand::Apply, TurnCommand::Give, TurnCommand::Lock,
	                           TurnCommand::Unlock, TurnCommand::CastUnlock})
	{
		EXPECT_FALSE(TurnEndsLock(command, all));
	}
	EXPECT_TRUE(TurnEndsLock(TurnCommand::GiveCallOff, all));
	// A let go asks only whether the hand holds something
	EXPECT_FALSE(TurnEndsLock(TurnCommand::LetGo, {.thingThere = false, .holdsSomething = true}));
	EXPECT_TRUE(TurnEndsLock(TurnCommand::LetGo, {.thingThere = true, .holdsSomething = false}));
	// A use asks for the thing and for something in the hand, but not whether the held thing is there
	EXPECT_TRUE(TurnEndsLock(TurnCommand::Apply, {.thingThere = false, .holdsSomething = true, .heldThere = true}));
	EXPECT_TRUE(TurnEndsLock(TurnCommand::Apply, {.thingThere = true, .holdsSomething = false}));
	EXPECT_FALSE(TurnEndsLock(TurnCommand::Apply, {.thingThere = true, .holdsSomething = true, .heldThere = false}));
	// A give also asks whether the held thing is there
	EXPECT_TRUE(TurnEndsLock(TurnCommand::Give, {.thingThere = true, .holdsSomething = true, .heldThere = false}));
	EXPECT_TRUE(TurnEndsLock(TurnCommand::Give, {.thingThere = false, .holdsSomething = true, .heldThere = true}));
	// A lock asks for the thing, and whether a hand holds it already
	EXPECT_TRUE(TurnEndsLock(TurnCommand::Lock, {.thingThere = false}));
	EXPECT_TRUE(TurnEndsLock(TurnCommand::Lock, {.thingThere = true, .thingLocked = true}));
	EXPECT_FALSE(TurnEndsLock(TurnCommand::Lock, {.thingThere = true, .holdsSomething = false}));
	// The end of a lock and the let go of a poured miracle ask only for the thing
	EXPECT_TRUE(TurnEndsLock(TurnCommand::Unlock, {.thingThere = false, .holdsSomething = true, .heldThere = true}));
	EXPECT_TRUE(TurnEndsLock(TurnCommand::CastUnlock, {.thingThere = false, .holdsSomething = true, .heldThere = true}));
}

TEST(HandSendWait, TheFrameEndsTheLockForABoxAnInterfaceThatIsOffBothButtonsOrAHeldThingThatWent)
{
	using hand_send_wait::FrameAction;
	using hand_send_wait::FrameEndsLock;
	// A hand with no action keeps the lock, also with both buttons down
	EXPECT_FALSE(FrameEndsLock({}));
	// The action keeps the thing of its press, and the thing went
	EXPECT_TRUE(FrameEndsLock({.action = FrameAction::UsesHeld, .heldThere = true, .pressThingGone = true}));
	EXPECT_FALSE(FrameEndsLock({.action = FrameAction::Free, .bothButtons = true}));
	// A front-end box ends it in every action
	EXPECT_TRUE(FrameEndsLock({.frontEndBox = true}));
	EXPECT_TRUE(FrameEndsLock({.frontEndBox = true, .onTotem = true}));
	// An interface that is off ends it, but not on a totem
	EXPECT_TRUE(FrameEndsLock({.interfaceOff = true}));
	EXPECT_FALSE(FrameEndsLock({.interfaceOff = true, .onTotem = true}));
	// Both buttons end every action but the free ones
	EXPECT_TRUE(FrameEndsLock({.action = FrameAction::Other, .bothButtons = true}));
	EXPECT_TRUE(FrameEndsLock({.action = FrameAction::UsesHeld, .bothButtons = true, .heldThere = true}));
	EXPECT_FALSE(FrameEndsLock({.action = FrameAction::Other}));
	// An action that uses the held thing ends when the thing is not there
	EXPECT_TRUE(FrameEndsLock({.action = FrameAction::UsesHeld, .heldThere = false}));
	EXPECT_FALSE(FrameEndsLock({.action = FrameAction::UsesHeld, .heldThere = true}));
	EXPECT_FALSE(FrameEndsLock({.action = FrameAction::Other, .heldThere = false}));
	// The take of a thing ends with both buttons, and when the thing is not there
	EXPECT_TRUE(FrameEndsLock({.action = FrameAction::Takes, .bothButtons = true, .takenThere = true}));
	EXPECT_TRUE(FrameEndsLock({.action = FrameAction::Takes, .takenThere = false}));
	EXPECT_FALSE(FrameEndsLock({.action = FrameAction::Takes, .takenThere = true}));
	EXPECT_FALSE(FrameEndsLock({.action = FrameAction::Other, .takenThere = false}));
}

TEST(HandSendWait, AThingOfThePressThatTheHandCannotUseOrThatAHandHoldsEndsTheLock)
{
	using hand_send_wait::PressThingEndsLock;
	// A thing in the world that the hand can use keeps the lock
	EXPECT_FALSE(PressThingEndsLock({}));
	// A thing that went from the world, a villager on its way to die, or a building with no part built ends it
	EXPECT_TRUE(PressThingEndsLock({.inWorld = false}));
	EXPECT_TRUE(PressThingEndsLock({.villagerDying = true}));
	EXPECT_TRUE(PressThingEndsLock({.nothingBuilt = true}));
	// A thing that the hand of a player holds ends it
	EXPECT_TRUE(PressThingEndsLock({.inHand = true}));
	// The frame ends the lock for such a thing in each action that keeps the thing of its press
	EXPECT_TRUE(hand_send_wait::FrameEndsLock({.action = hand_send_wait::FrameAction::Other, .pressThingGone = true}));
	EXPECT_TRUE(hand_send_wait::FrameEndsLock(
	    {.action = hand_send_wait::FrameAction::Takes, .takenThere = true, .pressThingGone = true}));
}

TEST(HandSendWait, BothButtonsAloneEndTheLockOnlyWhenNoOtherEndComesFirst)
{
	using hand_send_wait::BothButtonsAloneEndLock;
	using hand_send_wait::FrameAction;
	EXPECT_TRUE(BothButtonsAloneEndLock({.action = FrameAction::Other, .bothButtons = true}));
	EXPECT_TRUE(BothButtonsAloneEndLock({.action = FrameAction::UsesHeld, .bothButtons = true, .heldThere = true}));
	// Both buttons do not end the lock of a hand that does no action
	EXPECT_FALSE(BothButtonsAloneEndLock({.action = FrameAction::Free, .bothButtons = true}));
	// Another end of the lock comes first: a box, the interface off, the held thing gone or the thing of the press
	EXPECT_FALSE(BothButtonsAloneEndLock({.frontEndBox = true, .action = FrameAction::Other, .bothButtons = true}));
	EXPECT_FALSE(BothButtonsAloneEndLock({.interfaceOff = true, .action = FrameAction::Other, .bothButtons = true}));
	EXPECT_FALSE(BothButtonsAloneEndLock({.action = FrameAction::UsesHeld, .bothButtons = true, .heldThere = false}));
	EXPECT_FALSE(BothButtonsAloneEndLock({.action = FrameAction::Other, .bothButtons = true, .pressThingGone = true}));
	EXPECT_FALSE(BothButtonsAloneEndLock({.action = FrameAction::Other}));
}

TEST(HandSendWait, AVillagerOnItsWayToDieIsAThingThatTheHandCannotUse)
{
	using ecs::components::LivingAction;
	ecs::Registry registry;
	const auto villager = registry.Create();
	registry.Assign<ecs::components::Villager>(villager);
	auto& action = registry.Assign<LivingAction>(villager, VillagerStates::MoveToPos, uint16_t {0});
	EXPECT_FALSE(ecs::IsDyingVillager(registry, villager));
	// Only the final state counts: the top state of a villager that dies is not enough
	action.states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::Dying);
	EXPECT_FALSE(ecs::IsDyingVillager(registry, villager));
	action.states.at(static_cast<size_t>(LivingAction::Index::Final)) = static_cast<uint8_t>(VillagerStates::Dying);
	EXPECT_TRUE(ecs::IsDyingVillager(registry, villager));
	// Another thing with the same state is no villager, and a thing that is gone is no villager
	const auto animal = registry.Create();
	registry.Assign<LivingAction>(animal, VillagerStates::Dying, uint16_t {0})
	    .states.at(static_cast<size_t>(LivingAction::Index::Final)) = static_cast<uint8_t>(VillagerStates::Dying);
	EXPECT_FALSE(ecs::IsDyingVillager(registry, animal));
	registry.Destroy(villager);
	EXPECT_FALSE(ecs::IsDyingVillager(registry, villager));
}
