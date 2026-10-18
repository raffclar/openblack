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

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat2x2.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/SkeletalAnimation.h"
#include "Common/GameHeading.h"

// How a creature moves about something it acts on, such as a creature it casts a miracle at, as the game measures it.
//
// Sizes: a thing's radius on the ground is the larger of its model's half width and half length times its scale (a
// field's is always 5), and its height is its model's full height times its scale. A creature's is its height of 15 a
// size times how far its bones reach out from its middle in the first frame of its first animation. Walking up to a
// thing, it keeps clear of it by the thing's radius less some of its own: seven tenths of it for a thing much lower than
// four fifths of the creature, less the taller the thing, down to two fifths of it for a thing as tall as that or taller
// (a tree only a tenth of its radius, at most a quarter). It has arrived once it is within a tenth more than both their
// radii and the distance it was to keep.
//
// Getting away from a thing, it is far enough once it is the distance it was to keep plus a fifth more than the thing's
// radius away; else it goes that far from where it stands, straight away from the thing, to the nearest clear area as
// wide as it is tall: of the cells of the land within a hundred and thirty or so of the point, the middle of a square
// of cells whose every cell with its middle within half the width of the square's middle is clear.
//
// Turning to face a thing, it is facing it once it is within an eighth of a turn of it, or right on top of it.
//
// Pure, tested on made-up things.

