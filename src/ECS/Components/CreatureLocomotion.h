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
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/GameHeading.h"
#include "Creature/CreatureHeadLook.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureRouteFollow.h"

namespace openblack::ecs::components
{

/// How a creature is getting about: where it is going, the route it follows there, how fast it goes and what its legs
/// play. It moves once a game turn and is drawn between where it was and where it is by how far through the turn the
/// frame is.
struct CreatureLocomotion
{
	enum class Motion : uint8_t
	{
		Standing,
		/// Waiting for its route to be planned, fidgeting if it takes long
		Planning,
		/// Playing a confused fidget while it waits
		Confused,
		/// Turning on the spot
		Turning,
		/// Stepping off sideways or backwards into a walk
		Stepping,
		Walking,
		/// Pointing at the route ahead while it waits for its route: its first fidget
		Pointing,
	};
	Motion motion {Motion::Standing};
	/// Whether its heading has been read from where it faces
	bool started {false};
	/// Its heading as the game keeps it. Only the drawn turn of its body changes it to a yaw.
	GameHeading heading {};
	/// The heading it turns or steps towards
	GameHeading targetHeading {};
	/// Units a second
	float speed {0.0f};
	/// The fraction of its top speed it is asked to go at
	float fraction {0.0f};
	creature_locomotion::Speeds speeds {};
	/// The scale its body is drawn at, mesh units to the world's
	float scale {1.0f};
	/// How far round its middle its body reaches by its own size, which it keeps clear of things by, and as drawn,
	/// which others keep clear of it by
	float radius {5.0f};
	float shownRadius {5.0f};

	/// Where it is going, and how close counts as arriving
	std::optional<glm::vec2> destination;
	creature_locomotion::Ring ring {};
	/// Whether it walks onto the destination itself, as to a point, rather than to the edge of the ring
	bool ontoDestination {false};
	/// What it walks round, the searches for its route and the route it follows
	creature_route::Follower follower;
	/// The creature as its route search saw it when the walk started. The search finds the things in its way from it.
	/// None when the search finds only the land.
	std::optional<creature_obstacles::Walker> routeWalker;
	/// A new walk's first turn of searching, which searches further and notes what it meets
	bool firstSearchTurn {false};
	/// A route was found while it stood, and it sets off once it has got up
	bool routeReady {false};
	/// For testbed scenarios only: the search for its route makes no steps while this is set
	bool searchHeld {false};
	/// Whether the last move failed: nowhere to stand there, or no way there
	bool failed {false};
	/// The first turn of the search stopped far short of the destination: the destination, and the things that the
	/// search met in the order that it met them. The walk does not fail: it ends where the creature stands. None after
	/// any other end of a move.
	struct GaveUp
	{
		glm::vec2 destination {0.0f};
		std::vector<uint32_t> met;
	};
	std::optional<GaveUp> gaveUp;
	/// The creature clears a thing out of its way. Its route keeps clear of things by its drawn size, and its first
	/// search notes nothing that it meets.
	bool clearingWay {false};
	/// Turning to face a point without going anywhere
	bool facingOnly {false};
	/// Following something, keeping within a distance of it
	std::optional<entt::entity> following;
	float followDistance {0.0f};

	/// A turn on the spot or a step: the two animations blended, how far through it is and how long it lasts
	struct Move
	{
		std::optional<creature_locomotion::Pair> animations;
		float timeMs;
		float durationMs;
		/// A step: where the blend takes the root over the step in mesh units, and the blend's own length, over which its
		/// frames spread
		glm::vec3 displacement {0.0f};
		float blendDurationMs {0.0f};
		/// A step: the side of the steps, and the route's heading at the last turn
		creature_locomotion::Side side {creature_locomotion::Side::Right};
		GameHeading routeHeading {};
	};
	std::optional<Move> move;
	/// The walk animation's time in whole milliseconds, which the run keeps in step with
	int32_t walkTimeMs {0};
	/// How far it moved this turn
	float distance {0.0f};

	/// Fidgets while waiting for a route: milliseconds to the next, and whether it has had its first
	float fidgetMs {0.0f};
	bool fidgeted {false};
	/// Where its head looks while it waits for its route
	creature_head_look::RouteLook routeLook {};
	/// The point at the route ahead, its first fidget while it waits for its route. A route found while it points waits
	/// until the point is over.
	struct RoutePoint
	{
		/// Where the arm points
		glm::vec3 point {0.0f};
		/// The time of the four pointing animations
		float timeMs {0.0f};
		/// It faces the point, after a turn on the spot when the point was too far round
		bool ready {false};
		/// It points with its left arm
		bool mirrored {false};
		/// The point ends: the arm plays to the end of the animations
		bool ending {false};
	};
	std::optional<RoutePoint> routePoint;

	/// Where it was at the start of this turn and where it is at its end, drawn between them
	glm::vec3 fromPosition {0.0f};
	glm::vec3 toPosition {0.0f};
	GameHeading fromHeading {};
	GameHeading toHeading {};
	/// A turn or step ended this turn with its animation turning the body to its new heading: the body is drawn at its
	/// old heading until the turn ends, and at the new one at once from the next
	bool headingSnaps {false};
	/// What its legs play over this turn: each animation's time at the start of the turn, how far it moves on by the
	/// end, and its weight
	struct Track
	{
		size_t animation;
		float fromMs;
		float advanceMs;
		float durationMs;
		float weight;
		bool looping;
		/// Played at the breath's point instead, as standing is
		bool breathing;
		/// A turn's or step's pair: the animation blended keyframe by keyframe towards this one by blendWeight
		std::optional<size_t> blendedWith {};
		float blendWeight {0.0f};
		/// Played left to right
		bool mirrored {false};
	};
	std::vector<Track> tracks;
};

/// Whether the creature is in its move: from the wait for its route, through its fidgets, points, turns and steps, to
/// its walk. A turn on the spot to face a point is not part of a move. The body's needs count a creature in its move as
/// a creature that moves.
[[nodiscard]] inline bool InMove(const CreatureLocomotion& self)
{
	using Motion = CreatureLocomotion::Motion;
	return self.motion != Motion::Standing && !(self.motion == Motion::Turning && self.facingOnly);
}

} // namespace openblack::ecs::components
