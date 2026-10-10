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

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Animals/AnimalMove.h"
#include "Animals/Zoomer.h"
#include "Enums.h"
#include "Magic/FlockMiracleRules.h"

namespace openblack::ecs::components
{

/// What an animal is doing
enum class AnimalState : uint8_t
{
	/// Choosing what to do next: a flock's leader its next leg, a follower a point near the leader, a wolf its run
	DecideWhatToDo,
	/// A leader flying to where it was sent, or the first leg of its followers; then choosing again
	SpecialMoveToPos,
	/// A follower flying to a point near its leader, then to its place in the formation
	FollowFlock,
	/// A leader choosing its next leg about where its flock was made
	StartWander,
	/// Going somewhere, then into its final state: a wolf runs where it was sent, hunting on the way
	MoveToPos,
	/// A wolf that got where it was sent: it fades away
	SetDying,
	/// Wandering off, which for a wolf is running where it was sent
	Wander,
	/// A wolf after its prey
	Chase,
	/// A wolf leaping at its prey
	Pounce,
	/// A wolf settling down to eat
	StartToEat,
	/// Holding still while its clip plays out, then into its next state
	WaitForClip,
	/// A wolf eating what it brought down
	Eat,
	/// Brought down by a hunter, being eaten
	Downed,
	/// Killed: falling dead, its kind's dying clip playing out once
	Dying,
	/// Lying dead its time, then gone
	Dead,
	/// Running from a thing it reacts to, as from something flying at it
	FleeingFromObject,
	/// Far enough from the thing it fled, turned to watch it
	FleeingAndLookingAtObject,
	/// Held by a script that has given it nothing to do: it stays as it is
	InScript,
	/// Set by a script to go about with its flock: a bird picks its next leg as its leader would, about its flock's home
	MoveInFlock,
};

/// A living animal: a kind of the tables, flying or on the land
struct Animal
{
	AnimalInfo type {AnimalInfo::None};
	PlayerNames owner {PlayerNames::NEUTRAL};
	/// The flock it belongs to, none for an animal on its own
	entt::entity flock {entt::null};
	/// The thing it flees, none when fleeing nothing
	entt::entity fleeing {entt::null};
	/// The kind of reaction it flees: the reaction's own table row says how far it runs from the thing
	Reaction fleeReaction {Reaction::ReactToFlyingObject};
	AnimalState state {AnimalState::DecideWhatToDo};
	/// The state it goes into once it gets where it is going, and once its clip has played while it waits
	AnimalState finalState {AnimalState::DecideWhatToDo};
	AnimalState afterClip {AnimalState::DecideWhatToDo};
	uint32_t turnsInState {0};
	/// Its move across the land: where it is and is going, in map units, which way it faces and how fast it goes
	animals::Move move;
	/// How high above the land at its goal it wants to be, and how high above the land under it it is
	float goalHeight {0.0f};
	float height {0.0f};
	/// How far it is tilted into a turn, gliding to the tilt each turn sets
	animals::Zoomer bank;
	/// Hungrier as the turns go by, up to its kind's hunger; it hunts once there
	int32_t hunger {0};
	/// The clip it plays and its place in it, in milliseconds
	AnimId animation {AnimId::Invalid};
	uint32_t clipPlace {0};
	/// Where it was and is at the turn's ends, which it is drawn between, and the way it faced, in radians from +x
	/// towards +z
	glm::vec3 previousPosition {0.0f};
	glm::vec3 position {0.0f};
	float previousHeading {0.0f};
	float heading {0.0f};

	/// 0 to 1; an animal with none left is dead, which the miracles hurt and heal as the other living things
	float life {1.0f};
	/// Once killed, the turns its body lies dead before it goes
	int32_t deadTurns {0};

	[[nodiscard]] bool Dead() const { return !(life > 0.0f); }
};

/// The doves or bats about a temple: its flock, made the first time it is seen to, and the look of the temple's model
/// the flock's height was last taken for
struct TempleBirds
{
	entt::entity flock {entt::null};
	std::optional<glm::vec2> look;
};

/// An animal a miracle made: it fades out rather than dying, and it goes with its miracle
struct SpellAnimal
{
	entt::entity spell {entt::null};
	/// Its fade out, none while it isn't fading
	std::optional<animals::Zoomer> fade;
	/// Where it was at the last turn, to see whether it crossed into a shield
	glm::vec3 previous {0.0f};

	[[nodiscard]] bool Fading() const { return fade.has_value(); }
	/// Its alpha, 0..255
	[[nodiscard]] float Alpha() const { return fade.has_value() ? fade->Value() : magic::flock::k_FullAlpha; }
};

/// A spell wolf's run and hunt: where it is sent, the strip of land it hunts along, what it is after or eating
struct SpellWolf
{
	glm::vec2 finalDestination {0.0f};
	magic::flock::Corridor corridor;
	entt::entity prey {entt::null};
	/// The turn its chase started
	uint32_t huntStart {0};
	/// Where the prey it last found was, which it goes to if it finds none
	std::optional<glm::vec2> remembered;
	/// What it is eating, and how many more mouthfuls
	entt::entity food {entt::null};
	int eatCount {0};
};

/// How an animal is lit: by the land's light where it is, as most things are, or in a colour of its own
enum class AnimalLight : uint8_t
{
	/// The brightest of the land's light, wherever it is: the miracles' doves and bats
	BrightestLand,
	/// White: the miracles' wolves
	White,
	/// A colour of its own, the land's light left out: the whales' dark blue
	Own,
};

/// How an animal is drawn this frame: its bones as its clip poses it, its light, and its alpha (0..255), less than
/// full only for a translucent animal, which blends over what is behind it
struct AnimalPose
{
	std::vector<glm::mat4> bones;
	AnimalLight light {AnimalLight::BrightestLand};
	/// Its own colour, 0 to 255, for an animal in a light of its own
	glm::vec3 colour {255.0f, 255.0f, 255.0f};
	uint8_t alpha {255};
	/// The height in the world below which nothing of it is drawn, if any: a whale shows only what breaks the sea
	std::optional<float> cutBelow;
};

/// A villager or animal a hunter brought down: it falls, then is eaten over the turns, then dies
struct BeingEaten
{
	entt::entity hunter {entt::null};
	int turns {0};
	/// The turn it starts being eaten, once its fall has played out, and the turns of eating left after that
	int eatenFrom {0};
	int left {0};
};

} // namespace openblack::ecs::components
