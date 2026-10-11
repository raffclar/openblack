/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/AnimalSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

#include "Animals/AnimalMove.h"
#include "ECS/Components/Animal.h"

namespace openblack::ecs::systems
{

class AnimalSystem final: public AnimalSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(uint32_t turn, float turnFraction) override;
	void Reset() override;

	entt::entity CreateFlock(glm::vec2 centre, float domainRadius, float flockDistance) override;
	entt::entity CreateSpellAnimal(AnimalInfo type, glm::vec2 position, float heightAboveLand, uint16_t angle,
	                               PlayerNames owner, entt::entity flock, entt::entity spell) override;
	entt::entity CreateScriptFlock(int32_t id, glm::vec2 position, glm::vec2 home, float reach, float flockDistance) override;
	[[nodiscard]] entt::entity FindScriptFlock(int32_t id) const override;
	entt::entity CreateBird(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flock) override;
	entt::entity CreateScriptAnimal(AnimalInfo type, glm::vec2 position) override;
	void SetFlockTown(entt::entity flock, entt::entity town) override;
	entt::entity CreateGrazer(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flock,
	                          entt::entity town) override;
	void JoinFlock(entt::entity animal, entt::entity flock) override;
	bool SetScriptState(entt::entity animal, LivingStates state) override;
	void SetScale(entt::entity animal, float scale) override;
	[[nodiscard]] float RadiusOf(entt::entity animal) const override;
	[[nodiscard]] glm::vec3 MovementOf(entt::entity animal) const override;
	void SetFlockCentre(entt::entity flock, glm::vec2 centre) override;
	void KillByEffect(entt::entity animal, glm::vec3 position) override;
	void SetDying(entt::entity animal) override;
	void IntoHand(entt::entity animal) override;
	[[nodiscard]] entt::entity LeaderOf(entt::entity flock) const override;
	[[nodiscard]] std::vector<entt::entity> MembersOf(entt::entity flock) const override;
	[[nodiscard]] glm::vec2 GoalOf(entt::entity animal) const override;
	[[nodiscard]] float GoalHeightOf(entt::entity animal) const override;
	void SendBird(entt::entity bird, glm::vec2 goal, float height, bool leader) override;
	void SendWolf(entt::entity wolf, glm::vec2 start, glm::vec2 destination, float halfWidth) override;
	void StartFading(entt::entity animal) override;

	[[nodiscard]] bool IsAvailableForReaction(entt::entity animal) const override;
	bool SetupFleeFromObject(entt::entity entity, entt::entity object) override;
	bool SetupReactToFlyingObject(entt::entity animal, entt::entity object, float speed) override;
	void StopReaction(entt::entity animal) override;

	[[nodiscard]] bool IsFrighteningToCreature(entt::entity animal) const override;
	[[nodiscard]] bool CanPlayerPickUp(entt::entity animal) const override;
	[[nodiscard]] bool CanBePickedUpByCreature(entt::entity animal) const override;
	[[nodiscard]] bool CanBeStompedOnByCreature(entt::entity animal, float creatureHeight) const override;

private:
	// Moving
	/// Off to a goal at a height above the land there, in its kind's move state, into a final state once there
	void SetupMoveTo(components::Animal& animal, glm::vec2 goal, float goalHeight, components::AnimalState finalState);
	/// A turn's move towards its goal, climbing or sinking towards its goal's height: whether it arrived
	bool MoveTo3D(components::Animal& animal);
	/// It tilts into the turn it made
	static void Banked(components::Animal& animal, const animals::Turn& turn);
	/// A random point between two distances of a centre that the animal can reach without circling, or the centre
	[[nodiscard]] glm::vec2 RandomPos(const components::Animal& animal, glm::vec2 centre, float inner, float outer) const;

	// The birds of a flock
	/// A land bird's leader gives its leg up once it has kept to it its kind's stay time: whether it did
	bool LeaderGivesUpLeg(entt::entity entity, components::Animal& animal);
	/// A bird's speed, which a land bird chooses its flying clip afresh for
	static void SetSpeed(components::Animal& animal, size_t speed);
	/// The temples' doves and bats, seen to every hundred turns
	void ProcessTempleBirds();
	void Bird(entt::entity entity, components::Animal& animal);
	void SpecialMoveToPos(entt::entity entity, components::Animal& animal);
	void DecideWhatToDo(entt::entity entity, components::Animal& animal);
	/// A leader's next leg, about where its flock was made
	void StartWander(entt::entity entity, components::Animal& animal);
	/// A follower near its leader, then in formation
	void FollowFlock(entt::entity entity, components::Animal& animal);

