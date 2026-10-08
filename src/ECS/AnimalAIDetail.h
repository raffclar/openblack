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

#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/AnimalAI.h"
#include "ECS/Map.h"
#include "ECS/Systems/AnimalAISystemInterface.h"
#include "Enums.h"

namespace openblack
{
struct GAnimalInfo;
}

namespace openblack::ecs::components
{
struct Animal;
struct AnimalBrain;
struct Flock;
struct Transform;
} // namespace openblack::ecs::components

/// The animal AI's internals shared by ECS/AnimalAI.cpp (the turn, the grazers, the hand and death) and
/// ECS/AnimalPredators.cpp (the predators' decisions and their hunting).
namespace openblack::ecs::animal_ai::detail
{

constexpr float k_MapCoordsPerMetre = map_coords::k_FixedPerMetre;
constexpr int32_t k_Circle = 2048;
/// the returns of the original's "did something" tests (ReactToAnimalNeeds, KeepLeaderWithinDomain...)
constexpr int k_Started = 0x23;
constexpr int k_Nothing = 0x24;
/// How many turns a living takes to die
constexpr int16_t k_TurnsToDieOver = 600;

struct Context
{
	entt::entity entity;
	components::Animal& animal;
	components::AnimalBrain& brain;
	components::Transform& transform;
	const GAnimalInfo& info;
};

/// the predators' code: Tiger and Leopard run the Lion code (the tiger has its own lair), the Wolf its own decide,
/// needs and lair
enum class Hunter
{
	None,
	Cat,
	Tiger,
	Wolf,
};

/// What the game's animals share (the visual time, the death listeners, the species' own dying); stops with a message
/// when it is missing (before the game or after it has gone)
[[nodiscard]] systems::AnimalAISystemInterface& Shared();
/// The visual time of day (hours) of this turn
[[nodiscard]] float VisualTime();
/// The game turn: the common clock of ECS/Effects/Reactions (Game's turn count, 0 at a land's load)
[[nodiscard]] uint32_t Turn();

[[nodiscard]] const GAnimalInfo& InfoOf(const components::Animal& animal);
[[nodiscard]] bool IsGrazer(AnimalInfo type);
[[nodiscard]] Hunter HunterOf(AnimalInfo type);

/// The square spiral: every caller starts with dir 1, count 1, tests its own cell, then adds each step (whole
/// 10 m cells, the sub-cell offset kept): (-1, 0), (0, -1), (+1, 0) x2, (0, +1) x2, (-1, 0) x3...
struct Spiral
{
	map_coords::Spiral spiral;
	glm::ivec2 Next()
	{
		const auto& step = spiral.Next();
		return {step.x, step.z};
	}
	/// The way every caller of the spiral moves (the random position, the graze position and the flock search): the
	/// step is added to the high words only, so the sub-cell fraction never changes and the 16-bit add wraps at the
	/// map's edge
	void Advance(map_coords::MapCoords& coords) { map_coords::AddCells(coords, spiral.Next()); }
};

// MapCoords and angles (2048 per circle)
[[nodiscard]] glm::ivec2 Step(uint16_t angle, uint32_t speed);
/// gutils::GetAngleFromDXDZ on raw MapCoords / whole units. Two positions go
/// through gutils::GetAngleFromXZ, the difference / side of two angles through gutils::GetAngleDifference /
/// GetAngleSign
[[nodiscard]] uint16_t AngleOfMapCoords(int32_t dx, int32_t dz);
[[nodiscard]] float Metres(uint32_t speed);
[[nodiscard]] glm::vec2 Xz(const components::Transform& transform);
[[nodiscard]] MapInterface::CellId CellOf(glm::vec2 p);
void FaceAngle(components::Transform& transform, uint16_t angle);

// the land
[[nodiscard]] bool InBounds(glm::vec2 p);
/// Off the map collides with everything; type 1 with the water cells
[[nodiscard]] bool Collides(glm::vec2 p, uint32_t collideType);
[[nodiscard]] glm::vec2 SquarePos(glm::vec2 c, float size);
/// A random position around `c` (living::CalcRandomPos with the animal's own tests)
glm::vec2 CalcRandomPos(const Context& ctx, glm::vec2 c, float rMin, float rMax);
/// Outside both of its turning circles
[[nodiscard]] bool IsPosValidForTurnAngle(const Context& ctx, glm::vec2 p);
/// The flock's domain centre: also the leader's move goal
void SetDomainCentre(components::Flock& flock, glm::vec3 position);
[[nodiscard]] bool Available(entt::entity entity);

// the flock
[[nodiscard]] components::Flock* FlockOf(const components::Animal& animal);
[[nodiscard]] entt::entity LeaderOf(const components::Flock& flock);
[[nodiscard]] glm::vec2 FlockPos(const Context& ctx);
[[nodiscard]] bool PosWithinDomain(const Context& ctx, glm::vec2 p);
[[nodiscard]] bool IsLeader(const Context& ctx);

// dying and the hook the spells use (AnimalAI.h SetDeathCallback)
void SetDying(entt::entity entity, components::AnimalBrain& brain);
void Delete(entt::entity entity);

// states and moving
void SetTopState(entt::entity entity, components::AnimalBrain& brain, AnimalState state);
void SetTopState(Context& ctx, AnimalState state);
void PlayAnimThenSetState(Context& ctx, AnimalState state);
void SetSpeed(Context& ctx, uint32_t speed);
[[nodiscard]] uint32_t SpeedDefault(const Context& ctx);
void SetupMoveToPos(Context& ctx, glm::vec2 p, AnimalState final);
/// The current state = info.moveState, the destination `final`: false when the exit test refuses it
bool SetCurrentAndDestinationState(Context& ctx, AnimalState final);
/// The goal, InitStepsXZ, ARRIVED or STEP_THROUGH
void SetupMobileMoveToPos(Context& ctx, glm::vec2 p);
/// The wall-hugging move for ARRIVED / FINAL_STEP / STEP_THROUGH: 6 or 7 when it stepped (same / new map cell), 0xA
/// when it got there, 0 otherwise
int MoveTo(Context& ctx);
constexpr uint8_t k_MoveArrived = 1;
constexpr uint8_t k_MoveFinalStep = 4;
constexpr uint8_t k_MoveWander = 5;
constexpr uint8_t k_MoveStepThrough = 0xB;
bool MoveBy(Context& ctx, glm::ivec2 step);
/// Arrival test and steps set-up: re-aim with the species' turn limit
bool AreWeThere(const Context& ctx);
void InitStepsXZ(Context& ctx);
void SetTowardsAngle(Context& ctx, uint16_t target, float distance);
int CheckNeeds(Context& ctx);
int KeepLeaderWithinDomain(Context& ctx);
int KeepFlockMemberWithinFlockArea(Context& ctx);
void StartWander(Context& ctx);
/// With a flock LookForFlocksInSpiral(2 x domainRadius, merge), then StartWander (the species' own)
void InteractDecideWhatToDo(Context& ctx);
/// With merge: the bigger flock of the same species and player keeps everyone
void LookForFlocksInSpiral(Context& ctx, float radius, bool merge);
/// (turn - birth) / 1500 turns per year
[[nodiscard]] uint32_t AgeOf(const components::AnimalBrain& brain);
/// A turn of the breeding (an adult only), hunger and sleep counters
void ProcessNeeds(Context& ctx);
/// The cow's reaction to its needs: breeding (an adult only), then hunger, then sleep
int CowReactToAnimalNeeds(Context& ctx);
/// AgeOf below grownUpAge
[[nodiscard]] bool IsChild(const Context& ctx);

// the reactions (ECS/AnimalFlee.cpp)
void ProcessReaction(Context& ctx);
/// the states whose exit function is ExitReaction
[[nodiscard]] bool IsReactionState(uint8_t state);
/// Leaving the reaction states the reaction is dropped (the state kept)
void ExitReaction(components::AnimalBrain& brain, uint8_t next);
void FleeingFromPredatorReaction(Context& ctx);
void GotoFoodReaction(Context& ctx);
void ArrivesAtFoodReaction(Context& ctx);
void FleeingFromObjectReaction(Context& ctx);
void FleeingAndLookingReaction(Context& ctx);
[[nodiscard]] components::AnimalBrain* BrainOf(entt::entity entity);

// the birds (ECS/AnimalBirds.cpp)
[[nodiscard]] bool IsBird(AnimalInfo type);
void BirdDecideWhatToDo(Context& ctx);
void BirdStartWander(Context& ctx);
int BirdReactToAnimalNeeds(Context& ctx);
void SpecialMoveToPos(Context& ctx);
void FollowFlock(Context& ctx);
void BirdDying(Context& ctx);
/// The time to bank and the bank angle: Dove 2 s / 0.5 rad, the spell birds 0.5 s; 0 for the ground ones
[[nodiscard]] float TimeToBank(AnimalInfo type);
[[nodiscard]] float BankAngle(AnimalInfo type);

// the predators (ECS/AnimalPredators.cpp)
void SetRunToFinalDest(Context& ctx);
/// The spell wolf's move after the common one: the hunt when hungry, the end within 30 m
void SpellWolfMoveToPos(Context& ctx);
/// the villagers a predator caught: DOWNED, BEING_EATEN 300 turns, dead
void ProcessDownedVillagers();
void PredatorDecideWhatToDo(Context& ctx);
int PredatorReactToAnimalNeeds(Context& ctx);
void HuntingMoveToPos(Context& ctx);
void TargetPounce(Context& ctx);
void BeingEaten(Context& ctx);
void HideInLair(Context& ctx);
void CalculateLairPosition(Context& ctx);
/// test hook OPENBLACK_TEST_LAIRS: logs the forest list, then every predator flock leader recomputes its lair
void TestLairs();

} // namespace openblack::ecs::animal_ai::detail
