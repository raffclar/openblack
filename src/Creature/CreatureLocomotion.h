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

#include <array>
#include <functional>
#include <numbers>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/GameHeading.h"

/// How a creature walks and runs, once a game turn. Its speed follows a requested fraction of its top speed, speeding up
/// and slowing down at a steady rate, slower uphill. The distance it covers drives its walk and run animations, blended
/// by how fast it goes, so its feet keep to the ground. Before setting off, and at corners too sharp to walk round, it
/// stops and either steps off towards its way, walks straight on when nearly facing it, or turns on the spot.
///
/// Headings are kept as the game keeps them (see GameHeading): a creature with heading h faces (sin h, -cos h) on the
/// x, z plane.
namespace openblack::creature_locomotion
{
/// The places of the moving animations in the creature spec file
namespace animations
{
constexpr size_t k_Stand = 0;
constexpr size_t k_Walk = 1;
constexpr size_t k_Run = 2;
/// Turning on the spot to the right by about 0, 90 and 180 degrees, then the same to the left
constexpr size_t k_RightSpin = 3;
constexpr size_t k_LeftSpin = 6;
/// Stepping off to the right by 0, 90 and 180 degrees, then the same to the left
constexpr size_t k_RightStep = 10;
constexpr size_t k_LeftStep = 13;
} // namespace animations

/// Running flat out, a creature goes this much faster than its running speed
constexpr float k_TopSpeedMargin = 1.1f;
/// A creature this exhausted only goes slowly
constexpr float k_ExhaustedAt = 0.8f;

/// How fast a creature of a size walks and runs, in units a second
struct Speeds
{
	float walk;
	float run;
};
/// Walking is 8 and running 20 units a second for each unit of 0.875 size + 0.25. The size is not kept within its
/// limits for this.
[[nodiscard]] Speeds SpeedsFor(float size);

/// The speed a creature heads for when asked for a fraction of its top speed
[[nodiscard]] float TargetSpeed(float fraction, float runSpeed);
/// The fraction of its top speed for a walk at the speed that it got last. A creature with no speed yet walks.
[[nodiscard]] float KeptFraction(float kept, float walkFraction);
/// The fraction of its top speed that the creature keeps after its turn. An exhausted creature that is not held gets
/// its slow fraction. It keeps that fraction after it recovers, until a new action or a run sets a different speed.
[[nodiscard]] float RequiredFraction(float kept, float slowFraction, float exhaustion, bool held);

/// The heading a direction on the x, z plane faces, as the game calculates it, 0 for a direction too short to turn to
[[nodiscard]] GameHeading HeadingOf(glm::vec2 direction);
/// The direction on the x, z plane a heading faces
[[nodiscard]] glm::vec2 DirectionOf(GameHeading heading);
/// Turning on the spot without animations, one step a game turn: the heading closes the part of the gap to the target
/// that the time before the step gives, from where it is now. At the end of the turn it is the target.
[[nodiscard]] GameHeading LerpHeading(GameHeading heading, GameHeading target, float timeMs, float durationMs);

enum class Side : uint8_t
{
	Right,
	Left,
};

/// The side of the spins for a turn on the spot to a heading. A turn of none uses the spins to the right.
[[nodiscard]] Side SpinSideOf(float angle);

/// Two of the three 0, 90 and 180 degree animations of a side, and how far to blend from the first to the second
struct Pair
{
	size_t from;
	size_t to;
	float weight;
};
/// The pair of spins that shows a turn on the spot, as the game blends them: the turn made positive, then between the
/// 0 and 90 degree spins below a quarter turn, else between the 90 and 180 degree spins
[[nodiscard]] Pair SpinPairFor(size_t first, float angle);

/// The distances around a destination that count as arriving: anywhere from min to max
struct Ring
{
	float min;
	float max;
};
/// The arrival ring for a walk of a distance: min never beyond the destination, max at least a hair beyond min
[[nodiscard]] Ring MoveRing(float distance, float minDistance, float maxDistance);
/// The arrival ring for walking up to an object: its radius and the creature's, and some way further
[[nodiscard]] Ring ObjectRing(float creatureRadius, float objectRadius, float extra);
/// Whether a creature this far from its destination has arrived
[[nodiscard]] bool Arrived(float distance, const Ring& ring);
/// How long a walk of a distance is given before it is given up on, in milliseconds
[[nodiscard]] float TimeLimitMs(float distance);
/// A creature that runs away goes this far plus a random number of units below k_RunAwayExtra
constexpr uint32_t k_RunAwayExtra = 40;
/// A creature that runs away arrives anywhere within this many times its height of the point that it runs to
constexpr float k_RunAwayArrivalHeights = 5.0f;
/// The height of a creature of size 1
constexpr float k_RunAwayHeightOfSizeOne = 15.0f;
/// The cells that the search for a clear cell tries, out from the cell it aims at
constexpr int32_t k_RunAwaySearchCells = 1600;
/// The nearest place to stand is used only nearer than this to the point that the run aims at
constexpr float k_RunAwayStandReach = 30.0f;

/// What the run away needs to know of the land
struct RunAwayLand
{
	/// Whether the creature may go to a cell of the map
	std::function<bool(int32_t x, int32_t z)> cellClear;
	/// The height of the ground at a map position
	std::function<float(const map_coords::MapCoords& coords)> ground;
	/// Whether the creature may stand at a point on the land
	std::function<bool(glm::vec2 point)> canStand;
	/// The nearest place to stand near a point, if there is one
	std::function<std::optional<glm::vec2>(glm::vec2 point)> nearestStand;
};

/// The point that a creature runs to from a threat, as the game finds it:
/// 1. The distance is the random draw plus the species' run away distance.
/// 2. The way is from the threat to the corner of the creature's cell, made one unit long. Too short, it is along x.
/// 3. The run aims at the cell that the distance along the way gets to. The creature keeps its place in the cell.
/// 4. A spiral of cells out from that cell gives the first clear cell. The point is its corner, on the ground.
/// 5. With no clear cell, the point is the creature's own place.
/// 6. Where the creature may not stand at the point, the nearest place to stand is used if it is near enough.
[[nodiscard]] glm::vec3 RunAwayPoint(const map_coords::MapCoords& creature, glm::vec2 threat, uint32_t draw,
                                     float runAwayDistance, const RunAwayLand& land);
} // namespace openblack::creature_locomotion
