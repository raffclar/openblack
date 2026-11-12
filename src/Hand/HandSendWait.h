/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include "ECS/Components/HandSendWait.h"

/// The wait of the player's hand for its last send. These commands start the wait: the let go of the thing in the hand,
/// the give to a creature, the use of the held thing or seed on a thing or on the land, and a press on the ground of
/// the player's fight. Until the turn does the command, the hand sends no other of these commands. The fight also
/// refuses a press on a body.
///
/// The wait keeps the game turn of the send. The turn that does a command of the player keeps its own number. The hand's
/// pass of the next turn clears both numbers when that turn is not before the send.
namespace openblack::hand_send_wait
{

/// The hand sent a command at a game turn. A send at turn zero does not start a wait (vanilla-bugs row 1255, kept).
constexpr void MarkSent(ecs::components::HandSendWait& wait, uint32_t turn)
{
	wait.sentTurn = turn;
}

/// A turn did a command of the player that the wait covers
constexpr void MarkDone(ecs::components::HandSendWait& wait, uint32_t turn)
{
	wait.doneTurn = turn;
}

/// The hand's pass of a turn, after the commands of the turn. The wait ends when the last command that the turn did is
/// not before the send. Without a wait, the pass also clears the turn of the last command.
constexpr void TurnPass(ecs::components::HandSendWait& wait)
{
	if (wait.sentTurn <= wait.doneTurn)
	{
		wait = {};
	}
}

/// The hand waits for its last send
[[nodiscard]] constexpr bool Waits(const ecs::components::HandSendWait& wait)
{
	return wait.sentTurn != 0;
}

/// The end of the interface lock: the action of the hand stops, and the wait ends at once. The hand can send again in
/// the same frame. The lock ends also when the turn did not do the command yet.
constexpr void LockEnd(ecs::components::HandSendWait& wait)
{
	wait = {};
}

/// The commands of a hand that the turn does, which end the interface lock of the hand when the turn cannot do them
enum class TurnCommand : uint8_t
{
	/// The let go of the held thing, or the cast of the held seed on the land
	LetGo,
	/// The use of the held thing or seed on a thing
	Apply,
	/// The give of the held thing to a creature
	Give,
	/// The start of the lock on a thing: a creature, a pile, a field, a fish farm or a totem
	Lock,
	/// The end of the lock on a thing
	Unlock,
	/// The let go of the button that holds a miracle on, for a miracle that the hand pours
	CastUnlock,
	/// The call-off of a gift to a creature
	GiveCallOff,
};

/// What the turn knows of the hand and of the thing of a command, before it does the command
struct TurnFacts
{
	/// The thing of the command is in the world and not on its way out of it
	bool thingThere {false};
	/// The hand holds something: a thing or a miracle
	bool holdsSomething {false};
	/// The thing that the hand holds is in the world and not on its way out of it
	bool heldThere {false};
	/// A hand already holds the thing of the command in a lock
	bool thingLocked {false};
};

/// Whether the command ends the interface lock of the hand of its player, at the turn and before the command acts:
/// - The let go ends it when the hand holds nothing.
/// - The use ends it when the thing is not there, or when the hand holds nothing.
/// - The give ends it when the creature is not there, or when the hand holds nothing that is there.
/// - The lock ends it when the thing is not there, or when a hand already holds it in a lock.
/// - The end of a lock, and the let go of a poured miracle, end it when the thing is not there.
/// - The call-off of a gift always ends it.
[[nodiscard]] constexpr bool TurnEndsLock(TurnCommand command, const TurnFacts& facts)
{
	switch (command)
	{
	case TurnCommand::LetGo:
		return !facts.holdsSomething;
	case TurnCommand::Apply:
		return !facts.thingThere || !facts.holdsSomething;
	case TurnCommand::Give:
		return !facts.thingThere || !facts.holdsSomething || !facts.heldThere;
	case TurnCommand::Lock:
		return !facts.thingThere || facts.thingLocked;
	case TurnCommand::Unlock:
	case TurnCommand::CastUnlock:
		return !facts.thingThere;
	case TurnCommand::GiveCallOff:
		return true;
	}
	return false;
}

/// The kind of the action of the hand in a frame, for the ends of the interface lock
enum class FrameAction : uint8_t
{
	/// The hand does no action, or it is in the fight or on a totem. Both buttons do not end these.
	Free,
	/// The hand holds a thing or a seed for a use at the let go or for a pour. The action needs the held thing, and
	/// both buttons end it.
	UsesHeld,
	/// The hand takes a thing: from the press on the thing, while it waits or pulls, until the turn puts the thing in the
	/// hand. The action needs the thing, and both buttons end it.
	Takes,
	/// The hand does another action: a lock on a creature, a pile, a field or a fish farm, or a press that waits for a
	/// tap on a creature. Both buttons end it.
	Other,
};

/// What the interface of the hand of this computer knows at the start of a frame
struct FrameFacts
{
	/// A front-end box takes the interface: the menu of the game, the question at the start of a game, or the tattoo
	/// editor
	bool frontEndBox {false};
	/// The interface is off: a script or the game has the cinema bars in, or a hand demonstration plays
	bool interfaceOff {false};
	/// The hand grips a totem. The interface lock stays on while the interface is off.
	bool onTotem {false};
	FrameAction action {FrameAction::Free};
	/// The select button and the action button are both down
	bool bothButtons {false};
	/// The hand holds a thing or a seed that is in the world and not on its way out of it
	bool heldThere {false};
	/// The thing that the hand takes is in the world and not on its way out of it
	bool takenThere {false};
};

/// Whether the interface lock of the hand ends in this frame, before the hand acts on the input of the frame:
/// - The interface is off, and the hand does not grip a totem.
/// - The action needs the held thing, and the thing is not there.
/// - The hand takes a thing, and the thing is not there.
/// - A front-end box takes the interface.
/// - Both buttons are down in an action that both buttons end.
[[nodiscard]] constexpr bool FrameEndsLock(const FrameFacts& facts)
{
	if (facts.interfaceOff && !facts.onTotem)
	{
		return true;
	}
	if (facts.action == FrameAction::UsesHeld && !facts.heldThere)
	{
		return true;
	}
	if (facts.action == FrameAction::Takes && !facts.takenThere)
	{
		return true;
	}
	if (facts.frontEndBox)
	{
		return true;
	}
	return facts.action != FrameAction::Free && facts.bothButtons;
}

} // namespace openblack::hand_send_wait
