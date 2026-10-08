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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{
struct GAnimalInfo;
}

namespace openblack::ecs::animal_ai
{

/// The animals' per-turn AI like the original (docs/bw1-notes/animals.md): the animal state machine and its state
/// table. The grazing
/// species (sheep, tortoise, cow, horse, pig) wander in their herd, graze, sleep and breed; the predators (lion, tiger,
/// leopard, wolf; ECS/AnimalPredators.cpp) also stalk, chase, pounce on and eat other animals; the hand, the physics and
/// death work for every ground species; the birds (ECS/AnimalBirds.cpp) fly in flocks and never land.

/// the state table's states: 0..30 are the living states, 31..52 the animal ones
enum class AnimalState : uint8_t
{
	Invalid = 0,
	MoveToPos = 1,
	InScript = 4,
	FleeingFromObjectReaction = 6,
	GotoFoodReaction = 19,
	ArrivesAtFoodReaction = 20,
	LookingAtObjectReaction = 7,
	Flying = 10,
	Landed = 11,
	SetDying = 13,
	Dying = 14,
	Dead = 15,
	Drowning = 16,
	Downed = 17,
	BeingEaten = 18,
	WaitForAnimation = 23,
	FleeingAndLookingAtObjectReaction = 30,
	InHand = 24,
	MoveInFlock = 27,
	StartWander = 31,
	Wander = 32,
	Eat = 33,
	SeekSleep = 34,
	Sleeps = 35,
	SeekEnvironment = 36,
	StandardAction = 37,
	StartToEat = 38,
	FinishEating = 39,
	TargetPounce = 40,
	HuntingMoveToPos = 41,
	MoveToPosAndLookAround = 42,
	DecideWhatToDo = 43,
	SpecialMoveToPos = 44,
	FollowFlock = 45,
	LandOnObject = 46,
	LandAtPos = 47,
	InteractDecideWhatToDo = 48,
	FleeingFromPredatorReaction = 49,
	GivesBirth = 50,
	HideInLair = 51,
	SeekFood = 52,
};

/// The animals' part of the living turn, in the one living list with the villagers (living_turn::
/// ProcessLiving, ECS/LivingTurn.h). BeginAnimalsTurn: false without info.dat or the land (the animals take no turn);
/// `visualTime`: the visual time of day in hours (the big cats go to bed after 22:00, wolves hunt after 23:00)
[[nodiscard]] bool BeginAnimalsTurn(float visualTime);
/// One animal's turn when the list reaches it (its reactions, then its state): the brain made on its first turn, a
/// lone predator's lair, then its state. One that vanishes is deleted at once
void ProcessAnimal(entt::entity entity);
/// After the list: the villagers a predator holds, and the test hooks
void EndAnimalsTurn();

/// a pot's reaction (food 7: the hungry grazers within 35 m come and eat 50 of it), once until RemovePotReaction
/// (picked up, emptied, deleted). Called for the map's CREATE_POT, a pot the hand puts
/// down and the hand's new piles.
void SetupPotReaction(entt::entity pot);
void RemovePotReaction(entt::entity pot);
/// The Animal handler of the reactions: set once the game's reactions exist (before any map load) and again by
/// ClearReactions
void RegisterReactionHandler();
/// the map is unloaded
void ClearReactions();
/// Reaction 9: anything the hand throws or drops offers itself once to the
/// predators within 25 m, which flee when it comes fast enough
void SpreadFlyingObjectReaction(entt::entity object, PlayerNames thrower);

/// the flee-from-predator reaction, made when a predator is created: spread once, then, to the animals within 25 m
/// (the per-turn re-spreading is off in the shipped game)
void SpreadPredatorReaction(entt::entity predator);

/// The top state (DECIDE_WHAT_TO_DO before the animal's first turn)
[[nodiscard]] AnimalState TopState(entt::entity entity);
/// how it lay after its last landing (0 on its feet, 1 on its right side, 2 on its left, 3 none)
[[nodiscard]] uint16_t LandType(entt::entity entity);

/// whether the hand may pick it up: GAnimalInfo.playerCanPickUp
[[nodiscard]] bool ValidForPlaceInHand(entt::entity entity);
/// leaves its flock for one of its own, IN_HAND
void PlaceInHand(entt::entity entity);
/// every drop and throw is physics, FLYING
void InitialisePhysics(entt::entity entity);
/// the physics without a body (a particle system takes the animal): already FLYING -> true, nothing; else its previous
/// state is stored unless IN_HAND, then SetTopState(FLYING) must take, else false (refused)
[[nodiscard]] bool InitialisePhysicsWithoutBody(entt::entity entity);
/// the end of the physics, after its SetYAngle: the landType kept (living::AnimalLandType, 3 without a physics
/// object), the altitude 0, LANDED or dying / dead.
/// The heading is not touched here (living::AnimalLandingYaw -> SetYAngle, ECS/LivingPhysics)
void EndPhysics(entt::entity entity, uint16_t landType);
/// the drawn rotation and the game angle of the heading
void SetYAngle(entt::entity entity, float angle);
/// the object's own reactions end (the animals reacting to it stop, their state kept)
void EndReactionsOf(entt::entity object);
/// EndPhysics without a body, when no body could be built (from_hand's PlaceWithoutBody): no SetYAngle (the heading
/// stays), landType 3. An animal only:
/// living::EndPhysicsWithoutBody (which does the villager too; PlaceWithoutBody should call that one)
void PutDown(entt::entity entity);
/// SetDying when an effect took its last life. Nothing
/// while it flies: EndPhysics does it at rest.
void DestroyedByEffect(entt::entity entity);
/// at deletion (sunk, deleted): off its town and its flock; and (inferred: a deleted living leaves the living list)
/// no walk tags and no DownedVillager left
void Forget(entt::entity entity);
/// the animal's town; with a town (and an animal), at the head of the town's animal list. Literal: it does not take
/// the animal off the list of the town it had (only Forget unlinks), so a second call can list it twice
void SetTown(entt::entity animal, entt::entity town);

// ---- for the spells and scripts (ECS/AnimalApi.cpp; the Flock miracle) ----

/// AnimalArchetype::Create (with or without a flock; age 0 = the random one) and the player
/// that owns it (Animal::player; -1 none). entt::null for the species the original doesn't make.
entt::entity CreateAnimal(const glm::vec3& position, AnimalInfo type, entt::entity flock, uint32_t age, int32_t player);
/// SetupMoveToPos: the info's move state towards the point (the birds at that altitude over the land), then
/// `final`
void MoveTo(entt::entity entity, glm::vec2 position, float altitude, AnimalState final);
/// SetTopState: the state with its clip
void SetState(entt::entity entity, AnimalState state);
/// the top state only, the clip stays
void SetStateRaw(entt::entity entity, AnimalState state);
/// the SpellWolf's final destination: it runs there (SetRunToFinalDest) and dies
void SetFinalDestination(entt::entity entity, glm::vec2 position);
/// the final state's goal: where its move ends, x / altitude over the land / z
[[nodiscard]] std::optional<glm::vec3> Destination(entt::entity entity);
/// SetDying tells every listener first, once per death (not while it flies). Any number of
/// listeners; the id removes one
using DeathCallback = std::function<void(entt::entity)>;
uint32_t AddDeathListener(DeathCallback callback);
void RemoveDeathListener(uint32_t id);
/// the one listener of the old single slot: replaces the one it set before (empty: removes it)
void SetDeathCallback(DeathCallback callback);
/// a species' own SetDying (e.g. the SpellDove / SpellWolf: a fade instead of the common one): called instead of the
/// whole SetDying, before the flying check and the listeners; one slot
/// per species (empty: the common SetDying again)
using SpeciesDying = std::function<void(entt::entity)>;
void SetSpeciesDying(AnimalInfo type, SpeciesDying dying);
/// killed (SetDying: its dying and dead clips, the corpse) or gone at once (off its flock, out of the physics, deleted)
void Kill(entt::entity entity);
void Remove(entt::entity entity);
/// the last script reference of a script-controlled
/// animal has gone (ECS/ScriptHeld.h): a flock of its own if it has none, then dying if dead, else
/// INTERACT_DECIDE_WHAT_TO_DO (ECS/AnimalScript.cpp)
void ReleaseFromScript(entt::entity entity);
/// a script's state for an animal: the previous state kept, the exit function, the state with its
/// clip, the counter 0
void SetScriptState(entt::entity entity, AnimalState state);
/// The script's MOVE_GAME_THING on an animal: there already -> SetScriptState(IN_SCRIPT),
/// else SetupMoveToPos(pos, IN_SCRIPT); nothing while it is in the hand
void ScriptMoveTo(entt::entity entity, glm::vec2 position);
/// per-instance opacity (components::Alpha, the alpha-blended pass); 1 takes it off
void SetAlpha(entt::entity entity, float alpha);

/// a young one (age < grownUpAge) ageToScale[age - 1] (at age 0 the value just before the table), an adult 0.9
[[nodiscard]] float InitialScaleForAge(const GAnimalInfo& info, uint32_t age);
/// from the scale `current`: a young one current + GameFloatRand(float(ageToScale[age +
/// 1] - current) x 0.75); an adult t = (0.05 - GameFloatRand(0.1)) + 1, current < t ? (0.05 -
/// GameFloatRand(0.1)) + 1 : current. The game's synced GameFloatRand
[[nodiscard]] float ScaleForAge(const GAnimalInfo& info, uint32_t age, float current);
/// the script's SET_PROPERTY Age: InitialScaleForAge, ScaleForAge, then
/// the birth turn (turn - age x 1500)
void SetAge(entt::entity entity, uint32_t age);
/// (turn - birth turn) / 1500; 0 for a thing that is not an animal
[[nodiscard]] uint32_t GetAge(entt::entity entity);
/// SetSpeed of the metres in whole MapCoords
void SetSpeedInMetres(entt::entity entity, float metres);
/// the speed in metres
[[nodiscard]] float GetSpeedInMetres(entt::entity entity);
/// dying, or the top state DEAD, or not functional (available and life != 0)
[[nodiscard]] bool IsDead(entt::entity entity);

/// the birds (crows, doves, swallows, pigeons, seagulls, bats and the spell ones)
[[nodiscard]] bool IsFlyingSpecies(AnimalInfo type);

/// Test hooks, once per turn (ECS/AnimalDebugHooks.cpp)
void RunDebugHooks(uint32_t turn);

} // namespace openblack::ecs::animal_ai