	// The grazers
	void GrazerTurn(entt::entity entity, components::Animal& animal, components::Grazer& grazer);
	void GrazerDecide(entt::entity entity, components::Animal& animal, components::Grazer& grazer);
	/// Off wandering about its herd's leader, as far as its herd's reach
	void GrazerStartWander(entt::entity entity, components::Animal& animal);
	/// A straight step, steered afresh in each new cell; seeing to its needs first
	void GrazerWander(entt::entity entity, components::Animal& animal, components::Grazer& grazer);
	/// Its needs: whether it set off to see to one
	bool ReactToGrazerNeeds(entt::entity entity, components::Animal& animal, components::Grazer& grazer);
	/// The leader takes the herd somewhere new once out of its reach or there its stay time: whether it did
	bool KeepLeaderWithinDomain(entt::entity entity, components::Animal& animal);
	/// A member too far from its leader goes back: whether it is busy with that
	bool KeepMemberWithinFlockArea(entt::entity entity, components::Animal& animal);
	/// A new straight step for a wanderer keeping within distances of a centre
	void SetNewWander(entt::entity entity, components::Animal& animal, std::optional<glm::ivec2> centre, int32_t inner,
	                  int32_t outer);
	[[nodiscard]] std::optional<glm::ivec2> LookForGrazeSpot(entt::entity entity, const components::Animal& animal) const;
	void GivesBirth(entt::entity entity, components::Animal& animal);
	/// A herd without a town merges with another of its kind nearby, the bigger keeping them all
	void MergeNearbyHerd(entt::entity entity, components::Animal& animal);
	/// A grazer's new speed, and its clip at that speed
	static void SetGrazerSpeed(components::Animal& animal, uint16_t speed);
	/// A turn's walk on the land towards its goal: whether it arrived
	static bool GroundMoveTo(components::Animal& animal);
	/// Where its herd's leader is, or the herd's home with no leader
	[[nodiscard]] glm::ivec2 HerdPosOf(entt::entity flock) const;
	/// Whether a point is within the reach of an animal's herd's home: never for an animal with no herd
	[[nodiscard]] bool WithinDomain(const components::Animal& animal, glm::ivec2 point) const;
	[[nodiscard]] std::optional<animals::grazers::HerdSize> HerdSizeOf(const components::Animal& animal) const;
	[[nodiscard]] uint32_t AgeOf(const components::Grazer& grazer) const;
	/// Whether an animal sees to its needs before it does what its state does
	[[nodiscard]] static bool SeesToNeedsIn(components::AnimalState state);

	// Fleeing what it reacts to, for any animal on the land
	/// A turn of its flight or its watching; whether it was fleeing at all
	bool Flee(entt::entity entity, components::Animal& animal);

	// The wolves
	void Wolf(entt::entity entity, components::Animal& animal);
	/// Off to where it was sent, at its run
	void RunToFinalDestination(entt::entity wolf, components::Animal& animal);
	/// Running where it was sent, hunting when hungry, fading near there
	void WolfMoveToPos(entt::entity entity, components::Animal& animal);
	void WolfStartWander(entt::entity entity, components::Animal& animal);
	void ReactToFoodNeeds(entt::entity entity, components::Animal& animal);
	/// The first prey it meets searching the map cells about it, none for nothing
	[[nodiscard]] entt::entity FindPrey(entt::entity wolf, const components::Animal& animal);
	[[nodiscard]] bool IsHuntingTargetValid(entt::entity wolf, const components::Animal& animal, entt::entity prey) const;
	void SetupMoveToTarget(entt::entity wolf, components::Animal& animal, entt::entity prey);
	void Abandon(entt::entity wolf, components::Animal& animal);
	void Chase(entt::entity wolf, components::Animal& animal);
	void Pounce(entt::entity wolf, components::Animal& animal);
	/// A wolf brings its prey down
	static void BringDown(entt::entity wolf, entt::entity prey);
	void FinishPouncing(entt::entity wolf, components::Animal& animal, entt::entity prey);
	static void WaitForClip(components::Animal& animal, components::AnimalState next);
	void Eat(entt::entity wolf, components::Animal& animal);
	/// The villagers and animals brought down: eaten over the turns, then dead
	void ProcessEaten();

	/// The miracles' animals fade out and go
	void ProcessFading();
	/// An animal goes, and leaves its flock
	void Remove(entt::entity animal);
	/// A turn of a killed animal: falling dead, then lying dead its time before it goes
	void ProcessDeath(entt::entity entity, components::Animal& animal);
	/// The grey puff of smoke a dead body leaves as it goes
	void LeaveSmoke(entt::entity entity, const components::Animal& animal) const;
	/// A dying bird falls out of the sky through the physics
	void FallDying(entt::entity entity, const components::Animal& animal);
	/// An animal leaves its flock, which goes once empty
	void LeaveFlock(entt::entity entity, components::Animal& animal);

	/// The game's turn, and its clock at the last frame in milliseconds
	uint32_t _turn {0};
	/// How many flocks have been made, which orders them
	uint32_t _flocksMade {0};
	uint32_t _drawTime {0};
};

} // namespace openblack::ecs::systems