namespace openblack::creature_cast_moves
{

/// A creature of size 1 is this tall
inline constexpr float k_HeightOfSizeOne = 15.0f;
/// A field's radius on the ground
inline constexpr float k_FieldRadius = 5.0f;
/// It has arrived within this much more than both radii and the distance
inline constexpr float k_ArrivalMargin = 1.1f;
/// It is far enough away at the distance plus this many times the thing's radius
inline constexpr float k_GetAwayRadii = 1.2f;
/// It goes along +x when it is no farther than this from the thing
inline constexpr float k_GetAwayApart = 0.0001f;
/// It faces a thing within this angle of it, and is on top of it within this distance
inline constexpr float k_FacingRadians = 0.3926991f;
inline constexpr float k_OnTopDistance = 0.1f;
/// The land's cells are this wide, and the clear area is looked for over this many of them across
inline constexpr float k_CellSize = 10.0f;
inline constexpr int32_t k_ClearAreaCells = 26;
/// Every thing a clear cell is kept clear of keeps this far from the cell's middle, past its own radius
inline constexpr float k_ClearOfThings = 5.0f;

/// How far a creature's bones reach out from its middle on the ground, in its model's units: the furthest of them in the
/// first frame of an animation
[[nodiscard]] float BoneReach(const skeletal_animation::Animation& animation, const skeletal_animation::Skeleton& skeleton);
/// A creature's radius on the ground, by its bones' reach on its model and the scale its model is drawn at: the game's
/// 15 a size times the reach in its own units comes to the same
[[nodiscard]] float CreatureRadius(float modelScale, float boneReach);

/// What walking up to a thing keeps clear of: its radius less some of the creature's own, by how tall each is; a tree's
/// is only a little of its radius
[[nodiscard]] float RoutePlanRadius(float radius, float height, bool tree, float creatureHeight, float creatureRadius);
/// How far apart two points on the land are, each made a map position first, with a true square root
[[nodiscard]] float MapPointDistance(glm::vec2 a, glm::vec2 b);
/// Whether two places that a creature walks to are the same place: the square of the distance between them is less
/// than k_SameDestinationSquared. The creature does not ask again for the same place.
inline constexpr float k_SameDestinationSquared = 0.01f;
[[nodiscard]] bool SameDestination(glm::vec2 before, glm::vec2 now);
/// Whether a creature at a point with a heading heads for another point near enough. The game compares game angles
/// between map positions, and takes their difference without going round. A creature in the same cell of the land as
/// the point also heads for it.
[[nodiscard]] bool HeadsFor(glm::vec2 creature, glm::vec2 point, GameHeading heading);
/// Whether a creature walking up to a thing, so far away on the ground, has arrived
[[nodiscard]] bool Arrived(float distance, float creatureRadius, float routeRadius, float keep);
/// Going near a thing, the creature notes where it is every k_StallTurns turns of the step. On the last turn of each
/// such span, it gives up when it moved less than k_StallDistance since the note. It then wants what it went for less
/// for k_StallHoldBackSeconds. A creature under the control of a script never gives up.
constexpr uint32_t k_StallTurns = 50;
constexpr float k_StallDistance = 0.01f;
constexpr float k_StallHoldBackSeconds = 10.0f;
enum class Stall : uint8_t
{
	None,
	/// Note where the creature is now
	Note,
	/// Give up the step, and hold back the desire of the plan
	GiveUp,
};
/// What a creature that goes near a thing does about a stall, by the turns of the step, how far it moved since the
/// last note and whether a script controls it
[[nodiscard]] Stall CheckStall(uint32_t turns, float moved, bool scripted);

/// What a creature that walks to a thing does after it asks for the walk: a creature that goes near a thing, picks it
/// up or stomps on it
enum class WalkStart : uint8_t
{
	/// The walk started: it continues to the thing
	Walking,
	/// The creature cannot walk now: it asks again on the next turn
	TryAgain,
	/// It cannot stand where the thing is: it stops the step, and it does not choose the thing again for a time
	NoWay,
};
/// What a creature that walks to a thing does, by whether its walk started and whether it can stand at the thing
[[nodiscard]] WalkStart WalkStartOf(bool started, bool destinationValid);

/// How far from a thing a creature getting away from it must be
[[nodiscard]] float GetAwayDistance(float keep, std::optional<float> thingRadius);
/// Where it goes: that far from where it stands, straight away from the thing. The creature and the thing are their map
/// positions as points, with the height of the land under them. `apart` is the map distance between them. When it is
/// almost 0, the creature goes along +x.
[[nodiscard]] glm::vec3 GetAwayPoint(const glm::vec3& creature, const glm::vec3& thing, float apart, float distance);

/// A circle a thing on the land keeps creatures off, on the ground
struct CollideCircle
{
	glm::vec2 centre;
	float radius;
};
/// The circles a building, feature, field or other fixed thing keeps clear, from its model's half width and length
/// times its scale (at least 1 each), the middle of its model's box placed on the land, and its turn: one circle as wide
/// as the longer, or for a thing more than 1.4 times as long as wide a row of circles as wide as the shorter along it
[[nodiscard]] std::vector<CollideCircle> CollideCircles(glm::vec2 halfSize, glm::vec2 centre, const glm::mat2& turn);
/// A tree keeps clear only a little circle where it stands
inline constexpr float k_TreeCollideRadius = 0.3f;
/// Whether a circle comes within the reach of a cell's middle that makes the cell not clear
[[nodiscard]] bool BlocksCell(const CollideCircle& circle, int32_t x, int32_t z);

/// A ball a thing on the land keeps creatures off. Most are on the ground; a worship site's stand at their height in
/// the world, and the cell test measures to them through the air from the cell's middle at the height of zero.
struct CollideSphere
{
	glm::vec3 centre;
	float radius;
};
/// Whether a ball comes within the reach of a cell's middle (at the height of zero) that makes the cell not clear
[[nodiscard]] bool BlocksCell(const CollideSphere& sphere, int32_t x, int32_t z);
/// The ten balls a worship site keeps creatures off: along the edges of its model, each at a height above the site
/// given by the model and as wide as 1.3 times that height, placed by the site's place in the world, turn and scale
inline constexpr size_t k_WorshipSiteSpheres = 10;
[[nodiscard]] std::array<CollideSphere, k_WorshipSiteSpheres> WorshipSiteSpheres(const glm::vec3& position,
                                                                                 const glm::mat3& rotation, float scale);

/// What a thing in a cell of the land is to the test of whether the cell is clear for a creature
struct ThingInCell
{
	/// A temple keeps every cell it stands in from being clear
	bool temple;
	/// Villagers, animals and creatures, which keep the cell from being clear only when the test asks for it
	bool living;
	/// Whether its balls are looked at: a tree or a field always, anything else when the creature must go round it
	bool tested;
	/// The balls it keeps creatures off; things that move about have none
	std::vector<CollideSphere> spheres;
};
/// Whether a thing in a cell keeps the cell from being clear for a creature
[[nodiscard]] bool BlocksCell(const ThingInCell& thing, int32_t x, int32_t z, bool livingBlocks);

/// Whether a cell of the land, by its column and row, is clear
using ClearCell = std::function<bool(int32_t x, int32_t z)>;
/// The middle of the nearest area so wide about a point whose cells are clear, if any
[[nodiscard]] std::optional<glm::vec2> FindClearArea(glm::vec2 point, float width, const ClearCell& clear);

/// Whether a creature with a heading faces a point from where it stands. The game takes the heading from the creature's
/// map position to the point's, and the creature faces it when its own heading is near enough to it. A creature on top
/// of the point faces it.
[[nodiscard]] bool Facing(glm::vec2 creature, GameHeading heading, glm::vec2 point);
/// The point a creature turns to so that its back is to a thing: as far from the creature as the thing, on the other
/// side
[[nodiscard]] glm::vec2 PointToFaceAwayFrom(glm::vec2 creature, glm::vec2 thing);

} // namespace openblack::creature_cast_moves
