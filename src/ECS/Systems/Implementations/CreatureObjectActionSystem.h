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

#include "Creature/CreatureObjectActions.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct CreatureObjectAction;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class CreatureObjectActionSystem final: public CreatureObjectActionSystemInterface
{
public:
	void ProcessTurn() override;
	void BeginTurn() override;
	void CreatureTurn(entt::entity creature) override;
	void ActionTurn(entt::entity creature) override;
	void AfterPoseTurn(entt::entity creature) override;
	void UpdateHeld() override;
	void ShowHeldAtTurnEnd() override;

	bool PickUp(entt::entity creature, entt::entity object) override;
	bool PutDown(entt::entity creature) override;
	bool Discard(entt::entity creature) override;
	bool DiscardWith(entt::entity creature, size_t animation) override;
	bool EatHeld(entt::entity creature) override;
	bool EatHeldOnly(entt::entity creature) override;
	bool Keep(entt::entity creature, size_t animation) override;
	bool Throw(entt::entity creature, const glm::vec3& target) override;
	bool ThrowTaking(entt::entity creature, const glm::vec3& target, float flightSeconds) override;
	bool ThrowAimed(entt::entity creature, const glm::vec3& target, float flightSeconds) override;
	bool Destroy(entt::entity creature, entt::entity target) override;
	bool Kick(entt::entity creature, entt::entity target) override;
	bool PointAt(entt::entity creature, const glm::vec3& point) override;
	bool PointAtFor(entt::entity creature, const glm::vec3& point, float seconds, std::optional<entt::entity> thing) override;
	bool PointUntilTold(entt::entity creature, const glm::vec3& point) override;
	void MovePoint(entt::entity creature, const glm::vec3& point) override;
	void EndPoint(entt::entity creature) override;
	[[nodiscard]] std::optional<glm::vec3> GetPointingHand(entt::entity creature) const override;
	bool Catch(entt::entity creature, entt::entity object) override;
	bool TakeFromHand(entt::entity creature, entt::entity object, bool mirrored) override;
	[[nodiscard]] std::optional<TakingFromHand> GetTakingFromHand(entt::entity creature) const override;
	[[nodiscard]] std::optional<glm::vec3> TakePoint(entt::entity creature, glm::vec3 facing) const override;
	void Cancel(entt::entity creature) override;
	void Drop(entt::entity creature) override;

	[[nodiscard]] State GetState(entt::entity creature) const override;
	[[nodiscard]] creature_object_actions::Failure GetFailure(entt::entity creature) const override;
	[[nodiscard]] bool IsCatching(entt::entity creature) const override;
	[[nodiscard]] bool PlaysOnAlone(entt::entity creature) const override;
	[[nodiscard]] std::optional<float> GetProgress(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity creature) const override;
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity object) const override;
	[[nodiscard]] bool CanPickUp(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanEat(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanEatHeld(entt::entity creature) const override;
	[[nodiscard]] bool CanTakeFromHand(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanBeCaught(entt::entity object) const override;
	[[nodiscard]] bool CanThrowToImpress(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] creature_object_actions::KindFacts KindOf(entt::entity object) const override { return KindFactsOf(object); }
	[[nodiscard]] bool CanDestroy(entt::entity target) const override;
	[[nodiscard]] bool CanStompOn(entt::entity creature, entt::entity target) const override;
	[[nodiscard]] bool CanKick(entt::entity creature, entt::entity target) const override;
	[[nodiscard]] std::optional<float> PickUpStraightReach(entt::entity creature) const override;

private:
	/// What a creature held and no longer holds falls, or it swallows what it ate. With no creature: the things that
	/// creatures that are gone held.
	void LetGoHeldThings(std::optional<entt::entity> creature);
	/// What the thing's kind says about a creature picking it up
	[[nodiscard]] creature_object_actions::KindFacts KindFactsOf(entt::entity object) const;
	/// What the rules for a creature picking the thing up depend on
	[[nodiscard]] creature_object_actions::PickUpFacts PickUpFactsOf(entt::entity creature, entt::entity object) const;
	/// Catches made to wait while a creature's body acted out something else start once it is free
	/// Starts the catches that waited, of one creature or of all
	void StartPendingCatches(std::optional<entt::entity> only = std::nullopt);
	/// Puts what each creature holds in its hand, as the frame draws the body or as the last turn posed it
	static void PlaceHeld(bool drawn);
	/// An eat that stops between the bite and the swallow: the creature swallows what left its hand at once
	void SwallowStoppedEat(entt::entity creature);
	/// Starts an action on the creature in place of any it was doing, or records why it can't
	bool Start(entt::entity creature, components::CreatureObjectAction action);
	/// Where a released thing starts: where it hangs in the hand, or with its origin at the finger ends as a throw does
	enum class ReleaseFrom : uint8_t
	{
		Hanging,
		FingerEnds,
	};
	/// Lets go of the held object, which flies off with a velocity
	void Release(entt::entity creature, const glm::vec3& velocity, ReleaseFrom from = ReleaseFrom::Hanging);
	/// A held thing goes into the physics as the creature lets it go
	void LetGo(entt::entity creature, entt::entity object, const glm::vec3& velocity);
};

} // namespace openblack::ecs::systems
