/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <optional>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureObjectActions.h"

namespace openblack::ecs::systems
{

/// What the creatures do with the things about them (see components::CreatureObjectAction): they walk up to things and
/// pick them up, hold them in their hands, look them over, eat them, put them down, toss them away or throw them, knock
/// them down, and point at them. What is let go of flies until it comes to rest. The creatures' towns watch what they
/// do with fear or respect. Minds and the debug tools tell the creatures what to do.
class CreatureObjectActionSystemInterface
{
public:
	/// How the creature's last action went
	enum class State : uint8_t
	{
		/// It hasn't been told to do anything
		Idle,
		Busy,
		Done,
		Failed,
	};

	virtual ~CreatureObjectActionSystemInterface() = default;

	/// Once a game turn: creatures walk up to what they act on, what they carry makes them stronger, and their towns'
	/// views of them change
	virtual void ProcessTurn() = 0;
	/// Once a game turn, before the creatures' own turns: what creatures that are gone held falls
	virtual void BeginTurn() {}
	/// In the creature's own turn, after its walk: its hands walk up to their thing, and what it carries counts
	virtual void CreatureTurn(entt::entity /*creature*/) {}
	/// In the creature's own turn, before its body takes its pose: its action plays on by the time of the turn
	virtual void ActionTurn(entt::entity creature) = 0;
	/// In the creature's own turn, after its body takes its pose: the spheres of its body knock about what they hit,
	/// things are taken hold of and let go of, and what was let go of flies
	virtual void AfterPoseTurn(entt::entity creature) = 0;
	/// Once a frame: what the creatures hold is drawn in their hands, as the frame draws their bodies
	virtual void UpdateHeld() = 0;
	/// At the start of a turn: what the creatures hold goes back into their hands as their last turns posed them
	virtual void ShowHeldAtTurnEnd() = 0;

	/// Each returns whether the creature could start. Picking up and knocking down need empty hands, all those that act
	/// on what it holds need something held.
	virtual bool PickUp(entt::entity creature, entt::entity object) = 0;
	virtual bool PutDown(entt::entity creature) = 0;
	virtual bool Discard(entt::entity creature) = 0;
	/// Tosses away what it holds with another animation, letting go at the same moment
	virtual bool DiscardWith(entt::entity creature, size_t animation) = 0;
	virtual bool EatHeld(entt::entity creature) = 0;
	/// Eats what it holds when it is told only to eat, with no thing given: the food goes into its body, but it does not
	/// swallow the meal, and the eat fails when the animation ends
	virtual bool EatHeldOnly(entt::entity creature) = 0;
	/// Strokes, shakes, smells or examines what it holds, by the animation of that
	virtual bool Keep(entt::entity creature, size_t animation) = 0;
	virtual bool Throw(entt::entity creature, const glm::vec3& target) = 0;
	/// Throws what it holds at a point, to take the given seconds to get there, as into a store: done only once the thing
	/// has stopped flying
	virtual bool ThrowTaking(entt::entity creature, const glm::vec3& target, float flightSeconds) = 0;
	/// Throws what it holds at a point, to take the given seconds to get there, as at a thing that it aims ahead of
	virtual bool ThrowAimed(entt::entity creature, const glm::vec3& target, float flightSeconds) = 0;
	virtual bool Destroy(entt::entity creature, entt::entity target) = 0;
	/// Kicks a thing at its feet, which flies off ahead of it as the foot meets it; fails when the thing is in its own or
	/// the player's hand, or the species has no kick
	virtual bool Kick(entt::entity creature, entt::entity target) = 0;
	virtual bool PointAt(entt::entity creature, const glm::vec3& point) = 0;
	/// Points for some seconds at a point, or at a thing wherever it goes
	virtual bool PointAtFor(entt::entity creature, const glm::vec3& point, float /*seconds*/,
	                        std::optional<entt::entity> /*thing*/)
	{
		return PointAt(creature, point);
	}
	/// Points at a point until it is told to stop, as it does when it draws a gesture in the air. It turns to face the
	/// point first when it is far round.
	virtual bool PointUntilTold(entt::entity /*creature*/, const glm::vec3& /*point*/) { return false; }
	/// The point moves: the arm follows it
	virtual void MovePoint(entt::entity /*creature*/, const glm::vec3& /*point*/) {}
	/// The point ends: the arm plays to the end of its animations
	virtual void EndPoint(entt::entity /*creature*/) {}
	/// Where the pointing hand is, while the arm points and the point has not ended
	[[nodiscard]] virtual std::optional<glm::vec3> GetPointingHand(entt::entity /*creature*/) const { return std::nullopt; }
	/// Catches something flying at it, when it can still reach where it passes
	virtual bool Catch(entt::entity creature, entt::entity object) = 0;
	/// Reaches out to take a thing the player's hand gives it, with its left hand when mirrored: the thing leaves the
	/// hand for the creature's at the moment of taking, if the hand still gives it then
	virtual bool TakeFromHand(entt::entity /*creature*/, entt::entity /*object*/, bool /*mirrored*/) { return false; }
	/// While it reaches for a thing in the player's hand: the thing, and how many milliseconds its animation is from the
	/// moment it takes it (less than nothing before)
	struct TakingFromHand
	{
		entt::entity object {entt::null};
		float msFromTake {0.0f};
	};
	[[nodiscard]] virtual std::optional<TakingFromHand> GetTakingFromHand(entt::entity /*creature*/) const
	{
		return std::nullopt;
	}
	/// Where its right hand takes a thing from the player's hand, turned to face a point
	[[nodiscard]] virtual std::optional<glm::vec3> TakePoint(entt::entity /*creature*/, glm::vec3 /*facing*/) const
	{
		return std::nullopt;
	}
	/// Stops what it is doing, keeping hold of whatever it holds
	virtual void Cancel(entt::entity creature) = 0;
	/// Lets go of what it holds at once, dropping it from the hand
	virtual void Drop(entt::entity creature) = 0;

