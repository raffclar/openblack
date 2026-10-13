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
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// What an idle creature looks at as it looks about: the most interesting thing it can see, the nearer the better.
/// The longer it has watched one thing, the less anything else catches its eye, until what it watches goes out of sight.
namespace openblack::creature_look
{
/// The kinds of things a creature finds interesting, from most to least
enum class Interest : uint8_t
{
	Dove,
	Citadel,
	Creature,
	Animal,
	Villager,
	Abode,
	Tree,
	Fixed,
};
[[nodiscard]] float InterestOf(Interest kind);

/// After this many seconds watching one thing, nothing else catches the creature's eye
constexpr float k_BoredSeconds = 20.0f;
/// How high a creature's eyes are above its feet at size 1
constexpr float k_HeadHeight = 15.0f;

/// How far a creature looks about, in metres: bigger creatures see further
[[nodiscard]] float LookRange(float size);
/// Things within half the range are as interesting as they get; further ones less so, down to nothing at the range
[[nodiscard]] float DistanceFactor(float distance, float range);

/// The creature looking about
struct Viewer
{
	glm::vec3 position;
	/// Which way it faces, on the ground
	glm::vec3 ahead;
	float size;
};
/// Whether a creature can see a point: in front of it, within its range
[[nodiscard]] bool CanSee(const Viewer& viewer, const glm::vec3& point);

struct Candidate
{
	uint32_t id;
	Interest kind;
	/// Where the creature looks to look at it
	glm::vec3 point;
};

/// What the creature watches and for how many turns it has
struct Target
{
	std::optional<uint32_t> id;
	Interest kind {Interest::Fixed};
	glm::vec3 point {0.0f};
	uint32_t watchedTurns {0};
};

/// How much a thing catches the creature's eye from where it is
[[nodiscard]] float Priority(const Viewer& viewer, Interest kind, const glm::vec3& point);

/// One game turn of looking about: what it watches, if still in sight, is watched a turn longer; then each candidate in
/// sight, its priority less as the creature grows bored, takes over if it beats what is watched. Candidates should
/// include the thing watched, where it is now; if it isn't among them, it is no longer there.
[[nodiscard]] Target LookAbout(Target target, std::span<const Candidate> candidates, const Viewer& viewer,
                               float turnsPerSecond);

/// Where to look when nothing is interesting: straight ahead at head height
[[nodiscard]] glm::vec3 PointAhead(const Viewer& viewer);

/// The creature info values for the look of a creature that walks to a place
struct WhileGoingInfo
{
	/// The sweep turns the head from side to side, up to this angle (in radians) from ahead, by a step each turn
	float sweepHalfAngle {0.0f};
	float sweepAngleStep {0.0f};
	/// The sweep looks at the ground this far ahead. The distance increases by a step at each side, up to the furthest,
	/// and then starts again at the nearest.
	float sweepNearest {0.0f};
	float sweepFurthest {0.0f};
	float sweepDistanceStep {0.0f};
	/// Within this distance of the place, the creature looks only at the place
	float placeRange {0.0f};
};

/// The look of a creature that walks to a place: three looks in turn, and the sweep
struct WhileGoing
{
	enum class Look : uint8_t
	{
		/// At the place
		Place,
		/// At what it watches. If it watches nothing, at the last moving thing that it noticed in the last two seconds.
		/// If there is no such thing, at the place.
		Watched,
		/// The sweep from side to side
		Sweep,
	};
	Look look {Look::Place};
	/// The turns of the look that the creature has done
	uint32_t turns {0};
	/// The sweep's angle from ahead, its step, its distance and the distance step. The steps come from the creature
	/// info when the creature is made.
	float sweepAngle {0.0f};
	float sweepAngleStep {0.0f};
	float sweepDistance {0.0f};
	float sweepDistanceStep {0.0f};
};
/// The start of the look, from the creature info
[[nodiscard]] WhileGoing StartWhileGoing(const WhileGoingInfo& info);

/// Within this distance of the place, the creature does not use this look
constexpr float k_WhileGoingLeast = 8.0f;
/// Each look continues for two seconds of turns, then the next look starts
constexpr float k_WhileGoingLookSeconds = 2.0f;

/// The last moving thing that the creature noticed. The creature keeps the point where it noticed the thing, the height
/// of the thing and the game turn. The point does not follow the thing.
struct NoticedMover
{
	glm::vec2 at {0.0f};
	/// The height of the thing above the ground
	float height {0.0f};
	uint32_t turn {0};
};

/// Whether the creature notices a thing that it comes across, so that the thing becomes its last noticed mover. The
/// thing must move, and it must not be the creature itself.
[[nodiscard]] bool Notices(bool itself, bool moving);

/// The look uses a noticed mover for this many seconds of turns after the creature noticed it
constexpr float k_MoverSeconds = 2.0f;
/// Whether the creature noticed a mover in a turn that is recent enough for the look in this game turn
[[nodiscard]] bool IsRecent(uint32_t noticedTurn, uint32_t turn, float turnsPerSecond);

/// What the creature knows as it walks, for its look
struct WhileGoingView
{
	glm::vec2 position {0.0f};
	/// The direction the body faces, in radians as the game keeps it
	float heading {0.0f};
	glm::vec2 place {0.0f};
	/// The point of what the creature watches, if it watches something
	std::optional<glm::vec3> watched;
	/// The point of the last moving thing that the creature noticed. Its y is the ground below the point plus the height
	/// of the thing.
	std::optional<glm::vec3> mover;
	/// The game turn when the creature noticed the mover, and the game turn now
	uint32_t moverTurn {0};
	uint32_t turn {0};
	float turnsPerSecond {10.0f};
};
/// What the head looks at in this turn of the walk. It is empty within k_WhileGoingLeast of the place: then the creature
/// looks about as usual.
struct WhileGoingPoint
{
	glm::vec2 at {0.0f};
	/// The height of the point. When it is empty, the point is on the ground.
	std::optional<float> height;
};
[[nodiscard]] std::optional<WhileGoingPoint> LookWhileGoing(WhileGoing& state, const WhileGoingInfo& info,
                                                            const WhileGoingView& view);
} // namespace openblack::creature_look
