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

#include "Creature/CreatureLocomotion.h"
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
	};
	Motion motion {Motion::Standing};
	/// Whether its heading has been read from where it faces
	bool started {false};
	float heading {0.0f};
	/// The heading it turns or steps towards
	float targetHeading {0.0f};
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
	/// A new walk's first turn of searching, which searches further and notes what it meets
	bool firstSearchTurn {false};
	/// A route was found while it stood, and it sets off once it has got up
	bool routeReady {false};
	/// Whether the last move failed: nowhere to stand there, or no way there
	bool failed {false};
	/// Turning to face a point without going anywhere
	bool facingOnly {false};
	/// Following something, keeping within a distance of it
	std::optional<entt::entity> following;
	float followDistance {0.0f};

	/// A turn on the spot or a step: the two animations blended, how far through it is and how long it lasts, where
	/// it takes the root in mesh units, and the heading and route heading it started at
	struct Move
	{
		std::optional<creature_locomotion::Pair> animations;
		float timeMs;
		float durationMs;
		glm::vec2 displacement;
		float startHeading;
		float startRouteHeading;
	};
	std::optional<Move> move;
	/// The walk animation's time, which the run keeps in step with
	float walkTimeMs {0.0f};
	/// How far it moved this turn
	float distance {0.0f};

	/// Fidgets while waiting for a route: milliseconds to the next, and whether it has had its first
	float fidgetMs {0.0f};
	bool fidgeted {false};

	/// Where it was at the start of this turn and where it is at its end, drawn between them
	glm::vec3 fromPosition {0.0f};
	glm::vec3 toPosition {0.0f};
	float fromHeading {0.0f};
	float toHeading {0.0f};
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
	};
	std::vector<Track> tracks;
};

} // namespace openblack::ecs::components
