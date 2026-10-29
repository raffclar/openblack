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

#include <chrono>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// An animal as a saved game keeps it
struct LoadedAnimal
{
	AnimalInfo type {AnimalInfo::None};
	/// Its place and its goal in whole map units, and its height in the world
	glm::ivec2 position {0};
	float altitude {0.0f};
	glm::ivec2 goal {0};
	/// The step of a turn, in whole map units
	glm::ivec2 step {0};
	/// The way it faces in game angles, and its whole map units a turn
	uint16_t angle {0};
	uint16_t speed {0};
	/// The state of its move, by its number in the game's table of the move states
	uint8_t moveState {0};
	float scale {1.0f};
	float life {1.0f};
	uint32_t birthTurn {0};
	/// Its state and the state that it goes into next, by their numbers in the game's table of the animal states
	uint8_t state {0};
	uint8_t finalState {0};
	/// The flock that it joins at the start of the line, and its town. None for none.
	entt::entity flock {entt::null};
	entt::entity town {entt::null};
};

/// The animals: the land's birds and grazers, the doves or bats about each temple, and the flocks of doves and bats and the
/// packs of wolves the miracles make. A bird flock's leader flies legs about its home or where it is sent and its followers
/// keep a formation behind it; a wolf runs where it is sent, hunting what crosses its way. The miracle's animals fade out
/// rather than die.
class AnimalSystemInterface: public SavedStateInterface
{
public:
	virtual ~AnimalSystemInterface() = default;

	/// A turn of every animal: BeginTurn, AnimalTurn for each animal, then EndTurn
	virtual void ProcessTurn() = 0;
	/// Before the animals' turns. False when the game has no info tables, and then the animals do nothing this turn.
	virtual bool BeginTurn() = 0;
	/// The own turn of one animal, which the turn of the living gives it at its place in the list of the living
	virtual void AnimalTurn(entt::entity animal) = 0;
	/// After the animals' turns: the miracle's animals fade, and the temples' flocks are seen to
	virtual void EndTurn() = 0;
	/// Each frame: the animals are drawn between their last two turns, each posed by its own clip played on by the
	/// frame's game time
	virtual void Update(std::chrono::milliseconds frameGameTime, float turnFraction) = 0;
	/// Before the steps of a turn: each animal stands at the place of the last turn, upright and turned to its heading. The
	/// frames draw it between its last two turns, tilted and on the slope, but the turn sees it at its place in the game,
	/// as the game does.
	virtual void ShowTurnEnd() = 0;
	virtual void Reset() = 0;