	[[nodiscard]] virtual State GetState(entt::entity creature) const = 0;
	/// The cause of the last action's failure, when it failed
	[[nodiscard]] virtual creature_object_actions::Failure GetFailure(entt::entity /*creature*/) const
	{
		return creature_object_actions::Failure::Other;
	}
	/// Whether its body is taken up by a catch it has started: readying, stepping, reaching or bringing the hand back
	[[nodiscard]] virtual bool IsCatching(entt::entity creature) const = 0;
	/// How far through its animations the action is, 0 to 1, while it plays them
	[[nodiscard]] virtual std::optional<float> GetProgress(entt::entity creature) const = 0;
	[[nodiscard]] virtual std::optional<entt::entity> GetHeld(entt::entity creature) const = 0;
	/// What something is worth to eat, if anything
	[[nodiscard]] virtual std::optional<float> FoodValueOf(entt::entity object) const = 0;
	/// Whether a creature can pick something up from where it is: things and villagers, not the land's fixtures nor other
	/// creatures, nor what a script or the thing's state keeps from it, nor what is too big or heavy for it, a town's
	/// artefact, what its leash is tied to, or a thing out of the map it isn't holding already
	[[nodiscard]] virtual bool CanPickUp(entt::entity creature, entt::entity object) const = 0;
	/// Whether a creature can eat a thing: a thing that it can pick up and that is not a toy. It does not eat a sort of
	/// thing that it brought up before, and a computer player's creature does not eat its own villagers.
	[[nodiscard]] virtual bool CanEat(entt::entity creature, entt::entity object) const = 0;
	/// Whether a creature can eat the thing that it holds, with no pick-up first
	[[nodiscard]] virtual bool CanEatHeld(entt::entity creature) const = 0;
	/// Whether a creature can take something from a hand that gives it: as picking up, but wherever the thing is, and
	/// without the rules about artefacts, worshippers and what scripts hold
	[[nodiscard]] virtual bool CanTakeFromHand(entt::entity /*creature*/, entt::entity /*object*/) const { return false; }
	/// Whether a flying thing is of a kind creatures catch and its own state lets them, before its weight is compared
	[[nodiscard]] virtual bool CanBeCaught(entt::entity /*object*/) const { return false; }
	/// What kind of thing something is to a creature's hands, and what its kind says of its being picked up
	[[nodiscard]] virtual creature_object_actions::KindFacts KindOf(entt::entity /*object*/) const { return {}; }
	/// Whether a creature may throw a thing to impress, by its kind, its height and the creature's weight
	[[nodiscard]] virtual bool CanThrowToImpress(entt::entity /*creature*/, entt::entity /*object*/) const { return false; }
	/// Whether a creature can knock something down: trees, homes and things, statics and rocks among them
	[[nodiscard]] virtual bool CanDestroy(entt::entity target) const = 0;
	/// Whether a creature can stomp on a thing, by the kind of the thing and how tall it is for the creature
	[[nodiscard]] virtual bool CanStompOn(entt::entity /*creature*/, entt::entity /*target*/) const { return false; }
	/// Whether a creature can kick a thing, by the kind of the thing and how heavy it is for the creature
	[[nodiscard]] virtual bool CanKick(entt::entity /*creature*/, entt::entity /*target*/) const { return false; }
	/// How far a creature's hand gets at its furthest stretch when picking up, measured straight, from where the hand
	/// takes hold in its reaching animations; none when its species has nothing to reach with or isn't posed yet
	[[nodiscard]] virtual std::optional<float> PickUpStraightReach(entt::entity /*creature*/) const { return std::nullopt; }
};

} // namespace openblack::ecs::systems
