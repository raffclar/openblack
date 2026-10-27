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
#include <optional>
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureReach.h"

namespace openblack::ecs::components
{

/// What a creature is doing with something, from walking up to it to the end of the animation that acts on it
struct CreatureObjectAction
{
	enum class Phase : uint8_t
	{
		/// Walking up to it or turning to it until it is in reach
		Approach,
		/// Playing the animations
		Playing,
	};
	creature_object_actions::Kind kind {creature_object_actions::Kind::PickUp};
	Phase phase {Phase::Approach};
	creature_object_actions::Status status {creature_object_actions::Status::Running};
	/// What it acts on, and the point it throws at or points at
	std::optional<entt::entity> target;
	glm::vec3 point {0.0f};

	/// Up to four animations played together by weight, all at the same time, played left to right or not
	std::array<size_t, 4> animations {};
	std::array<float, 4> weights {};
	uint8_t animationCount {0};
	bool mirrored {false};
	float timeMs {0.0f};
	float durationMs {0.0f};
	/// The animations play at two times the usual speed until the action ends, because the creature was told to hurry
	bool hurried {false};
	/// When it takes hold or lets go, in the animation's time, and whether that has happened
	float eventMs {0.0f};
	bool eventDone {false};
	/// Pointing holds the arm at its point for a while: for the seconds that it was asked to point, or its usual while.
	/// Pointing at a thing (the target), it points wherever the thing goes.
	float holdMs {0.0f};
	std::optional<float> pointSeconds;
	/// Pointing: the arm points until it is told to stop, as it does when it draws a gesture
	bool pointsUntilTold {false};
	/// Pointing: the point ends, and the arm plays to the end of its animations
	bool pointEnding {false};
	/// Pointing: the moment that the arm gets to its point, in the time of the animations
	float pointMs {0.0f};
	/// Pointing: the creature turned to face the point first, which changes how it chooses its arm
	bool pointTurned {false};

	/// Where the hand gets to in the reaching animations in the model's own size, measured as it starts. Each use
	/// multiplies them by the scale that the creature has then.
	std::optional<creature_reach::Points> reach;
	/// How far the creature can reach at its scale of the last step
	float maxReach {0.0f};
	/// How many times it has walked or turned to get in reach
	uint32_t attempts {0};
	/// Whether it has started a walk to get in reach
	bool walked {false};
	/// Whether its walk to get in reach moves it
	creature_object_actions::ApproachStall stall;
	/// Whether it turned to get the thing in reach, and reaches when the turn ends
	bool turnedToReach {false};
	/// Whether it waited one turn after its walk stopped because it was shut in, for the mind to clear the way
	bool waitedForWay {false};
	/// Throwing: how long the throw is to take to get there
	float flightSeconds {0.0f};
	/// Throwing: how long it was asked to take, none to take as long as it would to fall the distance to the target
	std::optional<float> givenFlightSeconds;
	/// Throwing into a store: it isn't done until what it threw has stopped flying, which it keeps once let go
	bool waitsForLanding {false};
	std::optional<entt::entity> thrown;
	/// Catching: where the hand closes in each catching animation, in the model's units, and how the catch goes: ready
	/// and waiting for the thing, stepping across to where it will pass, reaching for it, missed and drawing back, or
	/// caught and finishing
	enum class Catching : uint8_t
	{
		Ready,
		Stepping,
		Reaching,
		Missed,
		Caught,
	};
	std::array<glm::vec3, 4> catchHands {};
	Catching catching {Catching::Ready};
	/// Whether it has finished turning to face the thing, which it waits for only once
	bool catchTurned {false};
	/// How high the thing was against the hand at the last frame it was still in the physics
	float catchHeight {0.5f};
	/// Eating: it swallows what it eats. Then it counts the meal, it can bring the meal back up, and its hunger falls.
	/// When it is told only to eat, with no thing given, it takes in the food of what it holds and does none of these.
	/// Then the eat fails when the animation ends.
	bool swallows {true};
	/// Eating: what left its hand at the moment of eating, out of sight, until it swallows it as the animation ends
	std::optional<entt::entity> eaten;
	/// Eating: the energy that the food gave the body as the animation started, which its hunger falls by once it keeps
	/// the meal down
	float mealEnergy {0.0f};
	/// Why it gave up, when it did
	std::string failure;
	/// The cause of the failure that the mind acts on
	creature_object_actions::Failure failureKind {creature_object_actions::Failure::Other};
};

/// What a creature holds in its hand, and which hand
/// The last thing a creature let go of into the physics, which the scripts can ask for and forget
struct CreatureDroppedObject
{
	entt::entity object {entt::null};
};

struct CreatureHeldObject
{
	entt::entity object {entt::null};
	/// Held in the left hand, having reached for it with the animations mirrored
	bool mirrored {false};
	/// The hand bone that holds it. It turns with this bone and hangs below the finger ends of the bone.
	uint32_t bone {0};
};

/// A catch the creature is to make once what its body is doing is over
struct PendingCatch
{
	entt::entity object {entt::null};
};

/// Where the creature's hand closes in each catching animation, in the model's units: measured at its first catch and
/// never again, whatever its shape becomes
struct CatchHands
{
	std::array<glm::vec3, 4> hands {};
};

/// Where the creature's hand gets in its pick-up animations, at the model's own size: measured the first time it is
/// wanted and never again, whatever its shape becomes
struct PickUpReach
{
	creature_reach::Points points {};
};

/// Where the creature's foot gets in its stomp animations, at the model's own size: measured the first time it is wanted
/// and never again, whatever its shape becomes
struct StompReach
{
	creature_reach::Points points {};
};

/// How fast the creature's hand moves as it tosses something away, in the model's units a second at the model's own
/// size, before it is mirrored for the other hand: measured at its first toss and never again, whatever its shape
/// becomes
struct TossHandVelocity
{
	glm::vec3 velocity {0.0f};
};

/// On something a creature holds: which creature
struct HeldByCreature
{
	entt::entity creature {entt::null};
	/// Where the frame draws the thing in the creature's hand as the frame draws the body. Only the draw reads it.
	struct Drawn
	{
		glm::vec3 position;
		glm::mat3 rotation;
	};
	/// Empty: the thing is drawn where its Transform puts it
	std::optional<Drawn> drawn;
};

} // namespace openblack::ecs::components
