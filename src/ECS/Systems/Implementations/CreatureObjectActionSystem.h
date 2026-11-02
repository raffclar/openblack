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
	void StopForPlan(entt::entity creature, bool stopsBody) override;
	[[nodiscard]] std::optional<float> GetProgress(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> GetCarried(entt::entity creature) const override;
	[[nodiscard]] bool HandsTouched(entt::entity creature) const override;
	bool CrushUnderFoot(entt::entity creature, entt::entity thing) override;
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity object) const override;
	[[nodiscard]] bool CanPickUp(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanEat(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanEatHeld(entt::entity creature) const override;
	[[nodiscard]] bool CanTakeFromHand(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] bool CanBeCaught(entt::entity object) const override;
	[[nodiscard]] bool CanThrowToImpress(entt::entity creature, entt::entity object) const override;
	[[nodiscard]] creature_object_actions::KindFacts KindOf(entt::entity object) const override { return KindFactsOf(object); }
	[[nodiscard]] std::optional<creature_object_actions::LiftFacts> LiftFactsOf(entt::entity creature,
	                                                                            entt::entity object) const override
	{
		return PickUpFactsOf(creature, object).lift;
	}
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
	/// Whether the creature may pick the thing up, with the kind facts of the thing already found
	[[nodiscard]] bool CanPickUpKind(entt::entity creature, entt::entity object,
	                                 const creature_object_actions::KindFacts& kind) const;
	/// Keeps the reach of the creature for a pick-up in the synced world, as the full pick-up facts do
	static void KeepPickUpReach(entt::entity creature);
	/// Whether the creature belongs to a computer player and no script controls it
	[[nodiscard]] static bool ComputerCreatureNotScripted(entt::entity creature);
	/// Catches made to wait while a creature's body acted out something else start once it is free
	/// Starts the catches that waited, of one creature or of all
	void StartPendingCatches(std::optional<entt::entity> only = std::nullopt);
	/// Puts what each creature holds in its hand, as the frame draws the body or as the last turn posed it
	static void PlaceHeld(bool drawn);
	/// An eat that stops between the bite and the swallow: the creature swallows what left its hand at once
	void SwallowStoppedEat(entt::entity creature);
	/// Starts an action on the creature in place of any it was doing, or records why it can't
	bool Start(entt::entity creature, components::CreatureObjectAction action);
	/// How the creature releases a thing:
	/// - Dropped: it starts where it hangs in the hand, and keeps the turn of the hand.
	/// - LetGo: as Dropped, and it keeps the angles of the turn in the hand.
	/// - Thrown: it starts with its origin at the finger ends, and turns by the angles of the turn in the hand.
	enum class ReleaseFrom : uint8_t
	{
		Dropped,
		LetGo,
		Thrown,
	};
	/// Lets go of the held object, which flies off with a velocity
	void Release(entt::entity creature, const glm::vec3& velocity, ReleaseFrom from = ReleaseFrom::Dropped);
	/// A held thing goes into the physics as the creature lets it go
	void LetGo(entt::entity creature, entt::entity object, const glm::vec3& velocity);
};

} // namespace openblack::ecs::systems
