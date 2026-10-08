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

#include <array>
#include <span>

#include "Creature/CreatureHandRules.h"

/// The hand holding a creature, frame by frame: the locked select that takes hold of it and lets it go, the time the
/// hand holds it (a click when short), and when the render hand enters and leaves its CREATURE state. Pure steps: the
/// hand reads the facts, and does what the step says. Wiki: docs/bw1-notes/hand-and-interface.md, "The hand on a
/// creature".
namespace openblack::ecs::hand_creature
{

/// How far the hand's locked select on a creature is: waiting for the turn to lock it, held, let go and waiting to end
/// it, then waiting for the end to be applied
enum class LockPhase : uint8_t
{
	Waiting,
	Held,
	WaitingForLockOff,
	Completion,
};

struct LockState
{
	LockPhase phase {LockPhase::Waiting};
	/// The action button went up since the press on the creature, kept until the held phase reads it, as the interface
	/// keeps its button bits
	bool released {false};
};

/// What the hand knows about its locked creature this frame
struct LockFacts
{
	/// The creature is still there
	bool available {false};
	/// The start packet locked it for the hand
	bool locked {false};
	/// The hand's locked select is still on this creature (a lock the turn refused has been cleared from it)
	bool stillSelected {false};
	/// The action button went up this frame
	bool actionReleased {false};
};

struct LockStep
{
	LockState next;
	/// Send the end of the locked select
	bool sendEnd {false};
	/// The locked select is over: the hand lets the creature go and forgets it (after the end, when both are set)
	bool letGo {false};
};

/// One frame of the creature's locked select: held once locked, let go at the release, ended one frame later, and over
/// once the end is applied. A creature no longer there ends it at once
[[nodiscard]] LockStep StepLock(const LockState& state, const LockFacts& facts);

/// The render hand's state while it holds a creature
constexpr int32_t k_CreatureHandState = 7;

/// The time the hand holds a creature, in camera ms, from the first frame of the Creature Interaction hand state
struct Interaction
{
	bool started {false};
	uint32_t cameraMs {0};
};

/// What the hand knows about the creature in it this frame
struct FrameFacts
{
	/// The interface's hand state is Creature Interaction
	bool creatureInteraction {false};
	/// The render hand has a creature
	bool hasCreature {false};
	/// This frame's camera ms
	uint32_t cameraMs {0};
	/// The state the render hand is asked for this frame
	int32_t requiredState {0};
	/// The state the render hand is in
	int32_t renderState {0};
	/// The creature is locked for the hand
	bool creatureLocked {false};
};

/// What the hand sends, in this order
enum class Send : uint8_t
{
	/// The click packet: the player's leash key
	Click,
	/// Leaving the CREATURE state: the creature is let go, and how it was treated is sent to its mind
	Feedback,
};

struct FrameStep
{
	Interaction interaction;
	/// The render hand's state from this frame
	int32_t renderState {0};
	std::array<Send, 2> sends {};
	uint8_t sendCount {0};

	[[nodiscard]] std::span<const Send> Sends() const { return std::span(sends).first(sendCount); }
};

/// One frame of the render hand with a creature. The camera ms count on every frame and are read only between the
/// interaction's first frame and its end; let go of the player's own creature within a click's time, the click is sent.
/// The CREATURE state begins only once the creature is locked, and leaving it sends the feedback, after the click.
/// `ownCreature` says whether the render hand's creature is the player's, and is asked only when the interaction ends
template <typename OwnCreature>
[[nodiscard]] FrameStep StepFrame(const Interaction& interaction, const FrameFacts& facts, OwnCreature&& ownCreature)
{
	FrameStep step {.interaction = interaction, .renderState = facts.renderState};
	step.interaction.cameraMs += facts.cameraMs;
	if (facts.creatureInteraction)
	{
		if (facts.hasCreature && !step.interaction.started)
		{
			step.interaction.started = true;
			step.interaction.cameraMs = 0;
		}
	}
	else if (step.interaction.started)
	{
		step.interaction.started = false;
		if (creature_hand::IsClick(step.interaction.cameraMs) && facts.hasCreature && ownCreature())
		{
			step.sends[step.sendCount++] = Send::Click;
		}
	}
	auto state = facts.requiredState;
	if (state == k_CreatureHandState && facts.renderState != k_CreatureHandState && !facts.creatureLocked)
	{
		// until the creature is locked for the hand, the hand stays as it is
		state = facts.renderState;
	}
	if (state != facts.renderState && facts.renderState == k_CreatureHandState)
	{
		step.sends[step.sendCount++] = Send::Feedback;
	}
	step.renderState = state;
	return step;
}

} // namespace openblack::ecs::hand_creature