	/// A new flock with nobody in it yet, made at a point its leader's legs wander about
	virtual entt::entity CreateFlock(glm::vec2 centre, float domainRadius, float flockDistance) = 0;
	/// An animal a miracle makes, joining a flock, born at a random age at its size for its age (a spell wolf always
	/// grown); it faces a game angle (2048 to the circle, 0 along +x, a quarter turn along +z), and a bird flies at a
	/// height above the land
	virtual entt::entity CreateSpellAnimal(AnimalInfo type, glm::vec2 position, float heightAboveLand, uint16_t angle,
	                                       PlayerNames owner, entt::entity flock, entt::entity spell) = 0;
	/// A flock a land script makes with its number, where it says, wandering about its home as far as it says (its
	/// kind's way when it says none), its followers within so far of their leader
	virtual entt::entity CreateScriptFlock(int32_t id, glm::vec2 position, glm::vec2 home, float reach,
	                                       float flockDistance) = 0;
	/// The latest flock a land script made with a number, none for none
	[[nodiscard]] virtual entt::entity FindScriptFlock(int32_t id) const = 0;
	/// One of the land's birds, as a land script or a temple makes it: flying at its kind's height over a point, at an
	/// age (a random one for none), joining a flock, or one of its own for none
	virtual entt::entity CreateBird(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flock) = 0;
	/// The town a flock belongs to, as a land script gives it; none for none
	virtual void SetFlockTown(entt::entity flock, entt::entity town) = 0;
	/// One of the land's grazers (sheep, cows, horses, pigs and tortoises), as a land script makes it: on the land at a
	/// point facing along +x, at an age (a random one for none), joining a flock, or one of its own for none, and
	/// belonging to a town, none for none. None for a kind that doesn't graze.
	virtual entt::entity CreateGrazer(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flock,
	                                  entt::entity town) = 0;
	/// An animal a challenge script makes: as a land script's animal on its own, at a random age, but in a flock of its own
	/// that keeps within 2 m of where it was made, and held still for the script. None for a kind openblack can't make yet.
	virtual entt::entity CreateScriptAnimal(AnimalInfo type, glm::vec2 position) = 0;
	/// An animal of any kind that stays at its position, with a size and a heading. It is in no flock and does not move.
	/// It shows the clip and the light of an animal that a script holds still.
	virtual entt::entity CreateStillAnimal(AnimalInfo type, glm::vec3 position, float heading, float scale) = 0;
	/// An animal of a saved game, as the save keeps it. It joins its flock last in the line, so that the members of a
	/// flock made in the order of their line keep that order. None for a kind that is not a land animal.
	virtual entt::entity CreateLoadedAnimal([[maybe_unused]] const LoadedAnimal& loaded) { return entt::null; }
	/// The flock of the birds of a temple, made about the temple the first time. None for what is not a temple.
	virtual entt::entity TempleFlock([[maybe_unused]] entt::entity temple) { return entt::null; }
	/// An animal leaves its flock, which goes when it is left empty, and joins another as its newest member
	virtual void JoinFlock(entt::entity animal, entt::entity flock) = 0;
	/// An animal leaves its flock, which goes once it has no one left
	virtual void LeaveFlock([[maybe_unused]] entt::entity animal) {}
	/// A script sets the state of an animal by its number in the game's animal state table, for example held still or
	/// with its flock. False for a state that openblack does not have, or a state with a start of its own.
	virtual bool SetScriptState(entt::entity animal, LivingStates state) = 0;
	/// A script sends an animal to a point on the land, where it is held still for the script; one already there is held
	/// at once. One held in a hand, carried off or flying through the air isn't sent.
	virtual void ScriptMoveTo(entt::entity animal, glm::vec2 point) = 0;
	/// The game's number for the state an animal is in for itself: the state it is in, or, while it is on its way into
	/// another (moving somewhere, or waiting for a clip to end), that one. None for what isn't an animal.
	[[nodiscard]] virtual std::optional<uint8_t> FinalGameStateOf(entt::entity animal) const = 0;
	/// A new size for an animal
	virtual void SetScale(entt::entity animal, float scale) = 0;
	/// A script lets an animal go. An animal out in the world that is in no flock gets a flock of its own, as wide and as
	/// close as its kind keeps one. With life, it then joins a flock near it if it can and starts a leg of its own; with
	/// no life, it starts to die. An animal in a hand or in the air is not changed.
	virtual void ReleaseFromScript([[maybe_unused]] entt::entity animal) {}
	/// An animal starts to wander, as one that a creature has healed does
	virtual void StartWandering([[maybe_unused]] entt::entity animal) {}
	/// An animal's radius across the ground: the larger half of its model's width and depth, scaled
	[[nodiscard]] virtual float RadiusOf(entt::entity animal) const = 0;
	/// The step an animal makes each turn, in metres across the land
	[[nodiscard]] virtual glm::vec3 MovementOf(entt::entity animal) const = 0;
	/// The point a flock's leader wanders about, moved
	virtual void SetFlockCentre(entt::entity flock, glm::vec2 centre) = 0;
	/// A shepherd moves its flock's place: the leader heads for the new place at once, wherever it was going
	virtual void MoveFlockPlace(entt::entity flock, glm::ivec2 place) = 0;
	/// An animal leaves its flock for a new flock of its own, about where it stands, as wide and as close as the one it
	/// left, which goes once no one is left in it. The new flock.
	virtual entt::entity SeparateIntoOwnFlock(entt::entity animal) = 0;
	/// An animal a shepherd has taken walks to the shepherd and lies dead there, as food taken to the store
	virtual void SendToSlaughter(entt::entity animal, glm::vec2 goal) = 0;
	/// An animal's speed now, in whole map units a turn
	[[nodiscard]] virtual uint16_t SpeedOf(entt::entity animal) const = 0;
	/// A script gives an animal a speed in metres a turn. The animal walks at that speed until its next state gives it a
	/// speed of its own. A miracle wolf keeps its speed
	virtual void SetSpeedFromScript([[maybe_unused]] entt::entity animal, [[maybe_unused]] float metresPerTurn) {}
	/// The speed of an animal that a script reads, in metres a turn: 0 for a dead animal
	[[nodiscard]] virtual float SpeedForScript([[maybe_unused]] entt::entity animal) const { return 0.0f; }
	/// The flock's leader, its first animal still there, none for an empty flock
	[[nodiscard]] virtual entt::entity LeaderOf(entt::entity flock) const = 0;
	/// The flock's animals, oldest first
	[[nodiscard]] virtual std::vector<entt::entity> MembersOf(entt::entity flock) const = 0;
	/// Where an animal is heading across the land now: a wolf hunting, its prey
	[[nodiscard]] virtual glm::vec2 GoalOf(entt::entity animal) const = 0;
	/// How high above the land at its goal an animal is heading for
	[[nodiscard]] virtual float GoalHeightOf(entt::entity animal) const = 0;
	/// A bird sent on its way: to a point at a height above the land, choosing what to do next once there; the leader of
	/// its flock has the others follow it in formation once they have arrived too
	virtual void SendBird(entt::entity bird, glm::vec2 goal, float height, bool leader) = 0;
	/// A wolf sent running from where it appeared to a destination, hunting along a strip of land so wide either side
	virtual void SendWolf(entt::entity wolf, glm::vec2 start, glm::vec2 destination, float halfWidth) = 0;
	/// A miracle's animal starts to fade out, then goes
	virtual void StartFading(entt::entity animal) = 0;
	/// An effect kills an animal where it has been put down: a miracle's animal fades out, any other is left with no life
	virtual void KillByEffect(entt::entity animal, glm::vec3 position) = 0;
	/// An animal with no life left starts dying where it is: it falls dead (a bird out of the sky), then lies dead its
	/// time. One in the physics starts dying only once it has come down.
	virtual void SetDying(entt::entity animal) = 0;
	/// An animal starts to die at once, without the work of a death: its life and its count of turns stay as they are.
	/// It falls dying, then its body lies dead for the turns that its earlier states left in the count.
	virtual void StartDyingWithoutDeath([[maybe_unused]] entt::entity animal) {}
	/// An animal is taken into a hand, the player's or a creature's: it plays its kind's clip for being held, if its
	/// kind has one
	virtual void IntoHand([[maybe_unused]] entt::entity animal) {}

