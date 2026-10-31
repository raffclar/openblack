/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <random>
#include <unordered_map>

#include "Creature/CreatureRoute.h"
#include "Creature/CreatureRouteFollow.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct CreatureLocomotion;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class CreatureLocomotionSystem final: public CreatureLocomotionSystemInterface
{
public:
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;
	void ProcessTurn() override;
	void ShareSearchSteps() override;
	void CreatureTurn(entt::entity creature) override;
	void Update(float turnFraction) override;
	void ShowTurnEnds() override;
	void ShowTurnEnd(entt::entity creature) override;

	MoveResult MoveTo(entt::entity creature, glm::vec2 point, Pace pace, float minDistance, float maxDistance) override;
	MoveResult LeadTo(entt::entity creature, glm::vec2 point, float extraSpeed, float maxDistance) override;
	MoveResult MoveToObject(entt::entity creature, entt::entity target, Pace pace, float extra) override;
	MoveResult Follow(entt::entity creature, entt::entity target, float distance, Pace pace) override;
	MoveResult FleeFrom(entt::entity creature, glm::vec2 threat, Pace pace) override;
	bool TurnToFace(entt::entity creature, glm::vec2 point) override;
	bool TurnToHeading(entt::entity creature, GameHeading heading) override;
	bool TurnToFaceAtOnce(entt::entity creature, glm::vec2 point) override;
	bool TurnToHeadingAtOnce(entt::entity creature, GameHeading heading) override;
	void TurnToFaceAtRest(entt::entity creature, glm::vec2 point) override;
	[[nodiscard]] bool TurnWaitsForRest(entt::entity creature) const override;
	void Stop(entt::entity creature) override;
	void StopAtOnce(entt::entity creature) override;
	void ResetPace(entt::entity creature) override;
	void SetKeptFraction(entt::entity creature, float fraction) override;
	void Place(entt::entity creature, glm::vec3 position) override;
	void PlaceWithoutWalking(entt::entity creature, glm::vec3 position) override;
	[[nodiscard]] bool IsMoving(entt::entity creature) const override;
	[[nodiscard]] float KeptFraction(entt::entity creature) const override;
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override;
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity creature, glm::ivec2 cell) override;

private:
	/// The heading to face a point from where the creature is, or none when the point is where it is
	[[nodiscard]] static std::optional<GameHeading> HeadingToFace(entt::entity creature, glm::vec2 point);
	/// Whether its body plays an action, as an action from its mind or an action with a thing
	[[nodiscard]] bool PlaysBodyAction(entt::entity creature) const;
	/// Its walk stops, and it turns on the spot to the heading
	bool StartTurnOnTheSpot(entt::entity creature, GameHeading heading);
	/// The turn that waits until the action of its body ends starts when the body is at rest
	void StartTurnAtRest(entt::entity creature);
	/// Starts a move for a fraction of top speed, the creature's state already gathered, to a point or up to a thing
	MoveResult StartMove(entt::entity creature, components::CreatureLocomotion& locomotion, glm::vec2 point, float fraction,
	                     float minDistance, float maxDistance, bool ontoDestination);
	/// How far round its middle a species' body reaches at a drawn scale of one, worked out once
	float BodyRadius(CreatureType species);
	/// Sets the breath of a creature that stops back to its resting period
	static void BreatheAtRest(entt::entity creature);

	/// Where creatures can walk, sorted from the land the first time it is wanted. The system is made anew with each
	/// land.
	std::optional<creature_route::WalkableLand> _land;
	std::unordered_map<CreatureType, float> _bodyRadii;
	/// The search steps that each creature's route search gets after the first turn of its walk, shared at the start of
	/// each turn of the living
	int32_t _searchShare {creature_route::k_FirstShareSearchSteps};
};

} // namespace openblack::ecs::systems
