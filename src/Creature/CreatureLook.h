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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// What a creature watches as it looks about. As its eyes sweep the land, each thing that it sees can take over from
/// what it watches: when it watches nothing, or when the thing catches its eye more than what it watches holds it. The
/// longer it watches one thing, the less a new thing catches its eye. It watches the thing until the thing goes out of
/// view.
namespace openblack::creature_look
{
/// The kinds of things a creature finds interesting, from most to least. A toy and another static thing that the
/// creature can pick up have interests of their own. A mobile object has no interest.
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
	Toy,
	Static,
	None,
};
[[nodiscard]] float InterestOf(Interest kind);

/// After this many whole seconds of watching one thing, nothing else catches the creature's eye
constexpr uint32_t k_BoredSeconds = 20;
/// How high a creature's eyes are above its feet at size 1
constexpr float k_HeadHeight = 15.0f;

/// How far a creature looks about, in metres: bigger creatures see further
[[nodiscard]] float LookRange(float size);
/// Things within half the range are as interesting as they get. Further things are less so: nothing at the range, and
/// less than nothing past it.
[[nodiscard]] float DistanceFactor(float distance, float range);

/// A creature keeps a thing in view when it can see the thing, and the thing is less than a sixth of a turn from the
/// way its body faces. The game measures this difference without a wrap at a full turn. A thing in its own map cell is
/// always in view. The angles are game angles.
constexpr int32_t k_BodyViewParts = 6;
[[nodiscard]] bool InView(uint16_t looking, uint16_t facing, uint16_t direction, bool sameCell);

/// A thing that the creature sees as its eyes sweep the land, and how far it is along the ground
struct Candidate
{
	uint32_t id;
	Interest kind;
	float distance;
};

/// What the creature can watch: a thing, or the mountains or the water that it finds on its map of the land as it moves
enum class Sight : uint8_t
{
	Thing,
	Mountains,
	Water,
};

/// How much each kind of sight catches the eye of one creature
struct Weights
{
	float things {0.0f};
	float mountains {0.0f};
	float water {0.0f};
};
[[nodiscard]] float WeightOf(const Weights& weights, Sight sight);

/// The mountains and the water have this interest
constexpr float k_LandInterest = 0.5f;

/// What the creature watches: the thing, or the place of the mountains or the water; its interest when the creature
/// started to watch it, where the creature looks to see it, and for how many turns it has watched it
struct Target
{
	std::optional<uint32_t> id;
	Sight sight {Sight::Thing};
	/// The place on the land of the mountains or the water, along the ground
	std::optional<glm::vec2> place;
	float interest {0.0f};
	glm::vec3 point {0.0f};
	uint32_t watchedTurns {0};
};
/// Whether the creature watches something
[[nodiscard]] bool Watches(const Target& target);

/// How much what it watches holds the creature: the creature's weight for that kind of sight, times the interest, times
/// the distance factor at the distance now (held between 0 and the range). Nothing holds it when it watches nothing.
[[nodiscard]] float Hold(const Target& target, float distance, float range, float weight);
/// A thing in view that the creature sees takes over what it watches, if it watches nothing or if the thing catches its
/// eye more than what it watches holds it. A thing catches its eye by the weight for things, its interest and the
/// distance factor, less as the whole seconds of watching go up to k_BoredSeconds. Returns whether the thing took over.
bool Consider(Target& target, const Candidate& candidate, float watchedDistance, float range, const Weights& weights,
              uint32_t turnsPerSecond);

/// The nearest mountains or water that the creature finds on its map as it moves, and how far they are along the ground
struct LandCandidate
{
	Sight sight {Sight::Mountains};
	glm::vec2 place {0.0f};
	float distance {0.0f};
};
/// The mountains or the water take over what the creature watches by the same rule as a thing, with the weight for
/// their kind and k_LandInterest. The creature does not have to see them. Returns whether they took over.
bool ConsiderLand(Target& target, const LandCandidate& candidate, float watchedDistance, float range, const Weights& weights,
                  uint32_t turnsPerSecond);
/// One turn of watching: while what the creature watches is in view, the creature watches it a turn longer. Else the
/// creature watches nothing.
void Watch(Target& target, bool inView);

/// The creature looking about
struct Viewer
{
	glm::vec3 position;
	/// Which way it faces, on the ground
	glm::vec3 ahead;
	float size;
};
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