	/// Whether an animal may take up a reaction: not held, flying or carried, and not dying, dead, brought down or holding
	/// still while a clip plays
	[[nodiscard]] virtual bool IsAvailableForReaction([[maybe_unused]] entt::entity animal) const { return false; }
	/// An animal flees from an object with no test, as it flees a fire, by the distances of the kind of reaction it
	/// takes up: false only when it isn't an animal. With the object gone it gives up on its next step.
	virtual bool SetupFleeFromObject([[maybe_unused]] entt::entity animal, [[maybe_unused]] entt::entity object,
	                                 [[maybe_unused]] Reaction reaction)
	{
		return false;
	}
	/// An animal frightened by a hunter runs from it: false only when it isn't an animal
	virtual bool SetupFleeFromPredator([[maybe_unused]] entt::entity animal, [[maybe_unused]] entt::entity predator)
	{
		return false;
	}
	/// The game's number for the state an animal is in now, none for what isn't an animal
	[[nodiscard]] virtual std::optional<uint8_t> TopGameStateOf([[maybe_unused]] entt::entity animal) const
	{
		return std::nullopt;
	}
	/// An animal takes up the reaction to a thing flying at a speed: it flees when the thing is nearer than it flies in
	/// two seconds, remembering it; otherwise it takes no notice. Whether it took it up.
	virtual bool SetupReactToFlyingObject([[maybe_unused]] entt::entity animal, [[maybe_unused]] entt::entity object,
	                                      [[maybe_unused]] float speed)
	{
		return false;
	}
	/// An animal takes up a reaction to look at a thing: it stands and turns to the thing. False when it is not an animal
	virtual bool SetupLookAtObject([[maybe_unused]] entt::entity animal, [[maybe_unused]] entt::entity object) { return false; }
	/// An animal takes up a reaction to food: it goes to the food and then stops the reaction. False when it is not an
	/// animal
	virtual bool SetupGotoFood([[maybe_unused]] entt::entity animal, [[maybe_unused]] entt::entity food) { return false; }
	/// The priority of a reaction to food for an animal. The table priority applies only when the food interests the
	/// animal, holds food and is near enough. Else the priority is 0
	[[nodiscard]] virtual uint32_t ReactToFoodPriority([[maybe_unused]] entt::entity animal, [[maybe_unused]] entt::entity food,
	                                                   [[maybe_unused]] uint32_t tablePriority) const
	{
		return 0;
	}
	/// An animal that does not react now remembers its state before it takes up a reaction
	virtual void RememberStateBeforeReaction([[maybe_unused]] entt::entity animal) {}
	/// The animal goes back to wandering when a reaction ends: its state before a reaction is set to wandering
	virtual void SetWanderBeforeReaction([[maybe_unused]] entt::entity animal) {}
	/// An animal's reaction ends, and the animal forgets what it fled. When `resetState` is true, the animal goes into
	/// the state after the reaction. When it is false, the reaction is gone and the animal stays in its state.
	virtual void StopReaction([[maybe_unused]] entt::entity animal, [[maybe_unused]] bool resetState) {}

	/// Whether a creature is frightened of an animal: bats frighten creatures, doves don't
	[[nodiscard]] virtual bool IsFrighteningToCreature(entt::entity animal) const = 0;
	/// Whether the player's hand may pick the animal up
	[[nodiscard]] virtual bool CanPlayerPickUp(entt::entity animal) const = 0;
	/// A bird's own say on whether a creature may pick it up: only when low enough over the land. None for the other
	/// animals, which take the rules of things in general.
	[[nodiscard]] virtual std::optional<bool> BirdCanBePickedUpByCreature(entt::entity animal) const = 0;
	/// Whether a creature of the height given (metres) may stamp on the animal
	[[nodiscard]] virtual bool CanBeStompedOnByCreature(entt::entity animal, float creatureHeight) const = 0;
};

} // namespace openblack::ecs::systems
