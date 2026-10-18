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

#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "Common/GameHeading.h"
#include "ECS/Systems/SavedStateInterface.h"

namespace openblack::creature_route
{
class WalkableLand;
struct Circle;
} // namespace openblack::creature_route

namespace openblack::ecs::systems
{

/// Gets the creatures about the land: they plan routes round what is in their way, walk and run along them, turn and
/// step off, and are drawn moving smoothly between game turns (see components::CreatureLocomotion). Minds and the
/// debug tools tell them where to go.
class CreatureLocomotionSystemInterface: public SavedStateInterface
{
public:
	/// What became of a request to move
	enum class MoveResult : uint8_t
	{
		/// Nowhere a creature can stand
		InvalidDestination,
		/// The creature can't walk: it has no walk or run animations, or isn't a creature
		Busy,
		Started,
	};
	enum class Pace : uint8_t
	{
		Walk,
		Run,
		/// The speed that it got last. A new creature walks.
		Keep,
	};

	virtual ~CreatureLocomotionSystemInterface() = default;

	/// Once a game turn: routes are planned and the creatures move
	virtual void ProcessTurn() = 0;
	/// Once a frame: the creatures are placed and their legs posed between the last two turns, by how far through the
	/// turn the frame is, 0 to 1
	virtual void Update(float turnFraction) = 0;
	/// A creature is placed and its legs posed where they are at the end of this turn, for the pose of the turn
	virtual void ShowTurnEnd(entt::entity creature) = 0;

	/// Walks or runs to anywhere from minDistance to maxDistance away from a point
	virtual MoveResult MoveTo(entt::entity creature, glm::vec2 point, Pace pace, float minDistance, float maxDistance) = 0;
	/// Walks to a point faster than its walk speed by an extra fraction of its top speed. It stops anywhere within
	/// maxDistance of the point.
	virtual MoveResult LeadTo(entt::entity creature, glm::vec2 point, float extraSpeed, float maxDistance) = 0;
	/// Walks or runs up to something, stopping some way short of it by both their sizes, and up to extra further
	virtual MoveResult MoveToObject(entt::entity creature, entt::entity target, Pace pace, float extra) = 0;
	/// Keeps within a distance of something, following it as it moves, until stopped
	virtual MoveResult Follow(entt::entity creature, entt::entity target, float distance, Pace pace) = 0;
	/// Runs away from a point, at a pace. It arrives anywhere within five times its height of the point that it runs to.
	virtual MoveResult FleeFrom(entt::entity creature, glm::vec2 threat, Pace pace) = 0;
	/// Stops (see Stop), then turns on the spot to face a point. While it moves or turns, or while its body plays an
	/// action, it does not turn and gives false. The caller must ask again on a later turn.
	virtual bool TurnToFace(entt::entity creature, glm::vec2 point) = 0;
	/// Stops (see Stop), then turns on the spot to a heading, also when the turn is very small or none. While it moves
	/// or turns, or while its body plays an action, it does not turn and gives false.
	virtual bool TurnToHeading(entt::entity creature, GameHeading heading) = 0;
	/// Stops at once where it is, then turns on the spot to face a point. An action that its body plays does not stop
	/// the turn.
	virtual bool TurnToFaceAtOnce(entt::entity creature, glm::vec2 point) = 0;
	/// Stops at once where it is, then turns on the spot to a heading. An action that its body plays does not stop the
	/// turn.
	virtual bool TurnToHeadingAtOnce(entt::entity creature, GameHeading heading) = 0;
	/// Stops as the game stops a walk. A creature on its way walks on and slows to a stand a short distance ahead. While
	/// it waits for its route, it stands. A turn on the spot to face a point continues.
	virtual void Stop(entt::entity creature) = 0;
	/// Stops at once where it is: the walk or the turn ends
	virtual void StopAtOnce(entt::entity creature) = 0;
	/// Sets the speed that it keeps back to its walk speed, as when it stops what it does
	virtual void ResetPace(entt::entity creature) = 0;
	/// Puts it at a point at once, as something other than its walking moves it, such as a step towards a catch
	virtual void Place(entt::entity creature, glm::vec3 position) = 0;
	/// Puts it at a point at once, as a script does. Its walk, turn, body action, face and head look stop, and it stands
	/// there. What it plans to do does not change.
	virtual void PlaceWithoutWalking(entt::entity creature, glm::vec3 position) = 0;
	/// Whether it is planning a route, turning or on its way
	[[nodiscard]] virtual bool IsMoving(entt::entity creature) const = 0;

	/// Where the creatures can walk on this land
	[[nodiscard]] virtual const creature_route::WalkableLand& GetWalkableLand() = 0;
	/// The circles of the things in a cell that the creature walks round, as its route search finds them there
	[[nodiscard]] virtual std::vector<creature_route::Circle> ThingCirclesIn(entt::entity creature, glm::ivec2 cell) = 0;
};

} // namespace openblack::ecs::systems
