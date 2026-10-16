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
	/// When it takes hold or lets go, in the animation's time, and whether that has happened
	float eventMs {0.0f};
	bool eventDone {false};
	/// Pointing loops its animations for a while rather than playing them once: for the seconds it was asked to point,
	/// or its usual while. Pointing at a thing (the target), it points wherever the thing goes.
	float holdMs {0.0f};
	std::optional<float> pointSeconds;

	/// Where the hand gets to in the reaching animations, and how far the creature can reach, measured as it starts
	std::optional<creature_reach::Points> reach;
	float maxReach {0.0f};
	/// How many times it has walked or turned to get in reach
	uint32_t attempts {0};
	/// Whether it has started a walk to get in reach
	bool walked {false};
	/// Whether it turned to get in reach since its last walk
	bool turnedSinceWalk {false};
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

/// How far the creature's hand gets at its furthest stretch when picking up, measured straight at the model's own size:
/// measured the first time it is wanted and never again, whatever its shape becomes
struct PickUpReach
{
	float straightReach {0.0f};
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
};

/// How the creature's town sees what it is doing, which its villagers react to, and for how long more it will once
/// the creature stops
struct CreatureTownAttitude
{
	creature_object_actions::TownAttitude attitude {creature_object_actions::TownAttitude::None};
	float secondsLeft {0.0f};
};

} // namespace openblack::ecs::components
